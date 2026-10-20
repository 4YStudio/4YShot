#pragma once

#include <QObject>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QLocalServer>
#include <QTimer>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <memory>
#include <vector>
#include "../platform/capture_manager.h"
#include "../ui/overlay_widget.h"
#include "../ui/long_capture_dialog.h"
#include "../ui/selection_model.h"
#include "../annotation/annotation_engine.h"
#include "../pin/pin_window.h"

namespace fouryshot::core {

class App : public QObject {
    Q_OBJECT
public:
    static App* instance();
    explicit App(int& argc, char** argv);
    ~App();

    int run();

    pin::PinWindow* createPin(const QPixmap& pixmap, const QPoint& pos);

public slots:
    void startCapture();
    void pinFromClipboard();

private slots:
    void onNewIpcConnection();
    void onCaptureFinished();
    void onLongCaptureRequested(const QImage& firstSegment, const QRect& globalRegion, qreal dpr);
    void captureNextLongSegment();
    void onLongCaptureFrameReady();
    void onLongCaptureFinishing();
    void onLongCaptureCancelled();
    void onLongCaptureFinished();
    void onLongCapturePanelDestroyed();

private:
    void setupTray();
    void setupIpc();
    void setupShortcuts();
    void closeActiveOverlays();
    bool sendIpcCommand(const QString& cmd);

    std::unique_ptr<QApplication> m_qapp;
    platform::CaptureManager m_captureManager;
    QSystemTrayIcon* m_trayIcon = nullptr;
    QLocalServer* m_ipcServer = nullptr;

    std::vector<ui::OverlayWidget*> m_activeOverlays;
    std::vector<pin::PinWindow*> m_activePins;
    ui::LongCaptureOverlay* m_longCaptureOverlay = nullptr;
    ui::LongCapturePanel* m_longCapturePanel = nullptr;
    QTimer* m_longCapturePollTimer = nullptr;
    QFutureWatcher<platform::RegionCaptureResult>* m_longCaptureWatcher = nullptr;
    QElapsedTimer m_longCaptureCaptureTimer;
    QRect m_desktopGeometry;
    QRect m_longCaptureRegion;
    qreal m_longCaptureDpr = 1.0;
    int m_longCaptureQuietFrames = 0;
    std::shared_ptr<ui::SelectionModel> m_selectionModel;
    std::shared_ptr<annotation::AnnotationEngine> m_annotationEngine;

    bool m_isDaemon = false;
    bool m_isCapturing = false;
    bool m_longCaptureRunning = false;
    bool m_longCaptureScrolling = false;
    bool m_longCaptureCapturePending = false;
    bool m_longCaptureSeedPending = false;
    quint64 m_longCaptureSessionGeneration = 0;
    quint64 m_longCaptureCaptureGeneration = 0;
};

} // namespace fouryshot::core
