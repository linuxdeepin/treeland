# `wlr-output-power-management-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-output-power-management-unstable-v1.xml` /
  `zwlr_output_power_manager_v1` 与 `zwlr_output_power_v1` v1。
- 测试源码：`tests/protocols/wlr-output-power-management-unstable-v1/`。
- Fixture：完整 Treeland desktop integration fixture 与启动时已有的真实 headless output。
- 覆盖等级：**P / E**；P 覆盖 native resource、排他约束、生命周期与 error，E 覆盖 OFF/ON
  经生产 `Helper` 提交到真实 output enabled state。

## 必须观察到的结果

| XML 语义 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 初始状态 | 为真实 `wl_output` 调用 `get_output_power` 并 roundtrip | 恰好一次 `mode(on)`，无 `failed` | P |
| 每 output 排他 control | 在第一个 control 有效时再次 `get_output_power` | 第二对象只收到一次 `failed`，没有 `mode` | P |
| manager/child 生命周期分离 | destroy manager 后继续使用第一个 power object | 子对象仍有效；不产生 protocol error | P |
| 关闭 output 的业务结果 | 第一个 object 在 manager destroy 后 `set_mode(off)` | 收到 `mode(off)`；fixture 从真实 production `WOutput` 读取 `enabled=false` | E |
| 恢复 output 的业务结果 | 对同一 object 调用 `set_mode(on)` | 收到 `mode(on)`；同一 production `WOutput` 读取 `enabled=true` | E |
| power object 生命周期 | destroy power object 与 `wl_output` | 正常释放，无 use-after-free 或 protocol error | P |
| 非法 mode | 独立连接中发送 `set_mode(2)` | display 因 `zwlr_output_power_v1.error.invalid_mode` 断开 | P |

## 生产结果

wlroots 原生实现 `wlr_output_power_manager_v1`；waylib 当前没有 wrapper。`Helper::init()` 直接创建
native manager，并注册其 `set_mode` signal 到 `Helper::onSetOutputPowerMode()`。manager 由 wlroots
在 `wl_display` 销毁时释放，`Helper` 只保留 listener pointer，不手动销毁 native manager。

`Helper::onSetOutputPowerMode()` 对 OFF/ON 创建真实 `wlr_output_state`，调用
`wlr_output_commit_state()`，并以 `m_powerOffOutputs` 区分协议关闭与其他 output-management 关闭。
wlroots 在 output enabled commit 后向 control 发送 `mode`；fixture 再从同一个 production `WOutput`
读取 `isEnabled()`，不以 client event 缓存代替业务结果。

### E 级覆盖流程

1. CI 使用 `WLR_BACKENDS=headless;WLR_RENDERER=pixman` 创建真实 headless `wlr_output`；无需物理
   显示器、GPU 或系统 display manager；
2. client 获取唯一 output 的 power control，确认 wlroots 发送的初始 `mode(on)`；
3. client 销毁 manager 但保留该 control，发送 `set_mode(off)`；native manager signal 进入
   `Helper::onSetOutputPowerMode()`，对真实 output commit disabled state；
4. client 同时断言 `mode(off)`，fixture 读取 production `WOutput::isEnabled()==false`；
5. 同一 control 发送 `set_mode(on)`，以 `mode(on)` 和 `WOutput::isEnabled()==true` 验证恢复。

步骤 4、5 的状态由 production output 提供，不调用 fixture setter，因此为 E 级。当前实现待首次 CI
执行记录结果，不把未运行结果写成已通过。

## 已知边界 / 下一项结果

- headless backend 能证明 enable state commit，但不能证明物理面板实际进入 DPMS/省电状态；该结论需
  DRM/KMS runner、可观测 connector power state 或外部功耗/屏幕 readback 证据。
- 未构造 output 销毁导致现有 control 收到 `failed`；需要可控 hotplug/output remove fixture。
- 没有生产 trusted-client/authorization hook；任何可连接 client 均可取得 control。测试只记录 native
  wlroots 的排他约束，不将其误报为授权策略。
