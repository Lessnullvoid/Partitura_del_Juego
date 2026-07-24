#include "ofApp.h"

void ChannelApp::setup() {
    ofSetBackgroundColor(0);
    ofSetFrameRate(30);
    ofHideCursor();
    if (dir_) ch_->setGlobalDirector(dir_);
    if (videoDir_) ch_->setVideoDirector(videoDir_);
    ch_->setup(idx_, w_, h_, pool_, osc_, cvp_);
}

void ChannelApp::update() {
    ch_->update();
}

void ChannelApp::draw() {
    ch_->draw();
}
