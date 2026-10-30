#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <memory>
#include <vector>
#include "../core/ocr_service.h"
#include "../core/ocr_text_model.h"
#include "../ui/text_selection_bubble.h"

namespace fouryshot::pin {

class PinWindow : public QWidget {
    Q_OBJECT
public:
    explicit PinWindow(const QPixmap& pixmap, const QPoint& initialPos, QWidget* parent = nullptr);
    ~PinWindow() override;

    void setZoom(qreal scale);
    qreal zoom() const;

    void setOpacity(qreal opacity);
    qreal opacity() const;

    void openOcrDialog();
    void copyAllOcrText();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void copyToClipboard();
    void saveToFile();
    int findOcrWordAt(const QPoint& localPos) const;
    int findNearestOcrWordAt(const QPoint& localPos) const;
    void updateSelectedOcrRange(int anchorIdx, int currentIdx);
    QList<QRect> getContinuousHighlightRects() const;
    QString getSelectedOcrText() const;
    QRect getSelectedOcrVisualBoundingRect() const;
    void updateTextBubble();
    Qt::Edges hitTestBorderEdges(const QPoint& pos) const;

    QPixmap m_pixmap;
    qreal m_scale = 1.0;
    qreal m_opacity = 1.0;
    bool m_isDragging = false;
    QPoint m_dragStartPos;
    bool m_isCollapsed = false;
    QSize m_preCollapseSize;

    // OCR Support
    std::unique_ptr<core::OcrService> m_ocrService;
    core::OcrTextModel m_ocrModel;
    bool m_ocrReady = false;
    bool m_isSelectingText = false;
    int m_textSelectAnchorIndex = -1;
    QPoint m_textSelectStart;
    ui::TextSelectionBubble* m_textBubble = nullptr;
};

} // namespace fouryshot::pin
