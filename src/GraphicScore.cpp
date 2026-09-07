#include "GraphicScore.h"

// ---- Paleta de marcas ---------------------------------------------------------
// Instalación monocroma: las marcas solo recorren niveles de gris.
static const ofColor kMarkColors[] = {
    ofColor(255, 255, 255),
    ofColor(186, 186, 186),
    ofColor(124, 124, 124),
};
static constexpr int kNumColors = 3;

/* static */ ofColor GraphicScore::nextMarkColor(const ofColor& cur) {
    for (int i = 0; i < kNumColors; i++) {
        if (kMarkColors[i] == cur)
            return kMarkColors[(i + 1) % kNumColors];
    }
    return kMarkColors[0];
}

// ---- Disposición cover-crop ---------------------------------------------------
GraphicScore::CoverLayout GraphicScore::coverLayout() const {
    if (!curRawTex_ || !curRawTex_->isAllocated())
        return {0, 0, (float)w_, (float)h_, 1.f};
    float vw = (float)curRawTex_->getWidth();
    float vh = (float)curRawTex_->getHeight();
    float fw = (float)w_, fh = (float)h_;
    float s  = std::max(fw / vw, fh / vh);
    float dw = vw * s, dh = vh * s;
    return { (fw - dw) * 0.5f, (fh - dh) * 0.5f, dw, dh, s };
}

// ---- Configuración ------------------------------------------------------------
void GraphicScore::setup(int w, int h) {
    w_ = w;
    h_ = h;

    ofFbo::Settings fs;
    fs.width          = w;
    fs.height         = h;
    fs.internalformat = GL_RGBA8;
    fs.useDepth       = false;
    fs.numSamples     = 0;
    fbo_.allocate(fs);
    fbo_.begin(); ofClear(0); fbo_.end();

    bwShader_.load("shaders/bw");
    if (!bwShader_.isLoaded())
        ofLogError("GraphicScore") << "bw shader failed to load";

    thermalShader_.load("shaders/bw.vert", "shaders/thermal.frag");
    if (!thermalShader_.isLoaded())
        ofLogError("GraphicScore") << "thermal shader failed to load";

    monoShader_.load("shaders/bw.vert", "shaders/mono.frag");
    if (!monoShader_.isLoaded())
        ofLogError("GraphicScore") << "mono shader failed to load";

    // Fuente pequeña para filas binarias densas
    monoFont_.load(OF_TTF_MONO, 10, true, true);
    // Fuente mayor para IDs de blob, coordenadas y números
    labelFont_.load(OF_TTF_MONO, 18, true, true);

    buildSequence();
    lastTime_ = ofGetElapsedTimef();
}

// ---- Secuencia ----------------------------------------------------------------
void GraphicScore::buildSequence() {
    // BwClean tras cada modo da respiro y contraste visual.
    sequence_ = {
        ScoreMode::BwClean,
        ScoreMode::ScanLine,
        ScoreMode::BwClean,
        ScoreMode::VideoNumbers,
        ScoreMode::BwClean,
        ScoreMode::BBoxTracker,
        ScoreMode::BwClean,
        ScoreMode::ThermalVision,
        ScoreMode::BwClean,
        ScoreMode::VideoNormal,
        ScoreMode::BwClean,
        ScoreMode::VideoLines,
        ScoreMode::BwClean,
        ScoreMode::Waveform,
        ScoreMode::BwClean,
        ScoreMode::VideoSquares,
        ScoreMode::BwClean,
        ScoreMode::BinaryText,
        ScoreMode::BwClean,
        ScoreMode::ThermalVision,
        ScoreMode::BwClean,
        ScoreMode::VideoNumbers,
        ScoreMode::BwClean,
        ScoreMode::GridData,
        ScoreMode::BwClean,
        ScoreMode::VideoNormal,
        ScoreMode::BwClean,
        ScoreMode::VideoLines,
        ScoreMode::BwClean,
        ScoreMode::Barcode,
        ScoreMode::BwClean,
        ScoreMode::ThermalVision,
        ScoreMode::BwClean,
        ScoreMode::VideoSquares,
        ScoreMode::BwClean,
        ScoreMode::ScanLine,
    };
    seqIdx_       = 0;
    director_     = sequence_[0];
    modeDuration_ = ofRandom(params_.minModeDuration, params_.maxModeDuration);
    modeTimer_    = 0.0;
}

// ---- Eventos ------------------------------------------------------------------
void GraphicScore::onCollision() {
    if (preFlashMode_ >= 0) return;
    if (director_ == ScoreMode::SlitScan) return;   // no interrumpir la superposición
    preFlashMode_ = (int)director_;
    flashTimer_   = 0.0;
}

void GraphicScore::onClipChange() {
    if (director_ == ScoreMode::SlitScan) return;   // no interrumpir la superposición
    strobeAlpha_ = 255.f;
    // Alterna entre una exposición completa y una a mitad
    strobeColor_ = (ofRandom(1.f) > 0.5f)
                   ? ofColor(255, 255, 255)
                   : ofColor(132, 132, 132);
}

// ---- Director -----------------------------------------------------------------
void GraphicScore::advanceDirector(double dt) {
    if (preFlashMode_ >= 0) {
        flashTimer_ += dt;
        if (flashTimer_ >= params_.flashDuration) {
            director_     = (ScoreMode)preFlashMode_;
            preFlashMode_ = -1;
        }
        return;
    }

    if (params_.forcedMode == (int)ScoreMode::SlitScan)
        params_.forcedMode = -1;

    if (params_.forcedMode >= 0 && params_.forcedMode < (int)ScoreMode::COUNT) {
        director_ = (ScoreMode)params_.forcedMode;
        return;
    }

    modeTimer_ += dt;
    if (modeTimer_ >= modeDuration_) {
        modeTimer_    = 0.0;
        seqIdx_       = (seqIdx_ + 1) % (int)sequence_.size();
        director_     = sequence_[seqIdx_];
        modeDuration_ = ofRandom(params_.minModeDuration, params_.maxModeDuration);
        // Cicla el color de marca en cada transición de modo
        params_.markColor = nextMarkColor(params_.markColor);

        if (director_ == ScoreMode::SlitScan) {
            // Garantiza tiempo suficiente para llenar todas las capas y mostrar el resultado.
            // Mínimo = tiempo de capturar kSlitLayers instantáneas + 12 s de buffer de visualización.
            float fillTime = (float)(kSlitLayers * std::max(1, params_.slitInterval)) / 30.0f;
            modeDuration_ = std::max(modeDuration_, (double)(fillTime + 12.0f));
            // Reinicia ambos buffers para que la cinta + superposición se construyan desde cero.
            slitLayersFilled_ = 0;
            slitLayerWrite_   = 0;
            slitFrameCount_   = 0;
            slitReady_        = false;
            slitGhostMat_     = cv::Mat();
            if (!slitRibbonMat_.empty()) slitRibbonMat_.setTo(0);
            slitWriteX_       = 0;
            slitSrcXNorm_     = 0.5f;
            slitStepAccum_    = 0.f;
        }
    }
}

