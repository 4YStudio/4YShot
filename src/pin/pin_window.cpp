#include "pin_window.h"
#include "../core/clipboard_util.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QWindow>
#include <QTimer>
#include <QCursor>
#include <algorithm>
#include <unordered_set>
#include <limits>
#include "../ui/ocr_dialog.h"

namespace fouryshot::pin {

PinWindow::PinWindow(const QPixmap& pixmap, const QPoint& initialPos, QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool),
      m_pixmap(pixmap) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);

    winId(); // Force creation of native QWindow handle for Wayland protocols

    // The incoming pixmap may carry a devicePixelRatio (e.g. 1.75 on HiDPI
    // screens). Qt widget geometry is in logical units, so the window must be
    // sized with the pixmap's LOGICAL size, otherwise the image is upscaled
    // by the DPR factor and ends up blurry/oversized.
    const qreal dpr = m_pixmap.devicePixelRatio() > 0.0 ? m_pixmap.devicePixelRatio() : 1.0;
    const int logicalW = qRound(m_pixmap.width() / dpr);
    const int logicalH = qRound(m_pixmap.height() / dpr);
    int w = std::max(60, logicalW);
    int h = std::max(60, logicalH);
    resize(w + 8, h + 8);
    move(initialPos);
    setCursor(Qt::SizeAllCursor);
    show();

    if (qEnvironmentVariableIsSet("FOURYSHOT_OCR_DEBUG")) {
        qInfo() << "[Pin] created" << frameGeometry() << "pixmap" << m_pixmap.width() << "x" << m_pixmap.height() << "dpr" << dpr;
        auto* dbgTimer = new QTimer(this);
        connect(dbgTimer, &QTimer::timeout, this, [this]() {
            qInfo() << "[Pin] cursor" << QCursor::pos();
        });
        dbgTimer->start(150);
    }

    m_textBubble = new ui::TextSelectionBubble(this);

    // PinWindow lays out and hit-tests against the pixmap's raw pixel dimensions.
    // Keep OCR boxes in that same coordinate space; using the pixmap DPR here
    // would shrink the boxes while the image itself is still addressed in pixels.
    m_ocrService = std::make_unique<core::OcrService>(this);
    m_ocrService->recognizeAsync(m_pixmap, m_pixmap.devicePixelRatio(), [this](const core::OcrResult& res) {
        if (res.success) {
            m_ocrModel.setResult(res);
            m_ocrReady = true;
            update();
        }
    });
}

PinWindow::~PinWindow() {
    if (m_textBubble) {
        m_textBubble->hideBubble();
        m_textBubble->deleteLater();
    }
    if (m_ocrService) {
        m_ocrService->cancel();
    }
}

void PinWindow::setZoom(qreal scale) {
    m_scale = std::clamp(scale, 0.1, 8.0);
    const qreal dpr = m_pixmap.devicePixelRatio() > 0.0 ? m_pixmap.devicePixelRatio() : 1.0;
    int w = std::max(60, static_cast<int>(qRound(m_pixmap.width() / dpr) * m_scale));
    int h = std::max(60, static_cast<int>(qRound(m_pixmap.height() / dpr) * m_scale));
    resize(w + 8, h + 8);
    update();
}

qreal PinWindow::zoom() const {
    return m_scale;
}

void PinWindow::setOpacity(qreal opacity) {
    m_opacity = std::clamp(opacity, 0.15, 1.0);
    setWindowOpacity(m_opacity);
    update();
}

qreal PinWindow::opacity() const {
    return m_opacity;
}

