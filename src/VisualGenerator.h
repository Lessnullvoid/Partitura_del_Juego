#pragma once

#include "ofMain.h"
#include "OSCSender.h"

#include <cstdint>

enum class GeneratorMode {
    RasterPulse = 0,
    BitMatrix,
    ModularGrid,
    PhaseLines,
    VectorField,
    DataLedger,
    SignalTrace,
    ThresholdBridge,
    Pulse,
    BarScan,
    GranularRaster,
    OrbitalRings,
    Strobe,
    DividedStrobe,
    AnalogNoise
};

enum class OrganizationMode {
    Unison = 0,
    Propagation,
    Counterpoint,
    Group4Plus4
};

enum class TemporalStage {
    Appearance = 0,
    Development,
    Threshold,
    Transformation,
    Dissolution
};

enum class ScreenRole {
    Body = 0,
    Trajectory,
    Velocity,
    Relationships,
    Density,
    Field,
    Prediction,
    Metadata
};

// Valores ya resueltos que usa un renderer tras aplicar los mapeos de
// organización, etapa y rol.
struct VisualState {
    double localTime = 0.0;
    float stageProgress = 0.f;
    float envelope = 1.f;
    float intensity = 0.5f;
    float density = 0.5f;
    float motion = 0.f;
    float flowMagnitude = 0.f;
    float flowAngle = 0.f;
    float rolePhase = 0.f;
    float propagationDelay = 0.f;
    bool observedGroup = true;
    uint32_t resolvedSeed = 1u;
};

// Objeto-valor pensado para montarse en Channel o GraphicScore.
// sourceTexture y cvData se toman prestados durante render().
struct GeneratorContext {
    int width = 0;
    int height = 0;
    int globalChannelIndex = 0;
    int globalChannelCount = 1;
    int windowGroup = 0;
    int segmentIndex = 0;

    double elapsedTime = 0.0;
    double chapterTime = 0.0;
    double chapterDuration = 1.0;

    float beatPhase = 0.f;
    int beatIndex = 0;
    float subdivisionPulse = 0.f;

    TemporalStage stage = TemporalStage::Appearance;
    float stageProgress = 0.f;
    OrganizationMode organization = OrganizationMode::Unison;
    ScreenRole role = ScreenRole::Body;

    const CVData* cvData = nullptr;
    const ofTexture* sourceTexture = nullptr;
    const VisualState* previousState = nullptr;

    uint32_t seed = 1u;
    float intensity = 0.5f;
    float density = 0.5f;
    float glowGain = 0.9f;
    float glowRadius = 2.2f;
    float feedbackDecay = 0.82f;
    ofColor cyan = ofColor(238, 238, 238);
    ofColor red = ofColor(112, 112, 112);
};

class VisualGenerator {
public:
    VisualGenerator() = default;

    void setup(int width, int height);
    void update(const GeneratorContext& context);
    void render(const GeneratorContext& context);

    void setMode(GeneratorMode mode);
    GeneratorMode getMode() const { return mode_; }

    const VisualState& getState() const { return state_; }
    VisualState resolveState(const GeneratorContext& context) const;

    ofFbo& getFbo() { return fbo_; }
    const ofFbo& getFbo() const { return fbo_; }
    void draw(float x, float y) const;
    void draw(float x, float y, float width, float height) const;

    static uint32_t hash(uint32_t value);
    static float hash01(uint32_t value);
    static float noise1D(float x, uint32_t seed);
    static float noise2D(float x, float y, uint32_t seed);

private:
    void ensureAllocated(int width, int height);
    float glowStrength() const;
    bool usesFeedback() const;
    float feedbackScale() const;
    void compositeGlow(const GeneratorContext& context);
    void drawAnalogBar(const GeneratorContext& context, float inset, float span,
                       float top, float thickness, float peak, uint32_t key,
                       float timeSalt);
    void drawAnalogHaze(const GeneratorContext& context, float timeSalt);
    void applyAnalogTreatment(const GeneratorContext& context);
    void renderScene(const GeneratorContext& context);
    void renderRasterPulse(const GeneratorContext& context);
    void renderBitMatrix(const GeneratorContext& context);
    void renderModularGrid(const GeneratorContext& context);
    void renderPhaseLines(const GeneratorContext& context);
    void renderVectorField(const GeneratorContext& context);
    void renderDataLedger(const GeneratorContext& context);
    void renderSignalTrace(const GeneratorContext& context);
    void renderThresholdBridge(const GeneratorContext& context);
    void renderPulse(const GeneratorContext& context);
    void renderBarScan(const GeneratorContext& context);
    void renderGranularRaster(const GeneratorContext& context);
    void renderOrbitalRings(const GeneratorContext& context);
    void renderStrobe(const GeneratorContext& context);
    void renderDividedStrobe(const GeneratorContext& context);
    void renderAnalogNoise(const GeneratorContext& context);

    void drawSource(const GeneratorContext& context, float alpha);
    void drawFrame(const GeneratorContext& context);
    ofColor foreground(float alpha = 255.f) const;
    ofColor accent(float alpha = 255.f) const;

    static float saturate(float value);
    static float smooth(float value);
    static float roleValue(ScreenRole role);

    ofFbo fbo_;
    ofFbo emissionFbo_;
    ofFbo blurA_;
    ofFbo blurB_;
    ofFbo feedbackA_;
    ofFbo feedbackB_;
    ofShader blurShader_;
    bool blurReady_ = false;
    bool feedbackFlip_ = false;
    GeneratorMode mode_ = GeneratorMode::RasterPulse;
    VisualState state_;
    int width_ = 0;
    int height_ = 0;

    ofTrueTypeFont matrixFont_;
    int loadedFontSize_ = 0;
    bool fontReady_ = false;

    ofShader noiseShader_;
    bool noiseShaderReady_ = false;
};
