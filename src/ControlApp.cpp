#include "ControlApp.h"
#include "SettingsStore.h"
#include <CoreGraphics/CoreGraphics.h>
#include <cmath>
#include <cstdlib>

namespace {
void sectionTitle(const char* title) {
    ImGui::Spacing();
    ImGui::TextDisabled("%s", title);
    ImGui::Separator();
}

bool compactSliderFloat(const char* label, const char* id, float* value,
                        float minValue, float maxValue,
                        const char* format = "%.2f") {
    const float startX = ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(startX + 108.f);
    ImGui::SetNextItemWidth(std::min(180.f, ImGui::GetContentRegionAvail().x));
    return ImGui::SliderFloat(id, value, minValue, maxValue, format);
}

bool compactSliderInt(const char* label, const char* id, int* value,
                      int minValue, int maxValue) {
    const float startX = ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(startX + 108.f);
    ImGui::SetNextItemWidth(std::min(180.f, ImGui::GetContentRegionAvail().x));
    return ImGui::SliderInt(id, value, minValue, maxValue);
}

constexpr std::size_t kIcuixianInputWidth = 1920;
constexpr std::size_t kIcuixianInputHeight = 1080;
constexpr double kIcuixianRefreshHz = 60.0;

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
}

static const char* kModeNames[] = {
    "BwClean", "ScanLine", "BBoxTracker", "BinaryText",
    "Waveform", "GridData", "Barcode",
    "VideoNormal", "VideoSquares", "VideoNumbers", "VideoLines",
    "ThermalVision", "SlitScan", "Flash", "Auto"
};

void ControlApp::buildWallSummaryFromSettings(const ofJson& cfg) {
    wallASummary_.clear();
    wallBSummary_.clear();
    if (!cfg.contains("presentationWindows") ||
        !cfg["presentationWindows"].is_array()) return;

    const auto& wins = cfg["presentationWindows"];
    const ofJson empty;
    const ofJson& vwc = cfg.contains("videoWallController") &&
                        cfg["videoWallController"].is_object()
                        ? cfg["videoWallController"] : empty;

    const auto buildLine = [&](size_t idx) -> std::string {
        if (idx >= wins.size()) return "";
        const auto& w = wins[idx];
        const std::string layout = w.value("layout", "4x1");
        const bool landscape = (layout == "2x2");
        int rotDeg = landscape ? 0 : 90;
        const char* wallKey = (idx == 0) ? "wallA" : "wallB";
        if (vwc.contains(wallKey) && vwc[wallKey].is_object())
            rotDeg = vwc[wallKey].value("rotationDegrees", rotDeg);
        const int chStart = static_cast<int>(idx) * 4;
        return (idx == 0 ? "WALL A  " : "WALL B  ") +
               layout + (landscape ? " landscape" : " portrait") +
               "  |  ICUIXIAN rotation " + std::to_string(rotDeg) + " deg" +
               "  |  channels " + std::to_string(chStart) + "-" +
               std::to_string(chStart + 3) +
               "  |  " + std::to_string(w.value("width", 1920)) +
               "x" + std::to_string(w.value("height", 1080)) +
               " @ " + std::to_string(w.value("x", 0)) +
               "," + std::to_string(w.value("y", 0));
    };

    wallASummary_ = buildLine(0);
    wallBSummary_ = buildLine(1);
}

void ControlApp::setup() {
    ofSetBackgroundColor(15, 15, 15);
    gui_.setup();

    ImGuiIO& io = ImGui::GetIO();
    // No escribir imgui.ini junto al ejecutable. En un bundle macOS distribuido
    // esa ubicación está firmada y debe permanecer inmutable tras el arranque.
    io.IniFilename = nullptr;
    io.Fonts->Clear();
    io.FontDefault =
        io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/SFNS.ttf", 18.f);
    if (!io.FontDefault) io.FontDefault = io.Fonts->AddFontDefault();

    unsigned char* fontPixels = nullptr;
    int fontWidth = 0;
    int fontHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&fontPixels, &fontWidth, &fontHeight);
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGenTextures(1, &fontTexture_);
    glBindTexture(GL_TEXTURE_2D, fontTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fontWidth, fontHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, fontPixels);
    io.Fonts->TexID = reinterpret_cast<void*>(
        static_cast<intptr_t>(fontTexture_));
    glBindTexture(GL_TEXTURE_2D, previousTexture);

    ImGuiStyle& style = ImGui::GetStyle();
    io.FontGlobalScale = 1.f;
    style.WindowRounding = 0.f;
    style.ChildRounding = 6.f;
    style.FrameRounding = 4.f;
    style.TabRounding = 4.f;
    style.WindowPadding = ImVec2(12.f, 10.f);
    style.FramePadding = ImVec2(7.f, 5.f);
    style.ItemSpacing = ImVec2(8.f, 6.f);
    style.ItemInnerSpacing = ImVec2(4.f, 3.f);
    style.ScrollbarSize = 10.f;

    strncpy(oscHostBuf_, osc_->getHost().c_str(), sizeof(oscHostBuf_) - 1);
    oscPort_ = osc_->getPort();

    const ofJson cfg = SettingsStore::load();
    dualWindowConfigured_ =
        cfg.is_object() && cfg.value("outputMode", std::string()) == "dualWindow8";
    if (cfg.is_object()) buildWallSummaryFromSettings(cfg);
    oscReceiverReady_ = oscReceiver_.setup(oscListenPort_);
    if (oscReceiverReady_)
        ofLogNotice("OSCReceiver") << "Listening on port " << oscListenPort_;
    else
        ofLogWarning("OSCReceiver") << "Could not listen on port "
                                    << oscListenPort_;
    if (performance_ && std::getenv("PDJ_PERF_AUTOSTART"))
        performance_->start(channels_, composer_);
}

void ControlApp::update() {
    pollVpcOsc();
    if (!performance_) return;
    performance_->update(channels_, composer_);
    if (performance_->hasResults() &&
        std::getenv("PDJ_PERF_EXIT_ON_COMPLETE")) {
        ofExit(0);
    }
}

void ControlApp::pollVpcOsc() {
    if (!oscReceiverReady_)
        return;
    while (oscReceiver_.hasWaitingMessages()) {
        ofxOscMessage message;
        oscReceiver_.getNextMessage(message);
        const auto parts = ofSplitString(message.getAddress(), "/", true, true);
        if (parts.size() == 3 && parts[0] == "pdjv" &&
            parts[1] == "program" && composer_) {
            const std::string& command = parts[2];
            if (command == "next") {
                composer_->forceNextMoment();
            } else if (command == "takeover") {
                const int generator = message.getNumArgs() > 0
                    ? message.getArgAsInt32(0) : -1;
                composer_->forceTakeover(generator);
            } else if (command == "enabled" &&
                       message.getNumArgs() > 0) {
                composer_->params().programEnabled =
                    message.getArgAsInt32(0) != 0;
            } else if (command == "dualVideoProbability" &&
                       message.getNumArgs() > 0) {
                composer_->params().dualVideoProbability =
                    ofClamp(message.getArgAsFloat(0), 0.f, 1.f);
            }
            continue;
        }
        if (parts.size() == 3 && parts[0] == "pdjv" &&
            parts[1] == "generator" && message.getNumArgs() > 0) {
            for (Channel* channel : channels_) {
                if (!channel) continue;
                GeneratorRuntimeParams& params = channel->generatorParams();
                if (parts[2] == "intensity")
                    params.intensity =
                        ofClamp(message.getArgAsFloat(0), 0.f, 1.f);
                else if (parts[2] == "density")
                    params.density =
                        ofClamp(message.getArgAsFloat(0), 0.f, 1.f);
                else if (parts[2] == "glowGain")
                    params.glowGain =
                        ofClamp(message.getArgAsFloat(0), 0.f, 1.5f);
                else if (parts[2] == "glowRadius")
                    params.glowRadius =
                        ofClamp(message.getArgAsFloat(0), 0.25f, 4.f);
                else if (parts[2] == "feedbackDecay")
                    params.feedbackDecay =
                        ofClamp(message.getArgAsFloat(0), 0.f, 0.94f);
            }
            continue;
        }
        if (parts.size() == 3 && parts[0] == "pdjv" &&
            parts[1] == "visuals" && parts[2] == "invert" &&
            message.getNumArgs() > 0) {
            const bool inverted = message.getArgAsInt32(0) != 0;
            for (Channel* channel : channels_) {
                if (channel) channel->setInvertPolarity(inverted);
            }
            continue;
        }
        if (parts.size() == 3 && parts[0] == "pdjv" &&
            parts[1] == "vpc") {
            for (auto* channel : channels_) {
                if (parts[2] == "reset")
                    channel->resetVideoPointCloudFeedback();
                else
                    channel->applyVpcOscParam(parts[2], message);
            }
            continue;
        }
        if (parts.size() == 5 && parts[0] == "pdjv" &&
            parts[1] == "channel" && parts[3] == "vpc") {
            const int channelIndex = ofToInt(parts[2]);
            if (channelIndex < 0 ||
                channelIndex >= static_cast<int>(channels_.size()))
                continue;
            if (parts[4] == "reset")
                channels_[channelIndex]->resetVideoPointCloudFeedback();
            else
                channels_[channelIndex]->applyVpcOscParam(parts[4], message);
        }
    }
}

