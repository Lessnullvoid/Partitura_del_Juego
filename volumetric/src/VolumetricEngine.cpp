#include "VolumetricEngine.h"
#include "generators/IVolumetricGenerator.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_set>

void PresentationResources::allocate(int w, int h, bool hdr) {
    ownerGroup = ownerGroup;
    ofFbo::Settings s;
    s.width = std::max(1, w);
    s.height = std::max(1, h);
    s.numSamples = 0;
    s.useDepth = hdr;
    s.depthStencilAsTexture = false;
    s.textureTarget = GL_TEXTURE_2D;
    s.internalformat = hdr ? GL_RGBA16F : GL_RGBA8;
    geometry.allocate(s);
    if (hdr) {
        s.useDepth = false;
        emissive.allocate(s);
        bloomA.allocate(s);
        bloomB.allocate(s);
        tonemap.allocate(s);
        feedback.allocate(s);
        feedback.begin();
        ofClear(0, 0, 0, 0);
        feedback.end();
    }
}

SurfaceTopology VolumetricEngine::topologyForPreset() const {
    if (generator.id == "WireframeShell") {
        if (generator.preset == 0)
            return SurfaceTopology::PoseShell;
        if (generator.preset == 1)
            return SurfaceTopology::MaskGrid;
        return SurfaceTopology::Both;
    }
    return surfaceTopology_;
}

void VolumetricEngine::refreshRenderState() {
    ScenePreset sp;
    GeneratorRegistry::apply(generator.id, generator.preset, renderState_, sp);
    generator.echoSpacing = sp.echoSpacing;
    generator.historySeconds = sp.historySeconds;
    useGpuPositions_ = gpuSimOk_ && (generator.id == "KineticFragmentation" || generator.id == "TemporalEcho");
}

