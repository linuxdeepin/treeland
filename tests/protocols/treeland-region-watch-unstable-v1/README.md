# `treeland-region-watch-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_region_watch_manager_v1` / `treeland_region_watch_v1`
- 测试源码：`tests/protocols/treeland-region-watch-unstable-v1/`
- Fixture：协议 fixture（`protocol_test_setup` 创建 headless 主输出 + 一个可供销毁的第二输出）
- 覆盖等级：**E**（覆盖 output 生命周期与 overlap 状态机）

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| set_region 记录区域 | `set_region(全主输出覆盖, top, 主输出)` | 服务端记录区域 = 输出布局位置 + 全覆盖条带（全局坐标）；无窗口时立即发送 `leave`（协议要求 set_region 后必须评估并回发当前状态） |
| enter | 创建并 map xdg-toplevel | 服务端 per-window rect 跟踪重算后发送 `enter` |
| leave | 销毁 toplevel | 重算后发送 `leave` |
| 多屏坐标 | 重绑到非原点第二输出（全覆盖） | 服务端区域 = 第二输出布局位置 + 全覆盖条带；主输出上的 toplevel 映射/关闭不触发任何 enter/leave（坐标不错位、不误报） |
| 重绑输出 | `set_region(100,40, top, 第二输出)` | 服务端区域更新为第二输出位置 + 条带，并重新发送 `leave` |
| output_removed | 服务端销毁第二输出 | 客户端收到 `output_removed`；服务端区域清空（inert） |
| inert 语义 | output_removed 后服务端触发一次 recheck | 不再发送任何 enter/leave/output_removed |
| manager.destroy | 销毁 manager 资源后继续 `set_region` | watcher 不受影响，区域重新记录并回发 `leave` |

## 已证明的生产链路

客户端绑定 `treeland_region_watch_manager_v1` 并 `get_region_watch` 创建 watcher；
`set_region` 在服务端按 anchor 计算输出边缘条带区域（左/右为全输出高的 width 厚条带，
上/下为全输出宽的 height 厚条带），并连接 `WOutput::beforeDestroy`。

窗口重叠检测走生产路径：`Helper::onSurfaceWrapperAdded` 为所有非 layer
`SurfaceWrapper` 维护 per-window rect 列表（几何/可见性变化经 300ms 去抖触发重算，
仅计入 XdgToplevel/XWayland），`TreelandRegionWatchManagerInterfaceV1::checkOverlapConflict`
对每个 watcher 求交集并按状态变化发送 `enter`/`leave`；`set_region` 后立即用当前窗口
列表评估一次。区域按输出在布局中的位置平移到全局坐标（`WOutput::position()`）后再与
窗口矩形比较。

服务端实现位于
`src/modules/region-watch/regionwatchmanagerinterfacev1.h`、
`regionwatchmanagerinterfacev1.cpp`，窗口 rect 跟踪位于 `src/seat/helper.cpp`。

## 已知边界 / 下一项结果

- 错误路径：`invalid_anchor` / `invalid_size` 为致命协议错误，框架不支持预期断连的
  case，未覆盖。
- 多 watcher：多个 watcher 同时监控不同区域的行为未单独验证（实现按 watcher 列表遍历，
  与单 watcher 一致）。
- 输出 mode/scale 变化：区域在 set_region 时快照，输出尺寸变化后需客户端重新
  set_region（协议语义），未覆盖动态变化场景。
