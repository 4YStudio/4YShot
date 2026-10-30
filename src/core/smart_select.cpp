// 智能选区算法：C++ 移植自 babry 的 smart_select.rs（轻量级本地 CV，无外部依赖）。
// 设计契约：截图完成后只分析一次，悬停/滚轮/单击只查询缓存矩形。
// 坐标：候选 rect 为原图物理像素，半开区间 right = x + width, bottom = y + height。
// 全部魔数与 babry 原实现一一对应，勿随意调整。

#include "smart_select.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>

namespace fouryshot::core {
namespace {

struct Line {
    int position = 0;
    float score = 0.0f;
};

// ---- 基础几何辅助（RectPx 语义，rect 以 QRect 存储）----

inline int rectRight(const QRect& r)  { return r.x() + r.width(); }   // 半开
inline int rectBottom(const QRect& r) { return r.y() + r.height(); }  // 半开

inline bool containsPoint(const QRect& r, const QPointF& p)
{
    return p.x() >= r.x() && p.y() >= r.y() && p.x() < rectRight(r) && p.y() < rectBottom(r);
}

// 矩形包含（闭区间语义：右下相等也算包含，babry contains_rect）
inline bool containsRect(const QRect& parent, const QRect& child)
{
    return parent.x() <= child.x() && parent.y() <= child.y()
        && rectRight(parent) >= rectRight(child) && rectBottom(parent) >= rectBottom(child);
}

inline QRect clampRect(const QRect& r, int w, int h)
{
    int left = std::max(r.x(), 0);
    int top = std::max(r.y(), 0);
    int right = std::min(rectRight(r), w);
    int bottom = std::min(rectBottom(r), h);
    right = std::max(right, left);
    bottom = std::max(bottom, top);
    return QRect(left, top, right - left, bottom - top);
}

inline double rectArea(const QRect& r) { return double(r.width()) * double(r.height()); }

inline double iou(const QRect& a, const QRect& b)
{
    const int left = std::max(a.x(), b.x());
    const int top = std::max(a.y(), b.y());
    const int right = std::min(rectRight(a), rectRight(b));
    const int bottom = std::min(rectBottom(a), rectBottom(b));
    if (right <= left || bottom <= top)
        return 0.0;
    const double intersection = double(right - left) * double(bottom - top);
    const double unionArea = rectArea(a) + rectArea(b) - intersection;
    return intersection / std::max(unionArea, 1.0);
}

// ---- 像素访问（32 位图）----

inline std::array<quint8, 3> pixelRgb(const QImage& img, int x, int y)
{
    if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
        return {0, 0, 0};
    const QRgb v = reinterpret_cast<const QRgb*>(img.constScanLine(y))[x];
    return {quint8(qRed(v)), quint8(qGreen(v)), quint8(qBlue(v))};
}

// ---- line_scores：每行/列的最长连续边缘段（允许 2 像素缺口）----

std::vector<float> lineScores(const std::vector<quint8>& edges, int aw, int ah, quint8 threshold, bool horizontal)
{
    const int count = horizontal ? ah : aw;
    const int span = horizontal ? aw : ah;
    std::vector<float> scores(count, 0.0f);
    for (int line = 1; line < count - 1; ++line) {
        int longest = 0;
        int run = 0;
        int gap = 0;
        for (int offset = 1; offset < span - 1; ++offset) {
            const int index = horizontal ? line * aw + offset : offset * aw + line;
            if (edges[index] >= threshold) {
                run += 1 + gap;
                gap = 0;
                longest = std::max(longest, run);
            } else if (run > 0 && gap < 2) {
                gap += 1;
            } else {
                run = 0;
                gap = 0;
            }
        }
        scores[line] = float(longest) / float(span);
    }
    return scores;
}

// ---- local_lines：局部峰值线抽取 ----

std::vector<Line> localLines(const std::vector<float>& scores, int radius, float minimum)
{
    std::vector<Line> result;
    for (int position = radius; position < int(scores.size()) - radius; ++position) {
        const float score = scores[position];
        if (score < minimum)
            continue;
        bool isLocalMax = true;
        for (int delta = 1; delta <= radius; ++delta) {
            if (scores[position - delta] > score || scores[position + delta] > score) {
                isLocalMax = false;
                break;
            }
        }
        if (!isLocalMax)
            continue;
        if (!result.empty() && position - result.back().position <= radius * 2) {
            if (score > result.back().score)
                result.back() = {position, score};
        } else {
            result.push_back({position, score});
        }
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const Line& a, const Line& b) { return a.score > b.score; });
    return result;
}

// ---- side_support：一条边上的边缘命中率 ----

float sideSupport(const std::vector<quint8>& edges, int aw, int ah, int row, int left, int right, quint8 threshold)
{
    const int start = std::min(left, aw - 1);
    const int end = std::max(std::min(right, aw - 1), start + 1);
    const int step = std::max((end - start) / 24, 1);
    int total = 0;
    int hit = 0;
    for (int y = std::max(row - 1, 0); y <= std::min(row + 1, ah - 1); ++y) {
        for (int x = start; x < end; x += step) {
            ++total;
            if (edges[y * aw + x] >= threshold)
                ++hit;
        }
    }
    return float(hit) / float(std::max(total, 1));
}

float sideSupportVertical(const std::vector<quint8>& edges, int aw, int ah, int column, int top, int bottom, quint8 threshold)
{
    const int start = std::min(top, ah - 1);
    const int end = std::max(std::min(bottom, ah - 1), start + 1);
    const int step = std::max((end - start) / 24, 1);
    int total = 0;
    int hit = 0;
    for (int y = start; y < end; y += step) {
        for (int x = std::max(column - 1, 0); x <= std::min(column + 1, aw - 1); ++x) {
            ++total;
            if (edges[y * aw + x] >= threshold)
                ++hit;
        }
    }
    return float(hit) / float(std::max(total, 1));
}

// seed 边支持：屏幕边缘特判（0.85）
float seedSupport(const std::vector<quint8>& gx, const std::vector<quint8>& gy,
                  int aw, int ah, const std::array<int, 4>& sides, quint8 threshold, int side)
{
    // sides = [left, top, right, bottom]（分析图坐标）
    const bool atEdge = (side == 0 && sides[0] <= 1)
        || (side == 1 && sides[1] <= 1)
        || (side == 2 && sides[2] + 2 >= aw)
        || (side == 3 && sides[3] + 2 >= ah);
    if (atEdge)
        return 0.85f;
    switch (side) {
    case 0: return sideSupportVertical(gx, aw, ah, sides[0], sides[1], sides[3], threshold);
    case 1: return sideSupport(gy, aw, ah, sides[1], sides[0], sides[2], threshold);
    case 2: return sideSupportVertical(gx, aw, ah, sides[2], sides[1], sides[3], threshold);
    default: return sideSupport(gy, aw, ah, sides[3], sides[0], sides[2], threshold);
    }
}

QRect analysisToRect(int left, int top, int right, int bottom, int aw, int ah, int w, int h)
{
    const auto x = [aw, w](int v) { return int(std::lround(float(v) / float(aw) * float(w))); };
    const auto y = [ah, h](int v) { return int(std::lround(float(v) / float(ah) * float(h))); };
    const int l = x(left);
    const int t = y(top);
    return clampRect(QRect(l, t, std::max(x(right) - l, 1), std::max(y(bottom) - t, 1)), w, h);
}

// ---- flat_regions：union-find 连通纯色区域 ----

std::vector<SmartCandidate> flatRegions(const std::vector<std::array<quint8, 3>>& rgb, int aw, int ah, int w, int h)
{
    const int n = aw * ah;
    std::vector<uint32_t> parent(n);
    std::iota(parent.begin(), parent.end(), 0u);

    const auto find = [&parent](uint32_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]]; // 路径减半
            x = parent[x];
        }
        return x;
    };

    const auto similar = [&rgb](int a, int b) {
        const auto& ca = rgb[a];
        const auto& cb = rgb[b];
        for (int k = 0; k < 3; ++k)
            if (std::abs(int(ca[k]) - int(cb[k])) > 3)
                return false;
        return true;
    };

    for (int y = 0; y < ah; ++y) {
        for (int x = 0; x < aw; ++x) {
            const int i = y * aw + x;
            if (x + 1 < aw && similar(i, i + 1)) {
                const uint32_t a = find(i);
                const uint32_t b = find(i + 1);
                if (a != b)
                    parent[std::max(a, b)] = std::min(a, b);
            }
            if (y + 1 < ah && similar(i, i + aw)) {
                const uint32_t a = find(i);
                const uint32_t b = find(i + aw);
                if (a != b)
                    parent[std::max(a, b)] = std::min(a, b);
            }
        }
    }

    // 统计各根的像素数
    std::vector<int> counts(n, 0);
    for (int i = 0; i < n; ++i)
        ++counts[find(i)];

    const int minPixels = std::max(n / 2500, 120);

    // 合格区域 -> 紧凑编号，并完整压缩 parent 使 parent[i] 直接是根
    std::vector<int> rootId(n, -1);
    struct Region { int count; int left; int top; int right; int bottom; };
    std::vector<Region> regions;
    for (int i = 0; i < n; ++i) {
        const uint32_t root = find(i);
        parent[i] = root;
        if (root == uint32_t(i) && counts[i] >= minPixels) {
            rootId[i] = int(regions.size());
            regions.push_back({counts[i], std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), 0, 0});
        }
    }

    for (int y = 0; y < ah; ++y) {
        for (int x = 0; x < aw; ++x) {
            const int id = rootId[parent[y * aw + x]];
            if (id < 0)
                continue;
            auto& r = regions[id];
            r.left = std::min(r.left, x);
            r.top = std::min(r.top, y);
            r.right = std::max(r.right, x);
            r.bottom = std::max(r.bottom, y);
        }
    }

    std::vector<SmartCandidate> candidates;
    for (size_t regionIndex = 0; regionIndex < regions.size(); ++regionIndex) {
        const auto& r = regions[regionIndex];
        const int w2 = r.right + 1 - r.left;
        const int h2 = r.bottom + 1 - r.top;
        if (w2 < 12 || h2 < 10)
            continue;
        if (w2 * 100 >= aw * 97 && h2 * 100 >= ah * 97)
            continue;
        const float fill = float(r.count) / float(w2 * h2);
        if (fill < 0.5f)
            continue;

        const auto member = [&parent, &rootId, &aw, regionIndex](int x, int y) {
            return rootId[parent[y * aw + x]] == int(regionIndex);
        };
        // 行/列覆盖率；最外一圈可能是抗锯齿/圆角，同时看内缩一像素的行列
        const auto rowFill = [&](int y) {
            int hit = 0;
            for (int x = r.left; x <= r.right; ++x)
                if (member(x, y)) ++hit;
            return float(hit) / float(w2);
        };
        const auto columnFill = [&](int x) {
            int hit = 0;
            for (int y = r.top; y <= r.bottom; ++y)
                if (member(x, y)) ++hit;
            return float(hit) / float(h2);
        };
        const float sideTop = std::max(rowFill(r.top), rowFill(std::min(r.top + 1, r.bottom)));
        const float sideBottom = std::max(rowFill(r.bottom), rowFill(std::max(r.bottom - 1, r.top)));
        const float sideLeft = std::max(columnFill(r.left), columnFill(std::min(r.left + 1, r.right)));
        const float sideRight = std::max(columnFill(r.right), columnFill(std::max(r.right - 1, r.left)));
        const float weakest = std::min({sideTop, sideBottom, sideLeft, sideRight});
        const float average = (sideTop + sideBottom + sideLeft + sideRight) / 4.0f;
        if (weakest < 0.35f || average < 0.6f)
            continue;
        candidates.push_back({
            analysisToRect(r.left, r.top, r.right + 1, r.bottom + 1, aw, ah, w, h),
            std::min(0.45f + fill * 0.25f + average * 0.25f, 0.95f),
            false});
    }
    return candidates;
}

