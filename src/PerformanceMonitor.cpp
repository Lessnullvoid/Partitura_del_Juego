#include "PerformanceMonitor.h"

#include "Channel.h"
#include "SettingsStore.h"
#include "VisualComposer.h"

#include <algorithm>
#include <cmath>
#include <mach/mach.h>
#include <sys/resource.h>

namespace {
double cpuSecondsNow() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0.0;
    return static_cast<double>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) +
           static_cast<double>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) /
               1000000.0;
}

float residentMemoryNowMB() {
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
        return 0.f;
    }
    return static_cast<float>(info.resident_size) / (1024.f * 1024.f);
}

const char* kSubsystemNames[] = {
    "decoder", "video render", "GPU readback", "computer vision",
    "score composition", "OSC", "channel draw", "total channel update"
};

const char* kScoreModeNames[] = {
    "BwClean", "ScanLine", "BBoxTracker", "BinaryText", "Waveform",
    "GridData", "Barcode", "VideoNormal", "VideoSquares", "VideoNumbers",
    "VideoLines", "ThermalVision", "SlitScan", "Flash"
};

}

PerformanceMonitor::PerformanceMonitor(int channelCount)
    : channelCount_(ofClamp(channelCount, 1, kMaxChannels)) {
}

void PerformanceMonitor::configure(const PerformanceTestConfig& config,
                                   int targetFps,
                                   const std::string& outputMode) {
    config_ = config;
    targetFps_ = std::max(1, targetFps);
    outputMode_ = outputMode;
}

void PerformanceMonitor::beginWindowFrame(int window) {
    if (window < 0 || window >= kMaxWindows) return;
    auto& data = windows_[window];
    const double now = ofGetElapsedTimef();
    if (data.lastFrameAt > 0.0) {
        const float frameMs = static_cast<float>((now - data.lastFrameAt) * 1000.0);
        data.rollingFrames.push_back(frameMs);
        while (data.rollingFrames.size() > 300) data.rollingFrames.pop_front();
        if (running_ && elapsedSeconds() >= config_.warmupSeconds) {
            data.testFrames.push_back(frameMs);
            if (phase_ >= 0 && phase_ < 6)
                data.phaseFrames[phase_].push_back(frameMs);
        }
    }
    data.lastFrameAt = now;
}

void PerformanceMonitor::recordWindowUpdate(int window, float milliseconds) {
    if (window < 0 || window >= kMaxWindows) return;
    windows_[window].summary.updateMs = milliseconds;
}

void PerformanceMonitor::beginWindowDraw(int window) {
    if (window < 0 || window >= kMaxWindows) return;
    auto& data = windows_[window];
    for (int i = 0; i < static_cast<int>(data.gpuQueries.size()); ++i) {
        if (!data.queryPending[i] || data.gpuQueries[i] == 0) continue;
        GLint available = GL_FALSE;
        glGetQueryObjectiv(data.gpuQueries[i], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available) {
            GLuint64 nanoseconds = 0;
            glGetQueryObjectui64v(data.gpuQueries[i], GL_QUERY_RESULT, &nanoseconds);
            data.summary.gpuMs = static_cast<float>(nanoseconds) / 1000000.f;
            data.queryPending[i] = false;
        }
    }

    data.activeQuery = -1;
    for (int i = 0; i < static_cast<int>(data.gpuQueries.size()); ++i) {
        if (data.queryPending[i]) continue;
        if (data.gpuQueries[i] == 0) glGenQueries(1, &data.gpuQueries[i]);
        glBeginQuery(GL_TIME_ELAPSED, data.gpuQueries[i]);
        data.activeQuery = i;
        break;
    }
}

void PerformanceMonitor::endWindowDraw(int window, float milliseconds) {
    if (window < 0 || window >= kMaxWindows) return;
    auto& data = windows_[window];
    data.summary.drawMs = milliseconds;
    if (data.activeQuery >= 0) {
        glEndQuery(GL_TIME_ELAPSED);
        data.queryPending[data.activeQuery] = true;
        data.activeQuery = -1;
    }
}

