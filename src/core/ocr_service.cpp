#include "ocr_service.h"
#include <QProcess>
#include <QTemporaryFile>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QFile>
#include <cmath>

namespace fouryshot::core {

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

    int jobId = ++m_jobCounter;
    auto* watcher = new QFutureWatcher<OcrResult>(this);

    connect(watcher, &QFutureWatcher<OcrResult>::finished, [this, watcher, jobId, callback]() {
        OcrResult result = watcher->result();
        if (jobId == m_jobCounter && callback) {
            callback(result);
        }
        watcher->deleteLater();
    });

    QFuture<OcrResult> future = QtConcurrent::run([cropPixmap, dpr]() -> OcrResult {
        OcrResult result;
        result.success = false;

        QTemporaryFile tempFile;
        tempFile.setAutoRemove(true);
        if (!tempFile.open()) {
            return result;
        }
        QString imgPath = tempFile.fileName() + ".png";
        cropPixmap.save(imgPath, "PNG");

        QProcess proc;
        proc.start("tesseract", {imgPath, "stdout", "-l", "chi_sim+eng", "--psm", "6", "tsv"});
        if (!proc.waitForStarted(2000)) {
            QFile::remove(imgPath);
            return result;
        }
        if (!proc.waitForFinished(6000)) {
            proc.kill();
            QFile::remove(imgPath);
            return result;
        }

        QByteArray tsvData = proc.readAllStandardOutput();
        QFile::remove(imgPath);

        QList<QByteArray> lines = tsvData.split('\n');
        if (lines.size() <= 1) {
            return result;
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
        return result;
    });

    watcher->setFuture(future);
}

} // namespace fouryshot::core
