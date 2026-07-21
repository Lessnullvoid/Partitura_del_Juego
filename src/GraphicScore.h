#pragma once
#include "ofMain.h"
#include "CVPipeline.h"
#include <deque>

enum class ScoreMode {
    BwClean    = 0,   // B&W base only — breathing room
    ScanLine,
    BBoxTracker,
    BinaryText,
    Waveform,
    GridData,
    Barcode,
    VideoNormal,    // raw color video, full frame
    VideoSquares,   // B&W base + color squares at blob positions
    VideoNumbers,   // video expressed as a grid of brightness digits (0-9)
    VideoLines,     // video expressed as contour/edge line drawing
    ThermalVision,  // Ironbow / FLIR thermal colormap on the B&W luma
    SlitScan,       // temporal slit-scan: time scrolls horizontally, y = spatial
    Flash,
    COUNT
};

struct DatamaticsParams {
    // B&W base image (passed through to shader)
    float bwThreshold  = 0.45f;
    float posterize    = 6.f;
    float brightness   = 0.0f;
    float contrast     = 1.2f;
    float gamma        = 0.85f;

    // ScoreDirector timing
    float minModeDuration = 4.f;
    float maxModeDuration = 9.f;
    float flashDuration   = 0.25f;

    // Layer opacity
    float markOpacity  = 0.9f;

    // Mark color — cycles white / red / electric-blue on each mode change
    ofColor markColor = ofColor(255);

    // Film look parameters
    float grain    = 0.035f;   // subtle analog grain
    float vignette = 0.32f;    // edge darkening
    float sCurve   = 0.55f;    // film tonal S-curve strength

    // Waveform
    int   waveformHistory = 1920;

    // Grid
    int   gridCols = 32;
    int   gridRows = 48;

    // Scanline
    int   scanStep = 4;

    // VideoSquares tuning
    float videoSquareSize  = 220.f;  // side of each color video square in px
    int   videoSquareCount = 6;      // max squares drawn simultaneously

    // Slit-scan
    int   slitStripW = 3;     // pixels stamped per frame (controls scroll speed)

    // Manual override (-1 = auto)
    int   forcedMode = -1;
};

class GraphicScore {
public:
    void setup(int w, int h);
    // bwTex  = B&W-processed video (from display FBO + shader)
    // rawTex = original color video frame (from player)
    void update(const ofTexture& bwTex, const ofTexture& rawTex, CVPipeline& cv);
    void draw(int x, int y, int w, int h);

    void onCollision();
    void onClipChange();  // triggers a short white/red strobe overlay

    ScoreMode         currentMode() const { return (preFlashMode_ >= 0) ? ScoreMode::Flash : director_; }
    DatamaticsParams& params()            { return params_; }
    ofFbo&            getFbo()            { return fbo_; }

private:
    void advanceDirector(double dt);
    void buildSequence();

    // Mode renderers (all draw into currently-bound FBO)
    void renderBase(const ofTexture& videoTex);
    void renderScanLine(CVPipeline& cv);
    void renderBBoxTracker(CVPipeline& cv);
    void renderBinaryText(CVPipeline& cv);
    void renderWaveform();
    void renderGridData(CVPipeline& cv);
    void renderBarcode(CVPipeline& cv);
    void renderVideoNormal();
    void renderVideoSquares(CVPipeline& cv);
    void renderVideoNumbers(CVPipeline& cv);
    void renderVideoLines(CVPipeline& cv);
    void renderThermal(CVPipeline& cv);   // Ironbow thermal colormap
    void renderSlitScan();                // temporal slit-scan ping-pong
    void updateSlitFbo(CVPipeline& cv);   // called before score fbo_.begin()
    void renderFlash();

    // Persistent data HUD drawn on top of every mode
    void renderDataHUD(const CVData& data);

    std::string toBinary8(int v);

    // Cover-crop helpers for the raw video texture
    struct CoverLayout { float ox, oy, dw, dh, scale; };
    CoverLayout coverLayout() const;

    // Pick next mark color (white → red → blue cycle)
    static ofColor nextMarkColor(const ofColor& cur);

    ofFbo    fbo_;
    ofShader bwShader_;
    ofShader thermalShader_;   // Ironbow thermal colormap
    ofTrueTypeFont monoFont_;  // small dense text (binary rows)
    ofTrueTypeFont labelFont_; // large labels, blob IDs, numbers
    ofImage        edgesImg_;  // reused every frame for VideoLines

    // Slit-scan CPU ring buffer (one column per frame, wraps at kSlitW)
    static constexpr int kSlitW = 270;   // number of time slices = analysis width
    static constexpr int kSlitH = 480;   // spatial height = analysis height
    cv::Mat slitMat_;      // kSlitH × kSlitW, CV_8UC1 — ring of columns
    int     slitWriteX_ = 0;
    bool    slitReady_   = false;
    ofImage slitImage_;   // assembled output uploaded as texture each frame

    DatamaticsParams params_;
    int  w_ = 0, h_ = 0;

    // Cached per-frame texture pointers (set in update, used by renderers)
    const ofTexture* curBwTex_  = nullptr;
    const ofTexture* curRawTex_ = nullptr;

    // Clip-change strobe overlay (fades fast over current mode)
    float   strobeAlpha_ = 0.f;
    ofColor strobeColor_ = ofColor(255);

    // ScoreDirector state
    ScoreMode director_     = ScoreMode::ScanLine;
    int       seqIdx_       = 0;
    double    modeTimer_    = 0.0;
    double    modeDuration_ = 5.0;
    int       preFlashMode_ = -1;  // -1 = not in flash
    double    flashTimer_   = 0.0;
    std::vector<ScoreMode> sequence_;

    // Waveform history
    std::deque<float> energyHistory_;

    double lastTime_ = 0.0;
};
