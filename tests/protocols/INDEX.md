# 协议测试规范

本目录记录 `tests/protocols/` 下每个协议测试的可观察契约。XML 规定
Wayland 线上请求与事件；本文档规定发出请求后，测试必须观察到的合成器结果。

## 如何阅读

每份规范都说明 fixture、生产结果、覆盖等级和已知边界：

- `P`：协议/资源级；验证请求、事件和协议错误。
- `I`：生产集成级；验证生产模块的状态、回调或生命周期。
- `E`：端到端业务级；验证真实生产对象产生了业务结果。
- `V`：渲染/像素级；在 `E` 基础上读取渲染结果或像素。

仅创建资源或由测试手工发出事件，不构成端到端验证。`E` 至少需要生产图中的
真实对象，例如 mapped `SurfaceWrapper`、已聚焦 `WSeat` 或 wlroots idle 状态；
`V` 还必须观察渲染或像素。

下表描述测试已实现的断言，而不是缓存的 CI 结果。某一提交是否满足规范，仍以
该提交上的本地或 CI 测试执行结果为准；新测试在其文档中会明确标出尚待执行。

## 当前协议索引

| 协议 | 覆盖等级 | 主要生产结果 |
| --- | --- | --- |
| [测试框架架构](framework/README.md) | 基础设施 | runner、fixture、C client、生产状态桥接与 E/V 断言边界 |
| [app-id-resolver-v1](treeland-app-id-resolver-v1/README.md) | I / E | resolver pidfd 应答；返回 app-id 转换真实 splash wrapper 为 xdg window |
| [capture-unstable-v1](treeland-capture-unstable-v1/README.md) | E / V / P | 真实窗口选择、frame copy 与目标 buffer 像素；无 source 错误 |
| [dde-shell-v1](treeland-dde-shell-v1/README.md) | E / P | mapped wrapper 的 DDE 元数据、锁屏；picker 的真实选中/PID 回传测试待执行确认 |
| [ddm-v1](treeland-ddm-v1/README.md) | I | 客户端连接生命周期 |
| [foreign-toplevel-manager-v1](treeland-foreign-toplevel-manager-v1/README.md) | E | 真实 toplevel、dock preview、窗口状态、激活/焦点与 icon rectangle |
| [input-manager-unstable-v1](treeland-input-manager-unstable-v1/README.md) | I / E（可选） | 默认空设备 manager 生命周期；uinput 驱动真实 libinput capability 热插拔 |
| [keyboard-state-notify-unstable-v1](treeland-keyboard-state-notify-unstable-v1/README.md) | P | watcher 配置与空键盘/空 modifier 边界 |
| [output-manager-v1](treeland-output-manager-v1/README.md) | I / P | 真实 `wl_output` 的 primary-output 链路 |
| [personalization-manager-v1](treeland-personalization-manager-v1/README.md) | E / I | 个性化状态挂接到真实 wrapper；font/appearance 配置的 setter/getter 生产回读与恢复 |
| [prelaunch-splash-v2](treeland-prelaunch-splash-v2/README.md) | I / E | splash 请求/关闭信号；生产 splash wrapper 创建、加入 workspace 与销毁 |
| [screensaver-v1](treeland-screensaver-v1/README.md) | E / P | 真实 ext-idle 抑制生命周期 |
| [shortcut-manager-v2](treeland-shortcut-manager-v2/README.md) | E / P | 聚焦窗口捕获与快捷键激活 |
| [virtual-output-manager-v1](treeland-virtual-output-manager-v1/README.md) | P / E | 虚拟输出资源校验；两个既有输出的镜像/恢复 |
| [wallpaper-color-v1](treeland-wallpaper-color-v1/README.md) | I | 订阅、去重与颜色通知 |
| [wallpaper-manager-unstable-v1](treeland-wallpaper-manager-unstable-v1/README.md) | I / P | 真实输出上的壁纸资源生命周期 |
| [wallpaper-shell-unstable-v1](treeland-wallpaper-shell-unstable-v1/README.md) | I / P | wallpaper shell 与 notifier 生命周期 |
| [wallpaper desktop 联合路径](treeland-wallpaper-desktop-v1/README.md) | P / E | manager 配置、shell surface 与真实 output 的关联 |
| [window-management-v1](treeland-window-management-v1/README.md) | E / P | show-desktop 隐藏并恢复真实窗口 |
| [wine-window-management-unstable-v1](treeland-wine-window-management-unstable-v1/README.md) | P / E | 真实 wrapper 的位置与置顶层同步 |
| [wine-window-state-unstable-v1](treeland-wine-window-state-unstable-v1/README.md) | P / E | 真实 wrapper 的最小化、attention 与可见性同步 |

