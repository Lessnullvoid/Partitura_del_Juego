#include "VolumetricComposer.h"
#include "../generators/IVolumetricGenerator.h"

void VolumetricComposer::reset(uint64_t seed) {
    seed_ = seed;
    elapsed_ = 0;
    stageIndex_ = 0;
    currentName_.clear();
}

void VolumetricComposer::setStages(const std::vector<ComposerStage>& stages) {
    stages_ = stages;
    stageIndex_ = 0;
    elapsed_ = 0;
}

void VolumetricComposer::update(double timelineSeconds, float dt) {
    (void)timelineSeconds;
    if (stages_.empty())
        return;
    elapsed_ += dt;
    while (stageIndex_ + 1 < stages_.size() && elapsed_ >= stages_[stageIndex_].durationSeconds) {
        elapsed_ -= stages_[stageIndex_].durationSeconds;
        ++stageIndex_;
    }
    currentName_ = stages_[stageIndex_].generatorId + "-p" + std::to_string(stages_[stageIndex_].preset);
}

void VolumetricComposer::apply(GeneratorState& generator, VolumetricRenderState& render) const {
    if (stages_.empty())
        return;
    const auto& stage = stages_[stageIndex_];
    generator.id = stage.generatorId;
    generator.preset = stage.preset;
    ScenePreset sp;
    GeneratorRegistry::apply(generator.id, generator.preset, render, sp);
    generator.echoSpacing = sp.echoSpacing;
    generator.historySeconds = sp.historySeconds;
}
