#include "ControlApp.h"
#include <fstream>

static const char* kModeNames[] = {
    "BwClean", "ScanLine", "BBoxTracker", "BinaryText",
    "Waveform", "GridData", "Barcode",
    "VideoNormal", "VideoSquares", "VideoNumbers", "VideoLines",
    "ThermalVision", "SlitScan", "Flash", "Auto"
};

void ControlApp::setup() {
    ofSetBackgroundColor(15, 15, 15);
    gui_.setup();
    strncpy(oscHostBuf_, osc_->getHost().c_str(), sizeof(oscHostBuf_) - 1);
    oscPort_ = osc_->getPort();
}

void ControlApp::update() {}

void ControlApp::draw() {
    if (!showUI_) return;
    gui_.begin();
    drawGlobalPanel();
    if (dir_) drawDirectorPanel();
    for (int i = 0; i < (int)channels_.size(); i++) drawChannelPanel(i);
    gui_.end();
}

void ControlApp::exit() {}

void ControlApp::keyPressed(int key) {
    if (key == 'u' || key == 'U') showUI_ = !showUI_;
}

void ControlApp::drawGlobalPanel() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 160), ImGuiCond_FirstUseEver);
    ImGui::Begin("Global");

    ImGui::Separator(); ImGui::Text("OSC Output");
    ImGui::InputText("Host", oscHostBuf_, sizeof(oscHostBuf_));
    ImGui::InputInt("Port", &oscPort_);
    if (ImGui::Button("Apply OSC")) {
        osc_->setHost(oscHostBuf_);
        osc_->setPort(oscPort_);
    }
    ImGui::Separator(); ImGui::Text("Clip Pool");
    ImGui::Text("Clips available: %d", pool_->totalClips());
    ImGui::End();
}

void ControlApp::drawDirectorPanel() {
    if (!dir_) return;
    GlobalDirectorParams& p = dir_->params();

    ImGui::SetNextWindowPos(ImVec2(10, 180), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(440, 380), ImGuiCond_FirstUseEver);
    ImGui::Begin("Global Director");

    // --- Live status ---
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
    ImGui::Text("Temporal: %s  x%.2f    Clear: %s  a%.0f",
                tName, spd, cName, dir_->getClearAlpha());

    // --- Manual triggers ---
    ImGui::Separator(); ImGui::Text("Manual Triggers");
    if (ImGui::Button("Slow Mo")) {
        // #region agent log
        { std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"B\",\"location\":\"ControlApp.cpp:83\",\"message\":\"SlowMo pressed\",\"data\":{\"tPhase\":" << (int)dir_->temporalPhase() << ",\"accepted\":" << (dir_->temporalPhase()==TemporalPhase::Idle?1:0) << "},\"timestamp\":" << (long long)(ofGetElapsedTimef()*1000) << "}\n"; }
        // #endregion
        dir_->triggerSlow();
    }
    ImGui::SameLine();
    if (ImGui::Button("Fast Fwd")) {
        // #region agent log
        { std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"B\",\"location\":\"ControlApp.cpp:85\",\"message\":\"FastFwd pressed\",\"data\":{\"tPhase\":" << (int)dir_->temporalPhase() << ",\"accepted\":" << (dir_->temporalPhase()==TemporalPhase::Idle?1:0) << "},\"timestamp\":" << (long long)(ofGetElapsedTimef()*1000) << "}\n"; }
        // #endregion
        dir_->triggerFast();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Black")) {
        // #region agent log
        { std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"B\",\"location\":\"ControlApp.cpp:87\",\"message\":\"ClearBlack pressed\",\"data\":{\"cPhase\":" << (int)dir_->clearPhase() << ",\"accepted\":" << (dir_->clearPhase()==ClearPhase::Idle?1:0) << "},\"timestamp\":" << (long long)(ofGetElapsedTimef()*1000) << "}\n"; }
        // #endregion
        dir_->triggerClear(ofColor(0, 0, 0));
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Red")) {
        // #region agent log
        { std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"B\",\"location\":\"ControlApp.cpp:89\",\"message\":\"ClearRed pressed\",\"data\":{\"cPhase\":" << (int)dir_->clearPhase() << ",\"accepted\":" << (dir_->clearPhase()==ClearPhase::Idle?1:0) << "},\"timestamp\":" << (long long)(ofGetElapsedTimef()*1000) << "}\n"; }
        // #endregion
        dir_->triggerClear(ofColor(150, 12, 12));
    }

    // --- Auto-trigger toggles ---
    ImGui::Separator(); ImGui::Text("Auto Triggers");
    ImGui::Checkbox("Auto Slow",  &p.autoSlow);
    ImGui::SameLine();
    ImGui::Checkbox("Auto Fast",  &p.autoFast);
    ImGui::SameLine();
    ImGui::Checkbox("Auto Clear", &p.autoClear);

    // --- Slow-mo params ---
    ImGui::Separator(); ImGui::Text("Slow Motion");
    ImGui::SliderFloat("Slow Speed",    &p.slowSpeed,       0.05f, 0.8f);
    ImGui::SliderFloat("Slow Ramp Dn",  &p.slowRampDownDur, 0.2f,  4.f);
    ImGui::SliderFloat("Slow Hold Min", &p.slowHoldMin,     1.f,   15.f);
    ImGui::SliderFloat("Slow Hold Max", &p.slowHoldMax,     2.f,   20.f);
    ImGui::SliderFloat("Slow Ramp Up",  &p.slowRampUpDur,   0.2f,  5.f);
    ImGui::SliderFloat("Slow Ivl Min",  &p.slowIntervalMin, 10.f,  120.f);
    ImGui::SliderFloat("Slow Ivl Max",  &p.slowIntervalMax, 15.f,  180.f);

    // --- Fast-forward params ---
    ImGui::Separator(); ImGui::Text("Fast Forward");
    ImGui::SliderFloat("Fast Speed",    &p.fastSpeed,       1.2f,  5.f);
    ImGui::SliderFloat("Fast Ramp Up",  &p.fastRampUpDur,   0.1f,  2.f);
    ImGui::SliderFloat("Fast Hold Min", &p.fastHoldMin,     0.3f,  6.f);
    ImGui::SliderFloat("Fast Hold Max", &p.fastHoldMax,     0.5f,  10.f);
    ImGui::SliderFloat("Fast Ramp Dn",  &p.fastRampDownDur, 0.1f,  2.f);
    ImGui::SliderFloat("Fast Ivl Min",  &p.fastIntervalMin, 10.f,  120.f);
    ImGui::SliderFloat("Fast Ivl Max",  &p.fastIntervalMax, 15.f,  180.f);

    // --- Clear params ---
    ImGui::Separator(); ImGui::Text("Screen Clear");
    ImGui::SliderFloat("Clr FadeIn",   &p.clearFadeInDur,   0.05f, 2.f);
    ImGui::SliderFloat("Clr Hold Min", &p.clearHoldMin,     0.1f,  4.f);
    ImGui::SliderFloat("Clr Hold Max", &p.clearHoldMax,     0.2f,  8.f);
    ImGui::SliderFloat("Clr FadeOut",  &p.clearFadeOutDur,  0.05f, 3.f);
    ImGui::SliderFloat("Clr Ivl Min",  &p.clearIntervalMin, 15.f,  180.f);
    ImGui::SliderFloat("Clr Ivl Max",  &p.clearIntervalMax, 20.f,  300.f);

    ImGui::End();
}

