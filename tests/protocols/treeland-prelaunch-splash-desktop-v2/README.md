# prelaunch splash 桌面集成：wrapper 生命周期与图标跨阶段保留

## 范围

- XML / interface：
  - `treeland-prelaunch-splash-unstable-v2`（`create_splash.icon_buffer`）
  - `treeland-app-id-resolver-unstable-v2`（把真实窗口匹配到 splash）
  - `treeland-foreign-toplevel-manager-unstable-v3`（dock preview context）
- 测试源码：`tests/protocols/treeland-prelaunch-splash-desktop-v2/`
- Fixture：完整生产 `Treeland` / `WaylandServer` / `ShellHandler` / workspace / headless output
- 覆盖等级：**E / V**（纹理像素读回，不是最终 output backing buffer）

## 两个阶段

阶段一验证 splash request/close 已进入生产 `ShellHandler` 并生成、销毁 splash wrapper。
阶段二用同一条链路把 AM 图标一路带到 dock preview：客户端 `create_splash` 之后立刻销毁自己的
`wl_buffer`（AM 的行为），窗口经 app-id resolver 匹配后把 splash 转成真实窗口，预览仍能画出图标。

## 必须观察到的结果

| 阶段 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| splash 建立 | `create_splash(app_id, icon = 16×16 ARGB8888)` | 生产 wrapper 为 `SplashScreen`、appId 一致、在 workspace 内、有 `prelaunchSplash()` item、尺寸为默认 `800 × 600` | E |
| 图标锁住 | 发送请求后立即销毁客户端 `wl_buffer` | `SurfaceWrapper::appIcon()` 仍可读，尺寸 `16 × 16` | E |
| 关闭 | 销毁客户端 splash 资源 | wrapper 离开 workspace 并发出 destroyed | E |
| 跨阶段保留 | 建 `800 × 600` xdg 窗口并让 resolver 应答 app-id | 同一个 wrapper 变成 `XdgToplevel`，`prelaunchSplash()` 为 `nullptr`，`appIcon()` 仍为 `16 × 16` | E |
| 图标进入 dock preview | `get_dock_preview_context` + `show([identifier])` | 标题行图标 item 存在、可见，`buffer` 指针等于 `wrapper->appIcon()`，宽高等于 `titleRowHeight` | E |
| 纹理像素 | 服务端 `render()` 后由 `WTextureCapturer` 读回 | 中心像素 RGBA `(255, 0, 0, 255)` | V |
| 无图标窗口 | 预览一个从未有 splash 的 `320 × 200` 窗口 | 图标 item 不存在，因此不占位 | E |
| 悬停切换 | 两个窗口之间来回 `show()` 10 次 | 每次都得到上一条的状态，无警告无崩溃 | E |
| 图标来源窗口关闭 | 销毁带图标的窗口后再 `show()` | 图标 item 不存在，无警告无崩溃 | E |

「无警告」由 fixture 的 `qInstallMessageHandler` 计数：`wTextureProvider() is nullptr`、
`can only be queried on the rendering thread`、`texture missing or item size invalid`、
`Reject buffer with invalid size`，以及任何提及 `DockPreview.qml` 的 warning/critical
（QML 引擎拒绝赋值只记日志、不报错）。

## 关键实现约束

- 真实窗口尺寸取 splash 的默认 `800 × 600`。尺寸不一致时生产代码走 `GeometryAnimation`，
  其 `ready()` 依赖 `ShaderEffectSource::onScheduledUpdateCompleted`，headless 的 software
  renderer 不调度该更新，splash item 永远不会销毁；同尺寸提交直接走 `completeSplashTransition`。
- 等待都基于生产状态或信号：wrapper 用 `SurfaceContainer::surfaceAdded`，转换用
  `type()` + `prelaunchSplash()` 轮询，预览用 `previewReveal` 与 QML binding 结果轮询，
  纹理用 `QFutureWatcher::finished`；轮询均有帧数上限。

## 已知边界

- 像素来自图标 item 的生产纹理，不是 output 的合成 backing buffer；叠放、output transform
  与 scanout 像素未覆盖；splash 自身的可见性与像素同样未覆盖。
- 图标被拉伸为正方形（`titleRowHeight × titleRowHeight`），非方形图标未做等比适配，
  已在 `DockPreview.qml` 用 `shortcut:` 标注。
- 图标 item 每次悬停重建，未覆盖 `WBufferItem::setBuffer()` 运行时替换既有 buffer 的路径
  （连续悬停两个带图标的窗口），生产里仍存在。
- `previewReveal` 动画在 headless 下会跑完，但未断言动画曲线。
