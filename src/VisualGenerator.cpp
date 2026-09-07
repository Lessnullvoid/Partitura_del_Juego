#include "VisualGenerator.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.f * kPi;

float fract(float value) {
    return value - std::floor(value);
}

uint32_t combineSeed(uint32_t seed, uint32_t value) {
    return VisualGenerator::hash(seed ^ (value + 0x9e3779b9u + (seed << 6u) + (seed >> 2u)));
}

std::string fixedValue(float value, int precision = 3) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

ofColor systemColor(const GeneratorContext& context, bool alternate,
                    float alpha) {
    ofColor color = alternate ? context.red : context.cyan;
    color.a = static_cast<unsigned char>(ofClamp(alpha, 0.f, 255.f));
    return color;
}
} // namespace

void VisualGenerator::setup(int width, int height) {
    blurReady_ = blurShader_.load("shaders/generator_blur.vert",
                                  "shaders/generator_blur.frag");
    noiseShaderReady_ = noiseShader_.load("shaders/analog_noise.vert",
                                          "shaders/analog_noise.frag");
    ensureAllocated(width, height);
}

void VisualGenerator::setMode(GeneratorMode mode) {
    if (mode_ == mode)
        return;
    mode_ = mode;
    if (feedbackA_.isAllocated()) {
        feedbackA_.begin();
        ofClear(0, 0, 0, 255);
        feedbackA_.end();
    }
    if (feedbackB_.isAllocated()) {
        feedbackB_.begin();
        ofClear(0, 0, 0, 255);
        feedbackB_.end();
    }
    feedbackFlip_ = false;
}

void VisualGenerator::ensureAllocated(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (fbo_.isAllocated() && emissionFbo_.isAllocated() &&
        width == width_ && height == height_) {
        return;
    }

    width_ = width;
    height_ = height;

    ofFbo::Settings settings;
    settings.width = width_;
    settings.height = height_;
    settings.internalformat = GL_RGBA16F;
    settings.useDepth = false;
    settings.useStencil = false;
    settings.numSamples = 0;
    settings.textureTarget = GL_TEXTURE_2D;
    fbo_.allocate(settings);
    emissionFbo_.allocate(settings);
    feedbackA_.allocate(settings);
    feedbackB_.allocate(settings);

    settings.width = std::max(1, width_ / 2);
    settings.height = std::max(1, height_ / 2);
    blurA_.allocate(settings);
    blurB_.allocate(settings);

    fbo_.begin();
    ofClear(0, 0, 0, 255);
    fbo_.end();
    emissionFbo_.begin();
    ofClear(0, 0, 0, 255);
    emissionFbo_.end();
    feedbackA_.begin();
    ofClear(0, 0, 0, 255);
    feedbackA_.end();
    feedbackB_.begin();
    ofClear(0, 0, 0, 255);
    feedbackB_.end();
    feedbackFlip_ = false;
    blurA_.begin();
    ofClear(0, 0, 0, 0);
    blurA_.end();
    blurB_.begin();
    ofClear(0, 0, 0, 0);
    blurB_.end();

    // Fuente dimensionada para ~22 columnas de lluvia a esta resolución (se carga una vez por cambio de dimensión).
    const int newFontSize = std::max(8, width_ / 22);
    if (newFontSize != loadedFontSize_) {
        fontReady_ = matrixFont_.load(OF_TTF_MONO, newFontSize, true, false);
        loadedFontSize_ = newFontSize;
    }
}

void VisualGenerator::update(const GeneratorContext& context) {
    render(context);
}

void VisualGenerator::render(const GeneratorContext& context) {
    const int requestedWidth = context.width > 0 ? context.width : width_;
    const int requestedHeight = context.height > 0 ? context.height : height_;
    ensureAllocated(requestedWidth, requestedHeight);
    state_ = resolveState(context);

    emissionFbo_.begin();
    ofPushStyle();
    ofPushMatrix();
    ofClear(0, 0, 0, 255);
    ofEnableAlphaBlending();
    ofSetLineWidth(1.f);
    ofNoFill();

    renderScene(context);
    if (mode_ < GeneratorMode::Pulse) {
        drawFrame(context);
        applyAnalogTreatment(context);
    }

    ofPopMatrix();
    ofPopStyle();
    emissionFbo_.end();
    compositeGlow(context);
}

float VisualGenerator::glowStrength() const {
    switch (mode_) {
        case GeneratorMode::Pulse:         return 0.58f;
        case GeneratorMode::BarScan:       return 1.f;
        case GeneratorMode::GranularRaster: return 0.66f;
        case GeneratorMode::OrbitalRings:   return 0.82f;
        // Modos Strobe/noise: los cortes duros no necesitan smear de bloom
        case GeneratorMode::Strobe:        return 0.f;
        case GeneratorMode::DividedStrobe: return 0.f;
        case GeneratorMode::AnalogNoise:   return 0.f;
        default:                           return 0.85f;
    }
}

bool VisualGenerator::usesFeedback() const {
    // Las estelas de feedback emborronarían el corte seco de los destellos Strobe y noise
        switch (mode_) {
        case GeneratorMode::Strobe:
        case GeneratorMode::DividedStrobe:
        case GeneratorMode::AnalogNoise:
            return false;
        default:
            return true;
    }
}

// Los campos raster se fundirían en una lámina continua con la longitud de
// estela que conviene a barras aisladas, así que cada modo escala el decay configurado.
float VisualGenerator::feedbackScale() const {
    switch (mode_) {
        case GeneratorMode::Pulse:        return 1.f;
        case GeneratorMode::BarScan:      return 0.86f;
        case GeneratorMode::GranularRaster: return 0.72f;
        case GeneratorMode::OrbitalRings:   return 0.9f;
        default:                          return 0.95f;
    }
}

// Los ocho generadores anteriores dibujan geometría vectorial limpia. En vez de
// reescribir cada uno, su buffer de emisión se expone con la misma óptica que
// el material de barras: puerta de exposición escalonada, dropout granular y polvo.
void VisualGenerator::applyAnalogTreatment(const GeneratorContext& context) {
    const float time = static_cast<float>(state_.localTime);
    const float tempo =
        2.2f + 0.4f * static_cast<float>((context.globalChannelIndex * 7) % 5);
    const float clock = time * tempo + state_.rolePhase * 3.f;
    const int step = static_cast<int>(std::floor(clock));
    const float stepPhase = fract(clock);
    const float strobe = stepPhase < 0.6f
        ? 1.f
        : 1.f - smooth(saturate((stepPhase - 0.6f) * 3.5f));
    const float blackout =
        hash01(combineSeed(state_.resolvedSeed,
                           static_cast<uint32_t>(step + 8192))) > 0.93f
            ? 0.3f
            : 1.f;
    const float exposure =
        ofClamp(0.42f + strobe * blackout * 0.58f, 0.1f, 1.f);
    const float timeSalt = std::floor(time * 24.f) * 0.39f;

    ofPushStyle();
    ofFill();
    ofEnableBlendMode(OF_BLENDMODE_MULTIPLY);
    const int level = static_cast<int>(exposure * 255.f);
    ofSetColor(level, level, level, 255);
    ofDrawRectangle(0.f, 0.f, static_cast<float>(width_),
                    static_cast<float>(height_));

    ofEnableBlendMode(OF_BLENDMODE_ADD);
    drawAnalogHaze(context, timeSalt);
    ofEnableAlphaBlending();
    ofPopStyle();
}

