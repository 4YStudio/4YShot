#include "toolbar.h"
#include <QIcon>
#include <QFrame>
#include <QPainter>
#include <QColorDialog>
#include <QMouseEvent>
#include <algorithm>
#include "icon_helper.h"

namespace fouryshot::ui {

PreviewBox::PreviewBox(QWidget* parent) : QWidget(parent) {
    setFixedSize(54, 26);
    setAttribute(Qt::WA_TranslucentBackground);
}

void PreviewBox::setTool(annotation::ToolType tool) {
    m_tool = tool;
    update();
}

void PreviewBox::setColor(const QColor& color) {
    m_color = color;
    update();
}

void PreviewBox::setStrokeWidth(int width) {
    m_strokeWidth = width;
    update();
}

void PreviewBox::setFontFamily(const QString& family) {
    m_fontFamily = family;
    update();
}

void PreviewBox::setFontSize(int size) {
    m_fontSize = size;
    update();
}

void PreviewBox::setFilter(bool isBlur, int intensity) {
    m_isBlur = isBlur;
    m_filterIntensity = intensity;
    update();
}

void PreviewBox::setStep(int startNumber, int size) {
    m_stepStartNumber = startNumber;
    m_stepSize = size;
    update();
}

void PreviewBox::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    QRect r = rect().adjusted(1, 1, -1, -1);
    p.setPen(QPen(QColor(255, 255, 255, 45), 1));
    p.setBrush(QColor(18, 18, 22, 230));
    p.drawRoundedRect(r, 4, 4);

    if (m_tool == annotation::ToolType::Mosaic) {
        int blockSize = std::clamp(m_filterIntensity / 3, 3, 7);
        int startX = r.center().x() - static_cast<int>(blockSize * 1.5);
        int startY = r.center().y() - static_cast<int>(blockSize * 1.5);
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                QColor c = ((i + j) % 2 == 0) ? QColor(80, 80, 95) : QColor(140, 140, 160);
                p.fillRect(QRect(startX + i * blockSize, startY + j * blockSize, blockSize, blockSize), c);
            }
        }
    } else if (m_tool == annotation::ToolType::Blur) {
        QRadialGradient grad(r.center(), 10);
        grad.setColorAt(0.0, QColor(255, 255, 255, 220));
        grad.setColorAt(0.5, QColor(180, 180, 205, 120));
        grad.setColorAt(1.0, Qt::transparent);
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawEllipse(r.center(), 10, 10);
    } else if (m_tool == annotation::ToolType::Text) {
        QFont f = font();
        if (!m_fontFamily.isEmpty()) {
            f.setFamily(m_fontFamily);
        }
        f.setBold(true);
        f.setPixelSize(13);
        p.setFont(f);
        p.setPen(m_color);
        p.drawText(r, Qt::AlignCenter, "Aa");
    } else if (m_tool == annotation::ToolType::StepNumber) {
        int badgeRadius = 9;
        p.setPen(Qt::NoPen);
        p.setBrush(m_color);
        p.drawEllipse(r.center(), badgeRadius, badgeRadius);
        p.setPen(Qt::white);
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(10);
        p.setFont(f);
        p.drawText(QRect(r.center().x() - badgeRadius, r.center().y() - badgeRadius, badgeRadius * 2, badgeRadius * 2),
                   Qt::AlignCenter, QString::number(m_stepStartNumber));
    } else if (m_tool == annotation::ToolType::Highlighter) {
        p.setPen(Qt::NoPen);
        QColor hlColor = m_color;
        hlColor.setAlpha(110);
        p.setBrush(hlColor);
        int h = std::clamp(m_strokeWidth, 4, 16);
        p.drawRoundedRect(QRect(r.left() + 6, r.center().y() - h / 2, r.width() - 12, h), 2, 2);
    } else {
        int w = std::clamp(m_strokeWidth, 1, 16);
        p.setPen(QPen(m_color, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(r.left() + 8, r.center().y(), r.right() - 8, r.center().y());
    }
}

Toolbar::Toolbar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(4);
    m_rootLayout->setSizeConstraint(QLayout::SetFixedSize);

    createMainToolbar();
    createSubToolbar();

    m_subBar->hide();
    adjustSize();
}

