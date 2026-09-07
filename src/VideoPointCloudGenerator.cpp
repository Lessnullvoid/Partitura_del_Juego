#include "VideoPointCloudGenerator.h"
#include <cmath>

VideoPointCloudSettings VideoPointCloudSettings::fromJson(const ofJson& cfg) {
    VideoPointCloudSettings s;
    s.applyJson(cfg);
    return s;
}

void VideoPointCloudSettings::applyJson(const ofJson& cfg) {
    if (!cfg.is_object())
        return;
    if (cfg.contains("qualityTier"))
        applyQualityTier(cfg.value("qualityTier", std::string("Installation")));
    if (cfg.contains("preset"))
        applyPreset(cfg.value("preset", preset));
    enabled = cfg.value("enabled", enabled);
    gridWidth = cfg.value("gridWidth", gridWidth);
    gridHeight = cfg.value("gridHeight", gridHeight);
    const std::string depthName = cfg.value("depthSource", std::string("Luminance"));
    if (depthName == "PdjvDepth")
        depthSource = PointCloudDepthSource::PdjvDepth;
    else if (depthName == "Hybrid")
        depthSource = PointCloudDepthSource::Hybrid;
    else if (cfg.contains("depthSource"))
        depthSource = PointCloudDepthSource::Luminance;
    const std::string maskName = cfg.value("maskMode", std::string("FullFrame"));
    if (maskName == "PlayersOnly")
        maskMode = PointCloudMaskMode::PlayersOnly;
    else if (maskName == "PlayersEmphasized")
        maskMode = PointCloudMaskMode::PlayersEmphasized;
    else if (cfg.contains("maskMode"))
        maskMode = PointCloudMaskMode::FullFrame;
    depthScale = cfg.value("depthScale", depthScale);
    depthCenter = cfg.value("depthCenter", depthCenter);
    pointSize = cfg.value("pointSize", pointSize);
    luminanceFloor = cfg.value("luminanceFloor", luminanceFloor);
    luminanceCeiling = cfg.value("luminanceCeiling", luminanceCeiling);
    gamma = cfg.value("gamma", gamma);
    colorGain = cfg.value("colorGain", colorGain);
    opacity = cfg.value("opacity", opacity);
    zInvert = cfg.value("zInvert", zInvert);
    xyScale = cfg.value("xyScale", xyScale);
    feedbackEnabled = cfg.value("feedbackEnabled", feedbackEnabled);
    feedbackDecay = cfg.value("feedbackDecay", feedbackDecay);
    feedbackScale = cfg.value("feedbackScale", feedbackScale);
    feedbackOffsetX = cfg.value("feedbackOffsetX", feedbackOffsetX);
    feedbackOffsetY = cfg.value("feedbackOffsetY", feedbackOffsetY);
    feedbackRotation = cfg.value("feedbackRotation", feedbackRotation);
    feedbackGain = cfg.value("feedbackGain", feedbackGain);
    autoFit = cfg.value("autoFit", autoFit);
    cameraYaw = cfg.value("cameraYaw", cameraYaw);
    cameraDistance = cfg.value("cameraDistance", cameraDistance);
    cameraFov = cfg.value("cameraFov", cameraFov);
    cameraPitch = cfg.value("cameraPitch", cameraPitch);
    transitionDisplacement =
        cfg.value("transitionDisplacement", transitionDisplacement);
    pdjvPackagePath = cfg.value("pdjvPackagePath", pdjvPackagePath);
}

