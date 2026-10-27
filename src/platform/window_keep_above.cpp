#include "window_keep_above.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>

namespace fouryshot::platform {
namespace {

bool m_installed = false;

constexpr auto kScriptName = "4yshot-keep-above";
constexpr auto kKWinService = "org.kde.KWin";
constexpr auto kScriptingPath = "/Scripting";
constexpr auto kScriptingInterface = "org.kde.kwin.Scripting";
constexpr auto kScriptInterface = "org.kde.kwin.Script";

// KWin 6 requires scripts to live inside its scripts directory; a script loaded
// straight from an arbitrary path is rejected (loadScript returns an id whose
// /Scripting/Script<id> object never appears).
QString scriptPath() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return dataDir + QLatin1String("/kwin/scripts/") + QLatin1String(kScriptName)
        + QLatin1String("/contents/code/main.js");
}

bool isKdeWayland() {
    if (QGuiApplication::platformName() != QLatin1String("wayland")) return false;
    QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered(QString::fromLatin1(kKWinService));
}

// Matching by app id keeps the rule scoped to this application: the KWin
// script must never touch unrelated windows. The hints cover the desktop file
// name as well as the executable name, in case the app id is reported
// differently (e.g. when launched without a desktop file).
QByteArray keepAboveScript() {
    return R"JS(// Installed by 4YShot: keep this application's windows above normal windows.
const TARGET_HINTS = ["4yshot", "fouryshot"];

function isTargetWindow(w) {
    if (!w) return false;
    const ids = [w.desktopFileName, w.resourceClass, w.resourceName];
    for (let i = 0; i < ids.length; ++i) {
        if (!ids[i]) continue;
        const id = String(ids[i]).toLowerCase();
        for (let j = 0; j < TARGET_HINTS.length; ++j) {
            if (id.indexOf(TARGET_HINTS[j]) >= 0) return true;
        }
    }
    return false;
}

function enforceKeepAbove(w) {
    if (isTargetWindow(w) && !w.keepAbove) w.keepAbove = true;
}

workspace.windowList().forEach(enforceKeepAbove);
if (workspace.windowAdded) workspace.windowAdded.connect(enforceKeepAbove);
)JS";
}

// Returns true when the file content changed (i.e. a reload is required).
bool writeScriptFile() {
    const QByteArray contents = keepAboveScript();
    const QString path = scriptPath();
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly)) {
        const bool same = existing.readAll() == contents;
        existing.close();
        if (same) return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[4YShot] 无法写入 KWin 置顶脚本:" << path << file.errorString();
        return false;
    }
    file.write(contents);
    file.close();
    return true;
}

bool isScriptLoaded() {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kKWinService), QString::fromLatin1(kScriptingPath),
        QString::fromLatin1(kScriptingInterface), QStringLiteral("isScriptLoaded"));
    msg << QString::fromLatin1(kScriptName);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
    return reply.type() != QDBusMessage::ErrorMessage
        && !reply.arguments().isEmpty() && reply.arguments().constFirst().toBool();
}

void unloadScript() {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kKWinService), QString::fromLatin1(kScriptingPath),
        QString::fromLatin1(kScriptingInterface), QStringLiteral("unloadScript"));
    msg << QString::fromLatin1(kScriptName);
    QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
}

} // namespace

void installKeepAboveHandler() {
    if (m_installed) return;
    if (!isKdeWayland()) {
        // X11 honours Qt::WindowStaysOnTopHint directly, and other Wayland
        // compositors expose no comparable scripting hook.
        qInfo() << "[4YShot] 非 KDE Wayland 环境，贴图置顶依赖 Qt 窗口标志";
        return;
    }

    // Several 4YShot processes can coexist (daemon + short-lived --capture/
    // --pin clients). The script is shared, so installing must be idempotent:
    // if an identical script is already running, leave it untouched. Otherwise
    // a client exiting could tear down the daemon's keep-above handling.
    const bool alreadyLoaded = isScriptLoaded();
    const bool contentChanged = writeScriptFile();
    if (alreadyLoaded && !contentChanged) {
        m_installed = true;
        return;
    }

    if (alreadyLoaded) unloadScript();

    QDBusMessage load = QDBusMessage::createMethodCall(
        QString::fromLatin1(kKWinService), QString::fromLatin1(kScriptingPath),
        QString::fromLatin1(kScriptingInterface), QStringLiteral("loadScript"));
    load << scriptPath() << QString::fromLatin1(kScriptName);
    const QDBusMessage loadReply = QDBusConnection::sessionBus().call(load, QDBus::Block, 3000);
    if (loadReply.type() == QDBusMessage::ErrorMessage || loadReply.arguments().isEmpty()) {
        qWarning() << "[4YShot] KWin 拒绝了置顶脚本:" << loadReply.errorMessage();
        return;
    }
    const int scriptId = loadReply.arguments().constFirst().toInt();
    if (scriptId < 0) {
        qWarning() << "[4YShot] KWin 未能注册置顶脚本（id < 0）";
        return;
    }

    QDBusMessage run = QDBusMessage::createMethodCall(
        QString::fromLatin1(kKWinService),
        QStringLiteral("/Scripting/Script%1").arg(scriptId),
        QString::fromLatin1(kScriptInterface), QStringLiteral("run"));
    const QDBusMessage runReply = QDBusConnection::sessionBus().call(run, QDBus::Block, 3000);
    if (runReply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "[4YShot] 置顶脚本执行失败:" << runReply.errorMessage();
        unloadScript();
        return;
    }

    m_installed = true;
    qInfo() << "[4YShot] 已启用贴图窗口置顶（KWin 脚本 id" << scriptId << "）";
}

void removeKeepAboveHandler() {
    if (!m_installed) return;
    unloadScript();
    m_installed = false;
}

} // namespace fouryshot::platform