// ---- Update -------------------------------------------------------------------
void GraphicScore::update(const ofTexture& bwTex, const ofTexture& rawTex, CVPipeline& cv) {
    double now = ofGetElapsedTimef();
    double dt  = now - lastTime_;
    lastTime_  = now;

    curBwTex_  = &bwTex;
    curRawTex_ = &rawTex;

    advanceDirector(dt);

    energyHistory_.push_back(cv.getData().motionEnergy);
    while ((int)energyHistory_.size() > params_.waveformHistory)
        energyHistory_.pop_front();

    ScoreMode active = (preFlashMode_ >= 0) ? ScoreMode::Flash : director_;

    // El slit-scan debe actualizar su buffer circular ANTES de abrir el FBO de la partitura
    if (active == ScoreMode::SlitScan)
        updateSlitFbo(cv);

    fbo_.begin();
    ofClear(0, 0, 0, 255);

    if (active == ScoreMode::VideoNormal) {
        renderVideoNormal();
    } else if (active == ScoreMode::VideoNumbers) {
        renderVideoNumbers(cv);
    } else if (active == ScoreMode::VideoLines) {
        renderVideoLines(cv);
    } else if (active == ScoreMode::ThermalVision) {
        renderThermal(cv);
    } else if (active == ScoreMode::SlitScan) {
        renderSlitScan();
    } else {
        renderBase(bwTex);
        switch (active) {
            case ScoreMode::BwClean:      /* solo base */         break;
            case ScoreMode::ScanLine:     renderScanLine(cv);     break;
            case ScoreMode::BBoxTracker:  renderBBoxTracker(cv);  break;
            case ScoreMode::BinaryText:   renderBinaryText(cv);   break;
            case ScoreMode::Waveform:     renderWaveform();        break;
            case ScoreMode::GridData:     renderGridData(cv);      break;
            case ScoreMode::Barcode:      renderBarcode(cv);       break;
            case ScoreMode::VideoSquares: renderVideoSquares(cv);  break;
            case ScoreMode::Flash:        renderFlash();           break;
            default: break;
        }
    }

    // HUD de datos persistente — siempre dibujado encima de todos los modos
    renderDataHUD(cv.getData());

    // Estroboscopio de cambio de clip: se desvanece rápido (~0.25 s) sobre lo que se ve
    if (strobeAlpha_ > 0.f) {
        ofSetColor(strobeColor_.r, strobeColor_.g, strobeColor_.b, (int)strobeAlpha_);
        ofDrawRectangle(0, 0, (float)w_, (float)h_);
        strobeAlpha_ -= (float)(dt * 1100.f);
        if (strobeAlpha_ < 0.f) strobeAlpha_ = 0.f;
    }

    fbo_.end();
}

void GraphicScore::updateExternal(const ofTexture* texture, float opacity) {
    const double now = ofGetElapsedTimef();
    lastTime_ = now;
    curBwTex_ = nullptr;
    curRawTex_ = nullptr;

    fbo_.begin();
    ofClear(0, 0, 0, 255);
    if (texture && texture->isAllocated()) {
        ofPushStyle();
        ofSetColor(255, 255, 255,
                   static_cast<int>(ofClamp(opacity, 0.f, 1.f) * 255.f));
        texture->draw(0, 0, static_cast<float>(w_), static_cast<float>(h_));
        ofPopStyle();
    }
    fbo_.end();
}

// ---- renderBase ---------------------------------------------------------------
void GraphicScore::renderBase(const ofTexture& videoTex) {
    if (!videoTex.isAllocated()) return;

    if (bwShader_.isLoaded()) {
        bwShader_.begin();
        bwShader_.setUniformTexture("tex0",           videoTex, 0);
        bwShader_.setUniform1f("threshold",      params_.bwThreshold);
        bwShader_.setUniform1f("posterizeLevels",params_.posterize);
        bwShader_.setUniform1f("brightness",     params_.brightness);
        bwShader_.setUniform1f("contrast",       params_.contrast);
        bwShader_.setUniform1f("gamma",          params_.gamma);
        bwShader_.setUniform1f("grain",          params_.grain);
        bwShader_.setUniform1f("vignette",       params_.vignette);
        bwShader_.setUniform1f("sCurve",         params_.sCurve);
        bwShader_.setUniform1f("time",           (float)ofGetElapsedTimef());
        videoTex.draw(0, 0, (float)w_, (float)h_);
        bwShader_.end();
    } else {
        videoTex.draw(0, 0, (float)w_, (float)h_);
    }
}

// ---- ScanLine -----------------------------------------------------------------
void GraphicScore::renderScanLine(CVPipeline& cv) {
    float energy = cv.getData().motionEnergy;
    int   step   = params_.scanStep;

    ofColor c = params_.markColor;
    ofPushStyle();
    ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 180.f));

    for (int y = 0; y < h_; y += step) {
        float phase = sinf((float)y * 0.01f + (float)ofGetElapsedTimef() * 2.f) * 0.5f + 0.5f;
        float lineW = ofMap(energy * phase, 0.f, 0.3f, 0.f, (float)w_, true);
        if (lineW > 1.f)
            ofDrawLine(0, (float)y, lineW, (float)y);
    }

    const EventData& ev = cv.getData().events;
    if (ev.ballDetected) {
        ofSetColor(c.r, c.g, c.b, 255);
        float bx = ev.ballPos.x * (float)w_;
        float by = ev.ballPos.y * (float)h_;
        ofSetLineWidth(2.f);
        ofDrawLine(bx - 12, by, bx + 12, by);
        ofDrawLine(bx, by - 12, bx, by + 12);
    }

    ofPopStyle();
}

