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
} // namespace

void VisualGenerator::setup(int width, int height) {
    ensureAllocated(width, height);
}

void VisualGenerator::ensureAllocated(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (fbo_.isAllocated() && width == width_ && height == height_) {
        return;
    }

    width_ = width;
    height_ = height;

    ofFbo::Settings settings;
    settings.width = width_;
    settings.height = height_;
    settings.internalformat = GL_RGBA8;
    settings.useDepth = false;
    settings.useStencil = false;
    settings.numSamples = 0;
    settings.textureTarget = GL_TEXTURE_2D;
    fbo_.allocate(settings);

    fbo_.begin();
    ofClear(0, 0, 0, 255);
    fbo_.end();
}

void VisualGenerator::update(const GeneratorContext& context) {
    render(context);
}

void VisualGenerator::render(const GeneratorContext& context) {
    const int requestedWidth = context.width > 0 ? context.width : width_;
    const int requestedHeight = context.height > 0 ? context.height : height_;
    ensureAllocated(requestedWidth, requestedHeight);
    state_ = resolveState(context);

    fbo_.begin();
    ofPushStyle();
    ofPushMatrix();
    ofClear(4, 5, 7, 255);
    ofEnableAlphaBlending();
    ofSetLineWidth(1.f);
    ofNoFill();

    renderScene(context);
    drawFrame(context);

    ofPopMatrix();
    ofPopStyle();
    fbo_.end();
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

void VisualGenerator::renderBitMatrix(const GeneratorContext& context) {
    const int columns = ofClamp(static_cast<int>(12.f + state_.density * 44.f), 8, 64);
    const float cell = static_cast<float>(width_) / columns;
    const int rows = std::max(1, static_cast<int>(std::ceil(height_ / cell)));
    const int tick = static_cast<int>(std::floor(state_.localTime * (3.0 + state_.intensity * 9.0)));
    const int threshold = static_cast<int>(50.f + state_.density * 155.f);

    ofFill();
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            uint32_t key = state_.resolvedSeed;
            key = combineSeed(key, static_cast<uint32_t>(column + row * columns));
            key = combineSeed(key, static_cast<uint32_t>(std::max(0, tick)));
            const int value = static_cast<int>(hash(key) & 255u);
            if (value > threshold) {
                continue;
            }

            const float roleBand = fract(static_cast<float>(column) / columns
                                         + state_.rolePhase);
            const bool highlighted = std::abs(roleBand - context.beatPhase) < 0.08f;
            const float margin = std::max(1.f, cell * 0.16f);
            ofSetColor(highlighted ? accent(220.f * state_.envelope)
                                   : foreground((55.f + value * 0.45f) * state_.envelope));
            ofDrawRectangle(column * cell + margin,
                            row * cell + margin,
                            std::max(1.f, cell - margin * 2.f),
                            std::max(1.f, cell - margin * 2.f));
        }
    }
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
    return ofColor(228, 234, 238, static_cast<unsigned char>(ofClamp(alpha, 0.f, 255.f)));
}

ofColor VisualGenerator::accent(float alpha) const {
    const unsigned char a = static_cast<unsigned char>(ofClamp(alpha, 0.f, 255.f));
    if (!state_.observedGroup) {
        return ofColor(255, 76, 62, a);
    }
    const int selector = static_cast<int>(state_.resolvedSeed % 3u);
    if (selector == 0) {
        return ofColor(32, 184, 255, a);
    }
    if (selector == 1) {
        return ofColor(255, 52, 72, a);
    }
    return ofColor(238, 242, 246, a);
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
