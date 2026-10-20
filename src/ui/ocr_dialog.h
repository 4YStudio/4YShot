#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>

namespace fouryshot::ui {

class OcrDialog : public QDialog {
    Q_OBJECT
public:
    explicit OcrDialog(const QPixmap& croppedImage, QWidget* parent = nullptr);

    void setText(const QString& text);

private slots:
    void onCopyAll();
    void onCopyAndClose();

private:
    void processOcr(const QPixmap& pixmap);

    QPlainTextEdit* m_textEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_copyCloseBtn = nullptr;
};

} // namespace fouryshot::ui
