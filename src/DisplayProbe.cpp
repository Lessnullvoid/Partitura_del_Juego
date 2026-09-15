#include "DisplayProbe.h"
#include "SettingsStore.h"

#include <IOKit/IOKitLib.h>
#include <algorithm>
#include <cmath>
#include <cctype>

namespace DisplayProbe {
namespace {

std::string toLowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool hasIcuixianDesktopSize(const CGRect& bounds) {
    return static_cast<int>(bounds.size.width) ==
               static_cast<int>(kIcuixianInputWidth) &&
           static_cast<int>(bounds.size.height) ==
               static_cast<int>(kIcuixianInputHeight);
}

CGDisplayModeRef copyIcuixianInputMode(CGDirectDisplayID displayID) {
    CFArrayRef modes = CGDisplayCopyAllDisplayModes(displayID, nullptr);
    if (!modes) return nullptr;

    CGDisplayModeRef best = nullptr;
    double bestRefreshError = 1000.0;
    const CFIndex count = CFArrayGetCount(modes);
    for (CFIndex i = 0; i < count; ++i) {
        auto mode = static_cast<CGDisplayModeRef>(
            const_cast<void*>(CFArrayGetValueAtIndex(modes, i)));
        if (CGDisplayModeGetPixelWidth(mode) != kIcuixianInputWidth ||
            CGDisplayModeGetPixelHeight(mode) != kIcuixianInputHeight ||
            CGDisplayModeGetWidth(mode) != kIcuixianInputWidth ||
            CGDisplayModeGetHeight(mode) != kIcuixianInputHeight) {
            continue;
        }

        const double refresh = CGDisplayModeGetRefreshRate(mode);
        const double refreshError =
            refresh > 0.0 ? std::abs(refresh - kIcuixianRefreshHz) : 0.5;
        if (refreshError <= 1.0 && refreshError < bestRefreshError) {
            best = mode;
            bestRefreshError = refreshError;
        }
    }

    if (best) CFRetain(best);
    CFRelease(modes);
    return best;
}

bool leftOf(const DisplayInfo& a, const DisplayInfo& b) {
    if (a.bounds.origin.x != b.bounds.origin.x)
        return a.bounds.origin.x < b.bounds.origin.x;
    return a.bounds.origin.y < b.bounds.origin.y;
}

Connector classifyHint(const std::string& hint, bool builtin) {
    if (builtin) return Connector::Builtin;
    const std::string lower = toLowerCopy(hint);
    if (lower.find("hdmi") != std::string::npos)
        return Connector::Hdmi;
    if (lower.find("thunderbolt") != std::string::npos ||
        lower.find("typec") != std::string::npos ||
        lower.find("type-c") != std::string::npos ||
        lower.find("/atc") != std::string::npos ||
        lower.find("atc0") != std::string::npos ||
        lower.find("atc1") != std::string::npos ||
        lower.find("atc2") != std::string::npos ||
        lower.find("atc3") != std::string::npos ||
        lower.find("dptx") != std::string::npos ||
        lower.find("displayport") != std::string::npos ||
        lower.find("dispext") != std::string::npos)
        return Connector::Thunderbolt;
    return Connector::Unknown;
}

std::string ancestryText(io_registry_entry_t entry) {
    std::string text;
    io_registry_entry_t current = entry;
    IOObjectRetain(current);
    for (int depth = 0; depth < 24 && current != IO_OBJECT_NULL; ++depth) {
        io_name_t name{};
        io_name_t cls{};
        io_string_t path{};
        IORegistryEntryGetName(current, name);
        IOObjectGetClass(current, cls);
        if (IORegistryEntryGetPath(current, kIOServicePlane, path) == KERN_SUCCESS) {
            text += ' ';
            text += path;
        }
        text += ' ';
        text += name;
        text += ' ';
        text += cls;

        io_registry_entry_t parent = IO_OBJECT_NULL;
        const kern_return_t parentResult =
            IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent);
        IOObjectRelease(current);
        current = IO_OBJECT_NULL;
        if (parentResult != KERN_SUCCESS) break;
        current = parent;
    }
    if (current != IO_OBJECT_NULL) IOObjectRelease(current);
    return text;
}

bool readNumber(CFDictionaryRef dict, CFStringRef key, uint32_t* out) {
    if (!dict || !out) return false;
    const CFTypeRef value = CFDictionaryGetValue(dict, key);
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) return false;
    int64_t number = 0;
    if (!CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberSInt64Type,
                          &number)) {
        return false;
    }
    *out = static_cast<uint32_t>(number);
    return true;
}

