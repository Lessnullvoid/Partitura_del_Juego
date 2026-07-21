#include "GraphicScore.h"

// ---- Color palette for mark cycling ------------------------------------------
// Order: white → red → electric blue → repeat
static const ofColor kMarkColors[] = {
    ofColor(255, 255, 255),   // white
    ofColor(220, 30,  30),    // red
    ofColor(30,  140, 255),   // electric blue
};
static constexpr int kNumColors = 3;

/* static */ ofColor GraphicScore::nextMarkColor(const ofColor& cur) {
    for (int i = 0; i < kNumColors; i++) {
        if (kMarkColors[i] == cur)
            return kMarkColors[(i + 1) % kNumColors];
    }
    return kMarkColors[0];
}

// ---- Cover-crop layout -------------------------------------------------------
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

// ---- Setup -------------------------------------------------------------------
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

    // Small font for dense binary rows
    monoFont_.load(OF_TTF_MONO, 10, true, true);
    // Larger font for blob IDs, coordinates, numbers
    labelFont_.load(OF_TTF_MONO, 18, true, true);

    buildSequence();
    lastTime_ = ofGetElapsedTimef();
}

// ---- Sequence ----------------------------------------------------------------
void GraphicScore::buildSequence() {
    // BwClean after every mode gives breathing room and visual contrast.
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
        ScoreMode::SlitScan,          // temporal time-scroll #1
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
        ScoreMode::SlitScan,          // temporal time-scroll #2
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

// ---- Events ------------------------------------------------------------------
void GraphicScore::onCollision() {
    if (preFlashMode_ >= 0) return;
    preFlashMode_ = (int)director_;
    flashTimer_   = 0.0;
}

void GraphicScore::onClipChange() {
    strobeAlpha_ = 255.f;
    // Alternate between white and red for variety
    strobeColor_ = (ofRandom(1.f) > 0.5f)
                   ? ofColor(255, 255, 255)
                   : ofColor(220, 25, 25);
}

// ---- Director ----------------------------------------------------------------
void GraphicScore::advanceDirector(double dt) {
    if (preFlashMode_ >= 0) {
        flashTimer_ += dt;
        if (flashTimer_ >= params_.flashDuration) {
            director_     = (ScoreMode)preFlashMode_;
            preFlashMode_ = -1;
        }
        return;
    }

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
        // Cycle mark color on every mode transition
        params_.markColor = nextMarkColor(params_.markColor);
    }
}

// ---- Update ------------------------------------------------------------------
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

    // Slit-scan must update its own ring buffer BEFORE the score FBO opens
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
            case ScoreMode::BwClean:      /* base only */         break;
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

    // Persistent data HUD — always drawn on top of every mode
    renderDataHUD(cv.getData());

    // Clip-change strobe: fades out fast (~0.25 s) over whatever is showing
    if (strobeAlpha_ > 0.f) {
        ofSetColor(strobeColor_.r, strobeColor_.g, strobeColor_.b, (int)strobeAlpha_);
        ofDrawRectangle(0, 0, (float)w_, (float)h_);
        strobeAlpha_ -= (float)(dt * 1100.f);
        if (strobeAlpha_ < 0.f) strobeAlpha_ = 0.f;
    }

    fbo_.end();
}

// ---- renderBase --------------------------------------------------------------
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

// ---- ScanLine ----------------------------------------------------------------
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

// ---- BBoxTracker -------------------------------------------------------------
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

        // Larger corner ticks
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

        // Large blob ID label
        unsigned int label = finder.getLabel(i);
        float        area  = (float)finder.getContourArea(i);
        std::string  info  = ofToString(label) + "  " + ofToString((int)(area / 100)) + "e2";

        ofSetColor(c.r, c.g, c.b, 255);
        if (labelFont_.isLoaded())
            labelFont_.drawString(info, rx + 3, ry - 6);
        else
            ofDrawBitmapString(info, rx + 3, ry - 6);

        // Velocity arrow
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

    // Ball indicator
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