// ---- BBoxTracker --------------------------------------------------------------
void GraphicScore::renderBBoxTracker(CVPipeline& cv) {
    glm::vec2 scale = cv.getAnalysisScale();
    ofxCv::ContourFinder& finder = cv.getContour();
    int n = (int)finder.size();
    if (n == 0) return;

    ofColor c = params_.markColor;
    ofPushStyle();
    ofNoFill();
    ofSetLineWidth(1.5f);
    ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 255.f));

    const std::vector<cv::Rect>& rects = cv.getBoundingRects();

    for (int i = 0; i < n && i < (int)rects.size(); i++) {
        float rx = rects[i].x      * scale.x;
        float ry = rects[i].y      * scale.y;
        float rw = rects[i].width  * scale.x;
        float rh = rects[i].height * scale.y;

        ofDrawRectangle(rx, ry, rw, rh);

        // Marcas de esquina más grandes
        float tk = std::min(rw, rh) * 0.18f;
        ofSetLineWidth(3.f);
        ofDrawLine(rx,      ry,      rx + tk, ry);
        ofDrawLine(rx,      ry,      rx,      ry + tk);
        ofDrawLine(rx + rw, ry,      rx + rw - tk, ry);
        ofDrawLine(rx + rw, ry,      rx + rw,      ry + tk);
        ofDrawLine(rx,      ry + rh, rx + tk,      ry + rh);
        ofDrawLine(rx,      ry + rh, rx,            ry + rh - tk);
        ofDrawLine(rx + rw, ry + rh, rx + rw - tk,  ry + rh);
        ofDrawLine(rx + rw, ry + rh, rx + rw,        ry + rh - tk);
        ofSetLineWidth(1.5f);

        // Etiqueta grande del ID de blob
        unsigned int label = finder.getLabel(i);
        float        area  = (float)finder.getContourArea(i);
        std::string  info  = ofToString(label) + "  " + ofToString((int)(area / 100)) + "e2";

        ofSetColor(c.r, c.g, c.b, 255);
        if (labelFont_.isLoaded())
            labelFont_.drawString(info, rx + 3, ry - 6);
        else
            ofDrawBitmapString(info, rx + 3, ry - 6);

        // Flecha de velocidad
        cv::Point2f cen = finder.getCentroid(i);
        cv::Vec2f   vel = finder.getVelocity(i);
        float cx2 = cen.x * scale.x;
        float cy2 = cen.y * scale.y;
        float ex  = cx2 + vel[0] * scale.x * 10.f;
        float ey  = cy2 + vel[1] * scale.y * 10.f;
        ofSetColor(c.r, c.g, c.b, 180);
        ofSetLineWidth(2.f);
        ofDrawLine(cx2, cy2, ex, ey);
        ofDrawCircle(cx2, cy2, 4.f);
        ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 255.f));
    }

    // Indicador de balón
    const EventData& ev = cv.getData().events;
    if (ev.ballDetected) {
        ofSetColor(c.r, c.g, c.b, 255);
        ofSetLineWidth(3.f);
        float bx = ev.ballPos.x * (float)w_;
        float by = ev.ballPos.y * (float)h_;
        ofDrawCircle(bx, by, 8.f);
        if (labelFont_.isLoaded())
            labelFont_.drawString("BALL", bx + 10, by + 6);
        else
            ofDrawBitmapString("BALL", bx + 10, by);
    }

    ofPopStyle();
}

// ---- BinaryText ---------------------------------------------------------------
void GraphicScore::renderBinaryText(CVPipeline& cv) {
    const CVData& data = cv.getData();
    int n = (int)data.blobs.size();

    ofColor c = params_.markColor;
    ofPushStyle();
    ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 220.f));

    // Línea de cabecera
    std::string header = toBinary8(ofGetFrameNum() & 0xFF) + "  N=" + toBinary8(n);
    if (labelFont_.isLoaded())
        labelFont_.drawString(header, 8, 28);
    else
        ofDrawBitmapString(header, 8, 28);

    int lineH = monoFont_.isLoaded() ? 16 : 14;
    for (int i = 0; i < n && i < 16; i++) {
        const auto& b = data.blobs[i];
        int ix  = (int)(b.x  * 255.f);
        int iy  = (int)(b.y  * 255.f);
        int ivx = (int)((b.vx + 0.5f) * 127.f) & 0xFF;
        int ivy = (int)((b.vy + 0.5f) * 127.f) & 0xFF;

        std::string line = toBinary8(ix) + " " + toBinary8(iy) +
                           "  " + toBinary8(ivx) + " " + toBinary8(ivy);

        float py = 52.f + (float)(i * lineH);
        if (monoFont_.isLoaded())
            monoFont_.drawString(line, 8, py);
        else
            ofDrawBitmapString(line, 8, py);
    }

    // Barra de densidad de crowd
    float crowd = data.events.crowdDensity;
    std::string crowdBin = toBinary8((int)(crowd * 255.f));
    float bary = (float)h_ - 40.f;
    if (labelFont_.isLoaded())
        labelFont_.drawString("CROWD " + crowdBin, 8, bary);
    else
        ofDrawBitmapString("CROWD " + crowdBin, 8, bary);
    ofSetLineWidth(2.f);
    ofDrawLine(8, bary + 8, 8 + crowd * 200.f, bary + 8);

    ofPopStyle();
}

// ---- Waveform -----------------------------------------------------------------
void GraphicScore::renderWaveform() {
    if (energyHistory_.empty()) return;

    int n = (int)energyHistory_.size();

    ofColor c = params_.markColor;
    ofPushStyle();
    ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 255.f));
    ofSetLineWidth(1.f);

    for (int i = 0; i < n; i++) {
        float y   = ofMap((float)i, 0.f, (float)n, 0.f, (float)h_);
        float barW = energyHistory_[i] * (float)w_ * 4.f;
        barW = ofClamp(barW, 0.f, (float)w_);
        if (barW > 0.5f)
            ofDrawLine(0, y, barW, y);
    }

    for (int i = std::max(0, n - h_); i < n; i++) {
        float y = (float)(i - (n - h_));
        float x = (float)w_ - 1.f - energyHistory_[i] * 60.f;
        ofDrawRectangle(x, y, 2, 2);
    }

    ofPopStyle();
}

