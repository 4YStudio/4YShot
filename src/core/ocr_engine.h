#pragma once

// PP-OCR (PaddleOCR) ONNX Runtime engine: DB text detection + angle
// classification + CRNN recognition. Far better than Tesseract for
// screenshot-style text (anti-aliased UI fonts, colored backgrounds,
// small sizes, mixed Chinese/English).

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>
#include <memory>
#include <mutex>

namespace fouryshot::core {

// True for full-width / CJK-range characters (used for token splitting and
// width-weighted pseudo word rects in the OCR layer).
bool isWideOcrChar(QChar ch);

struct OcrTextLine {
    QRect rect;          // in the coordinate space of the input image (physical px)
    QString text;
    float confidence = 0.0f;
};

class OcrEngine {
public:
    // Lazily created process-wide shared instance. Returns nullptr if the
    // ONNX Runtime engine is not available in this build (no HAS_ONNXRUNTIME).
    static OcrEngine* instance();

    // Locates the model directory; empty string when not found.
    static QString findModelsDir();

    bool isValid() const { return m_valid; }

    // Runs det + (optional cls) + rec on `image` (any QImage format).
    // Returns detected text lines in reading order (top-to-bottom,
    // left-to-right within a line). Rects are in the image's pixel space.
    QVector<OcrTextLine> recognize(const QImage& image);

    ~OcrEngine();

private:
    OcrEngine();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_valid = false;

    static std::mutex s_mutex;
    static std::unique_ptr<OcrEngine> s_instance;
};

} // namespace fouryshot::core
