#include "Channel.h"

// Analysis FBO dimensions: 1/4 of a 1080x1920 display.
// readToPixels at this size costs ~0.5 MB vs ~8 MB at full res (16x faster).
static constexpr int kCvW = 270;
static constexpr int kCvH = 480;

void Channel::setup(int idx, int w, int h, ClipPool* pool, OSCSender* osc, const CVParams& cvp) {
    idx_  = idx;
    w_    = w;
    h_    = h;
    pool_ = pool;
    osc_  = osc;

    // Full-res display FBO — B&W shader renders here
    ofFbo::Settings fs;
    fs.width          = w;
    fs.height         = h;
    fs.internalformat = GL_RGBA8;
    fs.useDepth       = false;
    fs.numSamples     = 0;
    bwFbo_.allocate(fs);

    // Small analysis FBO — only read back to CPU for CV
    ofFbo::Settings cvfs;
    cvfs.width          = kCvW;
    cvfs.height         = kCvH;
    cvfs.internalformat = GL_RGBA8;
    cvfs.useDepth       = false;
    cvfs.numSamples     = 0;
    cvFbo_.allocate(cvfs);

    // CVPipeline sees the small FBO pixels directly (halfRes=false, already at kCvW x kCvH)
    CVParams cvpAnalysis = cvp;
    cvpAnalysis.halfRes  = false;
    cv_.setup(kCvW, kCvH, cvpAnalysis);
    cv_.setDisplaySize(w, h);   // keeps getAnalysisScale() = (4, 4)

    score_.setup(w, h);

    if (videoDir_) {
        applyPendingVideoPlan();
    } else {
        loadNextClip();
    }
}

void Channel::loadNextClip() {
    if (videoDir_) {
        videoDir_->requestNext(idx_);
        applyPendingVideoPlan();
        return;
    }

    std::string path = pool_->getRandomClip(idx_);
    if (path.empty()) {
        ofLogWarning("Channel") << "No clip available for channel " << idx_;
        return;
    }
    currentClip_ = path;
    pool_->setActiveClip(idx_, path);

    player_.stop();
    player_.close();
    bool ok = player_.load(path);
    if (!ok) { ofLogError("Channel") << "[" << idx_ << "] Failed: " << path; return; }
    player_.setLoopState(OF_LOOP_NORMAL);
    player_.play();
    // Restore the current effective speed immediately so the first loop iteration
    // plays at the right rate even before the next update() call.
    if (dir_) {
        float eff = baseSpeed_ * dir_->getSpeedMultiplier();
        player_.setSpeed(eff);
    } else {
        player_.setSpeed(baseSpeed_);
    }
    cv_.reset();
    score_.onClipChange();  // flash strobe on every clip transition
    videoRevision_++;
    CVData& data = const_cast<CVData&>(cv_.getData());
    sendOscSnapshot(data);  // announce the cut immediately, then repeat every frame
    ofLogNotice("Channel") << "[" << idx_ << "] Playing: " << ofFilePath::getFileName(path);
}

void Channel::applyPendingVideoPlan() {
    if (!videoDir_) return;
    const VideoPlan& plan = videoDir_->planFor(idx_);
    if (plan.revision <= 0 || plan.revision == appliedPlanRevision_) return;
    loadVideoPlan(plan);
}

bool Channel::loadVideoPlan(const VideoPlan& plan) {
    if (plan.path.empty()) return false;

    player_.stop();
    player_.close();
    if (!player_.load(plan.path)) {
        ofLogError("Channel") << "[" << idx_ << "] Failed: " << plan.path;
        if (videoDir_) videoDir_->requestNext(idx_);
        return false;
    }

    currentClip_ = plan.path;
    pool_->setActiveClip(idx_, plan.path);
    player_.setLoopState(OF_LOOP_NONE);

    activePlan_ = plan;
    appliedPlanRevision_ = plan.revision;
    planStarted_ = false;
    planFinished_ = false;
    planConfigured_ = false;
    planStartedAt_ = ofGetElapsedTimef();
    lastSyncCorrectionAt_ = 0.f;

    player_.play();
    if (plan.shared) player_.setPaused(true);
    configureLoadedPlan();

    cv_.reset();
    score_.onClipChange();
    videoRevision_++;
    CVData& data = const_cast<CVData&>(cv_.getData());
    sendOscSnapshot(data);
    ofLogNotice("Channel") << "[" << idx_ << "] "
        << VideoDirector::planTypeName(plan.type)
        << (plan.shared ? " shared: " : ": ")
        << ofFilePath::getFileName(plan.path);
    return true;
}

