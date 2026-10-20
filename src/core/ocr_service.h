#pragma once

#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <vector>
#include <functional>

namespace fouryshot::core {

struct OcrWordItem {
    QRect logicalRect; // relative to the cropped area (logical pixels)
    QString text;
    float confidence = 0.0f;
    int lineNum = 0;
    int wordNum = 0;
    int parNum = 0;
    int blockNum = 0;
    int lineIndex = -1; // index into OcrResult::lines
};

struct OcrLineItem {
    int blockNum = 0;
    int parNum = 0;
    int lineNum = 0;
    QRect logicalRect; // bounding rect of the entire line in logical pixels
    std::vector<int> wordIndices; // indices into OcrResult::words
};

struct OcrResult {
    std::vector<OcrWordItem> words;
    std::vector<OcrLineItem> lines;
    QString fullText;
    bool success = false;
};

class OcrService : public QObject {
    Q_OBJECT
public:
    explicit OcrService(QObject* parent = nullptr);
    ~OcrService() override;

    // Run async recognition on crop pixmap with given DPR.
    // Calls callback on the main Qt thread when done.
    void recognizeAsync(const QPixmap& cropPixmap, qreal dpr, std::function<void(const OcrResult&)> callback);

    void cancel();

private:
    int m_jobCounter = 0;
};

} // namespace fouryshot::core
