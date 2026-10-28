#include "ocr_engine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <future>
#include <mutex>
#include <optional>
#include <thread>
#include <onnxruntime_cxx_api.h>

namespace fouryshot::core {

namespace {

constexpr float kDetThresh = 0.3f;        // DB prob-map binarization threshold
constexpr float kDetBoxThresh = 0.5f;     // mean prob inside a detected box
constexpr float kDetUnclipRatio = 1.6f;   // box expansion ratio
constexpr int kDetLimitSide = 960;        // det input long-side limit
constexpr int kDetMinBoxSize = 3;         // discard boxes smaller than this (px, resized space)
constexpr bool kDetUseDilation = true;    // one 3x3 max dilation merges close strokes

constexpr int kRecHeight = 48;            // rec input height
constexpr int kRecMaxWidth = 800;         // rec input width cap (long lines)
constexpr float kClsThresh = 0.5f;        // 180-degree rotation threshold
constexpr float kLineConfThresh = 0.3f;   // discard junk lines below this confidence

bool isWideChar(QChar ch) {
    const ushort u = ch.unicode();
    return (u >= 0x1100 && u <= 0x115F)   // Hangul Jamo
        || (u >= 0x2E80 && u <= 0x303F)   // CJK radicals, Kangxi, CJK punct
        || (u >= 0x3040 && u <= 0x318F)   // Hiragana/Katakana/Bopomofo
        || (u >= 0x3200 && u <= 0x4DBF)   // CJK compat, extensions
        || (u >= 0x4E00 && u <= 0x9FFF)   // CJK unified
        || (u >= 0xA960 && u <= 0xA97F)   // Hangul Jamo Extended-A
        || (u >= 0xAC00 && u <= 0xD7FF)   // Hangul syllables
        || (u >= 0xF900 && u <= 0xFAFF)   // CJK compat ideographs
        || (u >= 0xFE10 && u <= 0xFE6F)   // vertical forms, small forms
        || (u >= 0xFF00 && u <= 0xFF60)   // full-width forms
        || (u >= 0xFFE0 && u <= 0xFFE6)
        || (u >= 0x1F300 && u <= 0x1FAFF) // emoji-ish (render wide)
        || (u >= 0x20000);                // CJK ext B+
}

} // namespace

bool isWideOcrChar(QChar ch) {
    return isWideChar(ch);
}

struct OcrEngine::Impl {
    std::optional<Ort::Env> env;
    std::optional<Ort::Session> det;
    std::optional<Ort::Session> cls;
    std::optional<Ort::Session> rec;
    bool hasCls = false;

    std::string detIn, detOut;
    std::string recIn, recOut;
    std::string clsIn, clsOut;