// ---- GridData -----------------------------------------------------------------
void GraphicScore::renderGridData(CVPipeline& cv) {
    int cols = params_.gridCols;
    int rows = params_.gridRows;
    float cw = (float)w_ / (float)cols;
    float ch = (float)h_ / (float)rows;

    const CVData& data = cv.getData();
    ofColor c = params_.markColor;

    ofPushStyle();
    ofNoFill();
    ofSetLineWidth(1.f);

    for (int r = 0; r < rows; r++) {
        for (int col = 0; col < cols; col++) {
            float gx = col * cw + cw * 0.5f;
            float gy = r   * ch + ch * 0.5f;

            float phase = sinf((float)col * 0.3f + (float)r * 0.2f + (float)ofGetElapsedTimef()) * 0.5f + 0.5f;
            float bright = ofClamp(data.motionEnergy * phase * 6.f, 0.f, 1.f);
            ofSetColor(c.r, c.g, c.b, (int)(bright * params_.markOpacity * 255.f));

            float arm = cw * 0.35f * bright;
            ofDrawLine(gx - arm, gy, gx + arm, gy);
            ofDrawLine(gx, gy - arm, gx, gy + arm);
        }
    }

    glm::vec2 scale = cv.getAnalysisScale();
    ofxCv::ContourFinder& finder = cv.getContour();
    ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 255.f));
    ofSetLineWidth(3.f);
    for (int i = 0; i < (int)finder.size(); i++) {
        cv::Point2f cen = finder.getCentroid(i);
        float cx2 = cen.x * scale.x;
        float cy2 = cen.y * scale.y;
        ofDrawLine(cx2 - 16, cy2, cx2 + 16, cy2);
        ofDrawLine(cx2, cy2 - 16, cx2, cy2 + 16);
        ofDrawCircle(cx2, cy2, 4.f);

        // Lectura grande de coordenadas
        std::string pos = ofToString((int)cx2) + "," + ofToString((int)cy2);
        ofSetLineWidth(1.f);
        if (labelFont_.isLoaded())
            labelFont_.drawString(pos, cx2 + 10, cy2 + 8);
        else
            ofDrawBitmapString(pos, cx2 + 10, cy2);
        ofSetLineWidth(3.f);
    }

    ofPopStyle();
}

// ---- Barcode ------------------------------------------------------------------
void GraphicScore::renderBarcode(CVPipeline& cv) {
    const cv::Mat& fgMask = cv.getFgMask();
    if (fgMask.empty()) return;

    int mW = fgMask.cols;
    int mH = fgMask.rows;

    int numBars = 64;
    float barSpacing = (float)w_ / (float)numBars;

    ofColor c = params_.markColor;
    ofPushStyle();
    ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 255.f));
    ofSetLineWidth(barSpacing - 1.f);

    for (int b = 0; b < numBars; b++) {
        int srcX = (int)((float)b / (float)numBars * (float)mW);
        srcX = ofClamp(srcX, 0, mW - 1);

        float colSum = 0.f;
        for (int y = 0; y < mH; y++)
            colSum += fgMask.at<uchar>(y, srcX);

        float colAvg = colSum / (float)(mH * 255);
        float bx     = (float)b * barSpacing + barSpacing * 0.5f;
        float barH   = colAvg * (float)h_;
        float by0    = (float)h_ * 0.5f - barH * 0.5f;
        if (barH > 1.f)
            ofDrawLine(bx, by0, bx, by0 + barH);
    }

    ofPopStyle();
}

// ---- VideoNormal --------------------------------------------------------------
void GraphicScore::renderVideoNormal() {
    if (!curRawTex_ || !curRawTex_->isAllocated()) return;
    CoverLayout cl = coverLayout();
    curRawTex_->draw(cl.ox, cl.oy, cl.dw, cl.dh);
}

// ---- VideoSquares -------------------------------------------------------------
void GraphicScore::renderVideoSquares(CVPipeline& cv) {
    if (!curRawTex_ || !curRawTex_->isAllocated()) return;

    CoverLayout cl = coverLayout();
    // Tamaño de píxel de origen que corresponde a un píxel de visualización
    float sqDisplay = params_.videoSquareSize;
    float sqSource  = sqDisplay / cl.scale;   // píxeles de origen que corresponden a sqDisplay px

    float vw = (float)curRawTex_->getWidth();
    float vh = (float)curRawTex_->getHeight();

    glm::vec2 scale  = cv.getAnalysisScale();
    ofxCv::ContourFinder& finder = cv.getContour();
    int n = (int)finder.size();
    if (n == 0) return;

    ofColor c = params_.markColor;
    ofPushStyle();

    for (int i = 0; i < n && i < params_.videoSquareCount; i++) {
        cv::Point2f cen = finder.getCentroid(i);
        float cx = cen.x * scale.x;  // centroide en espacio de visualización
        float cy = cen.y * scale.y;

        // Rectángulo destino en espacio FBO
        float dx = ofClamp(cx - sqDisplay * 0.5f, 0.f, (float)w_ - sqDisplay);
        float dy = ofClamp(cy - sqDisplay * 0.5f, 0.f, (float)h_ - sqDisplay);

        // Mapea la posición de visualización de vuelta al espacio de píxeles de la textura original
        float srcX = ofClamp((dx - cl.ox) / cl.scale, 0.f, vw - sqSource);
        float srcY = ofClamp((dy - cl.oy) / cl.scale, 0.f, vh - sqSource);

        curRawTex_->drawSubsection(dx, dy, sqDisplay, sqDisplay,
                                    srcX, srcY, sqSource, sqSource);

        // Borde solo en las esquinas, en color de marca
        ofNoFill();
        ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 220.f));
        ofSetLineWidth(2.f);
        float tk = sqDisplay * 0.12f;
        ofDrawLine(dx,             dy,             dx + tk,        dy);
        ofDrawLine(dx,             dy,             dx,              dy + tk);
        ofDrawLine(dx + sqDisplay, dy,             dx + sqDisplay - tk, dy);
        ofDrawLine(dx + sqDisplay, dy,             dx + sqDisplay,  dy + tk);
        ofDrawLine(dx,             dy + sqDisplay, dx + tk,         dy + sqDisplay);
        ofDrawLine(dx,             dy + sqDisplay, dx,              dy + sqDisplay - tk);
        ofDrawLine(dx + sqDisplay, dy + sqDisplay, dx + sqDisplay - tk, dy + sqDisplay);
        ofDrawLine(dx + sqDisplay, dy + sqDisplay, dx + sqDisplay,   dy + sqDisplay - tk);

        // ID de blob en fuente grande
        ofSetColor(c.r, c.g, c.b, 255);
        unsigned int lbl = finder.getLabel(i);
        std::string  tag = ofToString(lbl);
        if (labelFont_.isLoaded())
            labelFont_.drawString(tag, dx + 4, dy + 22);
        else
            ofDrawBitmapString(tag, dx + 4, dy + 12);
    }

    ofPopStyle();
}