// ---- BinaryText --------------------------------------------------------------
void GraphicScore::renderBinaryText(CVPipeline& cv) {
    const CVData& data = cv.getData();
    int n = (int)data.blobs.size();

    ofColor c = params_.markColor;
    ofPushStyle();
    ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 220.f));

    // Header line
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

    // Crowd density bar
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

// ---- Waveform ----------------------------------------------------------------
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

// ---- GridData ----------------------------------------------------------------
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

        // Large coordinate readout
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

// ---- Barcode -----------------------------------------------------------------
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

// ---- VideoNormal -------------------------------------------------------------
void GraphicScore::renderVideoNormal() {
    if (!curRawTex_ || !curRawTex_->isAllocated()) return;
    CoverLayout cl = coverLayout();
    curRawTex_->draw(cl.ox, cl.oy, cl.dw, cl.dh);
}

// ---- VideoSquares ------------------------------------------------------------
void GraphicScore::renderVideoSquares(CVPipeline& cv) {
    if (!curRawTex_ || !curRawTex_->isAllocated()) return;

    CoverLayout cl = coverLayout();
    // Source pixel size matching one display pixel
    float sqDisplay = params_.videoSquareSize;
    float sqSource  = sqDisplay / cl.scale;   // source pixels that map to sqDisplay px

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
        float cx = cen.x * scale.x;  // display-space centroid
        float cy = cen.y * scale.y;

        // Destination rect in FBO space
        float dx = ofClamp(cx - sqDisplay * 0.5f, 0.f, (float)w_ - sqDisplay);
        float dy = ofClamp(cy - sqDisplay * 0.5f, 0.f, (float)h_ - sqDisplay);

        // Map display position back to raw texture pixel space
        float srcX = ofClamp((dx - cl.ox) / cl.scale, 0.f, vw - sqSource);
        float srcY = ofClamp((dy - cl.oy) / cl.scale, 0.f, vh - sqSource);

        curRawTex_->drawSubsection(dx, dy, sqDisplay, sqDisplay,
                                    srcX, srcY, sqSource, sqSource);

        // Corner-only border in mark color
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

        // Blob ID in large font
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
// Draw the video as a grid of digits 0-9 whose brightness matches each cell.
// Black background — no B&W base beneath.
void GraphicScore::renderVideoNumbers(CVPipeline& cv) {
    const cv::Mat& gray = cv.getGrayMat();
    if (gray.empty()) return;

    glm::vec2 aScale = cv.getAnalysisScale();  // (4,4) install, (1,1) test

    // Fixed grid: 36 columns × 64 rows across the display FBO
    const int nCols = 36;
    const int nRows = 64;
    float cellW = (float)w_ / nCols;
    float cellH = (float)h_ / nRows;

    // monoFont_ was loaded at size 10; a single char is roughly 6×11 px.
    const float kCharW = 6.0f;
    const float kCharH = 11.0f;
    float fs = std::min(cellW / kCharW, cellH / kCharH);

    ofColor c = params_.markColor;

    // Single matrix transform for the whole grid
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
// Cinematic line-drawing in three layers:
//
//   1. CLD texture   — ofxCv::CLD (FDoG Coherent Line Drawing) on a bilateral-
//                      filtered grayscale frame gives soft interior body lines.
//                      Rendered as a bilinearly-scaled texture so the upscaling
//                      itself produces a gentle, soft-pencil softness.
//
//   2. FG silhouettes — background-subtracted foreground mask → morph close →
//                      findContours → approxPolyDP → ofPolyline::getSmoothed +
//                      getResampledBySpacing → three-pass halo / mid-glow / crisp.
//
//   3. Flow strokes  — Farneback flow field sampled on a grid: short directed
//                      line segments that encode motion direction / speed.
//
// No raw pixel edges. Everything is anti-aliased and intentional.
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
    // Layer 1: CLD — Coherent Line Drawing (FDoG interior lines)
    // =========================================================================
    if (!gray.empty()) {
        // Bilateral filter first: smooths jersey-pattern / grass noise while
        // preserving the body-edge gradients that CLD cares about.
        cv::Mat filtered;
        cv::bilateralFilter(gray, filtered, 7, 50.0, 50.0);

        // CLD: ETF + FDoG gives coherent, artist-quality edge lines
        // halfw=4, smoothPasses=2, sigma1=0.4, sigma2=3, tau=0.97
        cv::Mat cldResult;
        ofxCv::CLD(filtered, cldResult, 4, 2, 0.4, 3.0, 0.97, 0);

        // Threshold: dark pixels are the lines.  OTSU auto-finds the cut point.
        cv::Mat cldLines;
        cv::threshold(cldResult, cldLines, 0, 255,
                      cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

        // Convert to RGB so ofSetColor tinting works in GL4.1
        cv::Mat cldRGB;
        cv::cvtColor(cldLines, cldRGB, cv::COLOR_GRAY2RGB);
        edgesImg_.setFromPixels(cldRGB.data, cldRGB.cols, cldRGB.rows, OF_IMAGE_COLOR);

        // Bilinear upscale gives a pleasantly soft "pencil" quality
        ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 120.f));
        edgesImg_.draw(0, 0, (float)w_, (float)h_);
    }

    // =========================================================================
    // Layer 2: FG silhouette polylines (clean outer player shapes)
    // =========================================================================
    const cv::Mat& fg = cv.getFgMask();
    if (!fg.empty()) {
        // Morphological close fills jersey-gap holes
        cv::Mat fgClean;
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
        cv::morphologyEx(fg, fgClean, cv::MORPH_CLOSE, kernel);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(fgClean, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_TC89_L1);

        // Build smoothed polylines once, draw in multiple passes
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

            // getSmoothed + getResampledBySpacing turns polygon edges into
            // fluid drawing-quality curves (the critical step)
            silhouettes.push_back(
                raw.getSmoothed(9).getResampledBySpacing(5.f));
        }

        ofPushMatrix();
        ofScale(aScale.x, aScale.y);

        // Wide soft halo
        ofSetLineWidth(7.f * invS);
        ofSetColor(c.r, c.g, c.b, 12);
        for (auto& p : silhouettes) p.draw();

        // Mid-glow
        ofSetLineWidth(3.f * invS);
        ofSetColor(c.r, c.g, c.b, 30);
        for (auto& p : silhouettes) p.draw();

        // Crisp ink line
        ofSetLineWidth(1.4f * invS);
        ofSetColor(c.r, c.g, c.b, (int)(params_.markOpacity * 255.f));
        for (auto& p : silhouettes) p.draw();

        ofPopMatrix();
    }

    // =========================================================================
    // Layer 2b: Tracked blob accent (extra crispness on individually-tracked
    //           objects — smoothed ContourFinder polylines)
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
    // Layer 3: Farneback optical flow strokes (motion directionality)
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
// Draws the video through the Ironbow thermal colormap shader, then overlays
// a minimal thermal-camera UI: temperature bar, active blob "heat" markers,
// and the standard data readout.
void GraphicScore::renderThermal(CVPipeline& cv) {
    // Use the cover-cropped FBO texture (raw color, already scaled to fill the FBO)
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
        // Graceful fallback — plain B&W so at least something visible shows
        renderBase(*src);
    }

    // ---- Thermal camera UI overlay ------------------------------------------
    const CVData& data = cv.getData();
    ofColor c = params_.markColor;
    ofPushStyle();

    // Right edge: vertical Ironbow gradient bar (8 px wide)
    int   barW  = 8;
    int   barH  = h_ - 40;
    float barX  = (float)w_ - barW - 8.f;
    float barY  = 20.f;
    for (int py = 0; py < barH; py++) {
        float t = 1.0f - (float)py / (float)(barH - 1);
        // Approximate Ironbow stops: 0=black, 0.2=violet, 0.4=crimson, 0.6=orange, 0.8=yellow, 1=white
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
    // Border
    ofNoFill();
    ofSetColor(c.r, c.g, c.b, 70);
    ofDrawRectangle(barX - 1, barY - 1, barW + 2, barH + 2);

    // Tick showing current energy level
    float energyTick = barY + barH * (1.0f - ofClamp(data.motionEnergy * 4.f, 0.f, 1.f));
    ofFill();
    ofSetLineWidth(1.5f);
    ofSetColor(255, 200);
    ofDrawLine(barX - 6, energyTick, barX - 1, energyTick);

    // HOT / CLD labels
    ofSetColor(c.r, c.g, c.b, 150);
    if (monoFont_.isLoaded()) {
        monoFont_.drawString("HOT", barX - 20, barY + 10);
        monoFont_.drawString("CLD", barX - 20, barY + barH + 6);
    }

    // Blob heat-spot cross-hairs
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

    // Frame counter top-left
    ofSetColor(c.r, c.g, c.b, 140);
    std::string frame = "FR " + ofToString((unsigned long long)ofGetFrameNum() % 10000ULL, 4, '0');
    if (monoFont_.isLoaded())
        monoFont_.drawString(frame, 8, 16);
    else
        ofDrawBitmapString(frame, 8, 12);

    ofPopStyle();
}

