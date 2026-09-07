#pragma once

#include "../data/PdjvCommon.h"
#include "../data/PdjvRuntime.h"
#include "../generators/IVolumetricGenerator.h"
#include "ofMain.h"
#include <functional>
#include <vector>

enum class SurfaceTopology { PoseShell, MaskGrid, Both };

enum class CameraMode { FullBody, PortraitBust };

struct SurfaceVertex {
    pdjv::PersistentPoint identity;
    glm::vec3 position{0};
    glm::vec3 normal{0, 0, 1};
    glm::vec3 velocity{0};
    glm::vec3 previous[5];
    int gridU = -1;
    int gridV = -1;
    bool onSurface = false;
};

struct PlayerSurfaceMesh {
    int trackingId = -1;
    double lastSeen = 0;
    float envelope = 1.f;
    bool posed = false;
    bool retiring = false;

    void initialize(int tid, int pointBudget, int maskGridW, int maskGridH,
                    SurfaceTopology topo, const std::string& packageId);
    void updateTargets(const pdjv::PlayerObservation& obs,
                       const pdjv::PdjvReader* reader,
                       const std::function<glm::vec3(float, float, float)>& worldFromImage,
                       float dt, const std::string& generatorId, float fragment);
    void applyGeneratorForces(const std::string& generatorId, int preset, float dt, float& scanOut);
    // Los puntos (nube) se escriben en vectores CPU preasignados para evitar
    // la reasignación del búfer GPU. Wires/triangles/segments siguen siendo ofVboMesh.
    void buildDrawLists(std::vector<glm::vec3>& cloudPos,
                        std::vector<ofFloatColor>& cloudColors,
                        ofVboMesh& wires, ofVboMesh& triangles, ofVboMesh& segments,
                        const VolumetricRenderState& rs, const GeneratorState& gen,
                        const pdjv::PlayerObservation* poseObs,
                        const std::function<glm::vec3(float, float, float)>& worldFromImage) const;
    void bounds(glm::vec3& lo, glm::vec3& hi) const;
    size_t wireIndexCount() const { return wireIndices_.size(); }
    size_t triangleIndexCount() const { return triangleIndices_.size(); }
    size_t vertexCount() const { return vertices_.size(); }
    glm::vec3 positionAt(size_t i) const { return i < vertices_.size() ? vertices_[i].position : glm::vec3{0}; }
    glm::vec3 previousAt(size_t i, int historyIndex) const {
        return i < vertices_.size() ? vertices_[i].previous[historyIndex] : glm::vec3{0};
    }

private:
    std::vector<SurfaceVertex> vertices_;
    std::vector<uint32_t> wireIndices_;
    std::vector<uint32_t> triangleIndices_;
    SurfaceTopology topology_ = SurfaceTopology::Both;
    int maskGridW_ = 32;
    int maskGridH_ = 64;

    void buildMaskGridTopology(int gw, int gh, const std::string& packageId);
    void buildPoseShellTopology(int budget, const std::string& packageId);
    void addWire(uint32_t a, uint32_t b);
    void addTriangle(uint32_t a, uint32_t b, uint32_t c);
    bool sampleSurfaceTarget(const pdjv::PlayerObservation& obs, const pdjv::PdjvReader* reader,
                             SurfaceVertex& vtx, float target[3]) const;
    void computeNormals();
};
