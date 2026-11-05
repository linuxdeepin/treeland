# `wl_data_device` selection 焦点回归测试

- Fixture：完整 Treeland、真实 seat、两个纯 C Wayland 客户端。目标客户端映射两个
  toplevel，源客户端映射一个 toplevel；源通过 `zwlr_data_control_v1` 发布文本，目标
  通过标准 `wl_data_device` 的 selection offer 读取 pipe。
- 等级：E。焦点由 fixture 请求生产 `WaylandServer::requestKeyboardFocus()` 切换；
  所有数据传输均经过真实 Wayland 请求、source send 事件和 pipe 字节断言。

| 用例 | 可观察断言 |
| --- | --- |
| initial selection | MIME 含 text/plain，读回 UTF-8 原文且 source send 一次 |
| same-client focus before dispatch | 同一客户端切换 surface，尚未分发新 selection 时，旧 offer 仍能读取原文 |
| same-client focus after temporary clear | 焦点短暂清空后恢复同一客户端，排队的旧 offer receive 仍能读取原文 |
| other-client focus invalidates old offer | 切换到另一客户端后，旧 offer 只读到 EOF，不触发 source send |
| return to client gets valid offer | 焦点返回后，新 offer 可以正常读取 |
| replacement invalidates old offer / replacement payload | 旧 source 收到 cancelled，旧 offer 返回 EOF，新 offer 读到替换内容 |
| source destruction invalidates old offer | 销毁 source 后旧 offer 返回 EOF，目标收到 NULL selection |

同步采用服务端线程回调、Wayland roundtrip 和 source send 完成，不用固定延时。
客户端特意保留旧 proxy 以检查服务端失效语义，在测试结束时统一销毁。

此测试直接覆盖导致右键粘贴失败的 focus/offer 生命周期边界；实际 xdg-popup 的创建、
菜单点击和关闭顺序仍需 GUI 复测。未覆盖拖放、XWayland、primary selection 或大数据传输。

```sh
cmake --build --preset ci --target test_wayland_data_device
ctest --test-dir build -R '^test_wayland_data_device$' -V
```
