#include "app.h"
#include <QCommandLineParser>
#include <QLocalSocket>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QDebug>
#include <QFile>
#include <QCursor>
#include <QElapsedTimer>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QTextStream>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include "config.h"
#include "../pin/pin_window.h"
#include "../platform/window_keep_above.h"

#ifdef HAS_KF6_GLOBALACCEL
#include <KGlobalAccel>
#endif

namespace fouryshot::core {

static const char* IPC_SERVER_NAME = "fouryshot_daemon_ipc";
static App* s_instance = nullptr;

App* App::instance() {
    return s_instance;
}

App::App(int& argc, char** argv) {
    s_instance = this;
    m_qapp = std::make_unique<QApplication>(argc, argv);
    m_qapp->setApplicationName("4YShot");
    m_qapp->setDesktopFileName(QStringLiteral("org.fouryshot.FourYShot"));
    m_qapp->setApplicationDisplayName("4YShot 截图工具");
    m_qapp->setApplicationVersion("1.0.0");
    m_qapp->setQuitOnLastWindowClosed(false);

    // Configure pure dark mode palette for all dialogs and widgets
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor("#1a1a1e"));
    darkPalette.setColor(QPalette::WindowText, QColor("#ffffff"));
    darkPalette.setColor(QPalette::Base, QColor("#24242a"));
    darkPalette.setColor(QPalette::AlternateBase, QColor("#1e1e24"));
    darkPalette.setColor(QPalette::ToolTipBase, QColor("#121214"));
    darkPalette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
    darkPalette.setColor(QPalette::Text, QColor("#ffffff"));
    darkPalette.setColor(QPalette::Button, QColor("#2b2b32"));
    darkPalette.setColor(QPalette::ButtonText, QColor("#ffffff"));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor("#1677ff"));
    darkPalette.setColor(QPalette::Highlight, QColor("#1677ff"));
    darkPalette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    darkPalette.setColor(QPalette::PlaceholderText, QColor("#8c8c8c"));
    m_qapp->setPalette(darkPalette);

    // Load stylesheet
    QFile qss(":/themes/modern.qss");
    if (qss.open(QIODevice::ReadOnly)) {
        m_qapp->setStyleSheet(qss.readAll());
    }

    // Set application icon
    m_qapp->setWindowIcon(QIcon(":/icons/tool-recognize-text.svg"));
}

App::~App() {
    // The KWin keep-above script is intentionally left loaded: it only targets
    // this application's appId, and unloading it on exit would strip pin
    // windows of their stacking guarantee while a daemon still owns them.
    if (m_ipcServer) {
        m_ipcServer->close();
    }
    QLocalServer::removeServer(IPC_SERVER_NAME);
}

