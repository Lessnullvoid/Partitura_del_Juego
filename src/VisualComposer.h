#pragma once

#include "ofMain.h"
#include "VisualGenerator.h"

#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <random>
#include <vector>

enum class ContentType : std::uint8_t {
    Video = 0,
    Generator,
    Breath,
    Transition
};

enum class TargetScope : std::uint8_t {
    Channel = 0,
    Group,
    Shared
};

struct VisualComposerParams {
    // Disabled by default so adding the composer does not alter existing shows.
    bool enabled = false;

    // Relative content-selection weights.
    float videoProbability = 0.45f;
    float generatorProbability = 0.35f;
    float breathProbability = 0.12f;
    float transitionProbability = 0.08f;

    // Protected dwell ranges in seconds.
    float videoMinDuration = 8.f;
    float videoMaxDuration = 30.f;
    float generatorMinDuration = 6.f;
    float generatorMaxDuration = 20.f;
    float breathMinDuration = 2.f;
    float breathMaxDuration = 7.f;
    float transitionMinDuration = 1.f;
    float transitionMaxDuration = 4.f;

    float bpm = 90.f;
    int beatSubdivision = 4;
    std::size_t recentHistorySize = 3;
    std::uint32_t seed = 0x50444a31u;
    int generatorCount = 8;

    // Relative organization-selection weights, in enum order.
    std::array<float, 4> organizationWeights{{0.20f, 0.30f, 0.35f, 0.15f}};
};

// Small, copyable metadata packet. It intentionally contains no textures,
// players, FBOs, or ownership-bearing generator objects.
struct ChapterState {
    int channel = -1;
    ContentType content = ContentType::Video;
    TemporalStage stage = TemporalStage::Appearance;
    OrganizationMode organization = OrganizationMode::Counterpoint;
    TargetScope targetScope = TargetScope::Channel;

    int generator = static_cast<int>(GeneratorMode::RasterPulse);
    int targetChannel = -1;
    int targetGroup = -1;
    int sharedTarget = -1;

    float elapsed = 0.f;
    float duration = 0.f;
    float minimumDwell = 0.f;
    float chapterPhase = 0.f;
    float stageProgress = 0.f;
    float envelope = 0.f;
    float beatPhase = 0.f;
    std::uint64_t beatIndex = 0;
    int subdivisionIndex = 0;
    bool subdivisionPulse = false;

    bool videoRequested = false;
    bool videoPlaying = false;
    std::uint32_t seed = 1u;
    std::uint64_t revision = 0;
};

class VisualComposer {
public:
    void setup(const VisualComposerParams& params = {}, int channelCount = 8);
    void update(float globalSpeed = 1.f);

    VisualComposerParams& params() { return params_; }
    const VisualComposerParams& params() const { return params_; }

    const ChapterState& stateFor(int channel) const;

    void notifyVideoFinished(int channel);
    bool shouldRequestVideo(int channel) const;
    void acknowledgeVideoStarted(int channel);

    void requestGenerator(int channel, GeneratorMode generator);
    void forceGenerator(int channel, GeneratorMode generator);
    void requestOrganization(OrganizationMode mode);
    void forceOrganization(OrganizationMode mode);
    void requestShared(ContentType content, int generator = -1);
    void forceShared(ContentType content, int generator = -1);

    std::uint64_t revision() const { return revision_; }
    int channelCount() const { return static_cast<int>(states_.size()); }

private:
    struct PendingGenerator {
        int value = -1;
    };

    bool validChannel(int channel) const;
    bool readyToAdvance(int channel) const;
    bool protectedDwellMet(int channel) const;
    void beginChapter(int channel, ContentType content, int generator,
                      TargetScope scope, int targetGroup, bool forced);
    void advanceChannel(int channel);
    void applyOrganization(OrganizationMode mode, bool forced);
    ContentType chooseContent(int channel);
    OrganizationMode chooseOrganization();
    int chooseGenerator(int channel);
    float chooseDuration(ContentType content);
    float randomUnit();
    int weightedIndex(const float* weights, int count);
    bool isRecent(int channel, int generator) const;
    void rememberGenerator(int channel, int generator);
    void updateStage(ChapterState& state);
    static float clamp01(float value);
    static int groupFor(int channel);

    VisualComposerParams params_;
    std::vector<ChapterState> states_;
    std::vector<std::deque<int>> histories_;
    std::vector<PendingGenerator> pendingGenerators_;
    std::vector<bool> videoFinished_;
    std::mt19937 rng_;

    OrganizationMode organization_ = OrganizationMode::Counterpoint;
    float beatPosition_ = 0.f;
    std::uint64_t beatIndex_ = 0;
    int subdivisionIndex_ = 0;
    bool subdivisionPulse_ = false;
    bool pendingOrganization_ = false;
    OrganizationMode requestedOrganization_ = OrganizationMode::Counterpoint;
    bool pendingShared_ = false;
    bool pendingSharedForced_ = false;
    ContentType requestedSharedContent_ = ContentType::Generator;
    int requestedSharedGenerator_ = -1;

    float lastTime_ = -1.f;
    std::uint64_t lastFrame_ = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t revision_ = 0;
    bool wasEnabled_ = false;
};