    // Character list for CTC decode: index 0 = blank, then dict chars,
    // then space (use_space_char=true, PaddleOCR convention).
    QStringList dict;
    int recClasses = 0;

    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    bool init(const QString& modelsDir);
    std::vector<QRect> detBoxes(const QImage& rgb);
    bool needRotate(const QImage& lineImg);
    // Returns recognized text and mean confidence of non-blank frames.
    QPair<QString, float> recognizeLine(const QImage& lineImg);
};

bool OcrEngine::Impl::init(const QString& modelsDir) {
    try {
        env.emplace(Ort::Env(ORT_LOGGING_LEVEL_WARNING, "4yshot-ocr"));
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(2);
        opts.SetInterOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        const QByteArray detPath = QDir(modelsDir).filePath("ch_PP-OCRv4_det_infer.onnx").toUtf8();
        const QByteArray recPath = QDir(modelsDir).filePath("ch_PP-OCRv4_rec_infer.onnx").toUtf8();
        const QByteArray clsPath = QDir(modelsDir).filePath("ch_ppocr_mobile_v2.0_cls_infer.onnx").toUtf8();
        const QByteArray dictPath = QDir(modelsDir).filePath("ppocr_keys_v1.txt").toUtf8();

        det.emplace(*env, detPath.constData(), opts);
        rec.emplace(*env, recPath.constData(), opts);

        if (QFile::exists(clsPath)) {
            cls.emplace(*env, clsPath.constData(), opts);
            hasCls = true;
        }

        Ort::AllocatorWithDefaultOptions alloc;
        detIn = det->GetInputNameAllocated(0, alloc).get();
        detOut = det->GetOutputNameAllocated(0, alloc).get();
        recIn = rec->GetInputNameAllocated(0, alloc).get();
        recOut = rec->GetOutputNameAllocated(0, alloc).get();
        if (hasCls) {
            clsIn = cls->GetInputNameAllocated(0, alloc).get();
            clsOut = cls->GetOutputNameAllocated(0, alloc).get();
        }

        // Dict: one character per line, file is UTF-8.
        QFile f(dictPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "[OcrEngine] cannot open dict" << dictPath;
            return false;
        }
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        while (!ts.atEnd()) {
            dict << ts.readLine();
        }
        f.close();
        // Trailing empty lines (if any) are not real characters.
        while (!dict.isEmpty() && dict.last().isEmpty()) dict.removeLast();

        // Validate charset size against the rec model output dimension.
        // NOTE: Debian's onnxruntime 1.21 has a broken session-metadata query
        // (GetInputTypeInfo/GetOutputTypeInfo -> GetShape reads garbage memory
        // and throws std::length_error). We warm up with a tiny dummy run so
        // the class count comes from the real output tensor. This also lazily
        // initializes recClasses before any parallel recognition happens.
        {
            QImage warm(64, 48, QImage::Format_RGB888);
            warm.fill(Qt::white);
            recognizeLine(warm);
        }
        const int expected = dict.size() + 2; // blank + dict + space
        if (recClasses > 0 && recClasses != expected) {
            qWarning() << "[OcrEngine] dict size mismatch: rec outputs" << recClasses
                       << "classes, dict expects" << expected;
            // Keep going: decode maps out-of-range indices to ''.
        }

        qInfo() << "[OcrEngine] ready: models from" << modelsDir
                << "dict" << dict.size() << "classes" << recClasses
                << "cls" << hasCls;
        return true;
    } catch (const Ort::Exception& e) {
        qWarning() << "[OcrEngine] init failed:" << e.what();
        return false;
    } catch (const std::exception& e) {
        qWarning() << "[OcrEngine] init failed:" << e.what();
        return false;
    }
}

// Run det on a single image (any size; downscaled to kDetLimitSide if
// needed). Returns boxes in the image's own coordinate space.
std::vector<QRect> detRun(const Ort::MemoryInfo& memInfo,
                          Ort::Session& det,
                          const std::string& detIn, const std::string& detOut,
                          const QImage& rgb, bool debug);

std::vector<QRect> OcrEngine::Impl::detBoxes(const QImage& rgb) {
    const int srcW = rgb.width();
    const int srcH = rgb.height();
    const bool debug = qEnvironmentVariableIsSet("FOURYSHOT_OCR_DEBUG");

    // Small/medium images: single det pass.
    if (std::max(srcW, srcH) <= kDetLimitSide) {
        return detRun(memInfo, *det, detIn, detOut, rgb, debug);
    }

    // Large screenshots: downscaling the whole image to kDetLimitSide makes
    // UI text unreadable for the detector. Instead, run det on overlapping
    // kDetLimitSide x kDetLimitSide tiles (in parallel) and merge the boxes.
    constexpr int kTileOverlap = 96;
    const int stride = kDetLimitSide - kTileOverlap;
    struct TileJob { QImage img; int tx, ty; };
    std::vector<TileJob> jobs;
    for (int ty = 0; ty < srcH; ty += stride) {
        for (int tx = 0; tx < srcW; tx += stride) {
            const int tw = std::min(kDetLimitSide, srcW - tx);
            const int th = std::min(kDetLimitSide, srcH - ty);
            if (tw < 32 || th < 32) break; // remainders are covered by the previous tile's overlap
            jobs.push_back({rgb.copy(tx, ty, tw, th), tx, ty});
        }
    }

    unsigned hw = std::thread::hardware_concurrency();
    const int maxWorkers = std::max(1u, std::min(hw, 8u));
    std::vector<QRect> all;
    std::vector<std::future<std::vector<QRect>>> futs;
    futs.reserve(jobs.size());
    for (const auto& j : jobs) {
        // Limit in-flight tiles to maxWorkers to bound memory.
        if (static_cast<int>(futs.size()) >= maxWorkers) {
            for (auto& f : futs) {
                for (QRect r : f.get()) all.push_back(r);
            }
            futs.clear();
        }
        futs.push_back(std::async(std::launch::async, [&j, this, debug]() {
            std::vector<QRect> res = detRun(memInfo, *det, detIn, detOut, j.img, debug);
            for (QRect& r : res) r.translate(j.tx, j.ty);
            return res;
        }));
    }
    for (auto& f : futs) {
        for (QRect r : f.get()) all.push_back(r);
    }
    futs.clear();

    // Merge boxes that overlap across tile seams (containment or IoU > 0.3).
    std::sort(all.begin(), all.end(), [](const QRect& a, const QRect& b) {
        if (a.top() != b.top()) return a.top() < b.top();
        return a.left() < b.left();
    });
    std::vector<QRect> merged;
    for (const QRect& r : all) {
        bool absorbed = false;
        for (QRect& m : merged) {
            const QRect inter = m.intersected(r);
            if (!inter.isValid()) continue;
            const int interArea = inter.width() * inter.height();
            const int rArea = r.width() * r.height();
            if (interArea * 10 >= rArea * 3) {
                m = m.united(r);
                absorbed = true;
                break;
            }
        }
        if (!absorbed) merged.push_back(r);
    }
    if (debug) {
        qInfo() << "[OcrEngine][det] tiled" << srcW << "x" << srcH
                << "-> raw" << all.size() << "merged" << merged.size();
    }
    return merged;
}

std::vector<QRect> detRun(const Ort::MemoryInfo& memInfo,
                          Ort::Session& det,
                          const std::string& detIn, const std::string& detOut,
                          const QImage& rgb, bool debug) {
    const int srcW = rgb.width();
    const int srcH = rgb.height();

    // 1. Resize: long side <= kDetLimitSide, both sides multiple of 32.
    float ratio = 1.0f;
    int rw = srcW, rh = srcH;
    const int maxSide = std::max(srcW, srcH);
    if (maxSide > kDetLimitSide) {
        ratio = static_cast<float>(kDetLimitSide) / maxSide;
        rw = std::round(srcW * ratio);
        rh = std::round(srcH * ratio);
    }
    rw = std::max(32, (rw + 31) / 32 * 32);
    rh = std::max(32, (rh + 31) / 32 * 32);

    QImage resized = rgb.scaled(rw, rh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // 2. Build CHW float tensor with ImageNet mean/std normalization.
    std::vector<float> data(3 * static_cast<size_t>(rw) * rh);
    const int plane = rw * rh;
    float* pR = data.data();
    float* pG = pR + plane;
    float* pB = pG + plane;
    for (int y = 0; y < rh; ++y) {
        const uchar* line = resized.constScanLine(y);
        for (int x = 0; x < rw; ++x) {
            const int idx = y * rw + x;
            pR[idx] = (line[x * 3 + 0] / 255.0f - 0.485f) / 0.229f;
            pG[idx] = (line[x * 3 + 1] / 255.0f - 0.456f) / 0.224f;
            pB[idx] = (line[x * 3 + 2] / 255.0f - 0.406f) / 0.225f;
        }
    }

    std::array<int64_t, 4> inShape{1, 3, static_cast<int64_t>(rh), static_cast<int64_t>(rw)};
    Ort::Value input = Ort::Value::CreateTensor<float>(
        memInfo, data.data(), data.size(), inShape.data(), 4);

    const char* ins[] = {detIn.c_str()};
    const char* outs[] = {detOut.c_str()};
    auto outputs = det.Run(Ort::RunOptions{nullptr}, ins, &input, 1, outs, 1);
    if (outputs.empty() || !outputs[0].IsTensor()) return {};

    auto pinfo = outputs[0].GetTensorTypeAndShapeInfo();
    auto pshape = pinfo.GetShape(); // {1,1,ph,pw}
    if (pshape.size() != 4) return {};
    const int ph = static_cast<int>(pshape[2]);
    const int pw = static_cast<int>(pshape[3]);
    const float* prob = outputs[0].GetTensorData<float>();

    if (debug) {
        float maxProb = 0.0f;
        for (int i = 0; i < ph * pw; ++i) maxProb = std::max(maxProb, prob[i]);
        qInfo() << "[OcrEngine][det] ph/pw" << ph << pw << "maxProb" << maxProb;
    }

    // 3. Binarize + dilate.
    std::vector<uint8_t> bin(ph * pw, 0), tmp(ph * pw, 0);
    for (int i = 0; i < ph * pw; ++i) bin[i] = prob[i] > kDetThresh ? 1 : 0;
    if (kDetUseDilation) {
        for (int y = 0; y < ph; ++y) {
            for (int x = 0; x < pw; ++x) {
                uint8_t m = 0;
                for (int dy = -1; dy <= 1 && !m; ++dy) {
                    const int yy = y + dy;
                    if (yy < 0 || yy >= ph) continue;
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int xx = x + dx;
                        if (xx < 0 || xx >= pw) continue;
                        if (bin[yy * pw + xx]) { m = 1; break; }
                    }
                }
                tmp[y * pw + x] = m;
            }
        }
        bin.swap(tmp);
    }

    // 4. Connected components (8-connectivity, iterative flood fill).
    if (debug) {
        long bins = 0;
        for (int i = 0; i < ph * pw; ++i) bins += bin[i];
        qInfo() << "[OcrEngine][det] bin pixels" << bins << "/" << ph * pw;
    }
    struct Comp {
        int x0, y0, x1, y1;
        long count = 0;
        double probSum = 0.0;
    };
    std::vector<int32_t> label(ph * pw, -1);
    std::vector<Comp> comps;
    std::vector<int32_t> stack;
    for (int start = 0; start < ph * pw; ++start) {
        if (!bin[start] || label[start] != -1) continue;
        const int id = static_cast<int>(comps.size());
        comps.emplace_back();
        Comp& c = comps.back();
        c.x0 = c.x1 = start % pw;
        c.y0 = c.y1 = start / pw;
        stack.clear();
        stack.push_back(start);
        label[start] = id;
        while (!stack.empty()) {
            const int p = stack.back();
            stack.pop_back();
            const int px = p % pw, py = p / pw;
            c.count++;
            c.probSum += prob[p];
            c.x0 = std::min(c.x0, px);
            c.y0 = std::min(c.y0, py);
            c.x1 = std::max(c.x1, px);
            c.y1 = std::max(c.y1, py);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    const int nx = px + dx, ny = py + dy;
                    if (nx < 0 || nx >= pw || ny < 0 || ny >= ph) continue;
                    const int np = ny * pw + nx;
                    if (bin[np] && label[np] == -1) {
                        label[np] = id;
                        stack.push_back(np);
                    }
                }
            }
        }
    }

    // 5. Filter + unclip + map back to source pixels.
    const float sx = srcW / static_cast<float>(pw);
    const float sy = srcH / static_cast<float>(ph);
    std::vector<QRect> out;
    out.reserve(comps.size());
    int dropSmall = 0, dropScore = 0, dropTiny = 0, maxCompH = 0;
    float maxCompScore = 0.0f;
    for (const auto& c : comps) {
        const int w = c.x1 - c.x0 + 1;
        const int h = c.y1 - c.y0 + 1;
        maxCompH = std::max(maxCompH, h);
        if (w < kDetMinBoxSize || h < kDetMinBoxSize) { dropSmall++; continue; }
        if (c.count <= 0) continue;
        const double score = c.probSum / c.count;
        maxCompScore = std::max(maxCompScore, static_cast<float>(score));
        if (score < kDetBoxThresh) { dropScore++; continue; }

        // Unclip: expand each edge by area*unclip/perimeter.
        const double area = static_cast<double>(w) * h;
        const double perim = 2.0 * (w + h);
        const double d = area * kDetUnclipRatio / perim;

        float fx0 = (c.x0 - d) * sx;
        float fy0 = (c.y0 - d) * sy;
        float fx1 = (c.x1 + 1 + d) * sx;
        float fy1 = (c.y1 + 1 + d) * sy;
        int x0 = std::clamp(static_cast<int>(std::floor(fx0)), 0, srcW - 1);
        int y0 = std::clamp(static_cast<int>(std::floor(fy0)), 0, srcH - 1);
        int x1 = std::clamp(static_cast<int>(std::ceil(fx1)), 0, srcW);
        int y1 = std::clamp(static_cast<int>(std::ceil(fy1)), 0, srcH);
        if (x1 - x0 < 2 || y1 - y0 < 2) { dropTiny++; continue; }
        out.emplace_back(x0, y0, x1 - x0, y1 - y0);
    }
    if (debug) {
        qInfo() << "[OcrEngine][det] comps" << comps.size() << "maxH" << maxCompH
                << "maxScore" << maxCompScore << "out" << out.size()
                << "drop small/score/tiny" << dropSmall << dropScore << dropTiny;
    }
    return out;
}

