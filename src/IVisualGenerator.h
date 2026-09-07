#pragma once

#include "ofMain.h"
#include <string>

enum class PointCloudDepthSource { Luminance, PdjvDepth, Hybrid };
enum class PointCloudMaskMode { FullFrame, PlayersOnly, PlayersEmphasized };

struct VideoPointCloudSettings {
    bool enabled = true;
    int gridWidth = 256;
    int gridHeight = 256;
    PointCloudDepthSource depthSource = PointCloudDepthSource::Luminance;
    PointCloudMaskMode maskMode = PointCloudMaskMode::FullFrame;
    float depthScale = 1.15f;
    float depthCenter = 0.5f;
    float pointSize = 2.2f;
    float luminanceFloor = 0.035f;
    float luminanceCeiling = 1.f;
    float gamma = 1.f;
    float colorGain = 1.f;
    float opacity = 0.9f;
    bool zInvert = false;
    float xyScale = 2.f;
    bool feedbackEnabled = false;
    float feedbackDecay = 0.88f;
    float feedbackScale = 1.f;
    float feedbackOffsetX = 0.f;
    float feedbackOffsetY = 0.f;
    float feedbackRotation = 0.f;
    float feedbackGain = 1.f;
    bool autoFit = true;
    float cameraYaw = 0.f;
    float cameraPitch = 0.f;
    float cameraDistance = 1.8f;
    float cameraFov = 42.f;
    float transitionDisplacement = 0.85f;
    int preset = 0;
    std::string pdjvPackagePath;

    static VideoPointCloudSettings fromJson(const ofJson& cfg);
    void applyJson(const ofJson& cfg);
    void applyPreset(int presetIndex);
    void applyQualityTier(const std::string& tierName);
    void applyQualityTier(int gridSize);
};

struct PdjvFrameView {
    bool valid = false;
    const uint8_t* maskData = nullptr;
    uint32_t maskWidth = 0;
    uint32_t maskHeight = 0;
    const uint8_t* depthData = nullptr;
    uint32_t depthWidth = 0;
    uint32_t depthHeight = 0;
    float playerBBox[4] = {0, 0, 1, 1};
};

struct VideoGeneratorContext {
    const ofTexture* videoTexture = nullptr;
    int videoWidth = 0;
    int videoHeight = 0;
    double playbackTime = 0.0;
    bool frameNew = false;
    int channelIndex = 0;
    const PdjvFrameView* analysisFrame = nullptr;
    const ofTexture* maskTexture = nullptr;
    const ofTexture* depthTexture = nullptr;
    float transitionAmount = 0.f;
};

class IVisualGenerator {
public:
    virtual ~IVisualGenerator() = default;
    virtual std::string id() const = 0;
    virtual void setup(const VideoPointCloudSettings& settings) = 0;
    virtual void update(const VideoGeneratorContext& context, float dt) = 0;
    virtual void draw(const VideoGeneratorContext& context, const ofRectangle& viewport) = 0;
    virtual void reset() = 0;
    virtual VideoPointCloudSettings& settings() = 0;
};
