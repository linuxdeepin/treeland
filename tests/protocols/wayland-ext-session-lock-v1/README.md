# `wayland-ext-session-lock-v1` 测试规范

## 范围

- XML / interface：`ext_session_lock_manager_v1` / `ext_session_lock_v1`（version 1），
  以及作为前置条件的 `zwlr_layer_shell_v1`（`EXTRA_XMLS`，用于制造 exclusive zone）
- 测试源码：`tests/protocols/wayland-ext-session-lock-v1/`
- Fixture：headless output fixture
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_session_lock_manager_v1`（v1） | 资源创建成功 | P |
| 工作区前置条件 | top layer surface 设 `exclusive_zone=40` 并提交 buffer | 回读 `Output::validGeometry()` 高度 == 输出高度 - 40 | E |
| 锁定会话 | `lock()` + 等待 grace timer | 收到 `locked` 事件 | P |
| 真实锁定状态 | 回读真实 `WSessionLock::isLocked()` | 为 true | E |
| 锁屏 surface 配置尺寸 | 收到 `ext_session_lock_surface_v1.configure` | 配置尺寸 == 输出全尺寸（不是工作区尺寸） | P |
| 锁屏窗口几何 | lock surface 提交全尺寸 buffer 后回读生产 wrapper | wrapper 尺寸 == `Output::geometry()`、位置 == 输出原点 | E |

## 生产结果

测试绑定 `ext_session_lock_manager_v1` 并调用 `lock()`。Treeland 的
`WaylandServer::onExtSessionLock` 启动 300 ms grace timer，超时后调用 `WSessionLock::lock()`，
向客户端发送 `locked` 事件并将生产 `WSessionLock` 对象置为 Locked 状态。测试在收到
`locked` 事件后，通过 server bridge 回读 `WSessionLockManager::lockCreated` 信号捕获的
生产 `WSessionLock::isLocked()`，断言为 true。此断言验证锁定确实到达了真实合成器
session-lock 对象，而非仅事件到达无协议错误。

此 fixture 中预期合成器在 grace timer 后接受锁定；若收到 `finished`，说明合成器拒绝了
锁定请求，测试失败。这样可防止无条件拒绝 session lock 的回归被误判为 P 级通过。

锁屏 surface 的几何由同一客户端覆盖：先创建一条 `exclusive_zone=40` 的 top layer surface
（面板/dock 的等价物），断言工作区确实比输出小 —— 没有 exclusive zone 的环境里，后面的
尺寸断言会平凡通过与假通过。随后 `get_lock_surface` → ack configure → 提交输出全尺寸 buffer，
再从 server bridge 回读 `LockScreenContainer` 中的生产 `SurfaceWrapper`，断言其尺寸等于
`Output::geometry()`（不是 `validGeometry()`）、位置等于输出原点。这条断言是
`Output::arrangeNonLayerSurface()` 缩窗回归的红/绿判据：未修复时合成器会把锁屏 surface
配置成工作区尺寸（例如 1280x680 而非 1280x720）。

## 已知边界 / 下一项结果

未覆盖：多输出锁屏（每个输出一个 lock surface）、`abandoned`（锁屏客户端崩溃回退到
greeter）、unlock 后 `isLocked()` 翻回 false 的回读，以及 V 级像素读回。