struct RegistryNode {
    uint32_t vendor = 0;
    uint32_t product = 0;
    uint32_t serial = 0;
    bool hasIdentity = false;
    std::string hint;
};

void harvestNode(io_registry_entry_t entry, std::vector<RegistryNode>& nodes) {
    RegistryNode node;
    node.hint = ancestryText(entry);

    CFTypeRef attrs = IORegistryEntryCreateCFProperty(
        entry, CFSTR("DisplayAttributes"), kCFAllocatorDefault, 0);
    if (attrs && CFGetTypeID(attrs) == CFDictionaryGetTypeID()) {
        const CFDictionaryRef attrDict = static_cast<CFDictionaryRef>(attrs);
        const CFTypeRef products = CFDictionaryGetValue(attrDict, CFSTR("ProductAttributes"));
        if (products && CFGetTypeID(products) == CFDictionaryGetTypeID()) {
            const CFDictionaryRef productDict = static_cast<CFDictionaryRef>(products);
            const bool gotVendor =
                readNumber(productDict, CFSTR("LegacyManufacturerID"), &node.vendor) ||
                readNumber(productDict, CFSTR("ManufacturerID"), &node.vendor);
            const bool gotProduct =
                readNumber(productDict, CFSTR("ProductID"), &node.product);
            readNumber(productDict, CFSTR("SerialNumber"), &node.serial);
            node.hasIdentity = gotVendor || gotProduct;
        }
    }
    if (attrs) CFRelease(attrs);

    CFTypeRef location = IORegistryEntryCreateCFProperty(
        entry, CFSTR("Location"), kCFAllocatorDefault, 0);
    if (location && CFGetTypeID(location) == CFStringGetTypeID()) {
        char loc[256] = {0};
        CFStringGetCString(static_cast<CFStringRef>(location), loc, sizeof(loc),
                           kCFStringEncodingUTF8);
        node.hint += " Location=";
        node.hint += loc;
    }
    if (location) CFRelease(location);

    nodes.push_back(std::move(node));
}

std::vector<RegistryNode> collectRegistryNodes() {
    std::vector<RegistryNode> nodes;
    const char* classes[] = {
        "AppleCLCD2",
        "IOMobileFramebufferShim",
        "IOMobileFramebuffer",
        "IODisplayConnect",
        "DCPAVServiceProxy"
    };

#if defined(kIOMainPortDefault)
    const mach_port_t masterPort = kIOMainPortDefault;
#else
    const mach_port_t masterPort = kIOMasterPortDefault;
#endif

    for (const char* className : classes) {
        io_iterator_t iterator = IO_OBJECT_NULL;
        if (IOServiceGetMatchingServices(
                masterPort, IOServiceMatching(className), &iterator) !=
            KERN_SUCCESS) {
            continue;
        }
        io_service_t service = IO_OBJECT_NULL;
        while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
            harvestNode(service, nodes);
            IOObjectRelease(service);
        }
        IOObjectRelease(iterator);
    }
    return nodes;
}

void classifyDisplay(DisplayInfo& display, const std::vector<RegistryNode>& nodes) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    const io_service_t port = CGDisplayIOServicePort(display.id);
#pragma clang diagnostic pop
    if (port != IO_OBJECT_NULL) {
        display.registryHint = ancestryText(port);
        display.connector = classifyHint(display.registryHint, display.builtin);
        if (display.connector != Connector::Unknown) return;
    }

    int bestScore = -1;
    const RegistryNode* best = nullptr;
    for (const auto& node : nodes) {
        if (!node.hasIdentity) continue;
        int score = 0;
        if (node.vendor == display.vendor) score += 2;
        if (node.product == display.product) score += 2;
        if (display.serial != 0 && node.serial == display.serial) score += 3;
        if (score <= 2) continue;
        if (score > bestScore) {
            bestScore = score;
            best = &node;
        }
    }
    if (!best) return;
    display.registryHint = best->hint;
    if (display.connector == Connector::Unknown)
        display.connector = classifyHint(best->hint, display.builtin);
}