void VideoPointCloudSettings::applyPreset(int presetIndex) {
    preset = presetIndex;
    if (presetIndex == 0) {
        depthSource = PointCloudDepthSource::Luminance;
        maskMode = PointCloudMaskMode::FullFrame;
        depthScale = 1.15f;
        pointSize = 2.2f;
        colorGain = 1.f;
        feedbackEnabled = false;
        opacity = 0.9f;
    } else if (presetIndex == 1) {
        depthSource = PointCloudDepthSource::PdjvDepth;
        maskMode = PointCloudMaskMode::PlayersOnly;
        depthScale = 1.45f;
        pointSize = 2.f;
        colorGain = 1.05f;
        feedbackEnabled = false;
        opacity = 0.92f;
    } else {
        depthSource = PointCloudDepthSource::Hybrid;
        maskMode = PointCloudMaskMode::PlayersEmphasized;
        depthScale = 1.25f;
        pointSize = 2.1f;
        colorGain = 1.f;
        feedbackEnabled = false;
        opacity = 0.88f;
    }
}

void VideoPointCloudSettings::applyQualityTier(const std::string& tierName) {
    if (tierName == "Preview")
        applyQualityTier(160);
    else if (tierName == "High")
        applyQualityTier(384);
    else if (tierName == "Diagnostic")
        applyQualityTier(128);
    else if (tierName == "MacMini")
        applyQualityTier(192);
    else
        applyQualityTier(256);
}

void VideoPointCloudSettings::applyQualityTier(int gridSize) {
    const int clamped = ofClamp(gridSize, 64, 512);
    gridWidth = clamped;
    gridHeight = clamped;
}

namespace {
static const float kDefaultYaw[8] = {-24.f, -8.f, 8.f, 24.f, -24.f, -8.f, 8.f, 24.f};

ofTexture& whiteTexture() {
    static ofTexture tex;
    static bool ready = false;
    if (!ready) {
        ofPixels px;
        px.allocate(1, 1, OF_PIXELS_GRAY);
        px[0] = 255;
        tex.allocate(px);
        ready = true;
    }
    return tex;
}
} // namespace

void VideoPointCloudGenerator::setup(const VideoPointCloudSettings& settings) {
    settings_ = settings;
    shaderReady_ = shader_.load("shaders/video_points.vert", "shaders/video_points.frag");
    buildGridIfNeeded();
    reset();
}

void VideoPointCloudGenerator::reset() {
    feedbackFlip_ = false;
    if (feedbackA_.isAllocated()) {
        feedbackA_.begin();
        ofClear(0, 0, 0, 0);
        feedbackA_.end();
    }
    if (feedbackB_.isAllocated()) {
        feedbackB_.begin();
        ofClear(0, 0, 0, 0);
        feedbackB_.end();
    }
}

void VideoPointCloudGenerator::buildGridIfNeeded() {
    if (builtGridW_ == settings_.gridWidth && builtGridH_ == settings_.gridHeight &&
        grid_.getNumVertices() > 0)
        return;
    builtGridW_ = settings_.gridWidth;
    builtGridH_ = settings_.gridHeight;
    grid_.clear();
    grid_.setMode(OF_PRIMITIVE_POINTS);
    grid_.getVertices().reserve(builtGridW_ * builtGridH_);
    grid_.getTexCoords().reserve(builtGridW_ * builtGridH_);
    for (int y = 0; y < builtGridH_; ++y) {
        const float v = (y + 0.5f) / builtGridH_;
        for (int x = 0; x < builtGridW_; ++x) {
            const float u = (x + 0.5f) / builtGridW_;
            grid_.addVertex(glm::vec3(u - 0.5f, 0.5f - v, 0.f));
            grid_.addTexCoord(glm::vec2(u, v));
        }
    }
}

void VideoPointCloudGenerator::update(const VideoGeneratorContext& context, float dt) {
    (void)context;
    (void)dt;
    buildGridIfNeeded();
}