// Barra luminosa de tramo fijo cuyo alcance horizontal no varía. Los dropouts
// de exposición a lo largo son lo que se lee como interferencia analógica.
void VisualGenerator::drawAnalogBar(const GeneratorContext& context, float inset,
                                    float span, float top, float thickness,
                                    float peak, uint32_t key, float timeSalt) {
    const int slices = 72;
    const float sliceWidth = span / slices;
    for (int slice = 0; slice < slices; ++slice) {
        const float grain =
            noise2D(slice * 0.23f, timeSalt, combineSeed(key, 43u));
        if (grain < 0.11f)
            continue;
        const float level = std::min(255.f, peak * (0.62f + grain * 0.6f));
        ofSetColor(systemColor(context, grain < 0.3f, level));
        ofDrawRectangle(inset + slice * sliceWidth, top,
                        sliceWidth + 0.75f, thickness);
    }
}

// Estrías de dropout dispersas. El paso de feedback retiene lo que se dibuje
// aquí, así que se mantiene raro y tenue; un campo denso lavaría el fotograma a gris.
void VisualGenerator::drawAnalogHaze(const GeneratorContext& context,
                                     float timeSalt) {
    ofFill();
    constexpr int kStreaks = 10;
    for (int streak = 0; streak < kStreaks; ++streak) {
        const uint32_t key = combineSeed(
            state_.resolvedSeed,
            static_cast<uint32_t>(601 + streak * 37 +
                                  static_cast<int>(timeSalt * 7.f)));
        const float grain = hash01(key);
        if (grain < 0.72f)
            continue;
        const float y = hash01(combineSeed(key, 5u)) * height_;
        const float length = width_ * (0.08f + hash01(combineSeed(key, 9u)) * 0.3f);
        const float x = hash01(combineSeed(key, 11u)) * (width_ - length);
        ofSetColor(context.red, static_cast<int>(
            (10.f + (grain - 0.72f) * 60.f) * state_.envelope));
        ofDrawRectangle(x, y, length, 0.5f + grain);
    }
}

void VisualGenerator::compositeGlow(const GeneratorContext& context) {
    const float strength =
        ofClamp(glowStrength() * context.glowGain, 0.f, 1.5f);
    if (blurReady_ && strength > 0.001f) {
        blurA_.begin();
        ofClear(0, 0, 0, 0);
        blurShader_.begin();
        blurShader_.setUniformTexture("tex0", emissionFbo_.getTexture(), 0);
        blurShader_.setUniform2f("direction", context.glowRadius, 0.f);
        ofSetColor(255);
        emissionFbo_.draw(0, 0, blurA_.getWidth(), blurA_.getHeight());
        blurShader_.end();
        blurA_.end();

        blurB_.begin();
        ofClear(0, 0, 0, 0);
        blurShader_.begin();
        blurShader_.setUniformTexture("tex0", blurA_.getTexture(), 0);
        blurShader_.setUniform2f("direction", 0.f, context.glowRadius);
        ofSetColor(255);
        blurA_.draw(0, 0, blurB_.getWidth(), blurB_.getHeight());
        blurShader_.end();
        blurB_.end();
    }

    fbo_.begin();
    ofClear(0, 0, 0, 255);
    ofPushStyle();
    ofEnableAlphaBlending();
    ofSetColor(255);
    emissionFbo_.draw(0, 0, width_, height_);
    if (strength > 0.001f) {
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        ofSetColor(255, 255, 255,
                   static_cast<int>(ofClamp(strength, 0.f, 1.f) * 175.f));
        blurB_.draw(0, 0, width_, height_);
    }
    ofPopStyle();
    fbo_.end();

    if (usesFeedback()) {
        ofFbo& previous = feedbackFlip_ ? feedbackA_ : feedbackB_;
        ofFbo& next = feedbackFlip_ ? feedbackB_ : feedbackA_;
        next.begin();
        ofClear(0, 0, 0, 255);
        ofPushStyle();
        ofEnableAlphaBlending();
        ofSetColor(255, 255, 255, static_cast<int>(
            ofClamp(context.feedbackDecay * feedbackScale(), 0.f, 0.94f)
            * 255.f));
        previous.draw(0, 0, width_, height_);
        glEnable(GL_BLEND);
        glBlendEquation(GL_MAX);
        glBlendFunc(GL_ONE, GL_ONE);
        ofSetColor(255);
        fbo_.draw(0, 0, width_, height_);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        ofEnableAlphaBlending();
        ofPopStyle();
        next.end();

        fbo_.begin();
        ofClear(0, 0, 0, 255);
        ofPushStyle();
        ofDisableAlphaBlending();
        ofSetColor(255);
        next.draw(0, 0, width_, height_);
        ofPopStyle();
        fbo_.end();
        feedbackFlip_ = !feedbackFlip_;
    }
}

