# `wayland-idle-inhibit-unstable-v1` 测试规范

## 范围

- XML / interface：`zwp_idle_inhibit_manager_v1` / `zwp_idle_inhibitor_v1`（version 1，无事件）
- 测试源码：`tests/protocols/wayland-idle-inhibit-unstable-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（solid buffer 映射）
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `zwp_idle_inhibit_manager_v1`（v1） | 资源创建成功 | P |
| 创建抑制器 | `create_inhibitor(已映射 surface)` | 返回非 NULL 的 inhibitor | P |
| 资源存活 | roundtrip 后销毁 inhibitor | 无协议错误 | P |

## 生产结果

测试观察 Treeland 通过 `wlr_idle_inhibit_v1_create` 提供的空闲抑制服务。该接口无事件，
故以真实已映射 surface 上 `create_inhibitor` 返回有效资源且存活为协议级证据，证明
空闲抑制管理器接入 surface 输出关联路径。

## 已知边界 / 下一项结果

未验证抑制器对空闲计时器的实际抑制效果（idle-inhibit 对接旧 wlr_idle，与 ext-idle-notify 的
wlr_idle_notifier_v1 为独立系统，故未做交叉时序断言）。下一项可结合 invoke_on_server_thread
检查 `wlr_idle_inhibit_v1_is_inhibited`（I 级）。