## XML request / event / 业务链路盘点

本节是对当前工作树的源码审计，不是某一次 CTest 的通过率。审计读取已安装的
`/usr/share/treeland-protocols/treeland-*.xml`，并只把测试客户端中实际调用的生成
request stub 算作 request 覆盖；生成的 client-protocol 文件本身不计入。`destroy` 也单列，
因为它证明资源生命周期，却通常不承载主要业务语义。

- **已注册并有测试目录的 19 个当前协议**：XML 共 185 条 request，其中测试客户端直接
  调用了 **144 条（77.8%）**。
- 去掉 48 条 `destroy` 生命周期 request 后，剩余 137 条工厂、配置和业务 request 中有
  **100 条（73.0%）** 被直接调用。
- 19 个协议中 **16 个（84.2%）** 至少有一条 E 级生产业务链路；仅 DDM、output-manager
  color-control、wallpaper-color 仍停留在 I/P 层。
- 当前 XML 共有 95 条 server event。这里**不发布“事件百分比”**：listener 中出现一个
  callback、或 fixture 手工发出一次 event，都不能证明事件负载或其业务来源被断言。下表只
  列出已实际断言的关键 event，并明确列出尚未逐项验证的 event。

| 协议 | request（已调用 / XML） | 已断言的 event 或生产结果 | 主要未覆盖 request / event / 业务结果 |
| --- | --- | --- | --- |
| app-id-resolver-v1 | 4 / 4 | `identify_request` 的 id、真实 pidfd；`respond` 转换同一 splash wrapper 并写入 app-id | resolver 断开 fallback、sandbox 后续策略、XWayland |
| capture-unstable-v1 | 8 / 11 | `source_ready/failed`、`buffer/buffer_done/ready/failed`；64×64 红色像素读回 | session 的 `start/frame_done`，及 `frame/object/ready/cancel` 持久流；OUTPUT/REGION、cursor、mask |
| dde-shell-v1 | 26 / 27 | checker/active/picker 事件；真实 wrapper DDE 元数据、lockscreen、picker PID | `set_xwindow_position_relative`；multitask 只证明 `toggle` 请求/信号，未证明真实 UI 状态；`shutdown/switch_user` 外部会话流程 |
| ddm-v1 | 0 / 7 | 无未请求 VT event；生产连接生命周期 | 所有会话/渲染控制 request 与 `switch_to_vt/acquire_vt` 的实际系统流程 |
| foreign-toplevel-manager-v1 | 16 / 16 | `toplevel/identifier/closed`；真实最小化、最大化、全屏、焦点与 icon rectangle | `pid/title/app_id/output_enter/output_leave/state/done/parent` payload；指定 `wl_output` 的 fullscreen hint、preview 像素 |
| input-manager-unstable-v1 | 1 / 22 | 默认测试仅证明空设备 manager 可绑定；uinput target 断言 Keyboard capability 热插拔 | settings/apply、真实 mouse/touchpad 配置生效、无设备 failed；uinput E 层需显式启用并实际执行 |
| keyboard-state-notify-unstable-v1 | 6 / 6 | watcher 配置、`apply` 的空键盘/空 modifier 边界 | `current_state/state_changed`、多 watcher、初始 locked、seat 销毁、重复 apply 与物理键盘对照 |
| output-manager-v1 | 4 / 7 | `primary_output`；未知 output 的 color-control 错误 | `set_color_temperature/set_brightness/commit` 成功路径及 `result/color_temperature/brightness`，真实 output/像素变化 |
| personalization-manager-v1 | 37 / 37* | cursor/font/appearance 回读 event；真实 wrapper 个性化状态 | manager `destroy` 为 v2 request（v1 global 只能走错误/兼容性边界）；字体渲染和 appearance 的最终 UI 像素 |
| prelaunch-splash-v2 | 3 / 3 | 创建、关闭；真实 splash wrapper 加入/离开 workspace | splash QML 最终可见性、纹理和像素 |
| screensaver-v1 | 2 / 3 | 真实 ext-idle 被 inhibit/uninhibit 改变 | 显式 `destroy` request、实际锁屏 UI |
| shortcut-manager-v2 | 6 / 9 | `commit_success`、`captured`、`activated`；真实 virtual keyboard 输入链 | swipe、hold、`unbind`、`commit_failure` 的业务分支；物理键盘 |
| virtual-output-manager-v1 | 5 / 5 | `outputs/error/virtual_output_list`；生产 copy output 创建与恢复 | 热插拔 successor、跨进程持久化、物理显示器内容一致性 |
| wallpaper-color-v1 | 3 / 3 | `output_color` 订阅与去重 | 真实壁纸分析来源、壁纸应用后的 output 色彩变化 |
| wallpaper-manager-unstable-v1 | 4 / 5 | `failed/changed`；`set_image_source` 与 wallpaper shell/output 关联 | `set_video_source`、实际映射/QML 接入、媒体解码失败、最终 output 像素 |
| wallpaper-shell-unstable-v1 | 5 / 6 | notifier add/remove、play/pause/slow-down；wallpaper shell 资源生命周期 | `ready` 的实际 owner 映射路径、`position/set_playback_rate` event payload、媒体播放和最终 output 像素 |
| window-management-v1 | 2 / 2 | `show_desktop`；真实 wrapper 可见性与 paint order | 多 workspace、minimized policy、preview UI |
| wine-window-management-unstable-v1 | 5 / 5 | `window_id/configure_position/configure_stacking`；真实 QQuickItem 位置/Z 值 | bottom/insert-after、多窗口 sibling、无效 sibling、重复 bind、越界坐标 |
| wine-window-state-unstable-v1 | 6 / 7 | `state_changed`；真实最小化、attention 与可见性 | `activate/activate_denied`、重复 bind、toplevel 销毁后的 inert 状态 |

