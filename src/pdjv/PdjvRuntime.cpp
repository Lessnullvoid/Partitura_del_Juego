#include "PdjvRuntime.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace pdjv {
namespace {

bool readAll(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    in.seekg(0, std::ios::end);
    const auto n = static_cast<size_t>(in.tellg());
    in.seekg(0);
    out.resize(n);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(n));
    return static_cast<bool>(in) || n == 0;
}

template <typename T>
bool loadLE(const uint8_t*& p, const uint8_t* end, T& v) {
    if (p + sizeof(T) > end)
        return false;
    std::memcpy(&v, p, sizeof(T));
    p += sizeof(T);
    return true;
}

std::string slurp(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string jsonString(const std::string& json, const std::string& key) {
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos)
        return {};
    pos = json.find(':', pos);
    pos = json.find('"', pos);
    if (pos == std::string::npos)
        return {};
    auto end = json.find('"', pos + 1);
    return json.substr(pos + 1, end - pos - 1);
}

double jsonNumber(const std::string& json, const std::string& key, double fallback) {
    const std::string pat = "\"" + key + "\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos)
        return fallback;
    pos = json.find(':', pos);
    if (pos == std::string::npos)
        return fallback;
    return std::strtod(json.c_str() + pos + 1, nullptr);
}

}  // namespace

bool PdjvReader::open(const std::string& packageDir) {
    path_ = packageDir;
    error_.clear();
    const std::string json = slurp(packageDir + "/manifest.json");
    if (json.empty()) {
        error_ = "missing manifest.json";
        return false;
    }
    manifest_.format = jsonString(json, "format");
    manifest_.packageId = jsonString(json, "packageId");
    manifest_.sourceId = jsonString(json, "sourceId");
    manifest_.analysisId = jsonString(json, "analysisId");
    manifest_.version = static_cast<int>(jsonNumber(json, "version", -1));
    manifest_.fps = jsonNumber(json, "fps", 30);
    manifest_.frameCount = static_cast<int>(jsonNumber(json, "frameCount", 0));
    manifest_.durationSeconds = jsonNumber(json, "durationSeconds", 0);
    manifest_.sourceWidth = static_cast<int>(jsonNumber(json, "sourceWidth", 1920));
    manifest_.sourceHeight = static_cast<int>(jsonNumber(json, "sourceHeight", 1080));
    manifest_.depthConvention = jsonString(json, "depthConvention");
    if (manifest_.format != "PDJV") {
        error_ = "format must be PDJV";
        return false;
    }
    if (manifest_.version != 0 && manifest_.version != 1) {
        error_ = "unsupported major version " + std::to_string(manifest_.version);
        return false;
    }
    std::vector<uint8_t> indexBytes;
    if (!readAll(packageDir + "/frame_index.bin", indexBytes) ||
        !readAll(packageDir + "/observations.bin", observations_) ||
        !readAll(packageDir + "/masks.bin", masks_) ||
        !readAll(packageDir + "/depth.bin", depths_) ||
        !readAll(packageDir + "/motion.bin", motion_)) {
        error_ = "missing PDJV binary files";
        return false;
    }
    if (indexBytes.size() < 10 || std::memcmp(indexBytes.data(), kIndexMagic, 4) != 0) {
        error_ = "invalid frame_index magic";
        return false;
    }
    const uint8_t* p = indexBytes.data();
    const uint8_t* end = p + indexBytes.size();
    p += 4;
    uint16_t ver = 0;
    uint32_t count = 0;
    if (!loadLE(p, end, ver) || !loadLE(p, end, count)) {
        error_ = "truncated frame_index";
        return false;
    }
    if (ver != kIndexVersion) {
        error_ = "unsupported frame_index version";
        return false;
    }
    index_.clear();
    for (uint32_t i = 0; i < count; ++i) {
        IndexRecord rec;
        if (!loadLE(p, end, rec.frame) || !loadLE(p, end, rec.timestamp) ||
            !loadLE(p, end, rec.offset) || !loadLE(p, end, rec.size)) {
            error_ = "truncated frame_index";
            return false;
        }
        if (rec.offset + rec.size > observations_.size()) {
            error_ = "frame index out of bounds";
            return false;
        }
        index_.push_back(rec);
    }
    return true;
}

