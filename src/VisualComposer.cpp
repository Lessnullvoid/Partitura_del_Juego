#include "VisualComposer.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace {
constexpr int kContentTypeCount = 4;
constexpr int kOrganizationCount = 4;

const char* collectiveMovementName(CollectiveMovement movement) {
    switch (movement) {
        case CollectiveMovement::Suspension:    return "suspension";
        case CollectiveMovement::Codification: return "codification";
        case CollectiveMovement::Accumulation: return "accumulation";
        case CollectiveMovement::Propagation:  return "propagation";
        case CollectiveMovement::Convergence:  return "convergence";
        case CollectiveMovement::Fragmentation:return "fragmentation";
        case CollectiveMovement::Saturation:   return "saturation";
        case CollectiveMovement::Rupture:      return "rupture";
        case CollectiveMovement::Residue:      return "residue";
    }
    return "codification";
}

int contentIndex(ContentType content) {
    return static_cast<int>(content);
}
} // namespace

void VisualComposer::setup(const VisualComposerParams& params, int channelCount) {
    params_ = params;
    const int count = std::max(1, channelCount);
    states_.assign(static_cast<std::size_t>(count), ChapterState{});
    histories_.assign(static_cast<std::size_t>(count), {});
    pendingGenerators_.assign(static_cast<std::size_t>(count), {});
    videoFinished_.assign(static_cast<std::size_t>(count), false);
    observations_.assign(static_cast<std::size_t>(count), {});
    // Semilla 0 significa "sacar una nueva", para que la instalación no
    // reproduzca la misma velada en cada arranque. Cualquier semilla distinta de cero queda exactamente repetible.
    if (params_.seed == 0u) {
        std::random_device device;
        params_.seed = device() | 1u;
    }
    rng_.seed(params_.seed);

    organization_ = OrganizationMode::Counterpoint;
    beatPosition_ = 0.f;
    beatIndex_ = 0;
    subdivisionIndex_ = 0;
    subdivisionPulse_ = false;
    pendingOrganization_ = false;
    pendingShared_ = false;
    lastTime_ = -1.f;
    lastFrame_ = std::numeric_limits<std::uint64_t>::max();
    revision_ = 0;
    wasEnabled_ = false;
    wasProgramEnabled_ = false;
    moment_ = InstallationMoment::PulseSystem;
    momentElapsed_ = 0.f;
    takeoverActive_ = false;
    takeoverDuration_ = 0.f;
    nextTakeoverAt_ = 0.f;
    collective_ = {};
    collectiveCandidate_ = CollectiveMovement::Codification;
    collectiveCandidateElapsed_ = 0.f;
    collectiveStateElapsed_ = 0.f;
    collectiveEventCooldownRemaining_ = 0.f;
    previousCollectiveActivity_ = 0.f;
    handledCollectiveRevision_ = 0;

    for (int channel = 0; channel < count; ++channel) {
        ChapterState& state = states_[static_cast<std::size_t>(channel)];
        state.channel = channel;
        state.targetChannel = channel;
        state.targetGroup = groupFor(channel);
        state.organization = organization_;
    }
    if (params_.enabled && params_.programEnabled) {
        wasEnabled_ = true;
        wasProgramEnabled_ = true;
        beginMoment(chooseOpeningMoment());
    }
}