int App::run() {
    QCommandLineParser parser;
    parser.setApplicationDescription("4YShot - 面向 Debian 13 + KDE 6 + Wayland 的高颜值全能截图工具");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption daemonOpt(QStringList() << "d" << "daemon", "作为常驻守护进程在后台启动，并在系统托盘显示。");
    QCommandLineOption captureOpt(QStringList() << "c" << "capture", "立即触发屏幕截图。");
    QCommandLineOption pinOpt(QStringList() << "p" << "pin", "从剪贴板粘贴图像并贴图置顶到屏幕。");
    QCommandLineOption probeCaptureOpt(QStringList() << "probe-capture");
    probeCaptureOpt.setDescription("诊断：验证 KWin 区域截图授权与链路后退出。");
    probeCaptureOpt.setFlags(QCommandLineOption::HiddenFromHelp);

    parser.addOption(daemonOpt);
    parser.addOption(captureOpt);
    parser.addOption(pinOpt);
    parser.addOption(probeCaptureOpt);

    parser.process(*m_qapp);

    if (parser.isSet(probeCaptureOpt)) {
        const QPoint origin = QGuiApplication::primaryScreen()->geometry().topLeft();
        QImage probeImage;
        QString probeError;
        if (platform::CaptureManager::captureRegionImage(QRect(origin, QSize(64, 64)), probeImage, &probeError)) {
            qInfo() << "KWin 区域截图自检成功:" << probeImage.size();
            QTextStream(stdout) << "OK " << probeImage.width() << "x" << probeImage.height() << Qt::endl;
        } else {
            qWarning() << "KWin 区域截图自检失败:" << probeError;
            QTextStream(stdout) << "FAIL " << probeError << Qt::endl;
            return 1;
        }

        QElapsedTimer fullTimer;
        fullTimer.start();
        QPixmap fullDesktop;
        std::shared_ptr<QImage> fullDesktopImage;
        QRect combinedGeometry;
        QList<platform::ScreenTarget> screens;
        const bool fullOk = m_captureManager.captureDesktop(
            fullDesktop, fullDesktopImage, combinedGeometry, screens);
        QTextStream(stdout) << "FULL-CAPTURE "
                            << (fullOk ? "OK" : "FAIL")
                            << " " << fullTimer.elapsed() << "ms"
                            << " " << fullDesktop.width() << "x" << fullDesktop.height()
                            << Qt::endl;
        return fullOk ? 0 : 1;
    }

    bool isDaemon = parser.isSet(daemonOpt);
    bool isCapture = parser.isSet(captureOpt);
    bool isPin = parser.isSet(pinOpt);

    // Wayland gives clients no control over stacking order, so pin windows need
    // a KWin-side "keep above" flag or other applications simply cover them.
    platform::installKeepAboveHandler();

    // If neither is explicitly passed, and not daemon, default to capture
    if (!isDaemon && !isPin) {
        isCapture = true;
    }

    if (isPin) {
        // Try to ask existing daemon or instance
        if (sendIpcCommand("pin")) {
            return 0;
        }
        setupIpc();
        pinFromClipboard();
        return m_qapp->exec();
    }

    if (isCapture && !isDaemon) {
        // Check if existing daemon or instance is running
        if (sendIpcCommand("capture")) {
            return 0;
        }
        // No instance running, setup IPC so subsequent captures/pins attach here
        setupIpc();
        startCapture();
        return m_qapp->exec();
    }

    if (isDaemon) {
        m_isDaemon = true;
        m_qapp->setQuitOnLastWindowClosed(false);
        setupIpc();
        setupTray();
        setupShortcuts();
        qDebug() << "4YShot daemon started successfully.";
        return m_qapp->exec();
    }

    return 0;
}

bool App::sendIpcCommand(const QString& cmd) {
    QLocalSocket socket;
    socket.connectToServer(IPC_SERVER_NAME);
    if (socket.waitForConnected(50)) {
        socket.write(cmd.toUtf8());
        socket.waitForBytesWritten(200);
        socket.disconnectFromServer();
        return true;
    }
    return false;
}

void App::setupIpc() {
    QLocalServer::removeServer(IPC_SERVER_NAME);
    m_ipcServer = new QLocalServer(this);
    connect(m_ipcServer, &QLocalServer::newConnection, this, &App::onNewIpcConnection);
    if (!m_ipcServer->listen(IPC_SERVER_NAME)) {
        qWarning() << "Failed to listen on IPC server:" << m_ipcServer->errorString();
    }
}

void App::onNewIpcConnection() {
    while (m_ipcServer->hasPendingConnections()) {
        QLocalSocket* client = m_ipcServer->nextPendingConnection();
        connect(client, &QLocalSocket::readyRead, [this, client]() {
            QByteArray data = client->readAll();
            QString cmd = QString::fromUtf8(data).trimmed();
            if (cmd == "capture") {
                startCapture();
            } else if (cmd == "pin") {
                pinFromClipboard();
            }
        });
    }
}

void App::setupTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray is not available";
        return;
    }

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/tool-recognize-text.svg"));
    m_trayIcon->setToolTip("4YShot 截图工具");

    auto* menu = new QMenu();
    auto* captureAction = menu->addAction("屏幕截图 (Print)");
    auto* pinAction = menu->addAction("剪贴板贴图 (F3)");
    menu->addSeparator();
    auto* quitAction = menu->addAction("退出 4YShot");

    connect(captureAction, &QAction::triggered, this, &App::startCapture);
    connect(pinAction, &QAction::triggered, this, &App::pinFromClipboard);
    connect(quitAction, &QAction::triggered, m_qapp.get(), &QApplication::quit);

    m_trayIcon->setContextMenu(menu);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            startCapture();
        }
    });
}

void App::setupShortcuts() {
#ifdef HAS_KF6_GLOBALACCEL
    auto* captureAction = new QAction(this);
    captureAction->setObjectName("4yshot_global_capture");
    captureAction->setText("4YShot 屏幕截图");
    connect(captureAction, &QAction::triggered, this, &App::startCapture);
    KGlobalAccel::setGlobalShortcut(captureAction, QKeySequence("Print"));

    auto* pinAction = new QAction(this);
    pinAction->setObjectName("4yshot_global_pin");
    pinAction->setText("4YShot 贴图置顶");
    connect(pinAction, &QAction::triggered, this, &App::pinFromClipboard);
    KGlobalAccel::setGlobalShortcut(pinAction, QKeySequence("F3"));
#endif
}