void VolumetricEngine::setup(const ofJson& cfg) {
    ofDisableArbTex();
    const std::string host = cfg.value("/osc/host"_json_pointer, std::string("localhost"));
    const int port = cfg.value("/osc/port"_json_pointer, 9001);
    oscReady = osc.setup(host, port);
    const int listen = cfg.value("/osc/listenPort"_json_pointer, 9002);
    oscInReady = oscIn.setup(listen);
    const std::string tierName = cfg.value("qualityTier", std::string("Preview"));
    tier = (tierName == "Installation") ? QualityTier::Installation
         : (tierName == "Diagnostic") ? QualityTier::Diagnostic
         : QualityTier::Preview;
    particleRes = (tier == QualityTier::Installation) ? 64 : 48;
    pointsPerPlayer = (tier == QualityTier::Installation) ? 6000
                    : (tier == QualityTier::Diagnostic) ? 1400 : 2400;
    maskGridW_ = (tier == QualityTier::Installation) ? 48 : (tier == QualityTier::Diagnostic) ? 0 : 32;
    maskGridH_ = (tier == QualityTier::Installation) ? 96 : (tier == QualityTier::Diagnostic) ? 0 : 64;
    bloomGain = (tier == QualityTier::Diagnostic) ? 0.f : 1.f;
    const std::string modeName = cfg.value("contentMode", std::string("SharedView"));
    if (modeName == "Independent")
        director.setMode(pdjv::ContentMode::Independent);
    else if (modeName == "FourPlusFour")
        director.setMode(pdjv::ContentMode::FourPlusFour);
    else if (modeName == "Mixed")
        director.setMode(pdjv::ContentMode::Mixed);
    else if (modeName == "Synchronized")
        director.setMode(pdjv::ContentMode::Synchronized);
    else
        director.setMode(pdjv::ContentMode::SharedView);
    primaryPath_ = ofToDataPath(
        cfg.value("/pdjv/primary"_json_pointer, std::string("golden_v0.pdjv")), true);
    if (!ofFile::doesFileExist(primaryPath_ + "/manifest.json")) {
        const std::string fallback =
            ofFilePath::join(ofFilePath::getCurrentWorkingDirectory(), "data/golden_v0.pdjv");
        if (ofFile::doesFileExist(fallback + "/manifest.json"))
            primaryPath_ = fallback;
    }
    shared_ = pool_.acquire(primaryPath_);
    packageError = shared_.reader ? shared_.reader->error() : "reader missing";
    if (shared_.timeline) {
        shared_.timeline->setLoop(true);
        shared_.timeline->play();
    }
    generator.id = cfg.value("defaultGenerator", std::string("CyanWireframeMesh"));
    const std::string camName = cfg.value("cameraMode", std::string("portraitBust"));
    cameraMode_ = (camName == "fullBody") ? CameraMode::FullBody : CameraMode::PortraitBust;
    composerEnabled_ = cfg.value("composerEnabled", false);
    if (composerEnabled_) {
        composer_.setStages({
            {"CyanWireframeMesh", 0, 45.f},
            {"GlitchPointCloud", 0, 35.f},
            {"WireframeShell", 1, 30.f},
            {"TemporalEcho", 0, 30.f},
            {"KineticFragmentation", 1, 25.f},
            {"SkeletonFilaments", 0, 30.f},
            {"DepthSlices", 1, 30.f},
        });
        composer_.reset(seed);
    }
    shadersOk_ = pointShader_.load("shaders/point.vert", "shaders/point.frag");
    wireShaderOk_ = wireShader_.load("shaders/wire.vert", "shaders/wire.frag");
    meshWireframeShaderOk_ = meshWireframeShader_.load("shaders/meshWireframe.vert", "shaders/meshWireframe.frag");
    groundGridShaderOk_ = groundGridShader_.load("shaders/groundGrid.vert", "shaders/groundGrid.frag");
    splatShaderOk_ = splatShader_.load("shaders/splat.vert", "shaders/splat.frag");
    bloomShader_.load("shaders/fullscreen.vert", "shaders/bloom.frag");
    tonemapShader_.load("shaders/fullscreen.vert", "shaders/tonemap.frag");
    feedbackShader_.load("shaders/fullscreen.vert", "shaders/feedback.frag");
    fullscreenShader_.load("shaders/fullscreen.vert", "shaders/bloom.frag");
    gpuSimOk_ = simulateShader_.load("shaders/fullscreen.vert", "shaders/simulate.frag");
    simPos_.allocate(particleRes, particleRes);
    simVel_.allocate(particleRes, particleRes);
    targetPix_.allocate(particleRes, particleRes, OF_PIXELS_RGBA);
    targetPix_.set(0);
    targetTex_.allocate(targetPix_);
    {
        ofFbo::Settings hs;
        hs.width = particleRes;
        hs.height = particleRes;
        hs.internalformat = GL_RGBA16F;
        hs.useDepth = false;
        for (int i = 0; i < 5; ++i) {
            history_[i].allocate(hs);
            history_[i].begin();
            ofClear(0, 0, 0, 0);
            history_[i].end();
        }
    }
    cam_.setNearClip(0.15f);
    cam_.setFarClip(40.f);
    cam_.setFov(38.f);

    // Preasigna el búfer GPU de la nube una sola vez con GL_DYNAMIC_DRAW para que
    // las subidas posteriores por fotograma usen glBufferSubData (sin reasignación).
    // Caso peor: 4 jugadores × (pointsPerPlayer + maskGrid) × 6 (eco temporal).
    const int gridPts = std::max(1, maskGridW_) * std::max(1, maskGridH_);
    cloudMaxPts_ = 4 * (pointsPerPlayer + gridPts) * 6 + 256;
    cpuCloudPos_.reserve(cloudMaxPts_);
    cpuCloudColors_.reserve(cloudMaxPts_);
    {
        std::vector<glm::vec3>    dummy(cloudMaxPts_, glm::vec3(0.f));
        std::vector<ofFloatColor> dummyC(cloudMaxPts_, ofFloatColor(0.f));
        cloudVbo_.setVertexData(dummy.data(), cloudMaxPts_, GL_DYNAMIC_DRAW);
        cloudVbo_.setColorData(dummyC.data(), cloudMaxPts_, GL_DYNAMIC_DRAW);
    }
    cloudActiveCount_ = 0;

    wires_.setMode(OF_PRIMITIVE_LINES);
    triangles_.setMode(OF_PRIMITIVE_TRIANGLES);
    segments_.setMode(OF_PRIMITIVE_LINES);
    groundGridMesh_.setMode(OF_PRIMITIVE_LINES);
    buildGroundGrid();
    refreshRenderState();
    status_ = packageError.empty() ? "ready " + primaryPath_ : packageError;
    ofLogNotice("volumetric") << status_;
}