void PinWindow::openOcrDialog() {
    if (m_pixmap.isNull()) return;
    auto* dlg = new ui::OcrDialog(m_pixmap, nullptr);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void PinWindow::copyAllOcrText() {
    if (!m_ocrModel.fullText().isEmpty()) {
        QGuiApplication::clipboard()->setText(m_ocrModel.fullText());
    } else {
        openOcrDialog();
    }
}

Qt::Edges PinWindow::hitTestBorderEdges(const QPoint& pt) const {
    Qt::Edges edges;
    const int margin = 6;
    if (pt.x() <= margin) edges |= Qt::LeftEdge;
    if (pt.x() >= width() - margin) edges |= Qt::RightEdge;
    if (pt.y() <= margin) edges |= Qt::TopEdge;
    if (pt.y() >= height() - margin) edges |= Qt::BottomEdge;
    return edges;
}

int PinWindow::findOcrWordAt(const QPoint& localPos) const {
    if (!m_ocrModel.hasWords()) return -1;
    QRect contentRect = rect().adjusted(4, 4, -4, -4);
    if (!contentRect.contains(localPos)) return -1;

    // Scale from contentRect (logical window coords) to the pixmap's logical
    // pixel space; word rects live in that space.
    const qreal dpr = m_pixmap.devicePixelRatio() > 0.0 ? m_pixmap.devicePixelRatio() : 1.0;
    const qreal scaleX = static_cast<qreal>(contentRect.width()) / (m_pixmap.width() / dpr);
    const qreal scaleY = static_cast<qreal>(contentRect.height()) / (m_pixmap.height() / dpr);
    const QPoint ptInImage(
        static_cast<int>((localPos.x() - contentRect.left()) / scaleX),
        static_cast<int>((localPos.y() - contentRect.top()) / scaleY)
    );
    return m_ocrModel.findWordAt(ptInImage);
}

int PinWindow::findNearestOcrWordAt(const QPoint& localPos) const {
    if (!m_ocrModel.hasWords() || m_pixmap.width() <= 0 || m_pixmap.height() <= 0) return -1;
    QRect contentRect = rect().adjusted(4, 4, -4, -4);

    const qreal dpr = m_pixmap.devicePixelRatio() > 0.0 ? m_pixmap.devicePixelRatio() : 1.0;
    const qreal scaleX = static_cast<qreal>(contentRect.width()) / (m_pixmap.width() / dpr);
    const qreal scaleY = static_cast<qreal>(contentRect.height()) / (m_pixmap.height() / dpr);

    const int clampedX = std::clamp(localPos.x(), contentRect.left(), contentRect.right());
    const int clampedY = std::clamp(localPos.y(), contentRect.top(), contentRect.bottom());
    const QPoint ptInImage(
        static_cast<int>((clampedX - contentRect.left()) / scaleX),
        static_cast<int>((clampedY - contentRect.top()) / scaleY)
    );
    return m_ocrModel.findNearestWordAt(ptInImage);
}

void PinWindow::updateSelectedOcrRange(int anchorIdx, int currentIdx) {
    m_ocrModel.updateSelectedRange(anchorIdx, currentIdx);
}

QList<QRect> PinWindow::getContinuousHighlightRects() const {
    QList<QRect> rects;
    if (!m_ocrModel.hasSelection() || m_pixmap.width() <= 0 || m_pixmap.height() <= 0) return rects;

    const bool debug = qEnvironmentVariableIsSet("FOURYSHOT_OCR_DEBUG");
    if (debug) {
        qInfo() << "[Highlight] selected" << m_ocrModel.selectedIndices().size()
                << "words" << m_ocrModel.wordCount() << "lines" << m_ocrModel.lines().size()
                << "pixmap" << m_pixmap.width() << "x" << m_pixmap.height()
                << "dpr" << m_pixmap.devicePixelRatio();
        for (int idx : m_ocrModel.selectedIndices()) {
            if (idx >= 0 && idx < m_ocrModel.wordCount()) {
                qInfo() << "[Highlight] word" << idx << m_ocrModel.word(idx).logicalRect
                        << m_ocrModel.word(idx).text;
            }
        }
    }

    QRect contentRect = rect().adjusted(4, 4, -4, -4);
    const qreal dpr = m_pixmap.devicePixelRatio() > 0.0 ? m_pixmap.devicePixelRatio() : 1.0;
    const qreal scaleX = static_cast<qreal>(contentRect.width()) / (m_pixmap.width() / dpr);
    const qreal scaleY = static_cast<qreal>(contentRect.height()) / (m_pixmap.height() / dpr);

    auto toVisual = [&](int left, int top, int width, int height) {
        return QRect(
            contentRect.left() + static_cast<int>(left * scaleX),
            contentRect.top() + static_cast<int>(top * scaleY),
            std::max(1, static_cast<int>(width * scaleX)),
            std::max(1, static_cast<int>(height * scaleY))
        );
    };

    // Model returns rects in OCR logical space; map them into window space.
    for (const QRect& r : m_ocrModel.highlightRects()) {
        rects.append(toVisual(r.left(), r.top(), r.width(), r.height()));
    }

    if (debug) {
        qInfo() << "[Highlight] ribbon rects" << rects.size();
        for (const auto& r : rects) qInfo() << "[Highlight] ribbon" << r;
    }
    return rects;
}

QString PinWindow::getSelectedOcrText() const {
    return m_ocrModel.selectedText();
}

QRect PinWindow::getSelectedOcrVisualBoundingRect() const {
    if (m_pixmap.width() <= 0 || m_pixmap.height() <= 0) return QRect();
    QRect contentRect = rect().adjusted(4, 4, -4, -4);
    const qreal dpr = m_pixmap.devicePixelRatio() > 0.0 ? m_pixmap.devicePixelRatio() : 1.0;
    const qreal scaleX = static_cast<qreal>(contentRect.width()) / (m_pixmap.width() / dpr);
    const qreal scaleY = static_cast<qreal>(contentRect.height()) / (m_pixmap.height() / dpr);
    const QRect u = m_ocrModel.selectedBoundingRect();
    if (u.isNull()) return QRect();
    return QRect(
        contentRect.left() + static_cast<int>(u.left() * scaleX),
        contentRect.top() + static_cast<int>(u.top() * scaleY),
        std::max(1, static_cast<int>(u.width() * scaleX)),
        std::max(1, static_cast<int>(u.height() * scaleY))
    );
}

void PinWindow::updateTextBubble() {
    if (!m_ocrModel.hasSelection()) {
        if (m_textBubble) {
            m_textBubble->hideBubble();
        }
        return;
    }

    QString text = getSelectedOcrText();
    QRect bounds = getSelectedOcrVisualBoundingRect();
    if (m_textBubble) {
        m_textBubble->updateBubble(text, bounds);
    }
}

void PinWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QRect contentRect = rect().adjusted(4, 4, -4, -4);

    // Soft drop shadow
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 100));
    painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 6, 6);

    // Subtle dark-mode border
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(contentRect, 4, 4);

    // Draw image inside contentRect
    painter.drawPixmap(contentRect, m_pixmap);

    // Render continuous OCR highlight overlays whenever text is selected or hovered
    if ((m_ocrModel.hasSelection() || m_ocrModel.hoveredIndex() != -1) && m_pixmap.width() > 0 && m_pixmap.height() > 0) {
        const qreal dpr = m_pixmap.devicePixelRatio();
        qreal scaleX = static_cast<qreal>(contentRect.width()) / (m_pixmap.width() / dpr);
        qreal scaleY = static_cast<qreal>(contentRect.height()) / (m_pixmap.height() / dpr);

        // A. Continuous selection highlight ribbons
        auto highlightRects = getContinuousHighlightRects();
        for (const auto& hr : highlightRects) {
            painter.setPen(QPen(QColor(22, 119, 255, 220), 1.2));
            painter.setBrush(QColor(22, 119, 255, 110));
            painter.drawRoundedRect(hr, 3, 3);
        }

        // B. Hovered word box
        if (m_ocrModel.hoveredIndex() >= 0 && m_ocrModel.hoveredIndex() < m_ocrModel.wordCount()) {
            if (!m_ocrModel.isSelected(m_ocrModel.hoveredIndex())) {
                const auto& item = m_ocrModel.word(m_ocrModel.hoveredIndex());
                QRect wr(
                    contentRect.left() + static_cast<int>(item.logicalRect.left() * scaleX),
                    contentRect.top() + static_cast<int>(item.logicalRect.top() * scaleY),
                    static_cast<int>(item.logicalRect.width() * scaleX),
                    static_cast<int>(item.logicalRect.height() * scaleY)
                );
                painter.setPen(QPen(QColor(22, 119, 255, 180), 1, Qt::DashLine));
                painter.setBrush(QColor(22, 119, 255, 35));
                painter.drawRoundedRect(wr.adjusted(-1, -1, 1, 1), 3, 3);
            }
        }
    }
}

void PinWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 1. Check if user is resizing near borders
        auto edges = hitTestBorderEdges(event->pos());
        if (edges != Qt::Edges() && window()->windowHandle()) {
            if (window()->windowHandle()->startSystemResize(edges)) {
                event->accept();
                return;
            }
        }

        // 2. Alt modifier forces window dragging even when on text
        if (event->modifiers() & Qt::AltModifier) {
            if (window()->windowHandle() && window()->windowHandle()->startSystemMove()) {
                event->accept();
                return;
            }
            m_isDragging = true;
            m_dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
            return;
        }

        // 3. Seamless direct text selection: click on any OCR word
        int hitWord = findOcrWordAt(event->pos());
        if (hitWord != -1) {
            m_isSelectingText = true;
            m_textSelectStart = event->pos();
            m_textSelectAnchorIndex = hitWord;
            m_ocrModel.selectSingle(hitWord);
            if (m_textBubble) {
                m_textBubble->hideBubble();
            }
            update();
            event->accept();
            return;
        }

        // 4. Clicked outside text: clear selection if any
        if (m_ocrModel.hasSelection()) {
            m_ocrModel.clearSelection();
            m_textSelectAnchorIndex = -1;
            if (m_textBubble) {
                m_textBubble->hideBubble();
            }
            update();
        }

        // 5. Default: Move window natively via Wayland startSystemMove()
        if (window()->windowHandle() && window()->windowHandle()->startSystemMove()) {
            event->accept();
            return;
        }

        // Fallback for X11 / environments without startSystemMove
        m_isDragging = true;
        m_dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void PinWindow::mouseMoveEvent(QMouseEvent* event) {
    // If dragging text selection
    if (m_isSelectingText) {
        int nearestWord = findNearestOcrWordAt(event->pos());
        if (nearestWord != -1 && m_textSelectAnchorIndex != -1) {
            updateSelectedOcrRange(m_textSelectAnchorIndex, nearestWord);
            update();
        }
        event->accept();
        return;
    }

    // If X11 dragging fallback
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragStartPos);
        event->accept();
        return;
    }

    // Dynamic hover cursor updates when mouse is not pressed
    if (!event->buttons()) {
        auto edges = hitTestBorderEdges(event->pos());
        if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) || (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
            setCursor(Qt::SizeFDiagCursor);
            return;
        } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) || (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
            setCursor(Qt::SizeBDiagCursor);
            return;
        } else if (edges & Qt::LeftEdge || edges & Qt::RightEdge) {
            setCursor(Qt::SizeHorCursor);
            return;
        } else if (edges & Qt::TopEdge || edges & Qt::BottomEdge) {
            setCursor(Qt::SizeVerCursor);
            return;
        }

        // Seamless text detection: show IBeamCursor when hovering over OCR words
        int hitWord = findOcrWordAt(event->pos());
        if (hitWord != -1) {
            if (m_ocrModel.hoveredIndex() != hitWord) {
                m_ocrModel.setHoveredIndex(hitWord);
                update();
            }
            setCursor(Qt::IBeamCursor);
            return;
        } else {
            if (m_ocrModel.hoveredIndex() != -1) {
                m_ocrModel.setHoveredIndex(-1);
                update();
            }
            setCursor(Qt::SizeAllCursor);
            return;
        }
    }
}

void PinWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_isSelectingText) {
            m_isSelectingText = false;
            if (m_ocrModel.hasSelection()) {
                updateTextBubble();
            } else if (m_textBubble) {
                m_textBubble->hideBubble();
            }
            update();
            event->accept();
            return;
        }
        m_isDragging = false;
        event->accept();
    }
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int hitWord = findOcrWordAt(event->pos());
        if (hitWord != -1) {
            // Select entire line
            m_ocrModel.selectLineOf(hitWord);
            m_textSelectAnchorIndex = m_ocrModel.selectedIndices().empty() ? -1 : m_ocrModel.selectedIndices().front();
            updateTextBubble();
            update();
            event->accept();
            return;
        }

        // Toggle collapse/expand when double clicking outside text
        if (!m_isCollapsed) {
            m_preCollapseSize = size();
            resize(80, 60);
            m_isCollapsed = true;
        } else {
            resize(m_preCollapseSize);
            m_isCollapsed = false;
        }
        event->accept();
    }
}

void PinWindow::wheelEvent(QWheelEvent* event) {
    int delta = event->angleDelta().y();
    if (event->modifiers() & Qt::AltModifier) {
        qreal newOpacity = m_opacity + (delta > 0 ? 0.08 : -0.08);
        setOpacity(newOpacity);
    } else {
        qreal factor = delta > 0 ? 1.1 : 0.9;
        setZoom(m_scale * factor);
    }
    event->accept();
}

