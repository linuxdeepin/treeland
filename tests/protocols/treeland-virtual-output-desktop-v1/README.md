# `treeland-virtual-output-desktop-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-virtual-output-desktop-v1/`
- Fixture：完整 Treeland desktop integration fixture、两个真实 headless backend
  output（`HEADLESS-1`、`HEADLESS-2`）。
- 覆盖等级：桌面业务链路为 **E**。

## 场景与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| 进入复制模式 | `create_virtual_output("protocol-copy-group", ["HEADLESS-1", "HEADLESS-2"])` | 第二个既有 `Output` 被替换为 copy/proxy `Output`，第一个仍是镜像源；root output 数仍为 2；root primary 仍为 `HEADLESS-1` |
| 恢复复制模式 | 销毁 group | 第二个 `Output` 恢复为 normal output；root output 数仍为 2 |
| layer 表面跨包装交换存活 | 在进入复制模式前，向 `HEADLESS-2` 的 `wl_output` 创建并映射 bottom 层 layer surface | 表面 wrapper 在每次 copy↔normal 包装交换后仍然存活（从未收到 `closed`），`ownsOutput` 与 container 均归属当前 `HEADLESS-2` 包装 |
| 禁用 source 折叠复制模式 | 复制模式下经 `wlr-output-management` 应用禁用 `HEADLESS-1` | copy 模式折叠为 extension：`HEADLESS-1` 被禁用、`HEADLESS-2` 转为 normal 并成为 primary；`HEADLESS-2` 上绑定的 layer surface 仍存活且被重新挂入新包装的 container，客户端未收到 `closed` |

## 已证明的生产链路

复制模式进入/恢复沿用原有断言（normal/copy 对应 `Output::isSource()` 的内部类型含义，
root primary 始终为 `HEADLESS-1`）。

新增的 E 级链路：客户端在复制模式之前向 `HEADLESS-2` 绑定一个 bottom 层 layer surface，
随后依次经历**进入复制模式**、**恢复**、**再次进入复制模式**、**禁用 source 触发折叠**
四次 copy↔normal 包装交换。每次交换后服务端读回必须满足：wrapper 存活、`ownsOutput` 为
当前 `HEADLESS-2` 包装、container 为 `HEADLESS-2` 的 output layer container；客户端
`closed` 事件计数保持 0。折叠由 `wlr-output-management` 的 `apply` 触发（`test` 不触发
转换），断言 `configuration succeeded`、`HEADLESS-1` wlr output 被禁用、`HEADLESS-2`
转为 source 并成为 primary。

这一场景正是生产 bug 的回归项：复制模式下禁用 source 会把镜像输出的包装对象销毁重建，
旧实现把绑定在该输出上的 layer surface 当作热拔插关闭（`WLayerSurface::closed()`），
导致 dde-desktop 等绑定具体输出的 layer 窗口被关闭后回退为普通窗口。修复后 layer
surface 在包装交换中被 detach 并重新挂入新包装的 container，而不是被关闭。

## 已知边界 / 下一项结果

- 未覆盖热拔插（物理移除）镜像源后的 successor 选择；该路径沿用
  `treeland-virtual-output-manager-v1` 的既有边界说明。
- 折叠仅验证了"仅剩一个 mirror"的路径（`handleCopyModeSourceDisabled` 返回 false）；
  多 mirror 存活时提升新 source 的路径（`promoteCopyOutputToSource`）未在本测试覆盖。
- 未覆盖 layer surface 的渲染像素（V 级）：本测试只断言 wrapper/container 归属与
  `closed` 事件计数。
- 本测试的业务断言在本地工作树已通过（QTest 3/3，29ms）；完整 protocol suite 的
  CTest 执行结果以该提交上的本地或 CI 运行为准（当前会话中全部 protocol target 存在
  与测试无关的 dconfig 服务退出挂起，属于环境问题，见 `framework/README.md`）。