bool PdjvReader::readFrame(size_t i, FrameRecord& out) const {
    if (i >= index_.size())
        return false;
    const auto& rec = index_[i];
    const uint8_t* p = observations_.data() + rec.offset;
    const uint8_t* end = p + rec.size;
    out = FrameRecord{};
    out.frame = rec.frame;
    out.timestamp = rec.timestamp;
    auto rd = [&](auto& v) { return loadLE(p, end, v); };
    uint32_t playerCount = 0;
    uint32_t eventCount = 0;
    if (!rd(playerCount) || !rd(out.globalMotionEnergy) ||
        !rd(out.collectiveCentroid[0]) || !rd(out.collectiveCentroid[1]) ||
        !rd(out.collectiveDirection[0]) || !rd(out.collectiveDirection[1]) ||
        !rd(out.collectiveDensity) || !rd(eventCount))
        return false;
    out.eventRefs.resize(eventCount);
    for (uint32_t e = 0; e < eventCount; ++e)
        if (!rd(out.eventRefs[e]))
            return false;
    out.players.resize(playerCount);
    for (uint32_t n = 0; n < playerCount; ++n) {
        auto& pl = out.players[n];
        uint8_t observed = 0;
        uint16_t joints = 0;
        if (!rd(pl.trackingId) || !rd(pl.teamId) || !rd(observed) || !rd(pl.confidence) ||
            !rd(pl.imageCentroid[0]) || !rd(pl.imageCentroid[1]) ||
            !rd(pl.velocity[0]) || !rd(pl.velocity[1]) ||
            !rd(pl.acceleration[0]) || !rd(pl.acceleration[1]))
            return false;
        pl.observed = observed == 1;
        for (int k = 0; k < 4; ++k)
            if (!rd(pl.boundingBox[k]))
                return false;
        if (!rd(pl.fieldPosition[0]) || !rd(pl.fieldPosition[1]) ||
            !rd(pl.groundContact[0]) || !rd(pl.groundContact[1]) ||
            !rd(pl.estimatedScale) || !rd(pl.fieldConfidence) ||
            !rd(pl.maskOffset) || !rd(pl.maskSize) || !rd(pl.maskWidth) || !rd(pl.maskHeight) ||
            !rd(pl.depthOffset) || !rd(pl.depthSize) || !rd(pl.depthWidth) || !rd(pl.depthHeight) ||
            !rd(pl.motionOffset) || !rd(pl.motionSize) || !rd(pl.motionWidth) || !rd(pl.motionHeight) ||
            !rd(joints))
            return false;
        pl.joints.resize(joints);
        for (uint16_t j = 0; j < joints; ++j)
            if (!rd(pl.joints[j].x) || !rd(pl.joints[j].y) || !rd(pl.joints[j].z) || !rd(pl.joints[j].c))
                return false;
        if (pl.maskOffset + pl.maskSize > masks_.size() ||
            pl.depthOffset + pl.depthSize > depths_.size() ||
            pl.motionOffset + pl.motionSize > motion_.size()) {
            return false;
        }
    }
    return true;
}

void FrameCache::bind(std::shared_ptr<PdjvReader> reader) {
    shutdown();
    reader_ = std::move(reader);
    running_ = true;
    thread_ = std::thread([this] { worker(); });
}

void FrameCache::shutdown() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
    std::lock_guard<std::mutex> lock(mu_);
    queue_.clear();
    ready_.clear();
}

void FrameCache::request(double timestamp, float prefetchSeconds) {
    if (!reader_ || reader_->index().empty())
        return;
    size_t idx = 0;
    const auto& index = reader_->index();
    while (idx + 1 < index.size() && index[idx + 1].timestamp <= timestamp)
        ++idx;
    cursor_ = idx;
    std::lock_guard<std::mutex> lock(mu_);
    queue_.clear();
    const double until = timestamp + prefetchSeconds;
    for (size_t i = idx; i < index.size() && index[i].timestamp <= until; ++i) {
        if (!ready_.count(i))
            queue_.push_back(i);
    }
    cv_.notify_one();
}

bool FrameCache::latest(FrameRecord& a, FrameRecord& b, float& alpha) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto ia = ready_.find(cursor_);
    auto ib = ready_.find(cursor_ + 1);
    if (ia == ready_.end())
        return false;
    a = ia->second;
    if (ib == ready_.end()) {
        b = a;
        alpha = 0;
        return true;
    }
    b = ib->second;
    const double dt = b.timestamp - a.timestamp;
    alpha = static_cast<float>((0.0) / std::max(1e-6, dt));
    return true;
}

