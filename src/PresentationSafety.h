#pragma once

#include <atomic>
#include <cstdint>

// Operator kill-switch: hide video on landscape (2x2) walls only.
// Portrait walls keep playing. Generators/breath on the landscape wall stay.
struct PresentationSafety {
    std::atomic<bool> hideLandscapeVideo{false};
    std::atomic<std::uint32_t> landscapeMask{0};

    bool active() const { return hideLandscapeVideo.load(); }

    bool isLandscapeChannel(int idx) const {
        if (idx < 0 || idx >= 32) return false;
        return (landscapeMask.load() & (1u << idx)) != 0;
    }

    bool hidesChannel(int idx) const {
        return active() && isLandscapeChannel(idx);
    }

    void setLandscapeChannel(int idx, bool landscape) {
        if (idx < 0 || idx >= 32) return;
        const std::uint32_t bit = 1u << static_cast<std::uint32_t>(idx);
        if (landscape) landscapeMask.fetch_or(bit);
        else landscapeMask.fetch_and(~bit);
    }
};
