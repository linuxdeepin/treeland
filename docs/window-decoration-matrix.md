# 窗口装饰器 / 圆角 / 默认值一览

> 状态：2026-11-09 更新。本文汇总 `SurfaceWrapper` 各窗口类型下「是否画装饰器（SSD 边框 +
> 阴影）、是否画标题栏、默认圆角」的实际行为，以及目前散落在各处的特判。
>
> 核心代码位置：
> - 类型定义：`src/surface/surfacewrapper.h`（`enum class Type`）
> - 构造默认值：`src/surface/surfacewrapper.cpp`（三个构造函数初始化列表）
> - 装饰器/标题栏创建：`SurfaceWrapper::setNoDecoration()` / `updateDecoration()` /
>   `updateTitleBar()`
> - 圆角计算：`SurfaceWrapper::radius()` / `setRadius()` / `setNoCornerRadius()`
> - 圆角渲染（裁剪）：`src/core/qml/SurfaceContent.qml`
> - 边框 + 阴影渲染：`src/core/qml/Decoration.qml`（`XdgShadow` + `Border`）
> - 协议仲裁：`src/core/surfaceprotocolcontroller.cpp`（`onSurfaceWrapperAdded`）
> - DConfig 默认值：`misc/dconfig/org.deepin.dde.treeland.user.json`

---

## 1. 关键不变量（先记住这三条）

1. **装饰器默认是「关」的。** 所有 `SurfaceWrapper` 构造函数里 `m_noDecoration = true`、
   `m_noTitleBar = true`。只有某条路径显式调用 `setNoDecoration(false)` /
   `setNoTitleBar(false)`，才会创建 SSD 边框 + 阴影 / 标题栏。
2. **`noTitleBar()` 已经包含 `noDecoration`。** getter 返回
   `m_noTitleBar || m_noDecoration`（全屏时恒为 true）。所以「无边框」的窗口**不可能**有
   标题栏，`setNoTitleBar(false)` 也不会让无边框窗口凭空长出标题栏。
3. **圆角只对「有装饰器」的窗口生效。** `SurfaceContent.qml` 的圆角裁剪条件是
   `cornerRadius > 0 && !noCornerRadius && decoration && visibleDecoration`。`m_noDecoration`
   为 true 时 `m_decoration` 为 null，圆角不渲染。

### 全局默认值（DConfig）

| 项 | 默认值 | 位置 |
| --- | --- | --- |
| 窗口圆角 `windowRadius` | **12** | `org.deepin.dde.treeland.user.json` |
| 标题栏高度 `windowTitlebarHeight` | **40** | 同上 |
| 阴影 | blur 40、offset (0,10)、rgba(0,0,0,0.4)、可见 | `SurfaceWrapper` 成员默认值 |
| 边框 | width 1、rgba(0,0,0,0.1)、可见 | 同上 |

---

## 2. 各类型矩阵

| Type | 装饰器（边框+阴影） | 标题栏 | 默认圆角 | 说明 |
| --- | --- | --- | --- | --- |
| `XdgToplevel` | 取决于 xdg-decoration（见 §3） | 取决于 personalization / decoration 协议 | DConfig `windowRadius`（12） | 唯一常规「有标题栏」的类型 |
| `XdgPopup` | ✅ 强制有（`setNoDecoration(false)`） | ❌ 无 | **8**（硬编码） | 无尺寸调整 MouseArea |
| `XWayland` | 取决于 X11 装饰 hints | 取决于 X11 装饰 hints | DConfig `windowRadius`（12） | 默认有边框 + 标题栏 |
| `Layer` | ❌ 无（默认 `true`） | ❌ 无 | **0** | 不参与 DConfig 圆角回退；quick-launchpad 有兼容特判 |
| `InputPopup` | ❌ 无 | ❌ 无 | **0** | `isInputPopupLike()` |
| `LockScreen` | ❌ 无（显式 `setNoDecoration(true)`） | ❌ 无 | 名义 12，但无装饰器故不生效 | 全屏锁屏面 |
| `SplashScreen`（prelaunch） | ✅ 有（`setNoDecoration(false)`） | ❌ 无 | DConfig `windowRadius`（12） | `updateDecoration()` 显式跳过标题栏；未指定尺寸时默认 800×600 |
| IM 候选面板（flag） | ❌ 无（强制 `setNoDecoration(true)`） | ❌ 无 | **0** | `m_isIMCandidatePanel` → `isInputPopupLike()` |
| 截图 mask | ❌ 无 | ❌ 无 | **0** | `setNoCornerRadius(true)` + `setNoDecoration(true)` |

