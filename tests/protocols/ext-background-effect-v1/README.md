# `ext-background-effect-v1` 测试规范

## 范围

- XML / interface：`ext_background_effect_manager_v1` / `ext_background_effect_surface_v1`（version 1，staging）
- 测试源码：`tests/protocols/ext-background-effect-v1/`
- 服务端实现：`waylib/src/server/wlroots_extra/wlr_ext_background_effect_v1.c`（vendored，来自 wlroots MR 5304）
  + `waylib/src/server/protocols/wbackgroundeffectmanagerv1.{h,cpp}`（`WBackgroundEffectManagerV1::surfaceBlurRegion`）
- Fixture：headless output fixture + xdg-toplevel-client（128×128 solid buffer 映射）
- 覆盖等级：E

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 `ext_background_effect_manager_v1`（v1） | 收到 `capabilities` 事件且仅含 `blur`（值 1） | P |
| 关联表面 | `get_background_effect(surface)` | 返回非 NULL 的 `ext_background_effect_surface_v1` | P |
| 设置区域 | `set_blur_region(10,10 40x40)` + `commit` | 回读真实 `SurfaceWrapper::blurRegion()` == (10,10 40x40) 且 `blur()` 为 true | E |
| 双缓冲保持 | 不再 `set_blur_region`，直接再 `commit` | 区域仍为 (10,10 40x40)（pending 状态不会在 commit 间被清空） | E |
| NULL 移除 | `set_blur_region(NULL)` + `commit` | 回读区域为空、`blur()` 为 false | E |
| 重新启用 | `set_blur_region(0,0 128x128)` + `commit` | 回读区域 == (0,0 128x128) | E |
| 销毁对象 | `ext_background_effect_surface_v1.destroy` + `commit` | 回读区域为空 | E |
| 重复关联错误 | 对同一 `wl_surface` 再次 `get_background_effect` | 连接收到 `background_effect_exists` 协议错误 | P |

## 生产结果

测试绑定真实的 `ext_background_effect_manager_v1` 全局并校验 bind 时下发的能力位；
映射真实 xdg_toplevel 后附加背景效果对象，依次驱动 set/commit/NULL/重启用/destroy 的
双缓冲状态流转。每次 commit 后通过 server bridge 回读生产 `SurfaceWrapper::blurRegion()`
（由 `Helper` 在 `WSurface::commit` 信号里经
`WBackgroundEffectManagerV1::surfaceBlurRegion()` 同步），证明请求确实到达真实合成器
surface 管线，且双缓冲状态在连续 commit 间保持（回归防护：vendored 实现曾因多余的
`pixman_region32_clear` 在第二次 commit 后丢失模糊区域）。最后对同一 surface 重复
`get_background_effect`，断言协议错误码为 `background_effect_exists`（致命错误，置于用例末尾）。

## 已知边界 / 下一项结果

未验证多矩形区域的逐矩形渲染遮罩与模糊像素效果（V 级）；blur 能力位关闭时的客户端
降级行为未覆盖。
