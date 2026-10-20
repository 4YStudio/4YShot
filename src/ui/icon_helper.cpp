#include "icon_helper.h"
#include <QPixmap>
#include <QPainter>

namespace fouryshot::ui {

QIcon IconHelper::createTintedIcon(
    const QString& svgPath,
    const QColor& normalColor,
    const QColor& checkedColor,
    const QColor& disabledColor,
    int size
) {
    int renderSize = size * 2; // 2x for sharp HiDPI rendering
    QPixmap basePixmap(svgPath);
    if (basePixmap.isNull()) {
        return QIcon(svgPath);
    }
    basePixmap = basePixmap.scaled(renderSize, renderSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    auto tint = [&basePixmap, renderSize](const QColor& col) -> QPixmap {
        QPixmap tinted(basePixmap.size());
        tinted.fill(Qt::transparent);

        QPainter p(&tinted);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawPixmap(0, 0, basePixmap);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(tinted.rect(), col);
        p.end();

        tinted.setDevicePixelRatio(2.0);
        return tinted;
    };

    QIcon icon;
    icon.addPixmap(tint(normalColor), QIcon::Normal, QIcon::Off);
    icon.addPixmap(tint(checkedColor), QIcon::Normal, QIcon::On);
    icon.addPixmap(tint(disabledColor), QIcon::Disabled, QIcon::Off);
    icon.addPixmap(tint(disabledColor), QIcon::Disabled, QIcon::On);

    return icon;
}

} // namespace fouryshot::ui