void ControlApp::draw() {
    if (!showUI_) return;
    gui_.begin();

    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ofGetWidth(), ofGetHeight()), ImGuiCond_Always);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("PDJ Control Dashboard", nullptr, flags);

    drawGlobalPanel();

    if (ImGui::Selectable("Overview", activePage_ == 0, 0, ImVec2(150.f, 30.f)))
        activePage_ = 0;
    ImGui::SameLine();
    if (dir_) {
        if (ImGui::Selectable("Global Director", activePage_ == 1, 0,
                              ImVec2(170.f, 30.f)))
            activePage_ = 1;
        ImGui::SameLine();
    }
    if (videoDir_) {
        if (ImGui::Selectable("Video Director", activePage_ == 2, 0,
                              ImVec2(170.f, 30.f)))
            activePage_ = 2;
        ImGui::SameLine();
    }
    if (composer_) {
        if (ImGui::Selectable("Visual Composer", activePage_ == 3, 0,
                              ImVec2(170.f, 30.f)))
            activePage_ = 3;
        ImGui::SameLine();
    }
    if (ImGui::Selectable("Channel Editor", activePage_ == 4, 0,
                          ImVec2(170.f, 30.f)))
        activePage_ = 4;
    ImGui::SameLine();
    if (performance_ &&
        ImGui::Selectable("Performance", activePage_ == 5, 0,
                          ImVec2(150.f, 30.f)))
        activePage_ = 5;
    ImGui::Separator();

    if (activePage_ == 0) {
        drawOverview();
    } else if (activePage_ == 1 && dir_) {
        drawDirectorPanel();
    } else if (activePage_ == 2 && videoDir_) {
        drawVideoDirectorPanel();
    } else if (activePage_ == 3 && composer_) {
        drawComposerPanel();
    } else if (activePage_ == 5 && performance_) {
        drawPerformancePanel();
    } else {
        ImGui::TextDisabled("EDIT CHANNEL");
        ImGui::SameLine();
        for (int i = 0; i < static_cast<int>(channels_.size()); i++) {
            if (i > 0) ImGui::SameLine();
            const std::string label = "Channel " + ofToString(i);
            if (ImGui::Selectable(label.c_str(), activeChannel_ == i, 0,
                                  ImVec2(120.f, 28.f)))
                activeChannel_ = i;
        }
        ImGui::Separator();
        if (activeChannel_ >= 0 &&
            activeChannel_ < static_cast<int>(channels_.size())) {
            ImGui::PushID(activeChannel_);
            drawChannelPanel(activeChannel_);
            ImGui::PopID();
        }
    }

    ImGui::End();
    gui_.end();
}

void ControlApp::exit() {
    if (performance_ && performance_->isRunning())
        performance_->stop(channels_, composer_, false);
    if (fontTexture_ != 0) {
        glDeleteTextures(1, &fontTexture_);
        fontTexture_ = 0;
    }
}

void ControlApp::keyPressed(int key) {
    if (key == 'u' || key == 'U') showUI_ = !showUI_;
}

bool ControlApp::saveOutputMode(const std::string& mode) {
    std::string settingsError;
    ofJson cfg = SettingsStore::load(&settingsError);
    if (!cfg.is_object()) {
        outputModeError_ = settingsError.empty()
            ? "settings.json is missing or invalid" : settingsError;
        return false;
    }
    cfg["outputMode"] = mode;
    if (!SettingsStore::save(cfg, &settingsError)) {
        outputModeError_ = settingsError.empty()
            ? "Could not write settings.json" : settingsError;
        return false;
    }
    outputModeError_.clear();
    return true;
}

bool ControlApp::detectAndSaveDualOutputs() {
    struct DetectedDisplay {
        CGDirectDisplayID id = kCGNullDirectDisplay;
        CGRect bounds = CGRectZero;
        std::size_t physicalArea = 0;
        bool external = false;
        bool is4K = false;
    };

    uint32_t displayCount = 0;
    if (CGGetActiveDisplayList(0, nullptr, &displayCount) != kCGErrorSuccess ||
        displayCount < 2) {
        outputModeError_ = "Two active displays were not detected";
        return false;
    }

    std::vector<CGDirectDisplayID> ids(displayCount);
    if (CGGetActiveDisplayList(displayCount, ids.data(), &displayCount) !=
        kCGErrorSuccess) {
        outputModeError_ = "macOS display detection failed";
        return false;
    }

    std::vector<DetectedDisplay> external;
    for (uint32_t i = 0; i < displayCount; ++i) {
        const CGDirectDisplayID id = ids[i];
        if (CGDisplayMirrorsDisplay(id) != kCGNullDirectDisplay) continue;

        const std::size_t physicalW = CGDisplayPixelsWide(id);
        const std::size_t physicalH = CGDisplayPixelsHigh(id);
        DetectedDisplay display;
        display.id = id;
        display.bounds = CGDisplayBounds(id);
        display.physicalArea = physicalW * physicalH;
        display.external = CGDisplayIsBuiltin(id) == 0;
        display.is4K = physicalW >= 3840;
        if (display.external) external.push_back(display);
    }

    std::vector<DetectedDisplay> candidates = external;
    if (candidates.size() < 2) {
        outputModeError_ =
            "ICUIXIAN setup requires two independent external displays";
        return false;
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const DetectedDisplay& a, const DetectedDisplay& b) {
                  if (a.is4K != b.is4K) return a.is4K > b.is4K;
                  return a.physicalArea > b.physicalArea;
              });
    candidates.resize(2);

    std::vector<CGDisplayModeRef> inputModes;
    inputModes.reserve(candidates.size());
    for (const auto& display : candidates) {
        const CGDisplayModeRef mode = copyIcuixianInputMode(display.id);
        if (!mode) {
            for (const auto retainedMode : inputModes) CFRelease(retainedMode);
            outputModeError_ =
                "ICUIXIAN 1920x1080 @ 60 Hz mode is unavailable on an input";
            return false;
        }
        inputModes.push_back(mode);
    }

    CGDisplayConfigRef displayConfig = nullptr;
    CGError modeResult = CGBeginDisplayConfiguration(&displayConfig);
    for (std::size_t i = 0;
         modeResult == kCGErrorSuccess && i < candidates.size(); ++i) {
        modeResult = CGConfigureDisplayWithDisplayMode(
            displayConfig, candidates[i].id, inputModes[i], nullptr);
    }
    if (modeResult == kCGErrorSuccess) {
        modeResult = CGCompleteDisplayConfiguration(
            displayConfig, kCGConfigurePermanently);
    } else if (displayConfig) {
        CGCancelDisplayConfiguration(displayConfig);
    }
    for (const auto mode : inputModes) CFRelease(mode);

    if (modeResult != kCGErrorSuccess) {
        outputModeError_ =
            "macOS could not switch both ICUIXIAN inputs to 1080p60";
        return false;
    }

    for (auto& display : candidates) {
        display.bounds = CGDisplayBounds(display.id);
        if (static_cast<int>(display.bounds.size.width) !=
                static_cast<int>(kIcuixianInputWidth) ||
            static_cast<int>(display.bounds.size.height) !=
                static_cast<int>(kIcuixianInputHeight)) {
            outputModeError_ =
                "ICUIXIAN input is not exposed as a 1920x1080 desktop";
            return false;
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const DetectedDisplay& a, const DetectedDisplay& b) {
                  if (a.bounds.origin.x != b.bounds.origin.x)
                      return a.bounds.origin.x < b.bounds.origin.x;
                  return a.bounds.origin.y < b.bounds.origin.y;
              });

    std::string settingsError;
    ofJson cfg = SettingsStore::load(&settingsError);
    if (!cfg.is_object()) {
        outputModeError_ = settingsError.empty()
            ? "settings.json is missing or invalid" : settingsError;
        return false;
    }

    ofJson windows = ofJson::array();
    for (const auto& display : candidates) {
        windows.push_back({
            {"x", static_cast<int>(display.bounds.origin.x)},
            {"y", static_cast<int>(display.bounds.origin.y)},
            {"width", static_cast<int>(display.bounds.size.width)},
            {"height", static_cast<int>(display.bounds.size.height)},
            {"layout", "4x1"}
        });
    }
    cfg["outputMode"] = "dualWindow8";
    cfg["presentationWindows"] = windows;
    cfg["videoWallController"] = {
        {"brand", "ICUIXIAN"},
        {"model", "0104-XZ"},
        {"asin", "B0DM98NVSH"},
        {"controllers", 2},
        {"inputWidth", kIcuixianInputWidth},
        {"inputHeight", kIcuixianInputHeight},
        {"refreshHz", kIcuixianRefreshHz},
        {"wallA", {{"layout", "4x1"}, {"layoutPerController", "4x1"}, {"rotationDegrees", 90}, {"panelOrientation", "portrait"}, {"panels", 4}}},
        {"wallB", {{"layout", "4x1"}, {"layoutPerController", "4x1"}, {"rotationDegrees", 90}, {"panelOrientation", "portrait"}, {"panels", 4}}}
    };
    // Ambos muros son verticales aquí (botón 4V+4V). Para mixto vertical+horizontal
    // usar "Configure Mixed Wall (4V + 2x2H)", que pone wallB rotationDegrees=0.

    if (!SettingsStore::save(cfg, &settingsError)) {
        outputModeError_ = settingsError.empty()
            ? "Could not write settings.json" : settingsError;
        return false;
    }

    const auto& a = candidates[0].bounds;
    const auto& b = candidates[1].bounds;
    detectedOutputSummary_ =
        "ICUIXIAN A " +
        ofToString(static_cast<int>(a.size.width)) + "x" +
        ofToString(static_cast<int>(a.size.height)) + " @ " +
        ofToString(static_cast<int>(a.origin.x)) + "," +
        ofToString(static_cast<int>(a.origin.y)) +
        " [4x1 portrait rot 90] | B " +
        ofToString(static_cast<int>(b.size.width)) + "x" +
        ofToString(static_cast<int>(b.size.height)) + " @ " +
        ofToString(static_cast<int>(b.origin.x)) + "," +
        ofToString(static_cast<int>(b.origin.y)) + " [4x1 portrait rot 90]";
    buildWallSummaryFromSettings(cfg);
    outputModeError_.clear();
    dualWindowConfigured_ = true;
    outputRestartRequired_ = true;
    return true;
}

