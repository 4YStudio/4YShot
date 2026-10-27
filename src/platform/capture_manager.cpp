#include "capture_manager.h"
#include <QGuiApplication>
#include <QProcess>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QElapsedTimer>
#include <QPainter>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QThread>
#include <QDBusUnixFileDescriptor>
#include <QDBusArgument>
#include <QVariantMap>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <algorithm>
#include <cstring>

namespace fouryshot::platform {

CaptureManager::CaptureManager(QObject* parent) : QObject(parent) {
}

void CaptureManager::warmUp() {
    // Non-blocking activation of headless Spectacle service in memory
    auto msg = QDBusMessage::createMethodCall("org.kde.Spectacle", "/", "org.freedesktop.DBus.Peer", "Ping");
    QDBusConnection::sessionBus().send(msg);
}

bool CaptureManager::captureDesktop(
    QPixmap& outPixmap,
    std::shared_ptr<QImage>& outImage,
    QRect& outCombinedGeometry,
    QList<ScreenTarget>& outScreens
) {
    outScreens.clear();
    auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        qWarning() << "No screens detected!";
        return false;
    }

    QRect combined;
    for (auto* screen : screens) {
        ScreenTarget target;
        target.screen = screen;
        target.name = screen->name();
        target.logicalGeometry = screen->geometry();
        target.dpr = screen->devicePixelRatio();
        combined = combined.united(target.logicalGeometry);
        outScreens.append(target);
    }
    outCombinedGeometry = combined;

    // Fast path: raw per-screen pixels over the authorized KWin ScreenShot2
    // interface. No helper process, no PNG encode/decode.
    QElapsedTimer fastTimer;
    fastTimer.start();
    if (captureViaKWinScreens(outPixmap, outImage, combined, outScreens)) {
        qInfo() << "[4YShot Performance] KWin full-desktop capture:" << fastTimer.elapsed() << "ms"
                << "Canvas:" << outPixmap.size()
                << "Scale:" << (outScreens.isEmpty() ? 0.0 : outScreens.constFirst().dpr);
        return true;
    }

    // Fallback: spawn Spectacle (slower: process startup + PNG round-trip).
    warmUp();
    bool ok = captureViaSpectacle(outPixmap, outImage);
    if (!ok || outPixmap.isNull()) {
        return false;
    }

    // Determine actual canvas scale of the captured multi-screen buffer
    qreal scaleX = combined.width() > 0 ? static_cast<qreal>(outPixmap.width()) / combined.width() : 1.0;
    qreal scaleY = combined.height() > 0 ? static_cast<qreal>(outPixmap.height()) / combined.height() : 1.0;

    for (auto& target : outScreens) {
        target.dpr = scaleX;
        target.physicalGeometry = QRect(
            std::round((target.logicalGeometry.x() - combined.x()) * scaleX),
            std::round((target.logicalGeometry.y() - combined.y()) * scaleY),
            std::round(target.logicalGeometry.width() * scaleX),
            std::round(target.logicalGeometry.height() * scaleY)
        );
    }

    return true;
}

bool CaptureManager::captureViaKWinScreens(
    QPixmap& outPixmap,
    std::shared_ptr<QImage>& outImage,
    const QRect& combinedGeometry,
    QList<ScreenTarget>& outScreens
) {
    if (outScreens.isEmpty() || combinedGeometry.isEmpty()) return false;

    QList<QImage> images;
    images.reserve(outScreens.size());
    for (auto& target : outScreens) {
        if (!target.screen || target.logicalGeometry.isEmpty()) return false;
        QImage image;
        QString error;
        if (!captureViaKWinArea(target.logicalGeometry, image, &error)) {
            qInfo() << "KWin fast capture unavailable on" << target.name << ":" << error
                    << "- falling back to Spectacle";
            return false;
        }
        const qreal scaleX = static_cast<qreal>(image.width()) / target.logicalGeometry.width();
        const qreal scaleY = static_cast<qreal>(image.height()) / target.logicalGeometry.height();
        if (scaleX <= 0.0 || std::abs(scaleX - scaleY) > 0.02) {
            qInfo() << "Anisotropic KWin area capture on" << target.name
                    << "(" << scaleX << "x" << scaleY << ") - falling back to Spectacle";
            return false;
        }
        // Mixed-DPI desktops are supported here: each screen keeps its own
        // scale and is placed on the canvas at its logical offset scaled by
        // its own DPI. This keeps every screen at native resolution (Spectacle
        // would upscale the whole desktop to a single uniform factor instead,
        // producing a blurry canvas on mixed-DPI setups).
        target.dpr = scaleX;
        images.append(image);
    }

    int canvasWidth = 0;
    int canvasHeight = 0;
    for (int i = 0; i < outScreens.size(); ++i) {
        auto& target = outScreens[i];
        const QPoint physicalOrigin(
            std::round((target.logicalGeometry.x() - combinedGeometry.x()) * target.dpr),
            std::round((target.logicalGeometry.y() - combinedGeometry.y()) * target.dpr)
        );
        target.physicalGeometry = QRect(physicalOrigin, images[i].size());
        canvasWidth = std::max(canvasWidth, physicalOrigin.x() + images[i].width());
        canvasHeight = std::max(canvasHeight, physicalOrigin.y() + images[i].height());
    }

    QImage canvas(canvasWidth, canvasHeight, QImage::Format_ARGB32_Premultiplied);
    if (canvas.isNull()) return false;
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    for (int i = 0; i < outScreens.size(); ++i) {
        painter.drawImage(outScreens[i].physicalGeometry.topLeft(), images[i]);
    }
    painter.end();

    auto image = std::make_shared<QImage>(canvas);
    outPixmap = QPixmap::fromImage(*image);
    outImage = std::move(image);
    return !outPixmap.isNull();
}

