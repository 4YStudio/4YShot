#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

namespace fouryshot::ui {

class TextSelectionBubble : public QWidget {
    Q_OBJECT
public:
    explicit TextSelectionBubble(QWidget* parent = nullptr);

    void updateBubble(const QString& selectedText, const QRect& visualAnchorRect);
    void hideBubble();

signals:
    void copyRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void onCopyClicked();

    QLabel* m_iconLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    QLabel* m_previewLabel = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QString m_currentText;
};

} // namespace fouryshot::ui

