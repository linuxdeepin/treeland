# `wlr-output-management-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-output-management-unstable-v1.xml` /
  `zwlr_output_manager_v1`、head、mode、configuration 与 configuration-head v4。
- 测试源码：`tests/protocols/wlr-output-management-unstable-v1/`。
- Fixture：完整 Treeland desktop integration fixture 与两个 headless output —— wlroots 默认提供的
  `WLR_HEADLESS_OUTPUTS=1` 那个（`1280×720`）加上 fixture 追加的一个 `1920×1080`；一次 apply
  涉及多个 head 的生产路径只在 ≥2 个 head 时存在。两个 head 的 mode 都由 client 用
  `set_custom_mode` 显式指定，因此用例不依赖启动尺寸。
- 覆盖等级：**P / E**；P 覆盖 native global 的快照、transaction 与生命周期，E 覆盖配置从
  client 经 `WOutputManagerV1`/`WaylandServer` 提交到真实 output 的 position、transform 与 scale。

## 必须观察到的结果

| XML 语义 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 初始 output 描述 | bind manager v4 并 roundtrip | 两个 head，各自带 `name`、`description`、virtual mode 的 `size`、`current_mode`、`enabled`、`position`、`transform`、`scale`、`adaptive_sync`，以及携带 serial 的 `done`；仅在固定刷新率时要求 `refresh` | P |
| enabled configuration test | 用最新 serial 创建 configuration，为一个 head 调用 `enable_head`、custom mode、position、transform、scale、adaptive-sync 后 `test` | 唯一 `succeeded`，无 `failed/cancelled`；不改变 output 状态 | P |
| disabled configuration test | 新 configuration 对一个 head 调用 `disable_head` 后 `test` | 唯一 `succeeded`，不关闭 fixture output | P |
| apply 的业务结果 | enabled configuration 设置 custom `1920×1080`、position `(37,53)`、`transform=90°`、`scale=2` 后 `apply` | `succeeded`，fixture 从真实 production output 读取到 enabled、坐标 `(37,53)`、90° 与 scale 2 | E |
| 恢复配置 | 用新 serial apply `(0,0)`、normal transform、scale 1 | `succeeded`，真实 output 回到 `(0,0)`、normal、scale 1 | E |
| 一次 apply 打开两个 head 且其中一个提交失败 | fixture 先把两个 output 关掉，再在一个 configuration 里 `enable_head` 两个 head，其中第二个 head 请求 `adaptive_sync=enabled`（headless backend 不支持该状态字段，提交必然失败）；**两种 `enable_head` 顺序各跑一遍** | 恰好一次 `failed`，没有 `succeeded`：先提交成功的那个 head 也必须整份报 failed，不能提前报 succeeded | E |
| 一次 apply 打开两个 head 全部成功 | 在一个 configuration 里 `enable_head` 两个 head，分别请求 `(0,0)` 与 `(1920,0)` | 恰好一次 `succeeded`，fixture 从两个真实 production output 读取到 enabled 与各自请求的坐标 | E |
| manager/head/mode 生命周期 | `stop`、收到 `finished`，release head 与 virtual mode | `finished` 恰好一次，client 可释放子资源 | P |

## 生产结果

wlroots 原生实现 `wlr_output_manager_v1`；waylib 的 `WOutputManagerV1` 创建并拥有 native
manager。`WaylandServer::init()` 通过 `m_server->attach<WOutputManagerV1>()` 发布 global，并把
`requestTestOrApply` 接到 `WaylandServer::onOutputTestOrApply()`。

测试不会在 fixture 伪造 configuration result。`test` 由 WaylandServer 对真实 output 调用
`wlr_output_test_state()` 后回传 `succeeded`；`apply` 则进入 production render/commit job，完成后
由 `WaylandServer::onOutputCommitFinished()` 发送 `succeeded`、更新 output layout。fixture 随后读取
`RootSurfaceContainer` 中同一个 `WOutput` 的 `WOutputItem`，并读取该 output 的 production
orientation/scale。因此 `(37,53)`、90°、scale 2 及回滚 `(0,0)`、normal、scale 1 是 E 级输出布局
结果，而不仅是 client 收到了成功 event。

### E 级覆盖流程

1. fixture 在 CI 的 `WLR_BACKENDS=headless;WLR_RENDERER=pixman` 环境使用两个真实 headless
   `wlr_output`（wlroots 默认的 `1280×720` + fixture 追加的 `1920×1080`），不依赖物理显示器、GPU
   或系统 display manager；每个用例都会用 custom mode 把 head 设成 `1920×1080`；
2. client 以 manager `done` 的最新 serial 创建完整 configuration，提交 custom mode、position
   `(37,53)`、90° transform 和 scale 2；
3. production `WOutputManagerV1` 发出 apply signal，`WaylandServer` 对真实 output 建立 commit job；成功
   回调更新 output layout 并发送 configuration `succeeded`；
4. client 在成功 event 后调用 fixture，fixture 从同一个 production `WOutputItem` 读取位置，并从
   同一个 `WOutput` 读取 orientation 和 scale，必须分别为 `(37,53)`、90°、2；
5. client 用新 serial apply normal、scale 1、`(0,0)`，并以相同的 production state readback
   验证恢复；
6. 多 head 用例先把两个 output 关掉（fixture 直接提交 disabled，等同用户关屏），再在一个
   configuration 里同时 `enable_head` 两个 head：一个 head 提交必然失败，client 必须收到
   `failed`。由于答复顺序取决于合成器存储 head 的顺序（wlroots 目前是头插，即最后
   `enable_head` 的 head 最先提交），这一步把两种 `enable_head` 顺序都跑一遍，任一种顺序下
   「先成功的 head 提前答复 succeeded」都会被钉住；随后再以相同的 production state readback
   验证一次两个 head 全部成功的 apply。

步骤 4 与 5 不读取 client event 缓存，也不调用测试专用 setter；因此该结论是 E 级。该流程待首次
CI 执行记录结果，当前未因本次实现而声称已通过。

多 head 用例的失败注入只使用协议允许的请求：`adaptive_sync=enabled` 在 headless backend 上不属于
`WLR_OUTPUT_STATE_BACKEND_OPTIONAL`，commit 被 `output_test()` 拒绝，因此第二个 head 的提交确定失败，
不需要在 fixture 里伪造 commit 结果。

## 已知边界 / 下一项结果

- headless output 没有固定 mode；测试以生产 headless 的 custom `1920×1080` mode 覆盖
  `set_custom_mode`。有固定 mode 的 GPU/X11 runner 可补 `mode/size/refresh/preferred/current_mode` event
  与 `set_mode` 的 I/E 路径。
- 尚未构造 stale serial 的 `cancelled`，以及 duplicate/unconfigured/used configuration 的 error；这些
  应由独立连接断言 protocol error，避免破坏主 E 链路。
- 尚未验证多 output 的相对布局、真正 disable 后重开的双 head apply 之外的组合、adaptive-sync 的硬件
  结果，或 output hotplug 导致的 head/mode `finished`。adaptive-sync 的硬件结果需要支持 VRR 的
  runner；scale/transform 已覆盖 production state，不等同于最终像素 readback。
