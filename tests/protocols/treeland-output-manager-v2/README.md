# `treeland-output-manager-v2` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-output-manager-v2/`
- Fixture：启动 headless backend 并创建真实 `wl_output` global 的协议 fixture。
- 覆盖等级：primary-output 为 **I**；picture-control 创建与提交为 **I**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 主输出 | 绑定 manager 和真实 `wl_output` | 初始 `primary_output` 事件携带非空 `wl_output` 对象 |
| 设置主输出 | 用真实 `wl_output` 请求 `set_primary_output` | 收到 `primary_output` 事件确认 |
| picture-control | 为真实 `wl_output` 请求 `get_picture_control` | 创建 control，连接不产生协议错误 |
| 提交（无变更） | `commit` 且无 pending 值 | `result` 事件值为 `success`(0) |
| 设置越界色温 | `set_color_temperature(500)` | 立即引发致命协议错误 `error.invalid_color_temperature`(0)，连接断开，无 `result` 事件 |

## 生产结果

primary-output 事件使用启动 headless backend 后创建的真实 output 的 `wl_output` 资源，
而非手工事件。`commit_result` 枚举值与 v1 的 uint flag（1=成功/0=失败）相反。

## 已知边界 / 下一项结果

尚未覆盖 `primary_output_failed` 事件（需要构造禁用或不可用的输出）、
`invalid_brightness` 协议错误、`unsupported` 结果以及渲染颜色变化验证。
