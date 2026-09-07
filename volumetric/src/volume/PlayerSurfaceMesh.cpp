#include "PlayerSurfaceMesh.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
constexpr int kRingsPerSegment = 6;
constexpr int kSegments = 14;

float regionQuota(pdjv::BodyRegion region) {
    switch (region) {
    case pdjv::BodyRegion::Head: return 0.12f;
    case pdjv::BodyRegion::Torso: return 0.28f;
    case pdjv::BodyRegion::LeftArm:
    case pdjv::BodyRegion::RightArm: return 0.14f;
    case pdjv::BodyRegion::LeftLeg:
    case pdjv::BodyRegion::RightLeg: return 0.16f;
    default: return 0.08f;
    }
}

bool maskAt(const std::vector<uint8_t>& masks, const pdjv::PlayerObservation& obs, int mx, int my) {
    if (mx < 0 || my < 0 || mx >= static_cast<int>(obs.maskWidth) || my >= static_cast<int>(obs.maskHeight))
        return false;
    const size_t idx = obs.maskOffset + static_cast<size_t>(my) * obs.maskWidth + mx;
    return idx < masks.size() && idx < obs.maskOffset + obs.maskSize && masks[idx] >= 32;
}

bool isContour(const std::vector<uint8_t>& masks, const pdjv::PlayerObservation& obs, int mx, int my) {
    if (!maskAt(masks, obs, mx, my))
        return false;
    static const int dx[] = {-1, 1, 0, 0};
    static const int dy[] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
        if (!maskAt(masks, obs, mx + dx[i], my + dy[i]))
            return true;
    }
    return false;
}
} // espacio de nombres

void PlayerSurfaceMesh::addWire(uint32_t a, uint32_t b) {
    if (a == b || a >= vertices_.size() || b >= vertices_.size())
        return;
    wireIndices_.push_back(a);
    wireIndices_.push_back(b);
}

void PlayerSurfaceMesh::addTriangle(uint32_t a, uint32_t b, uint32_t c) {
    if (a >= vertices_.size() || b >= vertices_.size() || c >= vertices_.size())
        return;
    if (a == b || b == c || a == c)
        return;
    triangleIndices_.push_back(a);
    triangleIndices_.push_back(b);
    triangleIndices_.push_back(c);
}

void PlayerSurfaceMesh::buildMaskGridTopology(int gw, int gh, const std::string& packageId) {
    maskGridW_ = gw;
    maskGridH_ = gh;
    const int gridCount = gw * gh;
    const size_t base = vertices_.size();
    vertices_.resize(base + gridCount);
    for (int v = 0; v < gh; ++v) {
        for (int u = 0; u < gw; ++u) {
            const size_t idx = base + static_cast<size_t>(v * gw + u);
            auto& vtx = vertices_[idx];
            vtx.gridU = u;
            vtx.gridV = v;
            vtx.identity.fallbackU = (u + 0.5f) / gw;
            vtx.identity.fallbackV = (v + 0.5f) / gh;
            vtx.identity.angular = static_cast<float>((u * 17 + v * 31) % 628) / 100.f;
            pdjv::assignRegion(static_cast<int>(idx * 19 + trackingId * 7), vtx.identity);
            vtx.identity.stablePointId =
                pdjv::stablePointId(packageId, trackingId, static_cast<int>(idx));
        }
    }
    for (int v = 0; v < gh; ++v) {
        for (int u = 0; u < gw; ++u) {
            const uint32_t i = static_cast<uint32_t>(base + v * gw + u);
            if (u + 1 < gw)
                addWire(i, i + 1);
            if (v + 1 < gh)
                addWire(i, static_cast<uint32_t>(i + gw));

            if (u + 1 < gw && v + 1 < gh) {
                const uint32_t right = i + 1;
                const uint32_t below = static_cast<uint32_t>(i + gw);
                const uint32_t belowRight = static_cast<uint32_t>(i + gw + 1);

                // Añade arista diagonal para una triangulación densa (referencia Image 1)
                addWire(i, belowRight);
                addWire(right, below);

                // Añade triángulos para la malla de superficie sombreada 3D
                addTriangle(i, right, below);
                addTriangle(right, belowRight, below);
            }
        }
    }
}