// ---- refine：原分辨率 ±radius 内按颜色跳变吸附精修（仅非 seed 候选）----

QRect refineRect(const QImage& img, const QRect& rect, int radius)
{
    const int w = img.width();
    const int h = img.height();
    const auto score = [&](int p, bool vertical, int from, int to) -> float {
        const int limit = vertical ? w : h;
        if (p <= 0 || p >= limit)
            return 0.0f;
        const int margin = (to - from) / 10;
        from += margin;
        to -= margin;
        const int step = std::max((to - from) / 48, 1);
        double sum = 0.0;
        int samples = 0;
        for (int along = from; along < to; along += step) {
            const auto a = vertical ? pixelRgb(img, p - 1, along) : pixelRgb(img, along, p - 1);
            const auto b = vertical ? pixelRgb(img, p, along) : pixelRgb(img, along, p);
            sum += std::max({std::abs(int(a[0]) - int(b[0])),
                             std::abs(int(a[1]) - int(b[1])),
                             std::abs(int(a[2]) - int(b[2]))});
            ++samples;
        }
        return float(sum / double(std::max(samples, 1)));
    };
    const auto best = [&](int original, bool vertical, int from, int to) -> int {
        const int limit = vertical ? w : h;
        if (original <= radius)
            return 0;
        if (original >= limit - radius)
            return limit;
        std::pair<float, int> choice = {score(original, vertical, from, to), original};
        for (int p = original - radius; p <= original + radius; ++p) {
            const float value = score(p, vertical, from, to) - std::abs(p - original) * 0.5f;
            if (value > choice.first)
                choice = {value, p};
        }
        return choice.first >= 6.0f ? choice.second : original;
    };

    const int left = best(rect.x(), true, rect.y(), rectBottom(rect));
    const int right = best(rectRight(rect), true, rect.y(), rectBottom(rect));
    const int top = best(rect.y(), false, left, right);
    const int bottom = best(rectBottom(rect), false, left, right);
    if (right - left < 4 || bottom - top < 4)
        return rect;
    return QRect(left, top, right - left, bottom - top);
}

