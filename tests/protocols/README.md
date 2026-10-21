# 协议测试指南

本目录用纯 C 的 Wayland client 验证 Treeland 的真实服务端行为。每个 target 由一个 CTest
可执行文件承载，独立进程、独立 headless 环境；CTest 把退出码 `77` 记为 Skip，skip 不算覆盖。

- 框架架构、同步规则、fixture 层次与验收清单：`framework/README.md`
- 每个协议的可观察契约、覆盖等级与剩余边界：`INDEX.md`
- 新增/调试协议测试的步骤、模板与断言要求：`.agents/skills/treeland-protocol-test/SKILL.md`

## 运行

首次配置、切换 preset 或新增测试目录后先重新配置：

```bash
cmake --preset ci
```

构建并运行单个 target（target 名是 `test_<NAME>`）：

```bash
cmake --build --preset ci --target test_treeland_keyboard_state_notify_unstable_v1
ctest --test-dir build --output-on-failure \
  -R '^test_treeland_keyboard_state_notify_unstable_v1$'
```

加 `-V` 查看合成器与 Wayland 日志。跑全部协议测试：

```bash
ctest --test-dir build --output-on-failure -L protocols
```

必须通过 CTest 运行：它注入 headless backend、renderer、超时与 skip 语义；直接执行
`build/tests/protocols/.../test_*` 会漏掉这些环境。

## 覆盖率报告

每次 `ctest -L protocols` 结束后由 CTest 的测试后钩子输出，收集已注册测试的 `XML` 与
`EXTRA_XMLS`。Treeland 私有协议的分母来自 `src/` 与 `waylib/src/server/` 生产构建文件里
实际生成服务端绑定的 XML，而不是安装目录库存；系统 `wayland-protocols` 与 wlroots 的未注册
XML 只列为提示，不计入分母。

一条命令完成构建并跑全量：

```bash
cmake --build --preset ci --target protocol-test-coverage-target
```

## 可选条件测试

uinput 与 GPU target 默认不注册：

```bash
cmake --preset ci -DTREELAND_ENABLE_UINPUT_PROTOCOL_TESTS=ON
cmake --preset ci -DTREELAND_ENABLE_GPU_PROTOCOL_TESTS=ON
```

`treeland-input-manager-uinput-v1` 创建 `BUS_VIRTUAL` uinput 键盘（不写按键），验证设备经
udev/libinput/wlroots 进入 `WBackend::inputAdded` / `inputRemoved` 后，input-manager 向
client 发送 Keyboard capability 的 available/unavailable。CTest 固定注入与 `treeland.service`
一致的 `LIBSEAT_BACKEND=seatd; SEATD_SOCK=/run/dde-seatd.sock`；没有 `/dev/uinput` 写权限或
连不上该 socket 时，runner 在启动前以 `77` 标记 Skipped。
