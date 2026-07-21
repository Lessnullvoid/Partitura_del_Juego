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

    ofxOscBundle bundle;

    auto addFloat = [&](const std::string& addr, float v) {
        ofxOscMessage m;
        m.setAddress(addr);
        m.addFloatArg(v);
        bundle.addMessage(m);
    };
    auto addInt = [&](const std::string& addr, int v) {
        ofxOscMessage m;
        m.setAddress(addr);
        m.addIntArg(v);
        bundle.addMessage(m);
    };

    // Flow + motion
    addFloat(base + "/flow/magnitude", d.flowMagnitude);
    addFloat(base + "/flow/angle",     d.flowAngle);
    addFloat(base + "/motion/energy",  d.motionEnergy);

    // Blobs
    addInt  (base + "/blobs/count",    d.blobCount);
    addFloat(base + "/contour/length", d.contourLength);

    int maxBlobs = std::min((int)d.blobs.size(), 8);
    for (int i = 0; i < maxBlobs; i++) {
        std::string bp = base + "/blob/" + ofToString(i);
        addFloat(bp + "/x",    d.blobs[i].x);
        addFloat(bp + "/y",    d.blobs[i].y);
        addFloat(bp + "/vx",   d.blobs[i].vx);
        addFloat(bp + "/vy",   d.blobs[i].vy);
        addFloat(bp + "/area", d.blobs[i].area);
        addFloat(bp + "/bbW",  d.blobs[i].bbW);
        addFloat(bp + "/bbH",  d.blobs[i].bbH);
    }

    // Events
    addInt  (base + "/event/collision",    d.events.collision    ? 1 : 0);
    addInt  (base + "/event/ball",         d.events.ballDetected ? 1 : 0);
    addFloat(base + "/event/ball/x",       d.events.ballPos.x);
    addFloat(base + "/event/ball/y",       d.events.ballPos.y);
    addFloat(base + "/event/crowd",        d.events.crowdDensity);
    addFloat(base + "/event/leg_distance", d.events.legDistance);

    // Current visual mode
    addInt  (base + "/score/mode",         d.scoreMode);

    sender_.sendBundle(bundle);
}