void VisualComposer::update(float globalSpeed) {
    const std::uint64_t frame = static_cast<std::uint64_t>(ofGetFrameNum());
    if (frame == lastFrame_) {
        return;
    }
    lastFrame_ = frame;

    const float now = ofGetElapsedTimef();
    float dt = 0.f;
    if (lastTime_ >= 0.f) {
        dt = std::max(0.f, now - lastTime_) * std::max(0.f, globalSpeed);
    }
    lastTime_ = now;

    if (!params_.enabled) {
        wasEnabled_ = false;
        subdivisionPulse_ = false;
        for (ChapterState& state : states_) {
            state.subdivisionPulse = false;
        }
        return;
    }

    if (!wasEnabled_) {
        wasEnabled_ = true;
        if (params_.programEnabled) {
            beginMoment(chooseOpeningMoment());
            wasProgramEnabled_ = true;
        } else {
            applyOrganization(chooseOrganization(), true);
        }
    }
    if (params_.programEnabled && !wasProgramEnabled_)
        beginMoment(chooseOpeningMoment());
    wasProgramEnabled_ = params_.programEnabled;

    if (dt > 0.f) {
        momentElapsed_ += dt;

        // Avanzar la cuenta atrás del overlay de inversión de color del generador.
        if (generatorInvertActive_) {
            generatorInvertRemaining_ -= dt;
            if (generatorInvertRemaining_ <= 0.f) {
                generatorInvertActive_ = false;
                generatorInvertRemaining_ = 0.f;
            }
        }

        // Avanzar la cuenta atrás de inversión de polaridad de canal completo.
        if (polarityInvertActive_) {
            polarityInvertRemaining_ -= dt;
            if (polarityInvertRemaining_ <= 0.f) {
                polarityInvertActive_ = false;
                polarityInvertRemaining_ = 0.f;
            }
        }

        const float beatsPerSecond = std::max(0.f, params_.bpm) / 60.f;
        const int subdivisions = std::max(1, params_.beatSubdivision);
        const float previousSubBeat = beatPosition_ * static_cast<float>(subdivisions);
        beatPosition_ += dt * beatsPerSecond;
        const float currentSubBeat = beatPosition_ * static_cast<float>(subdivisions);
        subdivisionPulse_ = static_cast<std::uint64_t>(std::floor(currentSubBeat)) !=
                            static_cast<std::uint64_t>(std::floor(previousSubBeat));
        beatIndex_ = static_cast<std::uint64_t>(std::floor(beatPosition_));
        subdivisionIndex_ = static_cast<int>(std::floor(currentSubBeat)) % subdivisions;
    } else {
        subdivisionPulse_ = false;
    }

    if (pendingOrganization_) {
        bool allReady = true;
        for (int channel = 0; channel < channelCount(); ++channel) {
            allReady = allReady && protectedDwellMet(channel);
        }
        if (allReady) {
            pendingOrganization_ = false;
            applyOrganization(requestedOrganization_, false);
        }
    }

    if (pendingShared_) {
        bool allReady = pendingSharedForced_;
        if (!allReady) {
            allReady = true;
            for (int channel = 0; channel < channelCount(); ++channel) {
                allReady = allReady && protectedDwellMet(channel);
            }
        }
        if (allReady) {
            const ContentType content = requestedSharedContent_;
            int generator = requestedSharedGenerator_;
            if (content == ContentType::Generator && generator < 0) {
                generator = chooseGenerator(0);
            }
            generator = std::max(0, generator);
            pendingShared_ = false;
            for (int channel = 0; channel < channelCount(); ++channel) {
                beginChapter(channel, content, generator, TargetScope::Shared, -1,
                             pendingSharedForced_);
            }
        }
    }

    for (ChapterState& state : states_) {
        state.elapsed += dt;
        state.chapterPhase = state.duration > 0.f
                                 ? clamp01(state.elapsed / state.duration)
                                 : 0.f;
        state.beatPhase = beatPosition_ - std::floor(beatPosition_);
        state.beatIndex = beatIndex_;
        state.subdivisionIndex = subdivisionIndex_;
        state.subdivisionPulse = subdivisionPulse_;
        updateStage(state);
    }

    updateCollectiveAnalysis(dt);

    if (params_.programEnabled) {
        if (moment_ == InstallationMoment::PulseSystem) {
            const float duration = std::max(0.1f, params_.pulseMomentDuration);
            const bool dataRelease = momentElapsed_ >= duration * 0.45f &&
                (collective_.movement == CollectiveMovement::Fragmentation ||
                 collective_.movement == CollectiveMovement::Saturation ||
                 collective_.movement == CollectiveMovement::Rupture);
            if (momentElapsed_ >= duration || dataRelease)
                beginMoment(momentAfterSystem(moment_));
            return;
        }
        if (moment_ == InstallationMoment::BarScanSystem) {
            const float duration = std::max(0.1f, params_.barScanMomentDuration);
            const bool dataRelease = momentElapsed_ >= duration * 0.45f &&
                (collective_.movement == CollectiveMovement::Suspension ||
                 collective_.movement == CollectiveMovement::Propagation ||
                 collective_.movement == CollectiveMovement::Rupture);
            if (momentElapsed_ >= duration || dataRelease)
                beginMoment(momentAfterSystem(moment_));
            return;
        }

        // Un estado colectivo recién estabilizado puede reorganizar la instalación
        // o disparar un evento a muro completo. El cooldown evita bucles de feedback:
        // el takeover cambia la convergencia, pero no puede redispararse a sí mismo.
        if (moment_ == InstallationMoment::Intercalation &&
            !takeoverActive_ &&
            collective_.revision != handledCollectiveRevision_) {
            handledCollectiveRevision_ = collective_.revision;
            if (collective_.movement == CollectiveMovement::Propagation) {
                applyCollectiveOrganization(OrganizationMode::Propagation);
            } else if (collective_.movement == CollectiveMovement::Fragmentation) {
                applyCollectiveOrganization(OrganizationMode::Counterpoint);
            } else if (collective_.movement == CollectiveMovement::Accumulation) {
                applyCollectiveOrganization(OrganizationMode::Group4Plus4);
            } else if (collectiveEventCooldownRemaining_ <= 0.f &&
                       collective_.movement == CollectiveMovement::Saturation &&
                       randomUnit() < ofClamp(params_.collectiveResponse, 0.f, 1.f)) {
                collectiveEventCooldownRemaining_ =
                    std::max(1.f, params_.collectiveEventCooldown);
                forceNoiseMoment();
                return;
            } else if (collectiveEventCooldownRemaining_ <= 0.f &&
                       collective_.movement == CollectiveMovement::Rupture) {
                collectiveEventCooldownRemaining_ =
                    std::max(1.f, params_.collectiveEventCooldown);
                const int strobe = (collective_.revision & 1u)
                    ? static_cast<int>(GeneratorMode::Strobe)
                    : static_cast<int>(GeneratorMode::DividedStrobe);
                forceTakeover(strobe);
                return;
            }
        }
        // Evento periódico de sincronización de ruido a muro completo (solo Intercalation).
        if (moment_ == InstallationMoment::Intercalation
            && !takeoverActive_
            && params_.noiseEventEnabled
            && momentElapsed_ >= nextNoiseAt_) {
            forceNoiseMoment();
            return;
        }

        // Overlay de inversión de color del generador: puramente visual, no cambia el contenido.
        // Permitido durante takeovers (los generadores siguen dibujando) pero no durante
        // momentos de sistema (Pulse / BarScan), que ya controlan el muro entero.
        if (moment_ == InstallationMoment::Intercalation
            && !generatorInvertActive_
            && params_.generatorInvertEnabled
            && momentElapsed_ >= nextGeneratorInvertAt_) {
            forceGeneratorInvertMoment();
            // Sin return: las actualizaciones de capítulo siguen igual — solo
            // cambió el estado del overlay visual.
        }

        // Inversión de polaridad de canal completo: invierte generadores Y nubes
        // de puntos de vídeo a la vez. Intervalo independiente de la inversión
        // solo de generador. Los dos eventos pueden solaparse; Channel gestiona ambos.
        if (moment_ == InstallationMoment::Intercalation
            && !polarityInvertActive_
            && params_.polarityInvertEnabled
            && momentElapsed_ >= nextPolarityInvertAt_) {
            forcePolarityInvertMoment();
        }

        if (takeoverActive_) {
            if (!states_.empty() &&
                states_.front().elapsed >= takeoverDuration_)
                endTakeover();
            return;
        }
        if (momentElapsed_ >= nextTakeoverAt_) {
            forceTakeover();
            return;
        }
    }

    if (pendingShared_) {
        return;
    }

    for (int channel = 0; channel < channelCount(); ++channel) {
        const PendingGenerator& pending =
            pendingGenerators_[static_cast<std::size_t>(channel)];
        if (pending.value >= 0 && protectedDwellMet(channel)) {
            advanceChannel(channel);
        }
    }

    if (organization_ == OrganizationMode::Unison) {
        bool allReady = !states_.empty();
        for (int channel = 0; channel < channelCount(); ++channel) {
            allReady = allReady && readyToAdvance(channel);
        }
        if (allReady) {
            const ContentType content = chooseContent(0);
            const int generator = chooseGenerator(0);
            for (int channel = 0; channel < channelCount(); ++channel) {
                beginChapter(channel, content, generator, TargetScope::Shared, -1, false);
            }
        }
    } else if (organization_ == OrganizationMode::Group4Plus4) {
        for (int group = 0; group < 2; ++group) {
            const int first = group * 4;
            if (!validChannel(first)) {
                continue;
            }
            const int end = std::min(first + 4, channelCount());
            bool allReady = true;
            for (int channel = first; channel < end; ++channel) {
                allReady = allReady && readyToAdvance(channel);
            }
            if (!allReady) {
                continue;
            }
            const ContentType content = chooseContent(first);
            const int generator = chooseGenerator(first);
            for (int channel = first; channel < end; ++channel) {
                beginChapter(channel, content, generator, TargetScope::Group, group, false);
            }
        }
    } else {
        for (int channel = 0; channel < channelCount(); ++channel) {
            if (readyToAdvance(channel)) {
                advanceChannel(channel);
            }
        }
    }
}

