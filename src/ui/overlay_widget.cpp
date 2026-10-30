#include "overlay_widget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QFontMetrics>
#include <algorithm>
#include <unordered_set>
#include <limits>
#include "ocr_dialog.h"
#include <QtGlobal>
#include "../pin/pin_window.h"

static bool ocrOverlayDbg()
{
    static const bool on = qEnvironmentVariableIsSet("FOURYSHOT_OCR_DEBUG");
    return on;
}
#include "../core/app.h"
#include "../core/clipboard_util.h"

namespace fouryshot::ui {

OverlayWidget::OverlayWidget(
    const platform::ScreenTarget& target,
    const QRect& desktopGeometry,
    const QPixmap& fullDesktopPixmap,
    std::shared_ptr<QImage> fullDesktopImage,
    std::shared_ptr<SelectionModel> selectionModel,
    std::shared_ptr<annotation::AnnotationEngine> annotationEngine,
    QWidget* parent
) : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool),
    m_target(target),
    m_desktopGeometry(desktopGeometry),
    m_fullDesktopPixmap(fullDesktopPixmap),
    m_fullDesktopImage(fullDesktopImage),
    m_selectionModel(selectionModel),
    m_annotationEngine(annotationEngine) {

    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    if (m_target.screen) {
        setScreen(m_target.screen);
    }
    setGeometry(m_target.logicalGeometry);

    // Pre-slice screen background with exact DPR for 1:1 hardware blit
    if (!m_fullDesktopPixmap.isNull()) {
        QRect srcRect(
            std::round((m_target.logicalGeometry.x() - m_desktopGeometry.x()) * m_target.dpr),
            std::round((m_target.logicalGeometry.y() - m_desktopGeometry.y()) * m_target.dpr),
            std::round(m_target.logicalGeometry.width() * m_target.dpr),
            std::round(m_target.logicalGeometry.height() * m_target.dpr)
        );
        m_screenPixmap = m_fullDesktopPixmap.copy(srcRect);
        m_screenPixmap.setDevicePixelRatio(m_target.dpr);
    }

    // Magnifier
    m_magnifier = new Magnifier(this);
    m_magnifier->hide();

    // Toolbar
    m_toolbar = new Toolbar(this);
    m_toolbar->hide();

    // In-place Text Editor
    m_textEditor = new QLineEdit(this);
    m_textEditor->setStyleSheet("background-color: #24242a; color: #ffffff; border: 2px solid #1677ff; border-radius: 4px; padding: 4px 8px; font-size: 16px; font-weight: bold;");
    m_textEditor->setPlaceholderText("输入文本，按回车提交...");
    m_textEditor->hide();
    connect(m_textEditor, &QLineEdit::returnPressed, this, &OverlayWidget::onCommitText);

    // OCR Service & Toast Timer
    m_ocrService = std::make_unique<core::OcrService>(this);
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() {
        m_toastMessage.clear();
        update();
    });

    // Floating Text Selection Bubble
    m_textBubble = new TextSelectionBubble(this);
    connect(m_textBubble, &TextSelectionBubble::copyRequested, this, [this]() {
        showToast("已复制选中文本到剪贴板");
    });

    connect(m_selectionModel.get(), &SelectionModel::rectChanged, this, &OverlayWidget::onSelectionChanged);
    connect(m_selectionModel.get(), &SelectionModel::stateChanged, this, &OverlayWidget::onSelectionStateChanged);
    connect(m_annotationEngine.get(), &annotation::AnnotationEngine::stateChanged, this, [this]() {
        if (m_toolbar) {
            m_toolbar->setCanUndo(m_annotationEngine->canUndo());
            m_toolbar->setCanRedo(m_annotationEngine->canRedo());
        }
        update();
    });

    connect(m_toolbar, &Toolbar::toolSelected, this, &OverlayWidget::onToolSelected);
    connect(m_toolbar, &Toolbar::colorChanged, this, &OverlayWidget::onColorChanged);
    connect(m_toolbar, &Toolbar::strokeWidthChanged, this, &OverlayWidget::onStrokeWidthChanged);
    connect(m_toolbar, &Toolbar::textFontChanged, this, &OverlayWidget::onTextFontChanged);
    connect(m_toolbar, &Toolbar::filterParamsChanged, this, &OverlayWidget::onFilterParamsChanged);
    connect(m_toolbar, &Toolbar::stepParamsChanged, this, &OverlayWidget::onStepParamsChanged);
    connect(m_toolbar, &Toolbar::undoTriggered, this, &OverlayWidget::onUndo);
    connect(m_toolbar, &Toolbar::redoTriggered, this, &OverlayWidget::onRedo);
    connect(m_toolbar, &Toolbar::pinTriggered, this, &OverlayWidget::onPin);
    connect(m_toolbar, &Toolbar::longCaptureTriggered, this, &OverlayWidget::onLongCapture);
    connect(m_toolbar, &Toolbar::ocrTriggered, this, &OverlayWidget::onOcr);
    connect(m_toolbar, &Toolbar::saveTriggered, this, &OverlayWidget::onSave);
    connect(m_toolbar, &Toolbar::copyTriggered, this, &OverlayWidget::onCopy);
    connect(m_toolbar, &Toolbar::closeTriggered, this, &OverlayWidget::onClose);
    connect(m_toolbar, &Toolbar::sizeChanged, this, &OverlayWidget::updateToolbarPosition);
}

void OverlayWidget::showOverlay() {
    if (ocrOverlayDbg())
        qWarning() << "[OvShow] target" << m_target.logicalGeometry << "dpr" << m_target.dpr
                   << "desktop" << m_desktopGeometry << "pixmapSz" << m_fullDesktopPixmap.size();
    showFullScreen();
    raise();
    activateWindow();
}

