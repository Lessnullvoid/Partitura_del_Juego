#pragma once

#include "IVisualGenerator.h"
#include "ofMain.h"

class VideoPointCloudGenerator final : public IVisualGenerator {
public:
    std::string id() const override { return "VideoPointCloud"; }
    void setup(const VideoPointCloudSettings& settings) override;
    void update(const VideoGeneratorContext& context, float dt) override;
    void draw(const VideoGeneratorContext& context, const ofRectangle& viewport) override;
    void reset() override;
    VideoPointCloudSettings& settings() override { return settings_; }

private:
    void buildGridIfNeeded();
    void bindUniforms(const VideoGeneratorContext& context);
    void drawPoints(const VideoGeneratorContext& context,
                    const ofRectangle& viewport, bool clearTarget,
                    bool boundedBlend = false);
    void drawFeedbackComposite(const VideoGeneratorContext& context,
                               const ofRectangle& viewport);
    void ensureFeedbackBuffers(int width, int height);

    VideoPointCloudSettings settings_;
    ofVboMesh grid_;
    ofShader shader_;
    ofCamera camera_;
    ofFbo feedbackA_;
    ofFbo feedbackB_;
    bool shaderReady_ = false;
    int builtGridW_ = 0;
    int builtGridH_ = 0;
    bool feedbackFlip_ = false;
    float defaultYawOffset_ = 0.f;
};
