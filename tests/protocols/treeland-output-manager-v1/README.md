# `treeland-output-manager-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-output-manager-v1/`
- Fixture：启动 headless backend 并创建真实 `wl_output` global 的协议 fixture。
- 覆盖等级：primary-output 为 **I**；color-control 创建为 **I**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 主输出 | 绑定 manager 和真实 `wl_output` | 初始 `primary_output` 指向当前主输出 |
| 非主输出 | 请求另一个 output | 不发送伪造的 primary-output 事件 |
| color-control | 为真实 `wl_output` 请求 control | 创建 control，连接不产生协议错误 |

## 生产结果

primary-output 事件使用启动 headless backend 后创建的真实 output，而非手工事件。

## 已知边界 / 下一项结果

尚未观察 color-control 的初始事件、提交结果、output 状态或渲染颜色变化。无效
`wl_output` 错误路径需要专门构造未由 `Helper` 管理的 server resource；常规客户端
绑定真实 `wl_output` global 无法覆盖该前置条件。