void VolumetricEngine::reset() {
    players_.clear();
    simSeeded_ = false;
    if (shared_.timeline)
        shared_.timeline->seek(0);
    seed = 1;
    composer_.reset(seed);
}

glm::vec3 VolumetricEngine::worldFromImage(float x, float y, float z) const {
    return {(x - 0.5f) * 3.8f, (0.88f - y) * 3.6f, (z - 0.42f) * 2.2f};
}

void VolumetricEngine::ensurePlayers(const pdjv::FrameRecord& frame, double t) {
    std::unordered_set<int> visible;
    const auto topo = topologyForPreset();
    const std::string packageId = shared_.reader ? shared_.reader->manifest().packageId : "local";
    for (const auto& obs : frame.players) {
        if (obs.observed)
            visible.insert(obs.trackingId);
        auto& mesh = players_[obs.trackingId];
        if (mesh.trackingId < 0) {
            const int gw = maskGridW_ > 0 ? maskGridW_ : 32;
            const int gh = maskGridH_ > 0 ? maskGridH_ : 64;
            mesh.initialize(obs.trackingId, pointsPerPlayer, gw, gh, topo, packageId);
        }
        if (obs.observed) {
            mesh.lastSeen = t;
            mesh.retiring = false;
            mesh.envelope = 1.f;
            mesh.updateTargets(obs, shared_.reader.get(),
                               [this](float x, float y, float z) { return worldFromImage(x, y, z); },
                               1.f / 30.f, generator.id, generator.fragment);
        } else {
            mesh.envelope = std::max(0.f, mesh.envelope - 0.02f);
            if (t - mesh.lastSeen > 1.5)
                mesh.retiring = true;
        }
    }
    for (auto it = players_.begin(); it != players_.end();) {
        auto& mesh = it->second;
        if (!visible.count(mesh.trackingId)) {
            mesh.envelope = std::max(0.f, mesh.envelope - 0.14f);
            mesh.retiring = t - mesh.lastSeen > 0.22;
        }
        if (mesh.retiring && mesh.envelope <= 0.001f)
            it = players_.erase(it);
        else
            ++it;
    }
}

void VolumetricEngine::applyGenerator(PlayerSurfaceMesh& mesh, float dt) {
    mesh.applyGeneratorForces(generator.id, generator.preset, dt, generator.scan);
}

void VolumetricEngine::pollOsc() {
    if (!oscInReady)
        return;
    while (oscIn.hasWaitingMessages()) {
        ofxOscMessage m;
        oscIn.getNextMessage(m);
        const std::string addr = m.getAddress();
        if (addr == "/pdjv/generator" && m.getNumArgs() > 0) {
            generator.id = m.getArgAsString(0);
            refreshRenderState();
        }
        if (addr == "/pdjv/preset" && m.getNumArgs() > 0) {
            generator.preset = ofClamp(m.getArgAsInt(0), 0, 2);
            refreshRenderState();
        }
        if (addr == "/pdjv/play" && m.getNumArgs() > 0)
            playing = m.getArgAsInt(0) != 0;
        if (addr == "/pdjv/camera/mode" && m.getNumArgs() > 0)
            cameraMode_ = m.getArgAsString(0) == "fullBody" ? CameraMode::FullBody : CameraMode::PortraitBust;
    }
}

