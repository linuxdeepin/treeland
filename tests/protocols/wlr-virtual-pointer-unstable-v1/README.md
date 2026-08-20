# `wlr-virtual-pointer-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-virtual-pointer-unstable-v1.xml` /
  `zwlr_virtual_pointer_manager_v1`、`zwlr_virtual_pointer_v1` v2。
- 测试接入：该 protocol target 的 `setup.cpp` 创建 wlroots native manager；每个
  `new_virtual_pointer` 被包装为 virtual `WInputDevice` 并 attach 到生产 primary `WSeat`。这不改变
  Treeland 默认公开的 global。
- 覆盖等级：待加入独立 protocol client 后为 **P / E**。

## 必须观察到的结果

| XML 语义 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| pointer 创建与 seat 路由 | `create_virtual_pointer_with_output(seat,output)` | production virtual `WInputDevice` attach 到 default seat，suggested output 进入 pointer mapping | E |
| motion/button/axis/frame | 依次发送 relative/absolute motion、button、axis/source/stop/discrete、frame | focused client 的真实 `wl_pointer` 收到匹配 event 与 frame 边界 | E |
| 生命周期 | destroy manager 后继续使用 pointer，再 destroy pointer | child 不被 manager 连带销毁，native input device 随 pointer destroy detach | P / E |
| 无效 axis/source | 独立连接传入非法 enum | `invalid_axis` / `invalid_axis_source` protocol error | P |

## 生产结果

wlroots 已实现 native manager，waylib 没有 wrapper。测试 fixture 的 listener 将 native pointer base
包装为 `WInputDevice(..., true)`，attach 到 primary seat，并在 native destroy signal 中 detach/delete
wrapper；这使 protocol event 进入既有 Treeland pointer/seat 分发，而不是停留在 wlroots signal 层。

### E 级覆盖流程

headless CI 创建 output 和 mapped pointer-focused client；另一 client 创建 virtual pointer 并注入事件。
测试必须由第一个 client 的 `wl_pointer` listener 验证 production seat 转发，不得由 fixture 直接调用
seat notify API。该流程不需要 GPU 或物理鼠标；授权策略、若引入，应另行以拒绝 client 验证。

## 已知边界 / 下一项结果

- 当前仅记录 fixture 接入方案；独立 C protocol client 与 fixture setup 尚待加入，不能把方案本身
  记为已执行的 request/event 覆盖。
- 目前 wlroots native manager 没有 trusted-client callback；所有连接 client 的创建被允许不等于授权测试。