void App::startCapture() {
    if (m_isCapturing || m_longCaptureRunning || m_longCapturePanel) return;
    m_isCapturing = true;

    QPixmap fullDesktop;
    std::shared_ptr<QImage> fullDesktopImage;
    QRect combinedGeometry;
    QList<platform::ScreenTarget> screens;

    bool ok = m_captureManager.captureDesktop(fullDesktop, fullDesktopImage, combinedGeometry, screens);
    if (!ok || fullDesktop.isNull()) {
        qWarning() << "Desktop capture failed!";
        m_isCapturing = false;
        if (!m_isDaemon && m_activePins.empty()) {
            m_qapp->quit();
        }
        return;
    }
    m_desktopGeometry = combinedGeometry;

    m_selectionModel = std::make_shared<ui::SelectionModel>();
    m_annotationEngine = std::make_shared<annotation::AnnotationEngine>();

    m_activeOverlays.clear();
    for (const auto& target : screens) {
        auto* overlay = new ui::OverlayWidget(
            target, combinedGeometry, fullDesktop, fullDesktopImage,
            m_selectionModel, m_annotationEngine
        );
        connect(overlay, &ui::OverlayWidget::captureCompleted, this, &App::onCaptureFinished);
        connect(overlay, &ui::OverlayWidget::captureCancelled, this, &App::onCaptureFinished);
        connect(overlay, &ui::OverlayWidget::longCaptureRequested,
                this, &App::onLongCaptureRequested);
        m_activeOverlays.push_back(overlay);
        overlay->showOverlay();
    }
}

pin::PinWindow* App::createPin(const QPixmap& pixmap, const QPoint& pos) {
    auto* pinWin = new pin::PinWindow(pixmap, pos);
    m_activePins.push_back(pinWin);
    connect(pinWin, &QObject::destroyed, this, [this, pinWin]() {
        auto it = std::find(m_activePins.begin(), m_activePins.end(), pinWin);
        if (it != m_activePins.end()) {
            m_activePins.erase(it);
        }
        if (!m_isDaemon && !m_isCapturing && m_activePins.empty()) {
            m_qapp->quit();
        }
    });
    return pinWin;
}

void App::onCaptureFinished() {
    closeActiveOverlays();
    m_isCapturing = false;

    if (!m_isDaemon && m_activePins.empty() && !m_longCaptureRunning && !m_longCapturePanel) {
        m_qapp->quit();
    }
}

void App::closeActiveOverlays() {
    for (auto* overlay : m_activeOverlays) {
        overlay->disconnect();
        overlay->close();
        overlay->deleteLater();
    }
    m_activeOverlays.clear();
}

