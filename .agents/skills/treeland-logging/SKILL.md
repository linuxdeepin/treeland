---
name: treeland-logging
description: "触发场景：treeland 或 waylib 代码中新增/修改/评审/删除日志、日志分类、`Q_LOGGING_CATEGORY`、`Q_DECLARE_LOGGING_CATEGORY`、`qCDebug`、`qCInfo`、`qCWarning`、`qCCritical`、日志级别选择、分类命名、清理日志输出。treeland 代码使用 `src/common/treelandlogging.h/.cpp` 中的集中式分类；waylib 代码使用 `waylib/src/server/wayliblogging.h/.cpp` 中的集中式分类。"
---

# 日志规范

> **铁律**：不得新增裸 `qDebug()`/`qWarning()`/`qCritical()`/`qInfo()`；新分类只允许出现在集中式 `*logging.h/.cpp` 中。

## 目标
让日志改动与集中式分类体系保持一致：

- **treeland**：集中式分类在 `src/common/treelandlogging.h/.cpp`
- **waylib**：集中式分类在 `waylib/src/server/wayliblogging.h/.cpp`

实现与评审工作都适用本 skill。

## 路由
按模块归属选择日志路径，不要凭个人偏好。

### Treeland 代码
使用集中式分类，声明在：

- `src/common/treelandlogging.h`
- `src/common/treelandlogging.cpp`

分类变量命名：`lcTl` + PascalCase 模块名（例如 `lcTlCore`、`lcTlInput`、`lcTlGestures`）。

字符串 ID 命名：`treeland.<module>[.<submodule>]`（例如 `treeland.core`、`treeland.input.gestures`）。

唯一例外是无法包含集中式头文件的独立可执行文件。目前只有两个：`treeland-xwayland`（`src/xwayland.cpp`，`lcXwaylandHelper`）与 `treeland-sd`（`src/systemd-socket.cpp`，`lcSdSocket`），它们可以在自己的 `.cpp` 里定义本地 `Q_LOGGING_CATEGORY`。其余可执行文件（`treeland-screensaver`、`treeland-debug`、`treeland-session-helper`）目前没有日志；若需要，就在各自文件里定义本地分类，不要拉入 `treelandlogging.h`。

### Waylib 代码
使用集中式分类，声明在：

- `waylib/src/server/wayliblogging.h`
- `waylib/src/server/wayliblogging.cpp`

分类变量命名：`lcWl` + PascalCase 模块名（例如 `lcWlSeat`、`lcWlOutput`、`lcWlCursor`）。

字符串 ID 命名：`waylib.<module>[.<submodule>]`、`waylib.protocols.<name>`、`waylib.qtquick.<name>` 或 `waylib.utils.<name>`（例如 `waylib.seat`、`waylib.input.pointer`、`waylib.protocols.inputmethod`）。

## Waylib 分类层级