> 「名义 12，但无装饰器故不生效」：`LockScreen` 的 `radius()` 会命中 DConfig 回退返回 12，
> 但因为 `m_noDecoration = true`，`SurfaceContent.qml` 的裁剪条件不满足，实际渲染是直角。

---

## 3. XdgToplevel 装饰器 / 标题栏的仲裁逻辑

这是最复杂的路径，集中在 `SurfaceProtocolController::onSurfaceWrapperAdded`。

### 3.1 装饰器（SSD 边框 + 阴影）

由 **xdg-decoration 协商结果**决定，与标题栏是两个独立轴：

```cpp
wrapper->setNoDecoration(mode != WXdgDecorationManager::Server);
```

`mode` 取值（`WXdgDecorationManager::DecorationMode`）：

| mode | 含义 | 装饰器 |
| --- | --- | --- |
| `Server` | 客户端要求服务端装饰 | ✅ 有 |
| `Client` | 客户端要求自绘（CSD） | ❌ 无 |
| `None` | 客户端发送 "unset" | 回退到 `preferredMode`（默认 `Server`）→ ✅ 有 |
| `Undefined` | 客户端从未创建 decoration | ❌ 无（当作 CSD） |

> 注意：**客户端不主动协商 xdg-decoration 时默认无 SSD 边框**，符合现代合成器惯例。
> 有两条写入路径：全局 `onSurfaceModeChanged()`（容器查找）负责注册后的实时变化；
> `onSurfaceWrapperAdded` 里的初始回放负责「协商发生在 wrapper 注册之前」的竞态。

### 3.2 标题栏

由两个协议源合并仲裁（单一写入点 `updateNoTitlebar`）：

```cpp
setNoTitleBar(attached->noTitlebar() || decorationHides);
```

- `attached->noTitlebar()`：旧 personalization 协议（`set_titlebar(disable)` → NoTitleBar）。
- `decorationHides`：新 decoration 协议（`set_titlebar_mode(hide)`）。
- 两个源任一为 true → 无标题栏；都为 false → 有标题栏（前提是有装饰器，见不变量 2）。

> 默认行为总结：客户端**完全不协商任何协议**时，xdg-decoration mode 为 `Undefined`
> （无装饰器），标题栏虽被 `setNoTitleBar(false)` 请求，但被不变量 2 拦下——实际结果是
> **无边框、无标题栏**。只有 xdg-decoration 协商到 `Server`（或 `preferredMode` 回退）
> 后，标题栏才会真正出现。

### 3.3 状态切换（最大化 / 全屏 / 平铺）

`SurfaceWrapper::applySurfaceState()`：

```cpp
setVisibleDecoration(newSurfaceState == State::Normal);   // 非 Normal 隐藏装饰器
setNoCornerRadius(Maximized || Fullscreen || Tiling);     // 非 Normal 无圆角
```

所以最大化 / 全屏 / 平铺窗口**直角 + 隐藏阴影边框**，恢复 Normal 后回到圆角。

---

## 4. 现有特判清单（改动装饰器 / 圆角 / 标题栏的地方）