void App::onLongCaptureRequested(const QImage& firstSegment, const QRect& globalRegion, qreal dpr) {
    if (firstSegment.isNull() || m_longCapturePanel) return;

    m_longCaptureRegion = globalRegion;
    ++m_longCaptureSessionGeneration;
    m_longCaptureDpr = dpr > 0.0 ? dpr : 1.0;
    m_longCaptureRunning = true;
    m_longCaptureQuietFrames = 0;
    m_longCaptureScrolling = false;
    m_longCaptureSeedPending = true;
    QScreen* screen = nullptr;
    qint64 bestIntersectionArea = -1;
    for (QScreen* candidate : QGuiApplication::screens()) {
        QRect intersection = candidate->geometry().intersected(globalRegion);
        qint64 area = static_cast<qint64>(intersection.width()) * intersection.height();
        if (area > bestIntersectionArea) {
            bestIntersectionArea = area;
            screen = candidate;
        }
    }
    if (!screen) screen = QGuiApplication::primaryScreen();

    // Remove every screenshot overlay before entering the scroll-capture flow.
    // Those overlays contain a frozen desktop image and must not remain under
    // the live desktop region while scrolling.
    closeActiveOverlays();

    m_longCaptureOverlay = new ui::LongCaptureOverlay(screen, globalRegion);
    m_longCaptureOverlay->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_longCaptureOverlay, &QObject::destroyed, this, [this]() {
        m_longCaptureOverlay = nullptr;
    });
    m_longCapturePanel = new ui::LongCapturePanel(firstSegment, m_longCaptureOverlay);
    connect(m_longCapturePanel, &ui::LongCapturePanel::sessionFinishing,
            this, &App::onLongCaptureFinishing);
    connect(m_longCapturePanel, &ui::LongCapturePanel::sessionCancelled,
            this, &App::onLongCaptureCancelled);
    connect(m_longCapturePanel, &ui::LongCapturePanel::sessionFinished,
            this, &App::onLongCaptureFinished);
    connect(m_longCapturePanel, &QObject::destroyed,
            this, &App::onLongCapturePanelDestroyed);

    QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);
    available.translate(-m_longCaptureOverlay->geometry().topLeft());
    const QRect localRegion = globalRegion.translated(-m_longCaptureOverlay->geometry().topLeft());
    const int panelWidth = m_longCapturePanel->width();
    const int rightSpace = available.right() - localRegion.right();
    const int leftSpace = localRegion.left() - available.left();
    bool placeRight = rightSpace >= panelWidth + 12 || rightSpace >= leftSpace;
    int x = placeRight ? localRegion.right() + 12
                       : localRegion.left() - panelWidth - 12;
    int y = localRegion.top();
    if (rightSpace < panelWidth + 12 && leftSpace < panelWidth + 12) {
        const int belowSpace = available.bottom() - localRegion.bottom();
        const int aboveSpace = localRegion.top() - available.top();
        if (belowSpace >= m_longCapturePanel->height() + 12 || belowSpace >= aboveSpace) {
            x = localRegion.left();
            y = localRegion.bottom() + 12;
        } else {
            x = localRegion.left();
            y = localRegion.top() - m_longCapturePanel->height() - 12;
        }
    }
    x = std::clamp(x, available.left(), std::max(available.left(), available.right() - panelWidth + 1));
    y = std::clamp(y, available.top(), std::max(available.top(), available.bottom() - m_longCapturePanel->height() + 1));
    m_longCapturePanel->move(x, y);
    m_longCaptureOverlay->showFullScreen();
    m_longCaptureOverlay->raise();
    m_longCapturePanel->show();
    m_longCaptureOverlay->setInputRegion(globalRegion, m_longCapturePanel->geometry());

    if (!m_longCapturePollTimer) {
        m_longCapturePollTimer = new QTimer(this);
        m_longCapturePollTimer->setSingleShot(true);
        connect(m_longCapturePollTimer, &QTimer::timeout,
                this, &App::captureNextLongSegment);
    }
    if (!m_longCaptureWatcher) {
        m_longCaptureWatcher = new QFutureWatcher<platform::RegionCaptureResult>(this);
        connect(m_longCaptureWatcher, &QFutureWatcher<QImage>::finished,
                this, &App::onLongCaptureFrameReady);
    }
    m_longCapturePollTimer->setInterval(120);
    m_longCapturePollTimer->start();
}

void App::captureNextLongSegment() {
    if (!m_longCaptureRunning || !m_longCapturePanel) return;
    if (m_longCaptureCapturePending) return;
    if (m_longCapturePollTimer) m_longCapturePollTimer->stop();

    m_longCaptureCapturePending = true;
    m_longCaptureCaptureGeneration = m_longCaptureSessionGeneration;
    m_longCaptureCaptureTimer.start();
    const QRect logicalRegion = m_longCaptureRegion;
    m_longCaptureWatcher->setFuture(QtConcurrent::run([logicalRegion]() {
        platform::RegionCaptureResult result;
        platform::CaptureManager::captureRegionImage(logicalRegion, result.image, &result.error);
        return result;
    }));
}

