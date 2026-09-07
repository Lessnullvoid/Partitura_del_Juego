#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace pdjv {

inline constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ull;
inline constexpr uint64_t kFnvPrime = 0x100000001B3ull;
inline constexpr uint16_t kIndexVersion = 0;
inline constexpr char kIndexMagic[4] = {'P', 'D', 'J', 'I'};

inline uint64_t fnv1a64(const uint8_t* data, size_t n) {
    uint64_t h = kFnvOffset;
    for (size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= kFnvPrime;
    }
    return h;
}

inline uint64_t stablePointId(const std::string& packageId, int trackingId, int pointSeed) {
    std::vector<uint8_t> payload(packageId.begin(), packageId.end());
    payload.push_back(0);
    uint8_t ids[8];
    int32_t t = trackingId;
    int32_t s = pointSeed;
    std::memcpy(ids, &t, 4);
    std::memcpy(ids + 4, &s, 4);
    payload.insert(payload.end(), ids, ids + 8);
    return fnv1a64(payload.data(), payload.size());
}

enum class BodyRegion : int {
    Unknown = 0,
    Head = 1,
    Torso = 2,
    LeftArm = 3,
    RightArm = 4,
    LeftLeg = 5,
    RightLeg = 6
};

struct Joint {
    float x = 0, y = 0, z = 0, c = 0;
};

struct PlayerObservation {
    int trackingId = -1;
    int teamId = -1;
    bool observed = false;
    float confidence = 0;
    float imageCentroid[2] = {0, 0};
    float velocity[2] = {0, 0};
    float acceleration[2] = {0, 0};
    float boundingBox[4] = {0, 0, 0, 0};
    float fieldPosition[2] = {0, 0};
    float groundContact[2] = {0, 0};
    float estimatedScale = 1;
    float fieldConfidence = 0;
    uint32_t maskOffset = 0, maskSize = 0, maskWidth = 0, maskHeight = 0;
    uint32_t depthOffset = 0, depthSize = 0, depthWidth = 0, depthHeight = 0;
    uint32_t motionOffset = 0, motionSize = 0, motionWidth = 0, motionHeight = 0;
    std::vector<Joint> joints;
};

struct FrameRecord {
    int frame = 0;
    double timestamp = 0;
    float globalMotionEnergy = 0;
    float collectiveCentroid[2] = {0.5f, 0.5f};
    float collectiveDirection[2] = {0, 0};
    float collectiveDensity = 0;
    std::vector<int> eventRefs;
    std::vector<PlayerObservation> players;
};

struct IndexRecord {
    int32_t frame = 0;
    double timestamp = 0;
    uint64_t offset = 0;
    uint32_t size = 0;
};

struct Manifest {
    int version = 0;
    std::string format;
    std::string packageId;
    std::string sourceId;
    std::string analysisId;
    std::string error;
    double fps = 30;
    int frameCount = 0;
    double durationSeconds = 0;
    int sourceWidth = 1920;
    int sourceHeight = 1080;
    std::string depthConvention = "larger_means_farther";
    float depthMin = 0;
    float depthMax = 1;
    bool fieldAvailable = false;
};

struct PersistentPoint {
    uint64_t stablePointId = 0;
    int bodyRegionId = 0;
    int jointA = 0;
    int jointB = 0;
    float longitudinal = 0.5f;
    float radial = 0.f;
    float fallbackU = 0.5f;
    float fallbackV = 0.5f;
};

