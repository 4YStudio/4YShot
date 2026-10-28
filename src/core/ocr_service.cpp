#include "ocr_service.h"
#include "ocr_engine.h"
#include <QProcess>
#include <QTemporaryFile>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QFile>
#include <cmath>

namespace fouryshot::core {

namespace {

// Split a line's text into highlight tokens: each wide (CJK) char is its
// own token; runs of ASCII letters/digits (with attached punctuation) form
// one token. Returns [start, len) char ranges into the text.
struct TokenRange { int start = 0; int len = 0; };

bool isTokenPunct(QChar ch) {
    // ASCII punctuation that glues to a latin/digit run
    return ch == QLatin1Char('.') || ch == QLatin1Char(',') || ch == QLatin1Char('-')
        || ch == QLatin1Char('_') || ch == QLatin1Char('/') || ch == QLatin1Char('\'')
        || ch == QLatin1Char('@') || ch == QLatin1Char('#') || ch == QLatin1Char(':');
}

std::vector<TokenRange> splitTextTokens(const QString& text) {
    std::vector<TokenRange> out;
    const int n = text.length();
    int i = 0;
    while (i < n) {
        const QChar ch = text.at(i);
        if (ch.isSpace()) { ++i; continue; }
        if (isWideOcrChar(ch)) {
            out.push_back({i, 1});
            ++i;
            continue;
        }
        // latin/digit run with glued punctuation
        const int start = i;
        int j = i;
        while (j < n) {
            const QChar c = text.at(j);
            if (c.isSpace() || isWideOcrChar(c)) break;
            if (isTokenPunct(c)) { ++j; continue; }
            if (c.isLetterOrNumber()) { ++j; continue; }
            break;
        }
        // trim trailing punctuation-only tail (keep at least 1 char)
        int end = std::max(start + 1, j);
        out.push_back({start, end - start});
        i = end;
    }
    return out;
}

float charWeight(QChar ch) {
    if (ch.isSpace()) return 0.35f;
    if (isWideOcrChar(ch)) return 1.0f;
    return 0.55f;
}

// Pseudo word rects: distribute the line rect horizontally proportional to
// per-char weights. Keeps word-level highlight/selection working with
// line-level PP-OCR boxes.
std::vector<QRect> tokenRects(const QString& text, const QRect& lineRect) {
    std::vector<QRect> out;
    const auto tokens = splitTextTokens(text);
    if (tokens.empty()) return out;

    double total = 0.0;
    for (int i = 0; i < text.length(); ++i) total += charWeight(text.at(i));
    if (total <= 0.0) total = 1.0;

    const double unit = lineRect.width() / total;
    double cursor = 0.0;
    for (const auto& t : tokens) {
        double wBefore = 0.0;
        for (int i = 0; i < t.start; ++i) wBefore += charWeight(text.at(i));
        double wTok = 0.0;
        for (int i = t.start; i < t.start + t.len; ++i) wTok += charWeight(text.at(i));

        int x0 = lineRect.left() + static_cast<int>(std::round(wBefore * unit));
        int x1 = lineRect.left() + static_cast<int>(std::round((wBefore + wTok) * unit));
        if (x1 <= x0) x1 = x0 + 1;
        out.emplace_back(x0, lineRect.top(), x1 - x0, lineRect.height());
        cursor = wBefore + wTok;
        Q_UNUSED(cursor);
    }
    return out;
}

#ifdef HAS_ONNXRUNTIME
// Map engine lines (physical px space) onto the OcrResult structure.
void mapEngineLines(const QVector<OcrTextLine>& lines, qreal dpr, OcrResult& result) {
    for (int li = 0; li < lines.size(); ++li) {
        const OcrTextLine& src = lines.at(li);
        const int lx1 = std::round(src.rect.x() / dpr);
        const int ly1 = std::round(src.rect.y() / dpr);
        const int lx2 = std::round((src.rect.x() + src.rect.width()) / dpr);
        const int ly2 = std::round((src.rect.y() + src.rect.height()) / dpr);
        QRect lineRect(lx1, ly1, std::max(1, lx2 - lx1), std::max(1, ly2 - ly1));

        OcrLineItem lineItem;
        lineItem.blockNum = 0;
        lineItem.parNum = 0;
        lineItem.lineNum = li + 1;
        lineItem.logicalRect = lineRect;
        const int lineIndex = static_cast<int>(result.lines.size());
        result.lines.push_back(lineItem);

        const auto rects = tokenRects(src.text, lineRect);
        const auto tokens = splitTextTokens(src.text);
        for (size_t t = 0; t < tokens.size(); ++t) {
            OcrWordItem word;
            word.logicalRect = rects.at(t);
            word.text = src.text.mid(tokens[t].start, tokens[t].len);
            word.confidence = src.confidence;
            word.blockNum = 0;
            word.parNum = 0;
            word.lineNum = li + 1;
            word.wordNum = static_cast<int>(t);
            word.lineIndex = lineIndex;
            result.lines[lineIndex].wordIndices.push_back(static_cast<int>(result.words.size()));
            result.words.push_back(word);
        }
    }

    for (int li = 0; li < lines.size(); ++li) {
        if (li > 0) result.fullText += QLatin1Char('\n');
        result.fullText += lines.at(li).text;
    }
    result.success = !result.words.empty();
}
#endif

// Tesseract fallback: parses the TSV output with word-level positions.
void runTesseract(const QImage& img, qreal dpr, OcrResult& result) {
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);
    if (!tempFile.open()) {
        return;
    }
    QString imgPath = tempFile.fileName() + ".png";
    img.save(imgPath, "PNG");