\* XML 共有 37 条 request；其中 manager `destroy` 的 `since=2` 高于生产 global 的 v1。
测试保留该调用用于兼容性/错误边界，正常可发布的 v1 request 集为其余 36 条。

### 未纳入上述覆盖率的 XML

以下 3 个 XML 仍由协议包提供，但当前没有对应的已注册生产测试 target，故不混入 185 的
分母，也不能被视为“已覆盖”：

| XML | request / event | 当前状态与缺口 |
| --- | --- | --- |
| `treeland-prelaunch-splash-v1` | 2 / 0 | 已由 v2 取代；未验证 v1 compatibility global 或迁移策略 |
| `treeland-shortcut-manager-v1` | 3 / 1 | 已由 v2 取代；未验证 v1 compatibility global、`shortcut` event |
| `treeland-remote-subsurface-unstable-v1` | 8 / 3 | 无测试目录；export token、remote subsurface 创建、位置/堆叠、错误 event 与真实 scene 结果均未覆盖 |

### 如何解读业务覆盖

request 覆盖率回答的是“客户端是否真的把 XML request 发到生产实现”；它不等同于所有业务
分支均已证明。例如 wallpaper 的 image request 已覆盖并有 shell/output 关联路径，视频
request 则没有；foreign 的状态改变已读取真实 wrapper，但 foreign handle 的每一种初始 event
payload 尚未逐项断言。新增或增强测试时，应同时更新该表和对应协议文档的“已知边界”，不要
只提高 request 数字。

## 新增协议

新增 XML 及测试时，请从 [protocol-specification-template.md](protocol-specification-template.md) 复制模板，并在同一变更中：

1. 创建该协议规范；
2. 在本索引添加一行；
3. 使用最低但真实的覆盖等级；
4. 写出尚未验证的下一项生产结果。

这样新增协议时，规范本身就是后续测试增强的待办清单。