bool ControlApp::configureMixedWall() {
    // Ejecuta la misma detección de pantallas que detectAndSaveDualOutputs() pero
    // guarda Wall A como 4x1 vertical (4 franjas, ICUIXIAN rotado 90 deg) y
    // Wall B como 2x2 horizontal (retícula 2x2, rotación ICUIXIAN 0 deg).
    struct DetectedDisplay {
        CGDirectDisplayID id = kCGNullDirectDisplay;
        CGRect bounds = CGRectZero;
        std::size_t physicalArea = 0;
        bool external = false;
        bool is4K = false;
    };

    uint32_t displayCount = 0;
    if (CGGetActiveDisplayList(0, nullptr, &displayCount) != kCGErrorSuccess ||
        displayCount < 2) {
        outputModeError_ = "Two active displays were not detected";
        return false;
    }

    std::vector<CGDirectDisplayID> ids(displayCount);
    if (CGGetActiveDisplayList(displayCount, ids.data(), &displayCount) !=
        kCGErrorSuccess) {
        outputModeError_ = "macOS display detection failed";
        return false;
    }

    std::vector<DetectedDisplay> external;
    for (uint32_t i = 0; i < displayCount; ++i) {
        const CGDirectDisplayID id = ids[i];
        if (CGDisplayMirrorsDisplay(id) != kCGNullDirectDisplay) continue;
        const std::size_t physicalW = CGDisplayPixelsWide(id);
        const std::size_t physicalH = CGDisplayPixelsHigh(id);
        DetectedDisplay display;
        display.id = id;
        display.bounds = CGDisplayBounds(id);
        display.physicalArea = physicalW * physicalH;
        display.external = CGDisplayIsBuiltin(id) == 0;
        display.is4K = physicalW >= 3840;
        if (display.external) external.push_back(display);
    }

    if (external.size() < 2) {
        outputModeError_ = "Mixed wall setup requires two independent external displays";
        return false;
    }

    std::sort(external.begin(), external.end(),
              [](const DetectedDisplay& a, const DetectedDisplay& b) {
                  if (a.is4K != b.is4K) return a.is4K > b.is4K;
                  return a.physicalArea > b.physicalArea;
              });
    external.resize(2);

    std::vector<CGDisplayModeRef> inputModes;
    inputModes.reserve(external.size());
    for (const auto& display : external) {
        const CGDisplayModeRef mode = copyIcuixianInputMode(display.id);
        if (!mode) {
            for (const auto retainedMode : inputModes) CFRelease(retainedMode);
            outputModeError_ = "1920x1080 @ 60 Hz mode unavailable on one of the displays";
            return false;
        }
        inputModes.push_back(mode);
    }

    CGDisplayConfigRef displayConfig = nullptr;
    CGError modeResult = CGBeginDisplayConfiguration(&displayConfig);
    for (std::size_t i = 0;
         modeResult == kCGErrorSuccess && i < external.size(); ++i) {
        modeResult = CGConfigureDisplayWithDisplayMode(
            displayConfig, external[i].id, inputModes[i], nullptr);
    }
    if (modeResult == kCGErrorSuccess) {
        modeResult = CGCompleteDisplayConfiguration(
            displayConfig, kCGConfigurePermanently);
    } else if (displayConfig) {
        CGCancelDisplayConfiguration(displayConfig);
    }
    for (const auto mode : inputModes) CFRelease(mode);

    if (modeResult != kCGErrorSuccess) {
        outputModeError_ = "macOS could not switch both ICUIXIAN inputs to 1080p60";
        return false;
    }

    for (auto& display : external) {
        display.bounds = CGDisplayBounds(display.id);
        if (static_cast<int>(display.bounds.size.width) !=
                static_cast<int>(kIcuixianInputWidth) ||
            static_cast<int>(display.bounds.size.height) !=
                static_cast<int>(kIcuixianInputHeight)) {
            outputModeError_ = "Display is not exposed as a 1920x1080 desktop";
            return false;
        }
    }

    // Ordenar de izquierda a derecha para que Window A sea la salida más a la izquierda (Wall A = vertical).
    std::sort(external.begin(), external.end(),
              [](const DetectedDisplay& a, const DetectedDisplay& b) {
                  if (a.bounds.origin.x != b.bounds.origin.x)
                      return a.bounds.origin.x < b.bounds.origin.x;
                  return a.bounds.origin.y < b.bounds.origin.y;
              });

    std::string settingsError;
    ofJson cfg = SettingsStore::load(&settingsError);
    if (!cfg.is_object()) {
        outputModeError_ = settingsError.empty()
            ? "settings.json is missing or invalid" : settingsError;
        return false;
    }

    // Wall A: pantalla más a la izquierda, franjas 4x1 verticales.
    // Wall B: pantalla más a la derecha, retícula 2x2 horizontal.
    ofJson windows = ofJson::array();
    windows.push_back({
        {"x",      static_cast<int>(external[0].bounds.origin.x)},
        {"y",      static_cast<int>(external[0].bounds.origin.y)},
        {"width",  static_cast<int>(external[0].bounds.size.width)},
        {"height", static_cast<int>(external[0].bounds.size.height)},
        {"layout", "4x1"}
    });
    windows.push_back({
        {"x",      static_cast<int>(external[1].bounds.origin.x)},
        {"y",      static_cast<int>(external[1].bounds.origin.y)},
        {"width",  static_cast<int>(external[1].bounds.size.width)},
        {"height", static_cast<int>(external[1].bounds.size.height)},
        {"layout", "2x2"}
    });

    cfg["outputMode"] = "dualWindow8";
    cfg["presentationWindows"] = windows;
    cfg["videoWallController"] = {
        {"brand", "ICUIXIAN"},
        {"model", "0104-XZ"},
        {"asin", "B0DM98NVSH"},
        {"controllers", 2},
        {"inputWidth", kIcuixianInputWidth},
        {"inputHeight", kIcuixianInputHeight},
        {"refreshHz", kIcuixianRefreshHz},
        {"wallA", {{"layout", "4x1"}, {"layoutPerController", "4x1"}, {"rotationDegrees", 90}, {"panelOrientation", "portrait"}, {"panels", 4}}},
        {"wallB", {{"layout", "2x2"}, {"layoutPerController", "2x2"}, {"rotationDegrees", 0},  {"panelOrientation", "landscape"}, {"panels", 4}}}
    };

    if (!SettingsStore::save(cfg, &settingsError)) {
        outputModeError_ = settingsError.empty()
            ? "Could not write settings.json" : settingsError;
        return false;
    }

    const auto& a = external[0].bounds;
    const auto& b = external[1].bounds;
    detectedOutputSummary_ =
        "Mixed wall: A " +
        ofToString(static_cast<int>(a.size.width)) + "x" +
        ofToString(static_cast<int>(a.size.height)) + " @ " +
        ofToString(static_cast<int>(a.origin.x)) + "," +
        ofToString(static_cast<int>(a.origin.y)) +
        " [4x1 portrait rot 90] | B " +
        ofToString(static_cast<int>(b.size.width)) + "x" +
        ofToString(static_cast<int>(b.size.height)) + " @ " +
        ofToString(static_cast<int>(b.origin.x)) + "," +
        ofToString(static_cast<int>(b.origin.y)) + " [2x2 landscape rot 0]";
    buildWallSummaryFromSettings(cfg);
    outputModeError_.clear();
    dualWindowConfigured_ = true;
    outputRestartRequired_ = true;
    return true;
}

