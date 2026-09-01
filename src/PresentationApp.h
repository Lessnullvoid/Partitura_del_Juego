#pragma once
#include "ofMain.h"
#include "Channel.h"
#include "ClipPool.h"
#include "OSCSender.h"
#include "GlobalDirector.h"
#include "VideoDirector.h"
#include "PerformanceMonitor.h"
#include <array>

// Presentation app for one controller output: renders four channels into equal
// vertical segments. Two instances cover channels 0-3 and 4-7 in dualWindow8.
class PresentationApp : public ofBaseApp {
public:
    static constexpr int kSegments = 4;

    PresentationApp(const std::array<Channel*, kSegments>& channels,
                    int channelOffset, int totalW, int totalH,
                    ClipPool* pool, OSCSender* osc, const CVParams& cvp,
                    GlobalDirector* dir = nullptr, VideoDirector* videoDir = nullptr,
                    VisualComposer* composer = nullptr,
                    PerformanceMonitor* performance = nullptr,
                    int performanceWindow = 0, int targetFps = 30);

    void setup()  override;
    void update() override;
    void draw()   override;

private:
    std::array<Channel*, kSegments> channels_;
    int             channelOffset_;
    int             totalW_, totalH_, segW_;
    ClipPool*       pool_;
    OSCSender*      osc_;
    CVParams        cvp_;
    GlobalDirector* dir_;
    VideoDirector*  videoDir_;
    VisualComposer* composer_;
    PerformanceMonitor* performance_;
    int performanceWindow_;
    int targetFps_;
};
