#pragma once
#include "ofMain.h"
#include "OSCSender.h"

class EventDetector {
public:
    struct Params {
        float ballMaxArea      = 2000.f;  // px^2 at analysis res — blobs smaller than this = potential ball
        float ballMinSpeed     = 0.015f;  // normalized velocity magnitude threshold
        int   crowdMinBlobs    = 5;       // minimum blobs for a crowd event
        float crowdMaxDist     = 0.25f;   // normalized radius to count blobs as "together"
        float collisionOverlap = 0.30f;   // IoU threshold to fire collision
        int   prevCountDrop    = 2;       // blob count drop ≥ this triggers collision
    };

    void update(CVData& data);

    Params& params() { return params_; }

private:
    bool  detectCollision(const CVData& d);
    bool  detectBall(const CVData& d, glm::vec2& outPos);
    float detectCrowd(const CVData& d);
    float detectLegDistance(const CVData& d);

    Params   params_;
    int      prevBlobCount_ = 0;
};