void ControlApp::drawGlobalPanel() {
    // Altura: base 92 + 20 por línea de resumen de muro cuando existe.
    const float wallLines = (!wallASummary_.empty() ? 1.f : 0.f) +
                            (!wallBSummary_.empty() ? 1.f : 0.f);
    const float globalH = 92.f + wallLines * 22.f;
    ImGui::BeginChild("GlobalBar", ImVec2(0.f, globalH), true,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("PDJ CONTROL");
    ImGui::SameLine(120.f);
    ImGui::TextDisabled("OSC");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.f);
    ImGui::InputText("Host", oscHostBuf_, sizeof(oscHostBuf_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.f);
    ImGui::InputInt("Port", &oscPort_);
    ImGui::SameLine();
    if (ImGui::Button("Apply OSC")) {
        osc_->setHost(oscHostBuf_);
        osc_->setPort(oscPort_);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|  CLIPS");
    ImGui::SameLine();
    ImGui::Text("%d available", pool_->totalClips());
    ImGui::SameLine();
    if (dualWindowConfigured_) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.55f, 1.f),
                           "Dual output configured");
    } else if (ImGui::Button("Enable dual output")) {
        if (saveOutputMode("dualWindow8")) {
            dualWindowConfigured_ = true;
            outputRestartRequired_ = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Configure ICUIXIAN outputs (4V + 4V)"))
        detectAndSaveDualOutputs();
    ImGui::SameLine();
    if (ImGui::Button("Configure Mixed Wall (4V + 2x2H)"))
        configureMixedWall();
    if (outputRestartRequired_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.68f, 0.25f, 1.f),
                           "Restart required");
    } else if (!outputModeError_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f),
                           "%s", outputModeError_.c_str());
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 108.f);
    ImGui::TextDisabled("U: hide UI");
    // Líneas de identidad por muro — siempre visibles para que el operador confirme el enrutado.
    if (!wallASummary_.empty())
        ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.55f, 1.f),
                           "%s", wallASummary_.c_str());
    if (!wallBSummary_.empty())
        ImGui::TextColored(ImVec4(0.40f, 0.65f, 1.00f, 1.f),
                           "%s", wallBSummary_.c_str());
    ImGui::EndChild();
}

void ControlApp::drawOverview() {
    ImGui::Spacing();
    ImGui::Text("OPERATION OVERVIEW");
    ImGui::TextDisabled("Transport, active modes, events and live CV status");
    ImGui::Spacing();

    const int channelCount = static_cast<int>(channels_.size());
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int columnsPerRow = std::max(1, std::min(4, channelCount));
    const int rowCount = std::max(1, (channelCount + columnsPerRow - 1) /
                                      columnsPerRow);
    const float cardWidth =
        (available.x - gap * std::max(0, columnsPerRow - 1)) /
        columnsPerRow;
    const float cardHeight =
        (available.y - gap * std::max(0, rowCount - 1)) / rowCount;

    for (int i = 0; i < channelCount; i++) {
        Channel* ch = channels_[i];
        if (!ch) continue;
        if ((i % columnsPerRow) != 0) ImGui::SameLine();

        ImGui::PushID(i);
        ImGui::BeginChild("OverviewCard", ImVec2(cardWidth, cardHeight), true);

        ImGui::Text("CHANNEL %d", i);
        ImGui::SameLine();
        ImGui::TextColored(ch->isPlaying() ? ImVec4(0.35f, 0.85f, 0.55f, 1.f)
                                           : ImVec4(0.75f, 0.75f, 0.75f, 1.f),
                           "%s", ch->isPlaying() ? "PLAYING" : "STOPPED");
        ImGui::Separator();

        const std::string clipName =
            ofFilePath::getFileName(ch->getCurrentClip());
        ImGui::TextWrapped("Clip: %s",
                           clipName.empty() ? "No clip loaded" : clipName.c_str());

        if (ImGui::Button("Next")) ch->loadNextClip();
        ImGui::SameLine();
        if (ImGui::Button(ch->isPlaying() ? "Pause" : "Play"))
            ch->isPlaying() ? ch->pause() : ch->play();
        ImGui::SameLine();
        if (ImGui::Button("Stop")) ch->stop();

        float speed = ch->getBaseSpeed();
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::SliderFloat("Speed", &speed, 0.1f, 4.f, "%.2fx"))
            ch->setSpeed(speed);

        sectionTitle("SCORE");
        const int currentMode = static_cast<int>(ch->getScore().currentMode());
        ImGui::Text("Active mode");
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.f, 1.f), "%s",
            kModeNames[std::min(currentMode, static_cast<int>(ScoreMode::Flash) + 1)]);

        sectionTitle("EVENTS");
        const CVData& data = ch->getCVPipeline().getData();
        ImGui::Text("Collision: %s", data.events.collision ? "YES" : "No");
        ImGui::Text("Ball: %s", data.events.ballDetected ? "YES" : "No");
        ImGui::Text("Crowd density: %.2f", data.events.crowdDensity);
        ImGui::Text("Leg distance: %.2f", data.events.legDistance);

        sectionTitle("LIVE CV");
        ImGui::Text("Motion energy: %.3f", data.motionEnergy);
        ImGui::Text("Flow magnitude: %.3f", data.flowMagnitude);
        ImGui::Text("Flow angle: %.2f", data.flowAngle);
        ImGui::Text("Blobs: %d", data.blobCount);
        ImGui::Text("Contour: %.0f", data.contourLength);
        ImGui::Text("Update: %.2f ms", ch->getUpdateMilliseconds());

        ImGui::EndChild();
        ImGui::PopID();
    }
}