bool CaptureManager::captureViaSpectacle(QPixmap& outPixmap, std::shared_ptr<QImage>& outImage) {
    auto image = std::make_shared<QImage>();
    if (!captureViaSpectacleImage(*image)) return false;
    outPixmap = QPixmap::fromImage(*image);
    outImage = std::move(image);
    return !outPixmap.isNull();
}

bool CaptureManager::captureRegionImage(const QRect& logicalRegion, QImage& outImage, QString* error) {
    if (error) error->clear();
    return captureViaKWinArea(logicalRegion, outImage, error);
}

bool CaptureManager::captureViaKWinArea(const QRect& logicalRegion, QImage& outImage, QString* error) {
    const auto reportFailure = [error](const QString& reason) {
        if (error) *error = reason;
        qWarning() << "KWin area capture failed:" << reason;
    };
    if (logicalRegion.isEmpty() || !QDBusUnixFileDescriptor::isSupported()) {
        reportFailure(QStringLiteral("选区无效，或 D-Bus 不支持文件描述符传递"));
        return false;
    }

    int pipeFds[2] = {-1, -1};
    if (::pipe(pipeFds) != 0) {
        reportFailure(QStringLiteral("无法创建 KWin 图像管道：%1").arg(QString::fromLocal8Bit(strerror(errno))));
        return false;
    }

    QDBusUnixFileDescriptor writer;
    writer.giveFileDescriptor(pipeFds[1]);
    pipeFds[1] = -1;
    const auto closeReader = [&pipeFds]() {
        if (pipeFds[0] >= 0) {
            ::close(pipeFds[0]);
            pipeFds[0] = -1;
        }
    };

    QVariantMap options;
    options.insert(QStringLiteral("include-cursor"), false);
    options.insert(QStringLiteral("native-resolution"), true);
    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin.ScreenShot2"),
        QStringLiteral("/org/kde/KWin/ScreenShot2"),
        QStringLiteral("org.kde.KWin.ScreenShot2"),
        QStringLiteral("CaptureArea"));
    request.setArguments({logicalRegion.x(), logicalRegion.y(),
                          static_cast<quint32>(logicalRegion.width()),
                          static_cast<quint32>(logicalRegion.height()),
                          options, QVariant::fromValue(writer)});

    QElapsedTimer timer;
    timer.start();
    const QDBusMessage reply = QDBusConnection::sessionBus().call(request, QDBus::Block, 5000);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        if (reply.errorName() == QLatin1String("org.kde.KWin.ScreenShot2.Error.NoAuthorized")) {
            reportFailure(QStringLiteral(
                "KWin 未授权本进程截图：长截图依赖 KWin 受限接口，"
                "请安装 DEB 包或重新执行构建以注册授权，然后重启程序；"
                "可用 --probe-capture 自检"));
        } else {
            reportFailure(QStringLiteral("KWin D-Bus 请求被拒绝 [%1]：%2")
                              .arg(reply.errorName(), reply.errorMessage()));
        }
        closeReader();
        return false;
    }

    const QVariant metadataValue = reply.arguments().constFirst();
    QVariantMap metadata = metadataValue.toMap();
    QString metadataSignature;
    if (metadata.isEmpty() && metadataValue.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const QDBusArgument mapArgument = metadataValue.value<QDBusArgument>();
        metadataSignature = mapArgument.currentSignature();
        metadata = qdbus_cast<QVariantMap>(mapArgument);
    }
    const QString type = metadata.value(QStringLiteral("type")).toString();
    const quint32 width = metadata.value(QStringLiteral("width")).toUInt();
    const quint32 height = metadata.value(QStringLiteral("height")).toUInt();
    const quint32 stride = metadata.value(QStringLiteral("stride")).toUInt();
    const quint32 formatValue = metadata.value(QStringLiteral("format")).toUInt();
    const quint64 dataSize = static_cast<quint64>(stride) * height;
    if (type != QStringLiteral("raw") || width == 0 || height == 0 || stride == 0 ||
        dataSize < stride || dataSize > 512ULL * 1024ULL * 1024ULL) {
        const QString details = metadata.isEmpty()
            ? QStringLiteral("空字典，Qt 类型 %1，D-Bus 签名 %2")
                  .arg(QString::fromLatin1(metadataValue.metaType().name()), metadataSignature)
            : QStringLiteral("%1（D-Bus 签名 %2）")
                  .arg(QString::fromUtf8(QJsonDocument::fromVariant(metadata).toJson(QJsonDocument::Compact)),
                       metadataSignature);
        reportFailure(QStringLiteral("KWin 返回了无效图像信息：%1").arg(details));
        closeReader();
        return false;
    }

    if (writer.isValid()) ::close(writer.takeFileDescriptor());
    QByteArray raw;
    raw.reserve(static_cast<qsizetype>(dataSize));
    QElapsedTimer readTimer;
    readTimer.start();
    while (static_cast<quint64>(raw.size()) < dataSize) {
        pollfd pollFd{pipeFds[0], POLLIN | POLLHUP, 0};
        const int remainingMs = std::max(1, 5000 - static_cast<int>(readTimer.elapsed()));
        const int ready = ::poll(&pollFd, 1, remainingMs);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0 || (pollFd.revents & (POLLERR | POLLNVAL))) {
            reportFailure(QStringLiteral("读取 KWin 图像超时或失败：已收 %1 / %2 字节")
                              .arg(raw.size()).arg(dataSize));
            closeReader();
            return false;
        }
        const qsizetype oldSize = raw.size();
        const qsizetype chunkSize = static_cast<qsizetype>(std::min<quint64>(
            dataSize - static_cast<quint64>(oldSize), 1024ULL * 1024ULL));
        raw.resize(oldSize + chunkSize);
        const ssize_t count = ::read(pipeFds[0], raw.data() + oldSize, static_cast<size_t>(chunkSize));
        if (count <= 0) {
            raw.resize(oldSize);
            reportFailure(QStringLiteral("KWin 图像管道提前关闭：只收到 %1 / %2 字节")
                              .arg(raw.size()).arg(dataSize));
            closeReader();
            return false;
        }
        raw.resize(oldSize + static_cast<qsizetype>(count));
    }
    closeReader();

    const auto format = static_cast<QImage::Format>(formatValue);
    const QImage rawImage(reinterpret_cast<const uchar*>(raw.constData()),
                          static_cast<int>(width), static_cast<int>(height),
                          static_cast<int>(stride), format);
    if (rawImage.isNull()) {
        reportFailure(QStringLiteral("KWin 返回不支持的图像格式 %1（%2×%3，stride %4）")
                          .arg(formatValue).arg(width).arg(height).arg(stride));
        return false;
    }
    outImage = rawImage.copy();
    if (outImage.isNull()) {
        reportFailure(QStringLiteral("复制 KWin 图像缓冲区失败"));
        return false;
    }
    qInfo() << "[4YShot Performance] KWin area capture:" << timer.elapsed() << "ms"
            << "Region:" << logicalRegion << "Image:" << outImage.size()
            << "Scale:" << metadata.value(QStringLiteral("scale")).toDouble();
    return true;
}

