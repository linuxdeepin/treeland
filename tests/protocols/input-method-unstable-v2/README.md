# `input-method-unstable-v2` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/input-method-unstable-v2.xml` /
  `zwp_input_method_manager_v2`、`zwp_input_method_v2`、popup surface 与 keyboard grab v2
- 测试源码：`tests/protocols/input-method-unstable-v2/`
- Fixture：完整生产 `Treeland` / `Helper`、默认 seat 与 headless output。一个 client 作为 mapped 且
  focused 的 text-input-v2 应用，另一个 client 作为 IM；不依赖系统输入法或物理设备。`ShellHandler`
  创建的 `WInputMethodHelper` 经 `WInputMethodManagerV2` 使用 wlroots 原生 global。
- 覆盖等级：**P / E**；P 覆盖 XML 的资源、生命周期与单 seat 约束，E 覆盖真实 text-input ↔ IM 转发。

## 必须观察到的结果

| XML 语义 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| inactive 接受编辑状态 | 首个 input method 在未 `activate` 时依次 `commit_string`、`set_preedit_string`、`delete_surrounding_text`、`commit(0)` | 请求不造成协议错误，也不产生 input-method event；后续 active text-input 的 reset 语义由下一层 E 测试验证 | P |
| popup 角色和销毁顺序 | 为无角色 `wl_surface` 调用 `get_input_popup_surface`，先销毁 popup 再销毁 surface | 创建和销毁均无协议错误 | P |
| focused text-input 的状态快照 | fake app 映射 xdg toplevel，并 enable text-input-v2，设置 surrounding text/content type/cursor rectangle | fake IM 收到 `activate`、`surrounding_text("abc",3,3)`、`text_change_cause(input_method)`、`content_type(auto_completion,email)` 和一个 `done` | E |
| IM 编辑结果回传应用 | fake IM 设置 delete、commit string、preedit 后 `commit(0)` | fake app 收到正确的 `delete_surrounding_text(1,2)`、`commit_string("committed")`、preedit string/cursor/styling | E |
| active popup 定位 | fake IM 为 roleless surface 创建 popup | 收到生产 helper 按 text-input cursor rectangle 发出的 `text_input_rectangle(11,12,13,14)` | E |
| keyboard grab 资源 | `grab_keyboard` 后 roundtrip，再 `release` | 子资源可创建并按 release 生命周期销毁 | P |
| 每 seat 唯一 input method | 同一 `wl_seat` 再次 `get_input_method` | 第二对象只收到一次 `unavailable`，没有其他 event | P |
| manager 与子对象生命周期分离 | `zwp_input_method_manager_v2.destroy` 后继续向第一个 IM 提交并 roundtrip | 已创建的 IM 未被 manager destroy 连带销毁，也没有协议错误 | P |

## 生产结果

`Helper::init() → ShellHandler::init() → WInputMethodHelper →
WServer::attach<WInputMethodManagerV2>() → wlr_input_method_manager_v2_create()`。

同一个 seat 的第二个 IM 是由生产 `WInputMethodHelper::handleNewIMV2()` 调用
`wlr_input_method_v2_send_unavailable()`。测试断言这个 event 是其唯一 event，而不是仅检查
client 未崩溃。popup 与 keyboard grab 的 resource 则由
`3rdparty/wlroots/types/wlr_input_method_v2.c` 创建和销毁。

### E 级证据

测试不依赖系统输入法：一个 client 映射真实 xdg toplevel、创建 text-input-v2 并提交状态，
但暂不 enable；另一个 client 先创建 IM 并验证 inactive 编辑，第一个 client 再
enable text-input-v2 触发激活。由
`WInputMethodHelper` 从真实 focused text-input 读取 surrounding text、content type、cursor
rectangle 后发送给 IM。测试断言这些 event 的 payload，而不是 fixture 伪造 event。

IM 的 delete/commit/preedit 随后经同一生产 helper 回传到第一个 client 的 text-input 对象；
该 client 断言收到的删除范围、committed 文本与 preedit 属性。keyboard-grab 的 keymap、按键、
modifier、repeat-info 也由另一 client 的 virtual keyboard 经真实 `WSeat` 转发。因此状态同步、
编辑回传、popup rectangle 与 grab 输入为 E 级；inactive、重复 IM 与资源销毁仍为 P 级。

## 已知边界 / 下一项结果

测试客户端直接调用 XML 中全部 **11 / 11 request**（含 `destroy` / `release`）：manager 2、
input method 7、popup 1、keyboard grab 1。

已实际断言 `unavailable` 以及 active 状态链的 `activate`、`surrounding_text`、
`text_change_cause`、`content_type`、`done`、`text_input_rectangle`，并由 fake text-input 断言
IM commit 的文本 event。下列 event 仍需要真实 keyboard 输入，不能以 headless 空 seat 的
“没有 event”冒充覆盖：

- focus 转移/disable 导致的 `deactivate`，以及连续 text-input state update 是否只传递最新快照。

自包含测试已经由另一个 client 的 virtual keyboard 驱动并断言 keyboard-grab 的
`keymap`、`key`、`modifiers`、`repeat_info`；使用另一 client 避免 IM 自己注入的 virtual keyboard
被生产实现按回环规则转交 default grab。下一项 E 级测试应覆盖 disable/focus 转移后的
`deactivate` 和两次 text-input state update 的最新状态快照。