// ---- VideoNumbers ------------------------------------------------------------
// Dibuja el vídeo como una cuadrícula de dígitos 0-9 cuyo brillo coincide con cada celda.
// Fondo negro — sin base B&W debajo.
void GraphicScore::renderVideoNumbers(CVPipeline& cv) {
    const cv::Mat& gray = cv.getGrayMat();
    if (gray.empty()) return;

    glm::vec2 aScale = cv.getAnalysisScale();  // (4,4) instalación, (1,1) test

    // Rejilla fija: 36 columnas × 64 filas a lo largo del FBO de visualización
    const int nCols = 36;
    const int nRows = 64;
    float cellW = (float)w_ / nCols;
    float cellH = (float)h_ / nRows;

    // monoFont_ se cargó a tamaño 10; un carácter mide unos 6×11 px.
    const float kCharW = 6.0f;
    const float kCharH = 11.0f;
    float fs = std::min(cellW / kCharW, cellH / kCharH);

    ofColor c = params_.markColor;

    // Una sola transformación de matriz para toda la rejilla
    ofPushMatrix();
    ofScale(fs, fs);

    for (int r = 0; r < nRows; r++) {
        for (int col = 0; col < nCols; col++) {
            int mx = (int)((col + 0.5f) * cellW / aScale.x);
            int my = (int)((r  + 0.5f) * cellH / aScale.y);
            mx = ofClamp(mx, 0, gray.cols - 1);
            my = ofClamp(my, 0, gray.rows - 1);

            float brightness = gray.at<uchar>(my, mx) / 255.f;
            if (brightness < 0.05f) continue;

            int digit = (int)(brightness * 9.9f);

            float px = (col * cellW) / fs;
            float py = (r   * cellH + cellH) / fs;

            ofSetColor(c.r, c.g, c.b, (int)(brightness * params_.markOpacity * 255.f));
            monoFont_.drawString(ofToString(digit), px, py);
        }
    }

    ofPopMatrix();
}

// ---- VideoLines --------------------------------------------------------------
// Dibujo de líneas cinematográfico en tres capas:
//
//   1. Textura CLD   — ofxCv::CLD (FDoG Coherent Line Drawing) sobre un fotograma
//                      en escala de grises filtrado bilateral produce líneas
//                      interiores suaves del cuerpo.
//                      Se renderiza como textura escalada bilinealmente para que
//                      el propio upscaling dé una suavidad de lápiz blando.
//
//   2. Siluetas FG   — máscara de primer plano (sustracción de fondo) → morph close →
//                      findContours → approxPolyDP → ofPolyline::getSmoothed +
//                      getResampledBySpacing → tres pases halo / brillo medio / nítido.
//
//   3. Trazos de flow — campo Farneback muestreado en una rejilla: segmentos
//                      cortos dirigidos que codifican dirección / velocidad del movimiento.
//
// Sin bordes de píxel crudos. Todo está antialiased y es deliberado.
void GraphicScore::renderVideoLines(CVPipeline& cv) {
    const cv::Mat& gray = cv.getGrayMat();
    glm::vec2 aScale = cv.getAnalysisScale();
    float invS = 1.f / std::min(aScale.x, aScale.y);
    ofColor c = params_.markColor;

    ofEnableAntiAliasing();
    ofEnableSmoothing();
    ofPushStyle();
    ofNoFill();

    // =========================================================================
    // Capa 1: CLD — Coherent Line Drawing (líneas interiores FDoG)
    // =========================================================================
    if (!gray.empty()) {
        // Primero filtro bilateral: suaviza ruido de camiseta / césped conservando
        // los gradientes de borde del cuerpo que le importan a CLD.
        cv::Mat filtered;
        cv::bilateralFilter(gray, filtered, 7, 50.0, 50.0);

        // CLD: ETF + FDoG da líneas de borde coherentes, de calidad artística
        // halfw=4, smoothPasses=2, sigma1=0.4, sigma2=3, tau=0.97
        cv::Mat cldResult;
        ofxCv::CLD(filtered, cldResult, 4, 2, 0.4, 3.0, 0.97, 0);

        // Umbral: los píxeles oscuros son las líneas. OTSU encuentra el corte automáticamente.
        cv::Mat cldLines;
        cv::threshold(cldResult, cldLines, 0, 255,
                      cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

        // Convertir a RGB para que el tinte de ofSetColor funcione en GL4.1
        cv::Mat cldRGB;
        cv::cvtColor(cldLines, cldRGB, cv::COLOR_GRAY2RGB);
        edgesImg_.setFromPixels(cldRGB.data, cldRGB.cols, cldRGB.rows, OF_IMAGE_COLOR);

        // El upscale bilineal da una calidad de "lápiz" agradablemente suave
        ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 120.f));
        edgesImg_.draw(0, 0, (float)w_, (float)h_);
    }

    // =========================================================================
    // Capa 2: polilíneas de silueta FG (formas exteriores limpias de los jugadores)
    // =========================================================================
    const cv::Mat& fg = cv.getFgMask();
    if (!fg.empty()) {
        // El cierre morfológico rellena huecos entre camisetas
        cv::Mat fgClean;
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
        cv::morphologyEx(fg, fgClean, cv::MORPH_CLOSE, kernel);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(fgClean, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_TC89_L1);

        // Construye las polilíneas suavizadas una vez; dibuja en varios pases
        std::vector<ofPolyline> silhouettes;
        silhouettes.reserve(contours.size());
        for (auto& cont : contours) {
            if (cv::contourArea(cont) < 300.f) continue;
            std::vector<cv::Point> approx;
            cv::approxPolyDP(cont, approx, 2.5, true);
            if ((int)approx.size() < 3) continue;

            ofPolyline raw;
            for (auto& pt : approx)
                raw.addVertex((float)pt.x, (float)pt.y);
            raw.close();

            // getSmoothed + getResampledBySpacing convierte aristas de polígono
            // en curvas fluidas de calidad de dibujo (el paso crítico)
            silhouettes.push_back(
                raw.getSmoothed(9).getResampledBySpacing(5.f));
        }

        ofPushMatrix();
        ofScale(aScale.x, aScale.y);

        // Halo amplio y suave
        ofSetLineWidth(7.f * invS);
        ofSetColor(c.r, c.g, c.b, 12);
        for (auto& p : silhouettes) p.draw();

        // Brillo medio
        ofSetLineWidth(3.f * invS);
        ofSetColor(c.r, c.g, c.b, 30);
        for (auto& p : silhouettes) p.draw();

        // Línea de tinta nítida
        ofSetLineWidth(1.4f * invS);
        ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 255.f));
        for (auto& p : silhouettes) p.draw();

        ofPopMatrix();
    }

    // =========================================================================
    // Capa 2b: acento de blob rastreado (más nitidez en objetos
    //           rastreados individualmente — polilíneas suavizadas de ContourFinder)
    // =========================================================================
    ofxCv::ContourFinder& finder = cv.getContour();
    int nBlobs = (int)finder.size();
    if (nBlobs > 0) {
        std::vector<ofPolyline> tracked;
        tracked.reserve(nBlobs);
        for (int i = 0; i < nBlobs; i++) {
            ofPolyline raw = finder.getPolyline(i);
            tracked.push_back(raw.getSmoothed(7).getResampledBySpacing(4.f));
        }

        ofPushMatrix();
        ofScale(aScale.x, aScale.y);

        ofSetLineWidth(3.5f * invS);
        ofSetColor(c.r, c.g, c.b, 20);
        for (auto& p : tracked) p.draw();

        ofSetLineWidth(1.7f * invS);
        ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 220.f));
        for (auto& p : tracked) p.draw();

        ofPopMatrix();
    }

    // =========================================================================
    // Capa 3: trazos de flujo óptico Farneback (direccionalidad del movimiento)
    // =========================================================================
    if (!gray.empty()) {
        ofxCv::FlowFarneback& flowObj = cv.getFlow();
        int analysisW = gray.cols;
        int analysisH = gray.rows;
        int step = 10;

        ofSetLineWidth(1.f);
        ofSetColor(c.r, c.g, c.b, 65);

        for (int fy = step / 2; fy < analysisH; fy += step) {
            for (int fx = step / 2; fx < analysisW; fx += step) {
                glm::vec2 off = flowObj.getFlowOffset(fx, fy);
                float mag = glm::length(off);
                if (mag < 0.35f || mag > 14.f) continue;

                float sx = (float)fx * aScale.x;
                float sy = (float)fy * aScale.y;
                float ex = sx + off.x * aScale.x * 3.f;
                float ey = sy + off.y * aScale.y * 3.f;
                ofDrawLine(sx, sy, ex, ey);
            }
        }
    }

    ofPopStyle();
}

