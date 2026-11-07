# `wayland-ext-image-copy-capture-v1` 测试规范

## 范围

- XML / interface：
  - 主：`ext_image_copy_capture_manager_v1` / `ext_image_copy_capture_session_v1`（version 1）
  - 附加：`ext_output_image_capture_source_manager_v1` / `ext_image_capture_source_v1`（version 1）
- 测试源码：`tests/protocols/wayland-ext-image-copy-capture-v1/`
- Fixture：headless output fixture（1920×1080）
- 覆盖等级：P

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 capture-source 管理器 + copy-capture 管理器 | 资源创建成功 | P |
| 创建输出源 | `create_source(wl_output)` | 返回非 NULL 的 capture source | P |

## 生产结果

测试观察 Treeland 通过 wlroots 提供的输出图像捕获服务：客户端从真实 headless `wl_output`
创建并销毁 capture source，证明 output-to-source 工厂路径可用，而不仅是两个 global 被公告。

## 暂停 / 恢复语义（多用户会话）

wm945 起 capture session 增加暂停能力，由 `CaptureManager` 按客户端 socket 的
 enable 状态驱动：

- 判定谓词：录制客户端所在 socket（`WSocket::rootSocket()`）不是 enabled 状态时暂停。
  该状态就是切换用户时被 `WSocket::setEnabled(false)` 关闭的会话 socket 状态，
  与 `WSocketPrivate::shutdown()` 冻结进程同一触发源。
  注意：`shutdown()` 只遍历自身 `clients`、不递归子 socket，所以沙箱子 socket 上的
  客户端会被暂停，但其进程不会被 `SIGSTOP`。
- 暂停期间：不向客户端 buffer 拷贝帧、不发 `damage` / `ready` / `failed`；
  damage 继续累积。恢复时若已知源尺寸则重置为整帧 damage（避免暂停期间
  resize 后越界），否则保留累积 damage，然后重新请求一帧。
- 恢复：重新 `request_frame`。若暂停前/暂停中已提交 `capture`，恢复后该帧完成；
  客户端无需重发请求。
- 全局（`dde` 哨兵）socket 不会被 disable，其上的客户端不会被暂停；
  无法判定 socket 的内部/测试客户端同样不暂停。
- 外部 Waylib 构建（`WITH_SUBMODULE_WAYLIB=OFF`）若其 wlroots 未包含该扩展 API，
  `CaptureManager` 会在启动时输出一条 `wlTlCapture` 警告并退化为「不暂停」。
- 光标子会话（`cursor_session`）的 enter/position/hotspot 不随暂停停止。

### 手动验证步骤（自动化用例受 #1407 阻塞）

1. 用户 A 启动录制客户端，确认持续出帧。
2. 切到用户 B：A 的录制画面停在最后一帧（客户端不再收到 `ready`）。
3. 切回 A：恢复出新帧，且首帧 damage 覆盖整帧。
4. 边界：暂停期间 resize 被捕获对象 / 分辨率变化，切回后首帧不应出现 damage 越界；
   暂停期间销毁并重建 frame；两个用户各自持有录制会话来回切换；长时间暂停后切回。

## 已知边界 / 下一项结果

不创建 capture session：headless 下进入 `wlr_output_configure_primary_swapchain()` 可能死锁
（issue #1407）。因此 `create_session()`、`buffer_size`、frame 创建、实际像素捕获（V 级）
以及上面的暂停/恢复语义均未纳入自动化用例；修复该问题后应在
`invoke_on_server_thread()` 中注入 `updateActiveUserSession()` 做会话切换激励，断言非活动
期间收不到 `ready`、切回后恢复。
