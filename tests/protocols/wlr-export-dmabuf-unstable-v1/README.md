# `wlr-export-dmabuf-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-export-dmabuf-unstable-v1.xml`，
  `zwlr_export_dmabuf_manager_v1` v1 与 `zwlr_export_dmabuf_frame_v1` v1。
- 测试源码：`tests/protocols/wlr-export-dmabuf-unstable-v1/`。
- Fixture：真实 Treeland desktop integration fixture、headless `wl_output` 与 mapped
  红色 xdg surface。
- 覆盖等级：CI 为 P / E；GPU runner 为 P / V。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 标准 CI（pixman） | bind manager/output；在真实 mapped xdg surface 后以 `overlay_cursor=1` 调用 `capture_output` | 下一次生产 output commit 没有 DMA-BUF，收到唯一 `cancel(temporary)`；没有 `frame/object/ready`，随后销毁 frame 与 manager | P / E |
| GPU runner | 同一 capture 流程 | 收到严格的 `frame → object×num_objects → ready`；尺寸、object 索引、FD、size/stride、timestamp 与顺序有效；客户端复制后关闭每个协议 FD，将 DMA-BUF 作为 EGLImage 导入、离屏采样并断言中心像素为红色 | P / V |
| 无可导出的 GPU backing buffer | GPU runner 发起 capture 后收到 cancel，或无法创建 headless output | 返回 77，CTest 标记 Skip，不将取消路径当作 DMA-BUF 成功 | P |

## 生产结果

测试 fixture 的 `setup.cpp` 直接创建 wlroots native manager。waylib 当前没有该协议 wrapper；native
manager 由 `wl_display` 销毁，且没有 Treeland 业务回调或额外所有权。标准 CI 的取消 event
来自真实 production output commit；GPU runner 的 frame/object/ready 及 EGL import/readback
则验证 native export 到 client 端导入的完整链路。

### GPU runner 启用方式

默认 CI 仅注册 `test_wlr_export_dmabuf_unstable_v1`，环境固定为 pixman，因此取消事件是
确定性生产行为。GPU job 配置时显式启用：

```bash
cmake --preset ci -DTREELAND_ENABLE_GPU_PROTOCOL_TESTS=ON
cmake --build --preset ci --target test_wlr_export_dmabuf_unstable_v1_gpu
ctest --test-dir build -R '^test_wlr_export_dmabuf_unstable_v1_gpu$' --output-on-failure
```

GPU target 使用 `WLR_RENDERER=gles2`；只有 runner 的 EGL/driver 与 headless output 真正
产出 DMA-BUF 时才要求 `ready` 与 EGL import/readback。任一能力不可用时返回 77。

## 已知边界 / 下一项结果

- 生产 wlroots manager 未提供 trusted-client/authorization hook；该 global 对可连接的
  Wayland client 可用。权限策略须在注册该 global 前另行实现。
- 尚未构造 output disable/destroy 或 resize，因此未逐项断言 `cancel(permanent)` 与
  `cancel(resizing)`。
