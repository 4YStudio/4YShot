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
- KDE Spectacle；截图时需能调用 `spectacle` 命令
- Tesseract OCR 与简体中文、英文语言数据（使用 OCR 时需要）

在 Debian 13 上安装构建和运行所需组件：

```bash
sudo apt update
sudo apt install -y \
  cmake ninja-build g++ \
  qt6-base-dev qt6-base-dev-tools \
  libkf6globalaccel-dev \
  spectacle \
  tesseract-ocr tesseract-ocr-chi-sim tesseract-ocr-eng
```

`libkf6globalaccel-dev` 用于启用 KDE 全局快捷键集成；如果构建时没有找到该库，程序仍可构建，但该集成不会启用。其他系统上的包名可能不同。

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

遇到问题时，请提供发行版、桌面环境、显示服务器（Wayland 或 X11）、运行方式，以及终端中的错误信息。非 Debian 13 + KDE Plasma 6 + Wayland 环境的问题可能需要额外适配。
