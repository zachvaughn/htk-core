#include "PreviewWidget.h"

#include <QPainter>
#include <QImage>

#include "core/HeadTracker.h"
#include "core/TrackingData.h"

#include <cmath>

using htk::core::HeadTracker;
using htk::core::TrackingData;

namespace htk::ui {

PreviewWidget::PreviewWidget(QWidget* parent)
    : QWidget(parent)
    , m_tracker(nullptr)
    , m_updateTimer(new QTimer(this))
{
    setMinimumSize(640, 480);
    setStyleSheet("background-color: black;");

    connect(m_updateTimer, &QTimer::timeout, this, &PreviewWidget::updateFrame);
}

PreviewWidget::~PreviewWidget() {
    stopPreview();
}

void PreviewWidget::setHeadTracker(HeadTracker* tracker) {
    m_tracker = tracker;
}

void PreviewWidget::startPreview() {
    if (m_tracker) {
        m_updateTimer->start(33);
    }
}

void PreviewWidget::stopPreview() {
    m_updateTimer->stop();
    m_currentImage = QImage();
    update();
}

void PreviewWidget::updateFrame() {
    if (!m_tracker || !m_tracker->isRunning()) {
        return;
    }

    // Trigger repaint
    update();
}

    void PreviewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // draw a grid background and the 3D position visualization
    if (m_tracker && m_tracker->isRunning()) {
        drawTrackingInfo(painter);

        // draw the 3D center head visualization
        draw3DHeadVisualization(painter);
    } else {
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 16));
        painter.drawText(rect(), Qt::AlignCenter, "No tracking active");
    }
}

    void PreviewWidget::draw3DHeadVisualization(QPainter& painter) {
    TrackingData data = m_tracker->getCurrentData();
    if (!data.isValid) return;

    // center of the screen
    int cx = width() / 2;
    int cy = height() / 2;

    // apply X and Y translations
    cx += static_cast<int>(data.x);
    cy += static_cast<int>(data.y);

    // dynamic scale based on Z distance
    float zScale = 1.0f - (data.z / 300.0f);
    if (zScale < 0.2f) zScale = 0.2f;

    float boxSize = 80.0f * zScale;

    // convert angles to radians
    float yawRad   = data.yaw   * static_cast<float>(M_PI) / 180.0f;
    float pitchRad = data.pitch * static_cast<float>(M_PI) / 180.0f;
    float rollRad  = data.roll  * static_cast<float>(M_PI) / 180.0f;

    painter.save();
    painter.translate(cx, cy);
    painter.rotate(data.roll);

    // compute front face offset based on yaw and pitch
    float dx = (boxSize / 2.0f) * std::sin(yawRad);
    float dy = -(boxSize / 2.0f) * std::sin(pitchRad);

    // draw reference depth
    painter.setPen(QPen(QColor(255, 255, 255, 40), 1, Qt::DashLine));
    painter.drawLine(-width(), 0, width(), 0);
    painter.drawLine(0, -height(), 0, height());

    // draw back
    painter.setPen(QPen(Qt::cyan, 2));
    painter.drawRect(QRectF(-boxSize / 2, -boxSize / 2, boxSize, boxSize));

    // draw connecting depth lines
    painter.setPen(QPen(QColor(0, 255, 255, 100), 1, Qt::DotLine));
    painter.drawLine(-boxSize/2, -boxSize/2, -boxSize/2 + dx, -boxSize/2 + dy);
    painter.drawLine(boxSize/2, -boxSize/2, boxSize/2 + dx, -boxSize/2 + dy);
    painter.drawLine(-boxSize/2, boxSize/2, -boxSize/2 + dx, boxSize/2 + dy);
    painter.drawLine(boxSize/2, boxSize/2, boxSize/2 + dx, boxSize/2 + dy);

    // draw front face
    painter.setPen(QPen(Qt::green, 3));
    painter.setBrush(QColor(0, 255, 0, 30));
    painter.drawRect(QRectF(-boxSize / 2 + dx, -boxSize / 2 + dy, boxSize, boxSize));

    // draw a nose indicator
    painter.setPen(QPen(Qt::red, 3));
    painter.drawLine(QPointF(dx, dy), QPointF(dx + dx * 0.5f, dy + dy * 0.5f));

    painter.restore();
}

