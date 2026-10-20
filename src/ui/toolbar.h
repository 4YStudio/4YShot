#pragma once

#include <QWidget>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QColor>
#include <QList>
#include <QSpinBox>
#include <QFontComboBox>
#include <QStackedWidget>
#include <QLabel>
#include "../annotation/annotation_engine.h"

namespace fouryshot::ui {

class PreviewBox : public QWidget {
    Q_OBJECT
public:
    explicit PreviewBox(QWidget* parent = nullptr);

    void setTool(annotation::ToolType tool);
    void setColor(const QColor& color);
    void setStrokeWidth(int width);
    void setFontFamily(const QString& family);
    void setFontSize(int size);
    void setFilter(bool isBlur, int intensity);
    void setStep(int startNumber, int size);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    annotation::ToolType m_tool = annotation::ToolType::Pen;
    QColor m_color = QColor("#1677ff");
    int m_strokeWidth = 3;
    QString m_fontFamily;
    int m_fontSize = 16;
    bool m_isBlur = false;
    int m_filterIntensity = 12;
    int m_stepStartNumber = 1;
    int m_stepSize = 26;
};

class Toolbar : public QWidget {
    Q_OBJECT
public:
    explicit Toolbar(QWidget* parent = nullptr);

    void setSelectedTool(annotation::ToolType tool);
    annotation::ToolType selectedTool() const;

    void setCanUndo(bool canUndo);
    void setCanRedo(bool canRedo);

signals:
    void toolSelected(annotation::ToolType tool);
    void colorChanged(const QColor& color);
    void strokeWidthChanged(int width);
    void textFontChanged(const QString& family, int size);
    void filterParamsChanged(bool isBlur, int intensity);
    void stepParamsChanged(int startNumber, int size);
    void undoTriggered();
    void redoTriggered();
    void pinTriggered();
    void longCaptureTriggered();
    void ocrTriggered();
    void saveTriggered();
    void copyTriggered();
    void closeTriggered();
    void sizeChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void onToolButtonClicked(int id);
    void onColorButtonClicked(const QColor& color);

private:
    void createMainToolbar();
    void createSubToolbar();
    QToolButton* createToolButton(const QString& iconPath, const QString& tooltip, int id = -1, bool checkable = true);
    void updateColorButtonsState();

    QWidget* m_mainBar = nullptr;
    QWidget* m_subBar = nullptr;
    QVBoxLayout* m_rootLayout = nullptr;
    QHBoxLayout* m_mainLayout = nullptr;
    QHBoxLayout* m_subLayout = nullptr;

    QButtonGroup* m_toolGroup = nullptr;
    QToolButton* m_undoBtn = nullptr;
    QToolButton* m_redoBtn = nullptr;

    // Sub-toolbar preview and controls
    PreviewBox* m_previewBox = nullptr;
    QStackedWidget* m_paramStack = nullptr;

    // Stroke page controls
    QSpinBox* m_strokeSpin = nullptr;

    // Filter page controls
    QToolButton* m_mosaicBtn = nullptr;
    QToolButton* m_blurBtn = nullptr;
    QSpinBox* m_filterSpin = nullptr;

    // Text page controls
    QFontComboBox* m_fontCombo = nullptr;
    QSpinBox* m_textSizeSpin = nullptr;

    // Step page controls
    QSpinBox* m_stepStartSpin = nullptr;
    QSpinBox* m_stepSizeSpin = nullptr;

    // Color palette container
    QWidget* m_colorContainer = nullptr;
    QList<QPair<QToolButton*, QColor>> m_colorButtons;

    QColor m_currentColor = QColor("#1677ff");
    int m_currentWidth = 3;
    QString m_fontFamily;
    int m_fontSize = 16;
    bool m_isBlur = false;
    int m_filterIntensity = 12;
    int m_stepStartNumber = 1;
    int m_stepSize = 26;
    annotation::ToolType m_activeTool = annotation::ToolType::None;
};

} // namespace fouryshot::ui