void PerformanceMonitor::recordChannel(int channel,
                                       const ChannelPerformanceSample& sample) {
    if (channel < 0 || channel >= channelCount_) return;
    channelLatest_[channel] = sample;
    if (!running_ || elapsedSeconds() < config_.warmupSeconds) return;
    const float values[] = {
        sample.decoderMs, sample.videoRenderMs, sample.readbackMs, sample.cvMs,
        sample.scoreMs, sample.oscMs, sample.drawMs, sample.totalMs
    };
    for (int i = 0; i < 8; ++i) {
        subsystemTotals_[i] += values[i];
        ++subsystemCounts_[i];
    }
    channelTotals_[channel] += sample.totalMs;
    ++channelCounts_[channel];
    if (sample.scoreMode >= 0 && sample.scoreMode < 16) {
        modeTotals_[sample.scoreMode] += sample.scoreMs;
        ++modeCounts_[sample.scoreMode];
    }
}

void PerformanceMonitor::recordChannelDraw(int channel, float milliseconds) {
    if (channel < 0 || channel >= channelCount_) return;
    channelLatest_[channel].drawMs = milliseconds;
    if (running_ && elapsedSeconds() >= config_.warmupSeconds) {
        subsystemTotals_[6] += milliseconds;
        ++subsystemCounts_[6];
    }
}

void PerformanceMonitor::start(const std::vector<Channel*>& channels,
                               VisualComposer* composer) {
    reset();
    savedForcedModes_.reserve(channels.size());
    for (auto* channel : channels) {
        savedForcedModes_.push_back(
            channel ? channel->getScore().params().forcedMode : -1);
    }
    savedComposerEnabled_ = composer && composer->params().enabled;
    startedAt_ = ofGetElapsedTimef();
    testStartedAt_ = startedAt_;
    previousCpuSeconds_ = cpuSecondsNow();
    lastSystemSampleAt_ = startedAt_;
    lastStatusLogAt_ = startedAt_;
    residentMemoryMB_ = residentMemoryNowMB();
    startMemoryMB_ = residentMemoryMB_;
    memoryBaselineCaptured_ = false;
    running_ = true;
    phase_ = -1;
}

void PerformanceMonitor::stop(const std::vector<Channel*>& channels,
                              VisualComposer* composer, bool completed) {
    if (!running_) return;
    running_ = false;
    completed_ = completed;
    for (std::size_t i = 0; i < channels.size() &&
                            i < savedForcedModes_.size(); ++i) {
        if (channels[i])
            channels[i]->getScore().params().forcedMode = savedForcedModes_[i];
    }
    if (composer) composer->params().enabled = savedComposerEnabled_;
    refreshSummaries();
    refreshSystemMetrics();
    evaluate();
    hasResults_ = true;
    exportReport();
}

void PerformanceMonitor::reset() {
    running_ = false;
    hasResults_ = false;
    completed_ = false;
    phase_ = -1;
    failureReasons_.clear();
    lastReportPath_.clear();
    subsystemTotals_.fill(0.0);
    subsystemCounts_.fill(0);
    channelTotals_.fill(0.0);
    channelCounts_.fill(0);
    modeTotals_.fill(0.0);
    modeCounts_.fill(0);
    for (auto& data : windows_) {
        data.rollingFrames.clear();
        data.testFrames.clear();
        for (auto& phaseFrames : data.phaseFrames) phaseFrames.clear();
        data.summary = {};
        data.lastFrameAt = 0.0;
    }
}

void PerformanceMonitor::update(const std::vector<Channel*>& channels,
                                VisualComposer* composer) {
    refreshSummaries();
    refreshSystemMetrics();
    if (!running_) return;

    const float elapsed = elapsedSeconds();
    if (!memoryBaselineCaptured_ && elapsed >= config_.warmupSeconds) {
        residentMemoryMB_ = residentMemoryNowMB();
        startMemoryMB_ = residentMemoryMB_;
        memoryBaselineCaptured_ = true;
    }
    const float measuredDuration =
        std::max(1.f, config_.durationSeconds - config_.warmupSeconds);
    const int nextPhase = elapsed < config_.warmupSeconds
        ? 0
        : 1 + std::min(4, static_cast<int>(
              ((elapsed - config_.warmupSeconds) / measuredDuration) * 5.f));
    if (nextPhase != phase_) {
        phase_ = nextPhase;
        applyStressPhase(phase_, channels, composer);
    }
    const double now = ofGetElapsedTimef();
    if (now - lastStatusLogAt_ >= config_.reportIntervalSeconds) {
        ofLogNotice("PerformanceMonitor")
            << "elapsed=" << ofToString(elapsed, 1)
            << "s phase=" << currentPhaseName()
            << " window0=" << ofToString(windows_[0].summary.fps, 2)
            << "fps CPU=" << ofToString(cpuPercent_, 1)
            << "% memory=" << ofToString(residentMemoryMB_, 1) << "MB";
        lastStatusLogAt_ = now;
    }
    if (elapsed >= config_.durationSeconds) stop(channels, composer, true);
}