void ControlApp::drawChannelPanel(int i) {
    Channel* ch = channels_[i];
    if (!ch) return;

    std::string title = "Channel " + ofToString(i);
    ImGui::SetNextWindowSize(ImVec2(440, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10 + i * 450, 180), ImGuiCond_FirstUseEver);
    ImGui::Begin(title.c_str());

    // Transport
    std::string clipName = ofFilePath::getFileName(ch->getCurrentClip());
    ImGui::Text("Clip: %s", clipName.empty() ? "(none)" : clipName.c_str());
    ImGui::SameLine();
    if (ImGui::Button(("Next##" + ofToString(i)).c_str())) ch->loadNextClip();

    bool playing = ch->isPlaying();
    if (ImGui::Button(playing ? ("Pause##" + ofToString(i)).c_str()
                               : ("Play##"  + ofToString(i)).c_str()))
        playing ? ch->pause() : ch->play();
    ImGui::SameLine();
    if (ImGui::Button(("Stop##" + ofToString(i)).c_str())) ch->stop();

    float spd = ch->getBaseSpeed();
    if (ImGui::SliderFloat(("Speed##" + ofToString(i)).c_str(), &spd, 0.1f, 4.f))
        ch->setSpeed(spd);

    // Image
    ImGui::Separator(); ImGui::Text("Image");
    CVParams& cvp = ch->getCVPipeline().params();
    ImGui::SliderFloat(("Contrast##"   + ofToString(i)).c_str(),&cvp.bwContrast,   0.5f, 3.f);
    ImGui::SliderFloat(("Brightness##" + ofToString(i)).c_str(),&cvp.bwBrightness,-0.5f, 0.5f);
    ImGui::SliderFloat(("Gamma##"      + ofToString(i)).c_str(),&cvp.bwGamma,      0.3f, 2.2f);
    ImGui::SliderFloat(("SCurve##"     + ofToString(i)).c_str(),&cvp.bwSCurve,     0.0f, 1.0f);
    ImGui::SliderFloat(("Grain##"      + ofToString(i)).c_str(),&cvp.bwGrain,      0.0f, 0.15f);
    ImGui::SliderFloat(("Vignette##"   + ofToString(i)).c_str(),&cvp.bwVignette,   0.0f, 1.0f);
    ImGui::Separator(); ImGui::Text("Stylised (off by default)");
    ImGui::SliderFloat(("Threshold##"  + ofToString(i)).c_str(),&cvp.bwThreshold,  0.0f, 1.f);
    ImGui::SliderFloat(("Posterize##"  + ofToString(i)).c_str(),&cvp.bwPosterize,  2.f, 256.f);
    ImGui::Checkbox   (("CLAHE##"      + ofToString(i)).c_str(),&cvp.useCLAHE);
    ImGui::SameLine();
    ImGui::SliderFloat(("Clip##"       + ofToString(i)).c_str(),&cvp.claheClipLimit,1.f,8.f);

    // CV
    ImGui::Separator(); ImGui::Text("CV");
    ImGui::Checkbox  (("CV On##"       + ofToString(i)).c_str(),&cvp.enabled);
    ImGui::SliderFloat(("BlobThr##"    + ofToString(i)).c_str(),&cvp.blobThreshold, 10.f,200.f);
    ImGui::SliderFloat(("MinArea##"    + ofToString(i)).c_str(),&cvp.blobMinArea,   100.f,10000.f);
    ImGui::SliderInt  (("CannyLo##"    + ofToString(i)).c_str(),&cvp.cannyLow,   5, 120);
    ImGui::SliderInt  (("CannyHi##"    + ofToString(i)).c_str(),&cvp.cannyHigh,  20, 240);

    // Events
    ImGui::Separator(); ImGui::Text("Events");
    EventDetector::Params& ep = ch->getDetector().params();
    ImGui::SliderFloat(("BallMaxArea##"  + ofToString(i)).c_str(),&ep.ballMaxArea,   100.f,10000.f);
    ImGui::SliderFloat(("BallMinSpeed##" + ofToString(i)).c_str(),&ep.ballMinSpeed,  0.001f,0.1f);
    ImGui::SliderInt  (("CrowdN##"       + ofToString(i)).c_str(),&ep.crowdMinBlobs, 2, 20);
    ImGui::SliderFloat(("CrowdDist##"    + ofToString(i)).c_str(),&ep.crowdMaxDist,  0.05f,0.5f);
    ImGui::SliderFloat(("ColOverlap##"   + ofToString(i)).c_str(),&ep.collisionOverlap,0.05f,0.9f);

    const CVData& d = ch->getCVPipeline().getData();
    bool col = d.events.collision;
    bool ball = d.events.ballDetected;
    ImGui::Text("Collision: %s  Ball: %s  Crowd: %.2f  Leg: %.2f",
                col?"YES":"---", ball?"YES":"---",
                d.events.crowdDensity, d.events.legDistance);

    // Score Director
    ImGui::Separator(); ImGui::Text("Score");
    DatamaticsParams& sp = ch->getScore().params();
    ImGui::SliderFloat(("MinDur##"   + ofToString(i)).c_str(),&sp.minModeDuration, 1.f, 15.f);
    ImGui::SliderFloat(("MaxDur##"   + ofToString(i)).c_str(),&sp.maxModeDuration, 2.f, 30.f);
    ImGui::SliderFloat(("FlashDur##" + ofToString(i)).c_str(),&sp.flashDuration,   0.05f,1.f);
    ImGui::SliderFloat(("Opacity##"  + ofToString(i)).c_str(),&sp.markOpacity,     0.f, 1.f);

    // Mode override (+1 because -1 = Auto maps to combo index 0)
    int modeOvr = sp.forcedMode + 1;
    static const char* modeItems =
        "Auto\0BwClean\0ScanLine\0BBoxTracker\0BinaryText\0Waveform\0GridData\0Barcode\0"
        "VideoNormal\0VideoSquares\0VideoNumbers\0VideoLines\0ThermalVision\0SlitScan\0Flash\0";
    if (ImGui::Combo(("Mode##" + ofToString(i)).c_str(), &modeOvr, modeItems))
        sp.forcedMode = modeOvr - 1;

    int curMode = (int)ch->getScore().currentMode();
    ImGui::Text("Active: %s", kModeNames[std::min(curMode, (int)ScoreMode::Flash + 1)]);
    // Color indicator (read-only — cycles automatically on mode change)
    ofColor mc = sp.markColor;
    float mcf[3] = { mc.r / 255.f, mc.g / 255.f, mc.b / 255.f };
    ImGui::ColorEdit3(("MarkColor##" + ofToString(i)).c_str(), mcf,
                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);

    // VideoSquares tuning
    ImGui::SliderFloat(("SqSize##"  + ofToString(i)).c_str(), &sp.videoSquareSize,  60.f, 400.f);
    ImGui::SliderInt  (("SqCount##" + ofToString(i)).c_str(), &sp.videoSquareCount, 1, 12);

    // SlitScan tuning
    ImGui::SliderInt(("SlitW##" + ofToString(i)).c_str(), &sp.slitStripW, 1, 12);

    // Live data
    ImGui::Separator(); ImGui::Text("CV Data");
    ImGui::Text("Flow mag: %.3f  angle: %.2f", d.flowMagnitude, d.flowAngle);
    ImGui::Text("Energy: %.3f  Blobs: %d", d.motionEnergy, d.blobCount);
    ImGui::Text("Contour len: %.0f", d.contourLength);

    ImGui::End();
}
