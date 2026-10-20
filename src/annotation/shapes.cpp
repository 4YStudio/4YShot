#include "shapes.h"
#include <cmath>
#include <QFont>
#include <QFontMetrics>

namespace fouryshot::annotation {

bool AnnotationItem::contains(const QPoint& pt) const {
    return boundingRect().contains(pt);
}

void AnnotationItem::translate(const QPoint& delta) {
    // default no-op, implemented in subclasses
}

// ---------------- RectItem ----------------
RectItem::RectItem(const QRect& rect, const QColor& color, int strokeWidth, bool rounded)
    : rect(rect), rounded(rounded) {
    this->type = ItemType::Rectangle;
    this->color = color;
    this->strokeWidth = strokeWidth;
}

void RectItem::draw(QPainter& painter) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    if (rounded) {
        painter.drawRoundedRect(rect.normalized(), 8, 8);
    } else {
        painter.drawRect(rect.normalized());
    }
    painter.restore();
}

QRect RectItem::boundingRect() const {
    int margin = strokeWidth / 2 + 2;
    return rect.normalized().adjusted(-margin, -margin, margin, margin);
}

void RectItem::translate(const QPoint& delta) {
    rect.translate(delta);
}

// ---------------- EllipseItem ----------------
EllipseItem::EllipseItem(const QRect& rect, const QColor& color, int strokeWidth)
    : rect(rect) {
    this->type = ItemType::Ellipse;
    this->color = color;
    this->strokeWidth = strokeWidth;
}

void EllipseItem::draw(QPainter& painter) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(rect.normalized());
    painter.restore();
}

QRect EllipseItem::boundingRect() const {
    int margin = strokeWidth / 2 + 2;
    return rect.normalized().adjusted(-margin, -margin, margin, margin);
}

void EllipseItem::translate(const QPoint& delta) {
    rect.translate(delta);
}

// ---------------- LineItem ----------------
LineItem::LineItem(const QPoint& start, const QPoint& end, const QColor& color, int strokeWidth)
    : start(start), end(end) {
    this->type = ItemType::Line;
    this->color = color;
    this->strokeWidth = strokeWidth;
}

void LineItem::draw(QPainter& painter) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(start, end);
    painter.restore();
}

QRect LineItem::boundingRect() const {
    int margin = strokeWidth / 2 + 4;
    return QRect(start, end).normalized().adjusted(-margin, -margin, margin, margin);
}

void LineItem::translate(const QPoint& delta) {
    start += delta;
    end += delta;
}

// ---------------- ArrowItem ----------------
ArrowItem::ArrowItem(const QPoint& start, const QPoint& end, const QColor& color, int strokeWidth)
    : start(start), end(end) {
    this->type = ItemType::Arrow;
    this->color = color;
    this->strokeWidth = strokeWidth;
}

void ArrowItem::draw(QPainter& painter) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);

    // Draw main line
    painter.drawLine(start, end);

    // Calculate arrow head
    double dx = end.x() - start.x();
    double dy = end.y() - start.y();
    double length = std::sqrt(dx * dx + dy * dy);
    if (length > 2.0) {
        double angle = std::atan2(dy, dx);
        double arrowSize = std::max(12.0, strokeWidth * 3.5);
        double arrowAngle = M_PI / 6.0; // 30 degrees

        QPointF p1 = end;
        QPointF p2 = end - QPointF(arrowSize * std::cos(angle - arrowAngle), arrowSize * std::sin(angle - arrowAngle));
        QPointF p3 = end - QPointF(arrowSize * std::cos(angle + arrowAngle), arrowSize * std::sin(angle + arrowAngle));

        QPolygonF head;
        head << p1 << p2 << p3;
        painter.drawPolygon(head);
    }
    painter.restore();
}

QRect ArrowItem::boundingRect() const {
    int margin = strokeWidth * 4;
    return QRect(start, end).normalized().adjusted(-margin, -margin, margin, margin);
}

void ArrowItem::translate(const QPoint& delta) {
    start += delta;
    end += delta;
}

// ---------------- PathItem ----------------
PathItem::PathItem(const QList<QPoint>& points, const QColor& color, int strokeWidth, bool isHighlighter)
    : points(points), isHighlighter(isHighlighter) {
    this->type = isHighlighter ? ItemType::Highlighter : ItemType::Pen;
    this->color = color;
    this->strokeWidth = strokeWidth;
}

void PathItem::addPoint(const QPoint& pt) {
    points.append(pt);
}

