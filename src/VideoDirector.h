#pragma once
#include "ofMain.h"
#include "ClipPool.h"
#include <array>
#include <limits>

enum class VideoPlanType {
    ShortFragment = 0,
    LongFragment  = 1,
    FullVideo     = 2
};

struct VideoDirectorParams {
    bool  enabled = true;

    float shortWeight = 0.50f;
    float longWeight  = 0.30f;
    float fullWeight  = 0.20f;

    float shortMin = 8.f;
    float shortMax = 25.f;
    float longMin  = 30.f;
    float longMax  = 120.f;

    float sharedIntervalMin = 120.f;
    float sharedIntervalMax = 300.f;
    float sharedStartDelay  = 0.35f;
    float driftTolerance    = 0.08f;
};

struct VideoPlan {
    std::string   path;
    VideoPlanType type = VideoPlanType::ShortFragment;
    float         startFraction = 0.f;
    float         requestedDuration = 0.f;
    int           revision = 0;
    bool          shared = false;
};

class VideoDirector {
public:
    static constexpr int kChannelCount = 4;

    void setup(ClipPool* pool, const VideoDirectorParams& params = {});
    void update(float globalSpeed = 1.f);

    VideoDirectorParams&       params()       { return params_; }
    const VideoDirectorParams& params() const { return params_; }

    const VideoPlan& planFor(int channelIdx) const;
    void requestNext(int channelIdx);
    void triggerSharedNow();

    void reportReady(int channelIdx, int planRevision, float duration,
                     float segmentStart, float segmentEnd);
    void reportFinished(int channelIdx, int planRevision);

    bool  isSharedActive() const { return sharedState_ != SharedState::Idle; }
    bool  isSharedPlaying() const { return sharedState_ == SharedState::Playing; }
    float sharedTargetSeconds() const { return sharedMediaSeconds_; }
    float driftTolerance() const { return params_.driftTolerance; }
    float secondsUntilShared() const;

    static const char* planTypeName(VideoPlanType type);

private:
    enum class SharedState { Idle, Loading, WaitingToStart, Playing };
    struct ReadyState {
        bool  ready = false;
        float duration = 0.f;
        float start = 0.f;
        float end = 0.f;
    };

    VideoPlan makePlan(const std::string& path, bool shared);
    VideoPlanType chooseType() const;
    void scheduleIndependent(int channelIdx);
    void scheduleNextShared(float now);
    void beginShared(float now);
    void finishShared(float now);
    bool validChannel(int channelIdx) const;

    ClipPool* pool_ = nullptr;
    VideoDirectorParams params_;
    std::array<VideoPlan, kChannelCount> plans_;
    std::array<ReadyState, kChannelCount> ready_;
    std::array<bool, kChannelCount> waitingForPlan_ = {};
    std::array<int, kChannelCount> nextRevision_ = {};

    SharedState sharedState_ = SharedState::Idle;
    float nextSharedAt_ = 0.f;
    float sharedStartAt_ = 0.f;
    float sharedMediaSeconds_ = 0.f;
    float sharedEndSeconds_ = 0.f;
    float lastUpdateAt_ = -1.f;
    uint64_t lastUpdateFrame_ = std::numeric_limits<uint64_t>::max();
    bool wasEnabled_ = true;
};