// ---- ThermalVision -----------------------------------------------------------
// Dibuja el vídeo con el shader de mapa de color térmico Ironbow y superpone
// una UI mínima de cámara térmica: barra de temperatura, marcadores de "calor"
// de blob activos y la lectura de datos habitual.
void GraphicScore::renderThermal(CVPipeline& cv) {
    // Usa la textura FBO recortada cover (color original, ya escalada para llenar el FBO)
    const ofTexture* src = curBwTex_;
    if (!src || !src->isAllocated()) return;

    if (thermalShader_.isLoaded()) {
        thermalShader_.begin();
        thermalShader_.setUniformTexture("tex0",       *src,              0);
        thermalShader_.setUniform1f("time",             (float)ofGetElapsedTimef());
        thermalShader_.setUniform1f("brightness",       params_.brightness);
        thermalShader_.setUniform1f("contrast",         params_.contrast);
        thermalShader_.setUniform1f("gamma",            params_.gamma);
        thermalShader_.setUniform1f("sCurve",           params_.sCurve);
        src->draw(0, 0, (float)w_, (float)h_);
        thermalShader_.end();
    } else {
        // Respaldo elegante — B&W plano para que al menos se vea algo
        renderBase(*src);
    }

    // ---- Superposición de UI de cámara térmica --------------------------------
    const CVData& data = cv.getData();
    ofColor c = params_.markColor;
    ofPushStyle();

    // Borde derecho: barra vertical de degradado Ironbow (8 px de ancho)
    int   barW  = 8;
    int   barH  = h_ - 40;
    float barX  = (float)w_ - barW - 8.f;
    float barY  = 20.f;
    for (int py = 0; py < barH; py++) {
        float t = 1.0f - (float)py / (float)(barH - 1);
        // Aproximación de paradas Ironbow: 0=negro, 0.2=violeta, 0.4=carmesí, 0.6=naranja, 0.8=amarillo, 1=blanco
        ofColor tc;
        if      (t < 0.2f) { float f = t / 0.2f;
                              tc = ofColor((int)(f*28), (int)(f*10), (int)(f*82)); }
        else if (t < 0.4f) { float f = (t-0.2f)/0.2f;
                              tc = ofColor(28+(int)(f*105), 10+(int)(f*6), 82-(int)(f*33)); }
        else if (t < 0.6f) { float f = (t-0.4f)/0.2f;
                              tc = ofColor(133+(int)(f*102), 16+(int)(f*39), 49-(int)(f*44)); }
        else if (t < 0.8f) { float f = (t-0.6f)/0.2f;
                              tc = ofColor(235+(int)(f*20), 55+(int)(f*159), 5+(int)(f*10)); }
        else                { float f = (t-0.8f)/0.2f;
                              tc = ofColor(255, 214+(int)(f*40), 15+(int)(f*240)); }
        ofSetColor(tc);
        ofDrawRectangle(barX, barY + py, barW, 1);
    }
    // Borde
    ofNoFill();
    ofSetColor(c.r, c.g, c.b, 70);
    ofDrawRectangle(barX - 1, barY - 1, barW + 2, barH + 2);

    // Marca que indica el nivel de energía actual
    float energyTick = barY + barH * (1.0f - ofClamp(data.motionEnergy * 4.f, 0.f, 1.f));
    ofFill();
    ofSetLineWidth(1.5f);
    ofSetColor(255, 200);
    ofDrawLine(barX - 6, energyTick, barX - 1, energyTick);

    // Etiquetas HOT / CLD
    ofSetColor(c.r, c.g, c.b, 150);
    if (monoFont_.isLoaded()) {
        monoFont_.drawString("HOT", barX - 20, barY + 10);
        monoFont_.drawString("CLD", barX - 20, barY + barH + 6);
    }

    // Cruces en los puntos calientes de blob
    glm::vec2 scale  = cv.getAnalysisScale();
    ofxCv::ContourFinder& finder = cv.getContour();
    int nBlobs = (int)finder.size();
    ofSetLineWidth(1.5f);
    for (int i = 0; i < nBlobs && i < 6; i++) {
        cv::Point2f cen = finder.getCentroid(i);
        float bx = cen.x * scale.x;
        float by = cen.y * scale.y;

        float area  = (float)finder.getContourArea(i);
        float heatT = ofClamp(area / 15000.f, 0.f, 1.f);

        ofColor heatC;
        if (heatT < 0.5f)
            heatC = ofColor(220, 30, 10).lerp(ofColor(255, 200, 0), heatT * 2.f);
        else
            heatC = ofColor(255, 200, 0).lerp(ofColor(255, 255, 255), (heatT - 0.5f) * 2.f);

        ofSetColor(heatC, 220);
        ofDrawLine(bx - 12, by, bx + 12, by);
        ofDrawLine(bx, by - 12, bx, by + 12);
        ofDrawCircle(bx, by, 3.f);

        std::string tStr = ofToString((int)(heatT * 99.f)) + "c";
        ofSetColor(heatC, 180);
        if (monoFont_.isLoaded())
            monoFont_.drawString(tStr, bx + 9, by + 4);
        else
            ofDrawBitmapString(tStr, bx + 9, by);
    }

    // Contador de fotogramas arriba a la izquierda
    ofSetColor(c.r, c.g, c.b, 140);
    std::string frame = "FR " + ofToString((unsigned long long)ofGetFrameNum() % 10000ULL, 4, '0');
    if (monoFont_.isLoaded())
        monoFont_.drawString(frame, 8, 16);
    else
        ofDrawBitmapString(frame, 8, 12);

    ofPopStyle();
}

