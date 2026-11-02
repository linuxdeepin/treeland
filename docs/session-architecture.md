# DesktopEnvironment 多用户对象框架

隔离单位是 Session，一个 Session 对应一个用户；合成器共用同一个 wl_display。
Session 负责身份、客户端连接、协议归属和生命周期。DesktopEnvironment 是用户业务对象的
组装入口，其公开 API 不提供 Session、用户名或 activeSession 查询。

```text
共享运行时：WServer / backend / renderer / 物理 output / 物理 input
    │
    └── SessionCoordinator（目前由 SessionManager 承担）
        ├── Session A ── DesktopEnvironment A
        └── Session B ── DesktopEnvironment B

DesktopEnvironment
├── config / 配置 providers（构造时固定）
├── surfaces（完整 surface 集合，包括 LayerSurface）
├── windows（顶层窗口集合） / Workspace
├── InputManager
├── WallpaperManager
├── SessionOutputManager / 逻辑输出
├── SessionSwitcher / ShortcutRunner
└── viewRoot
    ├── DockPreview（独立实例、固定绑定）
    └── TaskSwitcher（按需创建、固定绑定）
```

## 依赖与生命周期

InputManager、WallpaperManager、SessionSwitcher、ShortcutRunner 和 SessionOutputManager
均为普通 QObject，不再继承 SessionComponent，也不接收 Session。配置、工作区、逻辑 seat、
设备目录、壁纸资源服务由组装入口注入；输出参数注入共享系统配置。业务对象不查询当前用户，也不遍历其他 Session。

SessionManager 在登录身份确定后创建完整对象树。Session::setActive 只通知桌面启用/停用；
DesktopEnvironment 切换 UI 根节点和快捷键订阅；SessionInputChannel 负责输入硬件订阅。ShortcutRunner 直接使用
自己的工作区和切换器，不通过全局 taskSwitcher 取得另一套桌面的 UI。

配置初始化失败时使用生成默认值；尚未初始化时等待完成通知。配置回调重新处理本对象的
逻辑 seat，即使回调跨过一次用户切换，也不会按新的活动用户解释旧请求。

## 硬件边界

SessionOutputChannel 是身份相关的适配层，负责 primary、缩放、颜色提交
和输出协议配置请求的授权。显示模式使用 TreelandConfig，每屏参数使用共享 OutputConfig。
SessionOutputManager 发布实际硬件状态的独立协议视图，不保存个人显示参数，不在切换时恢复模式。
需要修改物理设置时调用通道；只有通道判断硬件是否接通。逻辑输出的 surface 校验使用所属桌面的完整 surface
集合，不能用顶层窗口列表，否则会错误拒绝任务栏等 LayerSurface。

物理 Output 是共享设备投影，只通过 WaylandServer 的桌面目录访问逻辑输出，不解析 Session。
物理输出调试 UI 的缩放命令通过运行时命令入口转发，不在 QML 查询当前用户。

## QML 边界

TaskSwitcher 和 DockPreview 使用 `required property DesktopEnvironment desktop`，创建时
注入并固定；切换用户不改写其依赖，也不重新连接 DockPreview 的协议对象。
窗口通过 `surface.desktop` 提供固定业务模型，不向 QML 暴露 `surface.session`。
标题栏使用所属桌面的配置；WorkspaceSwitcher 注入自己的 Workspace；WorkspaceProxy 从
WorkspaceModel.workspaceManager 获取所属工作区，包括“所有工作区”窗口列表。

Session 的业务 Q_PROPERTY、SurfaceWrapper.session 和运行时的 activeSession QML 属性已移除；C++ 边界接口保留。业务 QML 不应添加 session/activeSession 查询，也不应
新增对 WaylandServer.config/workspace 等活动桌面兼容入口的依赖。

## 迁移范围与限制

本轮完成上述业务对象、独立 DockPreview/TaskSwitcher、窗口与工作区预览入口的迁移。
部分共享运行时、协议安全边界和旧插件（例如多任务视图、锁屏）仍保留兼容访问；它们尚未
全部迁为独立桌面对象。本轮不代表所有 Wayland 协议的资源隔离已经完成，剩余边界见
[协议隔离审计](session-protocol-isolation.md)。

未进行编译、QML 编译或运行时测试。协议隔离 fixture 已增加真实对象树、父对象归属和
QML desktop 属性固定绑定的源码断言，等待在允许构建的环境执行。

## SessionInputChannel

输入启停与边界状态集中在 src/session/sessioninputchannel.*，业务对象不查询 Session。
每个通道独立持有 CompositorInputFilter、GestureManager/GestureRecognizer 和
ShortcutController，并将过滤器固定绑定到本用户的逻辑 WSeat。共享运行时仅保留启动过滤器
和按实际 seat 发送的 relative-pointer transport；用户桌面启用后启动过滤器停用。

停用顺序为关闭事件入口、取消 swipe、停止 hold 计时器、断开输入设置硬件订阅、取消本通道
的快捷键捕获，然后关闭桌面 UI 与快捷键动作订阅。后台通道销毁不会取消前台用户的捕获。
启用时清理旧过滤键状态，对切换前按住的键和按钮进行隔离；旧序列的 release/update/end
不能触发新用户动作。逻辑 seat 清除键盘窗口焦点时停止自动重复。

过滤器在 beforeDispose、beforeHandle 和 afterHandle/unaccepted 阶段检查入口是否启用、
设备是否确属事件 seat；目标 surface 通过逻辑 seat 的固定协议域验证。活动通知使用事件
seat，不能使用全局默认 seat。ShortcutRunner 只订阅自己的过滤器 modifier-release 信号。

