# `wayland-security-context-v1` 测试规范

## 范围

- XML / interface：`wp_security_context_manager_v1` / `wp_security_context_v1`（version 1）
- 测试源码：`tests/protocols/wayland-security-context-v1/`
- Fixture：headless output fixture
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_security_context_manager_v1` | 资源创建成功 | P |
| 创建监听器 | `create_listener(listen_fd, close_fd)` | 返回非 NULL `wp_security_context_v1` | P |
| 设置 app_id | `set_app_id("test-app")` | 无协议错误 | P |
| 提交 | `commit` | 无协议错误 | P |
| 第二连接 | 通过监听套接字连接 | 第二客户端枚举到 wl_compositor 等全局 | P |
| **生产回读** | — | 回读真实 `wlr_security_context_manager_v1::events.commit` 捕获的 `app_id` == "test-app" | **E** |

## 生产结果

测试观察 Treeland 通过 `WSecurityContextManager` 提供的安全上下文服务。
客户端 commit 安全上下文后，setup 通过 `WSecurityContextManager::handle()` 获取生产
manager 指针，并 `wl_signal_add` 监听
`events.commit` 信号，
回读捕获的 `app_id` 断言为 "test-app"，证明安全上下文提交到达生产管理器。

## 已知边界 / 下一项结果

waylib 实现与测试共同使用
`waylib/src/server/protocols/private/wsecuritycontextmanager_p.h` 中的私有布局定义；该定义不能替换为
vendored wlroots 的同名类型，因为 waylib 的 manager 额外包含 `new_client` 信号。
未验证 `sandbox_engine` / `instance_id` 字段与 `new_client` 信号。
