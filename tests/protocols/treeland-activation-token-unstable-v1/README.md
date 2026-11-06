# `treeland-activation-token-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_activation_token_manager_v1` / `treeland_activation_token_v1`
- 测试源码：`tests/protocols/treeland-activation-token-unstable-v1/`
- Fixture：desktop integration fixture（`protocol_test_setup` 创建 headless output）+ 映射 xdg-toplevel
- 覆盖等级：**E / P**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 未实现 prelaunch-splash 的客户端 | 只 bind `treeland_activation_token_manager_v1`（未 bind `treeland_prelaunch_splash_manager_v2`） | **bind 即被拒**：`wl_display.error` code = `not_privileged(1)`，interface 为该 manager，连接断开 |
| 应用管理器 | 同一条连接**先** bind `treeland_prelaunch_splash_manager_v2`（version 1），再 bind activation-token，随后连续两次 `request_token` | bind 成功；收到两个 `token` 事件，字符串均非空且**互不相同**（一次请求铸一个独立 token） |
| **生产回读** | 用第一个 token 对映射的 xdg-toplevel 调用 `xdg_activation_v1.activate()` | 回读真实 `ActivationManagerInterfaceV1::activateRequested`：信号被触发且 `disposition == Active(2)`，不是 `Attention(1)` |
| manager 单例 | 第一个客户端已 bind 时，第二个连接 bind `treeland_activation_token_manager_v1` | 第二个连接收到 `binding_rejected(0)` 并断开 |
| 单例释放 | 第一个客户端 `destroy` manager（并释放 splash manager）后，第三个连接先 bind splash、再 bind token manager | bind 成功 |
| splash manager 单例 | 第一个客户端已 bind `treeland_prelaunch_splash_manager_v2` 时，第二个连接 bind 同一 global | 第二个连接收到 `binding_rejected(0)` 并断开 |
| splash 单例释放 | 第一个客户端 `destroy` splash manager 后，第三个连接 bind | bind 成功 |

## 契约要点

判据仍然是**本连接是否实现了 `treeland-prelaunch-splash-unstable-v2`**：合成器遍历
`wl_client` 的 resource 表，只要存在 `treeland_prelaunch_splash_manager_v2` 的资源就认为
该客户端是应用管理器。与上一版不同的是判定点：

- 判定在 **bind 期**执行，不再在 `request_token` 里。生成的 `bind` 是 private 非虚，
  所以 global 由 `wl_global_create()` 手工创建，在自定义 bind 回调里判定，通过后才
  `add(client, id, version)`。
- 因此**绑定顺序是强制的**：先 bind `treeland_prelaunch_splash_manager_v2`，再 bind
  activation-token。反过来会被 `not_privileged` 直接断连。
- `treeland_activation_token_manager_v1` 每个会话只允许一个客户端持有（第二个
  `binding_rejected`），`destroy` 或断连释放；`treeland_prelaunch_splash_manager_v2`
  同样单例。单例状态按 `wl_client` 记，在 `destroy_resource`（覆盖显式 destroy 与断连）释放。
- `request_token` 已不带 `app_id`：测试客户端不传参数即编译期证据。合成器铸造时
  `app_id` 只用于日志。
- 每个 `request_token` 铸一个独立 token，多个未消费 token 可并存。

## 变异提示（这些用例必须变红）

- 去掉 bind 期的 `not_privileged` 判定（例如判据恒真）→ 「未实现 prelaunch-splash」用例失败。
- 去掉 token manager 单例判定 → 「第二个客户端 `binding_rejected`」用例失败。
- 去掉 splash manager 单例判定 → 「splash manager 第二 bind `binding_rejected`」用例失败。
- 把判据改成遍历整个 display 的客户端而不是本连接 → 各「拒绝」用例失败。

服务端实现位于 `src/modules/activation-token/activationtokenmanagerinterfacev1.*` 与
`src/modules/prelaunch-splash/prelaunchsplash.*`；装配点在
`src/core/focuscontroller.cpp` 的 `installActivationProtocol()`。

## 已知边界

- **残留风险（判据的信任前提）**：判据等价于「谁实现了 prelaunch-splash，谁就是应用管理器」。
  合成器不额外校验该客户端的身份，因此本机任意进程只要先 bind
  `treeland_prelaunch_splash_manager_v2` 就能拿到必然 `Active` 的 token。单例让这个
  「先到者」在一个会话内唯一，但它仍然不是身份校验。
- token 过期（60 秒）后 `activate` 的行为：未覆盖。
- dde-application-manager 侧必须按「先 splash、后 token manager」的顺序绑定。
