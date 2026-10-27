#pragma once

namespace fouryshot::platform {

// Wayland 的核心协议不允许客户端控制窗口堆叠顺序，KWin 因此会忽略
// Qt::WindowStaysOnTopHint，贴图窗口会被其他普通窗口盖住，失去置顶意义。
//
// 这里通过 org.kde.kwin.Scripting D-Bus 接口安装一个小型 KWin 脚本，让 KWin
// 把本程序的所有窗口标记为 "keep above"（固定在普通窗口之上），并对之后新建
// 的窗口持续生效。非 KDE Wayland 环境下自动退化为空操作（X11 下 Qt 的置顶
// 提示本身有效，无需处理）。
//
// 安装是幂等的：脚本内容未变时不会重复加载，因此多个 4YShot 进程（常驻
// daemon + 短命 --capture/--pin 客户端）可以安全共存。
void installKeepAboveHandler();

// 显式卸载脚本。普通退出不会调用它：脚本只作用于本程序的 appId，让它在会话
// 内常驻既无副作用，又能避免某个短命客户端退出时把 daemon 的置顶能力一并
// 撤掉。该函数供测试或卸载清理场景使用。
void removeKeepAboveHandler();

} // namespace fouryshot::platform