const ChapterState& VisualComposer::stateFor(int channel) const {
    static const ChapterState invalid;
    return validChannel(channel) ? states_[static_cast<std::size_t>(channel)] : invalid;
}

void VisualComposer::observeChannel(int channel, float motionEnergy,
                                   float flowMagnitude, float flowAngle,
                                   float crowdDensity, int blobCount,
                                   bool collision) {
    if (!validChannel(channel))
        return;
    CollectiveObservation& observation =
        observations_[static_cast<std::size_t>(channel)];
    observation.motion = clamp01(motionEnergy * 6.f);
    observation.flow = clamp01(flowMagnitude / 4.f);
    observation.angle = std::isfinite(flowAngle) ? flowAngle : 0.f;
    observation.crowd = clamp01(crowdDensity);
    observation.population = clamp01(static_cast<float>(blobCount) / 8.f);
    observation.collisionPulse = collision && !observation.collisionInput;
    observation.collisionInput = collision;
}

void VisualComposer::updateCollectiveAnalysis(float dt) {
    if (!params_.collectiveLogicEnabled || observations_.empty()) {
        collective_.phase = clamp01(collectiveStateElapsed_ /
                                    std::max(0.1f, params_.collectiveMinimumDwell));
        return;
    }

    collectiveStateElapsed_ += dt;
    collectiveEventCooldownRemaining_ =
        std::max(0.f, collectiveEventCooldownRemaining_ - dt);

    float rawActivity = 0.f;
    float rawPopulation = 0.f;
    float rawCrowd = 0.f;
    float flowWeight = 0.f;
    float flowX = 0.f;
    float flowY = 0.f;
    float collisionCount = 0.f;
    std::vector<float> channelActivities;
    channelActivities.reserve(observations_.size());
    for (CollectiveObservation& observation : observations_) {
        const float channelActivity = clamp01(
            observation.motion * 0.56f + observation.flow * 0.20f +
            observation.crowd * 0.14f + observation.population * 0.10f);
        channelActivities.push_back(channelActivity);
        rawActivity += channelActivity;
        rawPopulation += observation.population;
        rawCrowd += observation.crowd;
        flowWeight += observation.flow;
        flowX += std::cos(observation.angle) * observation.flow;
        flowY += std::sin(observation.angle) * observation.flow;
        if (observation.collisionPulse)
            collisionCount += 1.f;
        observation.collisionPulse = false;
    }
    const float count = static_cast<float>(observations_.size());
    rawActivity /= count;
    rawPopulation /= count;
    rawCrowd /= count;
    const float rawCoherence = flowWeight > 0.001f
        ? clamp01(std::sqrt(flowX * flowX + flowY * flowY) / flowWeight)
        : 0.f;

    float contrast = 0.f;
    for (float value : channelActivities)
        contrast += std::abs(value - rawActivity);
    contrast = clamp01((contrast / count) * 2.4f);

    std::vector<int> materialCounts(
        static_cast<std::size_t>(std::max(1, params_.generatorCount) + 3), 0);
    std::vector<int> generatorCounts(
        static_cast<std::size_t>(std::max(1, params_.generatorCount)), 0);
    for (const ChapterState& state : states_) {
        if (state.content == ContentType::Generator ||
            state.content == ContentType::Transition) {
            const int generator = ofClamp(
                state.generator, 0, std::max(1, params_.generatorCount) - 1);
            ++generatorCounts[static_cast<std::size_t>(generator)];
            ++materialCounts[static_cast<std::size_t>(generator)];
        } else {
            const int offset = std::max(1, params_.generatorCount) +
                (state.content == ContentType::Video ? 0 :
                 state.content == ContentType::Breath ? 1 : 2);
            ++materialCounts[static_cast<std::size_t>(offset)];
        }
    }

    int dominantGenerator = -1;
    int dominantGeneratorCount = 0;
    for (int generator = 0;
         generator < static_cast<int>(generatorCounts.size()); ++generator) {
        if (generatorCounts[static_cast<std::size_t>(generator)] >
            dominantGeneratorCount) {
            dominantGeneratorCount =
                generatorCounts[static_cast<std::size_t>(generator)];
            dominantGenerator = generator;
        }
    }
    int dominantMaterialCount = 0;
    for (int value : materialCounts)
        dominantMaterialCount = std::max(dominantMaterialCount, value);
    const float convergence = states_.empty() ? 0.f :
        static_cast<float>(dominantGeneratorCount) /
        static_cast<float>(states_.size());
    const float diversity = states_.size() <= 1 ? 0.f : clamp01(
        (1.f - static_cast<float>(dominantMaterialCount) /
                   static_cast<float>(states_.size())) * 1.35f);

    const float smoothing = dt > 0.f ? 1.f - std::exp(-dt / 1.6f) : 0.f;
    previousCollectiveActivity_ = collective_.activity;
    collective_.activity += (rawActivity - collective_.activity) * smoothing;
    collective_.population +=
        (rawPopulation - collective_.population) * smoothing;
    collective_.coherence += (rawCoherence - collective_.coherence) * smoothing;
    collective_.diversity += (diversity - collective_.diversity) * smoothing;
    collective_.convergence = convergence;
    collective_.dominantGenerator = dominantGenerator;
    collective_.tension = clamp01(
        collective_.activity * 0.48f + rawCrowd * 0.20f +
        collective_.population * 0.12f + contrast * 0.12f +
        collective_.coherence * collective_.activity * 0.08f);

    const float rise = dt > 0.0001f
        ? (collective_.activity - previousCollectiveActivity_) / dt : 0.f;
    CollectiveMovement candidate = CollectiveMovement::Codification;
    const bool fullWallGenerator = convergence >= 0.999f;
    if (fullWallGenerator &&
        (dominantGenerator == static_cast<int>(GeneratorMode::Strobe) ||
         dominantGenerator == static_cast<int>(GeneratorMode::DividedStrobe))) {
        candidate = CollectiveMovement::Rupture;
    } else if (fullWallGenerator &&
               dominantGenerator == static_cast<int>(GeneratorMode::AnalogNoise)) {
        candidate = CollectiveMovement::Saturation;
    } else if (fullWallGenerator) {
        candidate = CollectiveMovement::Convergence;
    } else if ((collisionCount / count) >= 0.125f ||
               (collective_.activity > 0.86f && contrast > 0.16f)) {
        candidate = CollectiveMovement::Rupture;
    } else if (collective_.activity > 0.62f || rawCrowd > 0.56f) {
        candidate = CollectiveMovement::Saturation;
    } else if (collective_.activity < 0.07f &&
               collective_.population < 0.06f) {
        candidate = CollectiveMovement::Suspension;
    } else if (collective_.coherence > 0.68f && flowWeight / count > 0.08f) {
        candidate = CollectiveMovement::Propagation;
    } else if (collective_.diversity > 0.58f || contrast > 0.18f) {
        candidate = CollectiveMovement::Fragmentation;
    } else if (rise > 0.025f) {
        candidate = CollectiveMovement::Accumulation;
    }

    const float minimumDwell = std::max(0.1f, params_.collectiveMinimumDwell);
    if (collective_.movement == CollectiveMovement::Rupture &&
        collectiveStateElapsed_ >= 2.4f) {
        candidate = CollectiveMovement::Residue;
    } else if (collective_.movement == CollectiveMovement::Residue &&
               collectiveStateElapsed_ < minimumDwell * 1.25f) {
        candidate = CollectiveMovement::Residue;
    } else if (collective_.movement == CollectiveMovement::Saturation &&
               collectiveStateElapsed_ >= minimumDwell &&
               candidate != CollectiveMovement::Saturation &&
               candidate != CollectiveMovement::Rupture) {
        candidate = CollectiveMovement::Rupture;
    } else if (collective_.movement == CollectiveMovement::Convergence &&
               collectiveStateElapsed_ >= minimumDwell &&
               candidate != CollectiveMovement::Convergence &&
               candidate != CollectiveMovement::Saturation &&
               candidate != CollectiveMovement::Rupture) {
        candidate = CollectiveMovement::Residue;
    }

    if (candidate == collectiveCandidate_) {
        collectiveCandidateElapsed_ += dt;
    } else {
        collectiveCandidate_ = candidate;
        collectiveCandidateElapsed_ = 0.f;
    }

    const bool urgent = candidate == CollectiveMovement::Rupture ||
        candidate == CollectiveMovement::Saturation ||
        (candidate == CollectiveMovement::Convergence && fullWallGenerator) ||
        candidate == CollectiveMovement::Residue;
    const float decisionHold = urgent ? 0.18f :
        std::max(0.1f, params_.collectiveDecisionHold);
    if (candidate != collective_.movement &&
        collectiveCandidateElapsed_ >= decisionHold &&
        (collectiveStateElapsed_ >= minimumDwell || urgent)) {
        collective_.movement = candidate;
        collective_.revision += 1;
        collectiveStateElapsed_ = 0.f;
        ofLogNotice("VisualComposer")
            << "Collective movement -> "
            << collectiveMovementName(candidate)
            << " activity=" << collective_.activity
            << " coherence=" << collective_.coherence
            << " diversity=" << collective_.diversity
            << " convergence=" << collective_.convergence;
    }
    collective_.phase = clamp01(collectiveStateElapsed_ / minimumDwell);
}

