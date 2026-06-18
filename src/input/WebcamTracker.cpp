#include "WebcamTracker.h"
#include <iostream>

namespace htk::input {

WebcamTracker::WebcamTracker()
    : m_isInitialized(false)
    , m_isTracking(false)
    , m_smoothingFactor(0.5f)
{
    m_trackingData.reset();
    m_centerPosition.reset();
}

WebcamTracker::~WebcamTracker() {
    shutdown();
}

    bool WebcamTracker::initialize(int cameraIndex) {
    // Open camera
    m_camera.open(cameraIndex);
    if (!m_camera.isOpened()) {
        std::cerr << "Failed to open camera " << cameraIndex << std::endl;
        return false;
    }

    // Set camera properties
    m_camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    m_camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    m_camera.set(cv::CAP_PROP_FPS, 30);

    // Cascade file
    std::vector<std::string> cascadePaths = {
        "resources/models/haarcascade_frontalface_default.xml",
        "../resources/models/haarcascade_frontalface_default.xml",
        "../../resources/models/haarcascade_frontalface_default.xml",
        "../../../resources/models/haarcascade_frontalface_default.xml"
    };

    bool cascadeLoaded = false;
    for (const auto& path : cascadePaths) {
        if (m_faceCascade.load(path)) {
            std::cout << "Loaded face cascade from: " << path << std::endl;
            cascadeLoaded = true;
            break;
        }
    }

    if (!cascadeLoaded) {
        std::cerr << "Failed to load face cascade from any path" << std::endl;
        std::cerr << "Tried:" << std::endl;
        for (const auto& path : cascadePaths) {
            std::cerr << "  - " << path << std::endl;
        }
        std::cerr << "Download from: https://github.com/opencv/opencv/tree/master/data/haarcascades" << std::endl;
        return false;
    }

    m_isInitialized = true;
    std::cout << "Head-Tracking Kit initialized successfully" << std::endl;
    return true;
}

bool WebcamTracker::update() {
    if (!m_isInitialized || !m_camera.read(m_currentFrame) || m_currentFrame.empty()) {
        return false;
    }

    cv::Mat gray;
    cv::cvtColor(m_currentFrame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    bool trackingSuccess = false;

    // optical flow
    if (m_isOpticalFlowActive && !m_prevPoints.empty()) {
        trackingSuccess = trackOpticalFlow(gray);
    }

    // fallback to Haar Cascade if flow failed or hasn't started
    if (!trackingSuccess) {
        cv::Rect faceRect;
        if (detectFace(m_currentFrame, faceRect)) {
            m_lastFaceRect = faceRect;
            estimatePose(faceRect); // base estimation
            initializeFeatures(gray, faceRect); // lock new points

            m_isOpticalFlowActive = true;
            trackingSuccess = true;
            m_trackingData.isValid = true;
            m_trackingData.confidence = 1.0f;
        } else {
            m_isOpticalFlowActive = false;
            m_trackingData.isValid = false;
            m_trackingData.confidence = 0.0f;
        }
    }

    m_prevGray = gray.clone();
    m_trackingData.timestamp = htk::core::TrackingData::now();
    m_isTracking = trackingSuccess;

    return true;
}

void WebcamTracker::initializeFeatures(const cv::Mat& grayFrame, const cv::Rect& faceRect) {
    // restrict feature finding to the upper half of the face
    cv::Rect roi = faceRect;
    roi.height = roi.height * 0.6;

    // ensure ROI is within bounds
    roi &= cv::Rect(0, 0, grayFrame.cols, grayFrame.rows);

    cv::Mat mask = cv::Mat::zeros(grayFrame.size(), CV_8U);
    mask(roi) = 255;

    // find the strongest corners
    cv::goodFeaturesToTrack(grayFrame, m_prevPoints, 50, 0.01, 10, mask);
}

bool WebcamTracker::trackOpticalFlow(const cv::Mat& grayFrame) {
    std::vector<cv::Point2f> nextPoints;
    std::vector<uchar> status;
    std::vector<float> err;

    // calculate optical flow
    cv::calcOpticalFlowPyrLK(
        m_prevGray, grayFrame,
        m_prevPoints, nextPoints,
        status, err,
        cv::Size(21, 21), 3 // window size
    );

    // calculate average movement
    float dx = 0.0f;
    float dy = 0.0f;
    int goodPointsCount = 0;

    std::vector<cv::Point2f> goodPoints;
    for (size_t i = 0; i < m_prevPoints.size(); i++) {
        // if point was tracked successfully and didn't jump insanely far
        if (status[i] && err[i] < 30.0f) {
            dx += (nextPoints[i].x - m_prevPoints[i].x);
            dy += (nextPoints[i].y - m_prevPoints[i].y);
            goodPoints.push_back(nextPoints[i]);
            goodPointsCount++;
        }
    }

    // force a Haar re-detection if points lost
    if (goodPointsCount < 10) {
        return false;
    }

    dx /= goodPointsCount;
    dy /= goodPointsCount;

    // apply movement
    float newYaw = m_trackingData.yaw - (dx * 0.5f);
    float newPitch = m_trackingData.pitch - (dy * 0.5f);

    // apply smoothing
    m_trackingData.yaw = m_trackingData.yaw * m_smoothingFactor + newYaw * (1.0f - m_smoothingFactor);
    m_trackingData.pitch = m_trackingData.pitch * m_smoothingFactor + newPitch * (1.0f - m_smoothingFactor);

    // update X/Y translation similarly
    m_trackingData.x += (dx * 1.5f);
    m_trackingData.y += (dy * 1.5f);

    m_prevPoints = goodPoints;
    return true;
}

bool WebcamTracker::detectFace(const cv::Mat& frame, cv::Rect& faceRect) {
    // Convert to grayscale for better detection
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    // Detect faces
    std::vector<cv::Rect> faces;
    m_faceCascade.detectMultiScale(
        gray,
        faces,
        1.1,  // Scale factor
        3,    // Min neighbors
        0,    // Flags
        cv::Size(80, 80)  // Min size
    );

    if (faces.empty()) {
        return false;
    }

    // Use the largest face in view
    faceRect = faces[0];
    for (const auto& face : faces) {
        if (face.area() > faceRect.area()) {
            faceRect = face;
        }
    }

    return true;
}

void WebcamTracker::estimatePose(const cv::Rect& faceRect) {
    // Get frame dimensions
    int frameWidth = m_currentFrame.cols;
    int frameHeight = m_currentFrame.rows;

    // Calculate center of face
    float faceCenterX = faceRect.x + faceRect.width / 2.0f;
    float faceCenterY = faceRect.y + faceRect.height / 2.0f;

    // Calculate center of frame
    float frameCenterX = frameWidth / 2.0f;
    float frameCenterY = frameHeight / 2.0f;

    // Calculate deltas from center (normalized -1 to 1)
    float deltaX = (faceCenterX - frameCenterX) / frameCenterX;
    float deltaY = (faceCenterY - frameCenterY) / frameCenterY;

    // Estimate yaw (left/right) from horizontal position
    float newYaw = -deltaX * 45.0f;

    // Estimate pitch (up/down) from vertical position
    float newPitch = -deltaY * 30.0f;

    // Estimate Z (depth) from face size
    // Larger face = closer to camera = negative Z
    float referenceFaceWidth = 150.0f;
    float faceSize = static_cast<float>(faceRect.width);
    float newZ = (referenceFaceWidth - faceSize) * 2.0f;

    // Estimate X and Y translation from face position
    float newX = deltaX * 100.0f;
    float newY = deltaY * 100.0f;

    // Apply smoothing
    if (m_trackingData.isValid && m_smoothingFactor > 0.0f) {
        m_trackingData.yaw = m_trackingData.yaw * m_smoothingFactor + newYaw * (1.0f - m_smoothingFactor);
        m_trackingData.pitch = m_trackingData.pitch * m_smoothingFactor + newPitch * (1.0f - m_smoothingFactor);
        m_trackingData.x = m_trackingData.x * m_smoothingFactor + newX * (1.0f - m_smoothingFactor);
        m_trackingData.y = m_trackingData.y * m_smoothingFactor + newY * (1.0f - m_smoothingFactor);
        m_trackingData.z = m_trackingData.z * m_smoothingFactor + newZ * (1.0f - m_smoothingFactor);
    } else {
        m_trackingData.yaw = newYaw;
        m_trackingData.pitch = newPitch;
        m_trackingData.x = newX;
        m_trackingData.y = newY;
        m_trackingData.z = newZ;
    }
    m_trackingData.roll = 0.0f;
}

    htk::core::TrackingData WebcamTracker::getTrackingData() const {
    return m_trackingData;
}

cv::Mat WebcamTracker::getCurrentFrame() const {
    return m_currentFrame.clone();
}

void WebcamTracker::setSmoothing(float factor) {
    m_smoothingFactor = std::max(0.0f, std::min(1.0f, factor));
}

void WebcamTracker::shutdown() {
    if (m_camera.isOpened()) {
        m_camera.release();
    }
    m_isInitialized = false;
    m_isTracking = false;
}

} // namespace htk::input