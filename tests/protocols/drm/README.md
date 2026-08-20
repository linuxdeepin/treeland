# `drm` (`wl_drm`) 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/drm.xml` / `wl_drm` v2
- 测试源码：`tests/protocols/drm/`
- Fixture：完整生产 `Treeland` / `Helper`、headless output 与生产 render window；`Helper::init()` 直接调用 wlroots C API `wlr_drm_create()`，不添加 waylib wrapper
- 覆盖等级：**P / V（GPU 条件）**；这是 wlroots 的 legacy native global，不存在 Treeland 业务模块

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| global 初始公告 | bind `wl_drm` v2 并 roundtrip | 恰好一个非空 `device`、一个 `capabilities(PRIME)`，以及至少一个 `format` | P |
| render-node 认证 | `authenticate(0)` 并 roundtrip | 收到一个 `authenticated` event | P |
| 旧 flink buffer | `create_buffer(...)` | display 失败，错误为 `wl_drm.error.invalid_name` | P |
| 旧 planar flink buffer | `create_planar_buffer(...)` | display 失败，错误为 `wl_drm.error.invalid_name` | P |
| PRIME DMA-BUF | 用 `device` 指定的 render node 创建并填充红色 GBM BO，导出 FD 后 `create_prime_buffer(...)`，map 为 xdg toplevel | 生产 render window 采样该 buffer；`WTextureCapturer` 读回 `64×64` 内容纹理中心像素 RGBA `(255,0,0,255)` | V |

## 生产结果

测试 fixture 不重实现 `wl_drm`：它从 `WBackend` 创建 renderer 后直接调用
`wlr_drm_create(server->handle(), renderer)`。因此所有 event、错误和 resource 生命周期均由
`3rdparty/wlroots/types/wlr_drm.c` 的生产实现发出。
`wlr_drm_create()` 仅在 renderer 能提供 DRM FD 时创建 global；`wlr_drm` 复制创建时所需的
format/node 数据，并监听 `wl_display` 销毁。测试不拥有或手动销毁该 native global。

成功的 PRIME 路径为：

`wl_drm.device render node → GBM device → GBM BO → 导出的 DMA-BUF FD →
wl_drm.create_prime_buffer → wlroots wlr_drm_buffer / wl_buffer →
xdg toplevel map → ShellHandler / SurfaceWrapper / WSurfaceItemContent →
WOutputRenderWindow render → WTextureCapturer readback`。

`authenticate` 的 magic 为 `0`。wlroots 的现代 render-node 实现不需要 DRM master
authentication，处理函数无条件发送 `authenticated`；该断言验证 Treeland 公开的 native
global 确实按实现响应，而不是宣称测试了 `drmAuthMagic()`。

## 已知边界 / 下一项结果

若 renderer 没有 DRM FD、客户端无法打开 `device` event 中的 node、GBM device/BO 创建失败，
client 退出码为 `77`，CTest 标记为 **Skipped**。这不是通过，也不证明 PRIME 路径。

普通 headless CI 往往落入该分支；挂载可访问的 GPU render node，并提供 GBM 驱动的 runner
时，测试必须执行上述真实 DMA-BUF 路径，任何协议错误均使 target 失败。

本规范描述已实现的断言；新增后的首次 GPU runner 执行结果尚待记录。

- `wl_drm` 是已废弃的 legacy 协议；`linux-dmabuf` 才是现代多 plane/modifier DMA-BUF 的主要协议。
- PRIME request 已证明 valid single-plane GBM DMA-BUF 被 native `wl_drm` 接受、由 compositor
  renderer 采样，并在真实 `WSurfaceItemContent` texture readback 中保留红色像素。
- 读回的是 mapped surface 的生产 scene texture，而不是整个 output 的最终合成 backing buffer；
  多窗口叠放、output transform、色彩管理和 scanout 的最终像素仍需单独的 GPU output readback。