bool Channel::configureLoadedPlan() {
    if (planConfigured_) return true;
    const float duration = std::max(0.f, player_.getDuration());
    if (duration <= 0.f) return false;

    if (activePlan_.type == VideoPlanType::FullVideo) {
        segmentStartSeconds_ = 0.f;
        segmentEndSeconds_ = duration;
    } else {
        const float requested = std::max(0.1f, activePlan_.requestedDuration);
        const float segmentDuration = std::min(requested, duration);
        const float latestStart = std::max(0.f, duration - segmentDuration);
        segmentStartSeconds_ =
            ofClamp(activePlan_.startFraction, 0.f, 1.f) * latestStart;
        segmentEndSeconds_ = segmentStartSeconds_ + segmentDuration;
    }

    player_.setPosition(ofClamp(segmentStartSeconds_ / duration, 0.f, 1.f));
    planConfigured_ = true;
    if (activePlan_.shared) {
        player_.setPaused(true);
        if (videoDir_) {
            videoDir_->reportReady(idx_, appliedPlanRevision_, duration,
                                   segmentStartSeconds_, segmentEndSeconds_);
        }
    } else {
        player_.setPaused(false);
        planStarted_ = true;
        planStartedAt_ = ofGetElapsedTimef();
    }
    return true;
}

void Channel::finishActivePlan() {
    if (planFinished_) return;
    planFinished_ = true;
    player_.setPaused(true);
    if (videoDir_) videoDir_->reportFinished(idx_, appliedPlanRevision_);
}

void Channel::sendOscSnapshot(CVData& data) {
    if (!osc_) return;

    int mode = (int)score_.currentMode();
    if (mode != lastScoreMode_) {
        lastScoreMode_ = mode;
        scoreRevision_++;
    }

    data.channelIdx       = idx_;
    data.frameSequence    = ++oscFrameSequence_;
    data.timestamp        = ofGetElapsedTimef();
    data.scoreMode        = mode;
    data.scoreRevision    = scoreRevision_;
    data.videoName        = ofFilePath::getFileName(currentClip_);
    const float rawPosition = player_.isLoaded() ? player_.getPosition() : 0.f;
    data.videoPosition    = std::isfinite(rawPosition)
        ? ofClamp(rawPosition, 0.f, 1.f)
        : 0.f;
    data.videoDuration    = player_.isLoaded() ? player_.getDuration() : 0.f;
    data.videoRevision    = videoRevision_;
    data.videoPlanType    = (int)activePlan_.type;
    data.videoShared      = activePlan_.shared ? 1 : 0;

    if (dir_) {
        data.temporalPhase   = (int)dir_->temporalPhase();
        data.speedMultiplier = dir_->getSpeedMultiplier();
        data.clearPhase      = (int)dir_->clearPhase();
        data.clearAlpha      = dir_->getClearAlpha() / 255.f;
    } else {
        data.temporalPhase   = 0;
        data.speedMultiplier = 1.f;
        data.clearPhase      = 0;
        data.clearAlpha      = 0.f;
    }

    osc_->send(data);
}

