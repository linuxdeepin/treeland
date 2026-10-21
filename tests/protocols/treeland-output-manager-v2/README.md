# `treeland-output-manager-v2` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-output-manager-v2/`
- Fixture：启动 headless backend 并创建真实 `wl_output` global 的协议 fixture。
- 覆盖等级：primary-output 为 **I**；picture-control 创建与提交为 **I**。
- v2 用 `treeland-output-uuid-unstable-v1` 的 uuid 字符串寻址输出：客户端先
  `get_uuid(wl_output)` 取 uuid，再用 uuid 调用本协议。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| uuid | 绑定 uuid manager 并对真实 `wl_output` 请求 `get_uuid` | 收到非空 uuid 字符串 |
| 主输出 | 绑定 manager | 初始 `primary_output` 事件携带主输出的 uuid（无主输出时为空串） |
| 设置主输出 | 用有效 uuid 请求 `set_primary_output` | 收到携带同一 uuid 的 `primary_output` 确认（同值请求也确认） |
| 设置主输出失败 | 请求 `set_primary_output` 传未知 uuid | 收到 `primary_output_failed(invalid_output=0)`，连接不断开 |
| 禁用输出 | 先在服务端禁用输出，再用其 uuid 请求 `set_primary_output` | 同上，非致命 |
| 禁用输出后提交 | 输出禁用后对已有 picture control `commit` | `result=invalid_output(3)`，连接保持；该对象随即永久失效 |
| picture-control | 用有效 uuid 请求 `get_picture_control` | 创建 control 并收到初始 `brightness`/`color_temperature`，无协议错误，创建时**不发** `result` |
| 未知 uuid | 用未知 uuid 请求 `get_picture_control` | 对象仍创建，但**无需 commit** 即收到 `result=invalid_output(3)`，且**无**初始 `brightness`/`color_temperature`；连接保持 |
| 失效后提交 | 已收到 `invalid_output` 的对象再 `commit`（含 `set_*` + `commit`） | 不再有任何事件（`result_event_count` 不变），连接保持，仅 `destroy` 有效 |
| 提交（无变更） | `commit` 且无 pending 值 | `result` 事件值为 `success`(0) |
| 设置越界色温 | `set_color_temperature(500)` | 立即引发致命协议错误 `error.invalid_color_temperature`(0)，连接断开，无 `result` 事件 |

## 生产结果

uuid 来自真实 output 的 `Output::uuid()`；picture control 在**未失效**期间按 uuid
跟随输出（监听 output model 增删重新解析，输出回来后会补发初始值）。
`commit_result` 枚举值与 v1 的 uint flag（1=成功/0=失败）相反。

`invalid_output` 是**终结**结果：无论是创建时立即下发，还是后续 `commit` 返回，
该 picture control 对象都永久失效（inert）——只有 `destroy` 有效，不再发任何
`result`/`brightness`/`color_temperature` 事件，之后即使插入 uuid 匹配的输出也
不恢复；客户端必须销毁它并重新 `get_picture_control`。范围校验仍保留在 `set_*`
时刻（越界仍是致命协议错误），失效对象只是不再写入 pending、不再产出事件。

## 已知边界 / 下一项结果

尚未覆盖 `invalid_brightness` 协议错误、`unsupported` 结果、渲染颜色变化验证，
以及输出真正销毁/重建后的 uuid 跟随（headless 重建会换 output name，需 fixture
支持固定输出名才能测同一 uuid）。「失效对象在匹配 uuid 的输出重新出现后仍不恢复」
属热插拔场景，同样受该 fixture 限制，暂由实现侧的 inert 状态保证。
