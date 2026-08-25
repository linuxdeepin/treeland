# `wayland-fractional-scale-v1` 测试规范

## 范围

- XML / interface：`wp_fractional_scale_manager_v1` / `wp_fractional_scale_v1`（version 1）
- 测试源码：`tests/protocols/wayland-fractional-scale-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（solid buffer 映射）
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_fractional_scale_manager_v1`（v1） | 资源创建成功 | P |
| 关联表面 | `get_fractional_scale(surface)` | 返回非 NULL 的 fractional-scale | P |
| 首选缩放 | 映射表面 + `commit` 后 roundtrip | 收到 `preferred_scale` 事件（scale>0） | P |

## 生产结果

测试观察 Treeland 通过 `wlr_fractional_scale_manager_v1_create` 提供的分数缩放服务。
已映射 surface 提交后服务端下发 `preferred_scale`，证明分数缩放接入 surface 输出关联路径。

## 已知边界 / 下一项结果

仅验证 scale 事件到达且非 0；未断言具体缩放值（取决于 headless 输出 scale），也未验证
非整数缩放下的实际像素缓冲重新分配（V 级）。
