#include "VideoDirector.h"
#include <algorithm>

namespace {
float randomRange(float a, float b) {
    if (b < a) std::swap(a, b);
    return ofRandom(a, b);
}
}

void VideoDirector::setup(ClipPool* pool, const VideoDirectorParams& params,
                          int channelCount) {
    pool_ = pool;
    params_ = params;
    channelCount_ = ofClamp(channelCount, 1, kChannelCount);
    wasEnabled_ = params_.enabled;
    lastUpdateAt_ = -1.f;
    nextSharedAt_ = 0.f;

    for (int i = 0; i < channelCount_; ++i) {
        waitingForPlan_[i] = true;
        if (params_.enabled) scheduleIndependent(i);
    }
}

void VideoDirector::update(float globalSpeed) {
    const uint64_t frame = ofGetFrameNum();
    if (frame == lastUpdateFrame_) return;
    lastUpdateFrame_ = frame;

    const float now = ofGetElapsedTimef();
    float dt = 0.f;
    if (lastUpdateAt_ < 0.f) {
        scheduleNextShared(now);
    } else {
        dt = std::max(0.f, now - lastUpdateAt_);
    }
    lastUpdateAt_ = now;

    if (params_.enabled && !wasEnabled_) {
        for (int i = 0; i < channelCount_; ++i) {
            if (waitingForPlan_[i]) scheduleIndependent(i);
        }
        scheduleNextShared(now);
    }
    wasEnabled_ = params_.enabled;
    if (!params_.enabled && sharedState_ == SharedState::Idle) return;

    if (params_.enabled && params_.sharedEventsEnabled &&
        sharedState_ == SharedState::Idle && now >= nextSharedAt_) {
        beginShared(now);
    } else if (sharedState_ == SharedState::Loading) {
        bool allReady = true;
        for (const auto& state : ready_) allReady = allReady && state.ready;
        if (allReady) {
            sharedMediaSeconds_ = ready_[0].start;
            sharedEndSeconds_ = ready_[0].end;
            sharedStartAt_ = now + std::max(0.f, params_.sharedStartDelay);
            sharedState_ = SharedState::WaitingToStart;
        }
    } else if (sharedState_ == SharedState::WaitingToStart && now >= sharedStartAt_) {
        sharedState_ = SharedState::Playing;
    } else if (sharedState_ == SharedState::Playing) {
        sharedMediaSeconds_ += dt * std::max(0.f, globalSpeed);
        if (sharedMediaSeconds_ >= sharedEndSeconds_) finishShared(now);
    }
}

const VideoPlan& VideoDirector::planFor(int channelIdx) const {
    static const VideoPlan empty;
    return validChannel(channelIdx) ? plans_[channelIdx] : empty;
}

void VideoDirector::requestNext(int channelIdx) {
    if (!validChannel(channelIdx) || !pool_) return;
    if (isSharedActive()) {
        finishShared(ofGetElapsedTimef());
        return;
    }
    scheduleIndependent(channelIdx);
}

void VideoDirector::triggerSharedNow() {
    if (!pool_ || !params_.sharedEventsEnabled) return;
    beginShared(ofGetElapsedTimef());
}

void VideoDirector::reportReady(int channelIdx, int planRevision, float duration,
                                float segmentStart, float segmentEnd) {
    if (!validChannel(channelIdx)) return;
    const VideoPlan& plan = plans_[channelIdx];
    if (!plan.shared || plan.revision != planRevision ||
        sharedState_ != SharedState::Loading) return;

    ReadyState& state = ready_[channelIdx];
    state.ready = true;
    state.duration = duration;
    state.start = segmentStart;
    state.end = segmentEnd;
}

void VideoDirector::reportFinished(int channelIdx, int planRevision) {
    if (!validChannel(channelIdx) || plans_[channelIdx].revision != planRevision) return;
    if (plans_[channelIdx].shared) return;

    waitingForPlan_[channelIdx] = true;
    if (params_.enabled) scheduleIndependent(channelIdx);
}

float VideoDirector::secondsUntilShared() const {
    if (isSharedActive()) return 0.f;
    return std::max(0.f, nextSharedAt_ - ofGetElapsedTimef());
}

const char* VideoDirector::planTypeName(VideoPlanType type) {
    switch (type) {
        case VideoPlanType::ShortFragment: return "Short";
        case VideoPlanType::LongFragment:  return "Long";
        case VideoPlanType::FullVideo:     return "Full";
    }
    return "Unknown";
}