// ---- renderDataHUD -----------------------------------------------------------
// Minimal persistent data stream drawn on top of EVERY mode.
// Keeps the "always monitored" feeling even in clean/color modes.
void GraphicScore::renderDataHUD(const CVData& data) {
    ofPushStyle();

    // Left edge: thin vertical energy bar (2 px wide, bottom-anchored)
    float barMaxH = (float)h_ * 0.25f;
    float barH    = ofClamp(data.motionEnergy * 5.f, 0.f, 1.f) * barMaxH;
    ofSetColor(255, 50);
    ofSetLineWidth(2.f);
    ofDrawLine(1.f, (float)h_, 1.f, (float)h_ - barH);

    // Bottom-left: frame number + blob count (monospace, very small, low opacity)
    ofSetColor(255, 55);
    char hud[32];
    snprintf(hud, sizeof(hud), "%04llu N%d", (unsigned long long)(ofGetFrameNum() % 10000), data.blobCount);
    if (monoFont_.isLoaded())
        monoFont_.drawString(hud, 6, (float)h_ - 6);
    else
        ofDrawBitmapString(hud, 6, (float)h_ - 10);

    // Ball crosshair — always shown when ball is detected (any mode)
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

// ---- Flash -------------------------------------------------------------------
void GraphicScore::renderFlash() {
    float alpha = ofMap((float)flashTimer_, 0.f, (float)params_.flashDuration, 200.f, 0.f);
    ofPushStyle();
    ofSetColor(255, (int)alpha);
    ofDrawRectangle(0, 0, (float)w_, (float)h_);
    ofPopStyle();
}

// ---- Draw --------------------------------------------------------------------
void GraphicScore::draw(int x, int y, int w, int h) {
    fbo_.draw(x, y, w, h);
}

// ---- updateSlitFbo -----------------------------------------------------------
// CPU ring buffer: each frame, store the CENTER COLUMN of grayMat_ at the
// current write position, then assemble the full display image by unrolling
// the ring (oldest column on the left, newest on the right).
// Runs BEFORE fbo_.begin() — no FBO synchronisation issues possible.
void GraphicScore::updateSlitFbo(CVPipeline& cv) {
    const cv::Mat& gray = cv.getGrayMat();
    if (gray.empty()) return;

    const int aW = gray.cols;   // e.g. 270
    const int aH = gray.rows;   // e.g. 480

    // Lazy init: match actual analysis resolution
    if (slitMat_.empty() || slitMat_.cols != aW || slitMat_.rows != aH)
        slitMat_ = cv::Mat::zeros(aH, aW, CV_8UC1);

    // Store this frame's center column into the ring buffer slot
    const int srcX = aW / 2;
    for (int y = 0; y < aH; y++)
        slitMat_.at<uint8_t>(y, slitWriteX_) = gray.at<uint8_t>(y, srcX);

    slitWriteX_ = (slitWriteX_ + 1) % aW;

    // Assemble the display matrix: left = oldest, right = newest.
    // Apply a mild brightness gradient (50% at left, 100% at right) so the
    // "past" fades naturally and the time axis reads intuitively.
    cv::Mat assembled(aH, aW, CV_8UC3);
    for (int x = 0; x < aW; x++) {
        int    bufX = (slitWriteX_ + x) % aW;   // oldest first
        float  fade = 0.45f + 0.55f * ((float)x / float(aW - 1));
        for (int y = 0; y < aH; y++) {
            uint8_t v = (uint8_t)(slitMat_.at<uint8_t>(y, bufX) * fade);
            assembled.at<cv::Vec3b>(y, x) = cv::Vec3b(v, v, v);
        }
    }

    // Tint with markColor (reuse for slit) — mix grey channel with mark hue
    ofColor mc = params_.markColor;
    if (mc.r != 255 || mc.g != 255 || mc.b != 255) {
        float mr = mc.r / 255.f, mg = mc.g / 255.f, mb = mc.b / 255.f;
        for (int y = 0; y < aH; y++) {
            for (int x = 0; x < aW; x++) {
                auto& px = assembled.at<cv::Vec3b>(y, x);
                float g = px[0] / 255.f;
                px[0] = (uint8_t)(g * mb * 255.f);
                px[1] = (uint8_t)(g * mg * 255.f);
                px[2] = (uint8_t)(g * mr * 255.f);
            }
        }
    }

    slitImage_.setFromPixels(assembled.data, aW, aH, OF_IMAGE_COLOR);
    slitReady_ = true;
}

// ---- renderSlitScan ----------------------------------------------------------
// Draws the assembled slit-scan texture and overlays a time axis.
void GraphicScore::renderSlitScan() {
    ofPushStyle();

    if (slitReady_) {
        ofSetColor(255);
        // Draw stretched to full score FBO — bilinear upscale softens the columns
        slitImage_.draw(0, 0, (float)w_, (float)h_);
    }

    const ofColor& c = params_.markColor;

    // Right-edge "present" marker
    ofSetLineWidth(1.5f);
    ofSetColor(c.r, c.g, c.b, 200);
    ofDrawLine((float)(w_ - 1), 0, (float)(w_ - 1), (float)h_);

    // Time-axis tick marks along the bottom
    // kSlitW columns ÷ display width → each output pixel = display_w / kSlitW frames
    float framesPerPx = (float)kSlitW / (float)w_;   // frames per display pixel
    float pixPerSec   = 30.f / framesPerPx;           // display pixels per second
    float labelY      = (float)h_ - 6.f;

    ofSetColor(c.r, c.g, c.b, 80);
    for (float t = 1.f; t * pixPerSec <= (float)w_; t += 1.f) {
        float markX = (float)w_ - t * pixPerSec;
        ofSetLineWidth(1.f);
        ofDrawLine(markX, (float)h_, markX, (float)h_ - 8.f);
        ofSetColor(c.r, c.g, c.b, 120);
        if (monoFont_.isLoaded())
            monoFont_.drawString("-" + ofToString((int)t) + "s", markX + 3.f, labelY);
        else
            ofDrawBitmapString("-" + ofToString((int)t) + "s", (int)markX + 3, (int)h_ - 8);
        ofSetColor(c.r, c.g, c.b, 80);
    }

    ofSetColor(c.r, c.g, c.b, 180);
    if (monoFont_.isLoaded()) monoFont_.drawString("NOW",    (float)(w_ - 30), labelY);
    else                      ofDrawBitmapString("NOW",     w_ - 30, (int)h_ - 8);

    ofSetColor(c.r, c.g, c.b, 100);
    if (monoFont_.isLoaded()) monoFont_.drawString("SLIT-SCAN", 8.f, 16.f);
    else                      ofDrawBitmapString("SLIT-SCAN", 8, 12);

    ofPopStyle();
}

// ---- Utility -----------------------------------------------------------------
std::string GraphicScore::toBinary8(int v) {
    std::string s;
    s.reserve(8);
    for (int bit = 7; bit >= 0; bit--)
        s += ((v >> bit) & 1) ? '1' : '0';
    return s;
}