void VolumetricEngine::packGpuTargets() {
    if (!gpuSimOk_)
        return;
    targetPix_.set(0);
    int slot = 0;
    for (auto& kv : players_) {
        if (slot >= 4)
            break;
        auto& mesh = kv.second;
        for (size_t i = 0; i < mesh.vertexCount(); ++i) {
            const int idx = slot * pointsPerPlayer + static_cast<int>(i);
            const int x = idx % particleRes;
            const int y = idx / particleRes;
            if (y >= particleRes)
                break;
            const glm::vec3 g = mesh.positionAt(i);
            targetPix_.setColor(x, y, ofFloatColor(g.x, g.y, g.z, mesh.envelope));
        }
        ++slot;
    }
    targetTex_.loadData(targetPix_);
}

void VolumetricEngine::runGpuSim(float dt) {
    if (!gpuSimOk_ || !simulateShader_.isLoaded())
        return;
    if (!simSeeded_) {
        simPos_.source().begin();
        ofClear(0, 0, 0, 0);
        targetTex_.draw(0, 0, particleRes, particleRes);
        simPos_.source().end();
        simSeeded_ = true;
    }
    auto pass = [&](int writeVel) {
        ofFbo& dest = writeVel ? simVel_.destination() : simPos_.destination();
        dest.begin();
        simulateShader_.begin();
        simulateShader_.setUniformTexture("srcPos", simPos_.source().getTexture(), 0);
        simulateShader_.setUniformTexture("srcVel", simVel_.source().getTexture(), 1);
        simulateShader_.setUniformTexture("srcTarget", targetTex_, 2);
        simulateShader_.setUniform1f("drag", 3.2f);
        simulateShader_.setUniform1f("attract", 1.8f + (1.f - generator.fragment) * 2.2f);
        simulateShader_.setUniform1f(
            "impulse", generator.id == "KineticFragmentation" ? generator.fragment * 4.f : 0.f);
        simulateShader_.setUniform1f("dt", std::max(0.001f, dt));
        simulateShader_.setUniform2f("res", static_cast<float>(particleRes), static_cast<float>(particleRes));
        simulateShader_.setUniform1i("writeVel", writeVel);
        simPos_.source().draw(0, 0, particleRes, particleRes);
        simulateShader_.end();
        dest.end();
    };
    pass(0);
    simPos_.swap();
    pass(1);
    simVel_.swap();
    for (int h = 4; h > 0; --h) {
        history_[h].begin();
        history_[h - 1].draw(0, 0, particleRes, particleRes);
        history_[h].end();
    }
    history_[0].begin();
    simPos_.source().draw(0, 0, particleRes, particleRes);
    history_[0].end();
}

void VolumetricEngine::update(float dt) {
    lastDt = dt;
    ++frameNumber;
    pollOsc();
    if (!shared_.timeline || !shared_.reader || !shared_.reader->error().empty())
        return;
    if (playing)
        shared_.timeline->play();
    else
        shared_.timeline->pause();
    shared_.timeline->update(dt);
    if (composerEnabled_) {
        composer_.update(shared_.timeline->time(), dt);
        composer_.apply(generator, renderState_);
    }
    pdjv::FrameRecord interp, nearest;
    if (!shared_.timeline->current(interp, nearest))
        return;
    ensurePlayers(interp, shared_.timeline->time());
    for (auto& kv : players_)
        applyGenerator(kv.second, dt);
    const int eventId = shared_.timeline->consumeEventOnce(nearest);
    if (eventId >= 0)
        generator.fragment = std::min(1.f, generator.fragment + 0.4f);
    generator.fragment = std::max(0.f, generator.fragment - dt * 0.25f);
    packGpuTargets();
    runGpuSim(dt);
    sendOsc();
}

void VolumetricEngine::buildGroundGrid() {
    groundGridMesh_.clear();
    const float y = -1.2f;
    const float extent = 5.0f;
    const float step = 0.25f;
    for (float x = -extent; x <= extent; x += step) {
        groundGridMesh_.addVertex(glm::vec3(x, y, -extent));
        groundGridMesh_.addColor(ofFloatColor(0.0f, 0.9f, 1.0f, 0.5f));
        groundGridMesh_.addVertex(glm::vec3(x, y, extent));
        groundGridMesh_.addColor(ofFloatColor(0.0f, 0.9f, 1.0f, 0.5f));
    }
    for (float z = -extent; z <= extent; z += step) {
        groundGridMesh_.addVertex(glm::vec3(-extent, y, z));
        groundGridMesh_.addColor(ofFloatColor(0.0f, 0.9f, 1.0f, 0.5f));
        groundGridMesh_.addVertex(glm::vec3(extent, y, z));
        groundGridMesh_.addColor(ofFloatColor(0.0f, 0.9f, 1.0f, 0.5f));
    }
}