VisualState VisualGenerator::resolveState(const GeneratorContext& context) const {
    VisualState result;
    const int channelCount = std::max(1, context.globalChannelCount);
    const int channel = ofClamp(context.globalChannelIndex, 0, channelCount - 1);
    const float progress = saturate(context.stageProgress);
    const float role = roleValue(context.role);

    result.localTime = context.elapsedTime;
    result.stageProgress = progress;
    result.intensity = saturate(context.intensity);
    result.density = saturate(context.density);
    result.rolePhase = role;
    result.observedGroup = (channel % 8) < 4;
    result.resolvedSeed = context.seed == 0u ? 1u : context.seed;

    if (context.cvData != nullptr) {
        result.motion = saturate(context.cvData->motionEnergy);
        result.flowMagnitude = saturate(context.cvData->flowMagnitude);
        result.flowAngle = context.cvData->flowAngle;
        const float crowd = saturate(context.cvData->events.crowdDensity);
        const float blobDensity = saturate(static_cast<float>(context.cvData->blobCount) / 12.f);
        result.density = saturate(result.density * 0.55f + crowd * 0.25f + blobDensity * 0.20f);
        result.intensity = saturate(result.intensity * 0.65f + result.motion * 0.35f);
    }

    switch (context.organization) {
        case OrganizationMode::Unison:
            result.resolvedSeed = combineSeed(result.resolvedSeed,
                                              static_cast<uint32_t>(context.segmentIndex + 1));
            break;

        case OrganizationMode::Propagation: {
            const float rank = channelCount > 1
                                   ? static_cast<float>(channel) / static_cast<float>(channelCount - 1)
                                   : 0.f;
            const float groupOffset = static_cast<float>(std::max(0, context.windowGroup)) * 0.08f;
            result.propagationDelay = rank * (0.35f + 1.4f * (1.f - result.density))
                                      + groupOffset;
            result.localTime -= result.propagationDelay;
            result.stageProgress = saturate(progress - rank * 0.22f);
            result.resolvedSeed = combineSeed(result.resolvedSeed,
                                              static_cast<uint32_t>(context.segmentIndex + 1));
            break;
        }

        case OrganizationMode::Counterpoint: {
            const float direction = (static_cast<int>(context.role) % 2 == 0) ? 1.f : -1.f;
            result.rolePhase = fract(role + static_cast<float>(channel) / channelCount
                                     + context.windowGroup * 0.125f);
            result.localTime = context.elapsedTime * (1.0 + 0.08 * direction)
                               + result.rolePhase * 2.75;
            result.intensity = saturate(result.intensity
                                        + direction * (context.beatPhase - 0.5f) * 0.22f);
            result.density = saturate(result.density
                                      + (0.5f - direction * 0.5f) * 0.16f);
            result.resolvedSeed = combineSeed(
                result.resolvedSeed,
                static_cast<uint32_t>(31 * static_cast<int>(context.role) + channel + 1));
            break;
        }

        case OrganizationMode::Group4Plus4:
            result.resolvedSeed = combineSeed(
                result.resolvedSeed,
                static_cast<uint32_t>(result.observedGroup ? channel + 1
                                                          : 101 + static_cast<int>(context.role)));
            if (result.observedGroup) {
                result.localTime = context.elapsedTime;
                result.intensity = saturate(result.intensity * 0.75f + result.motion * 0.25f);
            } else {
                result.localTime = context.elapsedTime * 0.72
                                   + static_cast<double>(role) * 4.0;
                result.intensity = saturate(0.25f + result.intensity * 0.45f
                                            + result.flowMagnitude * 0.30f);
                result.density = saturate(0.18f + result.density * 0.58f);
                result.rolePhase = fract(1.f - role + 0.125f * static_cast<float>(channel % 4));
            }
            break;
    }

    switch (context.stage) {
        case TemporalStage::Appearance:
            result.envelope = smooth(progress);
            result.density *= 0.35f + 0.65f * progress;
            if (context.previousState != nullptr) {
                const float carry = 1.f - smooth(progress);
                result.density = ofLerp(result.density,
                                        context.previousState->density, carry * 0.65f);
                result.intensity = ofLerp(result.intensity,
                                          context.previousState->intensity, carry * 0.45f);
                result.rolePhase = ofLerp(result.rolePhase,
                                          context.previousState->rolePhase, carry);
            }
            break;
        case TemporalStage::Development:
            result.envelope = 1.f;
            result.density = saturate(result.density + progress * 0.18f);
            break;
        case TemporalStage::Threshold:
            result.envelope = 0.82f + 0.18f * std::sin(progress * kPi);
            result.intensity = saturate(result.intensity + 0.25f + context.subdivisionPulse * 0.15f);
            break;
        case TemporalStage::Transformation:
            result.envelope = 1.f;
            result.rolePhase = fract(result.rolePhase + progress * 0.5f);
            result.localTime += progress * progress * 2.0;
            break;
        case TemporalStage::Dissolution:
            result.envelope = 1.f - smooth(progress);
            result.density *= 1.f - progress * 0.7f;
            break;
    }

    result.intensity = saturate(result.intensity);
    result.density = saturate(result.density);
    result.envelope = saturate(result.envelope);
    return result;
}

void VisualGenerator::renderScene(const GeneratorContext& context) {
    switch (mode_) {
        case GeneratorMode::RasterPulse:     renderRasterPulse(context); break;
        case GeneratorMode::BitMatrix:       renderBitMatrix(context); break;
        case GeneratorMode::ModularGrid:     renderModularGrid(context); break;
        case GeneratorMode::PhaseLines:      renderPhaseLines(context); break;
        case GeneratorMode::VectorField:     renderVectorField(context); break;
        case GeneratorMode::DataLedger:      renderDataLedger(context); break;
        case GeneratorMode::SignalTrace:     renderSignalTrace(context); break;
        case GeneratorMode::ThresholdBridge: renderThresholdBridge(context); break;
        case GeneratorMode::Pulse:           renderPulse(context); break;
        case GeneratorMode::BarScan:         renderBarScan(context); break;
        case GeneratorMode::GranularRaster:  renderGranularRaster(context); break;
        case GeneratorMode::OrbitalRings:    renderOrbitalRings(context); break;
        case GeneratorMode::Strobe:          renderStrobe(context); break;
        case GeneratorMode::DividedStrobe:   renderDividedStrobe(context); break;
        case GeneratorMode::AnalogNoise:     renderAnalogNoise(context); break;
    }
}

void VisualGenerator::renderRasterPulse(const GeneratorContext& context) {
    if (state_.observedGroup) {
        drawSource(context, 24.f * state_.envelope);
    }

    const int lines = std::max(12, static_cast<int>(24.f + state_.density * 150.f));
    const float beat = smooth(saturate(1.f - context.beatPhase * 3.f));
    const float travel = fract(static_cast<float>(state_.localTime) * 0.12f
                               + context.beatIndex * 0.03125f);

    ofFill();
    for (int i = 0; i < lines; ++i) {
        const float y = (static_cast<float>(i) + 0.5f) / lines * height_;
        const float n = noise2D(i * 0.21f,
                                static_cast<float>(state_.localTime) * 0.16f,
                                state_.resolvedSeed);
        const float pulse = std::exp(-std::abs(y / height_ - travel) * (10.f + 25.f * beat));
        const float alpha = (10.f + 80.f * n + 120.f * pulse * state_.intensity)
                            * state_.envelope;
        const float thickness = 1.f + pulse * (2.f + 10.f * state_.intensity);
        ofSetColor((i + context.beatIndex) % 7 == 0 ? accent(alpha) : foreground(alpha));
        ofDrawRectangle(0.f, y, static_cast<float>(width_), thickness);
    }

    const float scanX = fract(static_cast<float>(state_.localTime) * 0.08f
                              + state_.rolePhase) * width_;
    ofSetColor(accent(170.f * state_.envelope));
    ofDrawRectangle(scanX, 0.f, 1.f + 3.f * state_.intensity, static_cast<float>(height_));
}

