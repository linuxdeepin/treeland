# `treeland-region-watch-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_region_watch_manager_v1` / `treeland_region_watch_v1`
- 测试源码：`tests/protocols/treeland-region-watch-unstable-v1/`
- Fixture：协议 fixture（`protocol_test_setup` 创建单个 headless 主输出）
- 覆盖等级：**E**（当前仅覆盖单输出 overlap 状态机；多输出/output_removed 待完善，见文末 TODO）

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| set_region 记录区域 | `set_region(全主输出覆盖, top, 主输出)` | 服务端记录区域 = 输出布局位置 + 全覆盖条带（全局坐标）；无窗口时立即发送 `leave`（协议要求 set_region 后必须评估并回发当前状态） |
| enter | 创建并 map xdg-toplevel | 服务端 per-window rect 跟踪重算后发送 `enter` |
| leave | 销毁 toplevel | 重算后发送 `leave` |
| 条带区域 | `set_region(100,40, top, 主输出)` | 服务端区域更新为主输出位置 + 条带，并重新发送 `leave` |
| manager.destroy | 销毁 manager 资源后继续 `set_region` | watcher 不受影响，区域重新记录并回发 `leave` |

## 已证明的生产链路

客户端绑定 `treeland_region_watch_manager_v1` 并 `get_region_watch` 创建 watcher；
`set_region` 在服务端按 anchor 计算输出边缘条带区域（左/右为全输出高的 width 厚条带，
上/下为全输出宽的 height 厚条带），并连接 `WOutput::beforeDestroy`。

窗口重叠检测走生产路径：`Helper::onSurfaceWrapperAdded` 对非 layer
`SurfaceWrapper` 调用 `TreelandRegionWatchManagerInterfaceV1::addSurface`，由
region-watch 模块内部维护 per-window rect 列表（几何/可见性变化经 300ms 去抖触发重算，
仅计入 XdgToplevel/XWayland）；`checkOverlapConflict` 对每个 watcher 求交集并按状态变化
发送 `enter`/`leave`；`set_region` 后通过 `recheck()` 立即用当前窗口列表评估一次。区域按
输出在布局中的位置平移到全局坐标（`WOutput::position()`）后再与窗口矩形比较。

## 同步方式

enter/leave 由生产侧 300ms 去抖定时器异步发出，属框架禁止的“固定延时后猜测状态”
场景。测试改用与定时器完全相同的生产评估路径：server-bridge 在服务端线程同步调用
`manager->recheck()`，`invoke_on_server_thread`
返回即完成边界（评估与事件发送已在同一次调用内完成），随后一次 roundtrip 投递事件。
否定断言（inert、跨输出不误报）同样以一次注入评估 + roundtrip 为观察边界：事件只
会在 `evaluate()` 中发出，评估后计数未变即证明无事件。全程无固定延时、无重试轮询。

服务端实现位于
`src/modules/region-watch/regionwatchmanagerinterfacev1.h`、
`regionwatchmanagerinterfacev1.cpp`（含窗口 rect 跟踪与去抖定时器）。

## 已知边界 / 下一项结果

- **TODO(multi-output/output_removed)**：第二输出、多屏坐标平移、`output_removed` 与
  inert-until-set_region 语义尚未覆盖。补齐前需先让 fixture 的第二 headless 输出接入
  输出布局（有 `WOutput` 包装与布局位置），对应 `setup.cpp` /
  `treeland-region-watch-unstable-v1.c` 中的 TODO 注释。
- 错误路径：`invalid_anchor` / `invalid_size` 为致命协议错误，框架不支持预期断连的
  case，未覆盖。
- 多 watcher：多个 watcher 同时监控不同区域的行为未单独验证（实现按 watcher 列表遍历，
  与单 watcher 一致）。
- 输出 mode/scale 变化：区域在 set_region 时快照，输出尺寸变化后需客户端重新
  set_region（协议语义），未覆盖动态变化场景。
