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
#include "PerformanceMonitor.h"

struct GeneratorRuntimeParams {
    float intensity = 0.65f;
    float density = 0.5f;
    bool showHud = false;
    int detailTier = 2;
};

class Channel {
public:
    void setup(int idx, int w, int h, ClipPool* pool, OSCSender* osc, const CVParams& cvp);
    void update();
    void draw();
    void drawInRegion(int x, int y, int segW, int segH);

    void loadNextClip();
    void play();
    void pause();
    void stop();
    void setSpeed(float s);        // sets base speed; effective = base * globalMultiplier
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
    const ChapterState* composerState() const;
    VisualGenerator& getGenerator() { return generator_; }
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
    EventDetector  detector_;
    GeneratorRuntimeParams generatorParams_;
    VisualState transferredVisualState_;
    uint64_t lastComposerRevision_ = 0;

    ClipPool*        pool_ = nullptr;
    OSCSender*       osc_  = nullptr;
    GlobalDirector*  dir_  = nullptr;
    VideoDirector*   videoDir_ = nullptr;
    VisualComposer*  composer_ = nullptr;
    PerformanceMonitor* performanceMonitor_ = nullptr;
    ChannelPerformanceSample performanceSample_;
};
