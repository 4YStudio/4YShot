#pragma once

#include <QImage>
#include <QRect>
#include <QPointF>
#include <vector>

namespace fouryshot::core {

// 智能选区候选（移植自 babry smart_select.rs）。
// rect 为原图物理像素坐标：x, y, width, height，其中 width/height 满足
// right = x + width（半开区间，等价于 RectPx 语义，注意与 QRect::right() 区分）。
struct SmartCandidate {
    QRect rect;
    float quality = 0.0f; // 证据分数 [0.45, 0.95]；seed 候选 0.92
    bool seed = false;    // 来自外部窗口几何（合成器兜底）
};

// 截图完成后调用一次：对整图做边缘/纯色区域分析，返回候选矩形缓存。
// image 需为 32 位 RGB 格式（ARGB32/RGB32），内部会自动转换。
// seeds 为外部提供的窗口几何（物理像素），可为空。
std::vector<SmartCandidate> smartDetect(const QImage& image, const std::vector<QRect>& seeds);

// 悬停命中：返回包含 p 的最优候选（最内层/高置信优先），无命中返回 fallback。
QRect smartAt(const std::vector<SmartCandidate>& candidates, const QPointF& p, const QRect& fallback);

// 滚轮层级切换：direction > 0 扩大到最小父区域（无则 bounds），
// direction < 0 缩小到最大子区域（无则保持 current）。
QRect smartStep(const std::vector<SmartCandidate>& candidates, const QPointF& p,
                const QRect& current, const QRect& bounds, int direction);

// 悬停状态机：滚轮选定层级后，指针小幅移动（<=12 物理像素）不跳回默认候选。
class SmartCursor {
public:
    QRect hover(const std::vector<SmartCandidate>& candidates, const QPointF& p,
                const QRect& current, const QRect& bounds);
    QRect step(const std::vector<SmartCandidate>& candidates, const QPointF& p,
               const QRect& current, const QRect& bounds, int direction);

private:
    bool m_hasAnchor = false;
    QPointF m_anchor;
    bool m_manual = false;
};

} // namespace fouryshot::core