DisplayInfo makeDisplayInfo(CGDirectDisplayID id,
                            const std::vector<RegistryNode>& nodes) {
    DisplayInfo display;
    display.id = id;
    display.bounds = CGDisplayBounds(id);
    display.pixelWidth = CGDisplayPixelsWide(id);
    display.pixelHeight = CGDisplayPixelsHigh(id);
    display.builtin = CGDisplayIsBuiltin(id) != 0;
    display.mirrored = CGDisplayMirrorsDisplay(id) != kCGNullDirectDisplay;
    display.is4K = display.pixelWidth >= 3840;
    display.currently1080 = hasIcuixianDesktopSize(display.bounds);
    display.vendor = CGDisplayVendorNumber(id);
    display.product = CGDisplayModelNumber(id);
    display.serial = CGDisplaySerialNumber(id);

    CGDisplayModeRef mode = copyIcuixianInputMode(id);
    display.hasIcuixianMode = mode != nullptr;
    if (mode) CFRelease(mode);

    classifyDisplay(display, nodes);
    if (display.builtin) display.connector = Connector::Builtin;
    return display;
}

ofJson defaultController() {
    return {
        {"brand", "ICUIXIAN"},
        {"model", "0104-XZ"},
        {"asin", "B0DM98NVSH"},
        {"controllers", 2},
        {"inputWidth", kIcuixianInputWidth},
        {"inputHeight", kIcuixianInputHeight},
        {"refreshHz", kIcuixianRefreshHz}
    };
}

ofJson wallMeta(const std::string& layout, int rotationDegrees,
                const std::string& orientation, Connector connector) {
    return {
        {"layout", layout},
        {"layoutPerController", layout},
        {"rotationDegrees", rotationDegrees},
        {"panelOrientation", orientation},
        {"panels", 4},
        {"connector", connectorName(connector)}
    };
}

ofJson windowJson(const DisplayInfo& display, const std::string& layout) {
    return {
        {"x", static_cast<int>(display.bounds.origin.x)},
        {"y", static_cast<int>(display.bounds.origin.y)},
        {"width", static_cast<int>(display.bounds.size.width)},
        {"height", static_cast<int>(display.bounds.size.height)},
        {"layout", layout}
    };
}

DisplayInfo* findByConnector(std::vector<DisplayInfo>& displays,
                             Connector connector) {
    for (auto& display : displays) {
        if (display.connector == connector && display.hasIcuixianMode)
            return &display;
    }
    return nullptr;
}

}  // namespace

const char* connectorName(Connector connector) {
    switch (connector) {
        case Connector::Builtin: return "builtin";
        case Connector::Hdmi: return "hdmi";
        case Connector::Thunderbolt: return "thunderbolt";
        case Connector::Unknown: return "unknown";
    }
    return "unknown";
}

std::vector<DisplayInfo> listDisplays() {
    uint32_t count = 0;
    if (CGGetActiveDisplayList(0, nullptr, &count) != kCGErrorSuccess ||
        count == 0) {
        return {};
    }
    std::vector<CGDirectDisplayID> ids(count);
    if (CGGetActiveDisplayList(count, ids.data(), &count) != kCGErrorSuccess) {
        return {};
    }
    const std::vector<RegistryNode> nodes = collectRegistryNodes();
    std::vector<DisplayInfo> displays;
    displays.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        displays.push_back(makeDisplayInfo(ids[i], nodes));
    return displays;
}

std::vector<DisplayInfo> listExternalDisplays() {
    std::vector<DisplayInfo> external;
    for (auto& display : listDisplays()) {
        if (!display.builtin && !display.mirrored)
            external.push_back(std::move(display));
    }
    return external;
}