void PlayerSurfaceMesh::buildPoseShellTopology(int budget, const std::string& packageId) {
    struct Seg {
        int a, b, r;
    };
    const Seg segs[kSegments] = {
        {0, 5, 1}, {0, 6, 1}, {5, 6, 2}, {5, 11, 2}, {6, 12, 2}, {11, 12, 2},
        {5, 7, 3}, {7, 9, 3}, {6, 8, 4}, {8, 10, 4},
        {11, 13, 5}, {13, 15, 5}, {12, 14, 6}, {14, 16, 6}};
    const int perRing = std::max(4, budget / (kSegments * kRingsPerSegment));
    const size_t base = vertices_.size();
    for (int seg = 0; seg < kSegments; ++seg) {
        for (int ring = 0; ring < kRingsPerSegment; ++ring) {
            for (int radial = 0; radial < perRing; ++radial) {
                vertices_.emplace_back();
                auto& vtx = vertices_.back();
                vtx.identity.bodyRegionId = segs[seg].r;
                vtx.identity.jointA = segs[seg].a;
                vtx.identity.jointB = segs[seg].b;
                vtx.identity.longitudinal =
                    static_cast<float>(ring) / std::max(1, kRingsPerSegment - 1);
                vtx.identity.radial =
                    (static_cast<float>(radial) / std::max(1, perRing - 1)) * 2.f - 1.f;
                vtx.identity.angular = static_cast<float>(radial) / std::max(1, perRing - 1) * 6.2831853f;
                vtx.identity.stablePointId = pdjv::stablePointId(
                    packageId, trackingId, static_cast<int>(vertices_.size() - 1));
            }
        }
    }
    for (int seg = 0; seg < kSegments; ++seg) {
        for (int ring = 0; ring < kRingsPerSegment; ++ring) {
            const size_t ringBase = base + static_cast<size_t>(seg * kRingsPerSegment * perRing + ring * perRing);
            for (int radial = 0; radial < perRing; ++radial) {
                const uint32_t i = static_cast<uint32_t>(ringBase + radial);
                const uint32_t next = static_cast<uint32_t>(ringBase + (radial + 1) % perRing);
                addWire(i, next);
                if (ring + 1 < kRingsPerSegment) {
                    const uint32_t below = static_cast<uint32_t>(ringBase + perRing + radial);
                    const uint32_t belowNext = static_cast<uint32_t>(ringBase + perRing + (radial + 1) % perRing);
                    addWire(i, below);
                    addWire(i, belowNext);

                    // Añade triángulos de cáscara
                    addTriangle(i, next, below);
                    addTriangle(next, belowNext, below);
                }
            }
        }
    }
}

void PlayerSurfaceMesh::initialize(int tid, int pointBudget, int maskGridW, int maskGridH,
                                   SurfaceTopology topo, const std::string& packageId) {
    trackingId = tid;
    topology_ = topo;
    vertices_.clear();
    wireIndices_.clear();
    triangleIndices_.clear();
    posed = false;
    envelope = 1.f;
    retiring = false;
    if (topo == SurfaceTopology::MaskGrid || topo == SurfaceTopology::Both)
        buildMaskGridTopology(maskGridW, maskGridH, packageId);
    if (topo == SurfaceTopology::PoseShell || topo == SurfaceTopology::Both) {
        const int shellBudget = (topo == SurfaceTopology::Both) ? pointBudget / 3 : pointBudget;
        buildPoseShellTopology(shellBudget, packageId);
    }
    if (vertices_.empty()) {
        buildMaskGridTopology(32, 64, packageId);
    }
}

bool PlayerSurfaceMesh::sampleSurfaceTarget(const pdjv::PlayerObservation& obs,
                                            const pdjv::PdjvReader* reader,
                                            SurfaceVertex& vtx, float target[3]) const {
    if (!obs.maskWidth || !obs.maskHeight || !obs.maskSize || !reader)
        return false;
    const auto& masks = reader->masks();
    const auto& depths = reader->depths();
    float u = vtx.identity.fallbackU;
    float v = vtx.identity.fallbackV;
    const bool wantsContour = (vtx.identity.stablePointId % 100ull) < 30ull;
    for (int attempt = 0; attempt < 64; ++attempt) {
        const int mx = std::min(static_cast<int>(obs.maskWidth - 1), static_cast<int>(u * obs.maskWidth));
        const int my = std::min(static_cast<int>(obs.maskHeight - 1), static_cast<int>(v * obs.maskHeight));
        const bool onMask = maskAt(masks, obs, mx, my);
        const bool contourOk = !wantsContour || isContour(masks, obs, mx, my);
        if (onMask && contourOk) {
            target[0] = obs.boundingBox[0] + u * obs.boundingBox[2];
            target[1] = obs.boundingBox[1] + v * obs.boundingBox[3];
            target[2] = 0.45f;
            if (obs.depthWidth > 0 && obs.depthHeight > 0) {
                const int dx = std::min(static_cast<int>(obs.depthWidth - 1),
                                        static_cast<int>(u * obs.depthWidth));
                const int dy = std::min(static_cast<int>(obs.depthHeight - 1),
                                        static_cast<int>(v * obs.depthHeight));
                const size_t depthByte = obs.depthOffset +
                    (static_cast<size_t>(dy) * obs.depthWidth + dx) * sizeof(uint16_t);
                if (depthByte + sizeof(uint16_t) <= depths.size() &&
                    depthByte + sizeof(uint16_t) <= static_cast<size_t>(obs.depthOffset) + obs.depthSize) {
                    uint16_t raw = 0;
                    std::memcpy(&raw, depths.data() + depthByte, sizeof(raw));
                    target[2] = raw / 65535.f;
                }
            }
            const float edgeDist = wantsContour ? 0.018f : 0.012f;
            target[2] += std::sin(vtx.identity.angular) * edgeDist;
            return true;
        }
        u = std::fmod(u + 0.61803398875f, 1.f);
        v = std::fmod(v + 0.75487766625f, 1.f);
    }
    return false;
}

