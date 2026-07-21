#include "Channel.h"
#include <fstream>

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

    loadNextClip();
}

void Channel::loadNextClip() {
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
    ofLogNotice("Channel") << "[" << idx_ << "] Playing: " << ofFilePath::getFileName(path);
}

void Channel::update() {
    // #region agent log
    { static bool _once = false; if (!_once && idx_ == 0) { _once = true; std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"A\",\"location\":\"Channel.cpp:update\",\"message\":\"dir_ check\",\"data\":{\"dirNull\":" << (dir_?0:1) << ",\"idx\":" << idx_ << "},\"timestamp\":" << (long long)(ofGetElapsedTimef()*1000) << "}\n"; } }
    // #endregion
    // Advance GlobalDirector state machine (safe to call from every channel — has dt guard)
    if (dir_) {
        dir_->update();
        float effective = baseSpeed_ * dir_->getSpeedMultiplier();
        // #region agent log
        { static long long _lastSpd = 0; long long _now = (long long)(ofGetElapsedTimef()*1000); float _m = dir_->getSpeedMultiplier(); if (idx_ == 0 && _m != 1.0f && _now - _lastSpd > 500) { _lastSpd = _now; std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"D\",\"location\":\"Channel.cpp:update:speed\",\"message\":\"speedMultiplier active\",\"data\":{\"multiplier\":" << _m << ",\"effective\":" << effective << ",\"tPhase\":" << (int)dir_->temporalPhase() << "},\"timestamp\":" << _now << "}\n"; } }
        // #endregion
        // Always apply — AVFoundation can reset rate to 1.0 on loop restarts
        // and the stored speed value may not match what AVPlayer is actually doing.
        player_.setSpeed(effective);
    }

    player_.update();
    if (!player_.isLoaded() || player_.getWidth() == 0) return;

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

    // Send OSC
    if (osc_) {
        data.channelIdx = idx_;
        data.scoreMode  = (int)score_.currentMode();
        osc_->send(data);
    }
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
        // #region agent log
        { static long long _lastClr = 0; long long _now = (long long)(ofGetElapsedTimef()*1000); if (idx_ == 0 && _now - _lastClr > 100) { _lastClr = _now; std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"C\",\"location\":\"Channel.cpp:draw:clear\",\"message\":\"clear overlay drawing\",\"data\":{\"alpha\":" << dir_->getClearAlpha() << ",\"cPhase\":" << (int)dir_->clearPhase() << ",\"r\":" << (int)dir_->getClearColor().r << ",\"g\":" << (int)dir_->getClearColor().g << ",\"b\":" << (int)dir_->getClearColor().b << "},\"timestamp\":" << _now << "}\n"; } }
        // #endregion
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