void VisualGenerator::renderPulse(const GeneratorContext& context) {
    const float time = static_cast<float>(state_.localTime);
    const float tempo =
        1.8f + 0.22f * static_cast<float>((context.globalChannelIndex * 5) % 7);
    const float clock = time * tempo + state_.rolePhase * 5.f;
    const int step = static_cast<int>(std::floor(clock));
    const float stepPhase = fract(clock);
    // Exposición binaria: las barras están a amplitud plena la mayor parte del
    // paso y se sueltan rápido. El paso de feedback aporta la cola de decay.
    const float release = 1.f - smooth(saturate((stepPhase - 0.62f) * 5.f));
    const float strobe = stepPhase < 0.62f ? 1.f : release;
    const float inset = width_ * 0.085f;
    const float span = width_ - inset * 2.f;

    // Los huecos cubren toda la altura para que las barras salgan en cualquier
    // sitio; la ocupación dispersa deja grandes intervalos negros entre ellas.
    constexpr int kSlots = 9;
    const float hazeSalt = std::floor(time * 26.f) * 0.41f;
    drawAnalogHaze(context, hazeSalt);
    ofFill();
    for (int slot = 0; slot < kSlots; ++slot) {
        const uint32_t key = combineSeed(
            state_.resolvedSeed,
            static_cast<uint32_t>((step + 4096) * 13 + slot * 71));
        const float decision = hash01(key);
        if (decision < 0.55f)
            continue;

        const bool thick = hash01(combineSeed(key, 3u)) > 0.52f;
        const float thickness = thick
            ? height_ * (0.038f + hash01(combineSeed(key, 7u)) * 0.05f)
            : std::max(1.5f, height_ * (0.002f +
                hash01(combineSeed(key, 13u)) * 0.005f));
        const float centerY = height_ *
            ((slot + 0.5f) / kSlots +
             (hash01(combineSeed(key, 17u)) - 0.5f) * 0.07f);
        const float energy = saturate(strobe * (0.72f + state_.intensity * 0.28f));
        const float peak = (32.f + energy * 223.f) * state_.envelope;

        drawAnalogBar(context, inset, span, centerY - thickness * 0.5f,
                      thickness, peak, key, slot * 4.1f + hazeSalt);

        // Las barras gruesas llevan una línea interior más caliente; el halo
        // circundante viene del paso de blur, así que aquí no se pinta nada ancho.
        if (thick && energy > 0.45f) {
            ofSetColor(context.cyan, static_cast<int>(std::min(255.f, peak)));
            ofDrawRectangle(inset, centerY - thickness * 0.3f,
                            span, thickness * 0.6f);
        }
    }
}

void VisualGenerator::renderBarScan(const GeneratorContext& context) {
    const float time = static_cast<float>(state_.localTime);
    const int rails = ofClamp(
        static_cast<int>(30.f + state_.density * 62.f), 30, 92);
    const float spacing = static_cast<float>(height_) / rails;
    const float direction = (context.globalChannelIndex % 3 == 0) ? -1.f : 1.f;
    const float speed =
        direction * (0.06f + 0.015f * (context.globalChannelIndex % 5));
    const float front = fract(time * speed + state_.rolePhase);
    const float tempo =
        2.4f + 0.35f * static_cast<float>((context.globalChannelIndex * 3) % 5);
    const float clock = time * tempo + state_.rolePhase * 3.f;
    const int step = static_cast<int>(std::floor(clock));
    const float stepPhase = fract(clock);
    const float strobe = stepPhase < 0.58f
        ? 1.f
        : 1.f - smooth(saturate((stepPhase - 0.58f) * 4.f));
    // Los raíles conservan un tramo horizontal todo el momento; solo cambian
    // exposición, grosor y desplazamiento vertical.
    const float inset = width_ * 0.05f;
    const float span = width_ - inset * 2.f;
    const float timeSalt = std::floor(time * 26.f) * 0.41f;

    drawAnalogHaze(context, timeSalt);
    ofFill();
    for (int rail = 0; rail < rails; ++rail) {
        const float yn = (rail + 0.5f) / rails;
        const uint32_t key = combineSeed(
            state_.resolvedSeed,
            static_cast<uint32_t>((step + 4096) * 7 + rail * 53));
        const float stagger = noise1D(rail * 0.31f, state_.resolvedSeed);
        const float scanDistance =
            std::abs(fract(yn * 0.34f + front + stagger * 0.1f + 0.5f) - 0.5f);
        const float scan = std::exp(-scanDistance * scanDistance * 95.f);
        const float flicker = hash01(key);
        if (flicker < 0.44f - scan * 0.42f)
            continue;
        const float thickness = std::max(
            1.f, spacing * (0.14f + scan * 0.44f + flicker * 0.12f));
        const float peak = std::min(255.f,
            (8.f + 74.f * flicker + 214.f * scan) * strobe * state_.envelope);
        drawAnalogBar(context, inset, span, rail * spacing, thickness, peak,
                      key, timeSalt + rail * 0.13f);
    }
}

void VisualGenerator::renderGranularRaster(const GeneratorContext& context) {
    const int columns = ofClamp(
        static_cast<int>(24.f + state_.density * 38.f), 24, 62);
    const float cell = static_cast<float>(width_) / columns;
    const int rows = std::max(1, static_cast<int>(std::ceil(height_ / cell)));
    const float time = static_cast<float>(state_.localTime);
    const float sweep = fract(time * (0.055f + context.globalChannelIndex * 0.004f)
                              + state_.rolePhase);
    const int variant = static_cast<int>(state_.resolvedSeed % 4u);
    const float clock = time * (3.1f + state_.intensity * 2.2f)
                        + state_.rolePhase * 4.f;
    const int step = static_cast<int>(std::floor(clock));
    const float stepPhase = fract(clock);
    const float strobe = stepPhase < 0.52f
        ? 1.f
        : 1.f - smooth(saturate((stepPhase - 0.52f) * 3.4f));
    // Las inversiones de campo entero caen en límites de paso, de modo que el
    // raster se lee como exposición conmutada y no como un patrón animado continuo.
    const bool inverted =
        hash01(combineSeed(state_.resolvedSeed,
                           static_cast<uint32_t>(step + 4096))) > 0.86f;
    const float timeSalt = std::floor(time * 22.f) * 0.37f;

    drawAnalogHaze(context, timeSalt);
    ofFill();
    for (int row = 0; row < rows; ++row) {
        const float yn = (row + 0.5f) / rows;
        for (int column = 0; column < columns; ++column) {
            const float xn = (column + 0.5f) / columns;
            const float dx = xn - (0.5f + 0.24f * std::sin(time * 0.09f));
            const float dy = yn - (0.5f + 0.2f * std::cos(time * 0.07f));
            float field = 0.f;
            if (variant == 0)
                field = 1.f - std::abs(yn - sweep);
            else if (variant == 1)
                field = 1.f - std::abs(xn - sweep);
            else if (variant == 2)
                field = 1.f - glm::length(glm::vec2(dx, dy));
            else
                field = std::abs(std::sin((dx + dy) * kTwoPi * 1.5f
                                          + sweep * kTwoPi));
            const float grain = noise2D(column * 0.12f, row * 0.12f,
                                        state_.resolvedSeed);
            float density = saturate(field * 0.82f + grain * 0.32f
                                     - (variant == 2 ? 0.18f : 0.f));
            if (inverted)
                density = 1.f - density;
            if (density < 0.18f)
                continue;
            // Ruido de exposición por celda, más rápido que el barrido, mantiene
            // el campo granular en vez de plano.
            const float flicker = noise2D(column * 0.31f + timeSalt,
                                          row * 0.29f,
                                          combineSeed(state_.resolvedSeed, 71u));
            if (flicker < 0.16f)
                continue;
            const float quantized =
                std::floor(density * 7.f) / 7.f;
            const float size = cell * ofClamp(quantized, 0.08f, 0.88f);
            const float alpha = std::min(255.f,
                (18.f + quantized * 232.f) * (0.5f + flicker * 0.75f) *
                (0.42f + strobe * 0.58f) * state_.envelope);
            ofSetColor(systemColor(context,
                                   flicker < 0.34f ||
                                       (row / 8 + column / 8) % 5 == 0,
                                   alpha));
            ofDrawRectangle(column * cell + (cell - size) * 0.5f,
                            row * cell + (cell - size) * 0.5f, size, size);
        }
    }
}