void ControlApp::drawDirectorPanel() {
    if (!dir_) return;
    GlobalDirectorParams& p = dir_->params();

    ImGui::BeginChild("DirectorPanel", ImVec2(0.f, 0.f), true);

    const char* tName = "Idle";
    switch (dir_->temporalPhase()) {
        case TemporalPhase::SlowRampDown: tName = "SlowRampDown"; break;
        case TemporalPhase::SlowHold:     tName = "SlowHold";     break;
        case TemporalPhase::SlowRampUp:   tName = "SlowRampUp";   break;
        case TemporalPhase::FastRampUp:   tName = "FastRampUp";   break;
        case TemporalPhase::FastHold:     tName = "FastHold";     break;
        case TemporalPhase::FastRampDown: tName = "FastRampDown"; break;
        default: break;
    }
    const char* cName = "Idle";
    switch (dir_->clearPhase()) {
        case ClearPhase::FadeIn:  cName = "FadeIn";  break;
        case ClearPhase::Hold:    cName = "Hold";    break;
        case ClearPhase::FadeOut: cName = "FadeOut"; break;
        default: break;
    }
    float spd = dir_->getSpeedMultiplier();
    ImGui::Text("GLOBAL DIRECTOR");
    ImGui::SameLine(145.f);
    ImGui::Text("Temporal: %s  x%.2f   |   Clear: %s  a%.0f",
                tName, spd, cName, dir_->getClearAlpha());

    ImGui::Separator();
    const float columnWidth = ImGui::GetContentRegionAvail().x / 3.f;
    ImGui::Columns(3, "DirectorColumns", false);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::SetColumnWidth(1, columnWidth);

    ImGui::TextDisabled("TRIGGERS / SLOW MOTION");
    if (ImGui::Button("Slow Mo"))     dir_->triggerSlow();
    ImGui::SameLine();
    if (ImGui::Button("Fast Fwd"))    dir_->triggerFast();
    ImGui::SameLine();
    if (ImGui::Button("Clear Black")) dir_->triggerClear(ofColor(0, 0, 0));
    ImGui::SameLine();
    if (ImGui::Button("Clear Red"))   dir_->triggerClear(ofColor(150, 12, 12));

    ImGui::Checkbox("Auto Slow",  &p.autoSlow);
    ImGui::SameLine();
    ImGui::Checkbox("Auto Fast",  &p.autoFast);
    ImGui::SameLine();
    ImGui::Checkbox("Auto Clear", &p.autoClear);

    compactSliderFloat("Speed", "##SlowSpeed", &p.slowSpeed, 0.05f, 0.8f);
    compactSliderFloat("Ramp down", "##SlowRampDown", &p.slowRampDownDur, 0.2f, 4.f);
    compactSliderFloat("Hold min", "##SlowHoldMin", &p.slowHoldMin, 1.f, 15.f);
    compactSliderFloat("Hold max", "##SlowHoldMax", &p.slowHoldMax, 2.f, 20.f);
    compactSliderFloat("Ramp up", "##SlowRampUp", &p.slowRampUpDur, 0.2f, 5.f);
    compactSliderFloat("Ivl min", "##SlowIntervalMin", &p.slowIntervalMin, 10.f, 120.f, "%.0f");
    compactSliderFloat("Ivl max", "##SlowIntervalMax", &p.slowIntervalMax, 15.f, 180.f, "%.0f");

    ImGui::NextColumn();
    ImGui::TextDisabled("FAST FORWARD");
    compactSliderFloat("Speed", "##FastSpeed", &p.fastSpeed, 1.2f, 5.f);
    compactSliderFloat("Ramp up", "##FastRampUp", &p.fastRampUpDur, 0.1f, 2.f);
    compactSliderFloat("Hold min", "##FastHoldMin", &p.fastHoldMin, 0.3f, 6.f);
    compactSliderFloat("Hold max", "##FastHoldMax", &p.fastHoldMax, 0.5f, 10.f);
    compactSliderFloat("Ramp down", "##FastRampDown", &p.fastRampDownDur, 0.1f, 2.f);
    compactSliderFloat("Ivl min", "##FastIntervalMin", &p.fastIntervalMin, 10.f, 120.f, "%.0f");
    compactSliderFloat("Ivl max", "##FastIntervalMax", &p.fastIntervalMax, 15.f, 180.f, "%.0f");

    ImGui::NextColumn();
    ImGui::TextDisabled("SCREEN CLEAR");
    compactSliderFloat("Fade in", "##ClearFadeIn", &p.clearFadeInDur, 0.05f, 2.f);
    compactSliderFloat("Hold min", "##ClearHoldMin", &p.clearHoldMin, 0.1f, 4.f);
    compactSliderFloat("Hold max", "##ClearHoldMax", &p.clearHoldMax, 0.2f, 8.f);
    compactSliderFloat("Fade out", "##ClearFadeOut", &p.clearFadeOutDur, 0.05f, 3.f);
    compactSliderFloat("Ivl min", "##ClearIntervalMin", &p.clearIntervalMin, 15.f, 180.f, "%.0f");
    compactSliderFloat("Ivl max", "##ClearIntervalMax", &p.clearIntervalMax, 20.f, 300.f, "%.0f");

    ImGui::Columns(1);
    ImGui::EndChild();
}

