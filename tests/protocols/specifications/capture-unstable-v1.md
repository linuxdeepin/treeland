# `treeland-capture-unstable-v1` 测试规范

## 范围

本协议有两层测试，不能把基础错误路径误认为实际捕获成功。

| 测试源码 | Fixture | 覆盖等级 | 用途 |
| --- | --- | --- | --- |
| `treeland-capture-unstable-v1/` | 仅 `CaptureManagerV1`，没有 selector/source | P | 选择权、资源和无 source 协议错误 |
| `treeland-capture-desktop-v1/` | 完整生产 `Helper`、headless output、`ShellHandler`、QML selector 与 mapped xdg 窗口 | E / V | 真实选择窗口、复制 frame，并读取客户端目标 buffer 像素 |

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| context 所有权 | 两个客户端创建 context | selector 的 busy/所有权规则生效 |
| 无 source | source ready 前调用 `capture` 或 `create_session` | 连接收到 XML 规定的协议错误 |

## 基础协议路径（P）

第一个客户端 context 发送 `select_source(OUTPUT, 0, 0, NULL)` 后成为正在选择 source
的 context，测试断言它不会收到 `source_failed`。第二个 context 随即发送
`select_source(WINDOW, 0, 0, NULL)`，生产 `CaptureSourceSelector` 必须发送
`source_failed(SELECTOR_BUSY)`。销毁第一个 context 后，第三个 context 发送
`select_source(REGION, 0, 0, NULL)`，不应再收到 busy 失败，证明销毁确实释放生产
selector 的选择状态。

另外两个全新客户端各自发送 `get_context` 后立即调用 `capture()` 或
`create_session()`。由于没有 ready source，生产 `CaptureContextV1` 必须在 display
上 post `WL_DISPLAY_ERROR_IMPLEMENTATION`；测试用
`wl_display_get_protocol_error()` 读取该真实协议错误。

## 实际捕获路径（E / V）

`treeland-capture-desktop-v1` 使用同一个客户端同时建立被捕获窗口和 capture
context，完整步骤如下。

1. 客户端创建 xdg-toplevel，等待并 ack 初次 `xdg_surface.configure`，然后提交
   `64×64`、`wl_shm/ARGB8888`、所有像素为 `0xffff0000`（不透明红）的 buffer。
2. 生产 `ShellHandler` 收到 mapped xdg surface，创建 `SurfaceWrapper`，加入
   `Workspace`，并由生产 QML `SurfaceContent` 创建内部的
   `WSurfaceItemContent`。
3. 客户端创建 `treeland_capture_context_v1`，发送
   `select_source(WINDOW, freeze=0, with_cursor=0, mask=NULL)`。这使生产
   `CaptureManagerV1` 建立 `contextInSelection`，而 `Helper` 的既有连接创建
   QML `CaptureSourceSelector`。
4. fixture 先断言该 `WSurfaceItemContent` 位于生产 output 的 paint-order、可见且为
   `64×64`。headless QPA 不稳定地投递 Qt Quick hover/pointer，因此测试用一个仅供
   protocol test 使用的 access bridge 调用 selector 自己的
   `setSelectedSource()`；它创建真实 `CaptureSourceSurface`，而不是向
   `CaptureContextV1::setSource()` 塞入伪造 source。
5. selector 的生产 `setSelectedSource()` 调用生产
   `CaptureContextV1::setSource()`。客户端必须收到
   `source_ready(..., width=64, height=64, source_type=WINDOW)`；fixture 同时断言
   source 的实际类型为 `CaptureSource::Surface`、尺寸为 `64×64`。
6. 客户端发送 `capture()`。协议 roundtrip 确认生产 `onCapture()` 与 selector
   `doneSelection()` 已执行；fixture 调用真实 `WOutputRenderWindow::render()`。
   `renderEnd` 驱动 `CaptureSource::createImage()`，由 `WTextureCapturer` 读取所选
   `WSurfaceItemContent` 的生产纹理。
7. 客户端收到 `treeland_capture_frame_v1.buffer(format, 64, 64, 256)` 后，以该
   format 创建自己的 `wl_shm` 目标 buffer，发送 `frame.copy(buffer)`。生产
   `CaptureSource::copyBuffer()` 把 image 写入此 buffer，并发送 `buffer_done` 与
   `ready`。
8. 客户端仅等待真实 `ready` 或 `failed` 事件。通过条件为：没有 `failed`、收到
   `buffer_done` 与 `ready`、目标为 `64×64`/stride `256`，且第一个像素按服务器
   宣告的 `ARGB8888` 或 `ABGR8888` 格式解码后为 RGBA `(255,0,0,255)`。

已证明的完整数据流为：

`wl_shm 红色窗口 buffer → xdg map → ShellHandler/SurfaceWrapper/QML SurfaceContent
→ CaptureSourceSelector 真实 source 提交 → CaptureSourceSurface → source_ready →
WTextureCapturer → frame.buffer → 客户端 wl_shm frame.copy → frame.ready → 红色像素`。

该 E/V 路径已通过本地执行。测试没有根据时间判断 selector、纹理或 frame 是否就绪：
状态推进分别由 Wayland roundtrip、服务端同步回调，以及
`source_ready`/`buffer`/`ready` 事件驱动。

## 已知边界 / 下一项结果

当前只验证 **window/surface 的 one-shot copy**。尚未覆盖：

- Qt Quick headless QPA 下真实鼠标 hover/release 到 `ItemSelector` 的命中分发；
- `OUTPUT`、`REGION` source 的选区和坐标裁剪；
- `freeze=1`、`with_cursor=1` 与 mask surface 的实际合成语义；
- persistent `create_session()` 的 dmabuf `frame/object/ready/frame_done` 流；
- output 的最终合成 backing buffer 像素（当前 window source 已是生产 scene
  texture，目标 buffer 像素已验证）。
