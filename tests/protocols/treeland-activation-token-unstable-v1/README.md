# `treeland-activation-token-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_activation_token_manager_v1` / `treeland_activation_token_v1`
- 测试源码：`tests/protocols/treeland-activation-token-unstable-v1/`
- Fixture：desktop integration fixture（`protocol_test_setup` 创建 headless output），第二个真实 Session（另一用户）用于跨 Session 断言；用例映射 xdg-toplevel
- 覆盖等级：**E / P**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 未实现 prelaunch-splash 的客户端 | 只 bind `treeland_activation_token_manager_v1`（未 bind 本 Session 的 `treeland_prelaunch_splash_manager_v2`） | **bind 即被拒**：`wl_display.error` code = `not_privileged(1)`，interface 为该 manager，连接断开 |
| 应用管理器 | 同一条连接**先** bind `treeland_prelaunch_splash_manager_v2`（version 1），再 bind activation-token，随后连续两次 `request_token` | bind 成功；收到两个 `token` 事件，字符串均非空且**互不相同**（一次请求铸一个独立 token） |
| **生产回读** | 用第一个 token 对映射的 xdg-toplevel 调用 `xdg_activation_v1.activate()` | 回读真实 `ActivationManagerInterfaceV1::activateRequested`：信号被触发且 `disposition == Active(2)`，不是 `Attention(1)` |
| manager 单例 | 同一 Session 内第一个客户端已 bind 时，第二个连接 bind `treeland_activation_token_manager_v1` | 第二个连接收到 `binding_rejected(0)` 并断开 |
| 单例释放 | 第一个客户端 `destroy` manager（并释放 splash manager）后，第三个连接先 bind splash、再 bind token manager | bind 成功 |
| **断连释放** | 第一个客户端 bind 两个 manager 后**不做任何 destroy** 直接断开连接，之后的新连接 bind splash + token manager | bind 成功（AM 崩溃重启路径） |
| splash manager 单例 | 第一个客户端已 bind `treeland_prelaunch_splash_manager_v2` 时，同 Session 第二个连接 bind 同一 global | 第二个连接收到 `binding_rejected(0)` 并断开 |
| splash 单例释放 | 第一个客户端 `destroy` splash manager 后，第三个连接 bind | bind 成功 |
| **跨 Session 隔离** | A Session 的 AM 持有两个 manager 期间：B Session 的 AM bind 本 Session 的 splash + token manager；B Session 内未持有本 Session splash 的客户端 bind token manager；B Session 客户端用 A 铸出的 token 调 `activate()` | ① B 的 AM bind 成功（server 级单例会让它 `binding_rejected`）；② 该客户端收到 `not_privileged(1)`（判据按 Session，不按「谁在本机实现了 splash」）；③ `disposition == Invalid(0)`（A 的 token 不在 B 的 token 表里） |

## 契约要点

判据是**本 Session 的 splash manager 持有者**：activation-token manager 与
`ActivationManagerInterfaceV1` 一样按 Session 装配（`Session::attachInterface()`，装配点
`FocusController::registerSession()`），因此每个用户各有一个实例：

- global 通过 `WServerInterface::ownsSocket()` 只对本 Session 的 socket 可见；
- bind 期判定：先 `Session::ownsClient()` 确认请求者属于本 Session，再要求
  `Session::prelaunch()->protocol()->boundClient()` 正是该 `wl_client`。不再遍历
  `wl_client` 的 resource 表按接口名比对字符串。
- 判定在 **bind 期**执行，不在 `request_token` 里。生成的 `bind` 是 private 非虚，
  所以 global 由 `wl_global_create()` 手工创建，在自定义 bind 回调里判定，通过后才
  `add(client, id, version)`。因为 global 本身已按 Session 过滤，手工创建 global 只为
  「拒绝时要回协议错误」。
- 因此**绑定顺序是强制的**：先 bind `treeland_prelaunch_splash_manager_v2`，再 bind
  activation-token。反过来会被 `not_privileged` 直接断连。
- 两个 manager 都是**每 Session 单例**：同 Session 第二个客户端 `binding_rejected`，
  `destroy` 或断连释放（单例状态按 `wl_client` 记，在 `destroy_resource` 释放）。
  不同 Session 的实例互不影响，A 用户的 AM 不会把 B 用户的 AM 挡在 `binding_rejected` 外。
- `request_token` 铸出的 token 只进**本 Session 的** `ActivationManagerInterfaceV1`
  的 token 表，不跨用户可见。
- `request_token` 已不带 `app_id`：测试客户端不传参数即编译期证据。合成器铸造时
  `app_id` 只用于日志。
- 每个 `request_token` 铸一个独立 token，多个未消费 token 可并存。

## 变异提示（这些用例必须变红）

- 去掉 bind 期的 `not_privileged` 判定（例如判据恒真）→ 「未实现 prelaunch-splash」用例失败。
- 判据只保留 `Session::ownsClient()`、丢掉「必须是本 Session 的 splash 持有者」→ 「跨 Session 隔离」② 失败。
- 去掉 token manager 单例判定 → 「第二个客户端 `binding_rejected`」用例失败。
- 把单例状态改成跨实例共享（如 `static`）→ 「跨 Session 隔离」① 失败。
- 把 token 表改成跨实例共享（如 `static`）→ 「跨 Session 隔离」③ 失败。
- 去掉 splash manager 单例判定 → 「splash manager 第二 bind `binding_rejected`」用例失败。
- 去掉 `destroy_resource()` 里清空绑定者的那一行 → 「断连释放」用例失败。

服务端实现位于 `src/modules/activation-token/activationtokenmanagerinterfacev1.*` 与
`src/modules/prelaunch-splash/prelaunchsplash.*`；装配点在
`src/core/focuscontroller.cpp` 的 `registerSession()`。

## 已知边界

- **残留风险（判据的信任前提）**：判据等价于「谁实现了本 Session 的 prelaunch-splash，谁就是
  该用户的应用管理器」。合成器不额外校验该客户端的身份，因此某用户本机任意进程只要先 bind
  该用户的 `treeland_prelaunch_splash_manager_v2` 就能拿到必然 `Active` 的 token。单例让这个
  「先到者」在一个 Session 内唯一，但它仍然不是身份校验。
- token 过期（60 秒）后 `activate` 的行为：未覆盖。
- dde-application-manager 侧必须按「先 splash、后 token manager」的顺序绑定。
- 跨 Session 用例依赖 fixture 能创建第二个真实 Session（`nobody` 等账户）；账户不可用时该
  用例打印跳过原因并返回成功，其余用例照常执行。
