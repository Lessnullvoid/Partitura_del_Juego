#include "EventDetector.h"

void EventDetector::update(CVData& data) {
    EventData& ev = data.events;

    ev.collision    = detectCollision(data);
    ev.ballDetected = detectBall(data, ev.ballPos);
    ev.crowdDensity = detectCrowd(data);
    ev.legDistance  = detectLegDistance(data);

    prevBlobCount_ = data.blobCount;
}

bool EventDetector::detectCollision(const CVData& d) {
    // Heuristic 1: sudden drop in blob count (merge)
    int drop = prevBlobCount_ - d.blobCount;
    if (prevBlobCount_ > 0 && drop >= params_.collisionOverlap * prevBlobCount_) {
        return true;
    }

    // Heuristic 2: any two blobs overlap significantly (IoU on normalized bounding boxes)
    int n = (int)d.blobs.size();
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            const auto& a = d.blobs[i];
            const auto& b = d.blobs[j];

            float ax0 = a.x - a.bbW * 0.5f, ax1 = a.x + a.bbW * 0.5f;
            float ay0 = a.y - a.bbH * 0.5f, ay1 = a.y + a.bbH * 0.5f;
            float bx0 = b.x - b.bbW * 0.5f, bx1 = b.x + b.bbW * 0.5f;
            float by0 = b.y - b.bbH * 0.5f, by1 = b.y + b.bbH * 0.5f;

            float interW = std::max(0.f, std::min(ax1, bx1) - std::max(ax0, bx0));
            float interH = std::max(0.f, std::min(ay1, by1) - std::max(ay0, by0));
            float inter  = interW * interH;

            float areaA  = a.bbW * a.bbH;
            float areaB  = b.bbW * b.bbH;
            float uni    = areaA + areaB - inter;

            if (uni > 0.f && (inter / uni) >= params_.collisionOverlap) {
                return true;
            }
        }
    }
    return false;
}

bool EventDetector::detectBall(const CVData& d, glm::vec2& outPos) {
    float bestSpeed = -1.f;
    int   bestIdx   = -1;

    for (int i = 0; i < (int)d.blobs.size(); i++) {
        const auto& b = d.blobs[i];
        if (b.area > params_.ballMaxArea) continue;

        float speed = sqrtf(b.vx * b.vx + b.vy * b.vy);
        if (speed >= params_.ballMinSpeed && speed > bestSpeed) {
            bestSpeed = speed;
            bestIdx   = i;
        }
    }

    if (bestIdx >= 0) {
        outPos = {d.blobs[bestIdx].x, d.blobs[bestIdx].y};
        return true;
    }
    return false;
}

float EventDetector::detectCrowd(const CVData& d) {
    int n = (int)d.blobs.size();
    if (n < params_.crowdMinBlobs) return 0.f;

    // Count the largest cluster within crowdMaxDist of each blob
    int maxCluster = 0;
    float r2 = params_.crowdMaxDist * params_.crowdMaxDist;

    for (int i = 0; i < n; i++) {
        int cluster = 1;
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            float dx = d.blobs[i].x - d.blobs[j].x;
            float dy = d.blobs[i].y - d.blobs[j].y;
            if (dx * dx + dy * dy <= r2) cluster++;
        }
        maxCluster = std::max(maxCluster, cluster);
    }

    float density = ofClamp((float)(maxCluster - params_.crowdMinBlobs + 1) /
                            (float)std::max(1, n - params_.crowdMinBlobs + 1), 0.f, 1.f);
    return density;
}

float EventDetector::detectLegDistance(const CVData& d) {
    // For each blob, bbH / sqrt(area) approximates elongation (standing player = high ratio)
    if (d.blobs.empty()) return 0.f;

    float total = 0.f;
    for (const auto& b : d.blobs) {
        if (b.area <= 0.f) continue;
        float elongation = b.bbH / sqrtf(b.area / (float)(1080 * 1920));
        total += ofClamp(elongation, 0.f, 1.f);
    }
    return total / (float)d.blobs.size();
}
