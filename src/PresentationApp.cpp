#include "PresentationApp.h"

PresentationApp::PresentationApp(
                                 const std::array<Channel*, kSegments>& channels,
                                 int channelOffset, int totalW, int totalH,
                                 ClipPool* pool, OSCSender* osc, const CVParams& cvp,
                                 GlobalDirector* dir, VideoDirector* videoDir,
                                 VisualComposer* composer,
                                 PerformanceMonitor* performance,
                                 int performanceWindow, int targetFps,
                                 WallLayout layout)
    : channels_(channels), channelOffset_(channelOffset),
      totalW_(totalW), totalH_(totalH),
      segW_(layout == WallLayout::Grid2x2 ? totalW / 2 : totalW / kSegments),
      segH_(layout == WallLayout::Grid2x2 ? totalH / 2 : totalH),
      layout_(layout),
      pool_(pool), osc_(osc), cvp_(cvp), dir_(dir), videoDir_(videoDir),
      composer_(composer), performance_(performance),
      performanceWindow_(performanceWindow), targetFps_(targetFps)
{
}

void PresentationApp::setup() {
    ofSetBackgroundColor(0);
    // Deja un margen pequeño de planificador para que el trabajo de decode/draw
    // no baje la tasa medida de la instalación por debajo de los 30 fps pedidos.
    ofSetFrameRate(targetFps_ + 2);
    ofHideCursor();
    ofLogNotice("PresentationApp") << "Window offset=" << channelOffset_
        << "  size=" << totalW_ << "x" << totalH_
        << "  segW=" << segW_ << "  segH=" << segH_
        << "  layout=" << (layout_ == WallLayout::Grid2x2 ? "Grid2x2" : "Strips4x1");

    for (int i = 0; i < kSegments; i++) {
        if (dir_)      channels_[i]->setGlobalDirector(dir_);
        if (videoDir_) channels_[i]->setVideoDirector(videoDir_);
        if (composer_) channels_[i]->setVisualComposer(composer_);
        if (performance_) channels_[i]->setPerformanceMonitor(performance_);
        channels_[i]->setup(channelOffset_ + i, segW_, segH_, pool_, osc_, cvp_);
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

    if (layout_ == WallLayout::Grid2x2) {
        // Rejilla apaisada 2x2: arriba-izquierda, arriba-derecha, abajo-izquierda, abajo-derecha.
        // Cada cuadrante es segW_ x segH_ (mitad de la ventana en cada dimensión).
        channels_[0]->drawInRegion(0,      0,      segW_, segH_);
        channels_[1]->drawInRegion(segW_,  0,      segW_, segH_);
        channels_[2]->drawInRegion(0,      segH_,  segW_, segH_);
        channels_[3]->drawInRegion(segW_,  segH_,  segW_, segH_);
    } else {
        // Tiras verticales 4x1: paneles verticales con rotación ICUIXIAN 90 deg.
        for (int i = 0; i < kSegments; i++) {
            channels_[i]->drawInRegion(i * segW_, 0, segW_, totalH_);
        }
    }

    if (performance_) {
        performance_->endWindowDraw(
            performanceWindow_,
            static_cast<float>(ofGetElapsedTimeMicros() - started) / 1000.f);
    }
}