void VisualComposer::applyCollectiveOrganization(OrganizationMode mode) {
    organization_ = mode;
    for (ChapterState& state : states_)
        state.organization = mode;
}

int VisualComposer::activeVideoCount(int group) const {
    int count = 0;
    for (const ChapterState& state : states_) {
        if (groupFor(state.channel) == group &&
            state.content == ContentType::Video)
            ++count;
    }
    return count;
}

void VisualComposer::forceNextMoment() {
    if (!params_.programEnabled)
        return;
    if (moment_ == InstallationMoment::PulseSystem)
        beginMoment(InstallationMoment::BarScanSystem);
    else if (moment_ == InstallationMoment::BarScanSystem)
        beginMoment(InstallationMoment::Intercalation);
    else
        forceTakeover();
}

void VisualComposer::forceTakeover(int generator) {
    if (!params_.programEnabled ||
        moment_ != InstallationMoment::Intercalation)
        return;
    if (generator < 0) {
        const std::vector<int> available = systemGenerators();
        const std::size_t index = static_cast<std::size_t>(
            randomUnit() * static_cast<float>(available.size()));
        generator = available[std::min(index, available.size() - 1)];
    }
    takeoverActive_ = true;
    takeoverDuration_ = params_.takeoverDurationMin +
        (std::max(params_.takeoverDurationMin, params_.takeoverDurationMax) -
         params_.takeoverDurationMin) * randomUnit();
    organization_ = OrganizationMode::Unison;
    for (int channel = 0; channel < channelCount(); ++channel) {
        beginChapter(channel, ContentType::Generator, generator,
                     TargetScope::Shared, -1, true);
        states_[static_cast<std::size_t>(channel)].duration =
            takeoverDuration_;
        states_[static_cast<std::size_t>(channel)].minimumDwell =
            takeoverDuration_;
    }
}

void VisualComposer::notifyVideoFinished(int channel) {
    if (!validChannel(channel)) {
        return;
    }
    ChapterState& state = states_[static_cast<std::size_t>(channel)];
    if (state.content == ContentType::Video) {
        state.videoPlaying = false;
        videoFinished_[static_cast<std::size_t>(channel)] = true;
    }
}

bool VisualComposer::shouldRequestVideo(int channel) const {
    if (!params_.enabled || !validChannel(channel)) {
        return false;
    }
    const ChapterState& state = states_[static_cast<std::size_t>(channel)];
    return state.content == ContentType::Video && state.videoRequested;
}

void VisualComposer::acknowledgeVideoStarted(int channel) {
    if (!validChannel(channel)) {
        return;
    }
    ChapterState& state = states_[static_cast<std::size_t>(channel)];
    if (state.content == ContentType::Video) {
        state.videoRequested = false;
        state.videoPlaying = true;
        videoFinished_[static_cast<std::size_t>(channel)] = false;
    }
}

void VisualComposer::requestGenerator(int channel, GeneratorMode generator) {
    if (!validChannel(channel)) {
        return;
    }
    pendingGenerators_[static_cast<std::size_t>(channel)] = {
        static_cast<int>(generator)
    };
}

