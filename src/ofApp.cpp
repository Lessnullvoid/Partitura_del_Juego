#include "ofApp.h"

void ChannelApp::setup() {
    ofSetBackgroundColor(0);
    ofSetFrameRate(targetFps_);
    ofHideCursor();
    if (dir_) ch_->setGlobalDirector(dir_);
    if (videoDir_) ch_->setVideoDirector(videoDir_);
    if (composer_) ch_->setVisualComposer(composer_);
    if (performance_) ch_->setPerformanceMonitor(performance_);
    ch_->setup(idx_, w_, h_, pool_, osc_, cvp_);
}

void ChannelApp::update() {
    if (performance_) performance_->beginWindowFrame(idx_);
    const uint64_t started = ofGetElapsedTimeMicros();
    ch_->update();
    if (performance_) {
        performance_->recordWindowUpdate(
            idx_, static_cast<float>(ofGetElapsedTimeMicros() - started) / 1000.f);
    }
}

void ChannelApp::draw() {
    const uint64_t started = ofGetElapsedTimeMicros();
    if (performance_) performance_->beginWindowDraw(idx_);
    ch_->draw();
    if (performance_) {
        performance_->endWindowDraw(
            idx_, static_cast<float>(ofGetElapsedTimeMicros() - started) / 1000.f);
    }
}
