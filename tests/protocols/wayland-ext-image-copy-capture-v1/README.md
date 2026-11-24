# `wayland-ext-image-copy-capture-v1` 测试规范

## 范围

- XML / interface：
  - 主：`ext_image_copy_capture_manager_v1` / `ext_image_copy_capture_session_v1`（version 1）
  - 附加：`ext_output_image_capture_source_manager_v1` /
    `ext_foreign_toplevel_image_capture_source_manager_v1` /
    `ext_image_capture_source_v1`（version 1）
  - 窗口夹具：`ext_foreign_toplevel_list_v1`、`xdg_shell`、`wl_subcompositor`、
    `zxdg_decoration_manager_v1`、`zwlr_output_power_manager_v1`、`treeland_ddm_v1`
- 测试源码：`tests/protocols/wayland-ext-image-copy-capture-v1/`
- Fixture：输出工厂测试使用 1920×1080 headless output；窗口捕获测试使用 800×600
  headless output、固定不透明背景、隐藏光标、关闭开/关窗动画和 output layers。
  最大化几何动画保持启用，按真实 `finished` 信号同步；指针设备经过 wlroots/seat/QtQuick
  驱动生产 SSD DragHandler，不经过 libinput/uinput。关屏场景中再添加一块 800×600 输出，
  将窗口移到第二块屏，再通过真实 power-management 请求关闭捕获管线的原输出。
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
| 离屏渲染失败及恢复 | 锁住真实捕获 swapchain 的全部空闲 buffer，两会话提交 frame 后执行生产离屏渲染，再释放 buffer 并重新捕获 | 每个挂起 frame 恰好收到一次 `failed(unknown)`，不收到 `ready` 或 `stopped`；共享视口保留，两会话恢复后均收到 `ready`，像素正确 | P / V |
| 窗口 resize | 根面提交 320×260 buffer，先使用旧捕获 buffer，再使用新 buffer | 新 `buffer_size`；旧 buffer 收到 `buffer_constraints`，新 buffer 收到 `ready` 和新区域像素；屏幕和窗口几何不受捕获影响 | P / V |
| 最大化拖动还原 | 客户端 `set_maximized`，提交 configure 指定尺寸，再通过 SSD 按下/移动/释放指针 | 最大化图像完整；拖动还原但客户端尚未提交小 buffer 时仍完整捕获旧内容；还原 buffer 到达后约束缩小，整幅图像与还原前逐像素相等；继续拖动及释放不造成 SSD 漂移/截断 | E / V / P |
| 停止、重启及销毁 | 销毁所有 session 后重建，捕获期间销毁 toplevel | 重启后 frame 成功；窗口销毁后 session 收到 `stopped` | P / E |
| 捕获管线生命周期 | 只创建 source 不建 session；同一窗口连续 3 次 “source → session → 捕获 → 销毁 session 和 source”；会话期间提前销毁 source resource | 空闲 source 不创建离屏视口；每次会话恰好一条管线，销毁后捕获视口数回到 0 且屏幕不变；source resource 销毁后仍存活的 session 继续收到 frame | P / V |
| 组件定义复用 | 重建捕获会话并检查生产 capture input 的 delegate | delegate 始终为 QmlEngine 的同一个 CaptureSurfaceContent 缓存组件；具体实例及视口仍随会话释放，SSD 和整图像素不变 | P / V |
| 请求之间的合成器渲染 | 两次捕获之间让捕获视口额外渲染一帧 | 下一次捕获仍收到 `ready`，不等待不会发生的渲染 | P / V |
| 待处理请求遇到关屏 | 同一批请求中先 `capture`，再对原输出 `set_mode(OFF)`；窗口已移到第二块屏 | 原输出 disabled、第二块屏 enabled，待处理 frame 仍收到 `ready`，视口未重建 | P / E |
| 关屏后继续捕获 | 原输出关闭后提交新的 nested 子面 buffer，再申请 frame | 收到 `ready` 和新颜色像素；兜底渲染的 `renderEnd` 不包含任何提交输出，各输出 `commit_seq` 不增加 | P / V |
| 关屏时渲染失败及恢复 | 原输出关闭时再次耗尽捕获 swapchain，提交 frame，释放 buffer 后重试 | frame 收到 `failed(unknown)`，同一 session/视口恢复后捕获成功，输出保持关闭且未被离屏渲染提交 | P / V |
| 复电恢复 | 对原输出 `set_mode(ON)` 后再捕获 | 通过原有输出时钟继续收到 `ready` | P / E |
| 取消排队的兜底 | 关屏后同一批请求中申请 frame，再销毁 frame、session 和 source | 视口回到 0，已销毁的视口不再产生兜底渲染 | P / E |
| 关屏后的新会话 | 原输出仍关闭、窗口位于第二块屏时，创建全新的 source/session 并捕获 | 新视口绑定当前启用输出，首次约束有效，收到 `ready` 和正确像素，销毁后视口回到 0 | P / V |
| 全局渲染暂停与恢复 | 输出启用和关闭时分别通过 DDM `disable_render` 暂停，仅提交一次 `capture`，再 `enable_render` | 暂停时不绘制、不发送 ready/failed/stopped；恢复后原帧收到 ready，视口未重建，关屏路径仍不提交真实输出 | P / V |
| 暂停期间取消 | 暂停后销毁 frame/session/source，再恢复渲染 | 离屏视口归零，恢复不增加捕获渲染次数 | P / E |
| 暂停期间窗口失效 | 暂停后提交 frame，再销毁 toplevel | frame 收到 stopped 失败、session 收到 stopped；恢复不渲染已销毁视口 | P / E |

