#pragma once
#include "ofMain.h"
#include "ofxCv.h"
#include "OSCSender.h"
#include <opencv2/video/background_segm.hpp>
#include <opencv2/imgproc.hpp>

struct CVParams {
    bool  enabled        = true;
    bool  halfRes        = true;
    int   analysisEveryNFrames = 1;
    int   flowWindowSize = 8;
    float blobMinArea    = 500.f;
    float blobMaxArea    = 200000.f;
    int   bgSubHistory   = 120;
    float bgSubThreshold = 25.f;
    int   cannyLow       = 30;
    int   cannyHigh      = 90;
    float blobThreshold  = 80.f;

    // Image preprocessing
    bool  useCLAHE       = false;  // off by default — CLAHE creates harsh look
    float claheClipLimit = 2.0f;
    int   claheTileSize  = 8;
    bool  equalizeHist   = false;

    // B&W shader display params (passed to GraphicScore / shader)
    float bwThreshold      = 0.0f;   // off — let tonal curve do the work
    float bwPosterize      = 256.f;  // off — continuous gradation
    float bwBrightness     = 0.0f;
    float bwContrast       = 1.3f;
    float bwGamma          = 0.95f;
    float bwGrain          = 0.035f;
    float bwVignette       = 0.32f;
    float bwSCurve         = 0.55f;
};

class CVPipeline {
public:
    void setup(int fullW, int fullH, const CVParams& params);
    void update(ofPixels& grayPixels);
    void reset();

    const CVData&         getData()    const { return data_; }
    const cv::Mat&        getFgMask()  const { return fgMask_; }
    const cv::Mat&        getEdges()   const { return edges_; }
    const cv::Mat&        getGrayMat() const { return grayMat_; }  // analysis-res grayscale frame
    ofxCv::FlowFarneback& getFlow()          { return flow_; }
    ofxCv::ContourFinder& getContour()       { return contourFinder_; }

    // Bounding boxes in analysis-resolution space
    const std::vector<cv::Rect>& getBoundingRects() const { return boundingRects_; }

    glm::vec2 getAnalysisScale() const {
        return glm::vec2((float)fullW_ / (float)w_, (float)fullH_ / (float)h_);
    }

    // Override display resolution after setup (so getAnalysisScale() reflects the
    // real display size even when the analysis FBO is pre-downscaled before input).
    void setDisplaySize(int w, int h) { fullW_ = w; fullH_ = h; }

    CVParams& params() { return params_; }

private:
    CVParams  params_;
    int       fullW_ = 0, fullH_ = 0;
    int       w_ = 0, h_ = 0;

    cv::Mat   grayMat_, prevGray_;
    cv::Mat   fgMask_;
    cv::Mat   edges_;

    std::vector<cv::Rect>  boundingRects_;

    ofxCv::FlowFarneback                  flow_;
    ofxCv::ContourFinder                  contourFinder_;
    cv::Ptr<cv::BackgroundSubtractorMOG2> bgSub_;
    cv::Ptr<cv::CLAHE>                    clahe_;

    CVData    data_;
    bool      firstFrame_ = true;
};
