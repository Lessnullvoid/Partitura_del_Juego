#pragma once
#include "ofMain.h"
#include "Channel.h"
#include "ClipPool.h"
#include "OSCSender.h"
#include "GlobalDirector.h"

class ChannelApp : public ofBaseApp {
public:
    ChannelApp(Channel* ch, int idx, int w, int h,
               ClipPool* pool, OSCSender* osc, const CVParams& cvp,
               GlobalDirector* dir = nullptr)
        : ch_(ch), idx_(idx), w_(w), h_(h), pool_(pool), osc_(osc), cvp_(cvp), dir_(dir) {}

    void setup()  override;
    void update() override;
    void draw()   override;

private:
    Channel*         ch_;
    int              idx_, w_, h_;
    ClipPool*        pool_;
    OSCSender*       osc_;
    CVParams         cvp_;
    GlobalDirector*  dir_;
};
