# `treeland-screensaver-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-screensaver-v1/`、`tests/protocols/treeland-screensaver-desktop-v1/`
- Fixture：两个 target 均使用 desktop integration fixture。基础 target 读取生产
  `ScreensaverInterfaceV1::isInhibited()`；desktop target 创建 mapped xdg-toplevel，并使用上游
  `ext-idle-notify-v1` 客户端。
- 覆盖等级：抑制生命周期为 **E**；版本和错误路径为 **P**。

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| 抑制 idle | 创建 screensaver manager 后调用 inhibit | 在观察窗口内，真实 ext-idle notification 不发送 `idled` |
| 解除抑制 | 销毁/解除 inhibitor | 同一 ext-idle notification 随后发送 `idled` |
| 非法顺序/对象 | 发送 XML 不允许的请求 | Wayland 连接以规定的 EPROTO/协议错误结束 |

## 已证明的生产链路

基础 target 绑定 manager 后发送 inhibit/uninhibit，并通过 desktop runner 的 server callback
读取真实 `ScreensaverInterfaceV1::isInhibited()`；它还验证重复 inhibit 及未 inhibit 时的
uninhibit 所产生的协议错误。

desktop target 首先 map xdg-toplevel，fixture 确认它已成为可见的 workspace `SurfaceWrapper`。
随后客户端发送 `treeland_screensaver_v1.inhibit("protocol-test", "desktop idle inhibition")`，
再为同一个真实 `wl_seat` 发送上游
`ext_idle_notifier_v1.get_idle_notification(timeout=20ms, seat)`。

在 inhibit 仍存在的 150ms 观察窗口内，client listener 的 `idled` 计数必须仍为 `0`；这直接
观察 wlroots ext-idle notifier，而不是读取 screensaver 的内部标志。接着发送
`treeland_screensaver_v1.uninhibit()`，在最多 500ms 内同一个 listener 必须收到恰好
一次 `idled`。因此该测试证明 inhibit/uninhibit 已实际改变 idle inhibitor 的行为。

这里的观察窗口是验证“在 idle timeout 到期后仍未发生事件”这一否定结果所需的协议时间语义；
它不是 fixture ready 或普通 request 完成的同步手段。

## 未覆盖

未显示实际锁屏 UI；这里只验证 idle-inhibit 这一前置行为。
