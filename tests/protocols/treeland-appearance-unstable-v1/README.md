# appearance-unstable-v1

覆盖等级：I / P。

测试绑定 appearance 与 appearance-manager global，断言 bind 时收到初始
`color_scheme`，并通过 manager 的 `set_color_scheme(dark)` 验证配置变更会广播到
已绑定的 appearance client。其余外观字段、无效值和 DConfig 配置替换仍未覆盖。