inline double candidateRank(const SmartCandidate& c)
{
    const double area = rectArea(c.rect);
    const double tier = (c.seed || c.quality >= 0.72f) ? 0.0 : 10000.0;
    return tier + std::pow(area, 0.25) / std::pow(std::max(c.quality, 0.1f), 2);
}

} // namespace

std::vector<SmartCandidate> smartDetect(const QImage& imageIn, const std::vector<QRect>& seeds)
{
    const QImage image = (imageIn.format() == QImage::Format_ARGB32
                          || imageIn.format() == QImage::Format_RGB32
                          || imageIn.format() == QImage::Format_ARGB32_Premultiplied)
        ? imageIn
        : imageIn.convertToFormat(QImage::Format_ARGB32);
    const int w = image.width();
    const int h = image.height();

    // 1. 早退：小图直接返回 seed
    if (w < 8 || h < 8) {
        std::vector<SmartCandidate> out;
        for (const auto& s : seeds)
            out.push_back({clampRect(s, w, h), 0.92f, true});
        return out;
    }

    // 2. 降采样到分析图（最长边 800，最近邻）
    const int maxDimension = 800;
    const float scale = std::min(float(maxDimension) / float(std::max(w, h)), 1.0f);
    const int aw = std::max(int(std::lround(float(w) * scale)), 8);
    const int ah = std::max(int(std::lround(float(h) * scale)), 8);

    std::vector<quint8> luma(aw * ah);
    std::vector<std::array<quint8, 3>> rgb(aw * ah);
    for (int y = 0; y < ah; ++y) {
        const int sy = std::min(y * h / ah, h - 1);
        for (int x = 0; x < aw; ++x) {
            const int sx = std::min(x * w / aw, w - 1);
            const auto p = pixelRgb(image, sx, sy);
            rgb[y * aw + x] = p;
            luma[y * aw + x] = quint8((uint16_t(p[0]) * 54 + uint16_t(p[1]) * 183 + uint16_t(p[2]) * 19) / 256);
        }
    }

    // 3. 中心差分梯度 + 自适应阈值（目标前 10% 像素，下限 20）
    std::vector<quint8> gx(aw * ah, 0);
    std::vector<quint8> gy(aw * ah, 0);
    std::vector<uint32_t> histogram(256, 0);
    for (int y = 1; y < ah - 1; ++y) {
        for (int x = 1; x < aw - 1; ++x) {
            const int i = y * aw + x;
            const int dx = std::abs(int(luma[i + 1]) - int(luma[i - 1]));
            const int dy = std::abs(int(luma[i + aw]) - int(luma[i - aw]));
            gx[i] = quint8(dx);
            gy[i] = quint8(dy);
            ++histogram[std::max(dx, dy)];
        }
    }
    uint32_t seen = 0;
    const uint32_t target = uint32_t(aw * ah) / 10;
    quint8 threshold = 20;
    for (int value = 255; value >= 0; --value) {
        seen += histogram[value];
        if (seen >= target) {
            threshold = std::max<quint8>(threshold, quint8(value / 2));
            break;
        }
    }

    // 4-5. 长直线抽取（每方向最多 24 条）
    std::vector<Line> hLines = localLines(lineScores(gy, aw, ah, threshold, true), 2, 0.06f);
    std::vector<Line> vLines = localLines(lineScores(gx, aw, ah, threshold, false), 2, 0.06f);
    if (hLines.size() > 24) hLines.resize(24);
    if (vLines.size() > 24) vLines.resize(24);

    std::vector<SmartCandidate> candidates;

    // 6. seed 吸附：对每个外部窗口矩形逐边搜索真实边缘
    for (const auto& seedRect : seeds) {
        const QRect clamped = clampRect(seedRect, w, h);
        if (clamped.width() <= 8 || clamped.height() <= 8)
            continue;
        const auto toAnalysis = [aw, ah, w, h](int v, bool isX) -> int {
            const int full = isX ? w : h;
            const int analysis = isX ? aw : ah;
            return std::min(int(std::lround(std::max(v, 0) / float(full) * analysis)), analysis - 1);
        };
        const auto back = [aw, ah, w, h](int v, bool isX) -> int {
            const int full = isX ? w : h;
            const int analysis = isX ? aw : ah;
            return int(std::lround(float(v) / float(analysis) * full));
        };
        std::array<int, 4> sides = {
            toAnalysis(clamped.x(), true),
            toAnalysis(clamped.y(), false),
            toAnalysis(rectRight(clamped), true),
            toAnalysis(rectBottom(clamped), false),
        };
        const std::array<int, 4> originalSides = sides;
        const int radius = std::max(std::max(aw, ah) / 20, 4);
        for (int side = 0; side < 4; ++side) {
            const int original = sides[side];
            const int limit = (side == 0 || side == 2) ? aw : ah;
            std::pair<float, int> bestS = {seedSupport(gx, gy, aw, ah, sides, threshold, side), original};
            for (int position = std::max(original - radius, 0);
                 position <= std::min(original + radius, limit - 1); ++position) {
                auto trial = sides;
                trial[side] = position;
                if (trial[2] <= trial[0] + 8 || trial[3] <= trial[1] + 8)
                    continue;
                const float support = seedSupport(gx, gy, aw, ah, trial, threshold, side)
                    - std::abs(original - position) * 0.002f;
                if (support > bestS.first + 0.05f)
                    bestS = {support, position};
            }
            sides[side] = bestS.second;
        }
        float support = 0.0f;
        for (int side = 0; side < 4; ++side)
            support += seedSupport(gx, gy, aw, ah, sides, threshold, side);
        support /= 4.0f;
        const bool snapped = support >= 0.45f && sides != originalSides;
        QRect refined = clamped;
        if (snapped) {
            const int left = back(sides[0], true);
            const int top = back(sides[1], false);
            const int right = back(sides[2], true);
            const int bottom = back(sides[3], false);
            refined = clampRect(QRect(left, top, std::max(right - left, 1), std::max(bottom - top, 1)), w, h);
        }
        candidates.push_back({refined, support >= 0.45f ? 0.92f : 0.5f + support * 0.4f, true});
    }

    // 7. 四线枚举组合候选
    const int minW = aw / 12;
    const int minH = ah / 12;
    for (const auto& top : hLines) {
        for (const auto& bottom : hLines) {
            if (bottom.position <= top.position + minH)
                continue;
            for (const auto& left : vLines) {
                for (const auto& right : vLines) {
                    if (right.position <= left.position + minW)
                        continue;
                    if (top.position == 0 || left.position == 0)
                        continue;
                    const float sTop = sideSupport(gy, aw, ah, top.position, left.position, right.position, threshold);
                    const float sBottom = sideSupport(gy, aw, ah, bottom.position, left.position, right.position, threshold);
                    const float sLeft = sideSupportVertical(gx, aw, ah, left.position, top.position, bottom.position, threshold);
                    const float sRight = sideSupportVertical(gx, aw, ah, right.position, top.position, bottom.position, threshold);
                    const float weakest = std::min({sTop, sBottom, sLeft, sRight});
                    const float average = (sTop + sBottom + sLeft + sRight) / 4.0f;
                    if (weakest < 0.55f || average < 0.68f)
                        continue;
                    const QRect rect = analysisToRect(left.position, top.position, right.position, bottom.position, aw, ah, w, h);
                    if (rect.width() < w / 40 || rect.height() < h / 40
                        || rect.width() > w * 995 / 1000 || rect.height() > h * 995 / 1000)
                        continue;
                    candidates.push_back({rect, std::min(0.5f + average * 0.45f, 0.95f), false});
                }
            }
        }
    }

    // 8. 纯色连通区域候选
    auto flat = flatRegions(rgb, aw, ah, w, h);
    candidates.insert(candidates.end(), flat.begin(), flat.end());

    // 9. 非 seed 候选在原分辨率上精修
    const int refineRadius = int(std::ceil(float(w) / float(aw))) + 2;
    for (auto& c : candidates) {
        if (!c.seed)
            c.rect = refineRect(image, c.rect, refineRadius);
    }

    // 10. IoU 去重
    std::vector<SmartCandidate> deduped;
    for (const auto& c : candidates) {
        bool merged = false;
        for (auto& existing : deduped) {
            if (iou(existing.rect, c.rect) > 0.92) {
                existing.seed = existing.seed || c.seed;
                if (c.quality > existing.quality) {
                    existing.rect = c.rect;
                    existing.quality = c.quality;
                }
                merged = true;
                break;
            }
        }
        if (!merged)
            deduped.push_back(c);
    }

    // 11. 面积升序稳定排序 + 截断
    std::stable_sort(deduped.begin(), deduped.end(),
                     [](const SmartCandidate& a, const SmartCandidate& b) {
                         return rectArea(a.rect) < rectArea(b.rect);
                     });
    if (deduped.size() > 512)
        deduped.resize(512);
    return deduped;
}

