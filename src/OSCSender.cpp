#include "OSCSender.h"

void OSCSender::setup(const std::string& host, int port) {
    host_ = host;
    port_ = port;
    reconnect();
}

void OSCSender::reconnect() {
    sender_.setup(host_, port_);
    ofLogNotice("OSCSender") << "Sending to " << host_ << ":" << port_;
}

void OSCSender::send(const CVData& d) {
    int ch = d.channelIdx;
    std::string base = "/pdj/channel/" + ofToString(ch);

    ofxOscBundle coreBundle;
    ofxOscBundle blobBundle;
    ofxOscBundle contextBundle;
    ofxOscBundle generatorBundle;

    auto addFloat = [](ofxOscBundle& target, const std::string& addr, float v) {
        ofxOscMessage m;
        m.setAddress(addr);
        m.addFloatArg(v);
        target.addMessage(m);
    };
    auto addInt = [](ofxOscBundle& target, const std::string& addr, int v) {
        ofxOscMessage m;
        m.setAddress(addr);
        m.addIntArg(v);
        target.addMessage(m);
    };
    auto addString = [](ofxOscBundle& target, const std::string& addr, const std::string& v) {
        ofxOscMessage m;
        m.setAddress(addr);
        m.addStringArg(v);
        target.addMessage(m);
    };

    // Sequence and time let the receiver detect loss, reordering, or staleness.
    addInt  (coreBundle, base + "/state/frame", d.frameSequence);
    addFloat(coreBundle, base + "/state/time",  d.timestamp);

    // Flow + motion
    addFloat(coreBundle, base + "/flow/magnitude", d.flowMagnitude);
    addFloat(coreBundle, base + "/flow/angle",     d.flowAngle);
    addFloat(coreBundle, base + "/motion/energy",  d.motionEnergy);

    // Blobs
    addInt  (coreBundle, base + "/blobs/count",    d.blobCount);
    addFloat(coreBundle, base + "/contour/length", d.contourLength);

    // Always publish all fixed slots. Clearing unused slots prevents a departed
    // detection from remaining indefinitely in the SuperCollider state. One
    // compact message per slot keeps the UDP datagram below fragmentation size.
    int maxBlobs = std::min((int)d.blobs.size(), 8);
    for (int i = 0; i < 8; i++) {
        std::string bp = base + "/blob/" + ofToString(i);
        bool active = i < maxBlobs;
        ofxOscMessage m;
        m.setAddress(bp + "/state");
        m.addIntArg(active ? 1 : 0);
        m.addFloatArg(active ? d.blobs[i].x    : 0.f);
        m.addFloatArg(active ? d.blobs[i].y    : 0.f);
        m.addFloatArg(active ? d.blobs[i].vx   : 0.f);
        m.addFloatArg(active ? d.blobs[i].vy   : 0.f);
        m.addFloatArg(active ? d.blobs[i].area : 0.f);
        m.addFloatArg(active ? d.blobs[i].bbW  : 0.f);
        m.addFloatArg(active ? d.blobs[i].bbH  : 0.f);
        blobBundle.addMessage(m);
    }

    // Events
    addInt  (coreBundle, base + "/event/collision",    d.events.collision    ? 1 : 0);
    addInt  (coreBundle, base + "/event/ball",         d.events.ballDetected ? 1 : 0);
    addFloat(coreBundle, base + "/event/ball/x",       d.events.ballPos.x);
    addFloat(coreBundle, base + "/event/ball/y",       d.events.ballPos.y);
    addFloat(coreBundle, base + "/event/crowd",        d.events.crowdDensity);
    addFloat(coreBundle, base + "/event/leg_distance", d.events.legDistance);

    // Video, visual score, and shared performance moment. Revisions remain in
    // every snapshot, so a dropped change packet is recovered by the next one.
    addString(contextBundle, base + "/video/name",         d.videoName);
    addFloat (contextBundle, base + "/video/position",     d.videoPosition);
    addFloat (contextBundle, base + "/video/duration",     d.videoDuration);
    addInt   (contextBundle, base + "/video/revision",     d.videoRevision);
    addInt   (contextBundle, base + "/video/plan_type",    d.videoPlanType);
    addInt   (contextBundle, base + "/video/shared",       d.videoShared);
    addInt   (contextBundle, base + "/score/mode",         d.scoreMode);
    addInt   (contextBundle, base + "/score/revision",     d.scoreRevision);
    addInt   (contextBundle, base + "/director/temporal",  d.temporalPhase);
    addFloat (contextBundle, base + "/director/speed",     d.speedMultiplier);
    addInt   (contextBundle, base + "/director/clear",     d.clearPhase);
    addFloat (contextBundle, base + "/director/clear_alpha", d.clearAlpha);
    addInt   (generatorBundle, base + "/generator/active",       d.generatorActive);
    addInt   (generatorBundle, base + "/generator/mode",         d.generatorMode);
    addInt   (generatorBundle, base + "/generator/revision",     d.generatorRevision);
    addInt   (generatorBundle, base + "/generator/organization", d.organizationMode);
    addInt   (generatorBundle, base + "/generator/role",         d.screenRole);
    addInt   (generatorBundle, base + "/generator/stage",        d.generatorStage);
    addFloat (generatorBundle, base + "/generator/stage_progress",
              d.generatorStageProgress);
    addFloat (generatorBundle, base + "/generator/beat_phase",   d.generatorBeatPhase);
    addInt   (generatorBundle, base + "/generator/beat_index",   d.generatorBeatIndex);
    addFloat (generatorBundle, base + "/generator/subdivision",  d.generatorSubdivisionPulse);
    addFloat (generatorBundle, base + "/generator/envelope",     d.generatorEnvelope);
    addInt   (generatorBundle, base + "/generator/seed",         d.generatorSeed);
    addInt   (generatorBundle, base + "/generator/transition",   d.transitionActive);

    // Four small packets avoid IP fragmentation on a typical Ethernet LAN.
    sender_.sendBundle(coreBundle);
    sender_.sendBundle(blobBundle);
    sender_.sendBundle(contextBundle);
    sender_.sendBundle(generatorBundle);
}
