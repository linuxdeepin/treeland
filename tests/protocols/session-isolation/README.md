# Session 窗口隔离

新增用例，尚未编译或执行。断言目标为 E 级生产集成结果，不代表已经验证通过。

Fixture 复用实际 SessionManager，为全局 Session 和第二个本地账户分别建立 Session；
C 客户端通过两个 Session socket 建立真实连接，各自创建一个 mapped xdg-toplevel。
第二账户不存在时返回 77。测试连接与合成器位于同一进程，恢复 A 前先销毁 B 的服务端
连接，避免正常的后台进程暂停逻辑向测试进程发送 SIGSTOP。

Fixture 补齐三个 headless 输出，每个 Session 持有独立的 output 协议与窗口区域对象，显示参数使用共享系统配置；客户端在同一连接上
跟踪 head/finished 事件，账户往返切换后仍必须只有三个 head。

两个客户端还分别映射底部 LayerShell 面板（40/60 像素 exclusive zone）。

缩放用例由 fixture 调用生产 UI 入口 `SessionOutputManager::setScale`，验证共享系统配置、逻辑几何和物理状态；另有真实 C 客户端对三个 head 的枚举、属性事件与账户切换的回归断言；所有新增断言均未运行。

新增 shell/个性化/appearance 覆盖：比较两个 Session 的协议实例，客户端只看到各自的一份 globals；
两个真实 appearance manager 分别设置字体大小 17/22，另一个用户的 appearance 客户端
不应收到变更，也不应改变数值。这些断言尚未编译或执行。

断言：

- 在创建窗口前绑定三个 foreign-toplevel 协议，收到且仅收到自己的一个窗口和标题。
- 在两个窗口存在后重新绑定，初始枚举仍只返回所属 Session 的窗口与标题。
- Session 的窗口模型分别包含一个窗口，并持有不同的 Workspace、输入、输出、壁纸、切换器和窗口协议对象；设备 manager 的激活状态跟随所属 Session。
- 各 Session 的 manager 初始只有三个 head，切换账户不会新增 head 或收到 head.finished；同一 A 连接在三次 A/B 往返切换后仍只有原来的三个 head。
- A 修改系统缩放后 B 的现有 head 收到 scale 变更，后台协议快照同步系统状态，head 数量不增加。
- A 的 UI 设置系统缩放为 1.5，A/B 读取同一个 OutputConfig，屏幕逻辑宽度相同；切换到 B 或恢复 A 后物理缩放一直是 1.5。窗口列表与任务栏专属区域仍独立。
- 同一物理输出对应不同的 SessionOutput，两个面板各自贴底；专属区域分别为 40/60，切换时物理输出只发布当前用户的区域，恢复 A 后回到 40。
- A 切换到第二工作区后激活 B，B 的当前工作区保持独立；恢复 A 后仍保留原选择。
- B 使用 A 的私有 foreign-toplevel identifier 请求预览，真实预览回调不包含 A 窗口。identifier 在各 Session 内独立分配，重复数值可以合法指向 B 自己的窗口。

尚未覆盖：跨账户进程凭据验证、真实输入注入、屏幕配置、亮度和显示提交失败、
持续捕获授权、XWayland 实际窗口以及日志退出压力测试。

```sh
cmake --preset ci
cmake --build --preset ci --target test_session_isolation
ctest --test-dir build --output-on-failure -R '^test_session_isolation$'
```

新增源码断言：每个连接仅看到一组 selection/data-control、IME 和 xdg-foreign globals；A/B 的逻辑 seat、native seat 和 IME helper 独立，切换后的活动 seat 与 Session 一致。这些新增检查尚未编译或执行。

桌面对象树回归：检查 A/B DesktopEnvironment、UI 根节点和 surface 集合独立；输入/输出管理器归所属桌面所有；DockPreview/已创建 TaskSwitcher 的 desktop 属性始终绑定所属桌面。新增断言未编译、未运行。

输入对象隔离断言：A/B SessionInputChannel、CompositorInputFilter、GestureManager、ShortcutController 均独立，逻辑 seat 的 eventFilter 固定绑定所属通道。新增检查尚未编译或执行；真实设备的跨切换按键/触摸/手势尾部验证仍待真机进行。

本轮新增快捷键隔离断言（未编译、未执行）：

- A/B 各可见且仅可见自己的一个 `treeland_shortcut_manager_v3` global，协议对象固定连接各自控制器。
- 两个真实 C 客户端使用相同名称分别绑定 F9/F10，后台 B 的绑定立即进入自己的控制器，双方均不匹配对方按键。
- 后台 B 对同一按键改名后解绑，不应恢复旧名称绑定，也不影响 A。
- 激活 B 时两侧绑定保持不变；销毁 B 连接并恢复 A 后，A 的绑定仍然存在。

未覆盖：双用户同时捕获快捷键的事件流、捕获期间销毁或切换、真实设备输入序列代次隔离。

本轮新增窗口消失回归（未编译、未执行）：

- A/B 同 appId、同 instanceId 各自创建真实闪屏；关闭 B 的协议 resource 不移除 A 的闪屏。
- 两条连接各自仅看见一个预启动和一个 appId 解析 manager。
- 将 B 根 socket 停用后，经其子 socket 创建真实 xdg 窗口；B 激活后该窗口应恢复可见，切回 A 后 A 的同 appId 窗口仍存在且可见。
- 停用根 socket 前先销毁直接测试连接，避免生产 freezer 向同进程测试客户端发送 SIGSTOP。

该用例使用实际 Wayland 子连接模拟 XWayland 的 socket 层级；未覆盖两个真实 XWayland 进程、
解析器替换期间的完整应答、异步配置延迟期间注销、用户 UI 主动关闭闪屏后 real-window 的关闭事件流。

Session 展示域新增断言（未编译、未执行）：停用用户的完整 surface 注册表，包括顶层
窗口和 LayerShell，均应 renderingEnabled=false、visible=false、enabled=false，且有
WSurfaceItem 的对象带 NonLive。后台创建、激活和切回也检查此约束。断言仅证明待执行
用例的预期，不能替代真实渲染帧计数及输入事件投递测试。

捕获协议新增 registry 断言（未编译、未执行）：四个上游捕获 manager 在 A/B 各只出现一次，
且 global name 不同，证明来自独立实例。窗口源解析固定使用所属 Session 的 toplevelList。
此断言不覆盖实际像素读取、跨账户切换时已存在的输出捕获流停止，以及非法窗口源请求。

Socket 所有权新增断言（未编译、未执行）：逻辑 seat 固定绑定所属 Session 根 socket，
拒绝未知客户端和另一 Session 的真实连接；后台子 socket 的客户端只属于其父 Session。
Session 注销遵循先关闭客户端、再解除协议、最后销毁 socket 的顺序；注销路径尚未覆盖运行测试。

xdg-foreign 接口新增状态断言（未编译、未执行）：每个 Session 的 exporter/importer
分别对应两个 WServerInterface 对象，绑定同一所属 socket，共享 native registry。
配合现有 C registry 断言检查各连接仅看见自己的 exporter/importer global。
尚未覆盖创建期间的广播、restart 以及 export/import token 的完整请求链路。
