# ext-workspace-v1

使用生产启动路径中的每会话 `WExtWorkspaceManagerV1`，由纯 C 客户端连接两个真实 Session socket。
fixture 只初始化两套真实工作区；定向移除、重排用于注入协议没有提供的服务端变化。

覆盖：独立 native manager、registry 精确隔离、名称与 ID 隔离、group/output 归属、capabilities、coordinates、
active 状态、done、activate/create 在 commit 前不生效及提交后的模型结果、重名拒绝、数量上限、后台创建隔离、
移除后待提交请求失效、重排保持 ID、stop/finished。

group 覆盖会话的输出，当前只声明 CREATE_WORKSPACE 和 ACTIVATE；不声明 ASSIGN、DEACTIVATE、REMOVE。
未覆盖：输出热插拔、urgent 聚合、global 撤销及 display stop/restart。

本次环境无 CMake/编译器，测试尚未执行；缺少第二个可用账号时返回 77，不计作通过。

```sh
cmake --preset ci
cmake --build --preset ci --target test_wayland_ext_workspace_v1
ctest --test-dir build -V -R '^test_wayland_ext_workspace_v1$'
```