void VolumetricEngine::buildCloud(const pdjv::FrameRecord& frame) {
    // Vacía los vectores CPU (conserva la capacidad reservada — sin reasignación de heap).
    cpuCloudPos_.clear();
    cpuCloudColors_.clear();
    wires_.clear();
    triangles_.clear();
    segments_.clear();

    glm::vec3 lo(std::numeric_limits<float>::max());
    glm::vec3 hi(-std::numeric_limits<float>::max());
    bool hasBounds = false;
    for (auto& kv : players_) {
        auto& mesh = kv.second;
        const pdjv::PlayerObservation* poseObs = nullptr;
        for (const auto& obs : frame.players) {
            if (obs.trackingId == mesh.trackingId) {
                poseObs = &obs;
                break;
            }
        }
        mesh.buildDrawLists(cpuCloudPos_, cpuCloudColors_,
                            wires_, triangles_, segments_,
                            renderState_, generator, poseObs,
                            [this](float x, float y, float z) { return worldFromImage(x, y, z); });
        if (mesh.envelope > 0.001f && mesh.vertexCount() > 0) {
            glm::vec3 mlo, mhi;
            mesh.bounds(mlo, mhi);
            lo = glm::min(lo, mlo);
            hi = glm::max(hi, mhi);
            hasBounds = true;
        }
    }

    // Sube los puntos de la nube con glBufferSubData — sin reasignación del búfer GPU.
    cloudActiveCount_ = static_cast<int>(cpuCloudPos_.size());
    if (cloudActiveCount_ > 0 && cloudActiveCount_ <= cloudMaxPts_) {
        cloudVbo_.updateVertexData(cpuCloudPos_.data(), cloudActiveCount_);
        cloudVbo_.updateColorData(cpuCloudColors_.data(), cloudActiveCount_);
    }

    if (hasBounds) {
        const glm::vec3 targetCenter = (lo + hi) * 0.5f;
        const float targetRadius = std::max(0.45f, glm::length(hi - lo) * 0.58f);
        cloudCenter_ = glm::mix(cloudCenter_, targetCenter, 0.14f);
        cloudRadius_ = ofLerp(cloudRadius_, targetRadius, 0.14f);
    }
}