```
waylib
├── 内核
│   ├── waylib.object               lcWlObject
│   ├── waylib.socket               lcWlSocket
│   ├── waylib.seat                 lcWlSeat
│   ├── waylib.output               lcWlOutput
│   ├── waylib.output.drm           lcWlOutputDrm
│   ├── waylib.output.buffer        lcWlOutputBuffer
│   ├── waylib.input                lcWlInput
│   ├── waylib.cursor               lcWlCursor
│   ├── waylib.input.pointer        lcWlPointer
│   ├── waylib.input.touch          lcWlTouch
│   ├── waylib.input.gesture        lcWlGesture
│   └── waylib.input.drag           lcWlDrag
├── QtQuick
│   ├── waylib.texture.provider     lcWlTextureProvider
│   ├── waylib.qtquick.surface      lcWlSurface
│   ├── waylib.qtquick.texture      lcWlQtQuickTexture
│   ├── waylib.renderer             lcWlRenderer
│   └── waylib.qtquick.bufferitem   lcWlBufferItem
├── 工具
│   ├── waylib.utils.imagecapture   lcWlImageCapture
│   ├── waylib.utils.cursorimage    lcWlCursorImage
│   ├── waylib.utils.bufferdumper   lcWlBufferDumper
│   └── waylib.utils.thread         lcWlThreadUtils
├── 协议
│   ├── waylib.protocols.foreigntoplevel       lcWlForeignToplevel
│   ├── waylib.protocols.inputmethod           lcWlInputMethod
│   ├── waylib.protocols.extforeigntoplevel    lcWlExtForeignToplevel
│   ├── waylib.protocols.remotesubsurface      lcWlRemoteSubsurface
│   └── waylib.protocols.textinput             lcWlTextInput
├── Layer shell
│   ├── waylib.layer.shell          lcWlLayerShell
│   └── waylib.layer.surface        lcWlLayerSurface
├── XDG
│   ├── waylib.xdg.output           lcWlXdgOutput
│   ├── waylib.xdg.decoration       lcWlXdgDecoration
│   └── waylib.xdg.dialog           lcWlXdgDialog
├── 输出扩展
│   ├── waylib.output.layer         lcWlOutputLayer
│   └── waylib.output.helper        lcWlOutputHelper
├── QtQuick 扩展
│   ├── waylib.qml.creator          lcWlQmlCreator
│   └── waylib.cursor.quick         lcWlQuickCursor
└── 平台与渲染
    ├── waylib.platform             lcWlPlatform
    ├── waylib.render.helper        lcWlRenderHelper
    ├── waylib.render.buffer        lcWlRenderBuffer
    └── waylib.render.bufferrenderer lcWlBufferRenderer
```

`wlroots`（`lcWlroots`，ID 为 `wlroots`）是唯一不在 `waylib.*` 命名空间下的分类，用于单独过滤 wlroots 自身的消息。

已知的 ID 例外：`waylib.texture.provider`（历史遗留；新分类用 `waylib.qtquick.*`）与 `wlroots`。不要重命名已有 ID —— 字符串 ID 是对外公开的 `QT_LOGGING_RULES` 契约。

新增或移动分类时，在同一次改动里同步更新这棵树。

## 强制规则

1. **禁止无分类日志** —— 绝不使用裸 `qDebug()`、`qWarning()`、`qCritical()`、`qInfo()`。始终使用带分类的变体：`qCDebug(cat)`、`qCInfo(cat)`、`qCWarning(cat)`、`qCCritical(cat)`。

2. **库代码里禁止本地 Q_LOGGING_CATEGORY** —— treeland 与 waylib 库代码的所有分类都必须声明在集中式 `*logging.h/.cpp` 中。只有独立可执行文件（例如 `treeland-xwayland`）可以在自己的 `.cpp` 里定义本地分类。

3. **禁止在 .h 里 include 日志头文件** —— 只有 `treelandlogging.h` / `wayliblogging.h` 自身可以声明分类。其他 `.h` 不得 `#include` 日志头文件，也不得使用 `qC*()` 宏，应把日志调用移到 `.cpp`。唯一例外是无法把日志调用移到 `.cpp` 的 header-only 模板（参见 `waylib/src/server/utils/wthreadutils.h`）：这类头文件可以 include 集中式日志头并使用其中的分类。

4. **每个分类都要有注释** —— 每个 `Q_LOGGING_CATEGORY` 定义都要有说明。当前有两种约定：
   - **Treeland**（`treelandlogging.cpp`）：用分段 `//` 注释（例如 `// Core modules`、`// Input modules`）标注一组相关分类。
   - **Waylib**（`wayliblogging.cpp`）：在每个定义后加行尾 `//` 注释，例如：
     ```cpp
     Q_LOGGING_CATEGORY(lcWlSeat, "waylib.seat", QtInfoMsg) // Seat management (keyboard focus, capability)
     ```
   给任一文件新增分类时，沿用该文件已有的约定。

## 日志级别
选择能表达运维重要性的最低级别。

- `qCDebug`：细节调试与排障
- `qCInfo`：正常运行信息
- `qCWarning`：异常但可恢复
- `qCCritical`：需要立即处理的严重故障