bool OcrEngine::Impl::needRotate(const QImage& lineImg) {
    if (!hasCls) return false;

    // Resize to 48x192 keeping aspect ratio (pad right with background 0
    // after normalization = gray 127.5 -> 0).
    QImage resized = lineImg.scaled(192, 48, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    resized = resized.copy((resized.width() - 192) / 2, 0, 192, 48);

    std::vector<float> data(3 * 48 * 192);
    const int plane = 48 * 192;
    float* pR = data.data();
    float* pG = pR + plane;
    float* pB = pG + plane;
    for (int y = 0; y < 48; ++y) {
        const uchar* line = resized.constScanLine(y);
        for (int x = 0; x < 192; ++x) {
            const int idx = y * 192 + x;
            pR[idx] = (line[x * 3 + 0] / 255.0f - 0.5f) / 0.5f;
            pG[idx] = (line[x * 3 + 1] / 255.0f - 0.5f) / 0.5f;
            pB[idx] = (line[x * 3 + 2] / 255.0f - 0.5f) / 0.5f;
        }
    }

    try {
        std::array<int64_t, 4> inShape{1, 3, 48, 192};
        Ort::Value input = Ort::Value::CreateTensor<float>(
            memInfo, data.data(), data.size(), inShape.data(), 4);
        const char* ins[] = {clsIn.c_str()};
        const char* outs[] = {clsOut.c_str()};
        auto outputs = cls->Run(Ort::RunOptions{nullptr}, ins, &input, 1, outs, 1);
        if (outputs.empty() || !outputs[0].IsTensor()) return false;
        auto oinfo = outputs[0].GetTensorTypeAndShapeInfo();
        auto oshape = oinfo.GetShape();
        if (oshape.empty() || oshape.back() < 2) return false;
        const float* logits = outputs[0].GetTensorData<float>();
        // softmax
        const float e0 = std::exp(logits[0]), e1 = std::exp(logits[1]);
        const float p1 = e1 / (e0 + e1);
        return p1 > kClsThresh;
    } catch (const Ort::Exception&) {
        return false;
    }
}

QPair<QString, float> OcrEngine::Impl::recognizeLine(const QImage& lineImg) {
    // Resize to height 48 keeping aspect ratio.
    int w = std::round(static_cast<float>(lineImg.width()) / lineImg.height() * kRecHeight);
    w = std::clamp(w, 2, kRecMaxWidth);
    QImage resized = lineImg.scaled(w, kRecHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    std::vector<float> data(3 * static_cast<size_t>(kRecHeight) * w);
    const int plane = kRecHeight * w;
    float* pR = data.data();
    float* pG = pR + plane;
    float* pB = pG + plane;
    for (int y = 0; y < kRecHeight; ++y) {
        const uchar* line = resized.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const int idx = y * w + x;
            pR[idx] = (line[x * 3 + 0] / 255.0f - 0.5f) / 0.5f;
            pG[idx] = (line[x * 3 + 1] / 255.0f - 0.5f) / 0.5f;
            pB[idx] = (line[x * 3 + 2] / 255.0f - 0.5f) / 0.5f;
        }
    }

    try {
        std::array<int64_t, 4> inShape{1, 3, kRecHeight, static_cast<int64_t>(w)};
        Ort::Value input = Ort::Value::CreateTensor<float>(
            memInfo, data.data(), data.size(), inShape.data(), 4);
        const char* ins[] = {recIn.c_str()};
        const char* outs[] = {recOut.c_str()};
        auto outputs = rec->Run(Ort::RunOptions{nullptr}, ins, &input, 1, outs, 1);
        if (outputs.empty() || !outputs[0].IsTensor()) return {QString(), 0.0f};

        auto oinfo = outputs[0].GetTensorTypeAndShapeInfo();
        auto oshape = oinfo.GetShape(); // {1, T, C}
        if (oshape.size() != 3) return {QString(), 0.0f};
        const int64_t T = oshape[1];
        const int64_t C = oshape[2];
        if (T <= 0 || C <= 0) return {QString(), 0.0f};
        // First-run charset validation (replaces broken session-metadata query).
        if (recClasses == 0) {
            recClasses = static_cast<int>(C);
            const int expected = dict.size() + 2; // blank + dict + space
            if (recClasses != expected) {
                qWarning() << "[OcrEngine] dict size mismatch: rec outputs" << recClasses
                           << "classes, dict expects" << expected;
            }
        }
        const float* logits = outputs[0].GetTensorData<float>();

        QString text;
        double confSum = 0.0;
        int confCount = 0;
        int prev = 0; // blank index 0
        for (int64_t t = 0; t < T; ++t) {
            const float* row = logits + t * C;
            int best = 0;
            float bestVal = row[0];
            for (int64_t c = 1; c < C; ++c) {
                if (row[c] > bestVal) { bestVal = row[c]; best = static_cast<int>(c); }
            }
            if (best != prev && best != 0) {
                // The rec model's output is already softmax probabilities
                // (rows sum to 1) — bestVal is directly the confidence.
                const float p = bestVal;
                if (best - 1 < dict.size()) {
                    text += dict.at(best - 1);
                } else if (best - 1 == dict.size()) {
                    text += QLatin1Char(' ');
                }
                confSum += p;
                confCount++;
            }
            prev = best;
        }
        return {text, confCount > 0 ? static_cast<float>(confSum / confCount) : 0.0f};
    } catch (const Ort::Exception&) {
        return {QString(), 0.0f};
    }
}

std::mutex OcrEngine::s_mutex;
std::unique_ptr<OcrEngine> OcrEngine::s_instance;

QString OcrEngine::findModelsDir() {
    const QStringList required = {
        "ch_PP-OCRv4_det_infer.onnx", "ch_PP-OCRv4_rec_infer.onnx", "ppocr_keys_v1.txt"};

    const QString envDir = qEnvironmentVariable("FOURYSHOT_MODELS_DIR");
    QStringList candidates;
    if (!envDir.isEmpty()) candidates << envDir;

    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << QDir(appDir).filePath("models");
    candidates << QDir(appDir).filePath("../share/4yshot/models");
    candidates << QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("models");

    for (const QString& dir : candidates) {
        bool ok = true;
        for (const QString& f : required) {
            if (!QFile::exists(QDir(dir).filePath(f))) { ok = false; break; }
        }
        if (ok) return dir;
    }
    return QString();
}

OcrEngine::OcrEngine() {
    const QString dir = findModelsDir();
    if (dir.isEmpty()) {
        qWarning() << "[OcrEngine] models not found (set FOURYSHOT_MODELS_DIR or install models)";
        return;
    }
    m_impl = std::make_unique<Impl>();
    m_valid = m_impl->init(dir);
}

OcrEngine::~OcrEngine() = default;

OcrEngine* OcrEngine::instance() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_instance) {
        s_instance = std::unique_ptr<OcrEngine>(new OcrEngine());
    }
    if (!s_instance->m_valid) return s_instance.get(); // valid==false, caller falls back
    return s_instance.get();
}

