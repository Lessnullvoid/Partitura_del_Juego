#include "ControlApp.h"

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
}

static const char* kModeNames[] = {
    "BwClean", "ScanLine", "BBoxTracker", "BinaryText",
    "Waveform", "GridData", "Barcode",
    "VideoNormal", "VideoSquares", "VideoNumbers", "VideoLines",
    "ThermalVision", "SlitScan", "Flash", "Auto"
};

void ControlApp::setup() {
    ofSetBackgroundColor(15, 15, 15);
    gui_.setup();

    ImGuiIO& io = ImGui::GetIO();
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
}

void ControlApp::update() {}

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
    if (ImGui::Selectable("Channel Editor", activePage_ == 3, 0,
                          ImVec2(170.f, 30.f)))
        activePage_ = 3;
    ImGui::Separator();

    if (activePage_ == 0) {
        drawOverview();
    } else if (activePage_ == 1 && dir_) {
        drawDirectorPanel();
    } else if (activePage_ == 2 && videoDir_) {
        drawVideoDirectorPanel();
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
    if (fontTexture_ != 0) {
        glDeleteTextures(1, &fontTexture_);
        fontTexture_ = 0;
    }
}

void ControlApp::keyPressed(int key) {
    if (key == 'u' || key == 'U') showUI_ = !showUI_;
}

void ControlApp::drawGlobalPanel() {
    ImGui::BeginChild("GlobalBar", ImVec2(0.f, 72.f), true,
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
    ImGui::SameLine(ImGui::GetWindowWidth() - 108.f);
    ImGui::TextDisabled("U: hide UI");
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
    const float cardWidth =
        (available.x - gap * std::max(0, channelCount - 1)) /
        std::max(1, channelCount);

    for (int i = 0; i < channelCount; i++) {
        Channel* ch = channels_[i];
        if (!ch) continue;
        if (i > 0) ImGui::SameLine();

        ImGui::PushID(i);
        ImGui::BeginChild("OverviewCard", ImVec2(cardWidth, available.y), true);

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

    int modeOvr = sp.forcedMode + 1;
    static const char* modeItems =
        "Auto\0BwClean\0ScanLine\0BBoxTracker\0BinaryText\0Waveform\0GridData\0Barcode\0"
        "VideoNormal\0VideoSquares\0VideoNumbers\0VideoLines\0ThermalVision\0SlitScan\0Flash\0";
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::Combo("##Mode", &modeOvr, modeItems))
        sp.forcedMode = modeOvr - 1;

    int curMode = (int)ch->getScore().currentMode();
    ImGui::Text("Active: %s", kModeNames[std::min(curMode, (int)ScoreMode::Flash + 1)]);
    ofColor mc = sp.markColor;
    float mcf[3] = { mc.r / 255.f, mc.g / 255.f, mc.b / 255.f };
    ImGui::SetNextItemWidth(-1.f);
    ImGui::ColorEdit3("##MarkColor", mcf,
                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);

    compactSliderFloat("Square", "##SquareSize", &sp.videoSquareSize, 60.f, 400.f, "%.0f");
    compactSliderInt("Sq count", "##SquareCount", &sp.videoSquareCount, 1, 12);
    compactSliderInt("Slit W", "##SlitWidth", &sp.slitStripW, 1, 12);

    sectionTitle("LIVE CV DATA");
    ImGui::Text("Flow %.3f  Angle %.2f", d.flowMagnitude, d.flowAngle);
    ImGui::Text("Energy %.3f  Blobs %d", d.motionEnergy, d.blobCount);
    ImGui::Text("Contour %.0f", d.contourLength);

    ImGui::EndChild();
}
