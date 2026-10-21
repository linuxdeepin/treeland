---
name: treeland-build
description: "触发场景：配置/编译/安装 treeland、`cmake --preset`、`CMAKE_BUILD_TYPE=Debug`、`CMAKE_INSTALL_PREFIX=/usr`、`cmake --install`、`DESTDIR`、构建选项（`WITH_SUBMODULE_WAYLIB`、`BUILD_TREELAND_PROTOCOL_TESTS`、`BUILD_TREELAND_DEBUG`、`ADDRESS_SANITIZER`、`TREELAND_INSTALL_DEV`）、依赖缺失、`-Werror` 编译失败。"
---

# 构建与安装

> **铁律**：装到系统时 `-DCMAKE_INSTALL_PREFIX=/usr` 必须在 **configure** 阶段给。systemd unit 目录由 pkg-config 直接解析成绝对路径（`/usr/lib/systemd/system`、`/usr/lib/systemd/user`），**不受前缀影响**；不给 `/usr` 就会出现二进制装到 `/usr/local/bin`、unit 却落在 `/usr/lib/systemd/system` 的混装。`cmake --install --prefix` 也改不动这些绝对路径。

## 前置依赖
- CMake ≥ 3.25（`cmake_minimum_required(VERSION 3.25.0)`）
- C++20、C23；GCC 或 Clang
- Ninja（`CMakePresets.json` 里的 preset 都用它）
- pkg-config
- Qt ≥ 6.8（Core/Gui/Qml/Quick/QuickControls2/RemoteObjects/Network/DBus/Test…）、Dtk6、wlroots 0.20、外部 CMake 包 `TreelandProtocols`
- 完整清单以 `debian/control` 为准；`apt build-dep .` 一次装齐

## 配置
preset 只固定生成器与 `binaryDir`（`${sourceDir}/build`），**没有** `CMAKE_BUILD_TYPE`，也没有安装前缀，所以这两项要用 `-D` 补上；`-D` 可以和 `--preset` 同时用。

```bash
# 日常开发：对齐 CI 的 -Wall -Wextra -Werror，提前发现会挂 CI 的告警
cmake --preset ci -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr

# 调试运行（断点、符号、treeland-debug 开箱可用）
cmake --preset default -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr

# 只想编译不装到系统
cmake --preset default -DCMAKE_BUILD_TYPE=Debug
```

| preset | 生成器 | 说明 |
|--------|--------|------|
| `default` | Ninja | 默认配置 |
| `clang` | Ninja | 用 clang/clang++ |
| `ci` | Ninja | 加 `-Wall -Wextra -Werror`（另带两个 `-Wno-*`），CI 同款 |
| `ci-clang` | Ninja | clang + `-Werror` |

`CMAKE_BUILD_TYPE=Debug` 的副作用（细节见 `treeland-debug` skill）：

- 定义 `ALWAYS_ENABLE_TREELAND_DEBUG`：调试 Remote Object 默认开启，不需要改 DConfig
- 定义 `TREELAND_DEBUG_DEV_BUILD`：socket 名变成 `org.deepin.dde.treeland.debug-dev`，`treeland-debug` 免 polkit 提权
- 不传 `CMAKE_BUILD_TYPE` 时既不是 Debug 也不是 Release，上面前两个宏都不会定义

## 常用配置选项
| 选项 | 默认 | 说明 |
|------|------|------|
| `WITH_SUBMODULE_WAYLIB` | `ON` | `ON` 用仓库内的 `waylib/` 源码；`OFF` 用系统已安装的 waylib |
| `BUILD_TREELAND_PROTOCOL_TESTS` | `ON` | `tests/protocols/` 整棵树的开关 |
| `BUILD_TREELAND_DEBUG` | `ON` | `treeland-debug` 客户端（定义在 `tools/treeland-debug/CMakeLists.txt`） |
| `TREELAND_DEBUG_SOURCE` | `ON` | Debug 构建是否默认开调试 Remote Object |
| `ADDRESS_SANITIZER` | `OFF` | 加 `-fsanitize=address`；报告去向由 `ASAN_OPTIONS` 决定，本地直跑在 stderr，通过 `treeland.service` 启动才写到 `/tmp/treeland-asan*` |
| `BUILD_TREELAND_EXAMPLES` | `OFF` | 示例客户端 |
| `TREELAND_INSTALL_DEV` | `OFF` | 装开发文件（头文件、CMake config、`.so` 链接） |
| `DISABLE_DDM` | `OFF` | 关掉 DDM 与 greeter |
| `EXT_SESSION_LOCK_V1` | `ON` | 用 ext-session-lock 作为 DDM 之外的锁屏方案 |
| `USE_BUILD_TREE_WALLPAPER_FACTORY` | `OFF` | 直接用构建树里的 wallpaper factory |