void PreviewWidget::drawTrackingInfo(QPainter& painter) {
    TrackingData data = m_tracker->getCurrentData();

    // Draw tracking status
    painter.setPen(data.isValid ? Qt::green : Qt::red);
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(10, 25, data.isValid ? "TRACKING" : "NO FACE DETECTED");

    if (!data.isValid) {
        return;
    }

    painter.setPen(Qt::white);
    painter.setFont(QFont("Courier", 11));

    int y = 60;
    int lineHeight = 20;

    painter.drawText(10, y, QString("Yaw:   %1°").arg(data.yaw,   7, 'f', 2)); y += lineHeight;
    painter.drawText(10, y, QString("Pitch: %1°").arg(data.pitch, 7, 'f', 2)); y += lineHeight;
    painter.drawText(10, y, QString("Roll:  %1°").arg(data.roll,  7, 'f', 2)); y += lineHeight * 1.5;

    painter.drawText(10, y, QString("X: %1 mm").arg(data.x, 7, 'f', 1)); y += lineHeight;
    painter.drawText(10, y, QString("Y: %1 mm").arg(data.y, 7, 'f', 1)); y += lineHeight;
    painter.drawText(10, y, QString("Z: %1 mm").arg(data.z, 7, 'f', 1));

    // Confidence bar
    y += lineHeight * 1.5;
    painter.drawText(10, y, "Confidence:");

    QRect confidenceBar(10, y + 5, 200, 15);
    painter.setPen(Qt::white);
    painter.drawRect(confidenceBar);

    QRect confidenceFill = confidenceBar.adjusted(2, 2, -2, -2);
    confidenceFill.setWidth(static_cast<int>(confidenceFill.width() * data.confidence));

    QColor fillColor =
        data.confidence > 0.7f ? Qt::green :
        data.confidence > 0.4f ? Qt::yellow :
                                 Qt::red;

    painter.fillRect(confidenceFill, fillColor);

    drawHeadIndicator(painter);
}

void PreviewWidget::drawHeadIndicator(QPainter& painter) {
    int centerX = width() - 100;
    int centerY = height() - 100;
    int size = 60;

    TrackingData data = m_tracker->getCurrentData();

    float yawRad   = data.yaw   * static_cast<float>(M_PI) / 180.0f;
    float pitchRad = data.pitch * static_cast<float>(M_PI) / 180.0f;

    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(QPoint(centerX, centerY), size / 2, size / 2);

    float noseX = centerX + (size / 2.0f) * std::sin(yawRad) * std::cos(pitchRad);
    float noseY = centerY - (size / 2.0f) * std::sin(pitchRad);

    painter.setPen(QPen(Qt::red, 3));
    painter.drawLine(QPointF(centerX, centerY), QPointF(noseX, noseY));

    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10));
    painter.drawText(centerX - 30, height() - 20, "Head Pose");
}

QImage PreviewWidget::cvMatToQImage(const cv::Mat& mat) {
    if (mat.empty()) {
        return QImage();
    }

    switch (mat.type()) {
        case CV_8UC4:
            return QImage(mat.data, mat.cols, mat.rows,
                          static_cast<int>(mat.step),
                          QImage::Format_ARGB32).copy();

        case CV_8UC3: {
            cv::Mat rgb;
            cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
            return QImage(rgb.data, rgb.cols, rgb.rows,
                          static_cast<int>(rgb.step),
                          QImage::Format_RGB888).copy();
        }

        case CV_8UC1:
            return QImage(mat.data, mat.cols, mat.rows,
                          static_cast<int>(mat.step),
                          QImage::Format_Grayscale8).copy();

        default:
            return QImage();
    }
}

} // namespace htk::ui