void VisualComposer::forceGenerator(int channel, GeneratorMode generator) {
    if (!validChannel(channel)) {
        return;
    }
    beginChapter(channel, ContentType::Generator, static_cast<int>(generator),
                 TargetScope::Channel,
                 groupFor(channel), true);
}

void VisualComposer::requestOrganization(OrganizationMode mode) {
    if (params_.programEnabled) {
        forceOrganization(mode);
        return;
    }
    requestedOrganization_ = mode;
    pendingOrganization_ = true;
}

void VisualComposer::forceOrganization(OrganizationMode mode) {
    pendingOrganization_ = false;
    if (params_.programEnabled) {
        organization_ = mode;
        ++revision_;
        for (ChapterState& state : states_)
            state.organization = mode;
        return;
    }
    applyOrganization(mode, true);
}

void VisualComposer::requestShared(ContentType content, int generator) {
    if (params_.programEnabled) {
        if (content == ContentType::Generator)
            forceShared(content, generator);
        return;
    }
    requestedSharedContent_ = content;
    requestedSharedGenerator_ = generator;
    pendingShared_ = true;
    pendingSharedForced_ = false;
}

void VisualComposer::forceShared(ContentType content, int generator) {
    pendingShared_ = false;
    if (params_.programEnabled &&
        content == ContentType::Generator &&
        moment_ == InstallationMoment::Intercalation) {
        forceTakeover(generator);
        return;
    }
    if (params_.programEnabled && content == ContentType::Video)
        return;
    if (content == ContentType::Generator && generator < 0) {
        generator = chooseGenerator(0);
    }
    generator = std::max(0, generator);
    for (int channel = 0; channel < channelCount(); ++channel) {
        beginChapter(channel, content, generator, TargetScope::Shared, -1, true);
    }
}

bool VisualComposer::validChannel(int channel) const {
    return channel >= 0 && channel < channelCount();
}

bool VisualComposer::readyToAdvance(int channel) const {
    if (!validChannel(channel)) {
        return false;
    }
    const ChapterState& state = states_[static_cast<std::size_t>(channel)];
    if (state.content == ContentType::Video) {
        return videoFinished_[static_cast<std::size_t>(channel)] &&
               state.elapsed >= state.minimumDwell;
    }
    return state.elapsed >= state.duration;
}

bool VisualComposer::protectedDwellMet(int channel) const {
    if (!validChannel(channel)) {
        return false;
    }
    const ChapterState& state = states_[static_cast<std::size_t>(channel)];
    if (state.content == ContentType::Video &&
        !videoFinished_[static_cast<std::size_t>(channel)]) {
        return false;
    }
    return state.elapsed >= state.minimumDwell;
}

InstallationMoment VisualComposer::chooseOpeningMoment() {
    const float roll = randomUnit();
    if (roll < 0.34f)
        return InstallationMoment::PulseSystem;
    if (roll < 0.62f)
        return InstallationMoment::BarScanSystem;
    return InstallationMoment::Intercalation;
}

InstallationMoment VisualComposer::momentAfterSystem(InstallationMoment current) {
    if (randomUnit() < 0.28f) {
        return current == InstallationMoment::PulseSystem
                   ? InstallationMoment::BarScanSystem
                   : InstallationMoment::PulseSystem;
    }
    return InstallationMoment::Intercalation;
}

void VisualComposer::beginMoment(InstallationMoment moment) {
    moment_ = moment;
    momentElapsed_ = 0.f;
    takeoverActive_ = false;
    if (moment == InstallationMoment::Intercalation) {
        beginIntercalation();
        return;
    }

    organization_ = OrganizationMode::Unison;
    const int generator = moment == InstallationMoment::PulseSystem
        ? static_cast<int>(GeneratorMode::Pulse)
        : static_cast<int>(GeneratorMode::BarScan);
    const float duration = moment == InstallationMoment::PulseSystem
        ? std::max(0.1f, params_.pulseMomentDuration)
        : std::max(0.1f, params_.barScanMomentDuration);
    for (int channel = 0; channel < channelCount(); ++channel) {
        beginChapter(channel, ContentType::Generator, generator,
                     TargetScope::Shared, -1, true);
        ChapterState& state = states_[static_cast<std::size_t>(channel)];
        state.duration = duration;
        state.minimumDwell = duration;
    }
}

void VisualComposer::beginIntercalation() {
    moment_ = InstallationMoment::Intercalation;
    momentElapsed_ = 0.f;
    takeoverActive_ = false;
    organization_ = OrganizationMode::Counterpoint;
    // Reiniciar temporizadores de eventos periódicos: disparar un intervalo después de que empiece Intercalation.
    nextNoiseAt_           = std::max(1.f, params_.noiseEventInterval);
    nextGeneratorInvertAt_ = std::max(1.f, params_.generatorInvertInterval);
    nextPolarityInvertAt_  = std::max(1.f, params_.polarityInvertInterval);
    const std::vector<int> available = systemGenerators();
    const int generatorCount = static_cast<int>(available.size());
    // beginChapter avanza la revisión, así que el offset de rotación se toma
    // una vez al inicio; leerlo por canal anulaba el stride y ponía todo el
    // muro en un solo generador.
    const int rotation = static_cast<int>(revision_ % generatorCount);
    for (int channel = 0; channel < channelCount(); ++channel) {
        const int generator =
            available[static_cast<std::size_t>((channel + rotation) %
                                               generatorCount)];
        beginChapter(channel, ContentType::Generator, generator,
                     TargetScope::Channel, groupFor(channel), true);
    }
    for (int group = 0; group < 2; ++group) {
        const int first = group * 4;
        if (!validChannel(first))
            continue;
        const int count = std::min(4, channelCount() - first);
        const int selected = first +
            static_cast<int>(randomUnit() * count) % std::max(1, count);
        beginChapter(selected, ContentType::Video, chooseGenerator(selected),
                     TargetScope::Channel, group, true);
    }
    scheduleNextTakeover();
}

void VisualComposer::endTakeover() {
    beginIntercalation();
}

