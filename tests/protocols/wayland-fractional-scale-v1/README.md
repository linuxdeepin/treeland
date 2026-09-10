# `wayland-fractional-scale-v1` 测试规范

## 范围

- XML / interface：`wp_fractional_scale_manager_v1` / `wp_fractional_scale_v1`（version 1）
- 测试源码：`tests/protocols/wayland-fractional-scale-v1/`
- Fixture：headless output fixture + xdg-toplevel-client（solid buffer 映射）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `wp_fractional_scale_manager_v1`（v1） | 资源创建成功 | P |
| 关联表面 | `get_fractional_scale(surface)` | 返回非 NULL 的 fractional-scale | P |
| 首选缩放 | 映射表面 + `commit` 后 roundtrip | 收到 `preferred_scale` 事件（scale>0） | P |
| 缩放值与真实输出一致 | 回读真实 `WOutput::scale()` | `preferred_scale` == round(`WOutput::scale()` × 120) | E |

## 生产结果

测试创建并映射一个真实 xdg_toplevel，为其附加 `wp_fractional_scale_v1` 对象并提交。
服务端下发 `preferred_scale` 事件后，测试通过 server bridge 回读 headless 输出对应的
真实 `WOutput::scale()`，断言 `preferred_scale` 等于 `round(scale × 120)`。wlroots 正是
使用该 `WOutput` 的 scale 值生成 `preferred_scale` 事件，因此此断言验证事件反映了真实
生产输出状态，而非仅事件到达且非 0。

## 已知边界 / 下一项结果

未验证非整数缩放下的实际像素缓冲重新分配（V 级像素读回亦未覆盖）。
