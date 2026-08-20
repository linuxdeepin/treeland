# `wlr-screencopy-unstable-v1` 测试规范

## 范围

- XML / interface：`3rdparty/wlroots/protocol/wlr-screencopy-unstable-v1.xml` /
  `zwlr_screencopy_manager_v1` 与 `zwlr_screencopy_frame_v1` v3。
- 测试源码：`tests/protocols/wlr-screencopy-unstable-v1/`。
- Fixture：完整 Treeland desktop integration fixture、真实 `1920×1080` headless output，以及覆盖全
  output 的红色 layer-surface。
- 覆盖等级：**P / V**；P 覆盖 frame metadata/lifecycle，V 在 headless pixman renderer 上读取生产
  output copy 到 client SHM buffer 的像素。

## 必须观察到的结果

| XML 语义 | 客户端动作 | 必须观察到的结果 | 证据层级 |
| --- | --- | --- | --- |
| 全 output capture | map 红色 background layer 后 `capture_output`，收到 metadata 后以匹配 SHM buffer 调用 `copy` | `buffer → buffer_done → flags → ready`；buffer 中心像素为红色 | V |
| region capture | `capture_output_region(output,928,508,64,64)` 并 copy 匹配 SHM buffer | metadata 为 `64×64`，`flags → ready`，region 中心像素为红色 | V |
| damage capture | 新 region frame 对匹配 SHM buffer 调用 `copy_with_damage` | 首次 capture 的完整初始 damage 后，`damage → flags → ready`，中心像素为红色 | V |
| manager/frame 生命周期 | destroy 两个完成 frame 与 manager | 无 protocol error；frame 在 terminal event 后由 client 销毁 | P |

## 生产结果

`Helper::init()` 直接调用 `wlr_screencopy_manager_v1_create()` 发布 wlroots native manager；waylib
没有 wrapper，native manager 随 `wl_display` 销毁。`copy` 会等待真实 output 的下一次带 buffer commit；
wlroots 将该 production buffer 复制到 client 提供的 SHM buffer，随后发送 `flags` 和 `ready`。

### V 级覆盖流程

1. headless pixman CI 映射覆盖 `1920×1080` output 的红色 layer-surface；
2. client 请求全 output 或 64×64 region capture，读取 wlroots `buffer/buffer_done` metadata 并创建
   精确匹配的 SHM target buffer；
3. `copy` 请求安装 output-commit listener 后，fixture 渲染每个 production output；这样不依赖
   root output 顺序与 client 绑定的 `wl_output` 顺序一致，wlroots 只对被捕获 output 的下一次
   buffer commit 完成 copy；
4. client 仅在 `flags → ready` 后 mmap target buffer，读取中心像素并要求红色；`copy_with_damage`
   还要求首次累积的完整 `damage` 在 `flags/ready` 前到达。

这证明的是 compositor 输出 buffer 到独立 client buffer 的真实像素内容，不是 fixture 手工写入 target。
当前实现待首次 CI 执行记录结果，不把未运行结果写成已通过。

## 已知边界 / 下一项结果

- `failed`、`invalid_buffer` 与 `already_used` 尚待独立 frame/连接覆盖。
- headless pixman 不提供 v3 `linux_dmabuf`；DMA-BUF target、linux-dmabuf event 和 GPU import/readback
  应作为 opt-in GPU/DRM runner，能力不可用时 Skip。
- 尚未验证 cursor overlay、output transform/scale、output destroy，以及权限/授权策略；native wlroots
  manager 当前没有 trusted-client hook。