void OverlayWidget::enterLongCaptureMode() {
    if (m_longCaptureMode) return;
    m_longCaptureMode = true;
    m_magnifier->hide();
    m_toolbar->hide();
    if (m_textBubble) m_textBubble->hideBubble();
    update();
}

void OverlayWidget::setLongCaptureInputRegion(
    const QRect& globalSelection,
    const QRect& localPanelRect
) {
    if (!m_longCaptureMode) return;

    const QRect localSelection = globalSelection.translated(-m_target.logicalGeometry.topLeft());
    QRegion inputRegion(localPanelRect);
    if (!localSelection.isEmpty()) {
        const QRect outer = localSelection.adjusted(-4, -4, 4, 4).intersected(rect());
        const QRect inner = localSelection.adjusted(4, 4, -4, -4).intersected(rect());
        inputRegion += QRegion(outer).subtracted(QRegion(inner));
    }
    if (inputRegion.isEmpty()) {
        // A null Wayland input region means "accept everywhere"; leave one
        // harmless pixel so other screens stay pass-through.
        inputRegion = QRegion(QRect(0, 0, 1, 1));
    }
    setMask(inputRegion);
}

void OverlayWidget::showToast(const QString& msg) {
    m_toastMessage = msg;
    m_toastTimer->start(1800);
    update();
}

void OverlayWidget::triggerOcrScan() {
    if (!m_selectionModel->hasSelection()) return;
    QRect globalSel = m_selectionModel->rect();
    if (globalSel.width() < 10 || globalSel.height() < 10) return;

    QRect physRect(
        std::round((globalSel.x() - m_desktopGeometry.x()) * m_target.dpr),
        std::round((globalSel.y() - m_desktopGeometry.y()) * m_target.dpr),
        std::round(globalSel.width() * m_target.dpr),
        std::round(globalSel.height() * m_target.dpr)
    );
    QPixmap crop = m_fullDesktopPixmap.copy(physRect);
    if (crop.isNull()) return;
    if (ocrOverlayDbg())
        qWarning() << "[OvScan] dpr" << m_target.dpr << "pixDpr" << m_fullDesktopPixmap.devicePixelRatio()
                   << "globalSel" << globalSel << "physRect" << physRect << "crop" << crop.size()
                   << "desktopGeom" << m_desktopGeometry << "targetGeom" << m_target.logicalGeometry;
    crop.save("/tmp/ov_crop.png");

    m_ocrModel.clearResult();
    if (m_textBubble) {
        m_textBubble->hideBubble();
    }

    m_ocrService->recognizeAsync(crop, m_target.dpr, [this](const core::OcrResult& res) {
        if (res.success) {
            m_ocrModel.setResult(res);
            if (ocrOverlayDbg()) {
                qWarning() << "[OvRes]" << m_target.logicalGeometry << "words" << m_ocrModel.wordCount() << "lines" << m_ocrModel.lines().size();
                const auto& lines = m_ocrModel.lines();
                for (size_t li = 0; li < lines.size() && li < 4; ++li) {
                    const auto& L = lines[li];
                    qWarning() << "[OvRes] line" << li << L.logicalRect << "wordIdx" << L.wordIndices.size()
                               << (L.wordIndices.empty() ? QString() : m_ocrModel.word(L.wordIndices.front()).text);
                }
            }
            update();
        }
    });
}

int OverlayWidget::findOcrWordAt(const QPoint& localPos) const {
    if (!m_ocrModel.hasWords() || !m_selectionModel->hasSelection()) return -1;
    QRect localSel = m_selectionModel->rect().translated(-m_target.logicalGeometry.topLeft());
    if (!localSel.contains(localPos)) return -1;
    return m_ocrModel.findWordAt(localPos - localSel.topLeft());
}

int OverlayWidget::findNearestOcrWordAt(const QPoint& localPos) const {
    if (!m_ocrModel.hasWords() || !m_selectionModel->hasSelection()) return -1;
    QRect localSel = m_selectionModel->rect().translated(-m_target.logicalGeometry.topLeft());
    return m_ocrModel.findNearestWordAt(localPos - localSel.topLeft());
}

void OverlayWidget::updateSelectedOcrRange(int anchorIdx, int currentIdx) {
    m_ocrModel.updateSelectedRange(anchorIdx, currentIdx);
}

QList<QRect> OverlayWidget::getContinuousHighlightRects() const {
    QList<QRect> rects;
    if (!m_ocrModel.hasSelection() || !m_selectionModel->hasSelection()) {
        return rects;
    }

    QRect localSel = m_selectionModel->rect().translated(-m_target.logicalGeometry.topLeft());
    rects = m_ocrModel.highlightRects();
    for (auto& r : rects) {
        r.translate(localSel.topLeft());
    }
    if (ocrOverlayDbg()) {
        for (const auto& r : rects)
            qWarning() << "[OvRibbon] rect" << r << "localSel" << localSel;
    }
    return rects;
}

QString OverlayWidget::getSelectedOcrText() const {
    return m_ocrModel.selectedText();
}

QRect OverlayWidget::getSelectedOcrVisualBoundingRect() const {
    if (!m_selectionModel->hasSelection()) return QRect();
    QRect united = m_ocrModel.selectedBoundingRect();
    if (!united.isNull()) {
        united.translate(m_selectionModel->rect().translated(-m_target.logicalGeometry.topLeft()).topLeft());
    }
    return united;
}