    QProcess proc;
    proc.start("tesseract", {imgPath, "stdout", "-l", "chi_sim+eng", "--psm", "6", "tsv"});
    if (!proc.waitForStarted(2000)) {
        QFile::remove(imgPath);
        return;
    }
    if (!proc.waitForFinished(6000)) {
        proc.kill();
        QFile::remove(imgPath);
        return;
    }

    QByteArray tsvData = proc.readAllStandardOutput();
    QFile::remove(imgPath);

    QList<QByteArray> lines = tsvData.split('\n');
    if (lines.size() <= 1) {
        return;
    }

    int currentLineIdx = -1;
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray& line = lines[i].trimmed();
        if (line.isEmpty()) continue;

        QList<QByteArray> parts = line.split('\t');
        if (parts.size() < 12) continue;

        int level = parts[0].toInt();
        int blockNum = parts[2].toInt();
        int parNum = parts[3].toInt();
        int lineNum = parts[4].toInt();
        int left = parts[6].toInt();
        int top = parts[7].toInt();
        int width = parts[8].toInt();
        int height = parts[9].toInt();

        int x1 = std::round(left / dpr);
        int y1 = std::round(top / dpr);
        int x2 = std::round((left + width) / dpr);
        int y2 = std::round((top + height) / dpr);
        QRect logRect(x1, y1, std::max(1, x2 - x1), std::max(1, y2 - y1));

