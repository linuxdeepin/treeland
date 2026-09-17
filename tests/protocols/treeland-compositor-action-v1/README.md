# `treeland-compositor-action-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-compositor-action-v1/`
- Fixture：协议 fixture（headless treeland，默认 2 个工作区）
- 覆盖等级：**E**（工作区切换与 show-desktop 的真实生产状态）+ **P**（绑定、忽略语义、生命周期）

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| 绑定 | 连接并绑定 `treeland_compositor_action_v1` | 特权全局对象存在，绑定成功（fixture 未设置 `DDM_DISPLAY_MANAGER`，不启用会话 socket 过滤） |
| 切换工作区 | `trigger(workspace_2)` | 生产 `Workspace::currentIndex()` 变为 1 |
| prev/next | `trigger(prev_workspace)` / `trigger(next_workspace)` | 生产索引在 0/1 间正确移动 |
| 越界工作区 | `trigger(workspace_12)`（fixture 仅 2 个工作区） | 索引保持不变，`switchTo()` 忽略越界索引，对应协议 "无法执行即忽略" |
| show desktop | `trigger(show_desktop)` ×2 | 生产 `Helper::showDesktopState()` 在 SHOW ↔ NORMAL 间切换 |
| 不支持的动作 | `trigger(zoom_in)` | 动作被忽略；连接保持可用（无协议错误） |
| 未知动作 | `trigger(999)` | 同上 |
| 销毁 | `destroy` + roundtrip | 对象销毁后连接仍可用 |

## 已证明的生产链路

客户端发送 `trigger` 后，服务端在同一轮消息分发中同步路由到
`Helper::handleCompositorAction`（特权全局注册于 `Helper::init`，绑定授权使用
waylib 全局过滤器，与其它 `dde/` 特权协议一致）。测试通过 server bridge 读取
真实 `Workspace::currentIndex()` 与 `Helper::showDesktopState()`，因此证明的是
动作对合成器真实状态的影响，而非接口回显。协议为一次性 fire-and-forget，无任何
回传事件，测试同样验证了不支持/未知动作不会断开客户端。

## 未覆盖

以下分支仅由守卫短路，headless fixture 无法产生可观察的生产状态，故不进入断言：

- 锁屏/关机菜单/用户切换动作：headless 环境无锁屏实现，`isLockScreenAvailable()`
  为假，动作被安全忽略（含聚焦关机菜单变体，其当前降级为普通菜单，见
  `Helper::handleCompositorAction` 注释）。
- 多任务视图动作：headless 未加载 multitaskview 插件，`m_multitaskView` 为空，
  open/close/toggle 均按空守卫忽略。
- 特权拒绝路径：需要 `DDM_DISPLAY_MANAGER` 环境与非会话 socket 的第二客户端，
  过滤行为由 waylib 全局过滤器承担，与其它特权协议一致。
- FPS 覆盖层：无渲染级断言。
