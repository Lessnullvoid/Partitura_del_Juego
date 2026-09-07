#pragma once

#include "../generators/IVolumetricGenerator.h"
#include <string>
#include <vector>

struct ComposerStage {
    std::string generatorId;
    int preset = 0;
    float durationSeconds = 30.f;
};

class VolumetricComposer {
public:
    void reset(uint64_t seed);
    void setStages(const std::vector<ComposerStage>& stages);
    void update(double timelineSeconds, float dt);
    void apply(GeneratorState& generator, VolumetricRenderState& render) const;
    const std::string& currentStageName() const { return currentName_; }

private:
    std::vector<ComposerStage> stages_;
    double elapsed_ = 0;
    size_t stageIndex_ = 0;
    std::string currentName_;
    uint64_t seed_ = 1;
};