## 生产结果

输出工厂测试观察 Treeland 通过 wlroots 提供的输出图像捕获服务：客户端从真实 headless
`wl_output` 创建并销毁 capture source。

窗口测试经过 Session 的生产 foreign-toplevel 工厂、`src/session/toplevelcapturesource.*`、
`src/session/toplevelcaptureviewport.*` 和 `WExtImageCaptureSourceV1Impl`，由纯 C 客户端
读取最终 SHM 像素和 frame 元数据（normal transform、有效 presentation time、damage、ready）。
捕获通过独立的只读 `WSurfaceItem` 递归渲染同一个 wl_surface tree，SSD 标题栏/边框也是
独立节点，不创建阴影。捕获不重父项、不移动 live item、不引用 live QSG 子树或屏幕区域。
CaptureSurfaceContent 与 Border 的 QQmlComponent 定义由 QmlEngine 持有；捕获视口
复用定义，但仍创建自己的内容、标题栏和边框实例。Border 通过统一工厂创建，
不传递该 QML 类型不存在的 surface 属性。该检查证明组件对象复用，不是编译耗时基准。

捕获管线属于发起请求的客户端，而不是被捕获的窗口：`accept()` 只创建 client resource，
`start()` 在第一个 session 到来时才创建离屏视口，最后一个 `stop()` 同步释放视口，
client resource 与 session 都不存在时销毁 source 包装对象。因此空闲 source、反复截图和
中途销毁 source resource 都不会在网络窗口生命周期内累积 `WSurfaceItem` 副本、SSD、
swapchain 或渲染窗口的 output helper。释放后 `source.width/height` 与 SHM 格式保留，
下个 session 在首次渲染前就能拿到正确约束；swapchain 派生的 DMA-BUF 约束在重建后由
首帧刷新。`request_frame` 不依赖 wlroots 的 damage 调度提示，离屏视口按需渲染，
避免请求之间发生合成器渲染时客户端永远等不到 frame。
输出启用时沿用输出 frame 时钟；输出关闭时排队执行 `render(false)`，不重建管线，
也不向屏幕提交。`enabledChanged` 会重新安排尚未完成的请求，渲染重入则等当前
`renderEnd` 后再调度；最后一个 session 停止或 source 失效时清除排队状态。
全局 `renderEnabled=false` 表示暂停而非捕获失败：保留挂起请求，不轮询重试；
恢复渲染时通过状态变化通知重新安排请求，包括借用输出仍关闭的离屏兜底。
暂停回归经过真实 DDM 请求及其 disable 回调，同时覆盖先暂停再请求，以及同批请求
先 capture 再 disable_render、已排队 fallback 尚未执行的情况。请求处理后的服务端排队读取作为
队列同步点，不执行渲染；恢复后只观察原帧结果，不补发 capture、不强制 render，
也不通过移动窗口或启用输出帮助唤醒。
已排队 fallback 的取消在全局渲染启用、借用输出关闭时单独验证；暂停期间取消
使用另一条会话验证，避免暂停阻止排队后仍声称覆盖了排队任务的回收。
渲染失败回归通过持有真实 swapchain buffer 引用，让生产 `wlr_swapchain_acquire()`
没有空闲槽位可用；不替换 allocator/renderer，也不直接发送失败信号或协议 event。
客户端先通过真实请求提交 frame，再定向调用生产 `render(false)` 完成失败尝试，
以 roundtrip 验证每个 frame 的失败事件；释放引用后验证同一会话恢复和最终像素。
生产视口在本轮渲染清理完成、`inRendering` 为 false 后报告失败；捕获源通过现有
wlroots frame 分发路径结束所有挂起帧，返回 `unknown`，不停止会话或重建视口。
失败帧不发送 transform、damage 或 presentation_time 元数据，也不回传旧缓存图像。
下一次客户端 `capture` 请求重新调度渲染。
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

### 已有版本验证证据