void Toolbar::mousePressEvent(QMouseEvent* event) {
    event->accept();
}

void Toolbar::mouseMoveEvent(QMouseEvent* event) {
    event->accept();
}

void Toolbar::mouseReleaseEvent(QMouseEvent* event) {
    event->accept();
}

QToolButton* Toolbar::createToolButton(const QString& iconPath, const QString& tooltip, int id, bool checkable) {
    auto* btn = new QToolButton(m_mainBar);
    btn->setIcon(IconHelper::createTintedIcon(iconPath));
    btn->setIconSize(QSize(18, 18));
    btn->setToolTip(tooltip);
    btn->setCheckable(checkable);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setAutoRaise(true);

    if (id != -1 && checkable && m_toolGroup) {
        m_toolGroup->addButton(btn, id);
    }
    return btn;
}

void Toolbar::createMainToolbar() {
    m_mainBar = new QWidget(this);
    m_mainBar->setObjectName("toolbarWidget");
    m_mainLayout = new QHBoxLayout(m_mainBar);
    m_mainLayout->setContentsMargins(6, 4, 6, 4);
    m_mainLayout->setSpacing(3);

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(false);

    connect(m_toolGroup, &QButtonGroup::idClicked, this, &Toolbar::onToolButtonClicked);

    // Drawing Tool Buttons
    m_mainLayout->addWidget(createToolButton(":/icons/tool-rectangle.svg", "矩形 (R)", static_cast<int>(annotation::ToolType::Rectangle)));
    m_mainLayout->addWidget(createToolButton(":/icons/shape-ellipse.svg", "椭圆 (E)", static_cast<int>(annotation::ToolType::Ellipse)));
    m_mainLayout->addWidget(createToolButton(":/icons/tool-arrow.svg", "箭头 (A)", static_cast<int>(annotation::ToolType::Arrow)));
    m_mainLayout->addWidget(createToolButton(":/icons/tool-line.svg", "直线 (L)", static_cast<int>(annotation::ToolType::Line)));
    m_mainLayout->addWidget(createToolButton(":/icons/tool-free-draw.svg", "画笔 (P)", static_cast<int>(annotation::ToolType::Pen)));
    m_mainLayout->addWidget(createToolButton(":/icons/tool-highlight.svg", "荧光笔 (H)", static_cast<int>(annotation::ToolType::Highlighter)));
    m_mainLayout->addWidget(createToolButton(":/icons/tool-serial-number.svg", "序号标注 (N)", static_cast<int>(annotation::ToolType::StepNumber)));
    m_mainLayout->addWidget(createToolButton(":/icons/tool-text.svg", "文本 (T)", static_cast<int>(annotation::ToolType::Text)));
    m_mainLayout->addWidget(createToolButton(":/icons/tool-filter.svg", "马赛克/模糊 (M)", static_cast<int>(annotation::ToolType::Mosaic)));

    // Separator
    QFrame* sep1 = new QFrame(m_mainBar);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFrameShadow(QFrame::Sunken);
    sep1->setStyleSheet("color: rgba(255, 255, 255, 0.2); margin: 3px 2px;");
    m_mainLayout->addWidget(sep1);

    // Undo / Redo
    m_undoBtn = createToolButton(":/icons/undo.svg", "撤销 (Ctrl+Z)", -1, false);
    m_redoBtn = createToolButton(":/icons/redo.svg", "重做 (Ctrl+Y)", -1, false);
    m_undoBtn->setEnabled(false);
    m_redoBtn->setEnabled(false);
    connect(m_undoBtn, &QToolButton::clicked, this, &Toolbar::undoTriggered);
    connect(m_redoBtn, &QToolButton::clicked, this, &Toolbar::redoTriggered);
    m_mainLayout->addWidget(m_undoBtn);
    m_mainLayout->addWidget(m_redoBtn);

    // Separator
    QFrame* sep2 = new QFrame(m_mainBar);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFrameShadow(QFrame::Sunken);
    sep2->setStyleSheet("color: rgba(255, 255, 255, 0.2); margin: 3px 2px;");
    m_mainLayout->addWidget(sep2);

    // Action buttons
    auto* pinBtn = createToolButton(":/icons/pushpin.svg", "贴图到屏幕 (F3)", -1, false);
    auto* longCaptureBtn = createToolButton(":/icons/image-stitching.svg", "长截图（滚动拼接）", -1, false);
    auto* ocrBtn = createToolButton(":/icons/tool-recognize-text.svg", "离线文字识别", -1, false);
    auto* saveBtn = createToolButton(":/icons/save.svg", "保存到文件 (Ctrl+S)", -1, false);
    auto* copyBtn = createToolButton(":/icons/check.svg", "完成并复制到剪贴板 (Enter)", -1, false);
    copyBtn->setObjectName("confirmButton");
    auto* closeBtn = createToolButton(":/icons/close.svg", "取消截图 (Esc)", -1, false);
    closeBtn->setObjectName("closeButton");

    connect(pinBtn, &QToolButton::clicked, this, &Toolbar::pinTriggered);
    connect(longCaptureBtn, &QToolButton::clicked, this, &Toolbar::longCaptureTriggered);
    connect(ocrBtn, &QToolButton::clicked, this, &Toolbar::ocrTriggered);
    connect(saveBtn, &QToolButton::clicked, this, &Toolbar::saveTriggered);
    connect(copyBtn, &QToolButton::clicked, this, &Toolbar::copyTriggered);
    connect(closeBtn, &QToolButton::clicked, this, &Toolbar::closeTriggered);

    m_mainLayout->addWidget(pinBtn);
    m_mainLayout->addWidget(longCaptureBtn);
    m_mainLayout->addWidget(ocrBtn);
    m_mainLayout->addWidget(saveBtn);
    m_mainLayout->addWidget(copyBtn);
    m_mainLayout->addWidget(closeBtn);

    m_rootLayout->addWidget(m_mainBar);
}