void PerformanceMonitor::applyStressPhase(
    int phase, const std::vector<Channel*>& channels, VisualComposer* composer) {
    int forcedMode = static_cast<int>(ScoreMode::VideoNormal);
    if (phase == 2) forcedMode = static_cast<int>(ScoreMode::VideoLines);
    if (phase == 3) forcedMode = static_cast<int>(ScoreMode::VideoNumbers);
    if (phase == 4) forcedMode = static_cast<int>(ScoreMode::SlitScan);

    if (phase == 5 && composer) {
        composer->params().enabled = true;
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
            composer->forceGenerator(
                i, static_cast<GeneratorMode>(i % composer->params().generatorCount));
        }
        return;
    }

    if (composer) composer->params().enabled = false;
    for (auto* channel : channels) {
        if (!channel) continue;
        channel->getScore().params().forcedMode = forcedMode;
        if (phase == 4) channel->loadNextClip();
    }
}

float PerformanceMonitor::elapsedSeconds() const {
    if (startedAt_ <= 0.0) return 0.f;
    const double end = running_ ? ofGetElapsedTimef() : lastSystemSampleAt_;
    return static_cast<float>(std::max(0.0, end - startedAt_));
}

float PerformanceMonitor::progress() const {
    return ofClamp(elapsedSeconds() / std::max(1.f, config_.durationSeconds),
                   0.f, 1.f);
}

std::string PerformanceMonitor::currentPhaseName() const {
    static const char* names[] = {
        "Warm-up / normal video", "Normal video", "VideoLines + CV",
        "VideoNumbers + CV", "SlitScan + clip playback", "Generators"
    };
    return names[std::clamp(phase_, 0, 5)];
}

void PerformanceMonitor::refreshSummaries() {
    for (auto& data : windows_) {
        std::vector<float> samples(data.rollingFrames.begin(),
                                   data.rollingFrames.end());
        if (samples.empty()) continue;
        double total = 0.0;
        float maximum = 0.f;
        for (float value : samples) {
            total += value;
            maximum = std::max(maximum, value);
        }
        data.summary.averageFrameMs =
            static_cast<float>(total / static_cast<double>(samples.size()));
        data.summary.fps = data.summary.averageFrameMs > 0.f
            ? 1000.f / data.summary.averageFrameMs : 0.f;
        data.summary.p95FrameMs = percentile(samples, 0.95f);
        data.summary.p99FrameMs = percentile(samples, 0.99f);
        data.summary.maximumFrameMs = maximum;

        const auto& full = data.testFrames;
        data.summary.sampleCount = full.size();
        data.summary.lateFrames = 0;
        data.summary.stalls = 0;
        for (float value : full) {
            if (value > config_.thresholds.lateFrameMs)
                ++data.summary.lateFrames;
            if (value > config_.thresholds.stallFrameMs)
                ++data.summary.stalls;
        }
    }
}

void PerformanceMonitor::refreshSystemMetrics() {
    const double now = ofGetElapsedTimef();
    if (now - lastSystemSampleAt_ < 1.0) return;
    const double cpu = cpuSecondsNow();
    const double wall = now - lastSystemSampleAt_;
    if (wall > 0.0)
        cpuPercent_ = static_cast<float>(
            100.0 * (cpu - previousCpuSeconds_) / wall);
    previousCpuSeconds_ = cpu;
    lastSystemSampleAt_ = now;
    residentMemoryMB_ = residentMemoryNowMB();
}

