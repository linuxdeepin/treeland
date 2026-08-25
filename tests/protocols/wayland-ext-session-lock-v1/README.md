# `wayland-ext-session-lock-v1` 测试规范

## 范围

- XML / interface：`ext_session_lock_manager_v1` / `ext_session_lock_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-session-lock-v1/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_session_lock_manager_v1`（v1） | 资源创建成功 | P |
| 锁定会话 | `lock()` 后驱动事件循环 | 收到 `locked` 事件 | P |
| 解锁会话 | `unlock_and_destroy()` 后驱动事件循环 | 收到 `finished` 事件 | P |

## 生产结果

测试观察 Treeland 通过 `WSessionLockManager` 提供的会话锁服务。`lock` 后服务端真实锁定 headless
输出并下发 `locked`，`unlock_and_destroy` 后下发 `finished` 并恢复，证明会话锁生命周期完整接入。

## 已知边界 / 下一项结果

未创建 lock surface，故未验证锁屏表面渲染与 `get_lock_surface` 的 configure 协商（需 I/V 级）。
若 Treeland 对无 lock surface 的锁定有额外超时策略，可能需调整。