不要为了让信息“看得见”就把日常噪声升级成 warning 或 critical。

## 消息质量
写具体、可过滤的消息。

推荐：

```cpp
qCDebug(lcWlCursor) << "Cursor moved from" << oldPos << "to" << newPos;
```

避免：

```cpp
qCDebug(lcWlCursor) << "Cursor moved";
```

必要时包含：

- 涉及的对象或子系统
- 前后状态
- 标识符或名称
- 错误的影响

## 错误日志
记录失败时要带上足够上下文，使排障时无需再读周边代码。

推荐：

```cpp
qCWarning(lcWlCursor) << "Failed to attach device" << device->name()
                      << "- not a pointing device";
```

避免没有对象、原因或影响的模糊失败信息。

## 敏感数据
不要记录密钥、密码、token 或其他敏感用户数据。

## 新增分类
需要新增日志分类时按以下步骤：

1. **确定命名** —— 选定变量名（`lcTl*` 或 `lcWl*` + PascalCase）与位于合适层级的字符串 ID。
2. **加声明** —— 在集中式 `.h`（`treelandlogging.h` 或 `wayliblogging.h`）的正确分段中加 `Q_DECLARE_LOGGING_CATEGORY(lcXxx)`。
3. **加定义** —— 在集中式 `.cpp` 中加 `Q_LOGGING_CATEGORY(lcXxx, "domain.path", level)`，并用 `//` 注释说明其范围。
4. **包含头文件** —— 在用到该分类的 `.cpp` 中加 `#include "treelandlogging.h"` 或 `#include "wayliblogging.h"`。
5. **迁移调用点** —— 把裸 `qDebug()`/`qWarning()` 等替换为 `qCDebug(lcXxx)`/`qCWarning(lcXxx)` 等。
6. **校验** —— 运行 `rg '\bq(Debug|Warning|Critical|Info)\s*\(' src/ waylib/src/ | rg -v 'qC(Debug|Info|Warning|Critical)'`，确认没有残留的无分类调用。只用空括号形式（`qDebug\(\)`）会漏掉 printf 风格的 `qDebug("...")`。没有 `rg` 时换成 `grep -rE` / `grep -vE`。

treeland 侧要改两处（waylib 同理，换成 `lcWl` / `waylib.` 前缀）：

```diff
--- a/src/common/treelandlogging.h
+++ b/src/common/treelandlogging.h
 // Output module
 Q_DECLARE_LOGGING_CATEGORY(lcTlOutput)
+// Foo module
+Q_DECLARE_LOGGING_CATEGORY(lcTlFoo)

--- a/src/common/treelandlogging.cpp
+++ b/src/common/treelandlogging.cpp
 // Output module
 Q_LOGGING_CATEGORY(lcTlOutput, "treeland.output", QtInfoMsg)
+// Foo module
+Q_LOGGING_CATEGORY(lcTlFoo, "treeland.foo", QtInfoMsg)
```

调用点：`qCDebug(lcTlFoo) << ...`，并在消费该分类的 `.cpp` 里 `#include "common/treelandlogging.h"`。

## 评审清单
评审日志改动时逐项确认：

1. 分类符合 `lcTl*`/`lcWl*` 命名约定
2. 分类声明在集中式头文件中（库代码里不得本地定义）
3. 除日志头本身、或无法移出日志调用的 header-only 模板外，没有 `.h` include 日志头或使用 `qC*()`
4. 每个 `Q_LOGGING_CATEGORY` 定义都有说明范围的 `//` 注释
5. 所选级别与严重程度匹配
6. 消息包含足够上下文
7. 改动不泄露敏感信息
8. 改动没有引入裸 `qDebug()`/`qWarning()`/`qCritical()`/`qInfo()` 调用
9. 改动没有引入与任务无关的分类变动

## 仓库参考
- `src/common/treelandlogging.h`
- `src/common/treelandlogging.cpp`
- `waylib/src/server/wayliblogging.h`
- `waylib/src/server/wayliblogging.cpp`
