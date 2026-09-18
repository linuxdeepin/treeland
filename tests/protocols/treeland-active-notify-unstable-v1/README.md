# `treeland-active-notify-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_active_notify_manager_v1` / `treeland_active_notify_v1`
- 测试源码：`tests/protocols/treeland-active-notify-unstable-v1/`
- Fixture：desktop integration fixture（`protocol_test_setup` 创建 headless output）
- 覆盖等级：**P**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| Seat notifier 创建 | `get_active_notify(seat)` | notifier 创建成功，roundtrip 无 crash |
| 无真实输入时不发事件 | 挂 listener 后 roundtrip | 不收到 `activity_changed` / `drag_changed` |
| notifier 重建 | `destroy` 后再次 `get_active_notify(seat)` | 新 notifier 创建成功，无残留事件 |

## 已证明的生产链路

基础用例证明协议接口的绑定和按 seat 创建 notifier 的路径正常。

客户端绑定 `treeland_active_notify_manager_v1` 后，`get_active_notify(seat)` 用真实
`wl_seat` 创建 notifier。headless 环境中没有真实的指针按键、滚轮和拖放事件，因此挂上
listener 后 roundtrip 不应收到任何事件；断言该边界可以捕获实现伪造事件的问题。
`destroy` 后重建 notifier 验证管理器不依赖单例 notifier、可重复创建。

服务端实现位于
`src/modules/active-notify/activenotifymanagerinterfacev1.h`、
`activenotifymanagerinterfacev1.cpp`；生产事件来源为 `src/seat/helper.cpp` 的
鼠标左键 / 滚轮跟踪（`handleLeftButtonStateChanged`、`handleWhellValueChanged`）
和拖放生命周期（`handleRequestDrag`、`handleRequestDragForSeat`）。

## 已知边界 / 下一项结果

- 真实输入驱动的事件路径（左键 active/inactive 配对、滚轮逐事件脉冲、
  started→dropped/cancelled 生命周期）：尚未覆盖。后续应新建仅在
  `TREELAND_ENABLE_UINPUT_PROTOCOL_TESTS=ON` 时注册的 uinput E2E target。
- 滚轮双轴抵消：`delta.x()+delta.y()==0`（如 x=-120、y=+120）时当前不发送事件，
  沿用 v1 的启发式；如需二维精确判断，应改为按事件主轴判定。
- `get_active_notify(null)` 的协议错误：未覆盖（错误会杀死客户端连接，需要
  连接级错误处理支持）。
- 多 notifier / 多 seat：多个 notifier 同时监听同一 seat 或不同 seat 的行为未验证。
- 与 v1 `treeland_dde_active_v1` 并行注册时的双发行为：未验证。
