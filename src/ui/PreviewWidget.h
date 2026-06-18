#ifndef PREVIEWWIDGET_H
#define PREVIEWWIDGET_H

#include <QWidget>
#include <QImage>
#include <QTimer>

#include <opencv2/opencv.hpp>

// Forward declaration
namespace htk::core {
    class HeadTracker;
}

namespace htk::ui {

    // Preview Widget
    class PreviewWidget : public QWidget {
        Q_OBJECT

    public:
        // Create preview widget
        explicit PreviewWidget(QWidget* parent = nullptr);
        ~PreviewWidget();

        // Attach head tracker instance
        void setHeadTracker(htk::core::HeadTracker* tracker);

        // Start / stop preview updates
        void startPreview();
        void stopPreview();

    protected:
        // Qt paint callback
        void paintEvent(QPaintEvent* event) override;

    private slots:
        // Called to update frame & redraw
        void updateFrame();

    private:
        // Pointer to core head tracker
        htk::core::HeadTracker* m_tracker = nullptr;

        // Timer driving preview refresh (30 FPS)
        QTimer* m_updateTimer = nullptr;

        // Current camera frame converted to QImage
        QImage m_currentImage;

        // Convert OpenCV Mat to QImage
        QImage cvMatToQImage(const cv::Mat& mat);

        // Draw text and tracking info
        void drawTrackingInfo(QPainter& painter);

        // Draw head orientation indicator
        void draw3DHeadVisualization(QPainter& painter);
    };

} // namespace htk::ui

#endif // PREVIEWWIDGET_H