void PerformanceMonitor::evaluate() {
    failureReasons_.clear();
    bool foundWindow = false;
    for (int i = 0; i < kMaxWindows; ++i) {
        const auto& samples = windows_[i].testFrames;
        if (samples.empty()) continue;
        foundWindow = true;
        double total = 0.0;
        float maximum = 0.f;
        std::uint64_t late = 0;
        for (float value : samples) {
            total += value;
            maximum = std::max(maximum, value);
            if (value > config_.thresholds.lateFrameMs) ++late;
        }
        const float average = static_cast<float>(total / samples.size());
        const float fps = average > 0.f ? 1000.f / average : 0.f;
        const float p95 = percentile(samples, 0.95f);
        const float latePercent = 100.f * static_cast<float>(late) /
                                  static_cast<float>(samples.size());
        const std::string prefix = "Window " + ofToString(i) + ": ";
        if (fps < config_.thresholds.minimumFps)
            failureReasons_.push_back(prefix + "average FPS below threshold");
        if (p95 > config_.thresholds.p95FrameMs)
            failureReasons_.push_back(prefix + "p95 frame time too high");
        if (latePercent > config_.thresholds.maximumLatePercent)
            failureReasons_.push_back(prefix + "too many late frames");
        if (maximum > config_.thresholds.stallFrameMs)
            failureReasons_.push_back(prefix + "visible stall detected");
    }
    if (!foundWindow) failureReasons_.push_back("No presentation frame samples");
    if (memoryGrowthMB() > config_.thresholds.maximumMemoryGrowthMB)
        failureReasons_.push_back("Resident memory growth exceeded threshold");
    if (!completed_) failureReasons_.push_back("Test stopped before completion");
}

float PerformanceMonitor::percentile(std::vector<float> values, float p) {
    if (values.empty()) return 0.f;
    std::sort(values.begin(), values.end());
    const std::size_t index = std::min(
        values.size() - 1,
        static_cast<std::size_t>(std::ceil(p * values.size()) - 1));
    return values[index];
}

const WindowPerformanceSummary&
PerformanceMonitor::windowSummary(int window) const {
    static const WindowPerformanceSummary empty;
    return window >= 0 && window < kMaxWindows
        ? windows_[window].summary : empty;
}

const ChannelPerformanceSample&
PerformanceMonitor::channelLatest(int channel) const {
    static const ChannelPerformanceSample empty;
    return channel >= 0 && channel < channelCount_
        ? channelLatest_[channel] : empty;
}

std::string PerformanceMonitor::slowestSubsystem() const {
    int slowest = -1;
    double largest = -1.0;
    for (int i = 0; i < 8; ++i) {
        if (subsystemCounts_[i] == 0) continue;
        const double average =
            subsystemTotals_[i] / static_cast<double>(subsystemCounts_[i]);
        if (average > largest) {
            largest = average;
            slowest = i;
        }
    }
    return slowest >= 0
        ? std::string(kSubsystemNames[slowest]) + " (" +
              ofToString(largest, 2) + " ms avg)"
        : "No measured subsystem";
}

std::string PerformanceMonitor::slowestChannel() const {
    int slowest = -1;
    double largest = -1.0;
    for (int i = 0; i < channelCount_; ++i) {
        if (channelCounts_[i] == 0) continue;
        const double average =
            channelTotals_[i] / static_cast<double>(channelCounts_[i]);
        if (average > largest) {
            largest = average;
            slowest = i;
        }
    }
    return slowest >= 0
        ? "Channel " + ofToString(slowest) + " (" +
              ofToString(largest, 2) + " ms avg)"
        : "No measured channel";
}

std::string PerformanceMonitor::slowestVisualMode() const {
    int slowest = -1;
    double largest = -1.0;
    const int modeCount =
        static_cast<int>(sizeof(kScoreModeNames) / sizeof(kScoreModeNames[0]));
    for (int i = 0; i < modeCount; ++i) {
        if (modeCounts_[i] == 0) continue;
        const double average =
            modeTotals_[i] / static_cast<double>(modeCounts_[i]);
        if (average > largest) {
            largest = average;
            slowest = i;
        }
    }
    return slowest >= 0
        ? std::string(kScoreModeNames[slowest]) + " (" +
              ofToString(largest, 2) + " ms avg)"
        : "No measured visual mode";
}