void VolumetricEngine::drawChannelScene(int channel, int w, int h) {
    (void)channel;
    (void)w;
    (void)h;
    const float yaw = cameraAngle + director.cameraYawOffset(channel);
    cam_.setNearClip(0.12f);
    cam_.setFarClip(40.f);
    cam_.setFov(cameraMode_ == CameraMode::PortraitBust ? 28.f : 32.f);
    float distance = std::max(4.8f, cloudRadius_ * 3.35f);
    glm::vec3 lookAt = cloudCenter_;
    if (cameraMode_ == CameraMode::PortraitBust) {
        distance = std::max(3.6f, cloudRadius_ * 2.6f);
        lookAt.y += cloudRadius_ * 0.18f;
    }
    cam_.setPosition(cloudCenter_ + glm::vec3(std::sin(ofDegToRad(yaw)) * distance,
                                              cloudRadius_ * 0.08f,
                                              std::cos(ofDegToRad(yaw)) * distance));
    cam_.lookAt(lookAt);
    ofEnableDepthTest();
    cam_.begin();

    // 1. Renderiza la rejilla de reflexión de suelo 3D (estilo TouchDesigner / Image 2)
    if (renderState_.groundGrid && groundGridShaderOk_) {
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        groundGridShader_.begin();
        groundGridShader_.setUniform1f("time", ofGetElapsedTimef());
        groundGridShader_.setUniform1i("colorPalette", renderState_.colorPalette);
        groundGridMesh_.draw();
        groundGridShader_.end();
    }

    // 2. Renderiza las facetas de malla de superficie sombreada 3D (referencia Image 1 Cyan Mesh)
    if ((renderState_.triangleMesh || generator.id == "CyanWireframeMesh") && triangles_.getNumVertices() > 0) {
        ofEnableBlendMode(OF_BLENDMODE_ALPHA);
        if (meshWireframeShaderOk_) {
            meshWireframeShader_.begin();
            meshWireframeShader_.setUniform1i("colorPalette", renderState_.colorPalette);
            meshWireframeShader_.setUniform1f("normalLighting", renderState_.normalLighting);
            meshWireframeShader_.setUniform1f("rimGlow", renderState_.rimGlow);
            meshWireframeShader_.setUniform1f("opacity", 0.65f);
            triangles_.draw();
            meshWireframeShader_.end();
        } else {
            triangles_.draw();
        }
    }

    // 3. Renderiza aristas de wireframe eléctrico
    if (renderState_.wireframe && wires_.getNumVertices() > 0) {
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        if (meshWireframeShaderOk_) {
            meshWireframeShader_.begin();
            meshWireframeShader_.setUniform1i("colorPalette", renderState_.colorPalette);
            meshWireframeShader_.setUniform1f("normalLighting", 0.2f);
            meshWireframeShader_.setUniform1f("rimGlow", renderState_.rimGlow * 1.4f);
            meshWireframeShader_.setUniform1f("opacity", 0.95f);
            wires_.draw();
            meshWireframeShader_.end();
        } else if (wireShaderOk_) {
            wireShader_.begin();
            wires_.draw();
            wireShader_.end();
        } else {
            wires_.draw();
        }
    }

    // 4. Renderiza la nube de point sprites (dibujada desde un VBO persistente preasignado)
    if ((renderState_.points || renderState_.splats) && cloudActiveCount_ > 0) {
        ofEnablePointSprites();
        glEnable(GL_PROGRAM_POINT_SIZE);
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        ofSetColor(255);
        if (renderState_.splats && splatShaderOk_) {
            splatShader_.begin();
            splatShader_.setUniform1f("pointSize", (renderState_.wireframe ? 2.5f : 5.0f) * renderState_.pointScale);
            cloudVbo_.draw(GL_POINTS, 0, cloudActiveCount_);
            splatShader_.end();
        } else if (shadersOk_) {
            pointShader_.begin();
            pointShader_.setUniform1f("pointSize", (renderState_.wireframe ? 2.2f : 4.2f) * renderState_.pointScale);
            cloudVbo_.draw(GL_POINTS, 0, cloudActiveCount_);
            pointShader_.end();
        } else {
            cloudVbo_.draw(GL_POINTS, 0, cloudActiveCount_);
        }
        ofDisablePointSprites();
    }

    // 5. Renderiza filamentos glitch y segmentos de esqueleto
    if (segments_.getNumVertices() > 0) {
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        if (meshWireframeShaderOk_) {
            meshWireframeShader_.begin();
            meshWireframeShader_.setUniform1i("colorPalette", renderState_.colorPalette);
            meshWireframeShader_.setUniform1f("normalLighting", 0.0f);
            meshWireframeShader_.setUniform1f("rimGlow", 1.5f);
            meshWireframeShader_.setUniform1f("opacity", 0.85f);
            segments_.draw();
            meshWireframeShader_.end();
        } else {
            segments_.draw();
        }
    }

    cam_.end();
    ofDisableDepthTest();
    ofEnableAlphaBlending();
}

