# remote-subsurface-unstable-v1

覆盖等级：E / P。

测试导出真实 mapped xdg parent 与 child surface，交换 token 后调用
`create_remote_subsurface`、`set_position(-12, 34)` 和 `place_below(\"\")`。它在
compositor 线程回读 parent 的 `WSubsurface`，断言 Remote 类型、父对象、位置和 Below
层级；另断言未知 parent 与 sibling token 分别产生 `parent_rejected` 和
`invalid_sibling`。跨 client、循环 parent、映射生命周期及 sibling 间排序仍未覆盖。
