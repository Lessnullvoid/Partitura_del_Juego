#pragma once
#include "ofMain.h"
#include "OSCSender.h"

class EventDetector {
public:
    struct Params {
        float ballMaxArea      = 2000.f;  // px^2 a res. de análisis — blobs menores que esto = posible balón
        float ballMinSpeed     = 0.015f;  // umbral de magnitud de velocidad normalizada
        int   crowdMinBlobs    = 5;       // mínimo de blobs para un evento crowd
        float crowdMaxDist     = 0.25f;   // radio normalizado para contar blobs como "juntos"
        float collisionOverlap = 0.30f;   // umbral IoU para disparar colisión
        int   prevCountDrop    = 2;       // una caída de recuento de blobs ≥ esto dispara colisión
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
