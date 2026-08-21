# `treeland-virtual-output-manager-v1` 桌面业务测试规范

本文件是 [virtual-output-manager-v1](../treeland-virtual-output-manager-v1/README.md) 中桌面级场景的可执行
细则。测试源码为 `tests/protocols/treeland-virtual-output-desktop-v1/`，测试名为
`test_treeland_virtual_output_desktop_v1`。

## 前置状态

生产 `DesktopIntegrationFixture` 启动 `Helper`、root surface container 和 headless backend；
fixture 在默认 `HEADLESS-1` 之外创建真实的 `HEADLESS-2`。开始请求前，二者均为 normal
`Output`，root container 中正好有两个输出。若运行环境的 DConfig 保留了此前的 copy topology，
fixture 订阅 `TreelandConfig::configInitializeSucceed`：在该生产配置完成初始化的精确时点，清除
copy-output 的三个配置值并调用公开生产接口 `Helper::setOutputMode(Extension)`。desktop test
framework 同样以该完成信号重新检查 fixture readiness，之后才启动 client；不把宿主机遗留配置
当作测试前置条件，也不使用时间等待。

## 请求、生产路径与断言

| 步骤 | 客户端动作 | 生产路径 | 必须观察到的结果 |
| --- | --- | --- | --- |
| 建组 | `create_virtual_output("protocol-copy-group", ["HEADLESS-1", "HEADLESS-2"])` | `VirtualOutputManagerInterfaceV1Private::create_virtual_output()` → `requestCreateVirtualOutput` → `Helper::onSetCopyOutput()` | 至少一个 group `outputs` 事件含完全相同名称与顺序；`HEADLESS-1` 保持 normal 且是 root primary；`HEADLESS-2` 被替换为 copy/proxy；两个 backend output 和 root container 的两个条目仍存在。 |
| 解组 | `treeland_virtual_output_v1.destroy` | child resource 销毁 → `beforeDestroyVirtualOutput` → `Helper::onRestoreCopyOutput()` | `HEADLESS-2` 被重新创建为 normal `Output`；两个 backend output 与 root container 的两个条目仍存在；root primary 仍为 `HEADLESS-1`。 |

每一轮 client/server 同步由 `wl_display_roundtrip()` 建立顺序关系；fixture 就绪由 DConfig 初始化
完成信号和 root output model 的 `rowsInserted` 信号驱动，不以“等待若干毫秒后假定服务端已就绪”
的方式断言。创建 group 后会先由 `storeVirtualOutput()` 发出 `outputs`，再由
`storeCopyOutputConfig()` → `updateVirtualOutput()` 发出同 payload 的更新事件；测试验证其至少
出现一次及最终 payload，不把实现的两次通知误判为失败。

## 语义边界

XML 明确规定该请求不新增客户端 `wl_output`。因此本测试故意断言“已有两个输出没有消失或
新增”，而不是寻找第三个 output。这里的“虚拟输出”是对现有 outputs 的镜像组配置。

## 执行状态

该用例已在当前工作树通过，为 **E**。它不提供 output 像素比对，故不是 **V**。