void VisualGenerator::renderOrbitalRings(const GeneratorContext& context) {
    const float time = static_cast<float>(state_.localTime);
    const glm::vec2 center(
        width_ * (0.42f + 0.16f * std::sin(time * 0.07f + state_.rolePhase)),
        height_ * (0.48f + 0.1f * std::cos(time * 0.09f + state_.rolePhase)));
    const int rings = ofClamp(
        static_cast<int>(24.f + state_.density * 58.f), 24, 82);
    const int dashes = ofClamp(width_ / 9, 42, 150);
    const float maxRadius = glm::length(glm::vec2(width_, height_)) * 0.62f;
    const float clock = time * (2.6f + state_.intensity * 1.8f)
                        + state_.rolePhase * 4.f;
    const int step = static_cast<int>(std::floor(clock));
    const float stepPhase = fract(clock);
    const float strobe = stepPhase < 0.5f
        ? 1.f
        : 1.f - smooth(saturate((stepPhase - 0.5f) * 3.f));
    const float timeSalt = std::floor(time * 24.f) * 0.43f;
    // Los bursts encienden toda la familia de curvas a la vez; entre ellos solo
    // sobrevive un residuo fino, que el paso de feedback arrastra en estelas.
    const bool burst =
        hash01(combineSeed(state_.resolvedSeed,
                           static_cast<uint32_t>(step + 4096))) > 0.62f;

    drawAnalogHaze(context, timeSalt);
    ofNoFill();
    for (int ring = 1; ring <= rings; ++ring) {
        const float radius = maxRadius * ring / rings;
        const float phase = time * (0.045f + (ring % 7) * 0.005f)
                            + ring * 0.19f + state_.rolePhase * kTwoPi;
        const float ringNoise = noise1D(ring * 0.41f + timeSalt,
                                        combineSeed(state_.resolvedSeed, 97u));
        for (int dash = 0; dash < dashes; ++dash) {
            if ((dash + ring * 3 + context.globalChannelIndex) % 5 == 0)
                continue;
            const float grain = noise2D(dash * 0.17f, ring * 0.23f + timeSalt,
                                        combineSeed(state_.resolvedSeed, 131u));
            if (grain < (burst ? 0.12f : 0.46f))
                continue;
            const float a0 = kTwoPi * dash / dashes + phase;
            const float a1 = a0 + kTwoPi / dashes * 0.52f;
            const float wobble = 1.f + 0.035f * ringNoise;
            const float warp0 =
                radius * wobble * (1.f + 0.035f * std::sin(a0 * 5.f + time));
            const float warp1 =
                radius * wobble * (1.f + 0.035f * std::sin(a1 * 5.f + time));
            const glm::vec2 p0 = center + glm::vec2(std::cos(a0), std::sin(a0)) * warp0;
            const glm::vec2 p1 = center + glm::vec2(std::cos(a1), std::sin(a1)) * warp1;
            const float alpha = std::min(255.f,
                (26.f + 210.f * (1.f - static_cast<float>(ring) / rings))
                * (0.45f + grain * 0.8f) * (0.35f + strobe * 0.65f)
                * state_.envelope);
            ofSetColor(systemColor(context, grain < 0.34f || ring % 17 == 0,
                                   alpha));
            ofSetLineWidth(1.f + state_.intensity * 1.4f + grain * 1.1f);
            ofDrawLine(p0, p1);
        }
    }

    if (context.stage == TemporalStage::Threshold ||
        context.stage == TemporalStage::Transformation) {
        ofFill();
        const int marks = static_cast<int>(700.f + state_.density * 1500.f);
        for (int i = 0; i < marks; ++i) {
            uint32_t key = combineSeed(state_.resolvedSeed,
                                       static_cast<uint32_t>(i));
            const float x = hash01(key) * width_;
            const float y = hash01(combineSeed(key, 17u)) * height_;
            const float flicker = noise1D(i * 0.07f + time * 8.f, key);
            if (flicker < 0.58f)
                continue;
            ofSetColor(systemColor(context, flicker < 0.72f,
                                   (48.f + flicker * 120.f) * strobe
                                       * state_.envelope));
            ofDrawRectangle(x, y, 1.f + flicker * 1.5f,
                            1.f + flicker * 1.5f);
        }
    }
}

void VisualGenerator::renderBitMatrix(const GeneratorContext& context) {
    // Lluvia digital tipo Matrix: cada columna lleva uno o dos flujos de dígitos.
    // La cabeza del flujo es blanco brillante; los caracteres de cola se apagan a gris tenue.
    const int columns = ofClamp(static_cast<int>(8.f + state_.density * 28.f), 8, 36);
    const float cellW = static_cast<float>(width_) / columns;
    const int rows = static_cast<int>(std::ceil(static_cast<float>(height_) / cellW)) + 1;
    const float time = static_cast<float>(state_.localTime);

    ofFill();
    ofSetLineWidth(0.f);

    const int streamsPerColumn = state_.density > 0.55f ? 2 : 1;

    for (int col = 0; col < columns; ++col) {
        const uint32_t colSeed = combineSeed(state_.resolvedSeed,
                                             static_cast<uint32_t>(col * 13 + 1));
        for (int s = 0; s < streamsPerColumn; ++s) {
            const uint32_t sSeed = combineSeed(colSeed, static_cast<uint32_t>(s * 37 + 7));
            const float speed = (0.8f + hash01(combineSeed(sSeed, 0u)) * 2.4f)
                                * (0.4f + state_.intensity * 1.2f);
            const float startOffset = hash01(combineSeed(sSeed, 1u));
            const float trailLen = 3.f + hash01(combineSeed(sSeed, 2u))
                                   * 10.f * state_.density;

            // Posición fraccionaria de la cabeza en [0, rows)
            const float headF = fract(time * speed / rows + startOffset)
                                * static_cast<float>(rows);

            for (int r = 0; r < rows; ++r) {
                float behind = headF - static_cast<float>(r);
                if (behind < 0.f) behind += static_cast<float>(rows);
                if (behind > trailLen + 1.f) continue;

                const bool isHead = (behind < 1.f);
                const float trailFade = isHead
                    ? (1.f - behind)
                    : 1.f - (behind - 1.f) / trailLen;
                if (trailFade <= 0.f) continue;

                // El carácter parpadea en la cabeza y se mantiene estable en la cola
                const uint32_t timeTick = static_cast<uint32_t>(
                    std::max(0.0, state_.localTime) * 14.0);
                uint32_t cSeed = combineSeed(sSeed, static_cast<uint32_t>(r));
                if (isHead) cSeed = combineSeed(cSeed, timeTick);
                const int digit = static_cast<int>(hash(cSeed) % 10);

                const float x = col * cellW;
                const float y = r * cellW;

                if (isHead) {
                    ofSetColor(255, 255, 255,
                               static_cast<int>(255.f * state_.envelope));
                } else {
                    const float b = trailFade * trailFade;
                    ofSetColor(foreground(b * 200.f * state_.envelope));
                }

                if (fontReady_) {
                    matrixFont_.drawString(ofToString(digit),
                                          x + cellW * 0.08f,
                                          y + cellW * 0.88f);
                } else {
                    const float m = std::max(1.f, cellW * 0.1f);
                    ofDrawRectangle(x + m, y + m, cellW - m * 2.f, cellW - m * 2.f);
                }
            }
        }
    }

    // Pulso sutil de columna anclado al beat
    const int beatCol = static_cast<int>(
        context.beatPhase * static_cast<float>(columns)
        + static_cast<float>(context.beatIndex) * 3.7f)
        % columns;
    ofSetColor(accent(16.f * state_.envelope));
    ofDrawRectangle(static_cast<float>(beatCol) * cellW, 0.f,
                    cellW, static_cast<float>(height_));
}