void ControlApp::drawVideoDirectorPanel() {
    if (!videoDir_) return;
    VideoDirectorParams& p = videoDir_->params();

    ImGui::BeginChild("VideoDirectorPanel", ImVec2(0.f, 0.f), true);
    ImGui::Text("STOCHASTIC VIDEO DIRECTOR");
    ImGui::SameLine(280.f);
    ImGui::TextColored(p.enabled ? ImVec4(0.35f, 0.85f, 0.55f, 1.f)
                                 : ImVec4(0.75f, 0.75f, 0.75f, 1.f),
                       "%s", p.enabled ? "RUNNING" : "PAUSED");
    if (!videoDir_->isSharedActive()) {
        ImGui::SameLine();
        ImGui::TextDisabled("| next shared event in %.0f s",
                            videoDir_->secondsUntilShared());
    } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.65f, 0.25f, 1.f),
                           "| SHARED EVENT %s",
                           videoDir_->isSharedPlaying() ? "PLAYING" : "LOADING");
    }

    ImGui::Separator();
    ImGui::Checkbox("Automatic selection", &p.enabled);
    ImGui::SameLine();
    if (ImGui::Button("Shared event now")) videoDir_->triggerSharedNow();

    const float columnWidth = ImGui::GetContentRegionAvail().x / 3.f;
    ImGui::Columns(3, "VideoDirectorColumns", false);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::SetColumnWidth(1, columnWidth);

    ImGui::TextDisabled("OUTCOME WEIGHTS");
    compactSliderFloat("Short", "##ShortWeight", &p.shortWeight, 0.f, 1.f);
    compactSliderFloat("Long", "##LongWeight", &p.longWeight, 0.f, 1.f);
    compactSliderFloat("Full", "##FullWeight", &p.fullWeight, 0.f, 1.f);
    ImGui::TextWrapped("Weights are relative; they do not need to total 1.");

    ImGui::NextColumn();
    ImGui::TextDisabled("FRAGMENT LENGTHS");
    compactSliderFloat("Short min", "##ShortMin", &p.shortMin, 1.f, 60.f, "%.0f s");
    compactSliderFloat("Short max", "##ShortMax", &p.shortMax, 2.f, 90.f, "%.0f s");
    compactSliderFloat("Long min", "##LongMin", &p.longMin, 10.f, 180.f, "%.0f s");
    compactSliderFloat("Long max", "##LongMax", &p.longMax, 20.f, 300.f, "%.0f s");

    ImGui::NextColumn();
    ImGui::TextDisabled("SHARED SYNCHRONIZATION");
    compactSliderFloat("Ivl min", "##SharedMin", &p.sharedIntervalMin, 15.f, 600.f, "%.0f s");
    compactSliderFloat("Ivl max", "##SharedMax", &p.sharedIntervalMax, 30.f, 900.f, "%.0f s");
    compactSliderFloat("Start delay", "##SharedDelay", &p.sharedStartDelay, 0.1f, 2.f, "%.2f s");
    compactSliderFloat("Drift limit", "##Drift", &p.driftTolerance, 0.02f, 0.5f, "%.2f s");
    ImGui::Columns(1);

    sectionTitle("CHANNEL PLANS");
    for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
        Channel* ch = channels_[i];
        if (!ch) continue;
        ImGui::PushID(i + 100);
        ImGui::Text("CH %d", i);
        ImGui::SameLine(60.f);
        ImGui::TextColored(ch->isSharedVideoPlan()
                               ? ImVec4(1.f, 0.65f, 0.25f, 1.f)
                               : ImVec4(0.35f, 0.75f, 1.f, 1.f),
                           "%s%s",
                           VideoDirector::planTypeName(ch->getVideoPlanType()),
                           ch->isSharedVideoPlan() ? " / shared" : "");
        ImGui::SameLine(190.f);
        ImGui::TextDisabled("%s",
            ofFilePath::getFileName(ch->getCurrentClip()).c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 100.f);
        if (ImGui::Button("Next")) ch->loadNextClip();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void ControlApp::drawComposerPanel() {
    if (!composer_) return;
    VisualComposerParams& p = composer_->params();
    static int selectedGenerator = 0;
    static int selectedOrganization = 0;
    static const char* generatorNames[] = {
        "Raster Pulse", "Bit Matrix", "Modular Grid", "Phase Lines",
        "Vector Field", "Data Ledger", "Signal Trace", "Threshold Bridge",
        "Pulse Field", "Bar Scan", "Granular Raster", "Orbital Rings",
        "Full Strobe", "Divided Strobe", "Analog Noise"
    };
    static const char* momentNames[] = {
        "Pulse system", "Bar scan system", "Intercalation"
    };
    static const char* organizationNames[] = {
        "Unison", "Propagation", "Counterpoint", "4 + 4"
    };
    static const char* contentNames[] = {
        "Video", "Generator", "Breath", "Transition"
    };
    static const char* stageNames[] = {
        "Appearance", "Development", "Threshold",
        "Transformation", "Dissolution"
    };
    static const char* collectiveNames[] = {
        "Suspension", "Codification", "Accumulation", "Propagation",
        "Convergence", "Fragmentation", "Saturation", "Rupture",
        "Residue"
    };

    ImGui::BeginChild("ComposerPanel", ImVec2(0.f, 0.f), true);
    ImGui::Text("VISUAL COMPOSER");
    ImGui::SameLine(190.f);
    ImGui::TextColored(p.enabled ? ImVec4(0.35f, 0.85f, 0.55f, 1.f)
                                 : ImVec4(0.75f, 0.75f, 0.75f, 1.f),
                       "%s", p.enabled ? "ACTIVE" : "LEGACY VIDEO MODE");
    ImGui::Checkbox("Enabled", &p.enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Installation program", &p.programEnabled);
    const int activeMoment = ofClamp(
        static_cast<int>(composer_->moment()), 0, 2);
    ImGui::Text("Moment: %s  %.1fs%s",
                momentNames[activeMoment], composer_->momentElapsed(),
                composer_->takeoverActive() ? "  / GLOBAL TAKEOVER" : "");
    ImGui::SameLine();
    if (ImGui::Button("Next moment"))
        composer_->forceNextMoment();
    ImGui::SameLine();
    if (ImGui::Button("Global takeover"))
        composer_->forceTakeover();
    ImGui::TextDisabled("Point-cloud occupancy  group A: %d/4  group B: %d/4",
                        composer_->activeVideoCount(0),
                        composer_->activeVideoCount(1));
    const CollectiveState& collective = composer_->collectiveState();
    const int collectiveIndex = ofClamp(
        static_cast<int>(collective.movement), 0, 8);
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.f, 1.f),
                       "Collective: %s  phase %.2f  revision %llu",
                       collectiveNames[collectiveIndex], collective.phase,
                       static_cast<unsigned long long>(collective.revision));
    ImGui::TextDisabled(
        "activity %.2f  coherence %.2f  diversity %.2f  convergence %.2f  tension %.2f",
        collective.activity, collective.coherence, collective.diversity,
        collective.convergence, collective.tension);

    ImGui::SetNextItemWidth(190.f);
    ImGui::Combo("Organization", &selectedOrganization,
                 "Unison\0Propagation\0Counterpoint\0" "4 + 4\0");
    ImGui::SameLine();
    if (ImGui::Button("Apply organization"))
        composer_->forceOrganization(
            static_cast<OrganizationMode>(selectedOrganization));

    ImGui::SetNextItemWidth(190.f);
    ImGui::Combo("Generator", &selectedGenerator,
                 "Raster Pulse\0Bit Matrix\0Modular Grid\0Phase Lines\0"
                 "Vector Field\0Data Ledger\0Signal Trace\0Threshold Bridge\0"
                 "Pulse Field\0Bar Scan\0Granular Raster\0Orbital Rings\0"
                 "Full Strobe\0Divided Strobe\0Analog Noise\0");
    ImGui::SameLine();
    if (ImGui::Button("Shared generator"))
        composer_->forceShared(ContentType::Generator, selectedGenerator);
    ImGui::SameLine();
    if (ImGui::Button("Shared breath"))
        composer_->forceShared(ContentType::Breath);

    ImGui::Separator();
    ImGui::Columns(3, "ComposerColumns", false);
    ImGui::TextDisabled("CONTENT WEIGHTS");
    ImGui::Checkbox("Data-assisted form", &p.collectiveLogicEnabled);
    compactSliderFloat("Data response", "##CollectiveResponse",
                       &p.collectiveResponse, 0.f, 1.f);
    compactSliderFloat("Decision hold", "##CollectiveHold",
                       &p.collectiveDecisionHold, 0.2f, 6.f, "%.1f s");
    compactSliderFloat("Movement dwell", "##CollectiveDwell",
                       &p.collectiveMinimumDwell, 1.f, 20.f, "%.1f s");
    compactSliderFloat("Video", "##ComposerVideoW", &p.videoProbability, 0.f, 1.f);
    compactSliderFloat("Generator", "##ComposerGenW", &p.generatorProbability, 0.f, 1.f);
    compactSliderFloat("Breath", "##ComposerBreathW", &p.breathProbability, 0.f, 1.f);
    compactSliderFloat("Transition", "##ComposerTransW", &p.transitionProbability, 0.f, 1.f);
    compactSliderFloat("Second video", "##ComposerDualVideo",
                       &p.dualVideoProbability, 0.f, 1.f);
    compactSliderInt("Video cap", "##ComposerVideoCap",
                     &p.maxVideoPerGroup, 1, 2);

    ImGui::NextColumn();
    ImGui::TextDisabled("PROTECTED DURATIONS");
    compactSliderFloat("Video min", "##ComposerVideoMin", &p.videoMinDuration, 1.f, 120.f, "%.0f s");
    compactSliderFloat("Generator min", "##ComposerGenMin", &p.generatorMinDuration, 2.f, 60.f, "%.0f s");
    compactSliderFloat("Generator max", "##ComposerGenMax", &p.generatorMaxDuration, 3.f, 120.f, "%.0f s");
    compactSliderFloat("Breath min", "##ComposerBreathMin", &p.breathMinDuration, 0.5f, 20.f, "%.1f s");
    compactSliderFloat("Breath max", "##ComposerBreathMax", &p.breathMaxDuration, 1.f, 30.f, "%.1f s");
    compactSliderFloat("Pulse moment", "##ComposerPulseMoment",
                       &p.pulseMomentDuration, 4.f, 120.f, "%.0f s");
    compactSliderFloat("Bar moment", "##ComposerBarMoment",
                       &p.barScanMomentDuration, 4.f, 120.f, "%.0f s");
    compactSliderFloat("Takeover min", "##ComposerTakeoverMin",
                       &p.takeoverIntervalMin, 10.f, 300.f, "%.0f s");
    compactSliderFloat("Takeover max", "##ComposerTakeoverMax",
                       &p.takeoverIntervalMax, 10.f, 600.f, "%.0f s");
    compactSliderFloat("Data event cooldown", "##CollectiveCooldown",
                       &p.collectiveEventCooldown, 4.f, 120.f, "%.0f s");
    ImGui::Separator();
    ImGui::Checkbox("Noise events", &p.noiseEventEnabled);
    compactSliderFloat("Noise interval", "##NoiseInterval",
                       &p.noiseEventInterval, 10.f, 300.f, "%.0f s");
    compactSliderFloat("Noise duration", "##NoiseDuration",
                       &p.noiseEventDuration, 1.f, 20.f, "%.1f s");
    if (ImGui::Button("Trigger noise now"))
        composer_->forceNoiseMoment();
    ImGui::Separator();
    ImGui::Checkbox("Generator invert", &p.generatorInvertEnabled);
    compactSliderFloat("Invert interval", "##InvertInterval",
                       &p.generatorInvertInterval, 10.f, 300.f, "%.0f s");
    compactSliderFloat("Invert duration", "##InvertDuration",
                       &p.generatorInvertDuration, 1.f, 60.f, "%.1f s");
    if (composer_->generatorInvertActive()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 1.f, 0.3f, 1.f), "ACTIVE");
    }
    if (ImGui::Button("Trigger invert now"))
        composer_->forceGeneratorInvertMoment();
    ImGui::Separator();
    ImGui::Checkbox("Polarity invert", &p.polarityInvertEnabled);
    compactSliderFloat("Polarity interval", "##PolarityInterval",
                       &p.polarityInvertInterval, 10.f, 600.f, "%.0f s");
    compactSliderFloat("Polarity duration", "##PolarityDuration",
                       &p.polarityInvertDuration, 1.f, 60.f, "%.1f s");
    if (composer_->polarityInvertActive()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.f, 1.f), "ACTIVE");
    }
    if (ImGui::Button("Trigger polarity now"))
        composer_->forcePolarityInvertMoment();

    ImGui::NextColumn();
    ImGui::TextDisabled("RHYTHM / IMAGE");
    compactSliderFloat("BPM", "##ComposerBpm", &p.bpm, 20.f, 240.f, "%.1f");
    compactSliderInt("Subdivision", "##ComposerSub", &p.beatSubdivision, 1, 16);
    for (Channel* channel : channels_) {
        if (!channel) continue;
        compactSliderFloat("Intensity", ("##GenIntensity" + ofToString(channel->getIdx())).c_str(),
                           &channel->generatorParams().intensity, 0.f, 1.f);
        compactSliderFloat("Density", ("##GenDensity" + ofToString(channel->getIdx())).c_str(),
                           &channel->generatorParams().density, 0.f, 1.f);
        compactSliderFloat("Glow", ("##GenGlow" + ofToString(channel->getIdx())).c_str(),
                           &channel->generatorParams().glowGain, 0.f, 1.5f);
        compactSliderFloat("Glow radius", ("##GenGlowRadius" + ofToString(channel->getIdx())).c_str(),
                           &channel->generatorParams().glowRadius, 0.25f, 4.f);
        compactSliderFloat("Feedback decay", ("##GenFeedback" + ofToString(channel->getIdx())).c_str(),
                           &channel->generatorParams().feedbackDecay, 0.f, 0.94f);
        break;
    }
    if (!channels_.empty() && channels_[0]) {
        GeneratorRuntimeParams& editable = channels_[0]->generatorParams();
        // Solo niveles de gris: la instalación no admite crominancia, así que la
        // paleta se expone como dos valores de exposición, no como selectores de color.
        int primary = editable.cyan.r;
        int secondary = editable.red.r;
        if (compactSliderInt("Primary level", "##GenPrimaryLevel", &primary,
                             0, 255))
            editable.cyan = ofColor(static_cast<unsigned char>(primary));
        if (compactSliderInt("Secondary level", "##GenSecondaryLevel",
                             &secondary, 0, 255))
            editable.red = ofColor(static_cast<unsigned char>(secondary));
        bool inverted = channels_[0]->invertPolarity();
        if (ImGui::Checkbox("Invert polarity (black on white)", &inverted)) {
            for (Channel* channel : channels_) {
                if (channel) channel->setInvertPolarity(inverted);
            }
        }
        const GeneratorRuntimeParams source = editable;
        for (std::size_t i = 1; i < channels_.size(); ++i) {
            if (!channels_[i]) continue;
            channels_[i]->generatorParams().intensity = source.intensity;
            channels_[i]->generatorParams().density = source.density;
            channels_[i]->generatorParams().glowGain = source.glowGain;
            channels_[i]->generatorParams().glowRadius = source.glowRadius;
            channels_[i]->generatorParams().feedbackDecay =
                source.feedbackDecay;
            channels_[i]->generatorParams().cyan = source.cyan;
            channels_[i]->generatorParams().red = source.red;
        }
    }
    ImGui::Columns(1);

    sectionTitle("CHANNEL CHAPTERS");
    for (int i = 0; i < composer_->channelCount(); ++i) {
        const ChapterState& state = composer_->stateFor(i);
        const int content = ofClamp(static_cast<int>(state.content), 0, 3);
        const int stage = ofClamp(static_cast<int>(state.stage), 0, 4);
        const int organization =
            ofClamp(static_cast<int>(state.organization), 0, 3);
        const int generator = ofClamp(state.generator, 0, 11);
        ImGui::Text("CH %d  %s  %s  %s  %.0f%%",
                    i, contentNames[content],
                    content == static_cast<int>(ContentType::Video)
                        ? "-"
                        : generatorNames[generator],
                    stageNames[stage], state.stageProgress * 100.f);
        ImGui::SameLine(570.f);
        ImGui::TextDisabled("%s  beat %llu  rev %llu",
                            organizationNames[organization],
                            static_cast<unsigned long long>(state.beatIndex),
                            static_cast<unsigned long long>(state.revision));
    }
    ImGui::EndChild();
}

