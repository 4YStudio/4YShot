#pragma once

#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QRect>
#include <QRegion>
#include <QPoint>
#include <QLineEdit>
#include <QTimer>
#include <memory>
#include <vector>
#include "../platform/capture_manager.h"
#include "../core/ocr_service.h"
#include "selection_model.h"
#include "magnifier.h"
#include "toolbar.h"
#include "text_selection_bubble.h"
#include "../annotation/annotation_engine.h"

namespace fouryshot::ui {

class OverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWidget(
        const platform::ScreenTarget& target,
        const QRect& desktopGeometry,
        const QPixmap& fullDesktopPixmap,
        std::shared_ptr<QImage> fullDesktopImage,
        std::shared_ptr<SelectionModel> selectionModel,
        std::shared_ptr<annotation::AnnotationEngine> annotationEngine,
        QWidget* parent = nullptr
    );

    void showOverlay();
    void enterLongCaptureMode();
    void setLongCaptureInputRegion(const QRect& globalSelection, const QRect& localPanelRect);

signals:
    void captureCompleted();
    void captureCancelled();
    void longCaptureRequested(const QImage& firstSegment, const QRect& globalRegion, qreal dpr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onSelectionChanged(const QRect& rect);
    void onSelectionStateChanged(SelectionState state);
    void onToolSelected(annotation::ToolType tool);
    void onColorChanged(const QColor& color);
    void onStrokeWidthChanged(int width);
    void onTextFontChanged(const QString& family, int size);
    void onFilterParamsChanged(bool isBlur, int intensity);
    void onStepParamsChanged(int startNumber, int size);
    void onUndo();
    void onRedo();
    void onPin();
    void onLongCapture();
    void onOcr();
    void onSave();
    void onCopy();
    void onClose();
    void onCommitText();

private:
    void updateCursorIcon(const QPoint& pt);
    void positionMagnifier(const QPoint& localPos);
    void updateToolbarPosition();
    QPixmap renderSelectedAreaWithAnnotations() const;

    void triggerOcrScan();
    int findOcrWordAt(const QPoint& localPos) const;
    int findNearestOcrWordAt(const QPoint& localPos) const;
    void updateSelectedOcrRange(int anchorIdx, int currentIdx);
    QList<QRect> getContinuousHighlightRects() const;
    QString getSelectedOcrText() const;
    QRect getSelectedOcrVisualBoundingRect() const;
    void updateTextBubble();
    bool isNearSelectionBorder(const QPoint& globalPos, int margin = 6) const;
    void showToast(const QString& msg);

    platform::ScreenTarget m_target;
    QRect m_desktopGeometry;
    QPixmap m_fullDesktopPixmap;
    QPixmap m_screenPixmap;
    std::shared_ptr<QImage> m_fullDesktopImage;
    std::shared_ptr<SelectionModel> m_selectionModel;
    std::shared_ptr<annotation::AnnotationEngine> m_annotationEngine;

    Magnifier* m_magnifier = nullptr;
    Toolbar* m_toolbar = nullptr;
    QLineEdit* m_textEditor = nullptr;
    TextSelectionBubble* m_textBubble = nullptr;

    QPoint m_dragStartPos;
    QRect m_preDragRect;
    QPoint m_textEditorPos;

    // Active in-progress drawing item
    std::shared_ptr<annotation::AnnotationItem> m_previewItem;

    // In-place OCR word selection state
    std::unique_ptr<core::OcrService> m_ocrService;
    std::vector<core::OcrWordItem> m_ocrWords;
    std::vector<core::OcrLineItem> m_ocrLines;
    std::vector<int> m_selectedOcrIndices;
    int m_hoveredOcrIndex = -1;
    bool m_isSelectingText = false;
    bool m_longCaptureMode = false;
    int m_textSelectAnchorIndex = -1;
    QPoint m_textSelectStart;
    QPoint m_textSelectCurrent;
    qreal m_magnifierWheelRemainder = 0.0;

    // Toast feedback notification
    QString m_toastMessage;
    QTimer* m_toastTimer = nullptr;
};

} // namespace fouryshot::ui