        if (level == 4) {
            OcrLineItem lineItem;
            lineItem.blockNum = blockNum;
            lineItem.parNum = parNum;
            lineItem.lineNum = lineNum;
            lineItem.logicalRect = logRect;
            result.lines.push_back(lineItem);
            currentLineIdx = static_cast<int>(result.lines.size()) - 1;
        } else if (level == 5) {
            QString text = QString::fromUtf8(parts[11]).trimmed();
            if (text.isEmpty()) continue;

            float conf = parts[10].toFloat();
            int wordNum = parts[5].toInt();

            // Skip noise artifacts (e.g. 2px high artifacts, ultra-low confidence junk)
            if (logRect.height() < 5) continue;
            if (conf < 12.0f && text.length() <= 2) continue;

            if (currentLineIdx == -1 ||
                result.lines[currentLineIdx].lineNum != lineNum ||
                result.lines[currentLineIdx].parNum != parNum ||
                result.lines[currentLineIdx].blockNum != blockNum) {
                OcrLineItem lineItem;
                lineItem.blockNum = blockNum;
                lineItem.parNum = parNum;
                lineItem.lineNum = lineNum;
                lineItem.logicalRect = logRect;
                result.lines.push_back(lineItem);
                currentLineIdx = static_cast<int>(result.lines.size()) - 1;
            } else {
                // Maintain stable vertical top and height from the line-level entry, only expand horizontal span
                int lTop = result.lines[currentLineIdx].logicalRect.top();
                int lBottom = result.lines[currentLineIdx].logicalRect.bottom();
                int lLeft = std::min(result.lines[currentLineIdx].logicalRect.left(), logRect.left());
                int lRight = std::max(result.lines[currentLineIdx].logicalRect.right(), logRect.right());
                result.lines[currentLineIdx].logicalRect = QRect(lLeft, lTop, std::max(1, lRight - lLeft), std::max(1, lBottom - lTop));
            }

            OcrWordItem word;
            word.logicalRect = logRect;
            word.text = text;
            word.confidence = conf;
            word.blockNum = blockNum;
            word.parNum = parNum;
            word.lineNum = lineNum;
            word.wordNum = wordNum;
            word.lineIndex = currentLineIdx;

            int wordIdx = static_cast<int>(result.words.size());
            result.words.push_back(word);
            result.lines[currentLineIdx].wordIndices.push_back(wordIdx);
        }
    }

    // Build full text
    int lastLine = -1;
    for (const auto& w : result.words) {
        if (lastLine != -1 && w.lineNum != lastLine) {
            result.fullText += "\n";
        } else if (lastLine != -1 && !result.fullText.isEmpty() && !result.fullText.endsWith("\n") && !result.fullText.endsWith(" ")) {
            bool isAsciiA = result.fullText.back().toLatin1() > 0 && !result.fullText.back().isSpace();
            bool isAsciiB = !w.text.isEmpty() && w.text.front().toLatin1() > 0;
            if (isAsciiA && isAsciiB) {
                result.fullText += " ";
            }
        }
        result.fullText += w.text;
        lastLine = w.lineNum;
    }

    result.success = !result.words.empty();
}

} // namespace

OcrService::OcrService(QObject* parent) : QObject(parent) {}

OcrService::~OcrService() {
    cancel();
}

void OcrService::cancel() {
    m_jobCounter++;
}

void OcrService::recognizeAsync(const QPixmap& cropPixmap, qreal dpr, std::function<void(const OcrResult&)> callback) {
    if (cropPixmap.isNull() || cropPixmap.width() < 4 || cropPixmap.height() < 4) {
        OcrResult empty;
        callback(empty);
        return;
    }

    // QPixmap must stay on the GUI thread: convert to QImage here.
    QImage image = cropPixmap.toImage().convertToFormat(QImage::Format_RGB888);

    int jobId = ++m_jobCounter;
    auto* watcher = new QFutureWatcher<OcrResult>(this);

    connect(watcher, &QFutureWatcher<OcrResult>::finished, [this, watcher, jobId, callback]() {
        OcrResult result = watcher->result();
        if (jobId == m_jobCounter && callback) {
            callback(result);
        }
        watcher->deleteLater();
    });

    QFuture<OcrResult> future = QtConcurrent::run([image, dpr]() -> OcrResult {
        OcrResult result;

#ifdef HAS_ONNXRUNTIME
        OcrEngine* engine = OcrEngine::instance();
        if (engine && engine->isValid()) {
            const auto lines = engine->recognize(image);
            mapEngineLines(lines, dpr, result);
            return result;
        }
#endif
        runTesseract(image, dpr, result);
        return result;
    });

    watcher->setFuture(future);
}

} // namespace fouryshot::core
