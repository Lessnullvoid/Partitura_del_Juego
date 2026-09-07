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

enum class InstallationMoment : std::uint8_t {
    PulseSystem = 0,
    BarScanSystem,
    Intercalation
};

// Movimiento formal global inferido de los ocho flujos visuales/de datos. Son
// estados musicales, no modos de render: SuperCollider los usa para
// reorganizar densidad, sincronización, registro, espacio y silencio.
enum class CollectiveMovement : std::uint8_t {
    Suspension = 0,
    Codification,
    Accumulation,
    Propagation,
    Convergence,
    Fragmentation,
    Saturation,
    Rupture,
    Residue
};

struct CollectiveState {
    CollectiveMovement movement = CollectiveMovement::Codification;
    float phase = 0.f;
    float activity = 0.f;
    float coherence = 0.f;
    float diversity = 0.f;
    float convergence = 0.f;
    float population = 0.f;
    float tension = 0.f;
    int dominantGenerator = -1;
    std::uint64_t revision = 0;
};

struct VisualComposerParams {
    // Desactivado por defecto para que añadir el compositor no altere shows existentes.
    bool enabled = false;

    // Pesos relativos de selección de contenido.
    float videoProbability = 0.45f;
    float generatorProbability = 0.35f;
    float breathProbability = 0.12f;
    float transitionProbability = 0.08f;

    // Rangos de permanencia protegida en segundos.
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
    int generatorCount = 15;
    // Índices de generador que el programa nunca programa por sí solo. Orbital Rings queda
    // fuera de la secuencia por defecto; sigue alcanzable por control directo.
    std::vector<int> disabledGenerators{
        static_cast<int>(GeneratorMode::OrbitalRings)
    };
    bool programEnabled = true;
    float pulseMomentDuration = 24.f;
    float barScanMomentDuration = 32.f;
    int maxVideoPerGroup = 2;
    float dualVideoProbability = 0.32f;
    float takeoverIntervalMin = 45.f;
    float takeoverIntervalMax = 90.f;
    float takeoverDurationMin = 6.f;
    float takeoverDurationMax = 13.f;

    // Evento periódico de sincronización de ruido analógico a muro completo.
    bool  noiseEventEnabled  = true;
    float noiseEventInterval = 60.f;    // segundos entre eventos de ruido (durante Intercalation)
    float noiseEventDuration = 5.f;     // duración de cada evento de ruido en segundos

    // Overlay de inversión de color del generador: todos los canales que dibujan
    // generador pasan a fondo blanco / gráficos negros un instante y luego revierten.
    // Los canales VideoPointCloud no se ven afectados.
    bool  generatorInvertEnabled  = true;
    float generatorInvertInterval = 60.f;   // segundos entre eventos de inversión
    float generatorInvertDuration = 10.f;   // duración de cada evento de inversión

    // Inversión de polaridad de canal completo: toda la salida (generadores Y nubes
    // de puntos de vídeo) pasa a negativo un instante y luego revierte.
    // Agenda independiente de la inversión solo de generador de arriba.
    bool  polarityInvertEnabled  = true;
    float polarityInvertInterval = 90.f;   // segundos entre eventos de inversión de polaridad
    float polarityInvertDuration = 8.f;    // duración de cada evento de inversión de polaridad

    // Pesos relativos de selección de organización, en orden del enum.
    std::array<float, 4> organizationWeights{{0.20f, 0.30f, 0.35f, 0.15f}};

    // Movimiento formal asistido por datos. Los estados candidatos deben
    // permanecer estables el tiempo de decisionHold, y los estados musicales
    // conservan una permanencia protegida.
    bool collectiveLogicEnabled = true;
    float collectiveDecisionHold = 1.25f;
    float collectiveMinimumDwell = 4.f;
    float collectiveEventCooldown = 16.f;
    float collectiveResponse = 0.72f;
};

// Paquete de metadatos pequeño y copiable. A propósito no contiene texturas,
// reproductores, FBO ni objetos generador con propiedad.
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
    InstallationMoment moment() const { return moment_; }
    float momentElapsed() const { return momentElapsed_; }
    bool takeoverActive() const { return takeoverActive_; }
    bool generatorInvertActive() const { return generatorInvertActive_; }
    bool polarityInvertActive() const { return polarityInvertActive_; }
    const CollectiveState& collectiveState() const { return collective_; }
    void observeChannel(int channel, float motionEnergy, float flowMagnitude,
                        float flowAngle, float crowdDensity, int blobCount,
                        bool collision);
    int activeVideoCount(int group) const;
    void forceNextMoment();
    void forceTakeover(int generator = -1);
    void forceNoiseMoment();
    void forceGeneratorInvertMoment();
    void forcePolarityInvertMoment();

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
    bool generatorDisabled(int generator) const;
    std::vector<int> systemGenerators() const;
    // Las franjas adyacentes no deben mostrar el mismo material, de modo que
    // la junta entre dos pantallas siempre porte una diferencia.
    bool neighborUsesGenerator(int channel, int generator) const;
    bool neighborUsesContent(int channel, ContentType content) const;
    void rememberGenerator(int channel, int generator);
    void updateStage(ChapterState& state);
    void beginMoment(InstallationMoment moment);
    // Los momentos de sistema unificado son acontecimientos, no una apertura
    // fija, así que el primer momento y el que sigue a uno se extraen de la semilla.
    InstallationMoment chooseOpeningMoment();
    InstallationMoment momentAfterSystem(InstallationMoment current);
    void beginIntercalation();
    void endTakeover();
    void scheduleNextTakeover();
    void updateCollectiveAnalysis(float dt);
    void applyCollectiveOrganization(OrganizationMode mode);
    bool movementPrefersGenerator(CollectiveMovement movement,
                                  int generator) const;
    void ensureVideoReplacement(int departingChannel);
    ContentType chooseConstrainedContent(int channel);
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
    bool wasProgramEnabled_ = false;
    InstallationMoment moment_ = InstallationMoment::PulseSystem;
    float momentElapsed_ = 0.f;
    bool takeoverActive_ = false;
    float takeoverDuration_ = 0.f;
    float nextTakeoverAt_ = 0.f;
    float nextNoiseAt_    = 0.f;   // valor de momentElapsed_ en el que dispara el siguiente evento de ruido

    // Estado del overlay de inversión de color del generador (puramente visual, sin cambio de contenido).
    bool  generatorInvertActive_    = false;
    float generatorInvertRemaining_ = 0.f;  // cuenta atrás en segundos
    float nextGeneratorInvertAt_    = 0.f;  // umbral de disparo de momentElapsed_

    // Estado de inversión de polaridad de canal completo (generadores + VPC, puramente visual).
    bool  polarityInvertActive_    = false;
    float polarityInvertRemaining_ = 0.f;
    float nextPolarityInvertAt_    = 0.f;

    struct CollectiveObservation {
        float motion = 0.f;
        float flow = 0.f;
        float angle = 0.f;
        float crowd = 0.f;
        float population = 0.f;
        bool collisionInput = false;
        bool collisionPulse = false;
    };
    std::vector<CollectiveObservation> observations_;
    CollectiveState collective_;
    CollectiveMovement collectiveCandidate_ = CollectiveMovement::Codification;
    float collectiveCandidateElapsed_ = 0.f;
    float collectiveStateElapsed_ = 0.f;
    float collectiveEventCooldownRemaining_ = 0.f;
    float previousCollectiveActivity_ = 0.f;
    std::uint64_t handledCollectiveRevision_ = 0;
};