inline void assignRegion(int seed, PersistentPoint& p) {
    p.bodyRegionId = 1 + (std::abs(seed) % 6);
    struct Seg { int a, b, r; };
    const Seg segs[] = {
        {0, 5, 1}, {0, 6, 1}, {5, 6, 2}, {5, 11, 2}, {6, 12, 2}, {11, 12, 2},
        {5, 7, 3}, {7, 9, 3}, {6, 8, 4}, {8, 10, 4},
        {11, 13, 5}, {13, 15, 5}, {12, 14, 6}, {14, 16, 6}
    };
    std::vector<Seg> candidates;
    for (auto s : segs)
        if (s.r == p.bodyRegionId) candidates.push_back(s);
    auto chosen = candidates[std::abs(seed) % static_cast<int>(candidates.size())];
    p.jointA = chosen.a;
    p.jointB = chosen.b;
    p.longitudinal = (std::abs(seed) * 17 % 1000) / 999.f;
    p.radial = ((std::abs(seed) * 31 % 1000) / 999.f) * 2.f - 1.f;
    p.fallbackU = (std::abs(seed) * 13 % 1000) / 999.f;
    p.fallbackV = (std::abs(seed) * 19 % 1000) / 999.f;
}

inline void anatomicalTarget(const PlayerObservation& player,
                             const PersistentPoint& p,
                             float minConfidence,
                             float out[3],
                             bool& usedPose) {
    usedPose = false;
    const auto& joints = player.joints;
    if (p.jointA >= 0 && p.jointB >= 0 &&
        p.jointA < static_cast<int>(joints.size()) &&
        p.jointB < static_cast<int>(joints.size()) &&
        joints[p.jointA].c >= minConfidence &&
        joints[p.jointB].c >= minConfidence) {
        const float t = std::max(0.f, std::min(1.f, p.longitudinal));
        out[0] = joints[p.jointA].x + (joints[p.jointB].x - joints[p.jointA].x) * t;
        out[1] = joints[p.jointA].y + (joints[p.jointB].y - joints[p.jointA].y) * t;
        out[2] = joints[p.jointA].z + (joints[p.jointB].z - joints[p.jointA].z) * t;
        float dx = joints[p.jointB].x - joints[p.jointA].x;
        float dy = joints[p.jointB].y - joints[p.jointA].y;
        float nx = -dy, ny = dx;
        float len = std::sqrt(nx * nx + ny * ny);
        if (len > 1e-6f) {
            nx /= len;
            ny /= len;
            out[0] += nx * p.radial * 0.04f;
            out[1] += ny * p.radial * 0.04f;
        }
        usedPose = true;
        return;
    }
    out[0] = player.boundingBox[0] + p.fallbackU * player.boundingBox[2];
    out[1] = player.boundingBox[1] + p.fallbackV * player.boundingBox[3];
    out[2] = 0.45f;
}

inline PlayerObservation interpolatePlayers(const PlayerObservation& a,
                                            const PlayerObservation& b,
                                            float alpha) {
    PlayerObservation o = a;
    o.trackingId = a.trackingId;
    o.observed = a.observed && b.observed;
    auto lerp = [alpha](float x, float y) { return x + (y - x) * alpha; };
    o.confidence = lerp(a.confidence, b.confidence);
    for (int i = 0; i < 2; ++i) {
        o.imageCentroid[i] = lerp(a.imageCentroid[i], b.imageCentroid[i]);
        o.velocity[i] = lerp(a.velocity[i], b.velocity[i]);
        o.acceleration[i] = lerp(a.acceleration[i], b.acceleration[i]);
        o.fieldPosition[i] = lerp(a.fieldPosition[i], b.fieldPosition[i]);
        o.groundContact[i] = lerp(a.groundContact[i], b.groundContact[i]);
    }
    for (int i = 0; i < 4; ++i)
        o.boundingBox[i] = lerp(a.boundingBox[i], b.boundingBox[i]);
    o.joints.resize(std::min(a.joints.size(), b.joints.size()));
    for (size_t i = 0; i < o.joints.size(); ++i) {
        o.joints[i].x = lerp(a.joints[i].x, b.joints[i].x);
        o.joints[i].y = lerp(a.joints[i].y, b.joints[i].y);
        o.joints[i].z = lerp(a.joints[i].z, b.joints[i].z);
        o.joints[i].c = lerp(a.joints[i].c, b.joints[i].c);
    }
    if (!b.observed)
        o = a;
    return o;
}

}  // namespace pdjv