void ControlApp::drawPerformancePanel() {
    ImGui::Spacing();
    ImGui::Text("PERFORMANCE TEST");
    ImGui::TextDisabled(
        "Production frame pacing, subsystem cost, CPU and memory");
    ImGui::Spacing();

    const bool running = performance_->isRunning();
    if (!running) {
        const float minutes = performance_->config().durationSeconds / 60.f;
        const std::string startLabel =
            "Start " + ofToString(minutes, minutes < 1.f ? 1 : 0) +
            "-minute stress test";
        if (ImGui::Button(startLabel.c_str(), ImVec2(240.f, 34.f)))
            performance_->start(channels_, composer_);
    } else {
        if (ImGui::Button("Stop test", ImVec2(140.f, 34.f)))
            performance_->stop(channels_, composer_, false);
    }
    ImGui::SameLine();
    if (!running) {
        if (ImGui::Button("Reset results", ImVec2(140.f, 34.f)))
            performance_->reset();
    } else {
        ImGui::TextDisabled("Reset disabled while test is running");
    }
    ImGui::SameLine();
    if (performance_->hasResults() && ImGui::Button("Export report"))
        performance_->exportReport();

    const float elapsed = performance_->elapsedSeconds();
    const float duration = performance_->config().durationSeconds;
    ImGui::ProgressBar(performance_->progress(), ImVec2(-1.f, 20.f));
    ImGui::Text("Elapsed %.1f / %.1f s", elapsed, duration);
    ImGui::SameLine();
    ImGui::TextDisabled("Phase: %s",
                        performance_->currentPhaseName().c_str());

    ImGui::Separator();
    ImGui::Text("Process CPU: %.1f%%", performance_->cpuPercent());
    ImGui::SameLine(240.f);
    ImGui::Text("Memory: %.1f MB", performance_->residentMemoryMB());
    ImGui::SameLine(440.f);
    ImGui::Text("Growth: %+.1f MB", performance_->memoryGrowthMB());
    ImGui::Text("Slowest measured subsystem: %s",
                performance_->slowestSubsystem().c_str());
    ImGui::Text("Slowest channel: %s",
                performance_->slowestChannel().c_str());
    ImGui::SameLine(430.f);
    ImGui::Text("Slowest visual mode: %s",
                performance_->slowestVisualMode().c_str());

    ImGui::Spacing();
    ImGui::Text("PRESENTATION WINDOWS");
    for (int i = 0; i < PerformanceMonitor::kMaxWindows; ++i) {
        const auto& summary = performance_->windowSummary(i);
        if (summary.fps <= 0.f && summary.sampleCount == 0) continue;
        ImGui::PushID(1000 + i);
        ImGui::BeginChild("WindowPerf", ImVec2(0.f, 76.f), true);
        ImGui::Text("Window %d", i);
        ImGui::SameLine(110.f);
        ImGui::Text("FPS %.2f", summary.fps);
        ImGui::SameLine(220.f);
        ImGui::Text("Frame avg %.2f ms", summary.averageFrameMs);
        ImGui::SameLine(410.f);
        ImGui::Text("p95 %.2f / p99 %.2f ms",
                    summary.p95FrameMs, summary.p99FrameMs);
        ImGui::Text("Update %.2f ms  Draw %.2f ms  GPU %.2f ms",
                    summary.updateMs, summary.drawMs, summary.gpuMs);
        ImGui::SameLine(430.f);
        ImGui::Text("Late %llu  Stalls %llu",
                    static_cast<unsigned long long>(summary.lateFrames),
                    static_cast<unsigned long long>(summary.stalls));
        ImGui::EndChild();
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Text("CHANNEL COSTS");
    for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
        const auto& sample = performance_->channelLatest(i);
        ImGui::Text(
            "CH%d total %5.2f | decode %5.2f | render %5.2f | read %5.2f | "
            "CV %5.2f | score %5.2f | OSC %5.2f | draw %5.2f ms%s",
            i, sample.totalMs, sample.decoderMs, sample.videoRenderMs,
            sample.readbackMs, sample.cvMs, sample.scoreMs, sample.oscMs,
            sample.drawMs, sample.analyzed ? "  CV frame" : "");
    }

    if (performance_->hasResults()) {
        ImGui::Separator();
        const bool passed = performance_->passed();
        ImGui::TextColored(
            passed ? ImVec4(0.35f, 0.85f, 0.55f, 1.f)
                   : ImVec4(1.f, 0.35f, 0.35f, 1.f),
            "%s", passed ? "PASS" : "FAIL");
        for (const auto& reason : performance_->failureReasons())
            ImGui::BulletText("%s", reason.c_str());
        if (!performance_->lastReportPath().empty())
            ImGui::TextWrapped("Report: %s",
                               performance_->lastReportPath().c_str());
    }
}

