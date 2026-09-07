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

    // Preprocesado de imagen
    bool  useCLAHE       = false;  // desactivado por defecto — CLAHE da un aspecto duro
    float claheClipLimit = 2.0f;
    int   claheTileSize  = 8;
    bool  equalizeHist   = false;

    // Parámetros de visualización del shader B&W (pasados a GraphicScore / shader)
    float bwThreshold      = 0.0f;   // desactivado — que trabaje la curva tonal
    float bwPosterize      = 256.f;  // desactivado — gradación continua
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
    const cv::Mat&        getGrayMat() const { return grayMat_; }  // fotograma en escala de grises a resolución de análisis
    ofxCv::FlowFarneback& getFlow()          { return flow_; }
    ofxCv::ContourFinder& getContour()       { return contourFinder_; }

    // Cajas delimitadoras en espacio de resolución de análisis
    const std::vector<cv::Rect>& getBoundingRects() const { return boundingRects_; }

    glm::vec2 getAnalysisScale() const {
        return glm::vec2((float)fullW_ / (float)w_, (float)fullH_ / (float)h_);
    }

    // Sobrescribe la resolución de visualización tras setup (para que getAnalysisScale()
    // refleje el tamaño real de visualización aunque el FBO de análisis se haya reducido antes).
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
