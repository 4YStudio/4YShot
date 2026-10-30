#include "smart_seed.h"

#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>

namespace fouryshot::core {

namespace {

// D-Bus 接收端：KWin 脚本通过 callDBus 回传窗口矩形。
// Qt6 已移除 QDBusAdaptor 的宽松匹配，用 QDBusVirtualObject 按方法名分发最稳。
class SeedVirtualObject : public QDBusVirtualObject {
public:
    explicit SeedVirtualObject(SmartSeedCollector* collector)
        : QDBusVirtualObject(collector), m_collector(collector) {}

    QString introspect(const QString&) const override { return {}; }

    bool handleMessage(const QDBusMessage& message, const QDBusConnection&) override {
        qWarning() << "[SmartSeed] dbus call" << message.member() << message.arguments();
        const QList<QVariant> args = message.arguments();
        if (message.member() == QLatin1String("addWindow") && args.size() >= 4) {
            static const bool dbg = qEnvironmentVariableIsSet("FOURYSHOT_OCR_DEBUG");
            if (dbg)
                qWarning() << "[SmartSeed] win" << args.at(0) << args.at(1) << args.at(2)
                           << args.at(3) << args.value(4).toString();
            m_collector->addWindow(args.at(0).toInt(), args.at(1).toInt(),
                                   args.at(2).toInt(), args.at(3).toInt());
        } else if (message.member() == QLatin1String("windowsDone")) {
            m_collector->windowsDone();
        }
        return true;
    }

private:
    SmartSeedCollector* m_collector;
};

QString seedScript(const QString& service)
{
    // Plasma 6 (KWin 6) 脚本 API：workspace.windowList() + frameGeometry（逻辑坐标）
    return QStringLiteral(R"JS(
for (const w of workspace.windowList()) {
    try {
        if (w.normalClient === false) continue;
        if (w.minimized) continue;
        const cls = String(w.resourceClass || "");
        if (/plasmashell|krunner|ksplash|ksmserver|lock|polkit|kded|xwaylandvideobridge/i.test(cls)) continue;
        const g = w.frameGeometry;
        if (!g || g.width < 40 || g.height < 40) continue;
        callDBus("%1", "/", "org.fouryshot.SmartSeed", "addWindow",
                 Math.round(g.x), Math.round(g.y),
                 Math.round(g.width), Math.round(g.height), cls);
    } catch (e) {}
}
callDBus("%1", "/", "org.fouryshot.SmartSeed", "windowsDone");
)JS").arg(service);
}

} // namespace

SmartSeedCollector::SmartSeedCollector(QObject* parent) : QObject(parent) {}

SmartSeedCollector::~SmartSeedCollector() = default;

std::vector<QRect> SmartSeedCollector::collect(int timeoutMs)
{
    qWarning() << "[SmartSeed] collect start, timeout" << timeoutMs;
    m_regions.clear();
    m_done = false;
    m_service = QStringLiteral("org.fouryshot.smartseed.p%1").arg(QCoreApplication::applicationPid());

    QDBusConnection bus = QDBusConnection::sessionBus();
    {
        QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reg = bus.interface()->registerService(
            m_service, QDBusConnectionInterface::ReplaceExistingService,
            QDBusConnectionInterface::AllowReplacement);
        if (!reg.isValid() || reg.value() != uint(QDBusConnectionInterface::ServiceRegistered)) {
            qWarning() << "[SmartSeed] registerService failed:" << reg.error().name()
                       << reg.error().message() << "value" << (reg.isValid() ? int(reg.value()) : -1);
            return {};
        }
    }
    auto* virtualObject = new SeedVirtualObject(this);
    if (!bus.registerVirtualObject(QStringLiteral("/"), virtualObject,
                                   QDBusConnection::SingleNode)) {
        qWarning() << "[SmartSeed] registerVirtualObject failed";
        bus.unregisterService(m_service);
        return {};
    }

    // 写 KWin 脚本到临时文件
    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/fouryshot-smartseed.js");
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qWarning() << "[SmartSeed] cannot write script to" << path;
            bus.unregisterService(m_service);
            return {};
        }
        f.write(seedScript(m_service).toUtf8());
    }

    QDBusInterface scripting(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
                             QStringLiteral("org.kde.kwin.Scripting"), bus);
    // 先清理同名残留脚本（上次异常退出可能遗留）
    scripting.call(QStringLiteral("unloadScript"), QStringLiteral("fouryshotsmartseed"));
    QDBusReply<int> id = scripting.call(QStringLiteral("loadScript"), path,
                                        QStringLiteral("fouryshotsmartseed"));
    qWarning() << "[SmartSeed] loadScript id" << (id.isValid() ? id.value() : -1)
               << QString(id.error().message());
    if (!id.isValid() || id.value() < 0) {
        qWarning() << "[SmartSeed] loadScript failed, giving up";
        bus.unregisterService(m_service);
        return {};
    }

    QDBusInterface script(QStringLiteral("org.kde.KWin"),
                          QStringLiteral("/Scripting/Script%1").arg(id.value()),
                          QStringLiteral("org.kde.kwin.Script"), bus);
    QDBusReply<void> runReply = script.call(QStringLiteral("run"));
    qWarning() << "[SmartSeed] run" << QString(runReply.error().message());

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(this, &SmartSeedCollector::done, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    scripting.call(QStringLiteral("unloadScript"), QStringLiteral("fouryshotsmartseed"));
    
    bus.unregisterService(m_service);
    if (!m_done) qWarning() << "[SmartSeed] TIMEOUT no windowsDone, got" << m_regions.size();
    return m_regions;
}

void SmartSeedCollector::addWindow(int x, int y, int width, int height)
{
    m_regions.emplace_back(x, y, width, height);
}

void SmartSeedCollector::windowsDone()
{
    if (!m_done) {
        m_done = true;
        emit done();
    }
}

} // namespace fouryshot::core

#include "moc_smart_seed.cpp"
#include "smart_seed.moc"
