#include "long_capture_dialog.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QPushButton>
#include <QRegion>
#include <QScrollBar>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace fouryshot::ui {

LongCapturePanel::LongCapturePanel(const QImage& firstSegment, QWidget* parent)
    : QWidget(parent),
      m_composite(firstSegment.convertToFormat(QImage::Format_RGB32)),
      m_lastSegment(firstSegment.convertToFormat(QImage::Format_RGB32)),
      m_lastMergedSegment(firstSegment.convertToFormat(QImage::Format_RGB32)) {
    setFixedSize(300, 470);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setStyleSheet("LongCapturePanel { background: #1e1e24; border: 1px solid #50505a; border-radius: 10px; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(5);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setMinimumHeight(42);
    m_statusLabel->setStyleSheet("color: #dedee8; font-size: 12px; border: none;");
    layout->addWidget(m_statusLabel);

    m_debugView = new QPlainTextEdit(this);
    m_debugView->setReadOnly(true);
    m_debugView->setMaximumBlockCount(30);
    m_debugView->setFixedHeight(128);
    m_debugView->setStyleSheet(
        "QPlainTextEdit { color: #b8d7ff; background: rgba(0,0,0,100); "
        "border: 1px solid rgba(255,255,255,35); border-radius: 4px; "
        "font-family: monospace; font-size: 10px; padding: 3px; }");
    layout->addWidget(m_debugView);

    m_thumbnailLabel = new QLabel(this);
    m_thumbnailLabel->setFixedSize(276, 188);
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setStyleSheet(
        "QLabel { background: rgba(0, 0, 0, 80); border: 1px solid rgba(255,255,255,45); border-radius: 5px; }");
    layout->addWidget(m_thumbnailLabel, 1, Qt::AlignHCenter);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);
    m_copyButton = new QPushButton("复制", this);
    m_saveButton = new QPushButton("保存", this);
    m_cancelButton = new QPushButton("取消", this);
    m_copyButton->setObjectName("primaryButton");
    for (auto* button : {m_copyButton, m_saveButton, m_cancelButton}) {
        button->setFixedHeight(28);
        button->setStyleSheet("QPushButton { color: white; padding: 2px 8px; }");
        buttonLayout->addWidget(button);
    }
    layout->addLayout(buttonLayout);

    connect(m_copyButton, &QPushButton::clicked, this, &LongCapturePanel::copyResult);
    connect(m_saveButton, &QPushButton::clicked, this, &LongCapturePanel::saveResult);
    connect(m_cancelButton, &QPushButton::clicked, this, &LongCapturePanel::cancelSession);

    updateThumbnail();
    appendDebug("启动：首帧 1 张，等待滚动");
    updateStatus("等待滚动 · 已采集 1 帧");
    m_sessionTimer.start();
}

void LongCapturePanel::replaceSeedFrame(const QImage& segment) {
    const QImage cleanFrame = segment.convertToFormat(QImage::Format_RGB32);
    if (cleanFrame.isNull()) return;

    m_composite = cleanFrame;
    m_lastSegment = cleanFrame;
    m_lastMergedSegment = cleanFrame;
    m_pendingFrames.clear();
    m_positionedFrames.clear();
    m_bufferedBytes = 0;
    m_totalCapturedFrames = 1;
    m_mergedSegmentCount = 1;
    m_lastCaptureElapsedMs = -1;
    updateThumbnail();
    appendDebug(QString("首帧已替换为遮罩关闭后的实时选区：%1x%2")
                    .arg(cleanFrame.width()).arg(cleanFrame.height()));
    updateStatus("首帧已清理 · 等待滚动 · 已采集 1 帧");
}