void OverlayWidget::updateTextBubble() {
    if (!m_ocrModel.hasSelection() || !m_selectionModel->hasSelection()) {
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

void OverlayWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_longCaptureMode) {
        // Drop the frozen initial screenshot from the translucent backing store.
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        if (m_selectionModel->hasSelection()) {
            // Draw the outline outside the captured rectangle so the live page stays clean.
            QRect localSel = m_selectionModel->rect().translated(-m_target.logicalGeometry.topLeft());
            painter.setPen(QPen(QColor("#1677ff"), 2, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(localSel.adjusted(-3, -3, 3, 3));
        }
        return;
    }

    // 1. Draw desktop background slice (1:1 hardware blit if pre-sliced)
    if (!m_screenPixmap.isNull()) {
        painter.drawPixmap(0, 0, m_screenPixmap);
    } else if (!m_fullDesktopPixmap.isNull()) {
        QRect srcRect(
            std::round((m_target.logicalGeometry.x() - m_desktopGeometry.x()) * m_target.dpr),
            std::round((m_target.logicalGeometry.y() - m_desktopGeometry.y()) * m_target.dpr),
            std::round(m_target.logicalGeometry.width() * m_target.dpr),
            std::round(m_target.logicalGeometry.height() * m_target.dpr)
        );
        painter.drawPixmap(rect(), m_fullDesktopPixmap, srcRect);
    }

    QRect globalSel = m_selectionModel->rect();
    QRect localSel = globalSel.translated(-m_target.logicalGeometry.topLeft());

    // 2. Dim mask outside selection
    QRegion dimRegion(rect());
    if (m_selectionModel->hasSelection()) {
        dimRegion = dimRegion.subtracted(QRegion(localSel));
    }
    painter.setClipRegion(dimRegion);
    painter.fillRect(rect(), QColor(0, 0, 0, 110));
    painter.setClipping(false);

    // 3. Render OCR highlights and annotations clipped to selection box
    if (m_selectionModel->hasSelection()) {
        painter.save();
        painter.setClipRect(localSel);

        // A. Continuous Selected OCR text highlight ribbons
        auto highlightRects = getContinuousHighlightRects();
        for (const auto& hr : highlightRects) {
            painter.setPen(QPen(QColor(22, 119, 255, 220), 1.2));
            painter.setBrush(QColor(22, 119, 255, 110));
            painter.drawRoundedRect(hr, 3, 3);
        }

        // B. Hovered OCR text box (if not already selected)
        if (m_ocrModel.hoveredIndex() >= 0 && m_ocrModel.hoveredIndex() < m_ocrModel.wordCount()) {
            if (!m_ocrModel.isSelected(m_ocrModel.hoveredIndex())) {
                const auto& item = m_ocrModel.word(m_ocrModel.hoveredIndex());
                QRect wordRect = item.logicalRect.translated(localSel.topLeft());
                painter.setPen(QPen(QColor(22, 119, 255, 180), 1, Qt::DashLine));
                painter.setBrush(QColor(22, 119, 255, 35));
                painter.drawRoundedRect(wordRect.adjusted(-1, -1, 1, 1), 3, 3);
            }
        }

        // C. Vector annotations
        painter.translate(-m_target.logicalGeometry.topLeft());
        m_annotationEngine->draw(painter);

        if (m_previewItem) {
            m_previewItem->draw(painter);
        }
        painter.restore();
    }

    // 4. Render selection box outline & handles
    if (m_selectionModel->hasSelection()) {
        painter.setPen(QPen(QColor("#1677ff"), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(localSel);

        // 8 Handles
        const HandlePosition positions[] = {
            HandlePosition::TopLeft, HandlePosition::Top, HandlePosition::TopRight,
            HandlePosition::Right, HandlePosition::BottomRight, HandlePosition::Bottom,
            HandlePosition::BottomLeft, HandlePosition::Left
        };
        for (auto pos : positions) {
            QRect hr = m_selectionModel->handleRect(pos, 8).translated(-m_target.logicalGeometry.topLeft());
            painter.setPen(QPen(QColor("#1677ff"), 2));
            painter.setBrush(Qt::white);
            painter.drawRoundedRect(hr, 2, 2);
        }

        // Geometry badge
        QString dimText = QString("%1 × %2").arg(globalSel.width()).arg(globalSel.height());
        QFont font = painter.font();
        font.setPixelSize(11);
        font.setBold(true);
        painter.setFont(font);

        int badgeW = 86;
        int badgeH = 22;
        int badgeX = localSel.left();
        int badgeY = localSel.top() - badgeH - 5;
        if (badgeY < 5) badgeY = localSel.top() + 5;

        QRect badgeRect(badgeX, badgeY, badgeW, badgeH);
        painter.setPen(QPen(QColor(255, 255, 255, 40), 1));
        painter.setBrush(QColor(20, 20, 24, 230));
        painter.drawRoundedRect(badgeRect, 4, 4);

        painter.setPen(Qt::white);
        painter.drawText(badgeRect, Qt::AlignCenter, dimText);
    }

    // 5. Toast feedback notification
    if (!m_toastMessage.isEmpty()) {
        painter.save();
        QFont f = painter.font();
        f.setPixelSize(13);
        f.setBold(true);
        painter.setFont(f);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(m_toastMessage) + 36;
        int th = 34;
        QRect toastRect((width() - tw) / 2, 24, tw, th);
        painter.setPen(QPen(QColor(22, 119, 255, 200), 1.5));
        painter.setBrush(QColor(20, 20, 26, 240));
        painter.drawRoundedRect(toastRect, 8, 8);
        painter.setPen(Qt::white);
        painter.drawText(toastRect, Qt::AlignCenter, m_toastMessage);
        painter.restore();
    }
}

bool OverlayWidget::isNearSelectionBorder(const QPoint& globalPos, int margin) const {
    if (!m_selectionModel->hasSelection()) return false;
    QRect r = m_selectionModel->rect();
    QRect outer = r.adjusted(-margin, -margin, margin, margin);
    QRect inner = r.adjusted(margin, margin, -margin, -margin);
    return outer.contains(globalPos) && !inner.contains(globalPos);
}

void OverlayWidget::mousePressEvent(QMouseEvent* event) {
    if (ocrOverlayDbg())
        qWarning() << "[OvPress] local" << event->pos() << "global" << event->globalPosition().toPoint()
                   << "btn" << event->button();
    if (m_longCaptureMode) {
        event->ignore();
        return;
    }
    if (m_textEditor && m_textEditor->isVisible()) {
        onCommitText();
    }

    if (event->button() == Qt::LeftButton) {
        QPoint globalPos = event->globalPosition().toPoint();
        auto tool = m_annotationEngine->currentTool();

        // 1. Resizing via 8 handles ALWAYS takes top priority (even when a drawing tool or subtoolbar is active)
        if (m_selectionModel->hasSelection()) {
            auto handle = m_selectionModel->hitTestHandles(globalPos, 14);
            if (handle != HandlePosition::None) {
                m_selectionModel->setActiveHandle(handle);
                m_selectionModel->setState(SelectionState::Resizing);
                m_dragStartPos = globalPos;
                m_preDragRect = m_selectionModel->rect();
                return;
            }

            // 2. Moving via selection border margin (6px) ALWAYS moves selection box
            if (isNearSelectionBorder(globalPos, 6)) {
                m_selectionModel->setState(SelectionState::Moving);
                m_dragStartPos = globalPos;
                m_preDragRect = m_selectionModel->rect();
                return;
            }
        }

        // 3. Pointer mode (tool == None)
        if (tool == annotation::ToolType::None) {
            // Check if user clicked an OCR text word
            int hitWord = findOcrWordAt(event->pos());
            if (ocrOverlayDbg())
                qWarning() << "[OvHit] pos" << event->pos() << "hit" << hitWord << "nWords" << m_ocrModel.wordCount();
            if (hitWord != -1) {
                m_isSelectingText = true;
                m_textSelectStart = event->pos();
                m_textSelectCurrent = event->pos();
                m_textSelectAnchorIndex = hitWord;
                m_ocrModel.selectSingle(hitWord);
                if (m_textBubble) {
                    m_textBubble->hideBubble();
                }
                update();
                return;
            }

            // Click outside any word: if text was selected, clear it
            if (m_ocrModel.hasSelection()) {
                m_ocrModel.clearSelection();
                m_textSelectAnchorIndex = -1;
                if (m_textBubble) {
                    m_textBubble->hideBubble();
                }
                update();
            }

            // Inside selection box -> Move
            if (m_selectionModel->hasSelection() && m_selectionModel->rect().contains(globalPos)) {
                m_selectionModel->setState(SelectionState::Moving);
                m_dragStartPos = globalPos;
                m_preDragRect = m_selectionModel->rect();
                return;
            }

            // Start new selection
            m_selectionModel->clear();
            m_selectionModel->setState(SelectionState::Selecting);
            m_dragStartPos = globalPos;
            m_selectionModel->setRect(QRect(globalPos, QSize(1, 1)));
            m_toolbar->hide();
        } else {
            // 4. Drawing tool active
            if (!m_selectionModel->hasSelection() || !m_selectionModel->rect().contains(globalPos)) {
                return; // Only draw within selection box
            }

            m_dragStartPos = globalPos;
            QColor col = m_annotationEngine->currentColor();
            int sw = m_annotationEngine->currentStrokeWidth();

            switch (tool) {
            case annotation::ToolType::Rectangle:
                m_previewItem = std::make_shared<annotation::RectItem>(QRect(globalPos, QSize(0, 0)), col, sw);
                break;
            case annotation::ToolType::Ellipse:
                m_previewItem = std::make_shared<annotation::EllipseItem>(QRect(globalPos, QSize(0, 0)), col, sw);
                break;
            case annotation::ToolType::Line:
                m_previewItem = std::make_shared<annotation::LineItem>(globalPos, globalPos, col, sw);
                break;
            case annotation::ToolType::Arrow:
                m_previewItem = std::make_shared<annotation::ArrowItem>(globalPos, globalPos, col, sw);
                break;
            case annotation::ToolType::Pen:
                m_previewItem = std::make_shared<annotation::PathItem>(QList<QPoint>{globalPos}, col, sw, false);
                break;
            case annotation::ToolType::Highlighter:
                m_previewItem = std::make_shared<annotation::PathItem>(QList<QPoint>{globalPos}, col, sw, true);
                break;
            case annotation::ToolType::StepNumber: {
                int num = m_annotationEngine->nextStepNumber();
                int sz = m_annotationEngine->currentStepSize();
                auto item = std::make_shared<annotation::StepNumberItem>(globalPos, num, col, sz);
                m_annotationEngine->addItem(item);
                m_previewItem = nullptr;
                update();
                break;
            }
            case annotation::ToolType::Text: {
                m_textEditorPos = globalPos;
                QPoint localPt = event->pos();
                m_textEditor->move(localPt.x(), localPt.y() - 15);
                m_textEditor->resize(200, 36);

                QFont f = m_textEditor->font();
                if (!m_annotationEngine->currentFontFamily().isEmpty()) {
                    f.setFamily(m_annotationEngine->currentFontFamily());
                }
                f.setPointSize(m_annotationEngine->currentFontSize());
                m_textEditor->setFont(f);
                m_textEditor->setStyleSheet(QString(
                    "background-color: #24242a; color: %1; border: 2px solid #1677ff; border-radius: 4px; padding: 4px 8px; font-size: %2pt; font-weight: bold;"
                ).arg(col.name()).arg(m_annotationEngine->currentFontSize()));

                m_textEditor->clear();
                m_textEditor->show();
                m_textEditor->setFocus();
                break;
            }
            case annotation::ToolType::Mosaic: {
                auto filterItem = std::make_shared<annotation::FilterItem>(
                    QRect(globalPos, QSize(0, 0)), false, m_annotationEngine->currentFilterIntensity()
                );
                m_previewItem = filterItem;
                break;
            }
            case annotation::ToolType::Blur: {
                auto filterItem = std::make_shared<annotation::FilterItem>(
                    QRect(globalPos, QSize(0, 0)), true, m_annotationEngine->currentFilterIntensity()
                );
                m_previewItem = filterItem;
                break;
            }
            default:
                break;
            }
        }
        update();
    } else if (event->button() == Qt::RightButton) {
        if (m_annotationEngine->currentTool() != annotation::ToolType::None) {
            m_annotationEngine->setTool(annotation::ToolType::None);
            m_toolbar->setSelectedTool(annotation::ToolType::None);
            m_previewItem = nullptr;
            update();
        } else if (m_ocrModel.hasSelection()) {
            m_ocrModel.clearSelection();
            m_textSelectAnchorIndex = -1;
            updateTextBubble();
            update();
        } else if (m_selectionModel->hasSelection()) {
            m_selectionModel->clear();
            m_toolbar->hide();
            if (m_textBubble) m_textBubble->hideBubble();
            m_ocrModel.clearResult();
            update();
        } else {
            onClose();
        }
    }
}

void OverlayWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_longCaptureMode) {
        event->ignore();
        return;
    }
    QPoint globalPos = event->globalPosition().toPoint();
    QPoint localPos = event->pos();
    auto state = m_selectionModel->state();

    // 1. Update magnifier during new selection or idle
    if (state == SelectionState::Idle || state == SelectionState::Selecting) {
        if (m_fullDesktopImage && !m_fullDesktopImage->isNull()) {
            m_magnifier->updateSample(globalPos, *m_fullDesktopImage, m_target.dpr);
        }
        positionMagnifier(localPos);
        m_magnifier->show();
    } else {
        m_magnifier->hide();
    }

    // 2. Active selection box operations (ALWAYS executed, regardless of active tool)
    if (state == SelectionState::Selecting) {
        m_selectionModel->setRect(QRect(m_dragStartPos, globalPos).normalized());
        return;
    } else if (state == SelectionState::Moving) {
        QPoint delta = globalPos - m_dragStartPos;
        m_selectionModel->setRect(m_preDragRect.translated(delta));
        return;
    } else if (state == SelectionState::Resizing) {
        QRect r = m_preDragRect;
        QPoint delta = globalPos - m_dragStartPos;
        auto handle = m_selectionModel->activeHandle();

        if (handle == HandlePosition::TopLeft) r.setTopLeft(r.topLeft() + delta);
        else if (handle == HandlePosition::Top) r.setTop(r.top() + delta.y());
        else if (handle == HandlePosition::TopRight) r.setTopRight(r.topRight() + delta);
        else if (handle == HandlePosition::Right) r.setRight(r.right() + delta.x());
        else if (handle == HandlePosition::BottomRight) r.setBottomRight(r.bottomRight() + delta);
        else if (handle == HandlePosition::Bottom) r.setBottom(r.bottom() + delta.y());
        else if (handle == HandlePosition::BottomLeft) r.setBottomLeft(r.bottomLeft() + delta);
        else if (handle == HandlePosition::Left) r.setLeft(r.left() + delta.x());

        m_selectionModel->setRect(r.normalized());
        return;
    }

    // 3. Text dragging selection in progress
    if (m_isSelectingText) {
        m_textSelectCurrent = localPos;
        int nearestWord = findNearestOcrWordAt(localPos);
        if (nearestWord != -1 && m_textSelectAnchorIndex != -1) {
            updateSelectedOcrRange(m_textSelectAnchorIndex, nearestWord);
            if (ocrOverlayDbg())
                qWarning() << "[OvDrag] anchor" << m_textSelectAnchorIndex << "cur" << nearestWord
                           << "nSel" << m_ocrModel.selectedIndices().size()
                           << "wordRect" << m_ocrModel.word(nearestWord).logicalRect;
        }
        update();
        return;
    }

    // 4. Drawing in progress (mouse button pressed while drawing tool active)
    auto tool = m_annotationEngine->currentTool();
    if (tool != annotation::ToolType::None && (event->buttons() & Qt::LeftButton)) {
        if (m_previewItem) {
            if (auto rectItem = std::dynamic_pointer_cast<annotation::RectItem>(m_previewItem)) {
                rectItem->rect = QRect(m_dragStartPos, globalPos).normalized();
            } else if (auto ellipseItem = std::dynamic_pointer_cast<annotation::EllipseItem>(m_previewItem)) {
                ellipseItem->rect = QRect(m_dragStartPos, globalPos).normalized();
            } else if (auto lineItem = std::dynamic_pointer_cast<annotation::LineItem>(m_previewItem)) {
                lineItem->end = globalPos;
            } else if (auto arrowItem = std::dynamic_pointer_cast<annotation::ArrowItem>(m_previewItem)) {
                arrowItem->end = globalPos;
            } else if (auto pathItem = std::dynamic_pointer_cast<annotation::PathItem>(m_previewItem)) {
                pathItem->addPoint(globalPos);
            } else if (auto filterItem = std::dynamic_pointer_cast<annotation::FilterItem>(m_previewItem)) {
                filterItem->rect = QRect(m_dragStartPos, globalPos).normalized();
                if (m_fullDesktopImage) {
                    filterItem->applyFilter(*m_fullDesktopImage, m_target.dpr);
                }
            }
            update();
        }
        return;
    }

    // 5. Dynamic Hover cursor updates (when mouse is not pressed)
    if (m_selectionModel->hasSelection()) {
        // Handles check (takes highest priority for cursor)
        auto handle = m_selectionModel->hitTestHandles(globalPos, 14);
        if (handle != HandlePosition::None) {
            switch (handle) {
            case HandlePosition::TopLeft:
            case HandlePosition::BottomRight:
                setCursor(Qt::SizeFDiagCursor);
                break;
            case HandlePosition::TopRight:
            case HandlePosition::BottomLeft:
                setCursor(Qt::SizeBDiagCursor);
                break;
            case HandlePosition::Top:
            case HandlePosition::Bottom:
                setCursor(Qt::SizeVerCursor);
                break;
            case HandlePosition::Left:
            case HandlePosition::Right:
                setCursor(Qt::SizeHorCursor);
                break;
            default:
                break;
            }
            return;
        }

        // 6px border check -> SizeAllCursor (move selection box)
        if (isNearSelectionBorder(globalPos, 6)) {
            setCursor(Qt::SizeAllCursor);
            return;
        }

        // Inside selection box
        if (m_selectionModel->rect().contains(globalPos)) {
            if (tool == annotation::ToolType::None) {
                int hitWord = findOcrWordAt(localPos);
                if (ocrOverlayDbg()) {
                    static QPoint lastLoggedHoverPos{-9999, -9999};
                    if (localPos != lastLoggedHoverPos) {
                        lastLoggedHoverPos = localPos;
                        qWarning() << "[OvHover] pos" << localPos << "hit" << hitWord << "nWords" << m_ocrModel.wordCount();
                    }
                }
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
            } else {
                setCursor(Qt::CrossCursor);
                return;
            }
        }
    }

    // Outside selection box
    setCursor(Qt::CrossCursor);
}

void OverlayWidget::wheelEvent(QWheelEvent* event) {
    if (m_longCaptureMode) {
        event->ignore();
        return;
    }
    if (m_selectionModel->state() != SelectionState::Idle || m_selectionModel->hasSelection()) {
        QWidget::wheelEvent(event);
        return;
    }

    int delta = event->angleDelta().y();
    if (delta == 0) {
        delta = event->pixelDelta().y() * 8;
    }
    m_magnifierWheelRemainder += static_cast<qreal>(delta) / 120.0;
    int steps = static_cast<int>(m_magnifierWheelRemainder);
    if (steps != 0) {
        m_magnifierWheelRemainder -= steps;
        m_magnifier->adjustZoom(steps);
    }

    QPoint localPos = event->position().toPoint();
    if (m_fullDesktopImage && !m_fullDesktopImage->isNull()) {
        m_magnifier->updateSample(event->globalPosition().toPoint(), *m_fullDesktopImage, m_target.dpr);
    }
    positionMagnifier(localPos);
    m_magnifier->show();
    event->accept();
}

void OverlayWidget::positionMagnifier(const QPoint& localPos) {
    int magX = localPos.x() + 20;
    int magY = localPos.y() + 20;
    if (magX + m_magnifier->width() > width()) {
        magX = localPos.x() - m_magnifier->width() - 20;
    }
    if (magY + m_magnifier->height() > height()) {
        magY = localPos.y() - m_magnifier->height() - 20;
    }
    magX = std::clamp(magX, 0, std::max(0, width() - m_magnifier->width()));
    magY = std::clamp(magY, 0, std::max(0, height() - m_magnifier->height()));
    m_magnifier->move(magX, magY);
}

void OverlayWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (ocrOverlayDbg())
        qWarning() << "[OvRelease] local" << event->pos() << "global" << event->globalPosition().toPoint()
                   << "state" << static_cast<int>(m_selectionModel->state())
                   << "hasSel" << m_selectionModel->hasSelection();
    if (m_longCaptureMode) {
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (m_isSelectingText) {
            m_isSelectingText = false;
            updateTextBubble();
            update();
            return;
        }

        auto state = m_selectionModel->state();
        if (state == SelectionState::Selecting) {
            if (m_selectionModel->hasSelection()) {
                m_selectionModel->setState(SelectionState::Selected);
            } else {
                m_selectionModel->clear();
            }
        } else if (state == SelectionState::Moving || state == SelectionState::Resizing) {
            m_selectionModel->setState(SelectionState::Selected);
            updateToolbarPosition();
            triggerOcrScan();
        }

        if (m_previewItem) {
            m_annotationEngine->addItem(m_previewItem);
            m_previewItem = nullptr;
        }

        updateToolbarPosition();
        update();
    }
}

void OverlayWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (m_longCaptureMode) {
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton && m_annotationEngine->currentTool() == annotation::ToolType::None) {
        int hitWord = findOcrWordAt(event->pos());
        if (hitWord != -1) {
            // Select whole line of text on double click
            m_ocrModel.selectLineOf(hitWord);
            m_textSelectAnchorIndex = m_ocrModel.selectedIndices().empty() ? -1 : m_ocrModel.selectedIndices().front();
            updateTextBubble();
            update();
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void OverlayWidget::onCommitText() {
    if (m_textEditor && m_textEditor->isVisible()) {
        QString txt = m_textEditor->text().trimmed();
        if (!txt.isEmpty()) {
            QColor col = m_annotationEngine->currentColor();
            int sz = m_annotationEngine->currentFontSize();
            QString fam = m_annotationEngine->currentFontFamily();
            auto item = std::make_shared<annotation::TextItem>(m_textEditorPos, txt, col, sz, fam);
            m_annotationEngine->addItem(item);
        }
        m_textEditor->hide();
        update();
    }
}

void OverlayWidget::updateCursorIcon(const QPoint& pt) {
    if (!m_selectionModel->hasSelection()) {
        setCursor(Qt::CrossCursor);
        return;
    }

    auto handle = m_selectionModel->hitTestHandles(pt, 8);
    switch (handle) {
    case HandlePosition::TopLeft:
    case HandlePosition::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case HandlePosition::TopRight:
    case HandlePosition::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case HandlePosition::Top:
    case HandlePosition::Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case HandlePosition::Left:
    case HandlePosition::Right:
        setCursor(Qt::SizeHorCursor);
        break;
    default:
        if (m_selectionModel->rect().contains(pt)) {
            setCursor(Qt::SizeAllCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
        break;
    }
}

void OverlayWidget::updateToolbarPosition() {
    if (!m_selectionModel->hasSelection()) {
        m_toolbar->hide();
        return;
    }

    QRect globalSel = m_selectionModel->rect();
    QRect localSel = globalSel.translated(-m_target.logicalGeometry.topLeft());

    if (!rect().intersects(localSel)) {
        m_toolbar->hide();
        return;
    }

    m_toolbar->adjustSize();
    QSize tbSize = m_toolbar->sizeHint();
    m_toolbar->resize(tbSize);

    int tbX = localSel.right() - tbSize.width();
    int tbY = localSel.bottom() + 8;

    if (tbY + tbSize.height() > height() - 8) {
        tbY = localSel.top() - tbSize.height() - 8;
        if (tbY < 8) {
            tbY = localSel.bottom() - tbSize.height() - 8;
        }
    }
    tbX = std::clamp(tbX, 8, std::max(8, width() - tbSize.width() - 8));

    m_toolbar->move(tbX, tbY);
    m_toolbar->show();
    m_toolbar->raise();
}

void OverlayWidget::onSelectionChanged(const QRect& rect) {
    Q_UNUSED(rect);
    update();
}

void OverlayWidget::onSelectionStateChanged(SelectionState state) {
    if (state == SelectionState::Selected) {
        updateToolbarPosition();
        triggerOcrScan();
    } else {
        m_toolbar->hide();
        if (m_textBubble) m_textBubble->hideBubble();
        m_ocrService->cancel();
        m_ocrModel.clearResult();
    }
    update();
}

void OverlayWidget::onToolSelected(annotation::ToolType tool) {
    m_annotationEngine->setTool(tool);
    if (tool != annotation::ToolType::None) {
        setCursor(Qt::CrossCursor);
        m_ocrModel.clearSelection();
        m_ocrModel.setHoveredIndex(-1);
        if (m_textBubble) m_textBubble->hideBubble();
    } else {
        setCursor(Qt::ArrowCursor);
    }
    update();
}

void OverlayWidget::onColorChanged(const QColor& color) {
    m_annotationEngine->setColor(color);
    if (m_textEditor && m_textEditor->isVisible()) {
        m_textEditor->setStyleSheet(QString(
            "background-color: #24242a; color: %1; border: 2px solid #1677ff; border-radius: 4px; padding: 4px 8px; font-size: %2pt; font-weight: bold;"
        ).arg(color.name()).arg(m_annotationEngine->currentFontSize()));
    }
}

void OverlayWidget::onStrokeWidthChanged(int width) {
    m_annotationEngine->setStrokeWidth(width);
}

void OverlayWidget::onTextFontChanged(const QString& family, int size) {
    m_annotationEngine->setFontFamily(family);
    m_annotationEngine->setFontSize(size);
    if (m_textEditor && m_textEditor->isVisible()) {
        QFont f = m_textEditor->font();
        if (!family.isEmpty()) f.setFamily(family);
        f.setPointSize(size);
        m_textEditor->setFont(f);
        m_textEditor->setStyleSheet(QString(
            "background-color: #24242a; color: %1; border: 2px solid #1677ff; border-radius: 4px; padding: 4px 8px; font-size: %2pt; font-weight: bold;"
        ).arg(m_annotationEngine->currentColor().name()).arg(size));
        m_textEditor->adjustSize();
    }
}

void OverlayWidget::onFilterParamsChanged(bool isBlur, int intensity) {
    m_annotationEngine->setIsBlur(isBlur);
    m_annotationEngine->setFilterIntensity(intensity);
}

void OverlayWidget::onStepParamsChanged(int startNumber, int size) {
    m_annotationEngine->setStepStartNumber(startNumber);
    m_annotationEngine->setStepSize(size);
}

void OverlayWidget::onUndo() {
    m_annotationEngine->undo();
}

void OverlayWidget::onRedo() {
    m_annotationEngine->redo();
}

QPixmap OverlayWidget::renderSelectedAreaWithAnnotations() const {
    QRect globalSel = m_selectionModel->rect();
    if (globalSel.isEmpty()) return QPixmap();

    QRect physRect(
        std::round((globalSel.x() - m_desktopGeometry.x()) * m_target.dpr),
        std::round((globalSel.y() - m_desktopGeometry.y()) * m_target.dpr),
        std::round(globalSel.width() * m_target.dpr),
        std::round(globalSel.height() * m_target.dpr)
    );

    QPixmap crop = m_fullDesktopPixmap.copy(physRect);
    crop.setDevicePixelRatio(m_target.dpr);

    QPainter painter(&crop);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.translate(-globalSel.topLeft());
    m_annotationEngine->draw(painter);

    return crop;
}

void OverlayWidget::onCopy() {
    QPixmap result = renderSelectedAreaWithAnnotations();
    if (!result.isNull()) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setPixmap(result);
        clipboard->setImage(result.toImage());
        // This process quits right after copying; hand the image to wl-copy's
        // daemon so the selection survives the exit (see clipboard_util.cpp).
        core::persistClipboardImage(result.toImage());
    }
    emit captureCompleted();
}

void OverlayWidget::onPin() {
    QPixmap result = renderSelectedAreaWithAnnotations();
    if (!result.isNull()) {
        QRect globalSel = m_selectionModel->rect();
        if (core::App::instance()) {
            core::App::instance()->createPin(result, globalSel.topLeft());
        } else {
            new pin::PinWindow(result, globalSel.topLeft());
        }
    }
    emit captureCompleted();
}

void OverlayWidget::onLongCapture() {
    if (!m_selectionModel->hasSelection() || !m_fullDesktopImage || m_fullDesktopImage->isNull()) {
        return;
    }

    QRect globalRegion = m_selectionModel->rect();
    QRect physicalRegion(
        std::round((globalRegion.x() - m_desktopGeometry.x()) * m_target.dpr),
        std::round((globalRegion.y() - m_desktopGeometry.y()) * m_target.dpr),
        std::round(globalRegion.width() * m_target.dpr),
        std::round(globalRegion.height() * m_target.dpr)
    );
    QImage firstSegment = m_fullDesktopImage->copy(physicalRegion);
    if (firstSegment.isNull()) {
        showToast("无法读取当前选区，请重新选择区域");
        return;
    }

    emit longCaptureRequested(firstSegment, globalRegion, m_target.dpr);
}

void OverlayWidget::onSave() {
    QPixmap result = renderSelectedAreaWithAnnotations();
    if (result.isNull()) return;

    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        + QString("/4YShot_%1.png").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));

    QString filePath = QFileDialog::getSaveFileName(
        nullptr, "保存截图", defaultPath, "PNG 图像 (*.png);;JPEG 图像 (*.jpg);;WEBP 图像 (*.webp)"
    );

    if (!filePath.isEmpty()) {
        result.save(filePath, nullptr, 100);
        emit captureCompleted();
    }
}

void OverlayWidget::onOcr() {
    QPixmap result = renderSelectedAreaWithAnnotations();
    if (result.isNull()) return;

    auto* dlg = new OcrDialog(result, nullptr);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void OverlayWidget::onClose() {
    if (m_textBubble) m_textBubble->hideBubble();
    emit captureCancelled();
}

void OverlayWidget::keyPressEvent(QKeyEvent* event) {
    if (m_longCaptureMode) {
        if (event->key() == Qt::Key_Escape) {
            onClose();
        } else {
            event->ignore();
        }
        return;
    }
    if (m_textEditor && m_textEditor->isVisible()) {
        if (event->key() == Qt::Key_Escape) {
            m_textEditor->hide();
            return;
        }
    }

    if (event->key() == Qt::Key_Escape) {
        onClose();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_selectionModel->hasSelection()) {
            onCopy();
        }
    } else if (event->matches(QKeySequence::Copy)) {
        if (m_ocrModel.hasSelection()) {
            QString textToCopy = getSelectedOcrText();
            QGuiApplication::clipboard()->setText(textToCopy);
            QString previewSnippet = textToCopy.left(18);
            if (textToCopy.length() > 18) previewSnippet += "...";
            showToast(QString("已复制文本：%1").arg(previewSnippet));
            return;
        } else if (m_selectionModel->hasSelection()) {
            onCopy();
        }
    } else if (event->matches(QKeySequence::Save)) {
        if (m_selectionModel->hasSelection()) {
            onSave();
        }
    } else if (event->matches(QKeySequence::Undo)) {
        onUndo();
    } else if (event->matches(QKeySequence::Redo)) {
        onRedo();
    } else if (event->key() == Qt::Key_C && !(event->modifiers() & Qt::ControlModifier)) {
        if (m_magnifier) {
            QGuiApplication::clipboard()->setText(m_magnifier->hexColor());
        }
    } else if (event->key() == Qt::Key_F3) {
        onPin();
    } else if (event->key() == Qt::Key_R) {
        m_toolbar->setSelectedTool(annotation::ToolType::Rectangle);
    } else if (event->key() == Qt::Key_E) {
        m_toolbar->setSelectedTool(annotation::ToolType::Ellipse);
    } else if (event->key() == Qt::Key_A) {
        m_toolbar->setSelectedTool(annotation::ToolType::Arrow);
    } else if (event->key() == Qt::Key_L) {
        m_toolbar->setSelectedTool(annotation::ToolType::Line);
    } else if (event->key() == Qt::Key_P) {
        m_toolbar->setSelectedTool(annotation::ToolType::Pen);
    } else if (event->key() == Qt::Key_H) {
        m_toolbar->setSelectedTool(annotation::ToolType::Highlighter);
    } else if (event->key() == Qt::Key_N) {
        m_toolbar->setSelectedTool(annotation::ToolType::StepNumber);
    } else if (event->key() == Qt::Key_T) {
        m_toolbar->setSelectedTool(annotation::ToolType::Text);
    } else if (event->key() == Qt::Key_M) {
        m_toolbar->setSelectedTool(annotation::ToolType::Mosaic);
    } else {
        QWidget::keyPressEvent(event);
    }
}

} // namespace fouryshot::ui