bool PerformanceMonitor::exportReport(std::string* savedPath) {
    const std::string directory =
        ofFilePath::join(
            ofFilePath::getUserHomeDir(),
            "Documents/PartituraDelJuego/performance_reports");
    if (!ofDirectory::createDirectory(directory, true, true)) return false;
    const std::string stem =
        directory + "/performance_" + ofGetTimestampString("%Y%m%d_%H%M%S");
    const std::string jsonPath = stem + ".json";

    ofJson report;
    report["result"] = passed() ? "PASS" : "FAIL";
    report["completed"] = completed_;
    report["durationSeconds"] = elapsedSeconds();
    report["targetFPS"] = targetFps_;
    report["outputMode"] = outputMode_;
    report["cpuPercentLastSample"] = cpuPercent_;
    report["residentMemoryMB"] = residentMemoryMB_;
    report["memoryGrowthMB"] = memoryGrowthMB();
    report["slowestSubsystem"] = slowestSubsystem();
    report["slowestChannel"] = slowestChannel();
    report["slowestVisualMode"] = slowestVisualMode();
    report["failureReasons"] = failureReasons_;
    report["settings"] = SettingsStore::load();
    report["windows"] = ofJson::array();
    report["phases"] = ofJson::array();
    for (int i = 0; i < kMaxWindows; ++i) {
        const auto& data = windows_[i];
        if (data.testFrames.empty()) continue;
        const auto& summary = data.summary;
        report["windows"].push_back({
            {"index", i}, {"fps", summary.fps},
            {"averageFrameMs", summary.averageFrameMs},
            {"p95FrameMs", percentile(data.testFrames, 0.95f)},
            {"p99FrameMs", percentile(data.testFrames, 0.99f)},
            {"maximumFrameMs", *std::max_element(data.testFrames.begin(),
                                                  data.testFrames.end())},
            {"lateFrames", summary.lateFrames}, {"stalls", summary.stalls},
            {"gpuMsLastSample", summary.gpuMs}
        });
        for (int phase = 0; phase < 6; ++phase) {
            const auto& samples = data.phaseFrames[phase];
            if (samples.empty()) continue;
            double phaseTotal = 0.0;
            for (float value : samples) phaseTotal += value;
            const float phaseAverage =
                static_cast<float>(phaseTotal / samples.size());
            report["phases"].push_back({
                {"window", i},
                {"phase", phase},
                {"averageFps", phaseAverage > 0.f
                    ? 1000.f / phaseAverage : 0.f},
                {"p95FrameMs", percentile(samples, 0.95f)},
                {"maximumFrameMs",
                 *std::max_element(samples.begin(), samples.end())}
            });
        }
    }

    ofBuffer jsonBuffer;
    jsonBuffer.set(report.dump(2) + "\n");
    if (!ofBufferToFile(jsonPath, jsonBuffer)) return false;

    std::string csv =
        "window,fps,average_ms,p95_ms,p99_ms,max_ms,late_frames,stalls,gpu_ms\n";
    for (int i = 0; i < kMaxWindows; ++i) {
        const auto& data = windows_[i];
        if (data.testFrames.empty()) continue;
        const auto& summary = data.summary;
        csv += ofToString(i) + "," + ofToString(summary.fps, 3) + "," +
               ofToString(summary.averageFrameMs, 3) + "," +
               ofToString(percentile(data.testFrames, 0.95f), 3) + "," +
               ofToString(percentile(data.testFrames, 0.99f), 3) + "," +
               ofToString(*std::max_element(data.testFrames.begin(),
                                             data.testFrames.end()), 3) + "," +
               ofToString(summary.lateFrames) + "," +
               ofToString(summary.stalls) + "," +
               ofToString(summary.gpuMs, 3) + "\n";
    }
    ofBuffer csvBuffer;
    csvBuffer.set(csv);
    ofBufferToFile(stem + ".csv", csvBuffer);
    lastReportPath_ = jsonPath;
    if (savedPath) *savedPath = jsonPath;
    return true;
}
