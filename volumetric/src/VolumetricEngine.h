#pragma once

#include "data/PdjvRuntime.h"
#include "director/VolumetricComposer.h"
#include "generators/IVolumetricGenerator.h"
#include "volume/PlayerSurfaceMesh.h"
#include "ofMain.h"
#include "ofxOsc.h"
#include "ofxImGui.h"
#include <map>
#include <memory>
#include <string>

enum class QualityTier { Preview, Installation, Diagnostic };

struct PingPongFbo {
    ofFbo a;
    ofFbo b;
    bool flip = false;
    ofFbo& source() { return flip ? b : a; }
    ofFbo& destination() { return flip ? a : b; }
    void swap() { flip = !flip; }
    void allocate(int w, int h) {
        ofFbo::Settings s;
        s.width = w;
        s.height = h;
        s.internalformat = GL_RGBA16F;
        s.useDepth = false;
        a.allocate(s);
        b.allocate(s);
    }
};

struct PresentationResources {
    int ownerGroup = 0;
    ofFbo geometry;
    ofFbo emissive;
    ofFbo bloomA;
    ofFbo bloomB;
    ofFbo tonemap;
    ofFbo feedback;
    void allocate(int w, int h, bool hdr);
};

class VolumetricEngine {
public:
    void setup(const ofJson& cfg);
    void update(float dt);
    void drawWindow(int channelOffset, int w, int h, PresentationResources& res);
    void drawChannel(int channel, int x, int y, int w, int h, PresentationResources& res);
    void drawControl();
    void reset();
    const std::string& status() const { return status_; }
    bool playing = true;
    ofxImGui::Gui gui;

private:
    QualityTier tier = QualityTier::Preview;
    pdjv::PdjvAssetPool pool_;
    pdjv::PdjvAssetPool::Shared shared_;
    pdjv::VolumetricContentDirector director;
    VolumetricComposer composer_;
    std::string primaryPath_;
    std::string status_ = "init";
    std::string packageError;
    std::map<int, PlayerSurfaceMesh> players_;
    GeneratorState generator;
    VolumetricRenderState renderState_;
    SurfaceTopology surfaceTopology_ = SurfaceTopology::Both;
    CameraMode cameraMode_ = CameraMode::PortraitBust;
    int maskGridW_ = 32;
    int maskGridH_ = 64;
    int pointsPerPlayer = 2400;
    int particleRes = 48;
    int frameNumber = 0;
    float lastDt = 0.f;
    float cameraAngle = 0.f;
    float bloomThreshold = 0.55f;
    float bloomGain = 1.f;
    float exposure_ = 1.15f;
    bool useGpuPositions_ = false;
    bool composerEnabled_ = false;
    ofEasyCam cam_;
    // cloud_ se sustituye por un ofVbo persistente + vectores CPU para evitar
    // la reasignación del búfer GPU (glBufferData) en cada fotograma en Metal.
    ofVbo             cloudVbo_;
    std::vector<glm::vec3>     cpuCloudPos_;
    std::vector<ofFloatColor>  cpuCloudColors_;
    int               cloudActiveCount_ = 0;
    int               cloudMaxPts_ = 0;
    ofVboMesh wires_;
    ofVboMesh triangles_;
    ofVboMesh segments_;
    ofVboMesh groundGridMesh_;
    ofShader pointShader_;
    ofShader wireShader_;
    ofShader meshWireframeShader_;
    ofShader groundGridShader_;
    ofShader splatShader_;
    ofShader bloomShader_;
    ofShader tonemapShader_;
    ofShader feedbackShader_;
    ofShader simulateShader_;
    ofShader fullscreenShader_;
    ofTexture targetTex_;
    ofPixels targetPix_;
    PingPongFbo simPos_;
    PingPongFbo simVel_;
    ofFbo history_[5];
    bool shadersOk_ = false;
    bool wireShaderOk_ = false;
    bool meshWireframeShaderOk_ = false;
    bool groundGridShaderOk_ = false;
    bool splatShaderOk_ = false;
    bool gpuSimOk_ = false;
    bool simSeeded_ = false;
    glm::vec3 cloudCenter_{0, 0.2f, 0};
    float cloudRadius_ = 1.2f;
    uint64_t seed = 1;
    ofxOscSender osc;
    ofxOscReceiver oscIn;
    bool oscReady = false;
    bool oscInReady = false;

    glm::vec3 worldFromImage(float x, float y, float z) const;
    void ensurePlayers(const pdjv::FrameRecord& frame, double t);
    void applyGenerator(PlayerSurfaceMesh& mesh, float dt);
    void pollOsc();
    void packGpuTargets();
    void runGpuSim(float dt);
    void buildCloud(const pdjv::FrameRecord& frame);
    void buildGroundGrid();
    void drawChannelScene(int channel, int w, int h);
    void renderPresentation(int channelOffset, int w, int h, PresentationResources& res);
    void sendOsc();
    SurfaceTopology topologyForPreset() const;
    void refreshRenderState();
};

class VolumetricPresentationApp : public ofBaseApp {
public:
    VolumetricPresentationApp(VolumetricEngine* engine, int offset, int w, int h, int group);
    void setup() override;
    void update() override;
    void draw() override;

private:
    VolumetricEngine* engine_;
    int offset_;
    int w_, h_;
    PresentationResources resources;
};

class VolumetricControlApp : public ofBaseApp {
public:
    explicit VolumetricControlApp(VolumetricEngine* engine);
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    VolumetricEngine* engine_;
};
