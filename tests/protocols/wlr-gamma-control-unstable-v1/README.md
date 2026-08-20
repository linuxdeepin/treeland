# `wlr-gamma-control-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-gamma-control-unstable-v1.xml`，
  `zwlr_gamma_control_manager_v1` / `zwlr_gamma_control_v1` v1。
- 测试源码：`tests/protocols/wlr-gamma-control-unstable-v1/`。
- Fixture：完整 Treeland desktop integration fixture 与真实 headless `wl_output`。
- 覆盖等级：P。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 不支持 gamma LUT 的 output | bind manager 与 headless `wl_output`，`get_gamma_control` | 不发送 `gamma_size`，只发送一次 `failed` | P |
| failed control 的 request/lifetime | 在 failed 后用 pipe FD 调用 `set_gamma`，再 destroy control/manager | inert native resource 吸收 request、取得 FD，连接不发生 protocol error；两个 destroy 均可完成 | P |

## 生产结果

Treeland 在 `Helper::init()` 直接创建 wlroots native manager，并将 `set_gamma` signal 接到
`Helper::setGamma()`；waylib 当前没有 wrapper，native manager 随 display 销毁。
headless CI 的真实 output 没有 gamma LUT，wlroots 因此发送 `failed`。可选 DRM runner 则将
非 identity LUT 经 `Helper::setGamma()` 提交到真实 output；该生产 commit 失败时 Helper 发送
`failed`，测试据此失败。

### 可选 DRM runner（I 级）

`TREELAND_ENABLE_GPU_PROTOCOL_TESTS=ON` 时额外注册
`test_wlr_gamma_control_unstable_v1_drm`。它使用 `WLR_BACKENDS=drm`，不创建 headless output；
因此普通无头 CI 不会注册或执行它。

运行时只有在物理 output 发送一次非零 `gamma_size` 时才继续。client 生成三条完整、非 identity
LUT ramp 并调用 `set_gamma`；wlroots 发出的 `set_gamma` signal 必须经生产 `Helper::setGamma()`
提交到实际 DRM output。commit 失败会由 Helper 发送 `failed`，测试将其判为失败；没有可用 LUT、
无法创建 LUT 文件或不支持的 DRM 环境均返回 77 Skip。因此该路径证明真实 production output commit，
为 I 级。

## 已知边界 / 下一项结果

wlroots headless backend 的 gamma size 为零，普通 CI 无法合法创建 LUT。因此 CI 不能把
`set_gamma` 成功、LUT commit 或 output 色彩变化报告为覆盖。

DRM runner 已覆盖 production commit。若要达到 V 级，仍需 DRM/KMS 可读取的 gamma state 或外部
屏幕/readback fixture 验证实际呈现色彩；gamma-control 协议本身没有成功 ack，不能以“未 failed”
冒充像素结果。

- 未覆盖 LUT restore、多个 control 的 exclusivity。
- 未覆盖错误长度 LUT 的 `invalid_gamma` protocol error；它同样需要非零 gamma size。