QRect smartAt(const std::vector<SmartCandidate>& candidates, const QPointF& p, const QRect& fallback)
{
    const SmartCandidate* best = nullptr;
    for (const auto& c : candidates) {
        if (!containsPoint(c.rect, p))
            continue;
        if (!c.seed && c.quality < 0.52f)
            continue;
        if (!best || candidateRank(c) < candidateRank(*best))
            best = &c;
    }
    return best ? best->rect : fallback;
}

QRect smartStep(const std::vector<SmartCandidate>& candidates, const QPointF& p,
                    const QRect& current, const QRect& bounds, int direction)
{
    if (direction == 0)
        return current;
    const QRect* chosen = nullptr;
    for (const auto& c : candidates) {
        if (!containsPoint(c.rect, p) || c.rect == current)
            continue;
        if (direction > 0) {
            // 扩大：包含 current 的最小父区域
            if (!containsRect(c.rect, current))
                continue;
            if (!chosen || rectArea(c.rect) < rectArea(*chosen))
                chosen = &c.rect;
        } else {
            // 缩小：被 current 包含的最大子区域
            if (!containsRect(current, c.rect))
                continue;
            if (!chosen || rectArea(c.rect) > rectArea(*chosen))
                chosen = &c.rect;
        }
    }
    if (direction > 0)
        return chosen ? *chosen : bounds;
    return chosen ? *chosen : current;
}

QRect SmartCursor::hover(const std::vector<SmartCandidate>& candidates, const QPointF& p,
                             const QRect& current, const QRect& bounds)
{
    const bool inside = containsPoint(current, p);
    if (m_manual && inside && m_hasAnchor
        && std::hypot(p.x() - m_anchor.x(), p.y() - m_anchor.y()) <= 12.0) {
        return current; // 迟滞：保持滚轮选定的层级（anchor 不更新，缓慢漂移仍保持）
    }
    m_manual = false;
    m_anchor = p;
    m_hasAnchor = true;
    return smartAt(candidates, p, bounds);
}

QRect SmartCursor::step(const std::vector<SmartCandidate>& candidates, const QPointF& p,
                            const QRect& current, const QRect& bounds, int direction)
{
    m_manual = true;
    m_anchor = p;
    m_hasAnchor = true;
    return smartStep(candidates, p, current, bounds, direction);
}

} // namespace fouryshot::core
