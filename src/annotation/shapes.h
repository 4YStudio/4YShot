#pragma once

#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QPoint>
#include <QColor>
#include <QString>
#include <QImage>
#include <vector>
#include <memory>

namespace fouryshot::annotation {

enum class ItemType {
    Rectangle,
    Ellipse,
    Arrow,
    Line,
    Pen,
    Highlighter,
    StepNumber,
    Text,
    Mosaic,
    Blur
};

class AnnotationItem {
public:
    virtual ~AnnotationItem() = default;
    virtual void draw(QPainter& painter) = 0;
    virtual QRect boundingRect() const = 0;
    virtual bool contains(const QPoint& pt) const;
    virtual void translate(const QPoint& delta);

    ItemType type = ItemType::Rectangle;
    QColor color = QColor("#1677ff");
    int strokeWidth = 3;
    bool isSelected = false;
};

class RectItem : public AnnotationItem {
public:
    RectItem(const QRect& rect, const QColor& color, int strokeWidth, bool rounded = false);
    void draw(QPainter& painter) override;
    QRect boundingRect() const override;
    void translate(const QPoint& delta) override;

    QRect rect;
    bool rounded = false;
};

class EllipseItem : public AnnotationItem {
public:
    EllipseItem(const QRect& rect, const QColor& color, int strokeWidth);
    void draw(QPainter& painter) override;
    QRect boundingRect() const override;
    void translate(const QPoint& delta) override;

    QRect rect;
};

class LineItem : public AnnotationItem {
public:
    LineItem(const QPoint& start, const QPoint& end, const QColor& color, int strokeWidth);
    void draw(QPainter& painter) override;
    QRect boundingRect() const override;
    void translate(const QPoint& delta) override;

    QPoint start;
    QPoint end;
};

class ArrowItem : public AnnotationItem {
public:
    ArrowItem(const QPoint& start, const QPoint& end, const QColor& color, int strokeWidth);
    void draw(QPainter& painter) override;
    QRect boundingRect() const override;
    void translate(const QPoint& delta) override;

    QPoint start;
    QPoint end;
};

class PathItem : public AnnotationItem {
public:
    PathItem(const QList<QPoint>& points, const QColor& color, int strokeWidth, bool isHighlighter = false);
    void addPoint(const QPoint& pt);
    void draw(QPainter& painter) override;
    QRect boundingRect() const override;
    void translate(const QPoint& delta) override;

    QList<QPoint> points;
    bool isHighlighter = false;
};

class StepNumberItem : public AnnotationItem {
public:
    StepNumberItem(const QPoint& center, int number, const QColor& color, int size = 26);
    void draw(QPainter& painter) override;
    QRect boundingRect() const override;
    void translate(const QPoint& delta) override;

    QPoint center;
    int number = 1;
    int size = 26;
};

class TextItem : public AnnotationItem {
public:
    TextItem(const QPoint& pos, const QString& text, const QColor& color, int fontSize = 16, const QString& fontFamily = "");
    void draw(QPainter& painter) override;
    QRect boundingRect() const override;
    void translate(const QPoint& delta) override;

    QPoint pos;
    QString text;
    int fontSize = 16;
    QString fontFamily;
};

class FilterItem : public AnnotationItem {
public:
    FilterItem(const QRect& rect, bool isBlur = false, int intensity = 12);
    void draw(QPainter& painter) override;
    QRect boundingRect() const override;
    void translate(const QPoint& delta) override;
    void applyFilter(const QImage& sourceImage, qreal dpr = 1.0);

    QRect rect;
    bool isBlur = false;
    int intensity = 12;
    QImage filteredImage;
};

} // namespace fouryshot::annotation