void App::onLongCaptureFrameReady() {
    m_longCaptureCapturePending = false;
    const platform::RegionCaptureResult captureResult = m_longCaptureWatcher->result();
    const QImage desktopImage = captureResult.image;
    const qint64 captureTimeMs = m_longCaptureCaptureTimer.elapsed();
    if (m_longCaptureCaptureGeneration != m_longCaptureSessionGeneration ||
        !m_longCaptureRunning || !m_longCapturePanel) {
        if (m_longCaptureRunning && m_longCapturePanel && m_longCapturePollTimer) {
            m_longCapturePollTimer->setInterval(0);
            m_longCapturePollTimer->start();
        }
        return;
    }

    if (desktopImage.isNull()) {
        m_longCapturePanel->showMessage(QString("截图失败 · 耗时 %1 ms · %2 · 准备重试")
                                            .arg(captureTimeMs)
                                            .arg(captureResult.error.isEmpty()
                                                ? QStringLiteral("未返回错误详情")
                                                : captureResult.error));
        if (m_longCaptureScrolling) {
            captureNextLongSegment();
        } else {
            m_longCapturePollTimer->setInterval(150);
            m_longCapturePollTimer->start();
        }
        return;
    }

    const QImage segment = desktopImage;
    if (segment.size().isEmpty()) {
        m_longCapturePanel->showMessage("KWin 返回的选区图像为空");
        m_longCapturePollTimer->setInterval(300);
        m_longCapturePollTimer->start();
        return;
    }
    m_longCaptureDpr = static_cast<qreal>(segment.width()) / m_longCaptureRegion.width();

    // The selection overlay can still be present in the compositor's current
    // frame while the long-capture UI is being initialized. Replace the
    // temporary seed with this first asynchronous frame after the overlay has
    // had time to disappear, and do not treat that replacement as scroll.
    if (m_longCaptureSeedPending) {
        m_longCapturePanel->replaceSeedFrame(segment);
        m_longCaptureSeedPending = false;
        m_longCapturePanel->showMessage(QString("首帧已清理 · 选区 %1x%2 · 截图 %3 ms · 等待滚动")
                                            .arg(segment.width()).arg(segment.height()).arg(captureTimeMs));
        m_longCapturePollTimer->setInterval(120);
        m_longCapturePollTimer->start();
        return;
    }

    const bool motionDetected = m_longCapturePanel->recordFrame(segment, captureTimeMs);
    if (motionDetected) {
        m_longCaptureQuietFrames = 0;
        if (!m_longCaptureScrolling) m_longCapturePanel->setScrolling(true);
        m_longCaptureScrolling = true;
    } else if (m_longCaptureScrolling) {
        ++m_longCaptureQuietFrames;
    }

    if (m_longCaptureScrolling && m_longCaptureQuietFrames >= 3) {
        m_longCaptureScrolling = false;
        m_longCapturePanel->setScrolling(false);
        if (m_longCapturePanel->hasPendingFrames()) m_longCapturePanel->mergePendingFrames();
        m_longCapturePollTimer->setInterval(400);
        m_longCapturePollTimer->start();
    } else if (m_longCaptureScrolling) {
        // Chain captures as soon as the previous one completes. This avoids
        // timer drift and keeps collecting while the user is actively scrolling.
        captureNextLongSegment();
    } else {
        m_longCapturePollTimer->setInterval(120);
        m_longCapturePollTimer->start();
    }
}

void App::onLongCaptureCancelled() {
    m_longCaptureRunning = false;
    ++m_longCaptureSessionGeneration;
    m_longCaptureScrolling = false;
    m_longCaptureRegion = QRect();
    if (m_longCapturePollTimer) m_longCapturePollTimer->stop();
    if (m_longCaptureOverlay) m_longCaptureOverlay->close();
    m_isCapturing = false;
}

void App::onLongCaptureFinishing() {
    // Stop the polling loop before opening a modal save dialog or accessing
    // the clipboard. Any capture already running is invalidated by advancing
    // the generation, so its result cannot be appended during the dialog's
    // nested event loop.
    m_longCaptureRunning = false;
    ++m_longCaptureSessionGeneration;
    m_longCaptureScrolling = false;
    m_longCaptureSeedPending = false;
    if (m_longCapturePollTimer) m_longCapturePollTimer->stop();
    m_isCapturing = false;
}

void App::onLongCaptureFinished() {
    m_longCaptureRunning = false;
    ++m_longCaptureSessionGeneration;
    m_longCaptureScrolling = false;
    m_longCaptureRegion = QRect();
    if (m_longCapturePollTimer) m_longCapturePollTimer->stop();
    if (m_longCaptureOverlay) m_longCaptureOverlay->close();
    m_isCapturing = false;
}

void App::onLongCapturePanelDestroyed() {
    m_longCapturePanel = nullptr;
    if (!m_isDaemon && !m_isCapturing && m_activePins.empty()) {
        m_qapp->quit();
    }
}

void App::pinFromClipboard() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    QPixmap pix = clipboard->pixmap();
    if (pix.isNull()) {
        QImage img = clipboard->image();
        if (!img.isNull()) {
            pix = QPixmap::fromImage(img);
        }
    }
    if (!pix.isNull()) {
        QPoint center = QCursor::pos() - QPoint(pix.width() / 2, pix.height() / 2);
        createPin(pix, center);
    } else {
        qWarning() << "Clipboard does not contain an image to pin!";
    }
}

} // namespace fouryshot::core
