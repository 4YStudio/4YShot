#pragma once

#include <QWidget>
#include <QImage>
#include <QScreen>
#include <QElapsedTimer>
#include <QVector>
#include <QStringList>

class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace fouryshot::ui {

class LongCapturePanel : public QWidget {
    Q_OBJECT
public:
    explicit LongCapturePanel(const QImage& firstSegment, QWidget* parent = nullptr);

    void replaceSeedFrame(const QImage& segment);
    bool recordFrame(const QImage& segment, qint64 captureTimeMs);
    bool hasPendingFrames() const;
    // keepScrolling=true: 滚动采集中途合并以释放缓存，不切换滚动状态/按钮可见性
    void mergePendingFrames(bool keepScrolling = false);
    QImage compositeImage() const;
    void showMessage(const QString& message);
    void setScrolling(bool scrolling);

signals:
    void sessionFinishing();
    void sessionCancelled();
    void sessionFinished();

private slots:
    void copyResult();
    void saveResult();
    void cancelSession();

private:
    struct ScrollMatch {
        int offset = -1;
        int candidateOffset = -1;
        double score = 0.0;
        double stationaryScore = 0.0;
        bool accepted = false;
    };
    struct PositionedFrame {
        QImage image;
        int documentTop = 0;
    };

    static ScrollMatch findScrollMatch(const QImage& previous, const QImage& next);
    static QImage makeMatchImage(const QImage& image);
    void appendDebug(const QString& message);
    void updateThumbnail();
    void updateStatus(const QString& message);

    QImage m_composite;
    QImage m_lastSegment;
    QImage m_lastMergedSegment;
    QElapsedTimer m_sessionTimer;
    qint64 m_lastCaptureElapsedMs = -1;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_thumbnailLabel = nullptr;
    QPlainTextEdit* m_debugView = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QVector<QImage> m_pendingFrames;
    QVector<PositionedFrame> m_positionedFrames;
    int m_totalCapturedFrames = 1;
    int m_mergedSegmentCount = 1;
    qint64 m_bufferedBytes = 0;
    bool m_scrolling = false;
    bool m_sessionEnded = false;
};

// A clean, transparent surface used only while collecting a long screenshot.
// It is created after the original screenshot overlays are closed, so no
// frozen desktop image can remain underneath the selected region.
class LongCaptureOverlay : public QWidget {
    Q_OBJECT
public:
    LongCaptureOverlay(QScreen* screen, const QRect& globalSelection, QWidget* parent = nullptr);
    void setInputRegion(const QRect& globalSelection, const QRect& localPanelRect);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QRect m_screenGeometry;
    QRect m_globalSelection;
};

} // namespace fouryshot::ui