void VisualComposer::forceNoiseMoment() {
    // Programa un evento de ruido analógico a muro completo en organización Unison.
    // AnalogNoise queda excluido a propósito del pool normal de generadores
    // (desactivado en settings), así que este es el único camino que lo activa.
    const int gen = static_cast<int>(GeneratorMode::AnalogNoise);
    takeoverActive_ = true;
    takeoverDuration_ = std::max(0.1f, params_.noiseEventDuration);
    organization_ = OrganizationMode::Unison;
    for (int channel = 0; channel < channelCount(); ++channel) {
        beginChapter(channel, ContentType::Generator, gen,
                     TargetScope::Shared, -1, true);
        states_[static_cast<std::size_t>(channel)].duration = takeoverDuration_;
        states_[static_cast<std::size_t>(channel)].minimumDwell = takeoverDuration_;
    }
    // nextNoiseAt_ lo reinicia beginIntercalation() a noiseEventInterval
    // al terminar el takeover, así que aquí no hace falta reprogramar.
}

void VisualComposer::forceGeneratorInvertMoment() {
    // Activa un overlay temporizado de inversión de color en todos los canales que dibujan generador.
    // Los canales VideoPointCloud quedan fuera (se gestiona en Channel::applyPolarity).
    // No se cambia contenido ni organización.
    generatorInvertActive_    = true;
    generatorInvertRemaining_ = std::max(0.1f, params_.generatorInvertDuration);
    // Programar el siguiente evento desde ahora, para que queden espaciados
    // de forma uniforme aunque se disparen a mano.
    nextGeneratorInvertAt_ = momentElapsed_ + params_.generatorInvertInterval;
    ofLogNotice("VisualComposer") << "Generator invert moment: "
        << generatorInvertRemaining_ << "s";
}

void VisualComposer::forcePolarityInvertMoment() {
    // Activa una inversión de polaridad temporizada de canal completo en todos los canales.
    // Se invierte tanto la salida de generador como la de VideoPointCloud.
    // No se cambia contenido ni organización.
    polarityInvertActive_    = true;
    polarityInvertRemaining_ = std::max(0.1f, params_.polarityInvertDuration);
    nextPolarityInvertAt_ = momentElapsed_ + params_.polarityInvertInterval;
    ofLogNotice("VisualComposer") << "Polarity invert moment: "
        << polarityInvertRemaining_ << "s";
}

void VisualComposer::scheduleNextTakeover() {
    const float minimum = std::max(1.f, params_.takeoverIntervalMin);
    const float maximum = std::max(minimum, params_.takeoverIntervalMax);
    nextTakeoverAt_ = momentElapsed_ +
        minimum + (maximum - minimum) * randomUnit();
}

void VisualComposer::ensureVideoReplacement(int departingChannel) {
    if (!validChannel(departingChannel))
        return;
    const int group = groupFor(departingChannel);
    if (activeVideoCount(group) > 1)
        return;
    const int first = group * 4;
    const int count = std::min(4, channelCount() - first);
    for (int offset = 1; offset <= count; ++offset) {
        const int candidate =
            first + (departingChannel - first + offset) % count;
        if (candidate == departingChannel ||
            states_[static_cast<std::size_t>(candidate)].content ==
                ContentType::Video)
            continue;
        beginChapter(candidate, ContentType::Video,
                     chooseGenerator(candidate), TargetScope::Channel,
                     group, false);
        return;
    }
}

ContentType VisualComposer::chooseConstrainedContent(int channel) {
    const int group = groupFor(channel);
    const int videos = activeVideoCount(group);
    const bool currentlyVideo =
        states_[static_cast<std::size_t>(channel)].content ==
        ContentType::Video;
    const int videosWithoutCurrent = videos - (currentlyVideo ? 1 : 0);
    if (videosWithoutCurrent <= 0 && !currentlyVideo)
        return ContentType::Video;

    const int maximum = ofClamp(params_.maxVideoPerGroup, 1, 2);
    float weights[3] = {
        std::max(0.f, params_.generatorProbability),
        std::max(0.f, params_.breathProbability),
        std::max(0.f, params_.transitionProbability)
    };
    // Dos franjas oscuras juntas se fundirían en un bloque y borrarían la
    // junta, así que breath nunca es vecino de breath.
    if (neighborUsesContent(channel, ContentType::Breath))
        weights[1] = 0.f;
    if (!currentlyVideo && videos < maximum &&
        randomUnit() < ofClamp(params_.dualVideoProbability, 0.f, 1.f))
        return ContentType::Video;
    const int selected = weightedIndex(weights, 3);
    return selected == 0 ? ContentType::Generator
                         : selected == 1 ? ContentType::Breath
                                         : ContentType::Transition;
}

void VisualComposer::beginChapter(int channel, ContentType content, int generator,
                                  TargetScope scope, int targetGroup, bool) {
    if (!validChannel(channel)) {
        return;
    }

    ChapterState& state = states_[static_cast<std::size_t>(channel)];
    state.content = content;
    state.generator = std::max(0, generator);
    state.organization = organization_;
    state.targetScope = scope;
    state.targetChannel = scope == TargetScope::Channel ? channel : -1;
    state.targetGroup = targetGroup;
    state.sharedTarget = scope == TargetScope::Shared
                             ? state.generator
                             : -1;
    state.elapsed = 0.f;
    state.duration = chooseDuration(content);
    switch (content) {
        case ContentType::Video:
            state.minimumDwell = std::max(0.001f, params_.videoMinDuration);
            break;
        case ContentType::Generator:
            state.minimumDwell = std::max(0.001f, params_.generatorMinDuration);
            break;
        case ContentType::Breath:
            state.minimumDwell = std::max(0.001f, params_.breathMinDuration);
            break;
        case ContentType::Transition:
            state.minimumDwell = std::max(0.001f, params_.transitionMinDuration);
            break;
    }
    state.chapterPhase = 0.f;
    state.stage = TemporalStage::Appearance;
    state.stageProgress = 0.f;
    state.envelope = 0.f;
    state.videoRequested = content == ContentType::Video;
    state.videoPlaying = false;
    videoFinished_[static_cast<std::size_t>(channel)] = false;
    state.revision = ++revision_;
    std::uint32_t mixed = params_.seed ^ static_cast<std::uint32_t>(channel + 1);
    mixed ^= static_cast<std::uint32_t>(state.generator + 1) * 0x9e3779b9u;
    mixed ^= static_cast<std::uint32_t>(state.revision);
    mixed ^= mixed >> 16u;
    mixed *= 0x7feb352du;
    mixed ^= mixed >> 15u;
    state.seed = mixed == 0u ? 1u : mixed;

    if (content == ContentType::Generator) {
        rememberGenerator(channel, state.generator);
    }
}

