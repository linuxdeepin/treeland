# Session 协议销毁生命周期

新增回归测试代码，尚未完成编译、尚未运行，不代表已经验证通过。

复用真实 SessionManager 和 WServer，通过纯 C Wayland 客户端连接默认 Session
以及一个独立账户的 Session。没有可用的独立账户时返回 77；创建 Session 失败为失败。

实现的断言：

- 真实客户端绑定 wl_data_device_manager 并创建 data source，使注销时存在客户端资源。
- 对 screencopy 连续调用两次 Session::detachInterface，native destroy 仅触发一次，
  且接口已从 WServer 列表移除。
- 移除 Session 并释放最后一个 fixture 引用后，六个带 destroy 信号的原生 manager、
  xdg-foreign manager 及其 registry 的八个 destroy 信号全部触发。
- image-copy-capture 与 output-image-capture-source manager 没有 destroy 信号，
  同步检查其 global 的 user data 已清空，验证显式销毁已安排 global 回收；
  此断言不单独证明底层内存已释放。
- Session 接口均从 WServer 列表移除，所属客户端连接断开，另一个 Session 连接仍可 roundtrip。
- 同一账户重复创建、注销两次，验证销毁后的重新创建路径。

本测试触发实际 Session 析构，不覆盖额外 shared_ptr 仍持有 Session 的情况。
未覆盖子 socket、security-context 子连接、真实 XWayland 应用、mapped 窗口、
正在执行的捕获及剪贴板数据传输，也未覆盖完整 compositor stop/restart。

测试目标：`test_session_protocol_lifecycle`。执行结果需另行人工或 CI 确认。
