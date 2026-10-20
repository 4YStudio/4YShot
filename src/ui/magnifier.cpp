#include "magnifier.h"
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <algorithm>
#include <cmath>

namespace fouryshot::ui {

Magnifier::Magnifier(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFixedSize(GRID_SIZE * DISPLAY_PIXEL_SIZE + 16,
                 GRID_SIZE * DISPLAY_PIXEL_SIZE + 80);
}

void Magnifier::updateSample(const QPoint& cursorGlobalPos, const QImage& fullImage, qreal dpr) {
    m_cursorPos = cursorGlobalPos;
    m_dpr = dpr > 0 ? dpr : 1.0;

    int physX = std::round(cursorGlobalPos.x() * m_dpr);
    int physY = std::round(cursorGlobalPos.y() * m_dpr);

    if (fullImage.isNull()) {
        m_sampledImage = QImage();
        m_centerColor = Qt::black;
    } else {
        m_sampledImage = QImage(GRID_SIZE, GRID_SIZE, QImage::Format_RGB32);
        m_sampledImage.fill(Qt::black);

        int sampleSpan = GRID_SIZE * m_pixelsPerCell;
        int sampleStartX = physX - sampleSpan / 2;
        int sampleStartY = physY - sampleSpan / 2;
        for (int cellY = 0; cellY < GRID_SIZE; ++cellY) {
            for (int cellX = 0; cellX < GRID_SIZE; ++cellX) {
                int red = 0;
                int green = 0;
                int blue = 0;
                int sampleCount = 0;
                for (int dy = 0; dy < m_pixelsPerCell; ++dy) {
                    int y = sampleStartY + cellY * m_pixelsPerCell + dy;
                    if (y < 0 || y >= fullImage.height()) continue;
                    for (int dx = 0; dx < m_pixelsPerCell; ++dx) {
                        int x = sampleStartX + cellX * m_pixelsPerCell + dx;
                        if (x < 0 || x >= fullImage.width()) continue;
                        QRgb pixel = fullImage.pixel(x, y);
                        red += qRed(pixel);
                        green += qGreen(pixel);
                        blue += qBlue(pixel);
                        ++sampleCount;
                    }
                }
                if (sampleCount > 0) {
                    m_sampledImage.setPixelColor(
                        cellX, cellY,
                        QColor(red / sampleCount, green / sampleCount, blue / sampleCount)
                    );
                }
            }
        }

        if (physX >= 0 && physX < fullImage.width() && physY >= 0 && physY < fullImage.height()) {
            m_centerColor = fullImage.pixelColor(physX, physY);
        } else {
            m_centerColor = Qt::black;
        }
    }
    update();
}

void Magnifier::adjustZoom(int wheelSteps) {
    if (wheelSteps == 0) return;
    int nextPixelsPerCell = std::clamp(
        m_pixelsPerCell - wheelSteps, MIN_PIXELS_PER_CELL, MAX_PIXELS_PER_CELL
    );
    if (nextPixelsPerCell == m_pixelsPerCell) return;
    m_pixelsPerCell = nextPixelsPerCell;
    update();
}

QColor Magnifier::currentColor() const {
    return m_centerColor;
}

QString Magnifier::hexColor() const {
    return m_centerColor.name().toUpper();
}

QString Magnifier::rgbColor() const {
    return QString("rgb(%1, %2, %3)")
        .arg(m_centerColor.red())
        .arg(m_centerColor.green())
        .arg(m_centerColor.blue());
}

void Magnifier::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background card with shadow
    QRect cardRect = rect().adjusted(1, 1, -1, -1);
    painter.setPen(QPen(QColor(0, 0, 0, 40), 1));
    painter.setBrush(QColor(30, 30, 30, 220));
    painter.drawRoundedRect(cardRect, 8, 8);

    int gridWidth = GRID_SIZE * DISPLAY_PIXEL_SIZE;
    int gridHeight = GRID_SIZE * DISPLAY_PIXEL_SIZE;
    int startX = (width() - gridWidth) / 2;
    int startY = 8;

    // Render 11x11 pixel grid
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            QRect pixelRect(startX + x * DISPLAY_PIXEL_SIZE, startY + y * DISPLAY_PIXEL_SIZE,
                            DISPLAY_PIXEL_SIZE, DISPLAY_PIXEL_SIZE);
            QColor pxColor = Qt::black;
            if (!m_sampledImage.isNull() && x < m_sampledImage.width() && y < m_sampledImage.height()) {
                pxColor = m_sampledImage.pixelColor(x, y);
            }

            painter.setPen(Qt::NoPen);
            painter.setBrush(pxColor);
            painter.drawRect(pixelRect);

            // Subtle grid line
            painter.setPen(QColor(255, 255, 255, 25));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(pixelRect);
        }
    }

    // Highlight center pixel
    int centerIdx = GRID_SIZE / 2;
    QRect centerRect(startX + centerIdx * DISPLAY_PIXEL_SIZE - 1,
                     startY + centerIdx * DISPLAY_PIXEL_SIZE - 1,
                     DISPLAY_PIXEL_SIZE + 2, DISPLAY_PIXEL_SIZE + 2);
    painter.setPen(QPen(QColor("#1677ff"), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(centerRect);

    // Text details
    QFont font = painter.font();
    font.setPixelSize(10);
    font.setFamily("sans-serif");
    painter.setFont(font);

    int textY = startY + gridHeight + 14;

    // Position text
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(startX, textY, QString("(%1, %2)").arg(m_cursorPos.x()).arg(m_cursorPos.y()));

    // Color swatch & Hex
    int swatchSize = 10;
    QRect swatchRect(startX, textY + 6, swatchSize, swatchSize);
    painter.setPen(QPen(Qt::white, 1));
    painter.setBrush(m_centerColor);
    painter.drawRect(swatchRect);

    painter.setPen(Qt::white);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(startX + swatchSize + 5, textY + 15, hexColor());

    // RGB & shortcut tip
    font.setBold(false);
    font.setPixelSize(9);
    painter.setFont(font);
    painter.setPen(QColor(160, 160, 160));
    painter.drawText(startX, textY + 30, rgbColor());
    painter.drawText(startX, textY + 42,
                     QString("%1x%1/格  C复制").arg(m_pixelsPerCell));
}

} // namespace fouryshot::ui
