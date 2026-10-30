#pragma once

#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <vector>

#include "ocr_service.h"

namespace fouryshot::core {

// Pure-logic model for OCR-backed text selection, shared by OverlayWidget and
// PinWindow. All coordinates are in "OCR logical space" — i.e. the coordinate
// system of OcrWordItem::logicalRect (the cropped area, logical pixels).
// Owners translate/scale widget-local points into that space before calling
// in, and map the returned rects back for painting.
//
// This class is intentionally free of QObject/GUI dependencies so the
// hit-testing, range selection, highlight-band computation and text assembly
// exist in exactly one place (they used to be duplicated in pin_window.cpp
// and overlay_widget.cpp and could drift apart).
class OcrTextModel {
public:
    // Replace the recognition result. Clears selection and hover state.
    void setResult(const OcrResult& res);
    // Drop words/lines (e.g. selection region invalidated). Clears selection and hover.
    void clearResult();

    // Selection state -------------------------------------------------------
    void clearSelection();
    bool hasSelection() const { return !m_selectedIndices.empty(); }
    const std::vector<int>& selectedIndices() const { return m_selectedIndices; }
    bool isSelected(int wordIdx) const;

    void setHoveredIndex(int wordIdx) { m_hoveredIndex = wordIdx; }
    int hoveredIndex() const { return m_hoveredIndex; }

    // Data access -----------------------------------------------------------
    bool hasWords() const { return !m_words.empty(); }
    int wordCount() const { return static_cast<int>(m_words.size()); }
    const OcrWordItem& word(int idx) const { return m_words[static_cast<size_t>(idx)]; }
    const std::vector<OcrWordItem>& words() const { return m_words; }
    const std::vector<OcrLineItem>& lines() const { return m_lines; }
    const QString& fullText() const { return m_fullText; }

    // Geometry / selection operations (points in OCR logical space) ---------
    // Exact hit test against word rects expanded by (-3,-2,3,2); -1 if none.
    int findWordAt(const QPoint& pt) const;
    // Exact hit first, then line-aware nearest, then closest-center fallback.
    int findNearestWordAt(const QPoint& pt) const;

    void selectSingle(int wordIdx);
    // Double-click behaviour: every word on the line of wordIdx.
    void selectLineOf(int wordIdx);
    // Drag selection: all words between anchorIdx and currentIdx in reading order.
    void updateSelectedRange(int anchorIdx, int currentIdx);

    // Highlight bands in OCR logical space. One rect per contiguous run of
    // selected words; band height comes from the line rect when line data is
    // available, otherwise from the union of the selected words' rects.
    QList<QRect> highlightRects() const;
    // United bounding rect of highlightRects(); QRect() when nothing selected.
    QRect selectedBoundingRect() const;

    // Selected text; words on different lines separated by '\n', adjacent
    // ASCII words by a single space.
    QString selectedText() const;

private:
    std::vector<OcrWordItem> m_words;
    std::vector<OcrLineItem> m_lines;
    QString m_fullText;
    std::vector<int> m_selectedIndices;
    int m_hoveredIndex = -1;
};

} // namespace fouryshot::core
