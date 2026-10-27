#pragma once

#include <QImage>

namespace fouryshot::core {

// Copies PNG image data to wl-copy synchronously and waits until wl-copy has
// installed the selection. wl-copy forks a background daemon that owns the
// selection afterwards, so the clipboard keeps serving the image even after
// this process exits. Without this handoff, KDE's clipboard applet takes over
// the Qt selection but often cannot fetch the data before this process quits,
// leaving an empty image in the clipboard.
void persistClipboardImage(const QImage& image, int timeoutMs = 3000);

} // namespace fouryshot::core
