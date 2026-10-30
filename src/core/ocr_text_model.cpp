#include "ocr_text_model.h"

#include <algorithm>
#include <limits>
#include <map>
#include <unordered_set>

namespace fouryshot::core {

namespace {
constexpr int kHitPadX = 3;
constexpr int kHitPadY = 2;
constexpr int kRunGapMin = 20;   // minimum horizontal gap that keeps words in one run
constexpr int kRunGapFallback = 25; // gap when line data is unavailable
constexpr int kBandPadLeft = 2;
constexpr int kBandPadTop = 1;
constexpr int kBandPadRight = 4;
constexpr int kBandPadBottom = 2;
constexpr int kRangeSnapX = 4;   // tolerance when clipping a line range at the end words
} // namespace

void OcrTextModel::setResult(const OcrResult& res) {
    m_words = res.words;
    m_lines = res.lines;
    m_fullText = res.fullText;
    m_selectedIndices.clear();
    m_hoveredIndex = -1;
}

void OcrTextModel::clearResult() {
    m_words.clear();
    m_lines.clear();
    m_fullText.clear();
    m_selectedIndices.clear();
    m_hoveredIndex = -1;
}

void OcrTextModel::clearSelection() {
    m_selectedIndices.clear();
}

bool OcrTextModel::isSelected(int wordIdx) const {
    return std::find(m_selectedIndices.begin(), m_selectedIndices.end(), wordIdx) != m_selectedIndices.end();
}

int OcrTextModel::findWordAt(const QPoint& pt) const {
    for (size_t i = 0; i < m_words.size(); ++i) {
        const QRect hitBox = m_words[i].logicalRect.adjusted(-kHitPadX, -kHitPadY, kHitPadX, kHitPadY);
        if (hitBox.contains(pt)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int OcrTextModel::findNearestWordAt(const QPoint& pt) const {
    if (m_words.empty()) return -1;

    // 1. Direct hit test first.
    int exactHit = findWordAt(pt);
    if (exactHit != -1) return exactHit;

    // 2. Line-aware nearest test.
    if (!m_lines.empty()) {
        int bestLine = -1;
        int bestDistY = std::numeric_limits<int>::max();
        for (size_t l = 0; l < m_lines.size(); ++l) {
            const auto& line = m_lines[l];
            if (line.wordIndices.empty()) continue;
            int distY = 0;
            if (pt.y() < line.logicalRect.top()) {
                distY = line.logicalRect.top() - pt.y();
            } else if (pt.y() > line.logicalRect.bottom()) {
                distY = pt.y() - line.logicalRect.bottom();
            }
            if (distY < bestDistY) {
                bestDistY = distY;
                bestLine = static_cast<int>(l);
            }
        }

        if (bestLine != -1) {
            const auto& line = m_lines[static_cast<size_t>(bestLine)];
            int bestWord = line.wordIndices.front();
            int bestDistX = std::numeric_limits<int>::max();
            for (int wIdx : line.wordIndices) {
                const auto& w = m_words[static_cast<size_t>(wIdx)];
                int distX = 0;
                if (pt.x() < w.logicalRect.left()) {
                    distX = w.logicalRect.left() - pt.x();
                } else if (pt.x() > w.logicalRect.right()) {
                    distX = pt.x() - w.logicalRect.right();
                }
                if (distX < bestDistX) {
                    bestDistX = distX;
                    bestWord = wIdx;
                }
            }
            return bestWord;
        }
    }

    // 3. Fallback: closest center distance.
    int bestWord = -1;
    qint64 bestDistSq = std::numeric_limits<qint64>::max();
    for (size_t i = 0; i < m_words.size(); ++i) {
        const QPoint c = m_words[i].logicalRect.center();
        const qint64 dx = c.x() - pt.x();
        const qint64 dy = c.y() - pt.y();
        const qint64 dsq = dx * dx + dy * dy;
        if (dsq < bestDistSq) {
            bestDistSq = dsq;
            bestWord = static_cast<int>(i);
        }
    }
    return bestWord;
}

void OcrTextModel::selectSingle(int wordIdx) {
    m_selectedIndices.clear();
    if (wordIdx >= 0 && wordIdx < static_cast<int>(m_words.size())) {
        m_selectedIndices.push_back(wordIdx);
    }
}

void OcrTextModel::selectLineOf(int wordIdx) {
    m_selectedIndices.clear();
    if (wordIdx < 0 || wordIdx >= static_cast<int>(m_words.size())) return;
    const auto& w = m_words[static_cast<size_t>(wordIdx)];
    if (!m_lines.empty() && w.lineIndex >= 0 && w.lineIndex < static_cast<int>(m_lines.size())) {
        m_selectedIndices = m_lines[static_cast<size_t>(w.lineIndex)].wordIndices;
    } else {
        for (size_t i = 0; i < m_words.size(); ++i) {
            if (m_words[i].lineNum == w.lineNum) {
                m_selectedIndices.push_back(static_cast<int>(i));
            }
        }
    }
}

void OcrTextModel::updateSelectedRange(int anchorIdx, int currentIdx) {
    if (anchorIdx < 0 || anchorIdx >= static_cast<int>(m_words.size()) ||
        currentIdx < 0 || currentIdx >= static_cast<int>(m_words.size())) return;

    m_selectedIndices.clear();

    const auto& wA = m_words[static_cast<size_t>(anchorIdx)];
    const auto& wB = m_words[static_cast<size_t>(currentIdx)];

    bool aBeforeB = true;
    if (wA.lineIndex != -1 && wB.lineIndex != -1) {
        if (wA.lineIndex > wB.lineIndex) {
            aBeforeB = false;
        } else if (wA.lineIndex == wB.lineIndex) {
            aBeforeB = (wA.logicalRect.left() <= wB.logicalRect.left());
        }
    } else {
        if (wA.lineNum > wB.lineNum) {
            aBeforeB = false;
        } else if (wA.lineNum == wB.lineNum) {
            aBeforeB = (wA.logicalRect.left() <= wB.logicalRect.left());
        }
    }

    const int startIdx = aBeforeB ? anchorIdx : currentIdx;
    const int endIdx = aBeforeB ? currentIdx : anchorIdx;
    const auto& wStart = m_words[static_cast<size_t>(startIdx)];
    const auto& wEnd = m_words[static_cast<size_t>(endIdx)];

    if (!m_lines.empty() && wStart.lineIndex >= 0 && wEnd.lineIndex >= 0 &&
        wStart.lineIndex < static_cast<int>(m_lines.size()) &&
        wEnd.lineIndex < static_cast<int>(m_lines.size())) {

        const int lStart = wStart.lineIndex;
        const int lEnd = wEnd.lineIndex;

        if (lStart == lEnd) {
            const int minX = std::min(wStart.logicalRect.left(), wEnd.logicalRect.left());
            const int maxX = std::max(wStart.logicalRect.right(), wEnd.logicalRect.right());
            for (int wIdx : m_lines[static_cast<size_t>(lStart)].wordIndices) {
                const auto& w = m_words[static_cast<size_t>(wIdx)];
                if (w.logicalRect.right() >= minX && w.logicalRect.left() <= maxX) {
                    m_selectedIndices.push_back(wIdx);
                }
            }
        } else {
            // Line lStart: from wStart to end of line.
            for (int wIdx : m_lines[static_cast<size_t>(lStart)].wordIndices) {
                if (m_words[static_cast<size_t>(wIdx)].logicalRect.left() >= wStart.logicalRect.left() - kRangeSnapX) {
                    m_selectedIndices.push_back(wIdx);
                }
            }
            // Intermediate lines: all words.
            for (int l = lStart + 1; l < lEnd; ++l) {
                for (int wIdx : m_lines[static_cast<size_t>(l)].wordIndices) {
                    m_selectedIndices.push_back(wIdx);
                }
            }
            // Line lEnd: from start of line up to wEnd.
            for (int wIdx : m_lines[static_cast<size_t>(lEnd)].wordIndices) {
                if (m_words[static_cast<size_t>(wIdx)].logicalRect.right() <= wEnd.logicalRect.right() + kRangeSnapX) {
                    m_selectedIndices.push_back(wIdx);
                }
            }
        }
    } else {
        const int lo = std::min(anchorIdx, currentIdx);
        const int hi = std::max(anchorIdx, currentIdx);
        for (int i = lo; i <= hi; ++i) {
            m_selectedIndices.push_back(i);
        }
    }

    // Sort in reading order.
    std::sort(m_selectedIndices.begin(), m_selectedIndices.end(), [this](int a, int b) {
        const auto& wa = m_words[static_cast<size_t>(a)];
        const auto& wb = m_words[static_cast<size_t>(b)];
        if (wa.lineIndex != wb.lineIndex) return wa.lineIndex < wb.lineIndex;
        return wa.logicalRect.left() < wb.logicalRect.left();
    });
}

QList<QRect> OcrTextModel::highlightRects() const {
    QList<QRect> rects;
    if (m_selectedIndices.empty() || m_words.empty()) return rects;

    std::unordered_set<int> selSet(m_selectedIndices.begin(), m_selectedIndices.end());

    // Merge horizontally-adjacent selected words of one line into a run and
    // emit one band rect per run.
    auto emitRuns = [&](const std::vector<int>& lineWords, int bandTop, int bandHeight, int gap) {
        if (lineWords.empty()) return;
        int runLeft = m_words[static_cast<size_t>(lineWords[0])].logicalRect.left();
        int runRight = m_words[static_cast<size_t>(lineWords[0])].logicalRect.right();
        auto appendRun = [&]() {
            rects.append(QRect(runLeft - kBandPadLeft, bandTop - kBandPadTop,
                               (runRight - runLeft) + kBandPadRight,
                               bandHeight + kBandPadBottom));
        };
        for (size_t i = 1; i < lineWords.size(); ++i) {
            const auto& w = m_words[static_cast<size_t>(lineWords[i])];
            if (w.logicalRect.left() <= runRight + gap) {
                runRight = std::max(runRight, w.logicalRect.right());
            } else {
                appendRun();
                runLeft = w.logicalRect.left();
                runRight = w.logicalRect.right();
            }
        }
        appendRun();
    };

    if (!m_lines.empty()) {
        // Band height from the line rect keeps multi-size lines clean.
        for (const auto& line : m_lines) {
            std::vector<int> lineWords;
            for (int wIdx : line.wordIndices) {
                if (selSet.count(wIdx)) lineWords.push_back(wIdx);
            }
            if (lineWords.empty()) continue;
            std::sort(lineWords.begin(), lineWords.end(), [this](int a, int b) {
                return m_words[static_cast<size_t>(a)].logicalRect.left() <
                       m_words[static_cast<size_t>(b)].logicalRect.left();
            });
            const int gap = std::max(kRunGapMin, line.logicalRect.height());
            emitRuns(lineWords, line.logicalRect.top(), line.logicalRect.height(), gap);
        }
    } else {
        // No line data: group by word lineNum, band height from word union.
        std::map<int, std::vector<int>> wordsByLine;
        for (int idx : m_selectedIndices) {
            if (idx >= 0 && idx < static_cast<int>(m_words.size())) {
                wordsByLine[m_words[static_cast<size_t>(idx)].lineNum].push_back(idx);
            }
        }
        for (auto& [lineNum, wordList] : wordsByLine) {
            std::sort(wordList.begin(), wordList.end(), [this](int a, int b) {
                return m_words[static_cast<size_t>(a)].logicalRect.left() <
                       m_words[static_cast<size_t>(b)].logicalRect.left();
            });
            int runLeft = m_words[static_cast<size_t>(wordList[0])].logicalRect.left();
            int runRight = m_words[static_cast<size_t>(wordList[0])].logicalRect.right();
            int runTop = m_words[static_cast<size_t>(wordList[0])].logicalRect.top();
            int runBottom = m_words[static_cast<size_t>(wordList[0])].logicalRect.bottom();
            auto appendRun = [&]() {
                rects.append(QRect(runLeft - kBandPadLeft, runTop - kBandPadTop,
                                   (runRight - runLeft) + kBandPadRight,
                                   (runBottom - runTop) + kBandPadBottom));
            };
            for (size_t i = 1; i < wordList.size(); ++i) {
                const auto& w = m_words[static_cast<size_t>(wordList[i])];
                if (w.logicalRect.left() <= runRight + kRunGapFallback) {
                    runRight = std::max(runRight, w.logicalRect.right());
                    runTop = std::min(runTop, w.logicalRect.top());
                    runBottom = std::max(runBottom, w.logicalRect.bottom());
                } else {
                    appendRun();
                    runLeft = w.logicalRect.left();
                    runRight = w.logicalRect.right();
                    runTop = w.logicalRect.top();
                    runBottom = w.logicalRect.bottom();
                }
            }
            appendRun();
        }
    }
    return rects;
}

QRect OcrTextModel::selectedBoundingRect() const {
    const auto rects = highlightRects();
    if (rects.isEmpty()) return QRect();
    QRect united = rects[0];
    for (int i = 1; i < rects.size(); ++i) {
        united = united.united(rects[i]);
    }
    return united;
}

QString OcrTextModel::selectedText() const {
    if (m_selectedIndices.empty()) return QString();
    QString text;
    int prevLine = -1;
    for (int idx : m_selectedIndices) {
        if (idx < 0 || idx >= static_cast<int>(m_words.size())) continue;
        const auto& w = m_words[static_cast<size_t>(idx)];
        if (prevLine != -1 && w.lineNum != prevLine) {
            text += QLatin1Char('\n');
        } else if (prevLine != -1 && !text.isEmpty() && !text.endsWith(QLatin1Char('\n')) &&
                   !text.endsWith(QLatin1Char(' '))) {
            const bool isAsciiA = text.back().toLatin1() > 0 && !text.back().isSpace();
            const bool isAsciiB = !w.text.isEmpty() && w.text.front().toLatin1() > 0;
            if (isAsciiA && isAsciiB) {
                text += QLatin1Char(' ');
            }
        }
        text += w.text;
        prevLine = w.lineNum;
    }
    return text;
}

} // namespace fouryshot::core