bool CaptureManager::captureViaSpectacleImage(QImage& outImage) {
    QElapsedTimer timer;
    timer.start();

    // Use lossless 32-bit RGBA PNG in RAM-disk (/dev/shm) for optimum fidelity and speed
    QString tmpPath = QString("/dev/shm/4yshot_cap_%1.png").arg(QDateTime::currentMSecsSinceEpoch());

    QProcess process;
    // -b: background mode, -n: non-interactive (no notification), -o: output file
    process.start("spectacle", {"-b", "-n", "-o", tmpPath});
    if (!process.waitForStarted(1000)) {
        qWarning() << "Failed to start spectacle";
        return false;
    }

    if (!process.waitForFinished(8000) || process.exitCode() != 0) {
        qWarning() << "Spectacle failed or timed out. Exit code:" << process.exitCode()
                   << "Error:" << process.errorString()
                   << "Stderr:" << process.readAllStandardError();
        QFile::remove(tmpPath);
        return false;
    }
    qint64 snapMs = timer.elapsed();

    // Ensure the PNG file is completely flushed and the IEND trailer is written
    QElapsedTimer waitTimer;
    waitTimer.start();
    while (waitTimer.elapsed() < 3000) {
        QFile checkFile(tmpPath);
        if (checkFile.open(QIODevice::ReadOnly)) {
            if (checkFile.size() >= 12) {
                checkFile.seek(checkFile.size() - 12);
                QByteArray tail = checkFile.read(12);
                if (tail.contains("IEND")) {
                    checkFile.close();
                    break;
                }
            }
            checkFile.close();
        }
        QThread::msleep(10);
    }

    QImageReader reader(tmpPath);
    reader.setAutoTransform(true);
    if (!reader.read(&outImage)) {
        qWarning() << "Failed to load captured image from" << tmpPath << "Reason:" << reader.errorString() << "Format:" << reader.format();
        QFile::remove(tmpPath);
        return false;
    }
    QFile::remove(tmpPath);

    qInfo() << "[4YShot Performance] Spectacle capture:" << snapMs << "ms, Total capture & buffer processing:" << timer.elapsed() << "ms"
            << "Resolution:" << outImage.size();
    return !outImage.isNull();
}

} // namespace fouryshot::platform
