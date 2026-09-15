#include "PresentationApp.h"

PresentationApp::PresentationApp(
                                 const std::array<Channel*, kSegments>& channels,
                                 int channelOffset, int totalW, int totalH,
                                 ClipPool* pool, OSCSender* osc, const CVParams& cvp,
                                 GlobalDirector* dir, VideoDirector* videoDir,
                                 VisualComposer* composer,
                                 PerformanceMonitor* performance,
                                 int performanceWindow, int targetFps,
                                 WallLayout layout,
                                 WallIdentifyState* identify)
    : channels_(channels), channelOffset_(channelOffset),
      totalW_(totalW), totalH_(totalH),
      segW_(layout == WallLayout::Grid2x2 ? totalW / 2 : totalW / kSegments),
      segH_(layout == WallLayout::Grid2x2 ? totalH / 2 : totalH),
      layout_(layout),
      pool_(pool), osc_(osc), cvp_(cvp), dir_(dir), videoDir_(videoDir),
      composer_(composer), performance_(performance),
      performanceWindow_(performanceWindow), targetFps_(targetFps),
      identify_(identify)
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
    if (identify_) identify_->tick(ofGetElapsedTimef());
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

    drawIdentifyOverlay();

    if (performance_) {
        performance_->endWindowDraw(
            performanceWindow_,
            static_cast<float>(ofGetElapsedTimeMicros() - started) / 1000.f);
    }
}

void PresentationApp::drawIdentifyOverlay() {
    if (!identify_) return;
    const bool isWallB = channelOffset_ >= 4;
    if (isWallB ? !identify_->showsWallB() : !identify_->showsWallA()) return;

    if (layout_ == WallLayout::Grid2x2) {
        drawIdentifyCard(0,      0,      segW_, segH_, 0);
        drawIdentifyCard(segW_,  0,      segW_, segH_, 1);
        drawIdentifyCard(0,      segH_,  segW_, segH_, 2);
        drawIdentifyCard(segW_,  segH_,  segW_, segH_, 3);
    } else {
        for (int i = 0; i < kSegments; ++i)
            drawIdentifyCard(i * segW_, 0, segW_, totalH_, i);
    }
}

void PresentationApp::drawIdentifyCard(int x, int y, int w, int h, int slice) {
    const bool isWallB = channelOffset_ >= 4;
    const char wallLetter = isWallB ? 'B' : 'A';
    const int position = slice + 1;
    const int channel = channelOffset_ + slice;
    const int out = slice + 1;
    const int rotation = isWallB
        ? identify_->wallBRotationDegrees.load()
        : identify_->wallARotationDegrees.load();

    static const ofColor kSliceColors[kSegments] = {
        ofColor(220, 170, 28),
        ofColor(36, 168, 92),
        ofColor(46, 108, 214),
        ofColor(196, 58, 148)
    };

    ofPushStyle();
    ofEnableAlphaBlending();
    ofFill();
    ofSetColor(kSliceColors[slice], 235);
    ofDrawRectangle(x, y, w, h);
    ofNoFill();
    ofSetLineWidth(8.f);
    ofSetColor(0, 230);
    ofDrawRectangle(x + 6, y + 6, w - 12, h - 12);
    ofFill();
    ofSetColor(255);

    ofPushMatrix();
    ofTranslate(x + w * 0.5f, y + h * 0.5f);
    if (rotation != 0) ofRotateDeg(static_cast<float>(-rotation));

    const std::string title =
        std::string(1, wallLetter) + ofToString(position);
    const std::string detail =
        "CH" + ofToString(channel) + "   OUT" + ofToString(out);

    ofPushMatrix();
    ofScale(8.f, 8.f);
    const float titleW = static_cast<float>(title.size()) * 8.f;
    ofSetColor(0);
    ofDrawBitmapString(title, -titleW * 0.5f + 0.4f, 4.4f);
    ofSetColor(255);
    ofDrawBitmapString(title, -titleW * 0.5f, 4.f);
    ofPopMatrix();

    ofPushMatrix();
    ofScale(3.f, 3.f);
    const float detailW = static_cast<float>(detail.size()) * 8.f;
    ofSetColor(0);
    ofDrawBitmapString(detail, -detailW * 0.5f + 0.4f, 18.4f);
    ofSetColor(255);
    ofDrawBitmapString(detail, -detailW * 0.5f, 18.f);
    ofPopMatrix();

    ofPopMatrix();
    ofPopStyle();
}