void PlayerSurfaceMesh::computeNormals() {
    for (auto& vtx : vertices_) {
        vtx.normal = glm::vec3(0.f);
    }
    for (size_t ti = 0; ti + 2 < triangleIndices_.size(); ti += 3) {
        uint32_t ia = triangleIndices_[ti];
        uint32_t ib = triangleIndices_[ti + 1];
        uint32_t ic = triangleIndices_[ti + 2];
        if (ia >= vertices_.size() || ib >= vertices_.size() || ic >= vertices_.size())
            continue;
        const glm::vec3& pA = vertices_[ia].position;
        const glm::vec3& pB = vertices_[ib].position;
        const glm::vec3& pC = vertices_[ic].position;
        glm::vec3 n = glm::cross(pB - pA, pC - pA);
        if (glm::dot(n, n) > 1e-8f) {
            vertices_[ia].normal += n;
            vertices_[ib].normal += n;
            vertices_[ic].normal += n;
        }
    }
    for (auto& vtx : vertices_) {
        if (glm::dot(vtx.normal, vtx.normal) > 1e-8f) {
            vtx.normal = glm::normalize(vtx.normal);
        } else {
            vtx.normal = glm::vec3(0.f, 0.f, 1.f);
        }
    }
}

void PlayerSurfaceMesh::updateTargets(const pdjv::PlayerObservation& obs,
                                      const pdjv::PdjvReader* reader,
                                      const std::function<glm::vec3(float, float, float)>& worldFromImage,
                                      float dt, const std::string& generatorId, float fragment) {
    for (auto& vtx : vertices_) {
        float target[3] = {};
        bool usedPose = false;
        bool usedSurface = false;
        const bool wantsSurface = vtx.gridU >= 0 || (vtx.identity.stablePointId % 100ull) < 72ull;
        if (wantsSurface)
            usedSurface = sampleSurfaceTarget(obs, reader, vtx, target);
        if (!usedSurface)
            pdjv::anatomicalTarget(obs, vtx.identity, 0.35f, target, usedPose);
        vtx.onSurface = usedSurface;
        const glm::vec3 goal = worldFromImage(target[0], target[1], target[2]);
        glm::vec3& p = vtx.position;
        glm::vec3& vel = vtx.velocity;
        if (!posed) {
            p = goal;
            vel = {0, 0, 0};
            for (int h = 0; h < 5; ++h)
                vtx.previous[h] = goal;
            continue;
        }
        glm::vec3 force = (goal - p) * (4.5f + (1.f - fragment) * 2.2f);
        if (generatorId == "KineticFragmentation" || generatorId == "GlitchPointCloud")
            force += glm::vec3(obs.acceleration[0], 0.f, fragment) * 4.f;
        vel = vel * (1.f - 3.2f * dt) + force * dt;
        for (int h = 4; h > 0; --h)
            vtx.previous[h] = vtx.previous[h - 1];
        vtx.previous[0] = p;
        p += vel * dt;
    }
    posed = true;
    computeNormals();
}

void PlayerSurfaceMesh::applyGeneratorForces(const std::string& generatorId, int preset, float dt,
                                             float& scanOut) {
    if (generatorId == "ScanVolume")
        scanOut = std::fmod(scanOut + dt * (0.25f + preset * 0.1f), 1.f);
    if (generatorId == "DepthSlices" || generatorId == "VoxelQuantization") {
        const float step = 0.04f + preset * 0.02f;
        for (auto& vtx : vertices_)
            vtx.position.z = std::round(vtx.position.z / step) * step;
    }
}

