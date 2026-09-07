#pragma once

#include "IVisualGenerator.h"
#include "pdjv/PdjvRuntime.h"
#include <memory>
#include <string>

class PdjvOptionalBridge {
public:
    void setup(const std::string& packagePath);
    void reset();
    bool sync(double playbackSeconds, PdjvFrameView& outView,
              ofTexture& maskTex, ofTexture& depthTex);
    bool available() const { return reader_ && reader_->error().empty(); }
    PointCloudDepthSource effectiveDepthSource(PointCloudDepthSource requested) const;

private:
    std::shared_ptr<pdjv::PdjvReader> reader_;
    std::shared_ptr<pdjv::TimelinePlayer> timeline_;
    pdjv::FrameRecord frame_;
    ofPixels maskPixels_;
    ofPixels depthPixels_;
    bool maskReady_ = false;
    bool depthReady_ = false;
};
