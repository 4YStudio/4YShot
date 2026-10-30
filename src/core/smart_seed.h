#pragma once

#include <QObject>
#include <QRect>
#include <vector>

namespace fouryshot::core {

// 通过 KWin 脚本接口收集可见窗口几何，作为智能选区的 seed 候选。
// 对应 babry 的 compositor::window_regions()（合成器兜底窗口矩形）。
// 用法：SmartSeedCollector c; auto regions = c.collect(800);
class SmartSeedCollector : public QObject {
    Q_OBJECT
public:
    explicit SmartSeedCollector(QObject* parent = nullptr);
    ~SmartSeedCollector() override;

    // 阻塞收集（内部事件循环，直到 KWin 脚本回调完成或超时）。
    // 返回全局逻辑坐标的窗口矩形；失败返回空列表。
    std::vector<QRect> collect(int timeoutMs = 800);

    void addWindow(int x, int y, int width, int height);
    void windowsDone();

signals:
    void done();

private:
    std::vector<QRect> m_regions;
    bool m_done = false;
    QString m_service;
};

} // namespace fouryshot::core
