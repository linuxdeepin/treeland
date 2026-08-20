# `wlr-output-management-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-output-management-unstable-v1.xml` /
  `zwlr_output_manager_v1`、head、mode、configuration 与 configuration-head v4。
- 测试源码：`tests/protocols/wlr-output-management-unstable-v1/`。
- Fixture：完整 Treeland desktop integration fixture 与一个 `1920×1080` headless output。
- 覆盖等级：**P / E**；P 覆盖 native global 的快照、transaction 与生命周期，E 覆盖配置从
  client 经 `WOutputManagerV1`/`Helper` 提交到真实 output 的 position、transform 与 scale。

## 必须观察到的结果

| XML 语义 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 初始 output 描述 | bind manager v4 并 roundtrip | 一个 head、`name`、`description`、virtual mode 的 `size`、`current_mode`、`enabled`、`position`、`transform`、`scale`、`adaptive_sync` 与携带 serial 的 `done`；仅在固定刷新率时要求 `refresh` | P |
| enabled configuration test | 用最新 serial 创建 configuration，为唯一 head 调用 `enable_head`、custom mode、position、transform、scale、adaptive-sync 后 `test` | 唯一 `succeeded`，无 `failed/cancelled`；不改变 output 状态 | P |
| disabled configuration test | 新 configuration 对唯一 head 调用 `disable_head` 后 `test` | 唯一 `succeeded`，不关闭 fixture output | P |
| apply 的业务结果 | enabled configuration 设置 custom `1920×1080`、position `(37,53)`、`transform=90°`、`scale=2` 后 `apply` | `succeeded`，fixture 从真实 production output 读取到 enabled、坐标 `(37,53)`、90° 与 scale 2 | E |
| 恢复配置 | 用新 serial apply `(0,0)`、normal transform、scale 1 | `succeeded`，真实 output 回到 `(0,0)`、normal、scale 1 | E |
| manager/head/mode 生命周期 | `stop`、收到 `finished`，release head 与 virtual mode | `finished` 恰好一次，client 可释放子资源 | P |

## 生产结果

wlroots 原生实现 `wlr_output_manager_v1`；waylib 的 `WOutputManagerV1` 创建并拥有 native
manager。`Helper::init()` 通过 `m_server->attach<WOutputManagerV1>()` 发布 global，并把
`requestTestOrApply` 接到 `Helper::onOutputTestOrApply()`。

测试不会在 fixture 伪造 configuration result。`test` 由 Helper 对真实 output 调用
`wlr_output_test_state()` 后回传 `succeeded`；`apply` 则进入 production render/commit job，完成后
由 `Helper::onOutputCommitFinished()` 发送 `succeeded`、更新 output layout。fixture 随后读取
`RootSurfaceContainer` 中同一个 `WOutput` 的 `WOutputItem`，并读取该 output 的 production
orientation/scale。因此 `(37,53)`、90°、scale 2 及回滚 `(0,0)`、normal、scale 1 是 E 级输出布局
结果，而不仅是 client 收到了成功 event。

### E 级覆盖流程

1. fixture 在 CI 的 `WLR_BACKENDS=headless;WLR_RENDERER=pixman` 环境创建一个真实
   `1920×1080` headless `wlr_output`，不依赖物理显示器、GPU 或系统 display manager；
2. client 以 manager `done` 的最新 serial 创建完整 configuration，提交 custom mode、position
   `(37,53)`、90° transform 和 scale 2；
3. production `WOutputManagerV1` 发出 apply signal，`Helper` 对真实 output 建立 commit job；成功
   回调更新 output layout 并发送 configuration `succeeded`；
4. client 在成功 event 后调用 fixture，fixture 从同一个 production `WOutputItem` 读取位置，并从
   同一个 `WOutput` 读取 orientation 和 scale，必须分别为 `(37,53)`、90°、2；
5. client 用新 serial apply normal、scale 1、`(0,0)`，并以相同的 production state readback
   验证恢复。

步骤 4 与 5 不读取 client event 缓存，也不调用测试专用 setter；因此该结论是 E 级。该流程待首次
CI 执行记录结果，当前未因本次实现而声称已通过。

## 已知边界 / 下一项结果

- headless output 没有固定 mode；测试以生产 headless 的 custom `1920×1080` mode 覆盖
  `set_custom_mode`。有固定 mode 的 GPU/X11 runner 可补 `mode/size/refresh/preferred/current_mode` event
  与 `set_mode` 的 I/E 路径。
- 尚未构造 stale serial 的 `cancelled`，以及 duplicate/unconfigured/used configuration 的 error；这些
  应由独立连接断言 protocol error，避免破坏主 E 链路。
- 尚未验证多 output 的相对布局、真正 disable/apply、adaptive-sync 的硬件结果，或 output hotplug
  导致的 head/mode `finished`。这些场景需要多 head 或支持 VRR 的 runner；scale/transform 已覆盖
  production state，不等同于最终像素 readback。
