#include "PresentationApp.h"

PresentationApp::PresentationApp(
                                 const std::array<Channel*, kSegments>& channels,
                                 int channelOffset, int totalW, int totalH,
                                 ClipPool* pool, OSCSender* osc, const CVParams& cvp,
                                 GlobalDirector* dir, VideoDirector* videoDir,
                                 VisualComposer* composer,
                                 PerformanceMonitor* performance,
                                 int performanceWindow, int targetFps)
    : channels_(channels), channelOffset_(channelOffset),
      totalW_(totalW), totalH_(totalH), segW_(totalW / kSegments),
      pool_(pool), osc_(osc), cvp_(cvp), dir_(dir), videoDir_(videoDir),
      composer_(composer), performance_(performance),
      performanceWindow_(performanceWindow), targetFps_(targetFps)
{
}

void PresentationApp::setup() {
    ofSetBackgroundColor(0);
    ofSetFrameRate(targetFps_);
    ofHideCursor();

    for (int i = 0; i < kSegments; i++) {
        if (dir_)      channels_[i]->setGlobalDirector(dir_);
        if (videoDir_) channels_[i]->setVideoDirector(videoDir_);
        if (composer_) channels_[i]->setVisualComposer(composer_);
        if (performance_) channels_[i]->setPerformanceMonitor(performance_);
        channels_[i]->setup(channelOffset_ + i, segW_, totalH_, pool_, osc_, cvp_);
    }
}

void PresentationApp::update() {
    if (performance_) performance_->beginWindowFrame(performanceWindow_);
    const uint64_t started = ofGetElapsedTimeMicros();
    for (int i = 0; i < kSegments; i++) {
        channels_[i]->update();
    }
    if (performance_) {
        performance_->recordWindowUpdate(
            performanceWindow_,
            static_cast<float>(ofGetElapsedTimeMicros() - started) / 1000.f);
    }
}

void PresentationApp::draw() {
    const uint64_t started = ofGetElapsedTimeMicros();
    if (performance_) performance_->beginWindowDraw(performanceWindow_);
    for (int i = 0; i < kSegments; i++) {
        channels_[i]->drawInRegion(i * segW_, 0, segW_, totalH_);
    }
    if (performance_) {
        performance_->endWindowDraw(
            performanceWindow_,
            static_cast<float>(ofGetElapsedTimeMicros() - started) / 1000.f);
    }
}
