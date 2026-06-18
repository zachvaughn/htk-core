#include "HeadTracker.h"

#include <iostream>
#include <chrono>
#include <thread>

namespace htk::core {

HeadTracker::HeadTracker()
    : m_isRunning(false)
    , m_isPaused(false)
    , m_shouldStop(false)
    , m_freeTrackEnabled(true)
    , m_trackIREnabled(true)
{
    m_webcamTracker = std::make_unique<htk::input::WebcamTracker>();

#ifdef _WIN32
    m_freeTrackOutput = std::make_unique<FreeTrackOutput>();
    m_trackIROutput  = std::make_unique<TrackIROutput>();
#endif

    m_currentData.reset();
    m_centerOffset.reset();
}

HeadTracker::~HeadTracker() {
    shutdown();
}

bool HeadTracker::initialize(int cameraIndex) {
    std::cout << "Initializing Head-Tracking Kit..." << std::endl;

    // Initialize webcam tracker
    if (!m_webcamTracker->initialize(cameraIndex)) {
        std::cerr << "Failed to initialize Head-Tracking Kit" << std::endl;
        return false;
    }

#ifdef _WIN32
    // Initialize output protocols
    if (m_freeTrackEnabled) {
        if (!m_freeTrackOutput->initialize()) {
            std::cerr << "Warning: Failed to initialize FreeTrack output" << std::endl;
            m_freeTrackEnabled = false;
        }
    }

    if (m_trackIREnabled) {
        if (!m_trackIROutput->initialize()) {
            std::cerr << "Warning: Failed to initialize TrackIR output" << std::endl;
            m_trackIREnabled = false;
        }
    }

    if (!m_freeTrackEnabled && !m_trackIREnabled) {
        std::cerr << "Error: No output protocols initialized" << std::endl;
        return false;
    }
#else
    std::cout << "Note: Output protocols are only available on Windows" << std::endl;
#endif

    std::cout << "Head-Tracking Kit initialized successfully" << std::endl;
    return true;
}

bool HeadTracker::start() {
    if (m_isRunning) {
        std::cout << "Head-Tracking Kit already running" << std::endl;
        return true;
    }

    m_shouldStop = false;
    m_isPaused   = false;
    m_isRunning  = true;

    // Start update thread
    m_updateThread = std::make_unique<std::thread>(&HeadTracker::updateLoop, this);

    std::cout << "Head-Tracking Kit started" << std::endl;
    return true;
}

void HeadTracker::stop() {
    if (!m_isRunning) {
        return;
    }

    std::cout << "Head-Tracking Kit stopping..." << std::endl;

    m_shouldStop = true;

    if (m_updateThread && m_updateThread->joinable()) {
        m_updateThread->join();
    }

    m_isRunning = false;
    std::cout << "Head-Tracking Kit stopped" << std::endl;
}

void HeadTracker::shutdown() {
    stop();

    if (m_webcamTracker) {
        m_webcamTracker->shutdown();
    }

#ifdef _WIN32
    if (m_freeTrackOutput) {
        m_freeTrackOutput->shutdown();
    }
    if (m_trackIROutput) {
        m_trackIROutput->shutdown();
    }
#endif

    std::cout << "Head-Tracking Kit shutdown complete" << std::endl;
}

    void HeadTracker::recenter() {
    std::lock_guard<std::mutex> lock(m_dataMutex);

    // capture the absolute un-offset data directly from the source
    m_centerOffset = m_webcamTracker->getTrackingData();

    std::cout << "Recentered absolute zero to: yaw=" << m_centerOffset.yaw
              << " pitch=" << m_centerOffset.pitch
              << " roll="  << m_centerOffset.roll
              << std::endl;
}

void HeadTracker::pause() {
    m_isPaused = true;
    std::cout << "Head-Tracking Kit paused" << std::endl;
}

void HeadTracker::resume() {
    m_isPaused = false;
    std::cout << "Head-Tracking Kit resumed" << std::endl;
}

bool HeadTracker::isTracking() const {
    return m_isRunning && m_webcamTracker->isTracking();
}

TrackingData HeadTracker::getCurrentData() const {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    return m_currentData;
}

void HeadTracker::setSmoothing(float factor) {
    m_webcamTracker->setSmoothing(factor);
}

void HeadTracker::enableFreeTrack(bool enable) {
    m_freeTrackEnabled = enable;
    std::cout << "FreeTrack output " << (enable ? "enabled" : "disabled") << std::endl;
}

void HeadTracker::enableTrackIR(bool enable) {
    m_trackIREnabled = enable;
    std::cout << "TrackIR output " << (enable ? "enabled" : "disabled") << std::endl;
}

void HeadTracker::updateLoop() {
    using namespace std::chrono;

    constexpr int targetFPS = 60;
    const auto targetFrameTime = milliseconds(1000 / targetFPS);

    std::cout << "Update loop started (target: " << targetFPS << " FPS)" << std::endl;

    while (!m_shouldStop) {
        const auto frameStart = steady_clock::now();

        if (!m_isPaused) {
            // Update webcam tracker
            if (m_webcamTracker->update()) {
                // Get raw tracking data
                TrackingData rawData = m_webcamTracker->getTrackingData();

                // Apply center offset
                TrackingData centeredData = applyCenterOffset(rawData);

                // Update shared data
                {
                    std::lock_guard<std::mutex> lock(m_dataMutex);
                    m_currentData = centeredData;
                }

#ifdef _WIN32
                // Send to outputs
                if (centeredData.isValid) {
                    if (m_freeTrackEnabled) {
                        m_freeTrackOutput->sendData(centeredData);
                    }
                    if (m_trackIREnabled) {
                        m_trackIROutput->sendData(centeredData);
                    }
                }
#endif
            }
        }

        // Frame rate limiting
        const auto frameEnd  = steady_clock::now();
        const auto frameTime = duration_cast<milliseconds>(frameEnd - frameStart);

        if (frameTime < targetFrameTime) {
            std::this_thread::sleep_for(targetFrameTime - frameTime);
        }
    }

    std::cout << "Update loop stopped" << std::endl;
}

TrackingData HeadTracker::applyCenterOffset(const TrackingData& data) const {
    TrackingData result = data;

    result.yaw   -= m_centerOffset.yaw;
    result.pitch -= m_centerOffset.pitch;
    result.roll  -= m_centerOffset.roll;
    result.x     -= m_centerOffset.x;
    result.y     -= m_centerOffset.y;
    result.z     -= m_centerOffset.z;

    return result;
}

} // namespace htk::core