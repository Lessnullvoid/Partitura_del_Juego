#pragma once

#include "ofMain.h"
#include <CoreGraphics/CoreGraphics.h>
#include <string>
#include <vector>

namespace DisplayProbe {

constexpr std::size_t kIcuixianInputWidth = 1920;
constexpr std::size_t kIcuixianInputHeight = 1080;
constexpr double kIcuixianRefreshHz = 60.0;

enum class Connector {
    Builtin,
    Hdmi,
    Thunderbolt,
    Unknown
};

enum class WallPreset {
    Mixed,          // A 4x1/90, B 2x2/0
    DualPortrait    // both 4x1/90
};

const char* connectorName(Connector connector);

struct DisplayInfo {
    CGDirectDisplayID id = kCGNullDirectDisplay;
    CGRect bounds = CGRectZero;
    std::size_t pixelWidth = 0;
    std::size_t pixelHeight = 0;
    bool builtin = false;
    bool mirrored = false;
    bool is4K = false;
    bool currently1080 = false;
    bool hasIcuixianMode = false;
    Connector connector = Connector::Unknown;
    uint32_t vendor = 0;
    uint32_t product = 0;
    uint32_t serial = 0;
    std::string registryHint;
};

struct WallPair {
    DisplayInfo wallA;
    DisplayInfo wallB;
    bool assignedByConnector = false;
    bool needsModeSwitch = false;
    bool valid = false;
};

std::vector<DisplayInfo> listDisplays();
std::vector<DisplayInfo> listExternalDisplays();

// Safe launch assignment: HDMI+Thunderbolt 1080p, else two current 1080p
// externals left-to-right. Never treats a live 4K studio monitor as a wall
// unless it is one of exactly two externals that are HDMI+TB and both expose
// 1080p60.
WallPair pickLaunchPair();

// Operator button: two externals that can switch to 1080p60. Prefers
// Thunderbolt=A / HDMI=B, else the two largest candidates left-to-right.
WallPair pickConfigurePair();

bool switchPairToIcuixianMode(WallPair& pair, std::string* error);
void refreshBounds(WallPair& pair);

void applyPairToSettings(ofJson& cfg, const WallPair& pair, WallPreset preset);
bool savePair(ofJson& cfg, const WallPair& pair, WallPreset preset,
              std::string* error);

std::string describePair(const WallPair& pair, WallPreset preset);

}  // namespace DisplayProbe
