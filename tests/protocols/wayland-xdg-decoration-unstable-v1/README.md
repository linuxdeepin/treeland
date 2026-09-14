# `wayland-xdg-decoration-unstable-v1` 测试规范

## 范围

- XML / interface：`zxdg_decoration_manager_v1` / `zxdg_toplevel_decoration_v1`（version 2）
- 测试源码：`tests/protocols/wayland-xdg-decoration-unstable-v1/`
- Fixture：headless output fixture + 映射 xdg-toplevel
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zxdg_decoration_manager_v1` | 资源创建成功 | P |
| 获取装饰 | `get_toplevel_decoration(toplevel)` | 返回非 NULL `zxdg_toplevel_decoration_v1` | P |
| 设置模式 | `set_mode(Client)` | 收到 `configure` 事件含 mode | P |
| **生产回读** | — | 回读真实 `WXdgDecorationManager::modeBySurface(wsurface)` 返回 Client | **E** |

## 生产结果

测试观察 Treeland 通过 `WXdgDecorationManager` 提供的 toplevel 装饰服务。
客户端 `set_mode(Client)` 后，setup 通过 `WXdgDecorationManager::modeBySurface` 
回读真实生产装饰模式，断言为 Client，证明装饰模式设置到达生产装饰管理器。

## 已知边界 / 下一项结果

仅验证 Client 模式；未验证 Server / None 模式切换与 `unset_mode` 行为。