void Toolbar::createSubToolbar() {
    m_subBar = new QWidget(this);
    m_subBar->setObjectName("subToolbarWidget");
    m_subLayout = new QHBoxLayout(m_subBar);
    m_subLayout->setContentsMargins(6, 4, 6, 4);
    m_subLayout->setSpacing(6);

    // 1. Live Preview Box on the left
    m_previewBox = new PreviewBox(m_subBar);
    m_previewBox->setColor(m_currentColor);
    m_previewBox->setStrokeWidth(m_currentWidth);
    m_subLayout->addWidget(m_previewBox);

    // Separator
    QFrame* sepPreview = new QFrame(m_subBar);
    sepPreview->setFrameShape(QFrame::VLine);
    sepPreview->setStyleSheet("color: rgba(255, 255, 255, 0.2); margin: 2px 1px;");
    m_subLayout->addWidget(sepPreview);

    // 2. Stacked widget for tool-specific parameter inputs
    m_paramStack = new QStackedWidget(m_subBar);

    // --- Page 0: Stroke Width (Rect, Ellipse, Line, Arrow, Pen, Highlighter) ---
    auto* strokePage = new QWidget(m_paramStack);
    auto* strokeLayout = new QHBoxLayout(strokePage);
    strokeLayout->setContentsMargins(0, 0, 0, 0);
    strokeLayout->setSpacing(6);

    auto* strokeLbl = new QLabel("粗细:", strokePage);
    strokeLbl->setStyleSheet("color: #bfbfbf; font-size: 12px;");
    strokeLayout->addWidget(strokeLbl);

    m_strokeSpin = new QSpinBox(strokePage);
    m_strokeSpin->setRange(1, 50);
    m_strokeSpin->setValue(m_currentWidth);
    m_strokeSpin->setSuffix(" px");
    m_strokeSpin->setFixedWidth(78);
    m_strokeSpin->setCursor(Qt::PointingHandCursor);
    connect(m_strokeSpin, &QSpinBox::valueChanged, this, [this](int val) {
        m_currentWidth = val;
        m_previewBox->setStrokeWidth(val);
        emit strokeWidthChanged(val);
    });
    strokeLayout->addWidget(m_strokeSpin);
    m_paramStack->addWidget(strokePage);

    // --- Page 1: Mosaic / Blur Options & Intensity ---
    auto* filterPage = new QWidget(m_paramStack);
    auto* filterLayout = new QHBoxLayout(filterPage);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(6);

    auto* filterTypeGroup = new QButtonGroup(filterPage);
    m_mosaicBtn = new QToolButton(filterPage);
    m_mosaicBtn->setText("马赛克");
    m_mosaicBtn->setCheckable(true);
    m_mosaicBtn->setChecked(true);
    m_mosaicBtn->setCursor(Qt::PointingHandCursor);
    m_mosaicBtn->setStyleSheet("padding: 2px 8px; font-size: 11px;");

    m_blurBtn = new QToolButton(filterPage);
    m_blurBtn->setText("高斯模糊");
    m_blurBtn->setCheckable(true);
    m_blurBtn->setCursor(Qt::PointingHandCursor);
    m_blurBtn->setStyleSheet("padding: 2px 8px; font-size: 11px;");

    filterTypeGroup->addButton(m_mosaicBtn, 0);
    filterTypeGroup->addButton(m_blurBtn, 1);
    filterLayout->addWidget(m_mosaicBtn);
    filterLayout->addWidget(m_blurBtn);

    connect(filterTypeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_isBlur = (id == 1);
        m_activeTool = m_isBlur ? annotation::ToolType::Blur : annotation::ToolType::Mosaic;
        m_previewBox->setTool(m_activeTool);
        m_previewBox->setFilter(m_isBlur, m_filterIntensity);
        emit filterParamsChanged(m_isBlur, m_filterIntensity);
        emit toolSelected(m_activeTool);
    });

    auto* filterSep = new QFrame(filterPage);
    filterSep->setFrameShape(QFrame::VLine);
    filterSep->setStyleSheet("color: rgba(255, 255, 255, 0.2); margin: 2px 1px;");
    filterLayout->addWidget(filterSep);

    auto* intensityLbl = new QLabel("强度:", filterPage);
    intensityLbl->setStyleSheet("color: #bfbfbf; font-size: 12px;");
    filterLayout->addWidget(intensityLbl);

    m_filterSpin = new QSpinBox(filterPage);
    m_filterSpin->setRange(4, 60);
    m_filterSpin->setValue(m_filterIntensity);
    m_filterSpin->setSingleStep(2);
    m_filterSpin->setSuffix(" px");
    m_filterSpin->setFixedWidth(78);
    m_filterSpin->setCursor(Qt::PointingHandCursor);
    connect(m_filterSpin, &QSpinBox::valueChanged, this, [this](int val) {
        m_filterIntensity = val;
        m_previewBox->setFilter(m_isBlur, val);
        emit filterParamsChanged(m_isBlur, val);
    });
    filterLayout->addWidget(m_filterSpin);
    m_paramStack->addWidget(filterPage);

    // --- Page 2: Text Tool (Font family & Font size) ---
    auto* textPage = new QWidget(m_paramStack);
    auto* textLayout = new QHBoxLayout(textPage);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(6);

    auto* fontLbl = new QLabel("字体:", textPage);
    fontLbl->setStyleSheet("color: #bfbfbf; font-size: 12px;");
    textLayout->addWidget(fontLbl);

    m_fontCombo = new QFontComboBox(textPage);
    m_fontCombo->setFixedWidth(120);
    m_fontCombo->setCursor(Qt::PointingHandCursor);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont& font) {
        m_fontFamily = font.family();
        m_previewBox->setFontFamily(m_fontFamily);
        emit textFontChanged(m_fontFamily, m_fontSize);
    });
    textLayout->addWidget(m_fontCombo);

    auto* sizeLbl = new QLabel("字号:", textPage);
    sizeLbl->setStyleSheet("color: #bfbfbf; font-size: 12px;");
    textLayout->addWidget(sizeLbl);

    m_textSizeSpin = new QSpinBox(textPage);
    m_textSizeSpin->setRange(8, 72);
    m_textSizeSpin->setValue(m_fontSize);
    m_textSizeSpin->setSuffix(" pt");
    m_textSizeSpin->setFixedWidth(75);
    m_textSizeSpin->setCursor(Qt::PointingHandCursor);
    connect(m_textSizeSpin, &QSpinBox::valueChanged, this, [this](int val) {
        m_fontSize = val;
        m_previewBox->setFontSize(val);
        emit textFontChanged(m_fontFamily, val);
    });
    textLayout->addWidget(m_textSizeSpin);
    m_paramStack->addWidget(textPage);

    // --- Page 3: Step Number (Start Number & Circle Size) ---
    auto* stepPage = new QWidget(m_paramStack);
    auto* stepLayout = new QHBoxLayout(stepPage);
    stepLayout->setContentsMargins(0, 0, 0, 0);
    stepLayout->setSpacing(6);

    auto* startNumLbl = new QLabel("序号:", stepPage);
    startNumLbl->setStyleSheet("color: #bfbfbf; font-size: 12px;");
    stepLayout->addWidget(startNumLbl);

    m_stepStartSpin = new QSpinBox(stepPage);
    m_stepStartSpin->setRange(1, 99);
    m_stepStartSpin->setValue(m_stepStartNumber);
    m_stepStartSpin->setFixedWidth(55);
    m_stepStartSpin->setCursor(Qt::PointingHandCursor);
    connect(m_stepStartSpin, &QSpinBox::valueChanged, this, [this](int val) {
        m_stepStartNumber = val;
        m_previewBox->setStep(val, m_stepSize);
        emit stepParamsChanged(val, m_stepSize);
    });
    stepLayout->addWidget(m_stepStartSpin);

    auto* stepSizeLbl = new QLabel("大小:", stepPage);
    stepSizeLbl->setStyleSheet("color: #bfbfbf; font-size: 12px;");
    stepLayout->addWidget(stepSizeLbl);

    m_stepSizeSpin = new QSpinBox(stepPage);
    m_stepSizeSpin->setRange(16, 50);
    m_stepSizeSpin->setValue(m_stepSize);
    m_stepSizeSpin->setSuffix(" px");
    m_stepSizeSpin->setFixedWidth(75);
    m_stepSizeSpin->setCursor(Qt::PointingHandCursor);
    connect(m_stepSizeSpin, &QSpinBox::valueChanged, this, [this](int val) {
        m_stepSize = val;
        m_previewBox->setStep(m_stepStartNumber, val);
        emit stepParamsChanged(m_stepStartNumber, val);
    });
    stepLayout->addWidget(m_stepSizeSpin);
    m_paramStack->addWidget(stepPage);

    m_subLayout->addWidget(m_paramStack);

    // 3. Color Palette container
    m_colorContainer = new QWidget(m_subBar);
    auto* colorLayout = new QHBoxLayout(m_colorContainer);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(5);

    QFrame* sepColors = new QFrame(m_colorContainer);
    sepColors->setFrameShape(QFrame::VLine);
    sepColors->setStyleSheet("color: rgba(255, 255, 255, 0.2); margin: 2px 1px;");
    colorLayout->addWidget(sepColors);

    // 8 Ant Design Curated colors
    const char* colors[] = {
        "#ff4d4f", "#fa8c16", "#faad14", "#52c41a",
        "#1677ff", "#722ed1", "#434343", "#ffffff"
    };

    for (const char* hex : colors) {
        QColor col(hex);
        auto* btn = new QToolButton(m_colorContainer);
        btn->setFixedSize(20, 20);
        btn->setCursor(Qt::PointingHandCursor);

        connect(btn, &QToolButton::clicked, [this, col]() {
            onColorButtonClicked(col);
        });
        colorLayout->addWidget(btn);
        m_colorButtons.append({btn, col});
    }
    updateColorButtonsState();

    // Color dialog picker button (+)
    auto* pickBtn = new QToolButton(m_colorContainer);
    pickBtn->setFixedSize(20, 20);
    pickBtn->setCursor(Qt::PointingHandCursor);
    pickBtn->setToolTip("自定义颜色...");

    QPixmap pix(20, 20);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(255, 255, 255, 180), 1, Qt::DashLine));
    p.setBrush(QColor(255, 255, 255, 20));
    p.drawRoundedRect(1, 1, 18, 18, 4, 4);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(12);
    p.setFont(f);
    p.drawText(QRect(0, 0, 20, 20), Qt::AlignCenter, "+");
    pickBtn->setIcon(QIcon(pix));

    connect(pickBtn, &QToolButton::clicked, [this]() {
        QColor chosen = QColorDialog::getColor(m_currentColor, this, "选择标注颜色");
        if (chosen.isValid()) {
            onColorButtonClicked(chosen);
        }
    });
    colorLayout->addWidget(pickBtn);

    m_subLayout->addWidget(m_colorContainer);

    m_rootLayout->addWidget(m_subBar);
}

