#include "ofApp.h"

void ChannelApp::setup() {
    ofSetBackgroundColor(0);
    ofSetFrameRate(30);
    ofHideCursor();
    ch_->setup(idx_, w_, h_, pool_, osc_, cvp_);
    if (dir_) ch_->setGlobalDirector(dir_);
}

void ChannelApp::update() {
    ch_->update();
}

void ChannelApp::draw() {
    ch_->draw();
}