void PinWindow::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);

    if (m_ocrModel.hasSelection()) {
        menu.addAction("复制选中文字 (Ctrl+C)", [this]() {
            QGuiApplication::clipboard()->setText(getSelectedOcrText());
        });
        menu.addSeparator();
    }

    auto* ocrAction = menu.addAction(QIcon(":/icons/tool-recognize-text.svg"), "提取全部文字 (OCR)...", this, &PinWindow::openOcrDialog);
    ocrAction->setShortcut(QKeySequence(Qt::Key_T));

    auto* copyOcrAction = menu.addAction("复制所有文字 (Ctrl+Shift+C)", this, &PinWindow::copyAllOcrText);
    if (!m_ocrReady || m_ocrModel.fullText().isEmpty()) {
        copyOcrAction->setEnabled(false);
    }

    menu.addSeparator();
    menu.addAction("复制贴图图像", this, &PinWindow::copyToClipboard);
    menu.addAction("保存图像 (Ctrl+S)", this, &PinWindow::saveToFile);
    menu.addSeparator();

    auto* opacityMenu = menu.addMenu("不透明度");
    for (int op = 100; op >= 20; op -= 20) {
        opacityMenu->addAction(QString("%1%").arg(op), [this, op]() {
            setOpacity(op / 100.0);
        });
    }

    auto* zoomMenu = menu.addMenu("缩放比例");
    qreal zoomLevels[] = {0.5, 0.75, 1.0, 1.5, 2.0};
    for (qreal z : zoomLevels) {
        zoomMenu->addAction(QString("%1%").arg(static_cast<int>(z * 100)), [this, z]() {
            setZoom(z);
        });
    }

    menu.addSeparator();
    menu.addAction("关闭贴图 (Esc)", this, &QWidget::close);

    menu.exec(event->globalPos());
}

void PinWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape || event->matches(QKeySequence::Close)) {
        if (!m_ocrModel.hasSelection()) {
            close();
        } else {
            m_ocrModel.clearSelection();
            m_textSelectAnchorIndex = -1;
            updateTextBubble();
            update();
        }
    } else if (event->key() == Qt::Key_T) {
        openOcrDialog();
    } else if (event->matches(QKeySequence::Copy)) {
        if (m_ocrModel.hasSelection()) {
            QGuiApplication::clipboard()->setText(getSelectedOcrText());
        } else {
            copyToClipboard();
        }
    } else if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ShiftModifier) && (event->modifiers() & Qt::ControlModifier)) {
        copyAllOcrText();
    } else if (event->matches(QKeySequence::Save)) {
        saveToFile();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void PinWindow::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    if (m_textBubble && m_textBubble->isVisible() && m_ocrModel.hasSelection()) {
        updateTextBubble();
    }
}

void PinWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (qEnvironmentVariableIsSet("FOURYSHOT_OCR_DEBUG")) {
        qInfo() << "[Pin] frameGeometry" << frameGeometry() << "cursor" << QCursor::pos();
    }
    if (m_textBubble && m_textBubble->isVisible() && m_ocrModel.hasSelection()) {
        updateTextBubble();
    }
}

void PinWindow::hideEvent(QHideEvent* event) {
    if (m_textBubble) {
        m_textBubble->hideBubble();
    }
    QWidget::hideEvent(event);
}

void PinWindow::leaveEvent(QEvent* event) {
    if (m_ocrModel.hoveredIndex() != -1) {
        m_ocrModel.setHoveredIndex(-1);
        update();
    }
    QWidget::leaveEvent(event);
}

void PinWindow::copyToClipboard() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setPixmap(m_pixmap);
    clipboard->setImage(m_pixmap.toImage());
    // The app may quit once the last pin closes; hand the image to wl-copy's
    // daemon so the selection survives the exit (see clipboard_util.cpp).
    core::persistClipboardImage(m_pixmap.toImage());
}

void PinWindow::saveToFile() {
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        + QString("/4YShot_Pin_%1.png").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));
    QString filePath = QFileDialog::getSaveFileName(this, "保存贴图", defaultPath, "PNG 图像 (*.png);;JPEG 图像 (*.jpg)");
    if (!filePath.isEmpty()) {
        m_pixmap.save(filePath, nullptr, 100);
    }
}

} // namespace fouryshot::pin