void Toolbar::updateColorButtonsState() {
    for (const auto& pair : m_colorButtons) {
        auto* btn = pair.first;
        QColor col = pair.second;

        QPixmap pix(20, 20);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);

        bool isSelected = (col == m_currentColor);
        if (isSelected) {
            p.setPen(QPen(Qt::white, 2));
        } else {
            p.setPen(QPen(QColor(0, 0, 0, 80), 1));
        }
        p.setBrush(col);
        p.drawRoundedRect(2, 2, 16, 16, 4, 4);

        btn->setIcon(QIcon(pix));
    }
}

void Toolbar::onToolButtonClicked(int id) {
    auto tool = static_cast<annotation::ToolType>(id);
    auto* btn = m_toolGroup->button(id);

    if (m_activeTool == tool) {
        // Toggle off back to pointer mode
        m_activeTool = annotation::ToolType::None;
        if (btn) btn->setChecked(false);
        m_subBar->hide();
    } else {
        m_activeTool = tool;
        for (auto* b : m_toolGroup->buttons()) {
            b->setChecked(b == btn);
        }

        m_previewBox->setTool(tool);

        if (tool == annotation::ToolType::Rectangle ||
            tool == annotation::ToolType::Ellipse ||
            tool == annotation::ToolType::Arrow ||
            tool == annotation::ToolType::Line ||
            tool == annotation::ToolType::Pen ||
            tool == annotation::ToolType::Highlighter) {
            m_paramStack->setCurrentIndex(0);
            m_colorContainer->show();
            m_subBar->show();
        } else if (tool == annotation::ToolType::Mosaic || tool == annotation::ToolType::Blur) {
            m_paramStack->setCurrentIndex(1);
            m_colorContainer->hide();
            m_subBar->show();
        } else if (tool == annotation::ToolType::Text) {
            m_paramStack->setCurrentIndex(2);
            m_colorContainer->show();
            m_subBar->show();
        } else if (tool == annotation::ToolType::StepNumber) {
            m_paramStack->setCurrentIndex(3);
            m_colorContainer->show();
            m_subBar->show();
        } else {
            m_subBar->hide();
        }
    }
    adjustSize();
    emit toolSelected(m_activeTool);
    emit sizeChanged();
}

