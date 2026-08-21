# `treeland-wallpaper-color-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-wallpaper-color-v1/`
- Fixture：由服务端测试回调提供颜色的协议 fixture。
- 覆盖等级：**I**。

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| 订阅 | 绑定 manager | 客户端收到选择输出的当前颜色状态 |
| 更新与去重 | fixture 注入颜色变化 | 生产订阅层仅为真实变化发送一次颜色通知 |

## 已证明的生产链路

验证 production subscription/notification 层及其去重行为。

## 未覆盖

颜色由 fixture 直接注入；未经过壁纸分析，也未验证 output 色彩应用。
