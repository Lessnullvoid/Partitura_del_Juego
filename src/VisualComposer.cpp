#include "VisualComposer.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kContentTypeCount = 4;
constexpr int kOrganizationCount = 4;

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

    for (int channel = 0; channel < count; ++channel) {
        ChapterState& state = states_[static_cast<std::size_t>(channel)];
        state.channel = channel;
        state.targetChannel = channel;
        state.targetGroup = groupFor(channel);
        state.organization = organization_;
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
        applyOrganization(chooseOrganization(), true);
    }

    if (dt > 0.f) {
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
    requestedOrganization_ = mode;
    pendingOrganization_ = true;
}

void VisualComposer::forceOrganization(OrganizationMode mode) {
    pendingOrganization_ = false;
    applyOrganization(mode, true);
}

void VisualComposer::requestShared(ContentType content, int generator) {
    requestedSharedContent_ = content;
    requestedSharedGenerator_ = generator;
    pendingShared_ = true;
    pendingSharedForced_ = false;
}

void VisualComposer::forceShared(ContentType content, int generator) {
    pendingShared_ = false;
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

    const ContentType content = chooseContent(channel);
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
        if (!isRecent(channel, generator)) {
            candidates.push_back(generator);
        }
    }
    if (candidates.empty()) {
        for (int generator = 0; generator < count; ++generator) {
            candidates.push_back(generator);
        }
    }
    const int index = static_cast<int>(randomUnit() * static_cast<float>(candidates.size()));
    return candidates[static_cast<std::size_t>(std::min(index,
                                                        static_cast<int>(candidates.size()) - 1))];
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
    return minimum + (maximum - minimum) * randomUnit();
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
