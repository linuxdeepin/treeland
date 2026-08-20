# `RenderedOutputFixture` 测试规范

该 fixture 建立在 `DesktopIntegrationFixture` 之上，用于验证客户端 buffer 已进入真实
Wayland/QtQuick 渲染对象链。它是 capture、壁纸与色彩测试的共同前置条件，而不是
这些协议业务结果的替代品。

## 建立过程与基线断言

1. 复用完整生产 `Treeland`、`Helper`、已启动的 headless backend、root output 和
   `WOutputRenderWindow`。
2. 客户端创建、configure 并 map xdg-toplevel；它提交一个 `64×64`、每个像素均为
   `ARGB 0xffff0000` 的 `wl_shm` buffer。
3. fixture 断言 `ShellHandler` 已创建 wrapper，且 wrapper 已加入 `Workspace`。
4. 通过 `SurfaceWrapper::surfaceItem()->findItemContent()` 在真实 QML
   `SurfaceContent` delegate 树中取得 `WSurfaceItemContent`，确认其所属
   `WOutputRenderWindow` 存在。
5. 调用该生产 render window 的 `render()`，再以 `WTextureCapturer` 从真实
   `WSurfaceItemContent` 的 texture provider 异步读回 `QImage`。
6. 断言读回图像为 `64×64`，中心像素 RGBA 为 `(255,0,0,255)`。

## 已证明的链路

`wl_shm ARGB buffer → wl_surface commit → xdg-toplevel map → ShellHandler →
SurfaceWrapper → WSurfaceItemContent → WOutputRenderWindow scene texture →
WTextureCapturer readback`。

## 当前边界

读回对象是 window 的生产 scene texture，不是整个 output 的最终合成 backing buffer。
这已证明待捕获窗口的真实纹理可用。`treeland-capture-desktop-v1` 已在此基础上通过
`CaptureSourceSelector` 选择该 `WSurfaceItemContent`，并验证协议 copy 到客户端
buffer 的像素。壁纸/output 色彩测试若需要全 output 结果，还须增加 output viewport
或 backing-buffer 的读回。