void VisualComposer::advanceChannel(int channel) {
    PendingGenerator& pending = pendingGenerators_[static_cast<std::size_t>(channel)];
    if (pending.value >= 0) {
        const int generator = pending.value;
        pending = {};
        beginChapter(channel, ContentType::Generator, generator, TargetScope::Channel,
                     groupFor(channel), false);
        return;
    }

    if (params_.programEnabled &&
        moment_ == InstallationMoment::Intercalation &&
        states_[static_cast<std::size_t>(channel)].content ==
            ContentType::Video)
        ensureVideoReplacement(channel);
    const ContentType content =
        params_.programEnabled &&
                moment_ == InstallationMoment::Intercalation
            ? chooseConstrainedContent(channel)
            : chooseContent(channel);
    const int generator = chooseGenerator(channel);
    const TargetScope scope = organization_ == OrganizationMode::Propagation
                                  ? TargetScope::Channel
                                  : TargetScope::Channel;
    beginChapter(channel, content, generator, scope, groupFor(channel), false);
    if (organization_ == OrganizationMode::Propagation) {
        ChapterState& state = states_[static_cast<std::size_t>(channel)];
        state.targetChannel = (channel + 1) % channelCount();
    }
}

void VisualComposer::applyOrganization(OrganizationMode mode, bool forced) {
    organization_ = mode;
    ++revision_;

    if (mode == OrganizationMode::Unison) {
        const ContentType content = chooseContent(0);
        const int generator = chooseGenerator(0);
        for (int channel = 0; channel < channelCount(); ++channel) {
            beginChapter(channel, content, generator, TargetScope::Shared, -1, forced);
        }
        return;
    }

    if (mode == OrganizationMode::Group4Plus4) {
        for (int group = 0; group < 2; ++group) {
            const int first = group * 4;
            if (!validChannel(first)) {
                continue;
            }
            const ContentType content = chooseContent(first);
            const int generator = chooseGenerator(first);
            const int end = std::min(first + 4, channelCount());
            for (int channel = first; channel < end; ++channel) {
                beginChapter(channel, content, generator, TargetScope::Group, group, forced);
            }
        }
        return;
    }

    for (int channel = 0; channel < channelCount(); ++channel) {
        const ContentType content = chooseContent(channel);
        const int generator = chooseGenerator(channel);
        beginChapter(channel, content, generator, TargetScope::Channel,
                     groupFor(channel), forced);
        if (mode == OrganizationMode::Propagation) {
            ChapterState& state = states_[static_cast<std::size_t>(channel)];
            state.targetChannel = (channel + 1) % channelCount();
            state.elapsed = -0.25f * static_cast<float>(channel);
        }
    }
}

ContentType VisualComposer::chooseContent(int channel) {
    float weights[kContentTypeCount] = {
        std::max(0.f, params_.videoProbability),
        std::max(0.f, params_.generatorProbability),
        std::max(0.f, params_.breathProbability),
        std::max(0.f, params_.transitionProbability)
    };

    const ContentType current = validChannel(channel)
                                    ? states_[static_cast<std::size_t>(channel)].content
                                    : ContentType::Video;
    weights[contentIndex(current)] *= 0.20f;
    if (neighborUsesContent(channel, ContentType::Breath))
        weights[contentIndex(ContentType::Breath)] = 0.f;
    return static_cast<ContentType>(weightedIndex(weights, kContentTypeCount));
}

OrganizationMode VisualComposer::chooseOrganization() {
    return static_cast<OrganizationMode>(
        weightedIndex(params_.organizationWeights.data(), kOrganizationCount));
}

int VisualComposer::chooseGenerator(int channel) {
    const int count = std::max(1, params_.generatorCount);
    std::vector<int> candidates;
    candidates.reserve(static_cast<std::size_t>(count));
    for (int generator = 0; generator < count; ++generator) {
        if (!generatorDisabled(generator) && !isRecent(channel, generator) &&
            !neighborUsesGenerator(channel, generator)) {
            candidates.push_back(generator);
        }
    }
    // La separación de vecinos prima sobre el filtro de historial reciente: un
    // repetido tras unos capítulos vale; un repetido a través de una junta, no.
    if (candidates.empty()) {
        for (int generator = 0; generator < count; ++generator) {
            if (!generatorDisabled(generator) &&
                !neighborUsesGenerator(channel, generator)) {
                candidates.push_back(generator);
            }
        }
    }
    if (candidates.empty()) {
        for (int generator = 0; generator < count; ++generator) {
            if (!generatorDisabled(generator))
                candidates.push_back(generator);
        }
    }
    if (candidates.empty())
        candidates.push_back(0);

    // Los datos visuales sesgan la siguiente familia de material sin dictarla.
    // Un generador no preferido sigue siendo posible, para preservar la sorpresa
    // y evitar que el análisis se convierta en un clasificador determinista.
    std::vector<int> preferred;
    for (int generator : candidates) {
        if (movementPrefersGenerator(collective_.movement, generator))
            preferred.push_back(generator);
    }
    const float response = ofClamp(params_.collectiveResponse, 0.f, 1.f);
    const std::vector<int>& pool =
        !preferred.empty() && randomUnit() < response ? preferred : candidates;
    const int index = static_cast<int>(
        randomUnit() * static_cast<float>(pool.size()));
    return pool[static_cast<std::size_t>(std::min(
        index, static_cast<int>(pool.size()) - 1))];
}

bool VisualComposer::movementPrefersGenerator(CollectiveMovement movement,
                                              int generator) const {
    const auto isOneOf = [generator](std::initializer_list<GeneratorMode> modes) {
        for (GeneratorMode mode : modes) {
            if (generator == static_cast<int>(mode))
                return true;
        }
        return false;
    };
    switch (movement) {
        case CollectiveMovement::Suspension:
            return isOneOf({GeneratorMode::PhaseLines,
                            GeneratorMode::SignalTrace});
        case CollectiveMovement::Codification:
            return isOneOf({GeneratorMode::BitMatrix,
                            GeneratorMode::ModularGrid,
                            GeneratorMode::DataLedger});
        case CollectiveMovement::Accumulation:
            return isOneOf({GeneratorMode::RasterPulse,
                            GeneratorMode::Pulse,
                            GeneratorMode::GranularRaster});
        case CollectiveMovement::Propagation:
            return isOneOf({GeneratorMode::PhaseLines,
                            GeneratorMode::VectorField,
                            GeneratorMode::SignalTrace});
        case CollectiveMovement::Convergence:
            return isOneOf({GeneratorMode::Pulse,
                            GeneratorMode::ModularGrid,
                            GeneratorMode::ThresholdBridge});
        case CollectiveMovement::Fragmentation:
            return isOneOf({GeneratorMode::BitMatrix,
                            GeneratorMode::DataLedger,
                            GeneratorMode::OrbitalRings,
                            GeneratorMode::DividedStrobe});
        case CollectiveMovement::Saturation:
            return isOneOf({GeneratorMode::BarScan,
                            GeneratorMode::GranularRaster,
                            GeneratorMode::AnalogNoise});
        case CollectiveMovement::Rupture:
            return isOneOf({GeneratorMode::Strobe,
                            GeneratorMode::DividedStrobe});
        case CollectiveMovement::Residue:
            return isOneOf({GeneratorMode::PhaseLines,
                            GeneratorMode::SignalTrace,
                            GeneratorMode::ThresholdBridge});
    }
    return false;
}

