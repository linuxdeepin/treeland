# `wayland-ext-session-lock-v1` 测试规范

## 范围

- XML / interface：`ext_session_lock_manager_v1` / `ext_session_lock_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-session-lock-v1/`
- Fixture：headless output fixture
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_session_lock_manager_v1`（v1） | 资源创建成功 | P |
| 锁定会话 | `lock()` + 等待 grace timer | 收到 `locked` 事件 | P |
| 真实锁定状态 | 回读真实 `WSessionLock::isLocked()` | 为 true | E |

## 生产结果

测试绑定 `ext_session_lock_manager_v1` 并调用 `lock()`。Treeland 的
`Helper::onExtSessionLock` 启动 300 ms grace timer，超时后调用 `WSessionLock::lock()`，
向客户端发送 `locked` 事件并将生产 `WSessionLock` 对象置为 Locked 状态。测试在收到
`locked` 事件后，通过 server bridge 回读 `WSessionLockManager::lockCreated` 信号捕获的
生产 `WSessionLock::isLocked()`，断言为 true。此断言验证锁定确实到达了真实合成器
session-lock 对象，而非仅事件到达无协议错误。

若合成器拒绝锁定（如已有锁屏可见），则发送 `finished` 事件；此时测试回退到 P 级通过。

## 已知边界 / 下一项结果

未验证锁屏 surface 的创建与渲染（需要 lock surface + buffer），也未验证 unlock 后
`isLocked()` 翻回 false（V 级像素读回亦未覆盖）。
