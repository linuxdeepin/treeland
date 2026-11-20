# `wayland-ext-image-copy-capture-v1` 测试规范

## 范围

- XML / interface：
  - 主：`ext_image_copy_capture_manager_v1` / `ext_image_copy_capture_session_v1`（version 1）
  - 附加：`ext_output_image_capture_source_manager_v1` /
    `ext_foreign_toplevel_image_capture_source_manager_v1` /
    `ext_image_capture_source_v1`（version 1）
  - 窗口夹具：`ext_foreign_toplevel_list_v1`、`xdg_shell`、`wl_subcompositor`、
    `zxdg_decoration_manager_v1`
- 测试源码：`tests/protocols/wayland-ext-image-copy-capture-v1/`
- Fixture：输出工厂测试使用 1920×1080 headless output；窗口捕获测试使用 800×600
  headless output、固定不透明背景、隐藏光标、关闭开/关窗动画和 output layers。
  最大化几何动画保持启用，按真实 `finished` 信号同步；指针设备经过 wlroots/seat/QtQuick
  驱动生产 SSD DragHandler，不经过 libinput/uinput。
- Targets：`test_wayland_ext_image_copy_capture_v1`（P）、
  `test_wayland_ext_toplevel_image_capture_v1`（E / V / P）
  和可选 `test_wayland_ext_toplevel_image_capture_v1_gpu`（同场景、GLES2、SHM 捕获读回）。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 绑定全局 | 绑定 capture-source 管理器 + copy-capture 管理器 | 资源创建成功 | P |
| 创建输出源 | `create_source(wl_output)` | 返回非 NULL 的 capture source | P |
| 创建窗口源及会话 | 从真实 foreign-toplevel handle 创建 source，再 `create_session` | ARGB8888、窗口局部边界对应的 `buffer_size`、`done` | P / E |
| SSD 与递归子面 | 提交主面、负坐标/above/below/nested 子面，申请并捕获 SHM frame | 主面、递归子面及堆叠顺序、标题栏和外边框的真实像素；间隙透明，不包含阴影、背景或遮挡窗口 | V |
| 不改变屏幕 | 捕获后先回读生产 output buffer，不先重绘；再单独重绘和定向 `render(false)` | 每个提交帧整屏逐像素相等；每次捕获 render 前后 live QSG 节点矩阵不变；wrapper/surface 的位置、尺寸、父项、缩放、可见性和场景坐标不变；捕获 input 是独立且拒绝输入的子树；离屏渲染不增加真实输出 `commit_seq` | E / V |
| 重复捕获 | 同一个 session 连续捕获静止窗口 | 整幅图像逐像素相等，包括透明外边框，不能累积半透明像素 | V |
| 子面独立更新 | 仅 desynchronized nested 子面提交新 buffer，根面不 commit | 捕获到子面的新颜色 | V |
| 部分移出屏幕 | fixture 移动窗口到 `(-150,210)` 后捕获 | 捕获图像仍完整且使用相同局部坐标；实际屏幕像素与捕获前一致、几何不变 | E / V |
| 移动与 SSD 坐标 | 多次移动到正坐标、左上负坐标、右下超出屏幕的位置 | 捕获整幅图像与移动前逐像素相等，包括 SSD；live QSG 状态和实际屏幕无捕获副作用 | E / V |
| 共享 source | 创建两个 session，销毁第一个后更新子面 | 第二个 session 仍收到 `ready` 和新像素 | P / V |
| 窗口 resize | 根面提交 320×260 buffer，先使用旧捕获 buffer，再使用新 buffer | 新 `buffer_size`；旧 buffer 收到 `buffer_constraints`，新 buffer 收到 `ready` 和新区域像素；屏幕和窗口几何不受捕获影响 | P / V |
| 最大化拖动还原 | 客户端 `set_maximized`，提交 configure 指定尺寸，再通过 SSD 按下/移动/释放指针 | 最大化图像完整；拖动还原但客户端尚未提交小 buffer 时仍完整捕获旧内容；还原 buffer 到达后约束缩小，整幅图像与还原前逐像素相等；继续拖动及释放不造成 SSD 漂移/截断 | E / V / P |
| 停止、重启及销毁 | 销毁所有 session 后重建，捕获期间销毁 toplevel | 重启后 frame 成功；窗口销毁后 session 收到 `stopped` | P / E |
| 捕获管线生命周期 | 只创建 source 不建 session；同一窗口连续 3 次 “source → session → 捕获 → 销毁 session 和 source”；会话期间提前销毁 source resource | 空闲 source 不创建离屏视口；每次会话恰好一条管线，销毁后捕获视口数回到 0 且屏幕不变；source resource 销毁后仍存活的 session 继续收到 frame | P / V |
| 请求之间的合成器渲染 | 两次捕获之间让捕获视口额外渲染一帧 | 下一次捕获仍收到 `ready`，不等待不会发生的渲染 | P / V |

## 生产结果

输出工厂测试观察 Treeland 通过 wlroots 提供的输出图像捕获服务：客户端从真实 headless
`wl_output` 创建并销毁 capture source。