// ---- renderDataHUD -----------------------------------------------------------
// Flujo de datos persistente mínimo dibujado encima de TODOS los modos.
// Mantiene la sensación de "siempre monitorizado" incluso en modos limpios/en color.
void GraphicScore::renderDataHUD(const CVData& data) {
    ofPushStyle();

    // Borde izquierdo: barra vertical fina de energía (2 px de ancho, anclada abajo)
    float barMaxH = (float)h_ * 0.25f;
    float barH    = ofClamp(data.motionEnergy * 5.f, 0.f, 1.f) * barMaxH;
    ofSetColor(255, 50);
    ofSetLineWidth(2.f);
    ofDrawLine(1.f, (float)h_, 1.f, (float)h_ - barH);

    // Abajo a la izquierda: número de fotograma + recuento de blobs (monoespaciado, muy pequeño, baja opacidad)
    ofSetColor(255, 55);
    char hud[32];
    snprintf(hud, sizeof(hud), "%04llu N%d", (unsigned long long)(ofGetFrameNum() % 10000), data.blobCount);
    if (monoFont_.isLoaded())
        monoFont_.drawString(hud, 6, (float)h_ - 6);
    else
        ofDrawBitmapString(hud, 6, (float)h_ - 10);

    // Cruz del balón — siempre visible cuando se detecta el balón (cualquier modo)
    if (data.events.ballDetected) {
        float bx = data.events.ballPos.x * (float)w_;
        float by = data.events.ballPos.y * (float)h_;
        ofSetColor(255, 90);
        ofSetLineWidth(1.f);
        ofDrawLine(bx - 8, by, bx + 8, by);
        ofDrawLine(bx, by - 8, bx, by + 8);
    }

    ofPopStyle();
}

// ---- Flash --------------------------------------------------------------------
void GraphicScore::renderFlash() {
    float alpha = ofMap((float)flashTimer_, 0.f, (float)params_.flashDuration, 200.f, 0.f);
    ofPushStyle();
    ofSetColor(255, (int)alpha);
    ofDrawRectangle(0, 0, (float)w_, (float)h_);
    ofPopStyle();
}

// ---- Draw ---------------------------------------------------------------------
void GraphicScore::draw(int x, int y, int w, int h, float blackLevelCrush) {
    // Todo excepto la nube de puntos llega a las ventanas por esta llamada,
    // el único sitio donde se impone la regla de escala de grises.
    if (!monoShader_.isLoaded()) {
        fbo_.draw(x, y, w, h);
        return;
    }
    monoShader_.begin();
    monoShader_.setUniformTexture("tex0", fbo_.getTexture(), 0);
    monoShader_.setUniform1f("u_blackLevel", blackLevelCrush);
    fbo_.draw(x, y, w, h);
    monoShader_.end();
}

