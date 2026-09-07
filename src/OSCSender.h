#pragma once
#include "ofMain.h"
#include "ofxOsc.h"
#include <string>

struct EventData {
    bool      collision    = false;
    bool      ballDetected = false;
    glm::vec2 ballPos      = {0.f, 0.f};
    float     crowdDensity = 0.f;
    float     legDistance  = 0.f;
};

struct CVData {
    int    channelIdx    = 0;
    int    frameSequence = 0;
    float  timestamp     = 0.f;
    float  flowMagnitude = 0.f;
    float  flowAngle     = 0.f;
    float  motionEnergy  = 0.f;
    int    blobCount     = 0;

    struct BlobEntry {
        float x, y, vx, vy, area;
        float bbW = 0.f, bbH = 0.f;
    };
    std::vector<BlobEntry> blobs;
    float contourLength = 0.f;

    EventData events;
    int         scoreMode     = 0;
    int         scoreRevision = 0;
    std::string videoName;
    float       videoPosition = 0.f;
    float       videoDuration = 0.f;
    int         videoRevision = 0;
    int         videoPlanType = 0;
    int         videoShared = 0;
    int         temporalPhase = 0;
    float       speedMultiplier = 1.f;
    int         clearPhase = 0;
    float       clearAlpha = 0.f;

    // Compositor visual procedimental. Los valores por defecto conservan el flujo legado solo-vídeo.
    int         generatorActive = 0;
    int         composerContent = -1;
    int         generatorMode = -1;
    int         generatorRevision = 0;
    int         organizationMode = 0;
    int         screenRole = 0;
    int         generatorStage = 0;
    float       generatorStageProgress = 0.f;
    float       generatorChapterPhase = 0.f;
    float       generatorBeatPhase = 0.f;
    int         generatorBeatIndex = 0;
    int         generatorSubdivisionIndex = 0;
    int         generatorBeatSubdivision = 4;
    float       generatorSubdivisionPulse = 0.f;
    float       generatorBpm = 90.f;
    float       generatorEnvelope = 0.f;
    int         generatorSeed = 0;
    float       generatorIntensity = 0.f;
    float       generatorDensity = 0.f;
    float       generatorRolePhase = 0.f;
    float       generatorPropagationDelay = 0.f;
    int         generatorObservedGroup = 1;
    int         generatorResolvedSeed = 0;
    int         transitionActive = 0;
    int         programEnabled = 0;
    int         installationMoment = 0;
    int         groupVideoOccupancy = 0;
    int         globalTakeover = 0;
    int         collectiveMovement = 1;
    int         collectiveRevision = 0;
    float       collectivePhase = 0.f;
    float       collectiveActivity = 0.f;
    float       collectiveCoherence = 0.f;
    float       collectiveDiversity = 0.f;
    float       collectiveConvergence = 0.f;
    float       collectivePopulation = 0.f;
    float       collectiveTension = 0.f;
    int         collectiveDominantGenerator = -1;

    // Nube de puntos de vídeo en GPU (generador visual primario opcional).
    int         vpcEnabled = 0;
    int         vpcDepthSource = 0;
    int         vpcMaskMode = 0;
    int         vpcPreset = 0;
    int         vpcGridWidth = 0;
    int         vpcGridHeight = 0;
    float       vpcDepthScale = 0.f;
    float       vpcPointSize = 0.f;
    float       vpcLuminanceFloor = 0.f;
    float       vpcColorGain = 0.f;
    float       vpcCameraYaw = 0.f;
    float       vpcCameraDistance = 0.f;
    int         vpcFeedbackEnabled = 0;
    float       vpcFeedbackDecay = 0.f;
};

class OSCSender {
public:
    void setup(const std::string& host, int port);
    void send(const CVData& data);

    void setHost(const std::string& host) { host_ = host; reconnect(); }
    void setPort(int port)               { port_ = port; reconnect(); }
    const std::string& getHost() const   { return host_; }
    int  getPort()               const   { return port_; }

private:
    void reconnect();

    // coreBundle y blobBundle se envían en cada llamada.
    // contextBundle y generatorBundle se envían solo cuando cambian los campos de
    // revisión rastreados, o cada kHeartbeatInterval fotogramas como refresco garantizado.
    static constexpr int kHeartbeatInterval = 6;

    ofxOscSender sender_;
    std::string  host_;
    int          port_ = 9001;

    int heartbeatCounter_       = 0;
    int lastVideoRevision_      = -1;
    int lastScoreRevision_      = -1;
    int lastGeneratorRevision_  = -1;
};