void FrameCache::worker() {
    while (running_) {
        size_t i = static_cast<size_t>(-1);
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait_for(lock, std::chrono::milliseconds(20), [&] {
                return !running_ || !queue_.empty();
            });
            if (!running_)
                return;
            if (queue_.empty())
                continue;
            i = queue_.front();
            queue_.pop_front();
        }
        FrameRecord rec;
        try {
            if (reader_->readFrame(i, rec)) {
                std::lock_guard<std::mutex> lock(mu_);
                ready_[i] = rec;
                stats.loads++;
                while (ready_.size() > 48) {
                    auto it = ready_.begin();
                    if (it->first + 8 < cursor_)
                        ready_.erase(it);
                    else
                        break;
                }
            }
        } catch (...) {
        }
    }
}

void TimelinePlayer::bind(std::shared_ptr<PdjvReader> reader, std::shared_ptr<FrameCache> cache) {
    reader_ = std::move(reader);
    cache_ = std::move(cache);
    time_ = 0;
}

void TimelinePlayer::stop() {
    playing_ = false;
    time_ = 0;
}

void TimelinePlayer::seek(double seconds) {
    time_ = std::max(0.0, seconds);
    lastEventFrame_ = -1;
}

void TimelinePlayer::update(double dtSeconds) {
    if (!reader_ || !playing_)
        return;
    time_ += dtSeconds * speed_;
    const double dur = reader_->manifest().durationSeconds;
    if (dur > 0 && time_ > dur) {
        if (loop_)
            time_ = std::fmod(time_, dur);
        else {
            time_ = dur;
            playing_ = false;
        }
    }
    if (cache_)
        cache_->request(time_, 1.5f);
}

bool TimelinePlayer::current(FrameRecord& interpolated, FrameRecord& nearest) const {
    if (!reader_ || reader_->index().empty())
        return false;
    const auto& index = reader_->index();
    size_t i = 0;
    while (i + 1 < index.size() && index[i + 1].timestamp <= time_)
        ++i;
    FrameRecord a, b;
    if (!reader_->readFrame(i, a))
        return false;
    nearest = a;
    if (i + 1 < index.size() && reader_->readFrame(i + 1, b)) {
        const float alpha = static_cast<float>((time_ - a.timestamp) / std::max(1e-6, b.timestamp - a.timestamp));
        interpolated = a;
        interpolated.globalMotionEnergy = a.globalMotionEnergy + (b.globalMotionEnergy - a.globalMotionEnergy) * alpha;
        interpolated.players.clear();
        for (const auto& pa : a.players) {
            const auto it = std::find_if(b.players.begin(), b.players.end(), [&](const PlayerObservation& pb) {
                return pb.trackingId == pa.trackingId;
            });
            if (it != b.players.end())
                interpolated.players.push_back(interpolatePlayers(pa, *it, alpha));
            else
                interpolated.players.push_back(pa);
        }
    } else {
        interpolated = a;
    }
    return true;
}

int TimelinePlayer::consumeEventOnce(const FrameRecord& frame) {
    if (frame.eventRefs.empty() || frame.frame == lastEventFrame_)
        return -1;
    lastEventFrame_ = frame.frame;
    return frame.eventRefs.front();
}

PdjvAssetPool::~PdjvAssetPool() {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& kv : items_)
        if (kv.second.cache)
            kv.second.cache->shutdown();
}

PdjvAssetPool::Shared PdjvAssetPool::acquire(const std::string& packageDir) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = items_.find(packageDir);
    if (it != items_.end())
        return it->second;
    Shared shared;
    shared.reader = std::make_shared<PdjvReader>();
    if (!shared.reader->open(packageDir)) {
        return shared;
    }
    shared.cache = std::make_shared<FrameCache>();
    shared.cache->bind(shared.reader);
    shared.timeline = std::make_shared<TimelinePlayer>();
    shared.timeline->bind(shared.reader, shared.cache);
    items_[packageDir] = shared;
    return shared;
}

std::string VolumetricContentDirector::packageForChannel(int channel,
                                                         const std::string& primary,
                                                         const std::string& secondary) const {
    switch (mode_) {
        case ContentMode::Independent:
            return (channel % 2 == 0) ? primary : (secondary.empty() ? primary : secondary);
        case ContentMode::FourPlusFour:
            return channel < 4 ? primary : (secondary.empty() ? primary : secondary);
        case ContentMode::Mixed:
            return channel == 7 && !secondary.empty() ? secondary : primary;
        default:
            return primary;
    }
}

float VolumetricContentDirector::cameraYawOffset(int channel) const {
    if (mode_ == ContentMode::SharedView)
        return 0.f;
    return (channel - 3.5f) * 4.f;
}

}  // namespace pdjv
