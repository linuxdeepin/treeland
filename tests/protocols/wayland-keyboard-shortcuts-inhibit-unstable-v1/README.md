# keyboard-shortcuts-inhibit-unstable-v1

覆盖等级：I / P。

测试映射真实 xdg toplevel 并将其设为 seat 焦点，随后请求 inhibitor 并断言收到
`active`。失焦后的 compositor policy、`inactive` 事件与真实快捷键分发行为尚未覆盖。
