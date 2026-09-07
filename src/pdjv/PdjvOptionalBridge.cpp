#include "PdjvOptionalBridge.h"
#include <algorithm>
#include <cstring>

void PdjvOptionalBridge::setup(const std::string& packagePath) {
    reset();
    if (packagePath.empty())
        return;
    const std::string resolved = ofToDataPath(packagePath, true);
    if (!ofFile::doesFileExist(resolved + "/manifest.json"))
        return;
    reader_ = std::make_shared<pdjv::PdjvReader>();
    if (!reader_->open(resolved) || !reader_->error().empty()) {
        reader_.reset();
        return;
    }
    timeline_ = std::make_shared<pdjv::TimelinePlayer>();
    timeline_->bind(reader_, std::make_shared<pdjv::FrameCache>());
    timeline_->setLoop(true);
    timeline_->play();
}

void PdjvOptionalBridge::reset() {
    if (timeline_)
        timeline_->stop();
    timeline_.reset();
    reader_.reset();
    maskReady_ = false;
    depthReady_ = false;
}

PointCloudDepthSource PdjvOptionalBridge::effectiveDepthSource(
    PointCloudDepthSource requested) const {
    if (!available())
        return PointCloudDepthSource::Luminance;
    return requested;
}

bool PdjvOptionalBridge::sync(double playbackSeconds, PdjvFrameView& outView,
                              ofTexture& maskTex, ofTexture& depthTex) {
    outView = {};
    if (!timeline_ || !reader_)
        return false;
    timeline_->seek(playbackSeconds);
    timeline_->update(0);
    pdjv::FrameRecord nearest;
    if (!timeline_->current(frame_, nearest))
        return false;
    if (frame_.players.empty())
        return false;
    const pdjv::PlayerObservation* obs = &frame_.players[0];
    for (const auto& p : frame_.players) {
        if (p.observed) {
            obs = &p;
            break;
        }
    }
    outView.valid = obs->observed;
    outView.maskWidth = obs->maskWidth;
    outView.maskHeight = obs->maskHeight;
    outView.depthWidth = obs->depthWidth;
    outView.depthHeight = obs->depthHeight;
    std::memcpy(outView.playerBBox, obs->boundingBox, sizeof(outView.playerBBox));
    if (obs->maskWidth > 0 && obs->maskHeight > 0) {
        const size_t n = static_cast<size_t>(obs->maskWidth) * obs->maskHeight;
        if (obs->maskOffset + n <= reader_->masks().size()) {
            outView.maskData = reader_->masks().data() + obs->maskOffset;
            if (!maskReady_ || maskPixels_.getWidth() != static_cast<int>(obs->maskWidth) ||
                maskPixels_.getHeight() != static_cast<int>(obs->maskHeight)) {
                maskPixels_.allocate(obs->maskWidth, obs->maskHeight, OF_PIXELS_GRAY);
                maskReady_ = true;
            }
            for (size_t i = 0; i < n; ++i)
                maskPixels_.getData()[i] = outView.maskData[i];
            maskTex.loadData(maskPixels_);
        }
    }
    if (obs->depthWidth > 0 && obs->depthHeight > 0) {
        const size_t n = static_cast<size_t>(obs->depthWidth) * obs->depthHeight;
        if (obs->depthOffset + n * sizeof(uint16_t) <= reader_->depths().size()) {
            outView.depthData = reader_->depths().data() + obs->depthOffset;
            if (!depthReady_ || depthPixels_.getWidth() != static_cast<int>(obs->depthWidth) ||
                depthPixels_.getHeight() != static_cast<int>(obs->depthHeight)) {
                depthPixels_.allocate(obs->depthWidth, obs->depthHeight, OF_PIXELS_GRAY);
                depthReady_ = true;
            }
            for (size_t i = 0; i < n; ++i) {
                uint16_t raw = 0;
                std::memcpy(&raw, outView.depthData + i * sizeof(uint16_t), sizeof(raw));
                depthPixels_.getData()[i] = static_cast<unsigned char>(raw / 257);
            }
            depthTex.loadData(depthPixels_);
        }
    }
    return outView.valid || (obs->maskWidth > 0 || obs->depthWidth > 0);
}