void ControlApp::drawChannelPanel(int i) {
    Channel* ch = channels_[i];
    if (!ch) return;

    std::string clipName = ofFilePath::getFileName(ch->getCurrentClip());
    ImGui::Text("CHANNEL %d", i);
    ImGui::SameLine();
    ImGui::TextDisabled("| %s", ch->isPlaying() ? "PLAYING" : "STOPPED");
    ImGui::TextDisabled("%s", clipName.empty() ? "No clip loaded" : clipName.c_str());

    if (ImGui::Button(("Next##" + ofToString(i)).c_str())) ch->loadNextClip();
    ImGui::SameLine();
    bool playing = ch->isPlaying();
    if (ImGui::Button(playing ? ("Pause##" + ofToString(i)).c_str()
                               : ("Play##"  + ofToString(i)).c_str()))
        playing ? ch->pause() : ch->play();
    ImGui::SameLine();
    if (ImGui::Button(("Stop##" + ofToString(i)).c_str())) ch->stop();

    float spd = ch->getBaseSpeed();
    if (compactSliderFloat("Speed", "##Speed", &spd, 0.1f, 4.f))
        ch->setSpeed(spd);

    VideoPointCloudSettings& vpc = ch->videoPointCloudSettings();
    sectionTitle("VIDEO POINT CLOUD");
    ImGui::Checkbox("Enabled##Vpc", &vpc.enabled);
    const char* vpcPresets[] = {
        "Luminance Relief", "Player Extraction", "Hybrid Stadium Field"};
    int vpcPreset = ofClamp(vpc.preset, 0, 2);
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::Combo("Preset##Vpc", &vpcPreset, vpcPresets, 3))
        vpc.applyPreset(vpcPreset);
    ImGui::SameLine();
    if (ImGui::Button("Reset feedback##Vpc"))
        ch->resetVideoPointCloudFeedback();
    compactSliderInt("Grid W", "##VpcGridW", &vpc.gridWidth, 64, 384);
    compactSliderInt("Grid H", "##VpcGridH", &vpc.gridHeight, 64, 384);
    compactSliderFloat("Depth", "##VpcDepth", &vpc.depthScale, 0.f, 3.f);
    compactSliderFloat("Point", "##VpcPoint", &vpc.pointSize, 0.5f, 6.f);
    compactSliderFloat("Luma floor", "##VpcLuma", &vpc.luminanceFloor, 0.f, 0.5f);
    compactSliderFloat("Color gain", "##VpcColor", &vpc.colorGain, 0.f, 3.f);
    compactSliderFloat("Yaw", "##VpcYaw", &vpc.cameraYaw, -90.f, 90.f);
    compactSliderFloat("Distance", "##VpcDistance", &vpc.cameraDistance, 0.5f, 5.f);
    ImGui::Checkbox("Feedback##Vpc", &vpc.feedbackEnabled);
    if (vpc.feedbackEnabled)
        compactSliderFloat("Decay", "##VpcDecay", &vpc.feedbackDecay,
                           0.f, 0.995f);

    CVParams& cvp = ch->getCVPipeline().params();
    EventDetector::Params& ep = ch->getDetector().params();
    DatamaticsParams& sp = ch->getScore().params();
    const CVData& d = ch->getCVPipeline().getData();

    const float controlsGap = ImGui::GetStyle().ItemSpacing.x;
    const float controlsWidth =
        (ImGui::GetContentRegionAvail().x - controlsGap * 2.f) / 3.f;
    ImGui::BeginChild("ImageControls", ImVec2(controlsWidth, 0.f), false,
                      ImGuiWindowFlags_NoScrollbar);

    sectionTitle("IMAGE");
    compactSliderFloat("Contrast", "##Contrast", &cvp.bwContrast, 0.5f, 3.f);
    compactSliderFloat("Bright", "##Brightness", &cvp.bwBrightness, -0.5f, 0.5f);
    compactSliderFloat("Gamma", "##Gamma", &cvp.bwGamma, 0.3f, 2.2f);
    compactSliderFloat("S curve", "##SCurve", &cvp.bwSCurve, 0.f, 1.f);
    compactSliderFloat("Grain", "##Grain", &cvp.bwGrain, 0.f, 0.15f, "%.3f");
    compactSliderFloat("Vignette", "##Vignette", &cvp.bwVignette, 0.f, 1.f);
    compactSliderFloat("Threshold", "##Threshold", &cvp.bwThreshold, 0.f, 1.f);
    compactSliderFloat("Posterize", "##Posterize", &cvp.bwPosterize, 2.f, 256.f, "%.0f");
    ImGui::Checkbox("CLAHE", &cvp.useCLAHE);
    compactSliderFloat("Clip", "##ClaheClip", &cvp.claheClipLimit, 1.f, 8.f);

    sectionTitle("COMPUTER VISION");
    ImGui::Checkbox("Enabled", &cvp.enabled);
    compactSliderFloat("Blob thr", "##BlobThreshold", &cvp.blobThreshold, 10.f, 200.f, "%.0f");
    compactSliderFloat("Min area", "##MinArea", &cvp.blobMinArea, 100.f, 10000.f, "%.0f");
    compactSliderInt("Canny low", "##CannyLow", &cvp.cannyLow, 5, 120);
    compactSliderInt("Canny high", "##CannyHigh", &cvp.cannyHigh, 20, 240);

    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("EventControls", ImVec2(controlsWidth, 0.f), false,
                      ImGuiWindowFlags_NoScrollbar);

    sectionTitle("EVENTS");
    compactSliderFloat("Ball area", "##BallMaxArea", &ep.ballMaxArea, 100.f, 10000.f, "%.0f");
    compactSliderFloat("Ball speed", "##BallMinSpeed", &ep.ballMinSpeed, 0.001f, 0.1f, "%.3f");
    compactSliderInt("Crowd N", "##CrowdCount", &ep.crowdMinBlobs, 2, 20);
    compactSliderFloat("Crowd dist", "##CrowdDistance", &ep.crowdMaxDist, 0.05f, 0.5f);
    compactSliderFloat("Overlap", "##CollisionOverlap", &ep.collisionOverlap, 0.05f, 0.9f);

    bool col = d.events.collision;
    bool ball = d.events.ballDetected;
    ImGui::Text("COL %s  BALL %s", col ? "YES" : "---", ball ? "YES" : "---");
    ImGui::Text("Crowd %.2f  Leg %.2f",
                d.events.crowdDensity, d.events.legDistance);

    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("ScoreControls", ImVec2(controlsWidth, 0.f), false);

    sectionTitle("SCORE");
    compactSliderFloat("Min dur", "##MinDuration", &sp.minModeDuration, 1.f, 15.f);
    compactSliderFloat("Max dur", "##MaxDuration", &sp.maxModeDuration, 2.f, 30.f);
    compactSliderFloat("Flash", "##FlashDuration", &sp.flashDuration, 0.05f, 1.f);
    compactSliderFloat("Opacity", "##Opacity", &sp.markOpacity, 0.f, 1.f);

    static const int modeValues[] = {
        -1,
        (int)ScoreMode::BwClean,
        (int)ScoreMode::ScanLine,
        (int)ScoreMode::BBoxTracker,
        (int)ScoreMode::BinaryText,
        (int)ScoreMode::Waveform,
        (int)ScoreMode::GridData,
        (int)ScoreMode::Barcode,
        (int)ScoreMode::VideoNormal,
        (int)ScoreMode::VideoSquares,
        (int)ScoreMode::VideoNumbers,
        (int)ScoreMode::VideoLines,
        (int)ScoreMode::ThermalVision,
        (int)ScoreMode::Flash,
    };
    static constexpr int modeValueCount = sizeof(modeValues) / sizeof(modeValues[0]);
    static const char* modeItems =
        "Auto\0BwClean\0ScanLine\0BBoxTracker\0BinaryText\0Waveform\0GridData\0Barcode\0"
        "VideoNormal\0VideoSquares\0VideoNumbers\0VideoLines\0ThermalVision\0Flash\0";
    int modeOvr = 0;
    for (int i = 0; i < modeValueCount; i++) {
        if (modeValues[i] == sp.forcedMode) {
            modeOvr = i;
            break;
        }
    }
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::Combo("##Mode", &modeOvr, modeItems))
        sp.forcedMode = modeValues[modeOvr];

    int curMode = (int)ch->getScore().currentMode();
    ImGui::Text("Active: %s", kModeNames[std::min(curMode, (int)ScoreMode::Flash + 1)]);
    ofColor mc = sp.markColor;
    float mcf[3] = { mc.r / 255.f, mc.g / 255.f, mc.b / 255.f };
    ImGui::SetNextItemWidth(-1.f);
    ImGui::ColorEdit3("##MarkColor", mcf,
                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);

    compactSliderFloat("Square", "##SquareSize", &sp.videoSquareSize, 60.f, 400.f, "%.0f");
    compactSliderInt("Sq count", "##SquareCount", &sp.videoSquareCount, 1, 12);

    sectionTitle("LIVE CV DATA");
    ImGui::Text("Flow %.3f  Angle %.2f", d.flowMagnitude, d.flowAngle);
    ImGui::Text("Energy %.3f  Blobs %d", d.motionEnergy, d.blobCount);
    ImGui::Text("Contour %.0f", d.contourLength);

    ImGui::EndChild();
}
