#include "IVolumetricGenerator.h"

namespace {

void fill(VolumetricRenderState& s, const ScenePreset& src, ScenePreset& dst) {
    dst = src;
    if (!s.wireframe) {
        s.points = s.points || true;
        s.splats = s.splats || true;
    }
}

}  // espacio de nombres

std::vector<std::string> GeneratorRegistry::ids() {
    return {"CyanWireframeMesh", "GlitchPointCloud", "WireframeShell", "ScanVolume",
            "TemporalEcho", "KineticFragmentation", "SkeletonFilaments", "DepthSlices"};
}

void GeneratorRegistry::apply(const std::string& id, int preset, VolumetricRenderState& state, ScenePreset& params) {
    state = VolumetricRenderState{};
    ScenePreset p;
    p.id = id + "-p" + std::to_string(preset);

    if (id == "CyanWireframeMesh") { // Estética de referencia Image 1
        state.wireframe = true;
        state.triangleMesh = true;
        state.points = preset >= 1;
        state.splats = false;
        state.colorPalette = 0; // cian eléctrico
        state.normalLighting = 1.0f;
        state.rimGlow = 1.2f;
        state.groundGrid = true;
        state.wireWidth = 1.8f;
    } else if (id == "GlitchPointCloud") { // Estética de referencia Image 2
        state.wireframe = preset == 1;
        state.triangleMesh = false;
        state.points = true;
        state.splats = true;
        state.glitchFilaments = true;
        state.colorPalette = 1; // glitch monocromático
        state.normalLighting = 0.5f;
        state.rimGlow = 0.8f;
        state.groundGrid = true;
        state.pointScale = 1.4f + preset * 0.4f;
    } else if (id == "WireframeShell") {
        state.wireframe = true;
        state.triangleMesh = preset > 0;
        state.points = preset >= 2;
        state.segments = preset >= 2;
        state.splats = false;
        state.colorPalette = (preset == 0) ? 0 : 2;
        state.groundGrid = true;
    } else if (id == "ScanVolume") {
        p.scanSpeed = 0.2f + preset * 0.12f;
        state.scan = true;
        state.echoes = preset > 0;
        state.wireframe = preset > 1;
        state.colorPalette = 0;
        state.groundGrid = true;
    } else if (id == "TemporalEcho") {
        p.echoSpacing = 0.06f + preset * 0.03f;
        p.historySeconds = 0.6f + preset * 0.3f;
        state.echoes = true;
        state.segments = preset > 0;
        state.wireframe = preset > 1;
        state.colorPalette = 2;
        state.groundGrid = true;
    } else if (id == "KineticFragmentation") {
        p.fragment = 0.25f + preset * 0.25f;
        state.splats = true;
        state.wireframe = preset == 0;
        state.glitchFilaments = true;
        state.colorPalette = 1;
        state.groundGrid = true;
    } else if (id == "SkeletonFilaments") {
        state.skeleton = true;
        state.segments = true;
        state.points = preset != 2;
        state.wireframe = preset == 1;
        state.colorPalette = 0;
        state.groundGrid = true;
    } else { // DepthSlices
        state.depthSlices = true;
        p.echoSpacing = 0.04f + preset * 0.02f;
        state.wireframe = preset > 0;
        state.colorPalette = 0;
        state.groundGrid = true;
    }
    fill(state, p, params);
}