// ---- updateSlitFbo -----------------------------------------------------------
// Dos capas combinadas en un compuesto cinematográfico:
//   1. Cinta   — slit-scan clásico Form+Code: cada fotograma, la columna central
//      del fotograma de análisis en escala de grises se escribe en un buffer
//      circular y el buffer se desenrolla izquierda(más antiguo)->derecha(más nuevo)
//      con un fundido de brillo.
//   2. Fantasmas — superposición estroboscópica: cada slitInterval fotogramas se
//      captura un fotograma gris completo en un buffer circular de kSlitLayers
//      huecos; la capa fantasma (recalculada solo en fotogramas de captura)
//      hace screen-blend de todos los huecos llenos, más antiguo más tenue ->
//      más nuevo más brillante.
// Las dos capas se mezclan con screen-blend cada fotograma para que la
// distorsión ondulante de la cinta se vea mientras las siluetas de cuerpo
// entero congeladas flotan encima.
// Se ejecuta ANTES de fbo_.begin() — no hay problemas de sincronización de FBO.
void GraphicScore::updateSlitFbo(CVPipeline& cv) {
    const cv::Mat& gray = cv.getGrayMat();
    if (gray.empty()) return;

    const int aW = gray.cols;
    const int aH = gray.rows;

    // ---- 1. Cinta: rebanada de muestreo móvil + velocidad de escritura por movimiento --
    // En vez de una columna central fija que avanza a velocidad constante (el
    // aspecto "aburrido, una sola dirección"), la columna muestreada sigue el
    // centroide vivo de movimiento/blob, y el número de columnas escritas por
    // fotograma varía con la energía de movimiento — el movimiento rápido
    // comprime más tiempo en menos espacio (estelas), el calmo estira un
    // instante en más espacio (sostiene). Conserva la estructura clásica
    // Form+Code de la cinta, pero hace reactivos QUÉ se muestrea y a QUÉ
    // VELOCIDAD se desplaza.
    if (slitRibbonMat_.empty() || slitRibbonMat_.cols != aW || slitRibbonMat_.rows != aH)
        slitRibbonMat_ = cv::Mat::zeros(aH, aW, CV_8UC1);

    const CVData& cvData = cv.getData();

    float targetSrcXNorm = 0.5f;
    if (!cvData.blobs.empty()) {
        float sumWX = 0.f, sumW = 0.f;
        for (const auto& b : cvData.blobs) {
            float w = std::max(1.f, b.area);
            sumWX += b.x * w;
            sumW  += w;
        }
        if (sumW > 0.f) targetSrcXNorm = sumWX / sumW;
    }
    // Suaviza hacia el objetivo para que la rebanada derive en vez de temblar.
    slitSrcXNorm_ += (targetSrcXNorm - slitSrcXNorm_) * 0.12f;
    slitSrcXNorm_ = ofClamp(slitSrcXNorm_, 0.05f, 0.95f);
    const int srcX = (int)(slitSrcXNorm_ * (float)(aW - 1));

    // Velocidad de escritura variable: 1..4 columnas/fotograma según la energía
    // de movimiento, acumulada de forma fraccionaria para que la energía baja
    // siga avanzando con suavidad.
    slitStepAccum_ += 1.0f + ofClamp(cvData.motionEnergy * 6.f, 0.f, 3.f);
    int steps = (int)slitStepAccum_;
    slitStepAccum_ -= (float)steps;
    steps = std::max(1, steps);

    for (int s = 0; s < steps; s++) {
        for (int y = 0; y < aH; y++)
            slitRibbonMat_.at<uint8_t>(y, slitWriteX_) = gray.at<uint8_t>(y, srcX);
        slitWriteX_ = (slitWriteX_ + 1) % aW;
    }

    cv::Mat ribbon(aH, aW, CV_8UC1);
    for (int x = 0; x < aW; x++) {
        int   bufX = (slitWriteX_ + x) % aW;                    // el más antiguo primero
        float fade = 0.35f + 0.65f * ((float)x / float(aW - 1));
        for (int y = 0; y < aH; y++)
            ribbon.at<uint8_t>(y, x) = (uint8_t)(slitRibbonMat_.at<uint8_t>(y, bufX) * fade);
    }

    // ---- 2. Fantasmas: captura un fotograma completo cada slitInterval fotogramas --
    slitFrameCount_++;
    const int interval = std::max(1, params_.slitInterval);
    bool captured = false;
    if (slitFrameCount_ >= interval) {
        slitFrameCount_ = 0;
        captured = true;
        gray.copyTo(slitFrames_[slitLayerWrite_]);
        slitLayerWrite_ = (slitLayerWrite_ + 1) % kSlitLayers;
        if (slitLayersFilled_ < kSlitLayers) slitLayersFilled_++;
    }

    // Recalcula la capa fantasma en caché solo cuando se captura una instantánea nueva.
    if (captured || slitGhostMat_.empty()) {
        cv::Mat ghosts = cv::Mat::zeros(aH, aW, CV_8UC1);
        for (int i = 0; i < slitLayersFilled_; i++) {
            // Mapeo del anillo: i=0 es el más antiguo, i=filled-1 es el más nuevo
            int idx = (slitLayerWrite_ - slitLayersFilled_ + i + kSlitLayers) % kSlitLayers;
            float t = (float)(i + 1) / (float)slitLayersFilled_;   // 0..1, el más nuevo = 1
            float w = 0.25f + 0.65f * t;                             // peso: 0.25 -> 0.9

            const cv::Mat& src = slitFrames_[idx];
            for (int y = 0; y < aH; y++) {
                for (int x = 0; x < aW; x++) {
                    float sv = src.at<uint8_t>(y, x) * w / 255.f;   // 0..1 ponderado
                    float ov = ghosts.at<uint8_t>(y, x) / 255.f;    // salida actual 0..1
                    float result = 1.f - (1.f - ov) * (1.f - sv);   // screen blend
                    ghosts.at<uint8_t>(y, x) = (uint8_t)(result * 255.f);
                }
            }
        }
        slitGhostMat_ = ghosts;
    }

    // ---- 3. Combina cinta + fantasmas con screen blend, cada fotograma ---------
    cv::Mat combined(aH, aW, CV_8UC1);
    for (int y = 0; y < aH; y++) {
        for (int x = 0; x < aW; x++) {
            float rv = ribbon.at<uint8_t>(y, x) / 255.f;
            float gv = slitGhostMat_.at<uint8_t>(y, x) / 255.f;
            combined.at<uint8_t>(y, x) = (uint8_t)((1.f - (1.f - rv) * (1.f - gv)) * 255.f);
        }
    }

    // El compuesto permanece estrictamente en escala de grises — sin tinte de
    // markColor, para que la superposición se lea con cualquier color de marca.
    cv::Mat assembled3;
    cv::cvtColor(combined, assembled3, cv::COLOR_GRAY2RGB);

    slitImage_.setFromPixels(assembled3.data, aW, aH, OF_IMAGE_COLOR);
    slitReady_ = true;
}

// ---- renderSlitScan ----------------------------------------------------------
// Dibuja la textura de superposición compuesta y una etiqueta mínima encima.
void GraphicScore::renderSlitScan() {
    ofPushStyle();

    if (slitReady_) {
        ofSetColor(255);
        slitImage_.draw(0, 0, (float)w_, (float)h_);
    }

    // Superposición blanca fija (no markColor) — mantiene el modo monocromo para
    // que el ciclo rojo/azul de color de marca no tiña la superposición.
    ofSetLineWidth(1.5f);
    ofSetColor(255, 255, 255, 200);
    ofDrawLine((float)(w_ - 1), 0, (float)(w_ - 1), (float)h_);

    ofSetColor(255, 255, 255, 100);
    if (monoFont_.isLoaded()) monoFont_.drawString("SLIT-SCAN", 8.f, 16.f);
    else                      ofDrawBitmapString("SLIT-SCAN", 8, 12);

    ofPopStyle();
}

// ---- Utilidad -----------------------------------------------------------------
std::string GraphicScore::toBinary8(int v) {
    std::string s;
    s.reserve(8);
    for (int bit = 7; bit >= 0; bit--)
        s += ((v >> bit) & 1) ? '1' : '0';
    return s;
}
