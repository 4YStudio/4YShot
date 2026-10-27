# 4YShot 📸

4YShot 是一款面向 KDE Wayland 桌面的原生截图、标注与贴图工具，使用 C++、Qt 6 和 KDE/Wayland 桌面接口构建。

> **兼容性说明**：作者是在 **Debian 13（Trixie）+ KDE Plasma 6 + Wayland** 环境下开发和使用本项目的。其他发行版、桌面环境、窗口管理器或 X11 环境**不保证可以直接运行**。尤其截图功能依赖 KDE KWin 的 D-Bus 截图接口以及 `spectacle`，因此非 KDE/KWin 环境可能无法正常截图；其他环境需要自行验证和适配。

## 功能

- **屏幕截图**：启动后截取桌面并进入选区与标注界面；支持多显示器及混合缩放场景。
- **标注与编辑**：矩形、椭圆、直线、箭头、自由画笔、高亮笔、序号、文本，以及马赛克/模糊遮挡；支持撤销和重做。
- **像素放大镜与取色**：查看光标附近的像素细节、屏幕坐标及颜色值，并可快速复制颜色。
- **保存与复制**：将结果复制到剪贴板，或保存为图片。
- **桌面贴图**：将剪贴板图片作为置顶浮动窗口；支持拖动、缩放、透明度调整、折叠、复制和另存为。
- **离线 OCR**：使用 Tesseract 识别中文和英文，可查看或复制识别文本。需要安装 Tesseract 及对应语言数据。
- **后台常驻**：可运行托盘守护进程，通过命令或托盘菜单触发截图、贴图；检测到 KDE GlobalAccel 开发库时会启用全局快捷键支持。

## 运行环境和依赖

推荐且作者实际开发使用的环境：

- Debian 13 (Trixie)
- KDE Plasma 6
- Wayland 会话
- Qt 6.8 或更高版本
- KDE Spectacle（作为 KWin 接口不可用时的后备截图方案）
- wl-clipboard（复制图片的剪贴板持久化）
- Tesseract OCR 与简体中文、英文语言数据（使用 OCR 时需要）

首屏截图默认通过 KWin 的 `org.kde.KWin.ScreenShot2` 受限接口直接读取原始像素（毫秒级）；该接口需要授权（见下方「长截图的 KWin 授权说明」，构建或安装 DEB 包时会自动注册），未授权或失败时自动回退到 Spectacle 子进程方案。

在 Debian 13 上安装构建和运行所需组件：

```bash
sudo apt update
sudo apt install -y \
  cmake ninja-build g++ \
  qt6-base-dev qt6-base-dev-tools \
  libkf6globalaccel-dev \
  spectacle \
  wl-clipboard \
  tesseract-ocr tesseract-ocr-chi-sim tesseract-ocr-eng
```

`libkf6globalaccel-dev` 用于启用 KDE 全局快捷键集成；如果构建时没有找到该库，程序仍可构建，但该集成不会启用。`wl-clipboard` 提供复制图片的持久化支持（截图进程退出后剪贴板内容仍可粘贴），缺失时复制功能退化为依赖桌面剪贴板管理器。其他系统上的包名可能不同。

## 从源码构建

在项目根目录运行：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

编译产物位于 `build/4yshot`。启动程序默认进入截图模式：

```bash
./build/4yshot
```

### 长截图的 KWin 授权说明

滚动长截图通过 KWin 的受限 D-Bus 接口 `org.kde.KWin.ScreenShot2` 持续抓取选区。KWin 只授权给 desktop 文件中声明了 `X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2`、且 `Exec` 与进程可执行文件路径一致的应用：

- 每次执行 `cmake --build build` 后，构建系统会自动在 `~/.local/share/applications/4yshot-dev.desktop` 注册指向本次构建产物的授权条目（设了 `NoDisplay`，不会出现在应用菜单），开发版因此可以直接使用长截图；
- 通过 DEB 包安装的版本自带的 desktop 文件已包含授权声明，无需额外操作；
- 若长截图提示 “KWin 未授权本进程截图”，请重新构建并重启 4YShot；也可运行 `4yshot --probe-capture` 自检 KWin 区域截图链路，输出 `OK` 即正常。

## 贴图置顶说明

Wayland 的核心协议不提供任何让客户端控制窗口堆叠顺序的接口，KWin 会直接忽略 Qt 的“总在最前”窗口提示，因此贴图窗口默认会被其他普通窗口盖住。4YShot 在启动时通过 `org.kde.kwin.Scripting` D-Bus 接口注册一个小型 KWin 脚本，把本程序的窗口标记为“保持在其他窗口之上”（keep above），并对之后新建的贴图窗口持续生效。

- 脚本写在 `~/.local/share/kwin/scripts/4yshot-keep-above/`，只匹配本程序的 appId（`org.fouryshot.FourYShot`），不会影响其他程序；
- 该脚本不会开机自动加载，只在 4YShot 运行时由程序注册，本会话内常驻；若被意外卸载，重新触发一次截图或贴图即可恢复；
- X11 下无需这套机制（Qt 的置顶标志本身有效）；非 KDE 的 Wayland 合成器没有等价接口，贴图仍可能出现被遮挡的情况。

## 构建和安装 DEB 包

在 Debian 系系统上构建 `.deb`：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build
cpack -G DEB
```

生成的包名类似 `4yshot_1.0.0_amd64.deb`。使用 APT 安装（将文件名替换为实际生成的名称）：

```bash
sudo apt install ./4yshot_1.0.0_amd64.deb
```

包会安装命令行程序和桌面应用菜单项，并声明 Qt/KDE 动态库以及 Tesseract OCR 相关依赖。卸载：

```bash
sudo apt remove 4yshot
```

> DEB 包不会捆绑所有系统库，也不能保证在未验证的发行版或桌面环境中兼容。建议在目标系统上构建或测试软件包。

## 使用方式

```bash
4yshot              # 启动截图
4yshot --capture    # 启动截图
4yshot --pin        # 将剪贴板中的图片贴图到桌面
4yshot --daemon     # 启动后台托盘守护进程
4yshot --help       # 查看命令行选项
```

守护进程运行时，后续执行 `4yshot --capture` 或 `4yshot --pin` 会向已有进程发送请求。托盘菜单也可以触发截图、贴图或退出程序。全局快捷键的实际配置与 KDE Plasma 和 GlobalAccel 环境有关；也可以在 KDE「系统设置 → 快捷键」中自行设置命令。

## 主要截图操作

| 操作 | 功能 |
| --- | --- |
| `Enter` / `Ctrl+C` | 完成并复制截图 |
| `Ctrl+S` | 保存截图 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `Esc` / 鼠标右键 | 取消当前操作或退出截图 |
| `R` / `E` / `A` / `L` | 矩形 / 椭圆 / 箭头 / 直线工具 |
| `P` / `H` / `N` / `T` / `M` | 画笔 / 高亮 / 序号 / 文本 / 马赛克工具 |
| `C` | 放大镜模式下复制当前颜色值 |
| `F3` | 将截图或剪贴板图片贴图 |

贴图窗口支持左键拖动、滚轮缩放、`Alt`+滚轮调整透明度、双击折叠/还原；右键可打开操作菜单。

## 问题反馈

遇到问题时，请提供发行版、桌面环境、显示服务器（Wayland 或 X11）、运行方式，以及终端中的错误信息；截图类问题建议附上 `4yshot --probe-capture` 的输出。非 Debian 13 + KDE Plasma 6 + Wayland 环境的问题可能需要额外适配。
