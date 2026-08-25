# `wayland-security-context-v1` 测试规范

## 范围

- XML / interface：`wp_security_context_manager_v1` / `wp_security_context_v1`（version 1）
- 测试源码：`tests/protocols/wayland-security-context-v1/`
- Fixture：headless output fixture
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_security_context_manager_v1`（v1） | 资源创建成功 | P |
| 创建安全上下文 | `create_listener(listen_fd, close_fd)` + `set_app_id` + `commit` | 服务端将 listen_fd 加入事件循环 | P |
| 受信连接 | 经 listen_fd 建立第二连接 + 枚举 globals | 第二客户端收到全局列表（含 `wl_compositor`） | P |

## 生产结果

测试观察 Treeland 通过 `wlr_security_context_manager_v1_create` 提供的安全上下文服务。客户端
创建抽象 Unix 监听套接字并 commit 安全上下文后，服务端真实 accept 经该套接字的第二连接并下发
全局广播，证明 security-context 连接路径（fd 验证 + 受信客户端创建）完整接入。

## 已知边界 / 下一项结果

仅验证第二连接能枚举到全局；未断言 `set_app_id` / `set_sandbox_engine` 等元数据被 Treeland 记录
到客户端凭证（I 级需 invoke_on_server_thread 检查 client metadata）。