- `ci` preset（`-Wall -Wextra -Werror`）构建通过，无告警。
- headless + pixman：包含生命周期与“请求之间额外渲染”用例的最终版本连续 20 次通过。
- headless + GLES2：Intel UHD Graphics 630（i915）、OpenGL ES 3.2 / Mesa 26.2.6，
  同一版本连续 20 次通过，非 skip、非软件 fallback。
- 捕获、SSD、递归子面、foreign-toplevel、window-transition、alpha-modifier、decoration、
  remote-subsurface、input-manager 等相关 10 项回归全部通过。
- 尚无可访问的运行中 Treeland 桌面；物理 DRM/KMS scanout 和完整 libinput/uinput
  输入链路未验收，以上 headless 结果不能替代用户桌面的捕获瞬间验证。

### 关屏兜底修复验证

- `ci` preset（`-Wall -Wextra -Werror`）构建通过。
- 未修复实现运行新增用例时在 30 秒超时；协议日志确认先收到 `capture()`，再收到
  `set_mode(OFF)` 的成功事件，之后没有 frame 结果。
- Qt 6.12.0，headless + pixman / GLES2：包含关屏、新像素、复电、排队取消及关屏后
  新会话输出解析的最终场景，各连续 20 次通过。GLES2 实际打开 `/dev/dri/renderD128`，
  通过 renderer 和像素断言，非 skip、非 pixman fallback。
- 相关 11 项回归中 10 项通过。`test_wlr_output_management_unstable_v1` 在 stage 8
  的旋转断言失败（请求 90°、读回 0°）；该用例不创建捕获源，在旋转动画完成前读取
  状态。本次未修改输出管理实现或该用例。
- 物理 DRM/KMS scanout 与完整 libinput/uinput 输入链路仍未验收。

### 离屏渲染失败回归

- 未修复实现运行新增用例时，真实 swapchain 报告 `No free output buffer slot`，
  渲染尝试已结束，但客户端为 `ready=0`、`failed=0`、`stopped=0`，用例明确失败。
- 回归同时断言失败通知发生在渲染清理后、多会话各收到一次 `failed(unknown)`、
  不发送成功帧元数据、会话和视口保持有效，以及释放 buffer 后重新捕获成功。
- 原输出关闭时重复上述失败与恢复，并验证输出未被提交、视口未重建。
- 本次 `-Wall -Wextra -Werror` 构建通过；headless + pixman / GLES2 的完整场景各
  连续 5 次通过。GLES2 实际使用 `/dev/dri/renderD128`，无 skip 或软件 fallback。
- 捕获、desktop capture、递归子面、decoration、foreign-toplevel、input-manager、
  window-transition、alpha-modifier、输出管理等另外 12 项相关协议回归全部通过。

### 全局暂停恢复与组件复用

- 未修复实现在 DDM `enable_render` 后仍留下关屏输出的挂起帧，客户端为
  `ready=0`、`failed=0`；队列同步点不执行绘制，用例明确失败。
- 暂停期间不结束有效捕获请求；恢复后验证单次请求完成，不靠新请求、场景变化、
  强制绘制或复电辅助唤醒。暂停期间取消和窗口销毁均检查恢复后的渲染次数。
- 每次捕获管线重建均检查 delegate 与引擎缓存组件的指针一致，并继续验证全部
  SSD、子面和整图像素以及最后一个 session 的视口回收。
- `-Wall -Wextra -Werror` 构建通过；包含已排队 fallback 遇到暂停的最终场景在
  headless + pixman / GLES2 各连续 5 次通过。GLES2 实际使用 `/dev/dri/renderD128`，
  无 skip 或软件 fallback；另外 13 项相关协议回归全部通过。
- 尚未验证物理 DRM/VT 切换；组件复用没有单独测量编译或启动耗时。

## 已知边界 / 下一项结果

输出工厂 target 仍不创建 output capture session：headless 下进入
`wlr_output_configure_primary_swapchain()` 的历史死锁（issue #1407）未由本窗口测试验证。
窗口捕获使用独立的离屏 swapchain，不受此输出源测试边界限制。

GPU target 没有可访问 render node 时以 `77` skip，不允许以软件 fallback 代替 GPU 成功。
`QDir::System` 预检只检查节点能否打开；成功后仍必须通过 GLES2 renderer 和像素断言，
不能仅凭节点存在宣告 GPU 通过。GPU target 仍使用 SHM 客户端目的 buffer，
尚未覆盖客户端 DMA-BUF、fractional scale、多屏混合缩放/输出销毁、minimized/完全离屏窗口、
popup、cursor capture 和跨 Session 拒绝路径。
渲染失败回归覆盖真实 swapchain 槽位耗尽；显存不足、超出 GPU 尺寸限制及渲染目标
导入失败尚未单独注入验证。