`WITH_SUBMODULE_WAYLIB` 的名字是历史遗留：`waylib/` 是直接提交在本仓库里的源码，不是 git submodule（没有 `.gitmodules`），`git clone --recursive` 对它是空操作。

## 构建
```bash
cmake --build build
cmake --build build --parallel $(nproc)
cmake --build build --target treeland              # 只编主程序
cmake --build build --target treeland-debug        # 只编调试客户端
```

`cmake --build` 自动调用底层生成器；`--parallel` 是 CMake 内置参数，不需要区分 Ninja/Make。

## 安装
```bash
sudo cmake --install build                                # 全量
sudo cmake --install build --component treeland-debug     # 只装调试客户端
DESTDIR=/tmp/stage cmake --install build                  # 打包/沙箱，不碰系统
```

安装去向（`CMAKE_INSTALL_PREFIX=/usr` 时）：

| 内容 | 路径（`CMAKE_INSTALL_PREFIX=/usr`） |
|------|------|
| `treeland`、`treeland.sh` | `/usr/bin` |
| `treeland-sd`、`treeland-xwayland` | `/usr/libexec` |
| `libtreeland`、插件 | `${CMAKE_INSTALL_LIBDIR}`（Debian 上为 `/usr/lib/<triplet>`）、`${CMAKE_INSTALL_LIBDIR}/treeland/plugins` |
| systemd system unit（`treeland.service`） | `/usr/lib/systemd/system`（pkg-config 绝对路径） |
| systemd user unit（session-helper、screensaver） | `/usr/lib/systemd/user`（pkg-config 绝对路径） |
| 系统配置 | `/etc/treeland`（`CMAKE_INSTALL_FULL_SYSCONFDIR` 绝对路径） |
| DConfig 描述文件 | `/usr/share/dsg/configs/org.deepin.dde.treeland` |
| polkit 策略、D-Bus 策略 | `/usr/share/polkit-1/actions`、`/usr/share/dbus-1/system.d` |
| wayland-sessions | `/usr/share/wayland-sessions` |

`TREELAND_INSTALL_DEV=OFF`（默认）时 `libtreeland` 不装 `.so` 链接、头文件与 CMake config；需要给别人链接才开它。

systemd unit 目录和 `/etc/treeland` 在 configure 阶段就被解析成绝对路径，`cmake --install --prefix` 改不动它们 —— 这就是铁律的由来。

装完 systemd unit 后：

```bash
sudo systemctl daemon-reload
```

## 常见问题
1. `find_package(TreelandProtocols)` 失败：它不会被自动发现，必须装包，或用 `-DTreelandProtocols_DIR=<dir>` 指向它的 CMake 配置目录。CMake 按 `/usr/local` → `/usr` 的顺序搜索，`/usr/local` 下残留的旧 config 会遮蔽 `/usr` 下的正确版本；此时清掉缓存项重新 configure（`cmake -S . -B build -U TreelandProtocols_DIR`）。
2. 装完发现二进制在 `/usr/local/bin` 而 unit 在 `/usr/lib/systemd/system`：前缀给晚了。systemd 目录是 pkg-config 的绝对路径，跟前缀无关，重来一次 configure（见铁律）。
3. 新增目录或改了 `CMakeLists.txt` 后新 target 不出现：重新 configure，`cmake --build` 不会重新生成工程。
4. `-Werror` 报错但不确定是不是自己引入的：`cmake --preset ci` 复现；只用 `default` 编译会漏掉这类告警。
5. 运行的是刚装的版本还是构建树里的版本：构建树里跑 `./build/src/treeland.sh`，装好的走 `treeland.sh`，两者调试 socket 与日志去向不同，见 `treeland-debug` skill。