float VisualComposer::chooseDuration(ContentType content) {
    float minimum = 1.f;
    float maximum = 1.f;
    switch (content) {
        case ContentType::Video:
            minimum = params_.videoMinDuration;
            maximum = params_.videoMaxDuration;
            break;
        case ContentType::Generator:
            minimum = params_.generatorMinDuration;
            maximum = params_.generatorMaxDuration;
            break;
        case ContentType::Breath:
            minimum = params_.breathMinDuration;
            maximum = params_.breathMaxDuration;
            break;
        case ContentType::Transition:
            minimum = params_.transitionMinDuration;
            maximum = params_.transitionMaxDuration;
            break;
    }
    minimum = std::max(0.001f, minimum);
    maximum = std::max(minimum, maximum);
    float durationScale = 1.f;
    switch (collective_.movement) {
        case CollectiveMovement::Suspension:    durationScale = 1.25f; break;
        case CollectiveMovement::Codification: durationScale = 1.05f; break;
        case CollectiveMovement::Accumulation: durationScale = 0.88f; break;
        case CollectiveMovement::Propagation:  durationScale = 0.92f; break;
        case CollectiveMovement::Convergence:  durationScale = 1.12f; break;
        case CollectiveMovement::Fragmentation:durationScale = 0.68f; break;
        case CollectiveMovement::Saturation:   durationScale = 0.62f; break;
        case CollectiveMovement::Rupture:      durationScale = 0.42f; break;
        case CollectiveMovement::Residue:      durationScale = 1.32f; break;
    }
    return (minimum + (maximum - minimum) * randomUnit()) * durationScale;
}

float VisualComposer::randomUnit() {
    return std::generate_canonical<float, 24>(rng_);
}

int VisualComposer::weightedIndex(const float* weights, int count) {
    float total = 0.f;
    for (int i = 0; i < count; ++i) {
        total += std::max(0.f, weights[i]);
    }
    if (total <= 0.f) {
        return 0;
    }
    float choice = randomUnit() * total;
    for (int i = 0; i < count; ++i) {
        choice -= std::max(0.f, weights[i]);
        if (choice <= 0.f) {
            return i;
        }
    }
    return count - 1;
}

bool VisualComposer::isRecent(int channel, int generator) const {
    if (!validChannel(channel)) {
        return false;
    }
    const std::deque<int>& history = histories_[static_cast<std::size_t>(channel)];
    return std::find(history.begin(), history.end(), generator) != history.end();
}

bool VisualComposer::generatorDisabled(int generator) const {
    return std::find(params_.disabledGenerators.begin(),
                     params_.disabledGenerators.end(),
                     generator) != params_.disabledGenerators.end();
}

// Los generadores abstractos / raster (Pulse hasta DividedStrobe), menos los desactivados.
std::vector<int> VisualComposer::systemGenerators() const {
    const int first = static_cast<int>(GeneratorMode::Pulse);
    const int last = static_cast<int>(GeneratorMode::DividedStrobe);
    std::vector<int> available;
    for (int generator = first; generator <= last; ++generator) {
        if (!generatorDisabled(generator))
            available.push_back(generator);
    }
    if (available.empty())
        available.push_back(first);
    return available;
}

bool VisualComposer::neighborUsesGenerator(int channel, int generator) const {
    for (const int offset : {-1, 1}) {
        const int neighbor = channel + offset;
        if (!validChannel(neighbor))
            continue;
        const ChapterState& state = states_[static_cast<std::size_t>(neighbor)];
        const bool generated = state.content == ContentType::Generator ||
                               state.content == ContentType::Transition;
        if (generated && state.generator == generator)
            return true;
    }
    return false;
}

bool VisualComposer::neighborUsesContent(int channel, ContentType content) const {
    for (const int offset : {-1, 1}) {
        const int neighbor = channel + offset;
        if (!validChannel(neighbor))
            continue;
        if (states_[static_cast<std::size_t>(neighbor)].content == content)
            return true;
    }
    return false;
}

void VisualComposer::rememberGenerator(int channel, int generator) {
    std::deque<int>& history = histories_[static_cast<std::size_t>(channel)];
    history.push_front(generator);
    while (history.size() > params_.recentHistorySize) {
        history.pop_back();
    }
}

void VisualComposer::updateStage(ChapterState& state) {
    const float phase = clamp01(state.chapterPhase);
    float start = 0.f;
    float end = 0.12f;
    if (phase < 0.12f) {
        state.stage = TemporalStage::Appearance;
    } else if (phase < 0.55f) {
        state.stage = TemporalStage::Development;
        start = 0.12f; end = 0.55f;
    } else if (phase < 0.72f) {
        state.stage = TemporalStage::Threshold;
        start = 0.55f; end = 0.72f;
    } else if (phase < 0.90f) {
        state.stage = TemporalStage::Transformation;
        start = 0.72f; end = 0.90f;
    } else {
        state.stage = TemporalStage::Dissolution;
        start = 0.90f; end = 1.f;
    }
    state.stageProgress = clamp01((phase - start) / std::max(0.001f, end - start));
    switch (state.stage) {
        case TemporalStage::Appearance:
            state.envelope = state.stageProgress * state.stageProgress *
                             (3.f - 2.f * state.stageProgress);
            break;
        case TemporalStage::Development:
        case TemporalStage::Threshold:
        case TemporalStage::Transformation:
            state.envelope = 1.f;
            break;
        case TemporalStage::Dissolution:
            state.envelope = 1.f - state.stageProgress * state.stageProgress *
                             (3.f - 2.f * state.stageProgress);
            break;
    }
}

float VisualComposer::clamp01(float value) {
    return std::max(0.f, std::min(1.f, value));
}

int VisualComposer::groupFor(int channel) {
    return channel < 4 ? 0 : 1;
}
