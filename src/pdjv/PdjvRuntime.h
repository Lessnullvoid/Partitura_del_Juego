#pragma once

#include "PdjvCommon.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <unordered_map>
#include <memory>

namespace pdjv {

class PdjvReader {
public:
    bool open(const std::string& packageDir);
    const Manifest& manifest() const { return manifest_; }
    const std::string& error() const { return error_; }
    size_t frameCount() const { return index_.size(); }
    bool readFrame(size_t i, FrameRecord& out) const;
    const std::vector<IndexRecord>& index() const { return index_; }
    const std::vector<uint8_t>& masks() const { return masks_; }
    const std::vector<uint8_t>& depths() const { return depths_; }
    const std::vector<uint8_t>& motion() const { return motion_; }
    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::string error_;
    Manifest manifest_;
    std::vector<IndexRecord> index_;
    std::vector<uint8_t> observations_;
    std::vector<uint8_t> masks_;
    std::vector<uint8_t> depths_;
    std::vector<uint8_t> motion_;
};

struct CacheStats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> loads{0};
};

class FrameCache {
public:
    FrameCache() = default;
    FrameCache(const FrameCache&) = delete;
    FrameCache& operator=(const FrameCache&) = delete;
    ~FrameCache() { shutdown(); }
    void bind(std::shared_ptr<PdjvReader> reader);
    void request(double timestamp, float prefetchSeconds);
    bool latest(FrameRecord& a, FrameRecord& b, float& alpha) const;
    void shutdown();
    CacheStats stats;

private:
    void worker();
    std::shared_ptr<PdjvReader> reader_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::deque<size_t> queue_;
    std::unordered_map<size_t, FrameRecord> ready_;
    size_t cursor_ = 0;
};

class TimelinePlayer {
public:
    void bind(std::shared_ptr<PdjvReader> reader, std::shared_ptr<FrameCache> cache);
    void play() { playing_ = true; }
    void pause() { playing_ = false; }
    void stop();
    void seek(double seconds);
    void setSpeed(float speed) { speed_ = speed; }
    void setLoop(bool loop) { loop_ = loop; }
    void update(double dtSeconds);
    bool current(FrameRecord& interpolated, FrameRecord& nearest) const;
    double time() const { return time_; }
    bool playing() const { return playing_; }
    int consumeEventOnce(const FrameRecord& frame);

private:
    std::shared_ptr<PdjvReader> reader_;
    std::shared_ptr<FrameCache> cache_;
    double time_ = 0;
    float speed_ = 1;
    bool playing_ = false;
    bool loop_ = true;
    int lastEventFrame_ = -1;
};

class PdjvAssetPool {
public:
    struct Shared {
        std::shared_ptr<PdjvReader> reader;
        std::shared_ptr<FrameCache> cache;
        std::shared_ptr<TimelinePlayer> timeline;
    };
    Shared acquire(const std::string& packageDir);
    size_t size() const { return items_.size(); }
    ~PdjvAssetPool();

private:
    std::mutex mu_;
    std::unordered_map<std::string, Shared> items_;
};

enum class ContentMode { Independent, SharedView, Synchronized, FourPlusFour, Mixed };

class VolumetricContentDirector {
public:
    void setMode(ContentMode m) { mode_ = m; }
    ContentMode mode() const { return mode_; }
    std::string packageForChannel(int channel, const std::string& primary, const std::string& secondary) const;
    float cameraYawOffset(int channel) const;

private:
    ContentMode mode_ = ContentMode::SharedView;
};

}  // namespace pdjv