WallPair pickLaunchPair() {
    WallPair pair;
    std::vector<DisplayInfo> external = listExternalDisplays();
    if (external.size() < 2) return pair;

    std::vector<DisplayInfo> hdmi1080;
    std::vector<DisplayInfo> thunderbolt1080;
    std::vector<DisplayInfo> all1080;
    for (const auto& display : external) {
        if (!display.currently1080) continue;
        all1080.push_back(display);
        if (display.connector == Connector::Hdmi) hdmi1080.push_back(display);
        if (display.connector == Connector::Thunderbolt)
            thunderbolt1080.push_back(display);
    }

    if (!thunderbolt1080.empty() && !hdmi1080.empty()) {
        pair.wallA = thunderbolt1080.front();
        pair.wallB = hdmi1080.front();
        pair.assignedByConnector = true;
        pair.valid = true;
        return pair;
    }

    if (external.size() == 2) {
        DisplayInfo* hdmi = findByConnector(external, Connector::Hdmi);
        DisplayInfo* thunderbolt =
            findByConnector(external, Connector::Thunderbolt);
        if (hdmi && thunderbolt && hdmi->id != thunderbolt->id) {
            pair.wallA = *thunderbolt;
            pair.wallB = *hdmi;
            pair.assignedByConnector = true;
            pair.needsModeSwitch =
                !hdmi->currently1080 || !thunderbolt->currently1080;
            pair.valid = true;
            return pair;
        }
    }

    if (all1080.size() >= 2) {
        std::sort(all1080.begin(), all1080.end(), leftOf);
        pair.wallA = all1080[0];
        pair.wallB = all1080[1];
        pair.assignedByConnector = false;
        pair.valid = true;
        return pair;
    }
    return pair;
}

WallPair pickConfigurePair() {
    WallPair pair;
    std::vector<DisplayInfo> external = listExternalDisplays();
    if (external.size() < 2) return pair;

    DisplayInfo* hdmi = findByConnector(external, Connector::Hdmi);
    DisplayInfo* thunderbolt = findByConnector(external, Connector::Thunderbolt);
    if (hdmi && thunderbolt && hdmi->id != thunderbolt->id) {
        pair.wallA = *thunderbolt;
        pair.wallB = *hdmi;
        pair.assignedByConnector = true;
        pair.needsModeSwitch = true;
        pair.valid = true;
        return pair;
    }

    std::vector<DisplayInfo> candidates;
    for (const auto& display : external) {
        if (display.hasIcuixianMode) candidates.push_back(display);
    }
    if (candidates.size() < 2) return pair;

    std::sort(candidates.begin(), candidates.end(),
              [](const DisplayInfo& a, const DisplayInfo& b) {
                  if (a.is4K != b.is4K) return a.is4K > b.is4K;
                  return (a.pixelWidth * a.pixelHeight) >
                         (b.pixelWidth * b.pixelHeight);
              });
    candidates.resize(2);
    std::sort(candidates.begin(), candidates.end(), leftOf);
    pair.wallA = candidates[0];
    pair.wallB = candidates[1];
    pair.assignedByConnector = false;
    pair.needsModeSwitch = true;
    pair.valid = true;
    return pair;
}

bool switchPairToIcuixianMode(WallPair& pair, std::string* error) {
    if (!pair.valid) {
        if (error) *error = "Two active displays were not detected";
        return false;
    }

    const CGDirectDisplayID ids[2] = {pair.wallA.id, pair.wallB.id};
    std::vector<CGDisplayModeRef> modes;
    modes.reserve(2);
    for (const CGDirectDisplayID id : ids) {
        CGDisplayModeRef mode = copyIcuixianInputMode(id);
        if (!mode) {
            for (const auto retained : modes) CFRelease(retained);
            if (error)
                *error = "ICUIXIAN 1920x1080 @ 60 Hz mode is unavailable on an input";
            return false;
        }
        modes.push_back(mode);
    }

    CGDisplayConfigRef displayConfig = nullptr;
    CGError modeResult = CGBeginDisplayConfiguration(&displayConfig);
    for (std::size_t i = 0;
         modeResult == kCGErrorSuccess && i < modes.size(); ++i) {
        modeResult = CGConfigureDisplayWithDisplayMode(
            displayConfig, ids[i], modes[i], nullptr);
    }
    if (modeResult == kCGErrorSuccess) {
        modeResult = CGCompleteDisplayConfiguration(
            displayConfig, kCGConfigurePermanently);
    } else if (displayConfig) {
        CGCancelDisplayConfiguration(displayConfig);
    }
    for (const auto mode : modes) CFRelease(mode);

    if (modeResult != kCGErrorSuccess) {
        if (error)
            *error = "macOS could not switch both ICUIXIAN inputs to 1080p60";
        return false;
    }

    refreshBounds(pair);
    if (!pair.wallA.currently1080 || !pair.wallB.currently1080) {
        if (error)
            *error = "ICUIXIAN input is not exposed as a 1920x1080 desktop";
        return false;
    }
    pair.needsModeSwitch = false;
    return true;
}