void PathItem::draw(QPainter& painter) {
    if (points.size() < 2) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    if (isHighlighter) {
        QColor hlColor = color;
        hlColor.setAlpha(110);
        painter.setPen(QPen(hlColor, strokeWidth * 2.5, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
    } else {
        painter.setPen(QPen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    }

    QPainterPath path;
    path.moveTo(points.first());
    for (int i = 1; i < points.size(); ++i) {
        path.lineTo(points[i]);
    }
    painter.drawPath(path);
    painter.restore();
}

QRect PathItem::boundingRect() const {
    if (points.isEmpty()) return QRect();
    int minX = points.first().x(), maxX = minX;
    int minY = points.first().y(), maxY = minY;
    for (const auto& p : points) {
        minX = std::min(minX, p.x());
        maxX = std::max(maxX, p.x());
        minY = std::min(minY, p.y());
        maxY = std::max(maxY, p.y());
    }
    int margin = strokeWidth * 3;
    return QRect(minX, minY, maxX - minX, maxY - minY).adjusted(-margin, -margin, margin, margin);
}

void PathItem::translate(const QPoint& delta) {
    for (auto& p : points) {
        p += delta;
    }
}

// ---------------- StepNumberItem ----------------
StepNumberItem::StepNumberItem(const QPoint& center, int number, const QColor& color, int size)
    : center(center), number(number), size(size) {
    this->type = ItemType::StepNumber;
    this->color = color;
    this->strokeWidth = 2;
}

void StepNumberItem::draw(QPainter& painter) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw shadow
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.drawEllipse(center.x() - size / 2 + 1, center.y() - size / 2 + 2, size, size);

    // Draw circle
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(color);
    painter.drawEllipse(center.x() - size / 2, center.y() - size / 2, size, size);

    // Draw number
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(size * 0.58);
    painter.setFont(font);

    QRect textRect(center.x() - size / 2, center.y() - size / 2, size, size);
    painter.drawText(textRect, Qt::AlignCenter, QString::number(number));
    painter.restore();
}

QRect StepNumberItem::boundingRect() const {
    return QRect(center.x() - size / 2 - 2, center.y() - size / 2 - 2, size + 4, size + 4);
}

void StepNumberItem::translate(const QPoint& delta) {
    center += delta;
}

// ---------------- TextItem ----------------
TextItem::TextItem(const QPoint& pos, const QString& text, const QColor& color, int fontSize, const QString& fontFamily)
    : pos(pos), text(text), fontSize(fontSize), fontFamily(fontFamily) {
    this->type = ItemType::Text;
    this->color = color;
    this->strokeWidth = 2;
}

void TextItem::draw(QPainter& painter) {
    if (text.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    QFont font = painter.font();
    if (!fontFamily.isEmpty()) {
        font.setFamily(fontFamily);
    }
    font.setPixelSize(fontSize);
    font.setBold(true);
    painter.setFont(font);

    QFontMetrics fm(font);
    QRect bounds = fm.boundingRect(QRect(0, 0, 1000, 1000), Qt::AlignLeft | Qt::TextWordWrap, text);
    QRect bgRect = bounds.translated(pos).adjusted(-6, -4, 6, 4);

    // Dark-themed bubble
    painter.setPen(QPen(color, 1));
    painter.setBrush(QColor(24, 24, 28, 220));
    painter.drawRoundedRect(bgRect, 4, 4);

    // Text
    painter.setPen(color);
    painter.drawText(bounds.translated(pos), Qt::AlignLeft | Qt::TextWordWrap, text);
    painter.restore();
}

QRect TextItem::boundingRect() const {
    QFont font;
    if (!fontFamily.isEmpty()) {
        font.setFamily(fontFamily);
    }
    font.setPixelSize(fontSize);
    font.setBold(true);
    QFontMetrics fm(font);
    QRect bounds = fm.boundingRect(QRect(0, 0, 1000, 1000), Qt::AlignLeft | Qt::TextWordWrap, text);
    return bounds.translated(pos).adjusted(-8, -6, 8, 6);
}

void TextItem::translate(const QPoint& delta) {
    pos += delta;
}

// ---------------- FilterItem ----------------
FilterItem::FilterItem(const QRect& rect, bool isBlur, int intensity)
    : rect(rect), isBlur(isBlur), intensity(intensity) {
    this->type = isBlur ? ItemType::Blur : ItemType::Mosaic;
}

void FilterItem::applyFilter(const QImage& sourceImage, qreal dpr) {
    if (dpr <= 0) dpr = 1.0;
    QRect normalizedRect = rect.normalized();
    QRect physRect(
        std::round(normalizedRect.x() * dpr),
        std::round(normalizedRect.y() * dpr),
        std::round(normalizedRect.width() * dpr),
        std::round(normalizedRect.height() * dpr)
    );
    QRect validPhysRect = physRect.intersected(sourceImage.rect());
    if (validPhysRect.isEmpty()) return;

    QImage crop = sourceImage.copy(validPhysRect);
    if (isBlur) {
        int radius = std::max(2, intensity);
        int w = std::max(2, validPhysRect.width() / radius);
        int h = std::max(2, validPhysRect.height() / radius);
        crop = crop.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        crop = crop.scaled(validPhysRect.width(), validPhysRect.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    } else {
        int blockSize = std::max(4, static_cast<int>(intensity * dpr));
        int w = std::max(1, validPhysRect.width() / blockSize);
        int h = std::max(1, validPhysRect.height() / blockSize);
        crop = crop.scaled(w, h, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        crop = crop.scaled(validPhysRect.width(), validPhysRect.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    filteredImage = crop;
}

void FilterItem::draw(QPainter& painter) {
    if (!filteredImage.isNull()) {
        painter.drawImage(rect.normalized(), filteredImage);
    }
}

QRect FilterItem::boundingRect() const {
    return rect.normalized();
}

void FilterItem::translate(const QPoint& delta) {
    rect.translate(delta);
}

} // namespace fouryshot::annotation