void PlayerSurfaceMesh::bounds(glm::vec3& lo, glm::vec3& hi) const {
    lo = glm::vec3(std::numeric_limits<float>::max());
    hi = glm::vec3(-std::numeric_limits<float>::max());
    for (const auto& vtx : vertices_) {
        lo = glm::min(lo, vtx.position);
        hi = glm::max(hi, vtx.position);
    }
}

void PlayerSurfaceMesh::buildDrawLists(std::vector<glm::vec3>& cloudPos,
                                       std::vector<ofFloatColor>& cloudColors,
                                       ofVboMesh& wires, ofVboMesh& triangles, ofVboMesh& segments,
                                       const VolumetricRenderState& rs, const GeneratorState& gen,
                                       const pdjv::PlayerObservation* poseObs,
                                       const std::function<glm::vec3(float, float, float)>& worldFromImage) const {
    if (envelope <= 0.001f || !posed)
        return;

    const bool isCyanMesh = (gen.id == "CyanWireframeMesh" || rs.colorPalette == 0);
    const bool isGlitchPC = (gen.id == "GlitchPointCloud" || rs.colorPalette == 1);

    // Construye la malla de triángulos sombreada 3D
    if (rs.triangleMesh || isCyanMesh) {
        for (size_t i = 0; i < vertices_.size(); ++i) {
            const auto& vtx = vertices_[i];
            float vis = envelope;
            ofFloatColor baseCol(0.0f, 0.9f, 1.0f, vis * 0.85f);
            if (isGlitchPC) {
                baseCol = ofFloatColor(0.85f, 0.92f, 0.98f, vis * 0.75f);
            }
            triangles.addVertex(vtx.position);
            triangles.addNormal(vtx.normal);
            triangles.addColor(baseCol);
        }
        for (size_t ti = 0; ti + 2 < triangleIndices_.size(); ti += 3) {
            uint32_t i1 = triangleIndices_[ti];
            uint32_t i2 = triangleIndices_[ti + 1];
            uint32_t i3 = triangleIndices_[ti + 2];
            const auto& v1 = vertices_[i1];
            const auto& v2 = vertices_[i2];
            const auto& v3 = vertices_[i3];
            if (!v1.onSurface || !v2.onSurface || !v3.onSurface) continue;
            if (std::abs(v1.position.z - v2.position.z) > 0.25f ||
                std::abs(v2.position.z - v3.position.z) > 0.25f ||
                std::abs(v3.position.z - v1.position.z) > 0.25f) continue;
            if (glm::distance(v1.position, v2.position) > 0.5f ||
                glm::distance(v2.position, v3.position) > 0.5f ||
                glm::distance(v3.position, v1.position) > 0.5f) continue;
            triangles.addIndex(i1);
            triangles.addIndex(i2);
            triangles.addIndex(i3);
        }
    }

    // Construye la lista de sprites de nube de puntos. Se añade a vectores CPU preasignados para que
    // el llamante suba con glBufferSubData sin reasignar el búfer GPU.
    const auto addPoint = [&](const glm::vec3& p, const ofFloatColor& color) {
        if (!rs.points && !rs.splats)
            return;
        cloudPos.push_back(p);
        cloudColors.push_back(color);
    };

    for (size_t i = 0; i < vertices_.size(); ++i) {
        const auto& vtx = vertices_[i];
        if (!vtx.onSurface) continue;
        const auto& p = vtx.position;
        float vis = envelope;
        if (gen.id == "ScanVolume") {
            const float plane = gen.scan * 3.2f - 1.6f;
            vis *= ofClamp(1.f - std::abs(p.x - plane) * 3.4f, 0.f, 1.f);
        }
        if (gen.id == "DepthSlices") {
            vis *= (std::fmod(std::abs(p.z) + gen.preset * 0.05f, 0.2f) < 0.07f) ? 1.f : 0.2f;
        }

        const float depthTone = ofClamp((p.z + 0.35f) / 0.7f, 0.f, 1.f);
        ofFloatColor color;
        if (isCyanMesh) {
            color = ofFloatColor(0.0f, 0.95f, 1.0f, vis * 0.9f);
        } else if (isGlitchPC) {
            color = ofFloatColor(0.9f, 0.95f, 1.0f, vis * 0.85f);
        } else {
            color = ofFloatColor(0.68f + depthTone * 0.32f, 0.82f + depthTone * 0.18f, 1.f, vis * 0.72f);
        }

        if (gen.id == "SkeletonFilaments")
            color = ofFloatColor(1, 0.92, 0.75, vis * 0.85f);
        if (rs.wireframe && rs.points)
            color.a *= 0.25f;

        addPoint(p, color);

        if ((gen.id == "TemporalEcho" || isGlitchPC) && rs.points) {
            for (int e = 0; e < 5; ++e) {
                glm::vec3 q = vtx.previous[e];
                if (glm::dot(q - p, q - p) < 1e-6f)
                    continue;
                q.z -= (e + 1) * gen.echoSpacing;
                addPoint(q, ofFloatColor(color.r, color.g, color.b, vis * (0.28f - e * 0.04f)));
            }
        }
    }

    // Construye aristas de wireframe
    if (rs.wireframe || isCyanMesh) {
        for (size_t wi = 0; wi + 1 < wireIndices_.size(); wi += 2) {
            const auto& vtxA = vertices_[wireIndices_[wi]];
            const auto& vtxB = vertices_[wireIndices_[wi + 1]];
            if (!vtxA.onSurface || !vtxB.onSurface) continue;
            if (std::abs(vtxA.position.z - vtxB.position.z) > 0.25f) continue;
            if (glm::distance(vtxA.position, vtxB.position) > 0.5f) continue;
            const auto& a = vtxA.position;
            const auto& b = vtxB.position;
            float vis = envelope;
            if (gen.id == "ScanVolume") {
                const float plane = gen.scan * 3.2f - 1.6f;
                const float mid = 0.5f * (a.x + b.x);
                vis *= ofClamp(1.f - std::abs(mid - plane) * 3.4f, 0.f, 1.f);
            }

            ofFloatColor c;
            if (isCyanMesh) {
                c = ofFloatColor(0.0f, 1.0f, 0.92f, vis * 0.95f);
            } else if (isGlitchPC) {
                c = ofFloatColor(0.85f, 0.9f, 0.98f, vis * 0.75f);
            } else {
                c = ofFloatColor(0.55f, 0.95f, 1.f, vis * 0.88f);
            }

            wires.addVertex(a);
            wires.addNormal(vtxA.normal);
            wires.addColor(c);
            wires.addVertex(b);
            wires.addNormal(vtxB.normal);
            wires.addColor(ofFloatColor(c.r, c.g, c.b, vis * 0.55f));
        }
    }

    // Construye filamentos de proyección glitch (estilo Image 2)
    if (rs.glitchFilaments || isGlitchPC) {
        for (size_t i = 0; i < vertices_.size(); i += 18) {
            const auto& vtx = vertices_[i];
            if (glm::length(vtx.velocity) > 0.4f || (vtx.identity.stablePointId % 30ull == 0)) {
                glm::vec3 groundP = vtx.position;
                groundP.y = -1.2f; // Línea de proyección al suelo
                segments.addVertex(vtx.position);
                segments.addNormal(vtx.normal);
                segments.addColor(ofFloatColor(0.85f, 0.92f, 1.0f, envelope * 0.65f));
                segments.addVertex(groundP);
                segments.addNormal(glm::vec3(0, 1, 0));
                segments.addColor(ofFloatColor(0.0f, 0.75f, 1.0f, 0.05f));
            }
        }
    }

    // Construye filamentos óseos del esqueleto
    if (rs.segments && (gen.id == "SkeletonFilaments" || poseObs) && poseObs && poseObs->joints.size() >= 17) {
        const int bones[][2] = {{0, 5}, {0, 6}, {5, 6}, {5, 7}, {7, 9}, {6, 8}, {8, 10},
                                {5, 11}, {6, 12}, {11, 12}, {11, 13}, {13, 15}, {12, 14}, {14, 16}};
        for (auto b : bones) {
            const auto ja = worldFromImage(poseObs->joints[b[0]].x, poseObs->joints[b[0]].y,
                                           poseObs->joints[b[0]].z);
            const auto jb = worldFromImage(poseObs->joints[b[1]].x, poseObs->joints[b[1]].y,
                                           poseObs->joints[b[1]].z);
            segments.addVertex(ja);
            segments.addNormal(glm::vec3(0, 0, 1));
            segments.addVertex(jb);
            segments.addNormal(glm::vec3(0, 0, 1));
            ofFloatColor col = isCyanMesh ? ofFloatColor(0.0f, 1.0f, 0.85f, envelope * 0.9f)
                                          : ofFloatColor(1, 1, 1, envelope * 0.9f);
            segments.addColor(col);
            segments.addColor(ofFloatColor(col.r, col.g, col.b, envelope * 0.45f));
        }
    }
}
