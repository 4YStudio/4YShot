#pragma once

#include <QIcon>
#include <QString>
#include <QColor>

namespace fouryshot::ui {

class IconHelper {
public:
    static QIcon createTintedIcon(
        const QString& svgPath,
        const QColor& normalColor = QColor("#ffffff"),
        const QColor& checkedColor = QColor("#ffffff"),
        const QColor& disabledColor = QColor(255, 255, 255, 70),
        int size = 20
    );
};

} // namespace fouryshot::ui