void VisualGenerator::renderStrobe(const GeneratorContext& context) {
    // Strobe blanco a pantalla completa. La velocidad la marca state_.intensity:
    //   0.0 = 1 destello / 2 beats (lento)
    //   0.33 = 1 destello / beat (medio)
    //   0.67 = 2 destellos / beat (rápido)
    //   1.0 = 4 destellos / beat (muy rápido)
    // El reloj global de beat (beatIndex + beatPhase) se usa directo para que
    // las 8 franjas disparen en lock-step con independencia de OrganizationMode.
    const float totalBeats = static_cast<float>(context.beatIndex) + context.beatPhase;
    // Mapear intensity a: 0.5, 1, 2, 4 subdivisiones por beat.
    const float subdivisions = std::pow(2.f, std::round(state_.intensity * 3.f) - 1.f);
    const float subPhase = fract(totalBeats * subdivisions);

    // Destello duro: brillante el primer 15 % de cada subperiodo, luego corte instantáneo.
    static constexpr float kOnFraction = 0.15f;
    float brightness = 0.f;
    if (subPhase < kOnFraction) {
        const float t = subPhase / kOnFraction;
        brightness = (1.f - t) * (1.f - t);   // decay cuadrático
    }
    brightness *= state_.envelope;
    if (brightness < 0.004f) return;

    ofFill();
    ofSetColor(255, 255, 255, static_cast<int>(brightness * 255.f));
    ofDrawRectangle(0.f, 0.f, static_cast<float>(width_), static_cast<float>(height_));
}

void VisualGenerator::renderDividedStrobe(const GeneratorContext& context) {
    // Cada franja se divide en 4 zonas horizontales. La zona 0 dispara a 1x la tasa
    // base, zona 1 a 2x, zona 2 a 3x, zona 3 a 4x. El desfase entre
    // zonas crea un efecto de cascada rítmica.
    static constexpr int kZones = 4;
    const float zoneH = static_cast<float>(height_) / kZones;
    const float totalBeats = static_cast<float>(context.beatIndex) + context.beatPhase;
    // Subdivisión base: 1, 2 o 3 por beat, gobernada por intensity
    const float baseSubdiv = 1.f + std::round(state_.intensity * 2.f);

    ofFill();
    for (int z = 0; z < kZones; ++z) {
        const float zoneSubdiv = baseSubdiv * static_cast<float>(z + 1);
        // Pequeño desfase por zona para que no disparen todas a la vez
        const float phaseShift = static_cast<float>(z) * (0.25f / zoneSubdiv);
        const float subPhase = fract(totalBeats * zoneSubdiv + phaseShift);

        static constexpr float kOnFraction = 0.18f;
        float brightness = 0.f;
        if (subPhase < kOnFraction) {
            const float t = subPhase / kOnFraction;
            brightness = (1.f - t) * (1.f - t);
        }
        brightness *= state_.envelope;
        if (brightness < 0.004f) continue;

        ofSetColor(255, 255, 255, static_cast<int>(brightness * 255.f));
        ofDrawRectangle(0.f, static_cast<float>(z) * zoneH,
                        static_cast<float>(width_), zoneH);
    }
}

void VisualGenerator::renderAnalogNoise(const GeneratorContext& context) {
    // Ruido analógico a pantalla completa, renderizado entero en GPU vía el shader noise.
    // Todos los canales comparten el mismo tiempo de reloj de pared para que el
    // campo de ruido quede perfectamente sincronizado; el shader mapea el UV
    // local de cada canal a su franja de la coordenada global del muro, de modo
    // que los patrones a gran escala son continuos entre pantallas adyacentes.
    if (!noiseShaderReady_) {
        // Reserva: rellenar gris sólido si el shader no cargó
        ofFill();
        ofSetColor(foreground(80.f * state_.envelope));
        ofDrawRectangle(0.f, 0.f, static_cast<float>(width_), static_cast<float>(height_));
        return;
    }

    // Usar el tiempo de reloj de pared en bruto para que todos los canales vean
    // el mismo valor dentro de un fotograma, con independencia del modo de organización.
    const float t = static_cast<float>(ofGetElapsedTimef());

    ofFill();
    noiseShader_.begin();
    noiseShader_.setUniform2f("resolution",
                              static_cast<float>(width_),
                              static_cast<float>(height_));
    noiseShader_.setUniform1f("time",      t);
    noiseShader_.setUniform1f("intensity", state_.intensity);
    noiseShader_.setUniform1f("density",   state_.density);
    noiseShader_.setUniform1f("envelope",  state_.envelope);
    noiseShader_.setUniform1i("channelIndex", context.globalChannelIndex);
    noiseShader_.setUniform1i("channelCount",
                              std::max(1, context.globalChannelCount));
    ofDrawRectangle(0.f, 0.f,
                    static_cast<float>(width_), static_cast<float>(height_));
    noiseShader_.end();
}

void VisualGenerator::renderModularGrid(const GeneratorContext& context) {
    const int columns = ofClamp(3 + static_cast<int>(state_.density * 9.f), 3, 12);
    const int rows = ofClamp(3 + static_cast<int>(state_.density * 7.f), 3, 10);
    const float cw = static_cast<float>(width_) / columns;
    const float ch = static_cast<float>(height_) / rows;
    const float time = static_cast<float>(state_.localTime);

    ofNoFill();
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const int index = row * columns + column;
            const float n = hash01(combineSeed(state_.resolvedSeed,
                                               static_cast<uint32_t>(index)));
            const float oscillator = 0.5f + 0.5f * std::sin(
                time * (0.35f + n * 1.8f) + n * kTwoPi + context.beatPhase * kTwoPi);
            const float inset = (0.08f + oscillator * 0.32f) * std::min(cw, ch);
            const float alpha = (28.f + oscillator * 180.f * state_.intensity)
                                * state_.envelope;
            ofSetLineWidth(1.f + oscillator * 2.f);
            ofSetColor(index % 5 == context.beatIndex % 5 ? accent(alpha)
                                                          : foreground(alpha));
            ofDrawRectangle(column * cw + inset,
                            row * ch + inset,
                            std::max(1.f, cw - inset * 2.f),
                            std::max(1.f, ch - inset * 2.f));

            if (context.organization == OrganizationMode::Group4Plus4
                && !state_.observedGroup) {
                ofDrawLine(column * cw + inset,
                           row * ch + ch * 0.5f,
                           (column + 1) * cw - inset,
                           row * ch + ch * 0.5f);
            }
        }
    }
}

