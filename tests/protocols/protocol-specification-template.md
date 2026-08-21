# `<protocol-name>` 测试规范

## 范围

- XML / interface：`<全局接口与版本>`
- 测试源码：`tests/protocols/<测试目录>/`
- Fixture：`<协议 fixture | desktop integration fixture | rendered output fixture>`
- 覆盖等级：`P | I | E | V`

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| `<名称>` | `<请求>` | `<状态、事件或业务结果>` | `P/I/E/V` |

## 生产结果

说明测试观察到的真实生产对象或子系统。如果测试只检查协议实现的局部状态，必须
明确写出这一点。

## 已知边界 / 下一项结果

列出该测试尚未证明的语义，以及下一步所需前提，例如 mapped xdg 窗口、已聚焦
seat、真实 output、source buffer 或像素读回。
