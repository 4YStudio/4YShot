#!/usr/bin/env bash
#
# 4YShot 一键构建 + 打包脚本
#
# 用法：
#   ./package.sh          构建并生成 DEB 包（等价于 ./package.sh build）
#   ./package.sh install  构建、打包、用 apt 安装生成的 DEB
#   ./package.sh probe    构建并运行 --probe-capture 自检 KWin 截图链路
#   ./package.sh clean    删除 build 目录
#   ./package.sh help     显示本帮助
#
# 环境变量：
#   BUILD_TYPE  构建类型，默认 Release（可选 Debug）
#   JOBS        并行编译线程数，默认取 CPU 核数
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"
ACTION="${1:-build}"

die() { echo -e "\033[31m错误：$*\033[0m" >&2; exit 1; }
info() { echo -e "\033[36m==> $*\033[0m"; }
ok()   { echo -e "\033[32m==> $*\033[0m"; }

usage() {
    cat <<'EOF'
4YShot 一键构建 + 打包脚本

用法：
  ./package.sh          构建并生成 DEB 包（等价于 ./package.sh build）
  ./package.sh install  构建、打包、用 apt 安装生成的 DEB
  ./package.sh probe    构建并运行 --probe-capture 自检 KWin 截图链路
  ./package.sh clean    删除 build 目录
  ./package.sh help     显示本帮助

环境变量：
  BUILD_TYPE  构建类型，默认 Release（可选 Debug）
  JOBS        并行编译线程数，默认取 CPU 核数
EOF
}

ensure_tools() {
    local missing=()
    for tool in cmake ninja cpack g++; do
        command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        die "缺少构建工具：${missing[*]}，请先安装：\n    sudo apt install cmake ninja-build g++"
    fi
    local cmake_major cmake_minor
    cmake_major="$(cmake --version | head -1 | sed -E 's/.*version ([0-9]+)\..*/\1/')"
    cmake_minor="$(cmake --version | head -1 | sed -E 's/.*version [0-9]+\.([0-9]+)\..*/\1/')"
    if (( cmake_major < 3 || (cmake_major == 3 && cmake_minor < 20) )); then
        die "CMake 版本过低（需要 ≥ 3.20），当前：$(cmake --version | head -1)"
    fi
}

configure() {
    info "配置 CMake（${BUILD_TYPE}）..."
    cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
}

build() {
    info "编译中（${JOBS} 线程）..."
    cmake --build "$BUILD_DIR" -j"$JOBS"
    ok "编译完成：$BUILD_DIR/4yshot"
}

package() {
    info "生成 DEB 包..."
    ( cd "$BUILD_DIR" && cpack -G DEB )
    local deb
    deb="$(find "$BUILD_DIR" -maxdepth 1 -name '4yshot_*.deb' -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)"
    [[ -n "$deb" ]] || die "未找到生成的 DEB 包"
    ok "DEB 包已生成：$(realpath "$deb")"
    echo "安装：sudo apt install ./$(realpath --relative-to=. "$deb")"
}

probe() {
    info "运行 KWin 截图链路自检..."
    "$BUILD_DIR/4yshot" --probe-capture
}

do_clean() {
    info "删除 $BUILD_DIR ..."
    rm -rf "$BUILD_DIR"
    echo "已清理。"
}

case "$ACTION" in
    build)
        ensure_tools
        configure
        build
        package
        ;;
    install)
        ensure_tools
        configure
        build
        package
        local deb
        deb="$(find "$BUILD_DIR" -maxdepth 1 -name '4yshot_*.deb' -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)"
        [[ -n "$deb" ]] || die "未找到生成的 DEB 包"
        info "用 apt 安装 $deb ..."
        sudo apt install -y "./$deb"
        ok "安装完成，运行 4yshot 即可（安装版已自带 KWin 截图授权）。"
        ;;
    probe)
        ensure_tools
        configure
        build
        probe
        ;;
    clean)
        do_clean
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        die "未知操作：$ACTION（可用：build / install / probe / clean / help）"
        ;;
esac