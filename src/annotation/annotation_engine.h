#pragma once

#include "shapes.h"
#include <QObject>
#include <QPainter>
#include <memory>
#include <vector>

namespace fouryshot::annotation {

enum class ToolType {
    None,
    Rectangle,
    Ellipse,
    Line,
    Arrow,
    Pen,
    Highlighter,
    StepNumber,
    Text,
    Mosaic,
    Blur
};

class AnnotationEngine : public QObject {
    Q_OBJECT
public:
    explicit AnnotationEngine(QObject* parent = nullptr);

    void setTool(ToolType tool);
    ToolType currentTool() const;

    void setColor(const QColor& color);
    QColor currentColor() const;

    void setStrokeWidth(int width);
    int currentStrokeWidth() const;

    void setFontSize(int size);
    int currentFontSize() const;

    void setFontFamily(const QString& family);
    QString currentFontFamily() const;

    void setFilterIntensity(int intensity);
    int currentFilterIntensity() const;

    void setIsBlur(bool isBlur);
    bool isBlur() const;

    void setStepSize(int size);
    int currentStepSize() const;

    void setStepStartNumber(int num);
    int stepStartNumber() const;

    void addItem(std::shared_ptr<AnnotationItem> item);
    void removeItem(std::shared_ptr<AnnotationItem> item);
    void undo();
    void redo();
    void clear();

    bool canUndo() const;
    bool canRedo() const;

    int nextStepNumber() const;

    void draw(QPainter& painter);

    const std::vector<std::shared_ptr<AnnotationItem>>& items() const;

signals:
    void stateChanged();

private:
    ToolType m_tool = ToolType::None;
    QColor m_color = QColor("#1677ff");
    int m_strokeWidth = 3;
    int m_fontSize = 16;
    QString m_fontFamily;
    int m_filterIntensity = 12;
    bool m_isBlur = false;
    int m_stepSize = 26;
    int m_stepStartNumber = 1;

    std::vector<std::shared_ptr<AnnotationItem>> m_items;
    std::vector<std::shared_ptr<AnnotationItem>> m_undoHistory;
};

} // namespace fouryshot::annotation