| # | 特判 | 代码位置 | 行为 |
| --- | --- | --- | --- |
| 1 | `XdgPopup` | `surfaceprotocolcontroller.cpp` popup 分支 | `setNoTitleBar(true)` + `setNoDecoration(false)`：无标题栏但有阴影边框，圆角固定 8 |
| 2 | **quick-launchpad 兼容**（COMPAT） | `surfaceprotocolcontroller.cpp` | 旧 personalization 协议仍驱动 quick-launchpad；`isQuickLaunchpad() && noTitlebar()` 时 `setNoDecoration(false)` 恢复边框。**迁移完成后连同旧协议一起删** |
| 3 | 大启动器 cover | `surfaceprotocolcontroller.cpp` | `isLaunchpad()` → `setCoverEnabled(true)`（`LaunchpadCover.qml` 壁纸背景） |
| 4 | 启动器层级 | `shellhandler.cpp` | layer Top 的 `isLaunchpad() \|\| isQuickLaunchpad()` → `setZ(-1)` |
| 5 | 桌面层级 | `shellhandler.cpp` | layer Background 的 `isDesktop()` → `setZ(1)` |
| 6 | XWayland 装饰 | `surfaceprotocolcontroller.cpp` xwayland 分支 | 按 X11 的 `NoTitle` / `NoBorder` hints 设置标题栏与边框 |
| 7 | IM 候选面板 | `imcandidatepanelmanager.cpp` | `setIMCandidatePanel(true)` + `setNoDecoration(true)`；`isInputPopupLike()` 使圆角归 0 |
| 8 | 截图 mask | `capture.cpp` | `setNoTitleBar(true)` + `setNoCornerRadius(true)` + `setNoDecoration(true)` |
| 9 | 锁屏面 | `lockscreen.cpp` | `setNoDecoration(true)` + skip switcher/multitask |
| 10 | prelaunch splash | `surfacewrapper.cpp` 构造函数 | `setNoDecoration(false)` 给启动画面画边框阴影；`updateDecoration()` 对 SplashScreen 跳过标题栏 |
| 11 | 全屏/最大化/平铺 | `applySurfaceState()` | 隐藏装饰器 + 无圆角（见 §3.3） |

---

## 5. 圆角计算细节

`SurfaceWrapper::radius()`：

```cpp
if (isInputPopupLike()) return 0;      // InputPopup / IM 候选面板
if (m_type == Type::XdgPopup) return 8; // popup 固定 8
qreal radius = m_radius;
if (radius < 1 && m_type != Type::Layer)
    radius = ...windowRadius();        // DConfig，默认 12
return radius;
```

- `m_radius >= 1` 表示有 per-window 显式覆盖（旧 personalization `set_corner_radius` 或新
  decoration `set_corner_radius`），直接使用（`radius < 1` 时才回退到 DConfig）。
- `Type::Layer` 被**排除**在 DConfig 回退之外 → layer（含启动器）默认圆角 0，compositor 不
  给 layer 画圆角；layer 的圆角只能靠客户端自绘。
- 两种协议的 corner radius 写入**未仲裁**（见代码 NOTE：客户端只应使用其中一种，后到的
  signal 覆盖先到的）。

### 圆角是否真正生效的三个条件

1. `cornerRadius > 0`
2. `!noCornerRadius`（无边框 / 非 Normal 状态会置 true）
3. `decoration != null && visibleDecoration`（必须有装饰器）

---

## 6. 标题栏高度

- 标题栏高度来自 DConfig `windowTitlebarHeight`（默认 40），由 `TitleBar.qml` 消费，
  变化通过 personalization appearance 协议推送。
- `updateTitleBar()` 创建标题栏后设置 `m_surfaceItem->setTopPadding(m_titleBar->height())`，
  内容区下移。

---

## 7. 修改这类代码时的检查清单

- [ ] 改了 `noDecoration` / `noTitleBar` 的赋值，确认有没有破坏「装饰器默认关」的不变量。
- [ ] 确认 `noTitleBar()` getter 的 `m_noTitleBar || m_noDecoration` 语义：无边框窗口永无标题栏。
- [ ] 新增窗口类型时补全矩阵：装饰器 / 标题栏 / 圆角三列都要说明。
- [ ] 涉及 `Type::Layer` 的圆角时，记得它**不参与** DConfig 回退。
- [ ] 旧 personalization 协议的 quick-launchpad COMPAT 块（§4 #2）是临时迁移代码，
      删除旧协议时一并清理。
- [ ] 圆角渲染依赖 `decoration && visibleDecoration`，只设 radius 不设 decoration 不生效。
