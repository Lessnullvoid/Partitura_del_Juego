#include "CVPipeline.h"

using namespace ofxCv;
using namespace cv;

void CVPipeline::setup(int fullW, int fullH, const CVParams& params) {
    params_ = params;
    fullW_  = fullW;
    fullH_  = fullH;
    w_ = params.halfRes ? fullW / 2 : fullW;
    h_ = params.halfRes ? fullH / 2 : fullH;

    bgSub_ = cv::createBackgroundSubtractorMOG2(params_.bgSubHistory, params_.bgSubThreshold, false);

    clahe_ = cv::createCLAHE(params_.claheClipLimit,
                             cv::Size(params_.claheTileSize, params_.claheTileSize));

    flow_.setWindowSize(params_.flowWindowSize);
    flow_.setNumLevels(3);
    flow_.setNumIterations(2);
    flow_.setPyramidScale(0.5f);
    flow_.setPolyN(5);
    flow_.setPolySigma(1.1f);

    contourFinder_.setMinArea(params_.blobMinArea);
    contourFinder_.setMaxArea(params_.blobMaxArea);
    contourFinder_.setThreshold(params_.blobThreshold);
    contourFinder_.getTracker().setPersistence(15);
    contourFinder_.getTracker().setMaximumDistance(50);

    firstFrame_ = true;
}

void CVPipeline::update(ofPixels& grayPixels) {
    if (!params_.enabled) return;
    if (grayPixels.getWidth() == 0) return;

    Mat full = toCv(grayPixels);

    if (params_.halfRes) {
        cv::resize(full, grayMat_, cv::Size(w_, h_), 0, 0, cv::INTER_LINEAR);
    } else {
        full.copyTo(grayMat_);
    }

    // Adaptive contrast enhancement
    if (params_.useCLAHE) {
        // Recreate if clip limit changed
        clahe_->setClipLimit(params_.claheClipLimit);
        clahe_->setTilesGridSize(cv::Size(params_.claheTileSize, params_.claheTileSize));
        clahe_->apply(grayMat_, grayMat_);
    } else if (params_.equalizeHist) {
        cv::equalizeHist(grayMat_, grayMat_);
    }

    // Background subtraction → foreground mask
    bgSub_->apply(grayMat_, fgMask_);

    // Blob / contour finding on foreground mask
    contourFinder_.findContours(fgMask_);

    // Cache bounding rects for EventDetector
    boundingRects_.clear();
    for (int i = 0; i < (int)contourFinder_.size(); i++) {
        boundingRects_.push_back(contourFinder_.getBoundingRect(i));
    }

    // Edge detection
    cv::Canny(grayMat_, edges_, params_.cannyLow, params_.cannyHigh);

    // Motion energy + optical flow
    float energy = 0.f;
    if (!firstFrame_) {
        flow_.calcOpticalFlow(grayMat_);
        Mat diff;
        cv::absdiff(grayMat_, prevGray_, diff);
        energy = (float)cv::mean(diff)[0] / 255.f;
    }

    grayMat_.copyTo(prevGray_);
    firstFrame_ = false;

    // Populate CVData
    data_.motionEnergy = energy;

    glm::vec2 avgFlow = flow_.getAverageFlow();
    data_.flowMagnitude = glm::length(avgFlow);
    data_.flowAngle     = atan2f(avgFlow.y, avgFlow.x);

    data_.blobCount = (int)contourFinder_.size();
    data_.blobs.clear();

    float scaleX = 1.f / (float)w_;
    float scaleY = 1.f / (float)h_;

    for (int i = 0; i < (int)contourFinder_.size(); i++) {
        cv::Point2f center = contourFinder_.getCentroid(i);
        cv::Vec2f   vel    = contourFinder_.getVelocity(i);
        cv::Rect    bb     = contourFinder_.getBoundingRect(i);

        CVData::BlobEntry b;
        b.x    = center.x * scaleX;
        b.y    = center.y * scaleY;
        b.vx   = vel[0] * scaleX;
        b.vy   = vel[1] * scaleY;
        b.area = (float)contourFinder_.getContourArea(i);
        b.bbW  = (float)bb.width  * scaleX;
        b.bbH  = (float)bb.height * scaleY;
        data_.blobs.push_back(b);
    }

    float totalLen = 0.f;
    for (int i = 0; i < (int)contourFinder_.size(); i++) {
        totalLen += (float)contourFinder_.getArcLength(i);
    }
    data_.contourLength = totalLen;
}

void CVPipeline::reset() {
    firstFrame_ = true;
    flow_.resetFlow();
    bgSub_ = cv::createBackgroundSubtractorMOG2(params_.bgSubHistory, params_.bgSubThreshold, false);
}