void Channel::update() {
    // Advance GlobalDirector state machine (safe to call from every channel — has dt guard)
    if (dir_) dir_->update();
    if (videoDir_) {
        videoDir_->update(dir_ ? dir_->getSpeedMultiplier() : 1.f);
        applyPendingVideoPlan();
    }

    player_.update();
    if (videoDir_ && !planConfigured_) configureLoadedPlan();

    // Apply speed AFTER update() so it always overwrites any rate reset that
    // AVFoundation's internal loop-restart (OF_LOOP_NORMAL) silently performs.
    // Calling it every frame is intentional: AVFoundation can reset the rate to
    // 1.0 on loop boundaries, and this is the simplest reliable guard.
    const float globalSpeed = dir_ ? dir_->getSpeedMultiplier() : 1.f;
    const float effective = activePlan_.shared
        ? globalSpeed
        : baseSpeed_ * globalSpeed;
    player_.setSpeed(effective);

    if (videoDir_ && planConfigured_ && activePlan_.shared && !planFinished_) {
        if (videoDir_->isSharedPlaying()) {
            const float duration = player_.getDuration();
            const float targetSeconds = videoDir_->sharedTargetSeconds();
            if (!planStarted_) {
                if (duration > 0.f) player_.setPosition(targetSeconds / duration);
                player_.setPaused(false);
                planStarted_ = true;
                planStartedAt_ = ofGetElapsedTimef();
            } else if (duration > 0.f) {
                const float currentSeconds = player_.getPosition() * duration;
                const float now = ofGetElapsedTimef();
                if (std::abs(currentSeconds - targetSeconds) >
                        videoDir_->driftTolerance() &&
                    now - lastSyncCorrectionAt_ >= 0.5f) {
                    player_.setPosition(ofClamp(targetSeconds / duration, 0.f, 1.f));
                    lastSyncCorrectionAt_ = now;
                }
            }
        } else if (!planStarted_) {
            player_.setPaused(true);
        } else if (!videoDir_->isSharedActive()) {
            finishActivePlan();
        }
    } else if (videoDir_ && planConfigured_ && planStarted_ && !planFinished_ &&
               player_.getDuration() > 0.f) {
        const float currentSeconds = player_.getPosition() * player_.getDuration();
        // AVFoundation commonly stops on the last decoded frame before normalized
        // position reaches 1.0, while isPlaying() can remain true. A small
        // time-based tolerance handles both natural ends and planned out-points;
        // getIsMovieDone() is the authoritative fallback for short source clips.
        const bool reachedOut =
            currentSeconds >= std::max(segmentStartSeconds_,
                                       segmentEndSeconds_ - 0.25f);
        const bool movieDone = player_.getIsMovieDone();
        if (reachedOut || movieDone) finishActivePlan();
    }

    if (!player_.isLoaded() || player_.getWidth() == 0) {
        CVData& data = const_cast<CVData&>(cv_.getData());
        sendOscSnapshot(data);
        return;
    }

    // --- Display FBO: B&W shader on full-res video, cover-cropped ---
    bwFbo_.begin();
    ofClear(0);
    {
        float vw = player_.getWidth(),  vh = player_.getHeight();
        float fw = (float)w_,           fh = (float)h_;
        float scale = std::max(fw / vw, fh / vh);   // cover: no black bars, edges clip
        float dw = vw * scale, dh = vh * scale;
        player_.draw((fw - dw) * 0.5f, (fh - dh) * 0.5f, dw, dh);
    }
    bwFbo_.end();

    // --- Analysis FBO: tiny copy for cheap GPU→CPU readback (cover not needed here) ---
    cvFbo_.begin();
    ofClear(0);
    player_.draw(0, 0, kCvW, kCvH);
    cvFbo_.end();

    cvFbo_.readToPixels(grayPixels_);
    grayPixels_.setImageType(OF_IMAGE_GRAYSCALE);

    cv_.update(grayPixels_);

    // Event detection — writes into cv_.getData().events
    CVData& data = const_cast<CVData&>(cv_.getData());
    detector_.update(data);

    // Notify score about collision
    if (data.events.collision) score_.onCollision();

    // Sync score params from CVParams (shader uniforms)
    const CVParams& cvp = cv_.params();
    score_.params().bwThreshold = cvp.bwThreshold;
    score_.params().posterize   = cvp.bwPosterize;
    score_.params().brightness  = cvp.bwBrightness;
    score_.params().contrast    = cvp.bwContrast;
    score_.params().gamma       = cvp.bwGamma;
    score_.params().grain       = cvp.bwGrain;
    score_.params().vignette    = cvp.bwVignette;
    score_.params().sCurve      = cvp.bwSCurve;

    // Pass both bw texture and raw video texture to the score renderer
    score_.update(bwFbo_.getTexture(), player_.getTexture(), cv_);

    // Repeated full-state OSC snapshot: CV, events, video, score, and director.
    sendOscSnapshot(data);
}

void Channel::draw() {
    int sw = ofGetWidth(), sh = ofGetHeight();
    if (sw == w_ && sh == h_) {
        score_.draw(0, 0, w_, h_);
    } else {
        float scl = std::min((float)sw / w_, (float)sh / h_);
        int   dw  = (int)(w_ * scl);
        int   dh  = (int)(h_ * scl);
        int   ox  = (sw - dw) / 2;
        int   oy  = (sh - dh) / 2;
        ofSetColor(0);
        ofDrawRectangle(0, 0, sw, sh);
        ofSetColor(255);
        score_.draw(ox, oy, dw, dh);
    }

    // Global coordinated clear — all screens simultaneously.
    // Must reset GL state: GraphicScore render functions can leave ofNoFill()
    // active, and blend mode may be dirty. Reset explicitly before drawing.
    if (dir_ && dir_->isClearActive()) {
        ofPushStyle();
        ofFill();
        ofEnableAlphaBlending();
        ofColor cc = dir_->getClearColor();
        ofSetColor(cc.r, cc.g, cc.b, (int)dir_->getClearAlpha());
        ofDrawRectangle(0, 0, sw, sh);
        ofPopStyle();
    }

    // Temporal phase indicator — always visible so the user can confirm effects.
    if (dir_) {
        bool slow = dir_->isSlowMo();
        bool fast = dir_->isFastMo();
        if (slow || fast) {
            float spd  = dir_->getSpeedMultiplier();
            std::string label = (slow ? "SLOW " : "FAST ") + ofToString(spd, 2) + "x";
            ofPushStyle();
            ofFill();
            ofEnableAlphaBlending();
            // Dark pill background
            ofSetColor(0, 0, 0, 160);
            ofDrawRectangle(8, sh - 28, 90, 20);
            // Bright text
            ofSetColor(slow ? ofColor(80, 200, 255) : ofColor(255, 160, 40), 240);
            ofDrawBitmapString(label, 12, sh - 13);
            ofPopStyle();
        }
    }
}

void Channel::play()            { player_.play(); }
void Channel::pause()           { player_.setPaused(true); }
void Channel::stop()            { player_.stop(); }
void Channel::setSpeed(float s) {
    baseSpeed_ = s;
    // Effective speed applied in update() where GlobalDirector multiplier is known
}
bool Channel::isPlaying() const { return player_.isPlaying(); }
