#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QColor>

namespace fouryshot::ui {

class Magnifier : public QWidget {
    Q_OBJECT
public:
    explicit Magnifier(QWidget* parent = nullptr);

    void updateSample(const QPoint& cursorGlobalPos, const QImage& fullImage, qreal dpr = 1.0);
    void adjustZoom(int wheelSteps);
    QColor currentColor() const;
    QString hexColor() const;
    QString rgbColor() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPoint m_cursorPos;
    QImage m_sampledImage;
    QColor m_centerColor = Qt::black;
    qreal m_dpr = 1.0;
    int m_pixelsPerCell = 3;

    static constexpr int GRID_SIZE = 11; // 11x11 pixels
    static constexpr int DISPLAY_PIXEL_SIZE = 9;
    static constexpr int MIN_PIXELS_PER_CELL = 1;
    static constexpr int MAX_PIXELS_PER_CELL = 8;
};

} // namespace fouryshot::ui
