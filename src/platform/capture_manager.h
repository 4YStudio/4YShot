#pragma once

#include <QObject>
#include <QPixmap>
#include <QImage>
#include <QRect>
#include <QList>
#include <QScreen>
#include <memory>

namespace fouryshot::platform {

struct ScreenTarget {
    QScreen* screen = nullptr;
    QString name;
    QRect logicalGeometry;
    QRect physicalGeometry;
    qreal dpr = 1.0;
};

struct RegionCaptureResult {
    QImage image;
    QString error;
};

class CaptureManager : public QObject {
    Q_OBJECT
public:
    explicit CaptureManager(QObject* parent = nullptr);

    // Warm up background capture daemon (non-blocking D-Bus activation)
    void warmUp();

    // Capture a logical desktop region through KWin without encoding a full-screen image.
    // Safe to run outside the GUI thread.
    static bool captureRegionImage(const QRect& logicalRegion, QImage& outImage, QString* error = nullptr);

    // Fast capture of the full desktop via RAM-disk uncompressed BMP pipeline
    bool captureDesktop(
        QPixmap& outPixmap,
        std::shared_ptr<QImage>& outImage,
        QRect& outCombinedGeometry,
        QList<ScreenTarget>& outScreens
    );

private:
    bool captureViaSpectacle(QPixmap& outPixmap, std::shared_ptr<QImage>& outImage);
    static bool captureViaSpectacleImage(QImage& outImage);
    static bool captureViaKWinArea(const QRect& logicalRegion, QImage& outImage, QString* error);
    static bool captureViaKWinScreens(
        QPixmap& outPixmap,
        std::shared_ptr<QImage>& outImage,
        const QRect& combinedGeometry,
        QList<ScreenTarget>& outScreens
    );
};

} // namespace fouryshot::platform
