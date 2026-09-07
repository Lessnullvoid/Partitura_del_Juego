#pragma once

#include <string>
#include <vector>

struct GeneratorState {
    std::string id = "CyanWireframeMesh";
    int preset = 0;
    float scan = 0.f;
    float echoSpacing = 0.08f;
    float fragment = 0.f;
    float historySeconds = 0.8f;
};

struct ScenePreset {
    std::string id;
    float scanSpeed = 0.25f;
    float echoSpacing = 0.08f;
    float fragment = 0.0f;
    float historySeconds = 0.8f;
};

struct VolumetricRenderState {
    bool wireframe = true;
    bool points = true;
    bool splats = true;
    bool segments = false;
    bool echoes = false;
    bool scan = false;
    bool depthSlices = false;
    bool skeleton = false;

    // Parámetros de estado de renderizado estético ampliados
    bool triangleMesh = true;
    bool groundGrid = true;
    bool glitchFilaments = false;
    int colorPalette = 0; // 0: cian eléctrico, 1: glitch monocromático, 2: cobalto eléctrico, 3: Thermal
    float normalLighting = 1.0f;
    float rimGlow = 1.0f;
    float wireWidth = 1.5f;
    float pointScale = 1.0f;
};

class IVolumetricGenerator {
public:
    virtual ~IVolumetricGenerator() = default;
    virtual std::string id() const = 0;
    virtual std::vector<ScenePreset> presets() const = 0;
    virtual void configure(int preset, VolumetricRenderState& state, ScenePreset& params) const = 0;
};

class GeneratorRegistry {
public:
    static std::vector<std::string> ids();
    static void apply(const std::string& id, int preset, VolumetricRenderState& state, ScenePreset& params);
};
