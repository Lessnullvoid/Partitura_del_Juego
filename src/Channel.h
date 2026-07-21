#pragma once
#include "ofMain.h"
#include "CVPipeline.h"
#include "GraphicScore.h"
#include "OSCSender.h"
#include "ClipPool.h"
#include "EventDetector.h"
#include "GlobalDirector.h"

class Channel {
public:
    void setup(int idx, int w, int h, ClipPool* pool, OSCSender* osc, const CVParams& cvp);
    void update();
    void draw();

    void loadNextClip();
    void play();
    void pause();
    void stop();
    void setSpeed(float s);        // sets base speed; effective = base * globalMultiplier
    float getBaseSpeed()  const { return baseSpeed_; }

    bool        isPlaying()      const;
    std::string getCurrentClip() const { return currentClip_; }
    int         getIdx()         const { return idx_; }

    CVPipeline&    getCVPipeline()  { return cv_; }
    GraphicScore&  getScore()       { return score_; }
    EventDetector& getDetector()    { return detector_; }
    ofVideoPlayer& getPlayer()      { return player_; }

    void setGlobalDirector(GlobalDirector* d) { dir_ = d; }

private:
    int           idx_ = 0, w_ = 0, h_ = 0;
    std::string   currentClip_;
    float         baseSpeed_ = 1.0f;

    ofVideoPlayer  player_;
    ofFbo          bwFbo_;
    ofFbo          cvFbo_;
    ofPixels       grayPixels_;

    CVPipeline     cv_;
    GraphicScore   score_;
    EventDetector  detector_;

    ClipPool*        pool_ = nullptr;
    OSCSender*       osc_  = nullptr;
    GlobalDirector*  dir_  = nullptr;
};
