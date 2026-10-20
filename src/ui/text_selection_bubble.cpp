#include "text_selection_bubble.h"
#include <QPainter>
#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QFontMetrics>
#include <algorithm>

namespace fouryshot::ui {

TextSelectionBubble::TextSelectionBubble(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedHeight(34);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(14);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 3);
    setGraphicsEffect(shadow);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 8, 0);
    layout->setSpacing(8);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setPixmap(QIcon(":/icons/tool-recognize-text.svg").pixmap(14, 14));
    layout->addWidget(m_iconLabel);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setStyleSheet("color: #9898a8; font-size: 11px; font-weight: bold;");
    layout->addWidget(m_infoLabel);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setStyleSheet("color: #f0f0f5; font-size: 11px;");
    m_previewLabel->setMaximumWidth(160);
    layout->addWidget(m_previewLabel);

    // Subtle divider
    auto* divider = new QWidget(this);
    divider->setFixedSize(1, 14);
    divider->setStyleSheet("background-color: rgba(255, 255, 255, 0.15);");
    layout->addWidget(divider);

    m_copyBtn = new QPushButton("复制 (Ctrl+C)", this);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setFixedHeight(22);
    m_copyBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #1677ff;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 0 8px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: #4096ff;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #0958d9;"
        "}"
    );
    connect(m_copyBtn, &QPushButton::clicked, this, &TextSelectionBubble::onCopyClicked);
    layout->addWidget(m_copyBtn);

    hide();
}

void TextSelectionBubble::updateBubble(const QString& selectedText, const QRect& visualAnchorRect) {
    QString clean = selectedText.trimmed();
    if (clean.isEmpty()) {
        hide();
        return;
    }

    m_currentText = selectedText;
    m_infoLabel->setText(QString("%1字").arg(clean.length()));

    // Short preview
    QString preview = clean;
    preview.replace('\n', ' ');
    if (preview.length() > 18) {
        preview = preview.left(18) + "…";
    }
    m_previewLabel->setText(QString("“%1”").arg(preview));

    // Responsive: hide preview if parent window is narrow
    if (parentWidget() && parentWidget()->width() < 320) {
        m_previewLabel->hide();
    } else {
        m_previewLabel->show();
    }

    adjustSize();

    int bubbleW = width();
    int bubbleH = height();

    // Position directly in parent coordinates
    int targetX = visualAnchorRect.center().x() - bubbleW / 2;
    int targetY = visualAnchorRect.top() - bubbleH - 8;

    if (targetY < 4) {
        targetY = visualAnchorRect.bottom() + 8;
    }

    if (parentWidget()) {
        targetX = std::clamp(targetX, 4, std::max(4, parentWidget()->width() - bubbleW - 4));
        targetY = std::clamp(targetY, 4, std::max(4, parentWidget()->height() - bubbleH - 4));
    }

    move(targetX, targetY);
    show();
    raise();
}

void TextSelectionBubble::hideBubble() {
    hide();
}

void TextSelectionBubble::onCopyClicked() {
    if (m_currentText.isEmpty()) return;
    QGuiApplication::clipboard()->setText(m_currentText);

    m_copyBtn->setText("✓ 已复制");
    m_copyBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #52c41a;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 0 8px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
    );

    QTimer::singleShot(1200, this, [this]() {
        if (!m_copyBtn) return;
        m_copyBtn->setText("复制 (Ctrl+C)");
        m_copyBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: #1677ff;"
            "  color: #ffffff;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 0 8px;"
            "  font-size: 11px;"
            "  font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "  background-color: #4096ff;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #0958d9;"
            "}"
        );
    });

    emit copyRequested();
}

void TextSelectionBubble::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(1, 1, -1, -1);
    painter.setPen(QPen(QColor(60, 60, 72, 220), 1));
    painter.setBrush(QColor(24, 24, 30, 245));
    painter.drawRoundedRect(r, 6, 6);
}

} // namespace fouryshot::ui