窗口测试经过 Session 的生产 foreign-toplevel 工厂、`src/session/toplevelcapturesource.*`、
`src/session/toplevelcaptureviewport.*` 和 `WExtImageCaptureSourceV1Impl`，由纯 C 客户端
读取最终 SHM 像素和 frame 元数据（normal transform、有效 presentation time、damage、ready）。
捕获通过独立的只读 `WSurfaceItem` 递归渲染同一个 wl_surface tree，SSD 标题栏/边框也是
独立节点，不创建阴影。捕获不重父项、不移动 live item、不引用 live QSG 子树或屏幕区域。

捕获管线属于发起请求的客户端，而不是被捕获的窗口：`accept()` 只创建 client resource，
`start()` 在第一个 session 到来时才创建离屏视口，最后一个 `stop()` 同步释放视口，
client resource 与 session 都不存在时销毁 source 包装对象。因此空闲 source、反复截图和
中途销毁 source resource 都不会在网络窗口生命周期内累积 `WSurfaceItem` 副本、SSD、
swapchain 或渲染窗口的 output helper。释放后 `source.width/height` 与 SHM 格式保留，
下个 session 在首次渲染前就能拿到正确约束；swapchain 派生的 DMA-BUF 约束在重建后由
首帧刷新。`request_frame` 不依赖 wlroots 的 “已有待处理帧” 提示，离屏视口按需渲染，
避免请求之间发生合成器渲染时客户端永远等不到 frame。
夹具提供无标准客户端请求的窗口定位、指针设备事件和生产屏幕/几何/QSG 回读，
不伪造 source、frame、maximize 或 restore 结果。

每次捕获 renderer 的 `beforeRendering` / `afterRendering` 直接比较 live 节点矩阵；
这样动画导致的正常几何变化不被误判，也不会先强制重绘屏幕把共享节点污染覆盖掉。
整屏比较在预计不变的阶段逐提交帧执行，预期变化阶段暂停比较后重新取基线。
销毁窗口前暂停“不应变化”的像素比较，窗口引用使用 `QPointer`；捕获期间的观察器
不使用已销毁窗口的裸指针。屏幕基线固定读回 ARGB8888 并检查图像分配/读回结果，
不把 renderer 的 preferred format 直接传给可能不支持它的 QImage 转换。

在创建任何捕获 source/session 之前，先在相同正/负坐标移动窗口并重复渲染，断言对照场景
稳定。框架禁用了壁纸进程，因此必须提供固定不透明背景：软件 renderer 保留 output
buffer 时，没有背景会留下旧窗口像素，并在重复重绘时叠加阴影，造成与捕获无关的
差异。测试不通过屏蔽阴影区域、设置像素容差或跳过整屏对比来规避这个问题。
指针光标在基线建立之前隐藏；保留指针设备和真实事件路径，不通过屏蔽光标像素规避差异。

800×600 输出由生产 `OutputManager` 完成注册和启用；夹具不提前启用它，避免框架先
将其加入 layout、随后生产移除/重新注册时销毁仍在使用的 cursor layers。

## 运行与取证

```bash
cmake --build build --target test_wayland_ext_toplevel_image_capture_v1
ctest --test-dir build -R '^test_wayland_ext_toplevel_image_capture_v1$' -V
ctest --test-dir build -R '^test_wayland_ext_toplevel_image_capture_v1$' \
  --repeat until-fail:20 --output-on-failure

# 有可用 DRM render node 的机器上检查相同场景的 GPU 渲染路径
cmake --preset ci -DTREELAND_ENABLE_GPU_PROTOCOL_TESTS=ON
cmake --build build --target test_wayland_ext_toplevel_image_capture_v1_gpu
ctest --test-dir build -R '^test_wayland_ext_toplevel_image_capture_v1_gpu$' -V
```

设置 `TREELAND_CAPTURE_TEST_IMAGE=/tmp/opencode/toplevel-capture.pam` 可保存初始捕获图像；
屏幕对比失败时，同时保存该路径后缀为 `.before.png` / `.after.png` 的原始整屏图像。

### 本次验证证据

- `ci` preset（`-Wall -Wextra -Werror`）构建通过，无告警。
- headless + pixman：包含生命周期与“请求之间额外渲染”用例的最终版本连续 20 次通过。
- headless + GLES2：Intel UHD Graphics 630（i915）、OpenGL ES 3.2 / Mesa 26.2.6，
  同一版本连续 20 次通过，非 skip、非软件 fallback。
- 捕获、SSD、递归子面、foreign-toplevel、window-transition、alpha-modifier、decoration、
  remote-subsurface、input-manager 等相关 10 项回归全部通过。
- 尚无可访问的运行中 Treeland 桌面；物理 DRM/KMS scanout 和完整 libinput/uinput
  输入链路未验收，以上 headless 结果不能替代用户桌面的捕获瞬间验证。

## 已知边界 / 下一项结果

输出工厂 target 仍不创建 output capture session：headless 下进入
`wlr_output_configure_primary_swapchain()` 的历史死锁（issue #1407）未由本窗口测试验证。
窗口捕获使用独立的离屏 swapchain，不受此输出源测试边界限制。

GPU target 没有可访问 render node 时以 `77` skip，不允许以软件 fallback 代替 GPU 成功。
`QDir::System` 预检只检查节点能否打开；成功后仍必须通过 GLES2 renderer 和像素断言，
不能仅凭节点存在宣告 GPU 通过。GPU target 仍使用 SHM 客户端目的 buffer，
尚未覆盖客户端 DMA-BUF、fractional scale、多屏/输出销毁、minimized/完全离屏窗口、
popup、cursor capture 和跨 Session 拒绝路径。
