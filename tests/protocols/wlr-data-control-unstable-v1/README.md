# `wlr-data-control-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-data-control-unstable-v1.xml` /
  `zwlr_data_control_manager_v1`、`zwlr_data_control_device_v1`、source 与 offer v2。
- 测试源码：`tests/protocols/wlr-data-control-unstable-v1/`。
- Fixture：完整 Treeland desktop integration fixture、默认 `WSeat` 与两个独立 Wayland client；
  source/target 通过 data offer 与 pipe 传输 selection 内容。
- 覆盖等级：**P / E**；P 覆盖 resource 生命周期，E 覆盖真实 seat selection、offer 与数据传输。

## 必须观察到的结果

| 场景 | 动作 | 断言 | 证据层级 |
| --- | --- | --- | --- |
| selection transfer | source offer `text/plain` 后 set selection；target receive pipe | target 收到 offer/selection，source 收到 `send` 并写出 payload，target 读回完全一致内容 | E |
| source replacement | 第二 source 替换 selection | 第一 source 收到 `cancelled` | E |
| primary selection | 第三 source set primary selection | target 收到 v2 `primary_selection` | E |
| manager lifetime | destroy manager 后现有 device `set_selection(NULL)` | child device 仍有效，无协议错误 | P |

## 生产结果

`Helper::init()` 直接创建 wlroots native manager；waylib 当前没有 wrapper，native global 随
display 销毁。source 与 target 是两个独立 Wayland client。source 的 data source 经原生 data-control manager
写入真实 `WSeat` selection；target 的真实 data-control device 收到 offer/selection 后，以 pipe
调用 `receive`。wlroots 再向 source 发送 `send(fd,mime)`，source 写入 payload，target 从同一 pipe
读回逐字节一致内容。selection replacement 的 `cancelled` 和 v2 primary selection 均来自这条
真实 seat 链路，不是测试手工发送的 protocol event。

因此 selection、primary selection、offer/receive/send 与 replacement 为 E 级；manager destroy
后 child device 仍可用只证明 resource 生命周期，属于 P 级。

## 已知边界 / 下一项结果

- 尚缺独立连接中的 `used_source`、`invalid_offer` protocol error 断言，以及 device seat-destroy 的
  `finished`。
- 该 global 当前由 Treeland 直接公开；wlroots native implementation 没有 trusted-client 判定或
  authorization callback，因此 `unauthorized` 没有可触发的生产分支。测试记录该事实，不把所有
  client 被允许创建 data-control device 误报为授权策略测试。