void VideoPointCloudGenerator::bindUniforms(const VideoGeneratorContext& context) {
    shader_.setUniformTexture("videoTex", *context.videoTexture, 0);
    const ofTexture* mask = context.maskTexture ? context.maskTexture : &whiteTexture();
    const ofTexture* depth = context.depthTexture ? context.depthTexture : &whiteTexture();
    shader_.setUniformTexture("maskTex", *mask, 1);
    shader_.setUniformTexture("depthTex", *depth, 2);
    shader_.setUniform2f("videoTextureSize", context.videoTexture->getWidth(),
                         context.videoTexture->getHeight());
    shader_.setUniform2f("maskTextureSize", mask->getWidth(), mask->getHeight());
    shader_.setUniform2f("depthTextureSize", depth->getWidth(), depth->getHeight());
    const bool hasAnalysis =
        context.analysisFrame && context.analysisFrame->valid;
    if (hasAnalysis) {
        const float* bbox = context.analysisFrame->playerBBox;
        shader_.setUniform4f("analysisBBox", bbox[0], bbox[1],
                             bbox[2], bbox[3]);
    } else {
        shader_.setUniform4f("analysisBBox", 0.f, 0.f, 1.f, 1.f);
    }
    shader_.setUniform1f("analysisAvailable", hasAnalysis ? 1.f : 0.f);
    shader_.setUniform1f("transitionAmount",
                         ofClamp(context.transitionAmount, 0.f, 1.f));
    shader_.setUniform1f("transitionDisplacement",
                         settings_.transitionDisplacement);
    shader_.setUniform1f("transitionTime",
                         static_cast<float>(context.playbackTime));
    shader_.setUniform1f("transitionSeed",
                         static_cast<float>(context.channelIndex + 1) * 17.31f);
    const float aspect = context.videoHeight > 0
        ? static_cast<float>(context.videoWidth) / context.videoHeight
        : 1.f;
    shader_.setUniform1f("videoAspect", aspect);
    shader_.setUniform1f("depthScale", settings_.depthScale);
    shader_.setUniform1f("depthCenter", settings_.depthCenter);
    shader_.setUniform1f("pointSize", settings_.pointSize);
    shader_.setUniform1f("luminanceFloor", settings_.luminanceFloor);
    shader_.setUniform1f("luminanceCeiling", settings_.luminanceCeiling);
    shader_.setUniform1f("gammaVal", settings_.gamma);
    shader_.setUniform1f("colorGain", settings_.colorGain);
    shader_.setUniform1f("opacity", settings_.opacity);
    shader_.setUniform1f("zInvert", settings_.zInvert ? 1.f : 0.f);
    shader_.setUniform1f("xyScale", settings_.xyScale);
    shader_.setUniform1i("depthSource", static_cast<int>(settings_.depthSource));
    shader_.setUniform1i("maskMode", static_cast<int>(settings_.maskMode));
    shader_.setUniform1f("playerEmphasis", 1.f);
    shader_.setUniform1f("stadiumOpacity", settings_.maskMode == PointCloudMaskMode::PlayersEmphasized ? 0.18f : 1.f);
}

void VideoPointCloudGenerator::drawPoints(const VideoGeneratorContext& context,
                                         const ofRectangle& viewport,
                                         bool clearTarget,
                                         bool boundedBlend) {
    if (!shaderReady_ || !context.videoTexture || !context.videoTexture->isAllocated())
        return;
    const int ch = ofClamp(context.channelIndex, 0, 7);
    defaultYawOffset_ = kDefaultYaw[ch];
    const float yaw = ofDegToRad(settings_.cameraYaw + defaultYawOffset_);
    const float pitch = ofDegToRad(settings_.cameraPitch);
    camera_.setNearClip(0.01f);
    camera_.setFarClip(20.f);
    camera_.setFov(settings_.cameraFov);
    float dist = settings_.cameraDistance;
    if (settings_.autoFit) {
        const float sourceAspect = context.videoHeight > 0
            ? static_cast<float>(context.videoWidth) / context.videoHeight
            : 1.f;
        const float viewportAspect = std::max(
            0.01f, viewport.getWidth() / std::max(1.f, viewport.getHeight()));
        const float halfHeight = settings_.xyScale * 0.5f;
        const float halfWidth = halfHeight * sourceAspect;
        const float tanHalfFov = std::tan(
            ofDegToRad(ofClamp(settings_.cameraFov, 5.f, 150.f) * 0.5f));
        const float verticalDistance = halfHeight / tanHalfFov;
        const float horizontalDistance =
            halfWidth / (tanHalfFov * viewportAspect);
        dist = std::max(verticalDistance, horizontalDistance) +
               settings_.depthScale * 0.55f;
    }
    camera_.setPosition(std::sin(yaw) * dist,
                        std::sin(pitch) * dist * 0.35f,
                        std::cos(yaw) * dist);
    camera_.lookAt(glm::vec3(0.f));
    if (clearTarget) {
        ofPushStyle();
        ofSetColor(0, 0, 0, 255);
        ofDrawRectangle(viewport);
        ofPopStyle();
    }
    ofPushView();
    ofViewport(viewport);
    camera_.begin(viewport);
    ofEnableDepthTest();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    if (boundedBlend) {
        glBlendEquation(GL_MAX);
        glBlendFunc(GL_ONE, GL_ONE);
    }
    ofEnablePointSprites();
    glEnable(GL_PROGRAM_POINT_SIZE);
    shader_.begin();
    bindUniforms(context);
    grid_.draw();
    shader_.end();
    glBlendEquation(GL_FUNC_ADD);
    ofDisablePointSprites();
    ofDisableDepthTest();
    camera_.end();
    ofPopView();
    ofEnableAlphaBlending();
}