bool LongCapturePanel::recordFrame(
    const QImage& segment,
    qint64 captureTimeMs
) {
    QImage normalized = segment.convertToFormat(QImage::Format_RGB32);
    if (normalized.isNull() || normalized.size() != m_lastSegment.size()) {
        appendDebug(QString("尺寸异常：实际 %1x%2 · 期望 %3x%4")
                        .arg(segment.width()).arg(segment.height())
                        .arg(m_lastSegment.width()).arg(m_lastSegment.height()));
        updateStatus(QString("采集异常 · 实际 %1x%2 · 期望 %3x%4")
                         .arg(segment.width()).arg(segment.height())
                         .arg(m_lastSegment.width()).arg(m_lastSegment.height()));
        return false;
    }
    ++m_totalCapturedFrames;
    const qint64 elapsedMs = m_sessionTimer.elapsed();
    const qint64 frameIntervalMs = m_lastCaptureElapsedMs < 0
        ? 0 : elapsedMs - m_lastCaptureElapsedMs;
    m_lastCaptureElapsedMs = elapsedMs;
    const double averageFps = elapsedMs > 0
        ? (m_totalCapturedFrames - 1) * 1000.0 / elapsedMs : 0.0;
    const ScrollMatch match = findScrollMatch(m_lastSegment, normalized);
    const double frameChange = match.stationaryScore;
    if (frameChange < 1.0) {
        appendDebug(QString("帧 %1：稳定 · 截图 %2 ms · 间隔 %3 ms · 平均 %4 fps")
                        .arg(m_totalCapturedFrames).arg(captureTimeMs).arg(frameIntervalMs)
                        .arg(averageFps, 0, 'f', 1));
        updateStatus(QString("等待滚动 · 已采集 %1 帧 · %2 fps · 最近采集 %3 ms")
                         .arg(m_totalCapturedFrames).arg(averageFps, 0, 'f', 1).arg(captureTimeMs));
        return false;
    }

    constexpr qint64 maxBufferedBytes = 256LL * 1024LL * 1024LL;
    if (m_pendingFrames.size() < 180 && m_bufferedBytes + normalized.sizeInBytes() <= maxBufferedBytes) {
        m_pendingFrames.push_back(normalized);
        m_bufferedBytes += normalized.sizeInBytes();
    } else if (m_pendingFrames.size() == 180 || m_bufferedBytes + normalized.sizeInBytes() > maxBufferedBytes) {
        appendDebug(QString("缓存上限已达：%1 张 / %2 MB")
                        .arg(m_pendingFrames.size()).arg(m_bufferedBytes / (1024 * 1024)));
    }
    m_lastSegment = normalized;

    const QString matchText = match.accepted
        ? QString("位移 %1 px · 分数 %2").arg(match.offset).arg(match.score, 0, 'f', 1)
        : QString("候选 %1 px · 分数 %2 / 静止 %3")
              .arg(match.candidateOffset)
              .arg(match.score, 0, 'f', 1)
              .arg(match.stationaryScore, 0, 'f', 1);
    appendDebug(QString("帧 %1：变化 %2 · %3 · 缓存 %4 · 截图 %5 ms · 间隔 %6 ms")
                    .arg(m_totalCapturedFrames)
                    .arg(frameChange, 0, 'f', 1)
                    .arg(matchText)
                    .arg(m_pendingFrames.size())
                    .arg(captureTimeMs)
                    .arg(frameIntervalMs));
    updateStatus(match.accepted
        ? QString("检测到滚动 · 已采集 %1 帧 · 待合并 %2 张")
              .arg(m_totalCapturedFrames).arg(m_pendingFrames.size())
        : QString("画面变化，等待位移确认 · 已采集 %1 帧 · 缓存 %2 张")
              .arg(m_totalCapturedFrames).arg(m_pendingFrames.size()));
    return true;
}

bool LongCapturePanel::hasPendingFrames() const {
    return !m_pendingFrames.isEmpty();
}

