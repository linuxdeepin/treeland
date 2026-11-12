# `treeland-activation-token-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_activation_token_manager_v1` / `treeland_activation_token_v1`
- 测试源码：`tests/protocols/treeland-activation-token-unstable-v1/`
- Fixture：desktop integration fixture（`protocol_test_setup` 创建 headless output），第二个真实 Session（另一用户）用于跨 Session 断言；用例映射 xdg-toplevel
- 覆盖等级：**E / P**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 铸 token | bind 后连续两次 `request_token` | 收到两个 `token` 事件，字符串均非空且**互不相同**（一次请求铸一个独立 token） |
| **生产回读** | 用第一个 token 对映射的 xdg-toplevel 调用 `xdg_activation_v1.activate()` | 回读真实 `ActivationManagerInterfaceV1::activateRequested`：信号被触发且 `disposition == Active(2)`，不是 `Attention(1)` |
| **跨 Session 隔离** | A Session 的客户端铸出 token；B Session 的客户端 bind 本 Session 的 manager 并铸出 token；B Session 客户端用 A 铸出的 token 调 `activate()` | ① B 的客户端 bind 成功（两 Session 各有一个 manager 实例）；② `disposition == Invalid(0)`（A 的 token 不在 B 的 token 表里） |

## 契约要点

activation-token manager 与 `ActivationManagerInterfaceV1` 一样按 Session 装配
（`Session::attachInterface()`，装配点 `FocusController::registerSession()`），因此每个用户
各有一个实例：

- global 通过 `WServerInterface::ownsSocket()` 只对本 Session 的 socket 可见。
- **当前版本不校验调用者身份，也不强制单例**：任何属于本 Session 的客户端都可 bind
  （且可并发 bind 多个）。基于 appid 的认证和 singleton 拒绝路径都是 TODO，见
  `src/modules/activation-token/activationtokenmanagerinterfacev1.h` 的类注释。
- `request_token` 铸出的 token 只进**本 Session 的** `ActivationManagerInterfaceV1`
  的 token 表，不跨用户可见。
- `request_token` 已不带 `app_id`：测试客户端不传参数即编译期证据。
- 每个 `request_token` 铸一个独立 token，多个未消费 token 可并存。
- 铸出的 token 受信任（privileged）：即使不带 serial 也仍处置为 `Active`，不会降级为
  `Attention`。

## 变异提示（这些用例必须变红）

- 把 privileged token 的 `Active` 处置改回依赖 serial → 「生产回读」用例失败。
- 把 token 表改成跨实例共享（如 `static`）→ 「跨 Session 隔离」② 失败。

服务端实现位于 `src/modules/activation-token/activationtokenmanagerinterfacev1.*`；
装配点在 `src/core/focuscontroller.cpp` 的 `registerSession()`。

## 已知边界

- **身份校验与单例强制都未实现**：当前任何属于本 Session 的客户端都能 bind 并铸出必然
  `Active` 的 token。合成器后续会基于 appid 认证调用者，并恢复协议 XML 要求的
  `not_privileged` / `binding_rejected` 拒绝路径与相应测试用例（TODO）。
- token 过期（60 秒）后 `activate` 的行为：未覆盖。
- 跨 Session 用例依赖 fixture 能创建第二个真实 Session（`nobody` 等账户）；账户不可用时该
  用例打印跳过原因并返回成功，其余用例照常执行。
