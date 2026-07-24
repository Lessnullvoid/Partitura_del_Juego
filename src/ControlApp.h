#pragma once
#include "ofMain.h"
#include "ofxImGui.h"
#include "Channel.h"
#include "ClipPool.h"
#include "OSCSender.h"
#include "EventDetector.h"
#include "GlobalDirector.h"
#include "VideoDirector.h"

class ControlApp : public ofBaseApp {
public:
    ControlApp(std::vector<Channel*> channels, ClipPool* pool, OSCSender* osc,
               GlobalDirector* dir = nullptr, VideoDirector* videoDir = nullptr)
        : channels_(channels), pool_(pool), osc_(osc), dir_(dir), videoDir_(videoDir) {}

    void setup()  override;
    void update() override;
    void draw()   override;
    void exit()   override;
    void keyPressed(int key) override;

private:
    void drawGlobalPanel();
    void drawOverview();
    void drawDirectorPanel();
    void drawVideoDirectorPanel();
    void drawChannelPanel(int i);

    std::vector<Channel*> channels_;
    ClipPool*             pool_    = nullptr;
    OSCSender*            osc_     = nullptr;
    GlobalDirector*       dir_     = nullptr;
    VideoDirector*        videoDir_ = nullptr;

    ofxImGui::Gui gui_;
    bool          showUI_ = true;
    int           activePage_ = 0;
    int           activeChannel_ = 0;
    GLuint        fontTexture_ = 0;

    char oscHostBuf_[128] = "localhost";
    int  oscPort_         = 9001;
};