void VideoPointCloudGenerator::ensureFeedbackBuffers(int width, int height) {
    if (feedbackA_.getWidth() == width && feedbackA_.getHeight() == height &&
        feedbackB_.getWidth() == width && feedbackB_.getHeight() == height)
        return;
    ofFbo::Settings fbo;
    fbo.width = std::max(1, width);
    fbo.height = std::max(1, height);
    fbo.internalformat = GL_RGBA16F;
    fbo.useDepth = true;
    fbo.numSamples = 0;
    feedbackA_.allocate(fbo);
    feedbackB_.allocate(fbo);
    reset();
}

void VideoPointCloudGenerator::drawFeedbackComposite(
    const VideoGeneratorContext& context, const ofRectangle& viewport) {
    const int width = std::max(1, static_cast<int>(viewport.getWidth()));
    const int height = std::max(1, static_cast<int>(viewport.getHeight()));
    ensureFeedbackBuffers(width, height);
    ofFbo& previous = feedbackFlip_ ? feedbackB_ : feedbackA_;
    ofFbo& current = feedbackFlip_ ? feedbackA_ : feedbackB_;

    current.begin();
    ofClear(0, 0, 0, 0);
    glEnable(GL_BLEND);
    glBlendEquation(GL_MAX);
    glBlendFunc(GL_ONE, GL_ONE);
    ofPushMatrix();
    ofTranslate(width * 0.5f + settings_.feedbackOffsetX,
                height * 0.5f + settings_.feedbackOffsetY);
    ofRotateDeg(settings_.feedbackRotation);
    ofScale(settings_.feedbackScale, settings_.feedbackScale);
    ofTranslate(-width * 0.5f, -height * 0.5f);
    const float historyGain =
        ofClamp(settings_.feedbackDecay * settings_.feedbackGain, 0.f, 1.f);
    ofSetColor(255.f * historyGain);
    previous.draw(0, 0, width, height);
    ofPopMatrix();
    ofSetColor(255);
    drawPoints(context, ofRectangle(0, 0, width, height), false, true);
    glBlendEquation(GL_FUNC_ADD);
    current.end();

    ofPushStyle();
    ofSetColor(0, 0, 0, 255);
    ofDrawRectangle(viewport);
    ofPopStyle();
    ofSetColor(255);
    current.draw(viewport.x, viewport.y, viewport.width, viewport.height);
    feedbackFlip_ = !feedbackFlip_;
}

void VideoPointCloudGenerator::draw(const VideoGeneratorContext& context,
                                    const ofRectangle& viewport) {
    if (settings_.feedbackEnabled)
        drawFeedbackComposite(context, viewport);
    else
        drawPoints(context, viewport, true);
}