void VisualGenerator::renderPhaseLines(const GeneratorContext& context) {
    const int count = std::max(5, static_cast<int>(8.f + state_.density * 42.f));
    const int samples = std::max(48, width_ / 10);
    const float time = static_cast<float>(state_.localTime);

    ofNoFill();
    for (int line = 0; line < count; ++line) {
        const float lineNorm = count > 1 ? static_cast<float>(line) / (count - 1) : 0.5f;
        const float baseY = lineNorm * height_;
        const float seedPhase = hash01(combineSeed(state_.resolvedSeed,
                                                   static_cast<uint32_t>(line))) * kTwoPi;
        ofPolyline curve;
        for (int sample = 0; sample <= samples; ++sample) {
            const float xNorm = static_cast<float>(sample) / samples;
            const float x = xNorm * width_;
            const float wave = std::sin(xNorm * kTwoPi * (1.f + line % 4)
                                        + time * (0.5f + state_.rolePhase)
                                        + seedPhase);
            const float secondary = noise2D(xNorm * 4.f + line * 0.11f,
                                            time * 0.15f,
                                            state_.resolvedSeed) - 0.5f;
            const float amplitude = height_ / static_cast<float>(count)
                                    * (0.2f + 1.35f * state_.intensity);
            curve.addVertex(x, baseY + (wave * 0.65f + secondary) * amplitude);
        }
        const float phaseDistance = std::abs(fract(lineNorm - context.beatPhase + 0.5f) - 0.5f);
        const float alpha = (35.f + 150.f * (1.f - phaseDistance * 2.f))
                            * state_.envelope;
        ofSetColor(line % 6 == 0 ? accent(alpha) : foreground(alpha));
        ofSetLineWidth(1.f + state_.intensity * (line % 3 == 0 ? 2.f : 0.5f));
        curve.draw();
    }
}

void VisualGenerator::renderVectorField(const GeneratorContext& context) {
    const int columns = ofClamp(7 + static_cast<int>(state_.density * 17.f), 7, 24);
    const float spacing = static_cast<float>(width_) / columns;
    const int rows = std::max(3, static_cast<int>(height_ / spacing));
    const float time = static_cast<float>(state_.localTime);
    const float cvAngle = std::abs(state_.flowAngle) > kTwoPi
                              ? ofDegToRad(state_.flowAngle)
                              : state_.flowAngle;

    ofNoFill();
    for (int row = 0; row <= rows; ++row) {
        for (int column = 0; column <= columns; ++column) {
            const float x = (column + 0.5f) * spacing;
            const float y = (row + 0.5f) * spacing;
            const float field = noise2D(column * 0.23f + time * 0.07f,
                                        row * 0.23f - time * 0.05f,
                                        state_.resolvedSeed);
            float angle = field * kTwoPi * 2.f + state_.rolePhase * kTwoPi;
            angle = ofLerp(angle, cvAngle, state_.observedGroup ? state_.flowMagnitude * 0.65f : 0.1f);
            const float length = spacing * (0.18f + state_.intensity * 0.62f);
            const glm::vec2 direction(std::cos(angle), std::sin(angle));
            const glm::vec2 start(x, y);
            const glm::vec2 end = start + direction * length;
            const float alpha = (45.f + field * 145.f) * state_.envelope;
            ofSetColor((column + row + context.beatIndex) % 9 == 0
                           ? accent(alpha)
                           : foreground(alpha));
            ofSetLineWidth(1.f + state_.flowMagnitude * 2.f);
            ofDrawLine(start, end);
            const glm::vec2 sideA(std::cos(angle + 2.55f), std::sin(angle + 2.55f));
            const glm::vec2 sideB(std::cos(angle - 2.55f), std::sin(angle - 2.55f));
            ofDrawLine(end, end + sideA * length * 0.22f);
            ofDrawLine(end, end + sideB * length * 0.22f);
        }
    }
}

void VisualGenerator::renderDataLedger(const GeneratorContext& context) {
    const CVData* cv = context.cvData;
    const int rows = ofClamp(8 + static_cast<int>(state_.density * 24.f), 8, 32);
    const float rowHeight = static_cast<float>(height_) / rows;
    const int tick = static_cast<int>(std::floor(state_.localTime * 2.0));

    ofFill();
    for (int row = 0; row < rows; ++row) {
        const uint32_t key = combineSeed(state_.resolvedSeed,
                                         static_cast<uint32_t>(row + std::max(0, tick) * 37));
        const float value = hash01(key);
        const float barWidth = width_ * (0.08f + value * 0.74f)
                               * (0.45f + state_.intensity * 0.55f);
        const float y = row * rowHeight + rowHeight * 0.32f;
        const float alpha = (35.f + value * 125.f) * state_.envelope;
        ofSetColor(row % 7 == context.beatIndex % 7 ? accent(alpha)
                                                    : foreground(alpha));
        ofDrawRectangle(width_ * 0.22f, y, barWidth, std::max(1.f, rowHeight * 0.18f));

        ofSetColor(foreground(55.f * state_.envelope));
        ofDrawRectangle(0.f, row * rowHeight, width_, 1.f);
    }

    ofSetColor(foreground(210.f * state_.envelope));
    const int textY = 18;
    ofDrawBitmapString("CH " + ofToString(context.globalChannelIndex, 2, '0')
                           + "  SEG " + ofToString(context.segmentIndex, 3, '0'),
                       12, textY);
    ofDrawBitmapString("TIME " + fixedValue(static_cast<float>(state_.localTime), 2)
                           + "  BEAT " + ofToString(context.beatIndex),
                       12, textY + 15);
    ofDrawBitmapString("MOTION " + fixedValue(state_.motion)
                           + "  FLOW " + fixedValue(state_.flowMagnitude),
                       12, textY + 30);
    ofDrawBitmapString("BLOBS " + ofToString(cv != nullptr ? cv->blobCount : 0)
                           + "  STAGE " + ofToString(static_cast<int>(context.stage)),
                       12, textY + 45);
}

void VisualGenerator::renderSignalTrace(const GeneratorContext& context) {
    const int traces = ofClamp(2 + static_cast<int>(state_.density * 7.f), 2, 9);
    const int samples = std::max(80, width_ / 5);
    const float time = static_cast<float>(state_.localTime);
    const float band = static_cast<float>(height_) / traces;

    ofNoFill();
    for (int trace = 0; trace < traces; ++trace) {
        ofPolyline line;
        const float traceSeed = hash01(combineSeed(state_.resolvedSeed,
                                                   static_cast<uint32_t>(trace)));
        for (int sample = 0; sample <= samples; ++sample) {
            const float xNorm = static_cast<float>(sample) / samples;
            const float x = xNorm * width_;
            const float carrier = std::sin((xNorm * (2.f + trace % 5)
                                            - time * (0.12f + traceSeed * 0.3f))
                                           * kTwoPi);
            const float modulation = noise2D(xNorm * 7.f - time * 0.25f,
                                             trace * 0.8f,
                                             state_.resolvedSeed) - 0.5f;
            const float impulseCenter = fract(context.beatPhase
                                              + traceSeed * 0.15f
                                              + state_.rolePhase * 0.2f);
            const float distance = std::abs(xNorm - impulseCenter);
            const float impulse = std::exp(-distance * distance * 900.f)
                                  * context.subdivisionPulse;
            const float signal = carrier * 0.45f + modulation * 0.8f + impulse;
            line.addVertex(x,
                           band * (trace + 0.5f)
                               + signal * band * 0.38f * (0.35f + state_.intensity));
        }
        const float alpha = (75.f + traceSeed * 145.f) * state_.envelope;
        ofSetColor(trace == static_cast<int>(context.role) % traces
                       ? accent(alpha)
                       : foreground(alpha));
        ofSetLineWidth(1.f + (trace % 3 == 0 ? state_.intensity * 2.f : 0.f));
        line.draw();
    }
}