void refreshBounds(WallPair& pair) {
    if (pair.wallA.id != kCGNullDirectDisplay) {
        pair.wallA.bounds = CGDisplayBounds(pair.wallA.id);
        pair.wallA.currently1080 = hasIcuixianDesktopSize(pair.wallA.bounds);
        pair.wallA.pixelWidth = CGDisplayPixelsWide(pair.wallA.id);
        pair.wallA.pixelHeight = CGDisplayPixelsHigh(pair.wallA.id);
    }
    if (pair.wallB.id != kCGNullDirectDisplay) {
        pair.wallB.bounds = CGDisplayBounds(pair.wallB.id);
        pair.wallB.currently1080 = hasIcuixianDesktopSize(pair.wallB.bounds);
        pair.wallB.pixelWidth = CGDisplayPixelsWide(pair.wallB.id);
        pair.wallB.pixelHeight = CGDisplayPixelsHigh(pair.wallB.id);
    }
}

void applyPairToSettings(ofJson& cfg, const WallPair& pair, WallPreset preset) {
    const std::string layoutA = "4x1";
    const std::string layoutB = preset == WallPreset::Mixed ? "2x2" : "4x1";
    const int rotA = 90;
    const int rotB = preset == WallPreset::Mixed ? 0 : 90;
    const char* orientB = preset == WallPreset::Mixed ? "landscape" : "portrait";

    ofJson windows = ofJson::array();
    windows.push_back(windowJson(pair.wallA, layoutA));
    windows.push_back(windowJson(pair.wallB, layoutB));
    cfg["outputMode"] = "dualWindow8";
    cfg["presentationWindows"] = windows;

    if (!cfg.contains("videoWallController") ||
        !cfg["videoWallController"].is_object()) {
        cfg["videoWallController"] = defaultController();
    }
    ofJson& vwc = cfg["videoWallController"];
    vwc["brand"] = vwc.value("brand", "ICUIXIAN");
    vwc["model"] = vwc.value("model", "0104-XZ");
    vwc["asin"] = vwc.value("asin", "B0DM98NVSH");
    vwc["controllers"] = 2;
    vwc["inputWidth"] = kIcuixianInputWidth;
    vwc["inputHeight"] = kIcuixianInputHeight;
    vwc["refreshHz"] = kIcuixianRefreshHz;
    vwc["wallA"] = wallMeta(layoutA, rotA, "portrait", pair.wallA.connector);
    vwc["wallB"] = wallMeta(layoutB, rotB, orientB, pair.wallB.connector);
}

bool savePair(ofJson& cfg, const WallPair& pair, WallPreset preset,
              std::string* error) {
    applyPairToSettings(cfg, pair, preset);
    return SettingsStore::save(cfg, error);
}

std::string describePair(const WallPair& pair, WallPreset preset) {
    if (!pair.valid) return "No wall pair detected";
    const auto fmt = [](const DisplayInfo& d) {
        return ofToString(static_cast<int>(d.bounds.size.width)) + "x" +
               ofToString(static_cast<int>(d.bounds.size.height)) + " @ " +
               ofToString(static_cast<int>(d.bounds.origin.x)) + "," +
               ofToString(static_cast<int>(d.bounds.origin.y)) + " [" +
               connectorName(d.connector) + "]";
    };
    const char* bLayout =
        preset == WallPreset::Mixed ? "2x2 landscape rot 0" : "4x1 portrait rot 90";
    std::string text = "A " + fmt(pair.wallA) + " [4x1 portrait rot 90] | B " +
                       fmt(pair.wallB) + " [" + bLayout + "]";
    if (pair.assignedByConnector) text += " (thunderbolt=A, hdmi=B)";
    else text += " (left=A, right=B)";
    return text;
}

}  // namespace DisplayProbe