QVector<OcrTextLine> OcrEngine::recognize(const QImage& image) {
    QVector<OcrTextLine> out;
    if (!m_valid || !m_impl || image.isNull()) return out;

    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    if (rgb.width() < 4 || rgb.height() < 4) return out;

    // 1. Detect text line boxes.
    std::vector<QRect> boxes = m_impl->detBoxes(rgb);
    if (boxes.empty()) return out;

    // 2. Reading order: sort by top, group lines with vertical overlap,
    //    sort each group by left edge.
    std::sort(boxes.begin(), boxes.end(), [](const QRect& a, const QRect& b) {
        return a.top() < b.top();
    });
    std::vector<std::vector<QRect>> groups;
    for (const QRect& b : boxes) {
        bool placed = false;
        for (auto& g : groups) {
            // Overlap with group bounding rect
            QRect gb = g.front();
            for (const QRect& x : g) gb = gb.united(x);
            const int interH = std::min(gb.bottom(), b.bottom()) - std::max(gb.top(), b.top());
            const int minH = std::min(gb.height(), b.height());
            if (interH > 0 && interH * 10 >= minH * 4) {
                g.push_back(b);
                placed = true;
                break;
            }
        }
        if (!placed) groups.push_back({b});
    }
    boxes.clear();
    for (auto& g : groups) {
        std::sort(g.begin(), g.end(), [](const QRect& a, const QRect& b) {
            return a.left() < b.left();
        });
        for (const QRect& r : g) boxes.push_back(r);
    }

    // 3. Per line: optional 180-degree check, then recognize.
    // 3. Per line: optional 180-degree check, then recognize (in parallel;
    //    Ort::Run is thread-safe and each job only touches its own crops).
    struct LineJob { QRect crop; bool rotate; };
    std::vector<LineJob> lineJobs;
    for (const QRect& r : boxes) {
        const QRect crop = r.intersected(rgb.rect());
        if (crop.width() < 2 || crop.height() < 2) continue;
        lineJobs.push_back({crop, m_impl->needRotate(rgb.copy(crop))});
    }

    std::vector<QPair<QString, float>> results(lineJobs.size());
    {
        unsigned hw = std::thread::hardware_concurrency();
        const int maxWorkers = std::max(1u, std::min(hw, 8u));
        std::vector<std::future<void>> futs;
        futs.reserve(lineJobs.size());
        for (size_t i = 0; i < lineJobs.size(); ++i) {
            if (static_cast<int>(futs.size()) >= maxWorkers) {
                for (auto& f : futs) f.get();
                futs.clear();
            }
            futs.push_back(std::async(std::launch::async, [i, &lineJobs, &rgb, &results, this]() {
                QImage lineImg = rgb.copy(lineJobs[i].crop);
                if (lineJobs[i].rotate) lineImg = lineImg.mirrored(true, true);
                results[i] = m_impl->recognizeLine(lineImg);
            }));
        }
        for (auto& f : futs) f.get();
    }

    for (size_t i = 0; i < lineJobs.size(); ++i) {
        const QString& text = results[i].first;
        const float conf = results[i].second;
        if (text.trimmed().isEmpty()) continue;
        if (conf < kLineConfThresh) continue;

        OcrTextLine tl;
        tl.rect = lineJobs[i].crop;
        tl.text = text;
        tl.confidence = conf;
        out.push_back(tl);
    }
    return out;
}

} // namespace fouryshot::core