VideoPlan VideoDirector::makePlan(const std::string& path, bool shared) {
    VideoPlan plan;
    plan.path = path;
    plan.type = chooseType();
    plan.startFraction = ofRandomuf();
    plan.shared = shared;

    if (plan.type == VideoPlanType::ShortFragment) {
        plan.requestedDuration = randomRange(params_.shortMin, params_.shortMax);
    } else if (plan.type == VideoPlanType::LongFragment) {
        plan.requestedDuration = randomRange(params_.longMin, params_.longMax);
    }
    return plan;
}

VideoPlanType VideoDirector::chooseType() const {
    const float shortW = std::max(0.f, params_.shortWeight);
    const float longW = std::max(0.f, params_.longWeight);
    const float fullW = std::max(0.f, params_.fullWeight);
    const float total = shortW + longW + fullW;
    if (total <= 0.f) return VideoPlanType::FullVideo;

    const float pick = ofRandom(total);
    if (pick < shortW) return VideoPlanType::ShortFragment;
    if (pick < shortW + longW) return VideoPlanType::LongFragment;
    return VideoPlanType::FullVideo;
}

void VideoDirector::scheduleIndependent(int channelIdx) {
    if (!validChannel(channelIdx) || !pool_) return;
    const std::string path = pool_->getIndependentClip(channelIdx);
    if (path.empty()) return;

    VideoPlan plan = makePlan(path, false);
    plan.revision = ++nextRevision_[channelIdx];
    plans_[channelIdx] = plan;
    waitingForPlan_[channelIdx] = false;
    pool_->setActiveClip(channelIdx, path);
}

void VideoDirector::scheduleNextShared(float now) {
    nextSharedAt_ = now + randomRange(params_.sharedIntervalMin,
                                      params_.sharedIntervalMax);
}

void VideoDirector::beginShared(float now) {
    // Obtiene un clip por grupo de pool para que los canales verticales (0..kGroupThreshold-1)
    // reciban siempre clips verticales y los horizontales reciban clips horizontales.
    const int threshold = ClipPool::kGroupThreshold;
    const std::string pathA = pool_ ? pool_->getSharedClipForGroup(0) : "";
    const std::string pathB = pool_ ? pool_->getSharedClipForGroup(1) : "";

    if (pathA.empty() && pathB.empty()) {
        scheduleNextShared(now);
        return;
    }

    // Cae al clip vertical para canales horizontales cuando no hay pool
    // horizontal cargado.
    const std::string effectivePathB = pathB.empty() ? pathA : pathB;

    ready_.fill({});

    // Grupo A — canales verticales.
    if (!pathA.empty()) {
        VideoPlan planA = makePlan(pathA, true);
        const int groupAEnd = std::min(threshold, channelCount_);
        for (int i = 0; i < groupAEnd; ++i) {
            planA.revision = ++nextRevision_[i];
            plans_[i] = planA;
            waitingForPlan_[i] = false;
            pool_->setActiveClip(i, pathA);
        }
        ofLogNotice("VideoDirector") << "Shared event group A: "
            << ofFilePath::getFileName(pathA)
            << " (" << planTypeName(planA.type) << ")";
    }

    // Grupo B — canales horizontales (solo cuando el modo de 8 canales está activo).
    if (channelCount_ > threshold && !effectivePathB.empty()) {
        VideoPlan planB = makePlan(effectivePathB, true);
        for (int i = threshold; i < channelCount_; ++i) {
            planB.revision = ++nextRevision_[i];
            plans_[i] = planB;
            waitingForPlan_[i] = false;
            pool_->setActiveClip(i, effectivePathB);
        }
        ofLogNotice("VideoDirector") << "Shared event group B: "
            << ofFilePath::getFileName(effectivePathB)
            << " (" << planTypeName(planB.type) << ")";
    }

    sharedState_ = SharedState::Loading;
}

void VideoDirector::finishShared(float now) {
    sharedState_ = SharedState::Idle;
    scheduleNextShared(now);
    for (int i = 0; i < channelCount_; ++i) {
        waitingForPlan_[i] = true;
        if (params_.enabled) scheduleIndependent(i);
    }
}

bool VideoDirector::validChannel(int channelIdx) const {
    return channelIdx >= 0 && channelIdx < channelCount_;
}
