#include "clipboard_util.h"
#include <QBuffer>
#include <QProcess>
#include <QDebug>

namespace fouryshot::core {

void persistClipboardImage(const QImage& image, int timeoutMs) {
    if (image.isNull()) return;

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    QProcess wlProc;
    wlProc.start("wl-copy", {"-t", "image/png"});
    if (!wlProc.waitForStarted(2000)) {
        // wl-clipboard not installed: fall back to whatever the Qt clipboard
        // selection alone manages to persist.
        qWarning() << "wl-copy unavailable, clipboard persistence depends on the desktop's clipboard manager";
        return;
    }
    wlProc.write(pngData);
    wlProc.closeWriteChannel();
    // wl-copy reads stdin, installs the selection, then forks a daemon and
    // exits its parent process. Waiting here guarantees the data reached the
    // daemon before this process is allowed to quit — an async write would be
    // lost on exit and wl-copy would install an EMPTY selection instead.
    // Large images (e.g. composite long screenshots) need a generous budget
    // for PNG encoding plus pipe transfer.
    if (!wlProc.waitForFinished(timeoutMs)) {
        qWarning() << "wl-copy did not finish in time";
    }
}

} // namespace fouryshot::core