void VisualGenerator::renderThresholdBridge(const GeneratorContext& context) {
    if (state_.observedGroup) {
        drawSource(context, 18.f * state_.envelope);
    }

    const int bands = ofClamp(7 + static_cast<int>(state_.density * 25.f), 7, 32);
    const float time = static_cast<float>(state_.localTime);
    const float threshold = saturate(0.2f + state_.stageProgress * 0.6f
                                     + std::sin(time * 0.31f) * 0.08f);
    const float centerX = threshold * width_;

    ofFill();
    for (int band = 0; band < bands; ++band) {
        const float y0 = static_cast<float>(band) / bands * height_;
        const float y1 = static_cast<float>(band + 1) / bands * height_;
        const float n = noise2D(band * 0.18f,
                                time * 0.13f,
                                state_.resolvedSeed);
        const float left = std::max(0.f, centerX - width_ * n * 0.28f);
        const float right = std::min(static_cast<float>(width_),
                                     centerX + width_ * (1.f - n) * 0.28f);
        const bool crossing = n < state_.intensity;
        const float alpha = (crossing ? 165.f : 45.f) * state_.envelope;
        ofSetColor(crossing ? accent(alpha) : foreground(alpha));
        ofDrawRectangle(left, y0 + 1.f, std::max(1.f, right - left),
                        std::max(1.f, y1 - y0 - 2.f));
    }

    ofSetColor(foreground(230.f * state_.envelope));
    ofDrawRectangle(centerX, 0.f, 2.f + context.subdivisionPulse * 5.f,
                    static_cast<float>(height_));
    ofNoFill();
    ofSetLineWidth(2.f);
    ofSetColor(accent(190.f * state_.envelope));
    ofDrawRectangle(centerX - width_ * 0.04f,
                    height_ * (0.1f + 0.8f * context.beatPhase),
                    width_ * 0.08f,
                    height_ * 0.06f);
}

void VisualGenerator::drawSource(const GeneratorContext& context, float alpha) {
    if (context.sourceTexture == nullptr || !context.sourceTexture->isAllocated()
        || alpha <= 0.f) {
        return;
    }

    const float sourceWidth = context.sourceTexture->getWidth();
    const float sourceHeight = context.sourceTexture->getHeight();
    if (sourceWidth <= 0.f || sourceHeight <= 0.f) {
        return;
    }

    const float scale = std::max(width_ / sourceWidth, height_ / sourceHeight);
    const float drawWidth = sourceWidth * scale;
    const float drawHeight = sourceHeight * scale;
    ofSetColor(255, ofClamp(alpha, 0.f, 255.f));
    context.sourceTexture->draw((width_ - drawWidth) * 0.5f,
                                (height_ - drawHeight) * 0.5f,
                                drawWidth,
                                drawHeight);
}

void VisualGenerator::drawFrame(const GeneratorContext& context) {
    ofNoFill();
    const float alpha = 90.f * state_.envelope;
    ofSetColor(foreground(alpha));
    ofSetLineWidth(1.f);
    ofDrawRectangle(0.5f, 0.5f, width_ - 1.f, height_ - 1.f);

    const float marker = fract(static_cast<float>(state_.localTime) * 0.05f
                               + state_.rolePhase) * width_;
    ofSetColor(accent(140.f * state_.envelope));
    ofDrawLine(marker, height_ - 6.f, marker, static_cast<float>(height_));

    if (context.chapterDuration > 0.0) {
        const float chapterProgress = saturate(
            static_cast<float>(context.chapterTime / context.chapterDuration));
        ofFill();
        ofSetColor(foreground(100.f * state_.envelope));
        ofDrawRectangle(0.f, height_ - 2.f, width_ * chapterProgress, 2.f);
    }
}

ofColor VisualGenerator::foreground(float alpha) const {
    return ofColor(238, 238, 238, static_cast<unsigned char>(ofClamp(alpha, 0.f, 255.f)));
}

ofColor VisualGenerator::accent(float alpha) const {
    const unsigned char a = static_cast<unsigned char>(ofClamp(alpha, 0.f, 255.f));
    const int selector = static_cast<int>(state_.resolvedSeed % 3u);
    if (selector == 0) {
        return ofColor(238, 238, 238, a);
    }
    if (selector == 1) {
        return ofColor(154, 154, 154, a);
    }
    return ofColor(92, 92, 92, a);
}

void VisualGenerator::draw(float x, float y) const {
    if (fbo_.isAllocated()) {
        fbo_.draw(x, y);
    }
}

void VisualGenerator::draw(float x, float y, float width, float height) const {
    if (fbo_.isAllocated()) {
        fbo_.draw(x, y, width, height);
    }
}

uint32_t VisualGenerator::hash(uint32_t value) {
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float VisualGenerator::hash01(uint32_t value) {
    return static_cast<float>(hash(value) & 0x00ffffffu)
           / static_cast<float>(0x01000000u);
}

float VisualGenerator::noise1D(float x, uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const float fraction = fract(x);
    const float interpolation = smooth(fraction);
    const float a = hash01(combineSeed(seed, static_cast<uint32_t>(x0)));
    const float b = hash01(combineSeed(seed, static_cast<uint32_t>(x0 + 1)));
    return ofLerp(a, b, interpolation);
}

float VisualGenerator::noise2D(float x, float y, uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float tx = smooth(fract(x));
    const float ty = smooth(fract(y));

    const auto lattice = [seed](int lx, int ly) {
        uint32_t key = combineSeed(seed, static_cast<uint32_t>(lx));
        key = combineSeed(key, static_cast<uint32_t>(ly));
        return hash01(key);
    };

    const float a = ofLerp(lattice(x0, y0), lattice(x0 + 1, y0), tx);
    const float b = ofLerp(lattice(x0, y0 + 1), lattice(x0 + 1, y0 + 1), tx);
    return ofLerp(a, b, ty);
}

float VisualGenerator::saturate(float value) {
    return ofClamp(value, 0.f, 1.f);
}

float VisualGenerator::smooth(float value) {
    value = saturate(value);
    return value * value * (3.f - 2.f * value);
}

float VisualGenerator::roleValue(ScreenRole role) {
    return static_cast<float>(static_cast<int>(role)) / 8.f;
}
