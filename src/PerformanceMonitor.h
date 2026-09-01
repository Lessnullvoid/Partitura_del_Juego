#pragma once

#include "ofMain.h"

#include <array>
#include <deque>
#include <string>
#include <vector>

class Channel;
class VisualComposer;

struct ChannelPerformanceSample {
    float totalMs = 0.f;
    float decoderMs = 0.f;
    float videoRenderMs = 0.f;
    float readbackMs = 0.f;
    float cvMs = 0.f;
    float scoreMs = 0.f;
    float oscMs = 0.f;
    float drawMs = 0.f;
    int scoreMode = -1;
    bool analyzed = false;
};

struct PerformanceThresholds {
    float minimumFps = 29.f;
    float p95FrameMs = 38.f;
    float lateFrameMs = 50.f;
    float maximumLatePercent = 0.5f;
    float stallFrameMs = 100.f;
    float maximumMemoryGrowthMB = 256.f;
};

struct PerformanceTestConfig {
    float durationSeconds = 600.f;
    float warmupSeconds = 10.f;
    float reportIntervalSeconds = 5.f;
    PerformanceThresholds thresholds;
};

struct WindowPerformanceSummary {
    float fps = 0.f;
    float averageFrameMs = 0.f;
    float p95FrameMs = 0.f;
    float p99FrameMs = 0.f;
    float maximumFrameMs = 0.f;
    float updateMs = 0.f;
    float drawMs = 0.f;
    float gpuMs = 0.f;
    std::uint64_t sampleCount = 0;
    std::uint64_t lateFrames = 0;
    std::uint64_t stalls = 0;
};

class PerformanceMonitor {
public:
    static constexpr int kMaxWindows = 8;
    static constexpr int kMaxChannels = 8;

    explicit PerformanceMonitor(int channelCount = 8);

    void configure(const PerformanceTestConfig& config, int targetFps,
                   const std::string& outputMode);
    void beginWindowFrame(int window);
    void recordWindowUpdate(int window, float milliseconds);
    void beginWindowDraw(int window);
    void endWindowDraw(int window, float milliseconds);
    void recordChannel(int channel, const ChannelPerformanceSample& sample);
    void recordChannelDraw(int channel, float milliseconds);
    void update(const std::vector<Channel*>& channels, VisualComposer* composer);

    void start(const std::vector<Channel*>& channels, VisualComposer* composer);
    void stop(const std::vector<Channel*>& channels, VisualComposer* composer,
              bool completed = false);
    void reset();
    bool exportReport(std::string* savedPath = nullptr);

    bool isRunning() const { return running_; }
    bool hasResults() const { return hasResults_; }
    bool passed() const { return hasResults_ && failureReasons_.empty(); }
    float elapsedSeconds() const;
    float progress() const;
    int currentPhase() const { return phase_; }
    std::string currentPhaseName() const;
    int targetFps() const { return targetFps_; }
    const PerformanceTestConfig& config() const { return config_; }
    const WindowPerformanceSummary& windowSummary(int window) const;
    const ChannelPerformanceSample& channelLatest(int channel) const;
    float cpuPercent() const { return cpuPercent_; }
    float residentMemoryMB() const { return residentMemoryMB_; }
    float memoryGrowthMB() const {
        return memoryBaselineCaptured_ ? residentMemoryMB_ - startMemoryMB_ : 0.f;
    }
    const std::vector<std::string>& failureReasons() const {
        return failureReasons_;
    }
    std::string slowestSubsystem() const;
    std::string slowestChannel() const;
    std::string slowestVisualMode() const;
    std::string lastReportPath() const { return lastReportPath_; }

private:
    struct WindowData {
        std::deque<float> rollingFrames;
        std::vector<float> testFrames;
        std::array<std::vector<float>, 6> phaseFrames;
        WindowPerformanceSummary summary;
        std::array<GLuint, 3> gpuQueries{{0, 0, 0}};
        std::array<bool, 3> queryPending{{false, false, false}};
        int activeQuery = -1;
        double lastFrameAt = 0.0;
    };

    void refreshSummaries();
    void refreshSystemMetrics();
    void applyStressPhase(int phase, const std::vector<Channel*>& channels,
                          VisualComposer* composer);
    void evaluate();
    static float percentile(std::vector<float> values, float p);

    int channelCount_ = 8;
    int targetFps_ = 30;
    std::string outputMode_;
    PerformanceTestConfig config_;
    std::array<WindowData, kMaxWindows> windows_;
    std::array<ChannelPerformanceSample, kMaxChannels> channelLatest_;
    std::array<double, 8> subsystemTotals_{{0.0}};
    std::array<std::uint64_t, 8> subsystemCounts_{{0}};
    std::array<double, kMaxChannels> channelTotals_{{0.0}};
    std::array<std::uint64_t, kMaxChannels> channelCounts_{{0}};
    std::array<double, 16> modeTotals_{{0.0}};
    std::array<std::uint64_t, 16> modeCounts_{{0}};
    std::vector<int> savedForcedModes_;
    bool savedComposerEnabled_ = false;
    bool running_ = false;
    bool hasResults_ = false;
    bool completed_ = false;
    int phase_ = -1;
    double startedAt_ = 0.0;
    double testStartedAt_ = 0.0;
    double lastSystemSampleAt_ = 0.0;
    double previousCpuSeconds_ = 0.0;
    double lastStatusLogAt_ = 0.0;
    float cpuPercent_ = 0.f;
    float residentMemoryMB_ = 0.f;
    float startMemoryMB_ = 0.f;
    bool memoryBaselineCaptured_ = false;
    std::vector<std::string> failureReasons_;
    std::string lastReportPath_;
};
