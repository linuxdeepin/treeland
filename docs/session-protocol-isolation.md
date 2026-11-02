# Session 协议隔离边界审计

本文记录源码审计结果和需要确认的协议域设计；不表示已实现或验证全量隔离。
已确认的约束是单进程、共享同一个 WServer/wl_display；采用协议级隔离。
SessionProtocols 是每个 Session 的协议集合，Session::attachInterface 通过 setOwnsSocket(socket) 设置不可变的 socket 所有权边界。
未编译、未运行测试。

## 当前源码中的共享边界

| 协议/对象 | 当前入口 | 问题或必须检查的边界 |
| --- | --- | --- |
| xdg-shell、layer-shell | ShellHandler::init | 已迁入 SessionProtocols 独立实例；资源行为和子窗口关系的新增回归未运行 |
| xdg decoration/dialog/tag | SurfaceProtocolController::installDecoration | 已迁入 SessionProtocols；使用所属 xdg-shell 协议域 |
| prelaunch-splash / app-id-resolver | SessionPrelaunch | 协议、解析客户端、pending/closed appId 状态均固定到 Session；配置回调不使用活动用户；新增回归未运行 |
| Wine | SurfaceProtocolController::initializeSession | manager 已独立；仍需核对每条请求、窗口标识和事件目标 |
| XWayland | SessionManager::ensureSession | 进程和逻辑 seat 已独立，桌面/workarea 从所属 SessionOutput 计算；仍共享 output/xdg-output 公告 |
| wl_output / xdg-output | WOutput / WXdgOutputManager | 仍从物理输出/布局发布，后台客户端可能获得其他用户的状态变化 |
| wlr-output-management | SessionOutputManager | 独立协议对象发布共享系统硬件快照；切用户不恢复显示参数，新增三屏往返测试未运行 |
| wl_seat / input / text-input / input-method | SeatManager / WSeat / WInputMethodHelper | SessionSeatManager 独立持有逻辑 WSeat、serial、IME；活动 SeatManager 仅投影物理设备，新增回归未运行 |
| data-device / primary-selection / data-control / drag | WServerPrivate::init、installCommonProtocols、WSeat | 四组 manager 已迁入 SessionProtocols，selection 和拖放由所属逻辑 seat 保存；新增回归未运行 |
| xdg-foreign | installCommonProtocols | 每个 Session 使用独立 registry；exporter/importer 各有一个接口对象和 global，绑定相同所属 socket |
| personalization / appearance / show-desktop | WaylandServer / SurfaceProtocolController | personalization/appearance 已改为独立实例和固定配置，旧全局 appearance context 广播已移除；show-desktop 等仍需迁移 |
| capture / screencopy / session-lock / activation | CaptureManager / LockScreenManager / FocusController | activation 与窗口过渡 token 表已迁入 Session；捕获授权、输出枚举/结果及锁屏权限仍需迁移 |
| core compositor / subcompositor / shm / dmabuf 等 | WServer 及 renderer | 即便协议本身主要服务单客户端，也必须固定于资源所属协议域，不能默认绕过用户边界 |

## 已选择：共享 wl_display 的协议级隔离

所有有状态协议必须迁移到固定 Session，不能用活动用户替代请求者。
SessionProtocols 目前包含 shell、xdg 装饰/对话框/标签、personalization、appearance 等实例；
Wine、foreign-toplevel、output-management 等通过同一 Session::attachInterface 入口绑定所有权。

接口的 setOwnsSocket 只能设定一次，不会被协议自身 setFilter 替换；registry/bind 同时满足
socket 所有权、协议自身过滤和服务器过滤。归属检查沿客户端 socket 的 parentSocket 链查找，
包含 XWayland/沙箱子 socket。Session 先关闭客户端，再解除协议，最后销毁所属 socket；
协议对象的生命周期不得超出所属 socket。Scoped global 在 detach 前撤下，防止原生对象
随 display 延迟销毁期间变成无归属 global。未知 Session 的连接无法绑定 scoped global。

### 已选择：一个 wl_display、每协议实例和逻辑资源按 Session 路由

全部有状态协议分别实例化；wl_output、wl_seat 等基础 global 也必须提供用户独立的逻辑对象。
显式登记可共享的无用户状态能力；未登记协议默认不向普通 Session 发布。
所有绑定策略同时满足服务器策略和接口策略，不能使用早返回绕过全局边界。

公共资源上下文记录不可变 Session，子资源继承它；对象参数、窗口 ID、token、输出和 seat
都在所属上下文查询；事件只能遍历所属实例的资源集合。异步任务携带 Session 和激活代次。
同一客户端的新 registry 和重新 bind 不能改变原连接的 Session。

这种设计仍要求逐协议迁移原生 wlroots/waylib 全局，不能靠统一 registry filter 自动约束
已经绑定的资源，也不能通过冻结客户端进程代替事件隔离。

## 公共代码已发现并修正的缺口

WServer 的 globalFilter 原先在接口 filter 返回 true 时直接返回，跳过服务器级策略。
现改为两层过滤都必须通过，xwayland_shell 的特定客户端检查也不能绕过服务器策略。
新增 socket 所有权是独立于协议 filter 的固定约束；这仍不代表未迁移协议的资源/事件已完成隔离。

## 迁移验收

1. 为每个协议登记所有权、资源创建入口、请求域、事件域、异步任务和销毁路径。
2. 从 shell/output/seat/selection 迁移基础资源，再迁移依赖它们的协议族。
3. 每个协议族使用真实的两个用户连接，断言私人窗口/输入操作不改变另一 Session 的模型或私有事件；系统显示变更则通知各 Session，并保持协议对象和 head 身份稳定。
4. 覆盖同一连接多次切换、重新绑定、新增用户、退出重登、热插拔和延迟回调。
5. 验证用户退出只销毁自己的协议域，合成器退出不回调已销毁硬件服务。

## 逻辑 seat 的硬件投影

SessionSeatManager 在创建时取得固定 Session；每个 Session 都拥有独立的 WSeat/native seat，
seat 名称可相同但对象绝不复用。InputMethodHelper 通过 Session::attachInterface 创建五种
协议工厂；同一 Session 的多个 helper 共享这些工厂，以实际 seat 对象路由，而非比较名称。
候选框管理器、XWayland seat 和 IME helper 不随其他用户切换改写归属。

SeatManager 只保留活动域的投影。切换先清除旧焦点、结束 pointer/keyboard/touch grab，
再转移 backend 的物理设备及硬件光标；用户虚拟键盘、selection/primary selection、资源和
serial 留在原 Session。后台 seat 没有合成器窗口焦点，不能将虚拟键盘事件注入活动桌面。
移除 seat 时同步释放 SeatSurfaceManager 的 native listeners，然后才销毁 native seat。

wl_data_device_manager、primary-selection、wlr/ext data-control 均为 Session 内实例。
xdg-foreign 的 importer/exporter 共用本 Session 的 registry，不再共用 display 级 token 表。
WServerInterface 支持协议族的多个 globals，创建时过滤和 detach 时撤下均覆盖所有 globals。

仍未完成：wl_output/xdg-output 的逻辑公告；捕获、输出 power/gamma、部分私有
协议和共享桌面控制器。不能将本次 seat/selection 迁移当作全量隔离已完成。未编译、未运行测试。

激活协议通过 FocusController::registerSession 创建固定域实例；pending transition rect 表由 SessionProtocols 持有。请求处理核对目标/来源的 Session，再查本用户 token 表。