void LongCapturePanel::mergePendingFrames() {
    if (m_pendingFrames.isEmpty()) {
        updateStatus(QString("等待滚动 · 已采集 %1 帧 · 没有待合并帧").arg(m_totalCapturedFrames));
        return;
    }

    QElapsedTimer mergeTimer;
    mergeTimer.start();
    setScrolling(false);
    updateStatus(QString("合并中 · 0/%1 张").arg(m_pendingFrames.size()));
    appendDebug(QString("开始合并：%1 张缓存帧 · 当前结果 %2x%3")
                    .arg(m_pendingFrames.size()).arg(m_composite.width()).arg(m_composite.height()));
    if (m_positionedFrames.isEmpty()) m_positionedFrames.push_back({m_lastMergedSegment, 0});

    int appendedFrames = 0;
    int rejectedFrames = 0;
    int gapCount = 0;
    for (int index = 0; index < m_pendingFrames.size(); ++index) {
        const QImage& frame = m_pendingFrames[index];
        ScrollMatch bestMatch;
        int bestReference = -1;
        const int firstReference = std::max(0, static_cast<int>(m_positionedFrames.size()) - 8);
        for (int referenceIndex = m_positionedFrames.size() - 1;
             referenceIndex >= firstReference; --referenceIndex) {
            const auto& reference = m_positionedFrames[referenceIndex];
            const ScrollMatch candidate = findScrollMatch(reference.image, frame);
            if (!candidate.accepted) continue;
            if (bestReference < 0 || candidate.score < bestMatch.score) {
                bestMatch = candidate;
                bestReference = referenceIndex;
            }
        }

        if (bestReference < 0) {
            ++rejectedFrames;
            appendDebug(QString("合并 %1/%2：无可靠重叠，跳过")
                            .arg(index + 1).arg(m_pendingFrames.size()));
            continue;
        }

        const int documentTop = m_positionedFrames[bestReference].documentTop + bestMatch.offset;
        const int currentBottom = m_composite.height();
        if (documentTop > currentBottom + 2) {
            ++gapCount;
            appendDebug(QString("合并 %1/%2：发现 %3 px 空隙，拒绝错误拼接")
                            .arg(index + 1).arg(m_pendingFrames.size())
                            .arg(documentTop - currentBottom));
            continue;
        }

        const int initialGap = std::max(0, documentTop - currentBottom);
        if (initialGap > 0) {
            appendDebug(QString("合并 %1/%2：校正 %3 px 微小间隙")
                            .arg(index + 1).arg(m_pendingFrames.size()).arg(initialGap));
        }
        const int firstNewRow = initialGap > 0
            ? std::min(initialGap, frame.height())
            : std::clamp(currentBottom - documentTop, 0, frame.height());
        if (firstNewRow < frame.height()) {
            const int appendedHeight = frame.height() - firstNewRow;
            if (m_composite.height() > 60000 - appendedHeight) {
                appendDebug("达到 60000 px 最大高度，停止合并");
                break;
            }
            QImage expanded(m_composite.width(), m_composite.height() + appendedHeight,
                            QImage::Format_RGB32);
            expanded.setDevicePixelRatio(m_composite.devicePixelRatio());
            for (int y = 0; y < m_composite.height(); ++y) {
                std::copy_n(m_composite.constScanLine(y), m_composite.bytesPerLine(),
                            expanded.scanLine(y));
            }
            for (int y = firstNewRow; y < frame.height(); ++y) {
                std::copy_n(frame.constScanLine(y), frame.bytesPerLine(),
                            expanded.scanLine(m_composite.height() + y - firstNewRow));
            }
            m_composite = std::move(expanded);
            ++appendedFrames;
        }

        m_positionedFrames.push_back({frame, documentTop});
        if (m_positionedFrames.size() > 8) m_positionedFrames.remove(0);
        m_lastMergedSegment = frame;
        if ((index + 1) % 3 == 0 || index + 1 == m_pendingFrames.size()) {
            updateStatus(QString("合并中 · %1/%2 张 · 新增 %3 段")
                             .arg(index + 1).arg(m_pendingFrames.size()).arg(appendedFrames));
            appendDebug(QString("合并进度 %1/%2 · 最近位移 %3 px · 分数 %4")
                            .arg(index + 1).arg(m_pendingFrames.size())
                            .arg(bestMatch.offset).arg(bestMatch.score, 0, 'f', 1));
            update();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

    m_mergedSegmentCount += appendedFrames;
    appendDebug(QString("合并完成：新增 %1 段 · 未匹配 %2 张 · 空隙 %3 次 · 总高 %4 px")
                    .arg(appendedFrames).arg(rejectedFrames).arg(gapCount).arg(m_composite.height()));
    m_pendingFrames.clear();
    m_bufferedBytes = 0;
    updateThumbnail();
    appendDebug(QString("合并耗时 %1 ms").arg(mergeTimer.elapsed()));
    updateStatus(QString("等待滚动 · 已采集 %1 帧 · 合成 %2 段 · 高度 %3 px · 合并 %4 ms")
                     .arg(m_totalCapturedFrames).arg(m_mergedSegmentCount)
                     .arg(m_composite.height()).arg(mergeTimer.elapsed()));
}

QImage LongCapturePanel::compositeImage() const {
    return m_composite;
}

void LongCapturePanel::showMessage(const QString& message) {
    appendDebug(message);
    updateStatus(message);
}

void LongCapturePanel::setScrolling(bool scrolling) {
    if (m_scrolling == scrolling) return;
    m_scrolling = scrolling;
    m_copyButton->setVisible(!scrolling);
    m_saveButton->setVisible(!scrolling);
    m_cancelButton->setVisible(!scrolling);
    if (scrolling) {
        updateStatus(QString("滚动采集中 · 已采集 %1 帧 · 待合并 %2 张")
                         .arg(m_totalCapturedFrames).arg(m_pendingFrames.size()));
    } else {
        updateStatus(QString("滚动暂停 · 已采集 %1 帧 · 待合并 %2 张")
                         .arg(m_totalCapturedFrames).arg(m_pendingFrames.size()));
    }
}

void LongCapturePanel::copyResult() {
    emit sessionFinishing();
    QGuiApplication::clipboard()->setImage(m_composite);
    m_sessionEnded = true;
    emit sessionFinished();
    hide();
}

void LongCapturePanel::saveResult() {
    emit sessionFinishing();
    showMessage("采集已停止 · 选择保存位置");
    QString filePath = QFileDialog::getSaveFileName(
        this, "保存长截图", QDir::homePath() + "/4YShot_Long.png",
        "PNG 图像 (*.png);;JPEG 图像 (*.jpg)"
    );
    if (filePath.isEmpty()) return;
    if (!m_composite.save(filePath)) {
        updateStatus("保存失败，请检查文件路径和权限。");
        return;
    }
    m_sessionEnded = true;
    emit sessionFinished();
    hide();
}

void LongCapturePanel::cancelSession() {
    m_sessionEnded = true;
    emit sessionCancelled();
    hide();
}

void LongCapturePanel::updateThumbnail() {
    if (!m_thumbnailLabel || m_composite.isNull()) return;
    QPixmap preview = QPixmap::fromImage(m_composite).scaled(
        m_thumbnailLabel->size() - QSize(12, 12), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_thumbnailLabel->setPixmap(preview);
}

QImage LongCapturePanel::makeMatchImage(const QImage& image) {
    if (image.isNull()) return {};
    constexpr int maxWidth = 144;
    constexpr int maxHeight = 160;
    const qreal scale = std::min({1.0,
                                  static_cast<qreal>(maxWidth) / image.width(),
                                  static_cast<qreal>(maxHeight) / image.height()});
    const QSize size(std::max(1, qRound(image.width() * scale)),
                     std::max(1, qRound(image.height() * scale)));
    return image.scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation)
        .convertToFormat(QImage::Format_RGB32);
}

LongCapturePanel::ScrollMatch LongCapturePanel::findScrollMatch(
    const QImage& previous,
    const QImage& next
) {
    ScrollMatch result;
    if (previous.isNull() || previous.size() != next.size() || previous.height() < 40) return result;

    const QImage a = makeMatchImage(previous);
    const QImage b = makeMatchImage(next);
    if (a.isNull() || b.isNull() || a.size() != b.size() || a.height() < 20) return result;
    const int height = a.height();
    const int width = a.width();
    const int minOverlap = std::max(12, height / 10);
    const int maxOffset = height - minOverlap;
    const int minOffset = 1;
    const int xStep = std::max(1, width / 96);
    const int blockCount = std::min(8, width);

    qint64 stationaryDiff = 0;
    qint64 stationarySamples = 0;
    for (int y = 0; y < height; y += 2) {
        const auto* aLine = reinterpret_cast<const QRgb*>(a.constScanLine(y));
        const auto* bLine = reinterpret_cast<const QRgb*>(b.constScanLine(y));
        for (int x = 0; x < width; x += xStep) {
            const int la = (qRed(aLine[x]) * 30 + qGreen(aLine[x]) * 59 + qBlue(aLine[x]) * 11) / 100;
            const int lb = (qRed(bLine[x]) * 30 + qGreen(bLine[x]) * 59 + qBlue(bLine[x]) * 11) / 100;
            stationaryDiff += std::abs(la - lb);
            ++stationarySamples;
        }
    }
    if (stationarySamples == 0) return result;
    result.stationaryScore = static_cast<double>(stationaryDiff) / stationarySamples;

    double bestScore = std::numeric_limits<double>::max();
    int bestOffset = -1;
    const auto evaluateOffset = [&](int offset) {
        if (offset < minOffset || offset > maxOffset) return;
        const int overlap = height - offset;
        const int yStep = std::max(1, overlap / 72);
        std::array<qint64, 8> blockDiff{};
        std::array<qint64, 8> blockSamples{};
        for (int y = 0; y < overlap; y += yStep) {
            const auto* aLine = reinterpret_cast<const QRgb*>(a.constScanLine(y + offset));
            const auto* bLine = reinterpret_cast<const QRgb*>(b.constScanLine(y));
            for (int x = 0; x < width; x += xStep) {
                const int la = (qRed(aLine[x]) * 30 + qGreen(aLine[x]) * 59 + qBlue(aLine[x]) * 11) / 100;
                const int lb = (qRed(bLine[x]) * 30 + qGreen(bLine[x]) * 59 + qBlue(bLine[x]) * 11) / 100;
                const int block = std::min(blockCount - 1, x * blockCount / width);
                blockDiff[block] += std::abs(la - lb);
                ++blockSamples[block];
            }
        }
        qint64 diff = 0;
        qint64 samples = 0;
        int matchedBlocks = 0;
        for (int block = 0; block < blockCount; ++block) {
            if (blockSamples[block] == 0) continue;
            const double score = static_cast<double>(blockDiff[block]) / blockSamples[block];
            if (score <= 26.0) ++matchedBlocks;
            diff += blockDiff[block];
            samples += blockSamples[block];
        }
        if (samples == 0 || matchedBlocks < std::max(2, (blockCount * 2 + 2) / 3)) return;
        const double score = static_cast<double>(diff) / samples;
        if (score < bestScore) {
            bestScore = score;
            bestOffset = offset;
        }
    };

    // Coarse-to-fine search keeps continuous capture light enough to run at a
    // high cadence while retaining pixel-level alignment in the final pass.
    for (int offset = minOffset; offset <= maxOffset; offset += 2) evaluateOffset(offset);
    if (bestOffset >= 0) {
        const int coarseOffset = bestOffset;
        for (int offset = std::max(minOffset, coarseOffset - 2);
             offset <= std::min(maxOffset, coarseOffset + 2); ++offset) {
            evaluateOffset(offset);
        }
    }

    if (bestOffset >= 0) {
        result.score = bestScore;
        const qreal scaleY = static_cast<qreal>(previous.height()) / height;
        const int coarseOriginalOffset = qRound(bestOffset * scaleY);
        const int refineRadius = std::max(1, qCeil(scaleY));
        const int originalMinOverlap = std::max(24, previous.height() / 10);
        const int originalMaxOffset = previous.height() - originalMinOverlap;
        const int originalXStep = std::max(1, previous.width() / 128);
        double refinedScore = std::numeric_limits<double>::max();
        int refinedOffset = coarseOriginalOffset;
        for (int offset = std::max(1, coarseOriginalOffset - refineRadius);
             offset <= std::min(originalMaxOffset, coarseOriginalOffset + refineRadius); ++offset) {
            const int overlap = previous.height() - offset;
            const int yStep = std::max(1, overlap / 96);
            qint64 diff = 0;
            qint64 samples = 0;
            for (int y = 0; y < overlap; y += yStep) {
                const auto* aLine = reinterpret_cast<const QRgb*>(previous.constScanLine(y + offset));
                const auto* bLine = reinterpret_cast<const QRgb*>(next.constScanLine(y));
                for (int x = 0; x < previous.width(); x += originalXStep) {
                    const int la = (qRed(aLine[x]) * 30 + qGreen(aLine[x]) * 59 + qBlue(aLine[x]) * 11) / 100;
                    const int lb = (qRed(bLine[x]) * 30 + qGreen(bLine[x]) * 59 + qBlue(bLine[x]) * 11) / 100;
                    diff += std::abs(la - lb);
                    ++samples;
                }
            }
            if (samples == 0) continue;
            const double score = static_cast<double>(diff) / samples;
            if (score < refinedScore) {
                refinedScore = score;
                refinedOffset = offset;
            }
        }
        if (refinedScore < std::numeric_limits<double>::max()) result.score = refinedScore;
        result.candidateOffset = refinedOffset;
        const bool motionEvidence = result.stationaryScore >= 6.0 &&
                                   result.stationaryScore >= result.score + 3.0 &&
                                   result.stationaryScore >= result.score * 1.15 + 2.0;
        result.accepted = result.score <= 26.0 && motionEvidence;
        if (result.accepted) result.offset = result.candidateOffset;
    } else {
        result.score = -1.0;
    }
    return result;
}

void LongCapturePanel::appendDebug(const QString& message) {
    if (!m_debugView) return;
    m_debugView->appendPlainText(message);
    m_debugView->verticalScrollBar()->setValue(m_debugView->verticalScrollBar()->maximum());
}

void LongCapturePanel::updateStatus(const QString& message) {
    if (m_statusLabel) m_statusLabel->setText(message);
}

LongCaptureOverlay::LongCaptureOverlay(
    QScreen* screen,
    const QRect& globalSelection,
    QWidget* parent
) : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool),
    m_screenGeometry(screen ? screen->geometry() : QRect()),
    m_globalSelection(globalSelection) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    if (screen) setScreen(screen);
    if (!m_screenGeometry.isEmpty()) setGeometry(m_screenGeometry);
}

void LongCaptureOverlay::setInputRegion(
    const QRect& globalSelection,
    const QRect& localPanelRect
) {
    m_globalSelection = globalSelection;
    const QRect localSelection = globalSelection.translated(-m_screenGeometry.topLeft());
    const QRect outer = localSelection.adjusted(-4, -4, 4, 4).intersected(rect());
    const QRect inner = localSelection.adjusted(4, 4, -4, -4).intersected(rect());
    QRegion inputRegion = QRegion(outer).subtracted(QRegion(inner));
    inputRegion += QRegion(localPanelRect.intersected(rect()));
    if (inputRegion.isEmpty()) inputRegion = QRegion(QRect(0, 0, 1, 1));
    setMask(inputRegion);
    update();
}

void LongCaptureOverlay::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    const QRect localSelection = m_globalSelection.translated(-m_screenGeometry.topLeft());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#1677ff"), 2, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(localSelection.adjusted(-3, -3, 3, 3));
}

} // namespace fouryshot::ui
