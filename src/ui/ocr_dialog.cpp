#include "ocr_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>
#include <QTemporaryFile>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace fouryshot::ui {

OcrDialog::OcrDialog(const QPixmap& croppedImage, QWidget* parent)
    : QDialog(parent, Qt::Window | Qt::WindowStaysOnTopHint) {
    setWindowTitle("4YShot 离线文字识别 (OCR)");
    resize(520, 380);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    m_statusLabel = new QLabel("正在识别图像中的文字 (中英双语)...", this);
    m_statusLabel->setStyleSheet("color: #8c8c8c; font-size: 12px;");
    rootLayout->addWidget(m_statusLabel);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlaceholderText("文字识别结果将在此显示，您可以自由编辑...");
    m_textEdit->setStyleSheet("font-size: 13px; line-height: 1.5; font-family: 'JetBrains Mono', 'Fira Code', 'Noto Sans CJK SC', monospace;");
    rootLayout->addWidget(m_textEdit);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_copyBtn = new QPushButton("复制全部文本", this);
    m_copyCloseBtn = new QPushButton("复制并关闭", this);
    m_copyCloseBtn->setObjectName("primaryButton");
    auto* closeBtn = new QPushButton("关闭", this);

    connect(m_copyBtn, &QPushButton::clicked, this, &OcrDialog::onCopyAll);
    connect(m_copyCloseBtn, &QPushButton::clicked, this, &OcrDialog::onCopyAndClose);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

    btnLayout->addStretch();
    btnLayout->addWidget(m_copyBtn);
    btnLayout->addWidget(m_copyCloseBtn);
    btnLayout->addWidget(closeBtn);

    rootLayout->addLayout(btnLayout);

    processOcr(croppedImage);
}

void OcrDialog::processOcr(const QPixmap& pixmap) {
    if (pixmap.isNull()) {
        m_statusLabel->setText("识别失败：图像为空");
        return;
    }

    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, [this, watcher]() {
        QString text = watcher->result();
        setText(text);
        watcher->deleteLater();
    });

    QFuture<QString> future = QtConcurrent::run([pixmap]() -> QString {
        QTemporaryFile tempFile;
        tempFile.setAutoRemove(true);
        if (!tempFile.open()) {
            return QString();
        }
        QString imgPath = tempFile.fileName() + ".png";
        pixmap.save(imgPath, "PNG");

        QProcess proc;
        proc.start("tesseract", {imgPath, "stdout", "-l", "chi_sim+eng", "--psm", "6"});
        if (!proc.waitForStarted(2000)) {
            QFile::remove(imgPath);
            return "启动 OCR 引擎失败";
        }
        if (!proc.waitForFinished(6000)) {
            QFile::remove(imgPath);
            return "OCR 识别超时";
        }

        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        QFile::remove(imgPath);
        return out;
    });

    watcher->setFuture(future);
}

void OcrDialog::setText(const QString& text) {
    m_textEdit->setPlainText(text);
    if (text.isEmpty()) {
        m_statusLabel->setText("未在选区中识别到清晰文字");
    } else {
        m_statusLabel->setText(QString("识别完成，共 %1 个字符").arg(text.length()));
    }
}

void OcrDialog::onCopyAll() {
    QGuiApplication::clipboard()->setText(m_textEdit->toPlainText());
    m_statusLabel->setText("已将文本复制到剪贴板！");
}

void OcrDialog::onCopyAndClose() {
    onCopyAll();
    close();
}

} // namespace fouryshot::ui
