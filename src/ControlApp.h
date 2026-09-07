#pragma once
#include "ofMain.h"
#include "ofxImGui.h"
#include "Channel.h"
#include "ClipPool.h"
#include "OSCSender.h"
#include "EventDetector.h"
#include "GlobalDirector.h"
#include "VideoDirector.h"
#include "PerformanceMonitor.h"
#include "ofxOsc.h"

class ControlApp : public ofBaseApp {
public:
    ControlApp(std::vector<Channel*> channels, ClipPool* pool, OSCSender* osc,
               GlobalDirector* dir = nullptr, VideoDirector* videoDir = nullptr,
               VisualComposer* composer = nullptr,
               PerformanceMonitor* performance = nullptr,
               int oscListenPort = 9002)
        : channels_(channels), pool_(pool), osc_(osc), dir_(dir),
          videoDir_(videoDir), composer_(composer), performance_(performance),
          oscListenPort_(oscListenPort) {}

    void setup()  override;
    void update() override;
    void draw()   override;
    void exit()   override;
    void keyPressed(int key) override;

private:
    bool saveOutputMode(const std::string& mode);
    bool detectAndSaveDualOutputs();
    bool configureMixedWall();
    void buildWallSummaryFromSettings(const ofJson& cfg);
    void drawGlobalPanel();
    void drawOverview();
    void drawDirectorPanel();
    void drawVideoDirectorPanel();
    void drawComposerPanel();
    void drawPerformancePanel();
    void drawChannelPanel(int i);
    void pollVpcOsc();

    std::vector<Channel*> channels_;
    ClipPool*             pool_    = nullptr;
    OSCSender*            osc_     = nullptr;
    GlobalDirector*       dir_     = nullptr;
    VideoDirector*        videoDir_ = nullptr;
    VisualComposer*       composer_ = nullptr;
    PerformanceMonitor*   performance_ = nullptr;

    ofxImGui::Gui gui_;
    bool          showUI_ = true;
    bool          dualWindowConfigured_ = false;
    bool          outputRestartRequired_ = false;
    std::string   outputModeError_;
    std::string   detectedOutputSummary_;
    std::string   wallASummary_;   // líneas de identidad por muro mostradas en la app de control
    std::string   wallBSummary_;
    int           activePage_ = 0;
    int           activeChannel_ = 0;
    GLuint        fontTexture_ = 0;
    ofxOscReceiver oscReceiver_;
    int           oscListenPort_ = 9002;
    bool          oscReceiverReady_ = false;

    char oscHostBuf_[128] = "localhost";
    int  oscPort_         = 9001;
};