每个 SessionInputChannel 还持有独立的 ShortcutManagerV3 协议实例，使用 Session::attachInterface
固定客户端作用域。绑定、解绑、捕获和通知始终作用于同一个控制器；后台用户的请求立即
更新自己的对象。用户切换不再更换协议内部的 controller，也不清空、缓存或回放绑定。
ShortcutRunner 的通知和修饰键查询直接使用注入的协议及控制器。

CompositorInputFilter 只依赖 InputContext，不再持有 WaylandServer。SessionInputContext
作为边界适配器验证 seat 归属，并从本用户的 surface 模型解析窗口；焦点和边缘平铺请求
经过该边界。锁屏模式、VT 切换和调试视图属于共享运行时能力，由适配器提供。
输入过滤器、快捷键协议与 QML 无需查找当前 Session。

每个控制器只在注入的 GestureManager 注册手势。不同桌面不再共享识别器、hold
计时器、累计 swipe delta 或按键动作表。TogglableGesture 也支持显式注入手势管理器。

新增真实对象状态断言检查 A/B 通道、过滤器、手势管理器、控制器、协议及逻辑 seat 绑定独立。
双 Session C 客户端还检查同名绑定各自生效、后台 upsert 后解绑不保留旧绑定、切换保留绑定。
本次没有引入独立的通用 SessionInputRouter 或完整输入序列代次机制；物理设备交接仍由
SeatManager 与 SessionInputChannel 配合完成。屏幕选择捕获、空闲与输出唤醒、relative-pointer
传输及 shortcuts-inhibit 仍共享基础设施，本次不代表这些服务已经全部完成 Session 化。
未编译或运行测试；需要真机验证 A 持续按键、鼠标拖动、三指滑动/长按期间切换 B，确认
尾部事件与旧用户计时回调不触发 B 的动作。

## 系统显示配置

主屏、单屏、复制模式及复制输出组只使用 TreelandConfig。每块屏幕的分辨率、刷新率、
缩放、旋转、位置和颜色参数只使用 `DConfigManager::outputConfig(outputId)` 的系统路径。
旧 `/<username>/outputs/<outputId>` 不再读写，也不会在切换时导入或恢复；已有系统路径
和键名保持不变。用户文件中的五个显示模式键已撤回。

每个 Session 仍有独立 output-management 协议对象和 SessionOutput，协议快照同步同一份
实际硬件状态；窗口列表、LayerSurface 和专属区域独立计算。活动通道授权硬件修改，
用户切换仅切换授权与桌面，不能修改系统显示模式。系统缩放更新同时同步所有已创建
XWayland 的 XSettings，光标等个人偏好仍按所属用户同步。

已调整双 Session 回归用例：A/B 同读系统缩放 1.5，切换不回滚，B 的现有 head 收到变更，
A/B 面板仍分别占用 40/60。以上改动未编译、未运行测试。

## 窗口创建与启动匹配

SessionPrelaunch 为每用户持有独立的预启动协议、appId 解析协议、等待请求、闪屏及关闭
记录。ShellHandler 仅根据真实 surface 客户端所属 Session 路由；不能按全局 appId 查找、
关闭或转换其他用户窗口。配置初始化回调固定使用请求所属 Session，附带请求代次；
旧请求回调、重复请求和旧闪屏超时不得删除新请求或已转换的正常窗口。协议 close 使用
appId 与 instanceId 匹配本用户请求。应用历史配置的持久化仍使用现有 WindowConfigStore
路径，本轮未改变其 appId 级存储归属。

窗口可见性缓存固定监听所属 Session 的根 WSocket，而不是 XWayland 的子 socket。
后台完成创建的窗口在根 socket 恢复时必须更新，纯闪屏也使用同一条通知路径。
SurfaceWrapper 一旦绑定 Session，Release 路径同样禁止重新绑定到另一个用户。

新增回归检查同 appId 闪屏的独立创建和关闭，以及真实子 socket 的后台窗口在激活后恢复
可见、切回原用户时原窗口仍存在。本轮只做源码静态检查，未编译或运行；子 socket 用例
覆盖与 XWayland 相同的 socket 层级，不等于两个真实 XWayland 应用的真机复现证据。

## Session 展示域

SessionScene 是所有所属 surface 的统一展示边界，不只管理顶层窗口列表。普通用户停用后，
其窗口、弹窗、LayerShell、输入法窗口、闪屏及子 surface 的内容均不可展示或接收输入。
窗口代理跟随源窗口权限，不能用预览 live 开关重新启用后台内容。QML 只读取中性的
`renderingEnabled` 属性，不解析 Session。独立登录服务 dde 的锁屏/系统覆盖层维持专门
系统通道；普通用户的 privileged 标记不能绕过停用。

停用时隐藏并禁用 item，给 surface 设置 NonLive，清退独立动画与离屏内容，释放过渡源。
延迟动画回调及后台新建窗口同样受控。退出动画即使已从用户窗口模型移除，其 wrapper
仍由 SessionScene 跟踪到销毁，避免切换后继续显示上一用户画面。

切回时只恢复 Session 展示权限，再依据 mapped、最小化、工作区、显示桌面和锁屏状态
计算最终可见性；不修改这些状态来强行显示所有窗口。底层输入入口按子 surface 的根窗口
再次确认展示许可，并保留逻辑 seat 和客户端域校验。系统显示模式不随该展示开关恢复。

新增断言检查停用用户全部已注册 surface 的 renderingEnabled、visible、enabled 和 NonLive，
覆盖顶层窗口及 LayerShell；后台子 socket 用例也检查同一状态。尚未编译或运行；未覆盖
真实 GPU 帧计数、所有弹窗/输入法类型、切换期间每种动画与输入事件的真机验证。
