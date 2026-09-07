#pragma once
#include "ofMain.h"
#include "CVPipeline.h"
#include "GraphicScore.h"
#include "OSCSender.h"
#include "ClipPool.h"
#include "EventDetector.h"
#include "GlobalDirector.h"
#include "VideoDirector.h"
#include "VisualComposer.h"
#include "VisualGenerator.h"
#include "VideoPointCloudGenerator.h"
#include "pdjv/PdjvOptionalBridge.h"
#include "PerformanceMonitor.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

struct GeneratorRuntimeParams {
    float intensity = 0.65f;
    float density = 0.5f;
    float glowGain = 0.9f;
    float glowRadius = 2.2f;
    float feedbackDecay = 0.82f;
    ofColor cyan = ofColor(238, 238, 238);
    ofColor red = ofColor(112, 112, 112);
    bool showHud = false;
    int detailTier = 2;
};

class Channel {
public:
    Channel() = default;
    ~Channel();

    void setup(int idx, int w, int h, ClipPool* pool, OSCSender* osc, const CVParams& cvp);
    void update();
    void draw();
    void drawInRegion(int x, int y, int segW, int segH);

    void loadNextClip();
    void play();
    void pause();
    void stop();
    void setSpeed(float s);        // fija la velocidad base; efectiva = base * globalMultiplier
    float getBaseSpeed()  const { return baseSpeed_; }

    bool        isPlaying()      const;
    std::string getCurrentClip() const { return currentClip_; }
    int         getIdx()         const { return idx_; }
    float       getUpdateMilliseconds() const { return updateMilliseconds_; }

    CVPipeline&    getCVPipeline()  { return cv_; }
    GraphicScore&  getScore()       { return score_; }
    EventDetector& getDetector()    { return detector_; }
    ofVideoPlayer& getPlayer()      { return player_; }

    void setGlobalDirector(GlobalDirector* d) { dir_ = d; }
    void setVideoDirector(VideoDirector* d) { videoDir_ = d; }
    void setVisualComposer(VisualComposer* c) { composer_ = c; }
    void setPerformanceMonitor(PerformanceMonitor* monitor) {
        performanceMonitor_ = monitor;
    }
    GeneratorRuntimeParams& generatorParams() { return generatorParams_; }
    // Polaridad de la imagen monocroma: false dibuja claro sobre negro, true
    // invierte la región terminada a negro sobre blanco.
    void setInvertPolarity(bool invert) { invertPolarity_ = invert; }
    bool invertPolarity() const { return invertPolarity_; }
    const ChapterState* composerState() const;
    VisualGenerator& getGenerator() { return generator_; }
    VideoPointCloudGenerator& videoPointCloud() { return videoPointCloud_; }
    VideoPointCloudSettings& videoPointCloudSettings() { return vpcSettings_; }
    void configureVideoPointCloud(const VideoPointCloudSettings& settings);
    bool applyVpcOscParam(const std::string& param, const ofxOscMessage& msg);
    void resetVideoPointCloudFeedback();
    bool videoPointCloudActive() const { return vpcSettings_.enabled; }
    VideoPlanType getVideoPlanType() const { return activePlan_.type; }
    bool isSharedVideoPlan() const { return activePlan_.shared; }

private:
    void sendOscSnapshot(CVData& data);
    void applyPendingVideoPlan();
    bool loadVideoPlan(const VideoPlan& plan);
    bool configureLoadedPlan();
    void finishActivePlan();
    bool updateProceduralChapter();
    void populateGeneratorOsc(CVData& data) const;
    void populateVpcOsc(CVData& data) const;
    VideoGeneratorContext buildVideoGeneratorContext() const;
    void drawVideoPointCloud(int x, int y, int segW, int segH);
    // isVpc: si es true, se suprime la superposición de inversión del generador
    // del compositor para que los fotogramas VideoPointCloud no se vean afectados por el evento invert.
    void applyPolarity(int x, int y, int segW, int segH, bool isVpc = false) const;

    // Procesamiento CV fuera de hilo. Recoge píxeles publicados por el hilo principal,
    // llama a cv_.update() y publica el resultado.
    void cvWorkerLoop();
    // Reset seguro entre hilos: se serializa frente a una llamada cv_.update() en curso.
    void resetCvAsync();

    int           idx_ = 0, w_ = 0, h_ = 0;
    std::string   currentClip_;
    float         baseSpeed_ = 1.0f;
    int           oscFrameSequence_ = 0;
    int           videoRevision_ = 0;
    int           scoreRevision_ = 0;
    int           lastScoreMode_ = -1;
    float         updateMilliseconds_ = 0.f;
    VideoPlan     activePlan_;
    int           appliedPlanRevision_ = 0;
    float         segmentStartSeconds_ = 0.f;
    float         segmentEndSeconds_ = 0.f;
    float         planStartedAt_ = 0.f;
    float         lastSyncCorrectionAt_ = 0.f;
    bool          planStarted_ = false;
    bool          planFinished_ = false;
    bool          planConfigured_ = false;

    ofVideoPlayer  player_;
    ofFbo          bwFbo_;
    ofFbo          cvFbo_;
    ofPixels       grayPixels_;

    CVPipeline     cv_;
    GraphicScore   score_;
    VisualGenerator generator_;
    VideoPointCloudGenerator videoPointCloud_;
    PdjvOptionalBridge pdjvBridge_;
    VideoPointCloudSettings vpcSettings_;
    PdjvFrameView pdjvView_;
    ofTexture pdjvMaskTex_;
    ofTexture pdjvDepthTex_;
    EventDetector  detector_;
    GeneratorRuntimeParams generatorParams_;
    bool invertPolarity_ = false;
    VisualState transferredVisualState_;
    uint64_t lastComposerRevision_ = 0;

    ClipPool*        pool_ = nullptr;
    OSCSender*       osc_  = nullptr;
    GlobalDirector*  dir_  = nullptr;
    VideoDirector*   videoDir_ = nullptr;
    VisualComposer*  composer_ = nullptr;
    PerformanceMonitor* performanceMonitor_ = nullptr;
    ChannelPerformanceSample performanceSample_;

    // --- Hilo trabajador CV ------------------------------------------------
    // cvMutex_ protege: cvPending_, cvRunning_, cvPixelShare_.
    // cvComputeMutex_ serializa cv_.update() frente a cv_.reset().
    // cvDataMutex_ protege cvDataReady_ (escribe el trabajador, lee el principal).
    // cvPubData_ es solo del hilo principal (se consume tras fijar cvResultReady_).
    std::thread             cvThread_;
    std::mutex              cvMutex_;
    std::condition_variable cvCv_;
    ofPixels                cvPixelShare_;      // staging de píxeles bajo cvMutex_
    bool                    cvPending_  = false;
    bool                    cvRunning_  = false;

    std::mutex              cvComputeMutex_;
    std::mutex              cvDataMutex_;
    CVData                  cvDataReady_;
    std::atomic<bool>       cvResultReady_{false};

    CVData                  cvPubData_;         // resultado publicado solo en el hilo principal
};