void Toolbar::setSelectedTool(annotation::ToolType tool) {
    m_activeTool = tool;
    for (auto* b : m_toolGroup->buttons()) {
        b->setChecked(m_toolGroup->id(b) == static_cast<int>(tool));
    }
    m_previewBox->setTool(tool);

    if (tool == annotation::ToolType::Rectangle ||
        tool == annotation::ToolType::Ellipse ||
        tool == annotation::ToolType::Arrow ||
        tool == annotation::ToolType::Line ||
        tool == annotation::ToolType::Pen ||
        tool == annotation::ToolType::Highlighter) {
        m_paramStack->setCurrentIndex(0);
        m_colorContainer->show();
        m_subBar->show();
    } else if (tool == annotation::ToolType::Mosaic || tool == annotation::ToolType::Blur) {
        m_paramStack->setCurrentIndex(1);
        m_colorContainer->hide();
        m_subBar->show();
    } else if (tool == annotation::ToolType::Text) {
        m_paramStack->setCurrentIndex(2);
        m_colorContainer->show();
        m_subBar->show();
    } else if (tool == annotation::ToolType::StepNumber) {
        m_paramStack->setCurrentIndex(3);
        m_colorContainer->show();
        m_subBar->show();
    } else {
        m_subBar->hide();
    }
    adjustSize();
    emit sizeChanged();
}

annotation::ToolType Toolbar::selectedTool() const {
    return m_activeTool;
}

void Toolbar::onColorButtonClicked(const QColor& color) {
    m_currentColor = color;
    m_previewBox->setColor(color);
    updateColorButtonsState();
    emit colorChanged(color);
}

void Toolbar::setCanUndo(bool canUndo) {
    if (m_undoBtn) m_undoBtn->setEnabled(canUndo);
}

void Toolbar::setCanRedo(bool canRedo) {
    if (m_redoBtn) m_redoBtn->setEnabled(canRedo);
}

} // namespace fouryshot::ui
