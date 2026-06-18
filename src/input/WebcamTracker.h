#ifndef WEBCAMTRACKER_H
#define WEBCAMTRACKER_H

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <string>

#include "../core/TrackingData.h"

namespace htk::input {

    class WebcamTracker {
    public:
        WebcamTracker();
        ~WebcamTracker();

        // Initialize camera and face detection
        bool initialize(int cameraIndex = 0);

        // Update tracking (call each frame)
        bool update();

        // Get current tracking data
        htk::core::TrackingData getTrackingData() const;

        // Get current camera frame for preview
        cv::Mat getCurrentFrame() const;

        // Cleanup
        void shutdown();

        // Settings
        void setSmoothing(float factor);
        bool isTracking() const { return m_isTracking; }

    private:
        cv::VideoCapture m_camera;
        cv::CascadeClassifier m_faceCascade;

        cv::Mat m_currentFrame;
        cv::Rect m_lastFaceRect;

        cv::Mat m_prevGray;
        std::vector<cv::Point2f> m_prevPoints;
        bool m_isOpticalFlowActive = false;

        htk::core::TrackingData m_trackingData;
        htk::core::TrackingData m_centerPosition;

        bool m_isInitialized;
        bool m_isTracking;
        float m_smoothingFactor;

        // Internal methods
        bool detectFace(const cv::Mat& frame, cv::Rect& faceRect);
        void estimatePose(const cv::Rect& faceRect);
        void smoothData(htk::core::TrackingData& data);
        void initializeFeatures(const cv::Mat& grayFrame, const cv::Rect& faceRect);
        bool trackOpticalFlow(const cv::Mat& grayFrame);
    };

} // namespace htk::input

#endif // WEBCAMTRACKER_H