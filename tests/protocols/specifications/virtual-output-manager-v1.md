# `treeland-virtual-output-manager-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-virtual-output-manager-v1/`
- Fixture：协议 fixture。
- 覆盖等级：基础资源用例为 **P**；桌面业务用例为 **E**。

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| 参数校验 | 用有效、空名称、重复名称、空 outputs 调用 `create_virtual_output` | 创建或错误资源符合 manager 契约 |
| 查询/列举 | 获取已创建 virtual output | 收到正确的输出名称和 list 事件 payload |
| 错误事件 | 服务端触发 error 路径 | 资源收到预期 code 和 message |

## 已证明的生产链路

客户端调用 `create_virtual_output("group1", ["DP-1", "HDMI-1"])`，并在返回资源上
注册 listener。生产 manager 必须发送 `outputs("group1", ["DP-1", "HDMI-1"])`；
随后 `get_virtual_output("group1")` 得到的第二个资源也必须收到相同 payload，
`get_virtual_output_list()` 必须列出 `group1`。

同一测试还发送空 group 名、空 `wl_array` 和重复 `group1`，分别断言生产资源发送
`INVALID_GROUP_NAME`（消息含 empty）、`INVALID_SCREEN_NUMBER`、`INVALID_GROUP_NAME`
（消息含 already exists）。最后 fixture 调用 manager 的既有事件路径，客户端断言
resource 收到 `INVALID_OUTPUT` 和 `test error`。这些是 manager/resource 层的实际协议
逻辑，不代表后端显示输出已创建。

## 桌面级业务验证

`treeland-virtual-output-manager-v1` 的 XML 明确规定：调用该接口**不会**向客户端新增
`wl_output`。因此，“创建新的 `WOutput` / `wl_output`”不是本协议正确的验收条件。

桌面级用例位于 `tests/protocols/treeland-virtual-output-desktop-v1/`，使用生产桌面
fixture 的两个真实 headless backend output：`HEADLESS-1` 和 `HEADLESS-2`。它按以下
顺序验证真实业务链路：

1. 客户端发送 `create_virtual_output("protocol-copy-group", ["HEADLESS-1", "HEADLESS-2"])`；
2. 收到 production manager 发出的 `outputs` 事件，名称及数组内容必须完全一致；
3. `Helper::onSetCopyOutput()` 将第二个既有 `Output` 替换为 copy/proxy `Output`，第一个
   仍是镜像源；root output 数仍为 2，两个既有 backend output 都仍存在；
4. 客户端销毁 group 后，`Helper::onRestoreCopyOutput()` 把第二个 `Output` 恢复为 normal
   output；root output 数仍为 2。

这里的 normal/copy 判断对应 `Output::isPrimary()` 的内部类型含义（normal 为 true、copy
为 false），并不等同于“root primary output”。测试还单独断言 root primary 仍为
`HEADLESS-1`。

该用例已在当前工作树通过，桌面业务覆盖等级为 **E**。

## 未覆盖

尚未覆盖热插拔时镜像源被移除后的 successor 选择、持久化配置跨进程恢复，以及真实物理
显示器上的显示内容一致性。这些均不要求、也不应通过“新增客户端 `wl_output`”来验证。