void VolumetricEngine::renderPresentation(int channelOffset, int w, int h, PresentationResources& res) {
    if (res.geometry.getWidth() != w || res.geometry.getHeight() != h)
        res.allocate(w, h, tier != QualityTier::Diagnostic);
    pdjv::FrameRecord interp, nearest;
    if (shared_.timeline)
        shared_.timeline->current(interp, nearest);
    buildCloud(interp);
    const int seg = std::max(1, w / 4);
    res.geometry.begin();
    ofClear(0, 0, 0, 255);
    for (int i = 0; i < 4; ++i) {
        ofPushView();
        ofViewport(i * seg, 0, seg, h);
        drawChannelScene(channelOffset + i, seg, h);
        ofPopView();
    }
    res.geometry.end();

    if (tier == QualityTier::Diagnostic) {
        ofSetColor(255);
        res.geometry.draw(0, 0, w, h);
        return;
    }

    res.emissive.begin();
    ofClear(0, 0, 0, 0);
    bloomShader_.begin();
    bloomShader_.setUniformTexture("src", res.geometry.getTexture(), 0);
    bloomShader_.setUniform1f("threshold", bloomThreshold);
    ofSetColor(255);
    res.geometry.draw(0, 0, w, h);
    bloomShader_.end();
    res.emissive.end();

    res.bloomA.begin();
    ofClear(0, 0, 0, 0);
    ofSetColor(255);
    res.emissive.draw(0, 0, w / 2, h / 2);
    res.bloomA.end();

    res.tonemap.begin();
    ofClear(0, 0, 0, 255);
    ofSetColor(255);
    res.geometry.draw(0, 0, w, h);
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    ofSetColor(255, 255, 255, static_cast<int>(bloomGain * 160));
    res.emissive.draw(0, 0, w, h);
    res.bloomA.draw(0, 0, w, h);
    res.tonemap.end();

    res.feedback.begin();
    feedbackShader_.begin();
    feedbackShader_.setUniformTexture("prev", res.feedback.getTexture(), 0);
    feedbackShader_.setUniformTexture("curr", res.tonemap.getTexture(), 1);
    feedbackShader_.setUniform1f("decay", 0.92f);
    ofSetColor(255);
    res.tonemap.draw(0, 0, w, h);
    feedbackShader_.end();
    res.feedback.end();

    ofEnableAlphaBlending();
    ofSetColor(255);
    res.feedback.draw(0, 0, w, h);
}

void VolumetricEngine::drawWindow(int channelOffset, int w, int h, PresentationResources& res) {
    renderPresentation(channelOffset, w, h, res);
    ofSetColor(170);
    ofDrawBitmapString(generator.id + "  " + ofToString(static_cast<int>(players_.size())) + " players", 12, 18);
    ofSetColor(255);
}

void VolumetricEngine::drawChannel(int channel, int x, int y, int w, int h, PresentationResources& res) {
    (void)channel;
    (void)x;
    (void)y;
    drawWindow(0, w * 4, h, res);
}

void VolumetricEngine::sendOsc() {
    if (!oscReady)
        return;
    pdjv::FrameRecord interp, nearest;
    if (shared_.timeline)
        shared_.timeline->current(interp, nearest);
    for (int ch = 0; ch < 8; ++ch) {
        ofxOscMessage m;
        m.setAddress("/pdjv/channel/" + ofToString(ch) + "/timeline/time");
        m.addFloatArg(shared_.timeline ? shared_.timeline->time() : 0);
        osc.sendMessage(m, false);
        ofxOscMessage n;
        n.setAddress("/pdjv/channel/" + ofToString(ch) + "/scene/id");
        n.addStringArg(generator.id);
        osc.sendMessage(n, false);
        ofxOscMessage p;
        p.setAddress("/pdjv/channel/" + ofToString(ch) + "/scene/preset");
        p.addIntArg(generator.preset);
        osc.sendMessage(p, false);
        ofxOscMessage c;
        c.setAddress("/pdj/channel/" + ofToString(ch) + "/score/mode");
        c.addIntArg(generator.id == "ScanVolume" ? 1 : 0);
        osc.sendMessage(c, false);
        ofxOscMessage blobs;
        blobs.setAddress("/pdj/channel/" + ofToString(ch) + "/blobs/count");
        blobs.addIntArg(static_cast<int>(interp.players.size()));
        osc.sendMessage(blobs, false);
        ofxOscMessage cam;
        cam.setAddress("/pdjv/channel/" + ofToString(ch) + "/camera/mode");
        cam.addStringArg(cameraMode_ == CameraMode::FullBody ? "fullBody" : "portraitBust");
        osc.sendMessage(cam, false);
    }
}

void VolumetricEngine::drawControl() {
    gui.begin();
    ImGui::Begin("Volumetric runtime");
    ImGui::TextUnformatted(status_.c_str());
    ImGui::Text("package %s", packageError.empty() ? "ok" : packageError.c_str());
    ImGui::Text("time %.2f  players %zu  dt %.1f ms  fps %.1f",
                shared_.timeline ? shared_.timeline->time() : 0.0,
                players_.size(), lastDt * 1000.0, ofGetFrameRate());
    ImGui::Text("gpu sim %s  wire %s  cache loads %llu  osc in %s",
                gpuSimOk_ ? "on" : "cpu",
                wireShaderOk_ ? "on" : "off",
                shared_.cache ? static_cast<unsigned long long>(shared_.cache->stats.loads.load()) : 0ull,
                oscInReady ? "9002" : "off");
    ImGui::Checkbox("play", &playing);
    ImGui::Checkbox("composer", &composerEnabled_);
    if (ImGui::Button("reset"))
        reset();
    ImGui::SliderFloat("camera", &cameraAngle, -35.f, 35.f);
    ImGui::SliderFloat("bloom thr", &bloomThreshold, 0.1f, 1.2f);
    ImGui::SliderFloat("exposure", &exposure_, 0.5f, 2.f);
    ImGui::SliderFloat("echo z", &generator.echoSpacing, 0.02f, 0.2f);
    const char* cams[] = {"portraitBust", "fullBody"};
    int camIdx = cameraMode_ == CameraMode::FullBody ? 1 : 0;
    if (ImGui::Combo("camera mode", &camIdx, cams, 2))
        cameraMode_ = camIdx == 1 ? CameraMode::FullBody : CameraMode::PortraitBust;
    const auto ids = GeneratorRegistry::ids();
    int g = 0;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i)
        if (generator.id == ids[i])
            g = i;
    std::vector<const char*> genPtrs;
    for (const auto& id : ids)
        genPtrs.push_back(id.c_str());
    if (ImGui::Combo("generator", &g, genPtrs.data(), static_cast<int>(genPtrs.size()))) {
        generator.id = ids[g];
        refreshRenderState();
    }
    ImGui::SliderInt("preset", &generator.preset, 0, 2);
    if (ImGui::IsItemDeactivatedAfterEdit())
        refreshRenderState();
    ImGui::End();
    gui.end();
}

VolumetricPresentationApp::VolumetricPresentationApp(VolumetricEngine* engine, int offset, int w, int h, int group)
    : engine_(engine), offset_(offset), w_(w), h_(h) {
    resources.ownerGroup = group;
}

void VolumetricPresentationApp::setup() {
    ofSetFrameRate(30);
    ofBackground(0);
    resources.allocate(std::max(1, w_), std::max(1, h_), true);
    if (engine_->status() == "init") {
        ofJson cfg = ofLoadJson("settings.json");
        if (!cfg.is_object())
            cfg = ofJson::object();
        engine_->setup(cfg);
    }
}

void VolumetricPresentationApp::update() {
    if (offset_ == 0)
        engine_->update(ofGetLastFrameTime());
}

void VolumetricPresentationApp::draw() {
    engine_->drawWindow(offset_, w_, h_, resources);
}

VolumetricControlApp::VolumetricControlApp(VolumetricEngine* engine) : engine_(engine) {}

void VolumetricControlApp::setup() {
    engine_->gui.setup();
}

void VolumetricControlApp::update() {}

void VolumetricControlApp::draw() {
    ofBackground(12);
    if (engine_->status() != "init")
        engine_->drawControl();
}

void VolumetricControlApp::keyPressed(int key) {
    if (key == ' ')
        engine_->playing = !engine_->playing;
    if (key == 'r')
        engine_->reset();
}
