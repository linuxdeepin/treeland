# Liquid Glass 色散（Chromatic Dispersion）实现说明

> 文档记录删除前的实现方式，供后续重做时查阅。
> **当前代码库已移除色散功能**；玻璃仅保留单通道折射 + Fresnel 高光。

## 1. 目标观感

希望边缘出现由**折射后的背景**被「拉开」形成的蓝/青/紫细边，而不是：

- 在玻璃轮廓上描固定色边
- 对未折射底图做大位移 RGB 错位（露底、叠红）
- 轴对齐矩形色带

理想结构：

```text
玻璃外缘 (SDF) ──弧形环带── 折射后的内容轮廓
                    ↑
              色散主要出现在这里
```

## 2. 整体管线（删除前）

```text
1) 圆角矩形 SDF → 外形裁切 + AA
2) bezel 剖面 surfaceHeight(t) → 边缘斜率 slopeMag
3) 平面法线 n2（圆角连续旋转）
4) 单通道主折射：
     inward = -n2
     magG = Snell 楔形幅度 * contentRamp
     uvG = texCoord + inward * magG / size
5) 色散（可选）：
     在 distFromEdge < dispersionWidth 的环带内
     对 R/G/B 使用略不同的 mag（同一 inward 方向）
     color = mix(sG, vec3(sR.r, sG.g, sB.b), fringeMask)
6) Fresnel 式边缘高光 + tint
```

## 3. 主折射（与色散共用的场）

### 3.1 几何

| 量 | 含义 |
|----|------|
| `distFromEdge` | SDF 内侧距离（像素） |
| `bezel` | 厚度从 0→满的过渡带宽度 |
| `t = clamp(distFromEdge/bezel,0,1)` | 在 bezel 内的归一化位置 |
| `h = surfaceHeight(t)` | squircle 剖面相对高度 |
| `H = h * thickness` | 有效光学高度 |
| `n2` | 圆角矩形连续平面法线（指向外侧） |
| `inward = -n2` | 内容采样方向（向内拉） |

### 3.2 幅度（Snell 薄楔近似）

```text
slopeMag = min(|dh| * thickness/bezel, refractionMaxTan)
slopeAngle = atan(slopeMag)
eta = 1/ior
sinI = slopeMag / sqrt(slopeMag²+1)
mag = H * contentRamp * max(tan(slopeAngle) - tan(asin(eta*sinI)), 0)
mag = min(mag, maxDisp)
off = inward * mag
uv  = texCoord + off/size
```

### 3.3 contentRamp（贴边 vs 折射强度）

```text
contentRamp = mix(contentEdgePull, 1.0, smoothstep(0, contentRampEnd, t))
```

- **contentEdgePull**：贴边保留多少折射位移（默认 0.42）
- **contentRampEnd**：bezel 内 t 到多少拉满（默认 0.50）
- 用于减轻「图标被拽进内侧、外圈大黑缝」，与色散正交，删除色散后仍可保留。

## 4. 色散算法（已删除部分）

### 4.1 参数

| Uniform / API | 默认 | 含义 |
|---------------|------|------|
| `dispersion` / `dispersionPx` | 5 | 最大通道分离（像素量级） |
| `dispersionWidth` | 6 | 环带宽度（从玻璃外缘向内，px）；0=关 |
| `dispersionBlend` | 0.75 | 混入强度 |

DConfig：`glassDispersionPx` / `glassDispersionWidth` / `glassDispersionBlend`。

### 4.2 空间 mask

```text
fringeMask = smoothstep 从外缘向内 width 像素
fringeMask = fringeMask² * blend
```

仅在此外圈环带做 CA；带外只用 G 路径单次采样。

### 4.3 分通道折射幅度

```text
di ∝ dispersion * fringeMask
iorR = ior - di,  iorG = ior,  iorB = ior + di   // 蓝光 n 更大
magR, magG, magB = 各自 Snell 幅度
// 保证最小分离，避免 di 太小时看不出 CA：
if (magB - magR < dispersion*fringeMask)
    以 magG 为中心强制拉开 minSplit
uvR/G/B = texCoord + inward * magR/G/B / size
color = mix(sG, vec3(sR.r, sG.g, sB.b), fringeMask)
```

要点：

- **方向相同**（都是 `inward`），只变 \|pull\|
- **不**用未折射的 `texCoord` 单独作为 R 通道（避免露底叠红）
- 结果色来自背景采样差，不是固定调色板

### 4.4 与错误实现的对比

| 错误 | 后果 |
|------|------|
| R 采 `texCoord`（位移 0），B 大位移 | 露底图 + 叠红 |
| 固定蓝/青 `mix` 染色 | 像描边不是色散 |
| CA 强度绑整块 displacement | 整个折射区都在色散，width 失效 |
| 轴对齐大 offset | 矩形色带，不贴圆角弧 |

## 5. 已知局限（删除原因）

1. 环带锚在 **玻璃 SDF 外缘**，不是严格的「折射后图标轮廓」；易与目标效果的「贴小圆角的弧形膜」不一致。
2. 最小 px 分离与 Snell 微差混用，大 thickness 时仍可能脏边、黑点（远处深色被拉开）。
3. 多次调参仍难稳定达到「细、柔、青蓝、弧形填缝」观感。
4. 产品决策：**去掉色散**，只保留单通道折射玻璃。

## 6. 删除后的代码状态

- Shader：仅 `magG` 单次折射采样 + Fresnel 高光
- 移除 uniforms / QML / dconfig / test_glass 滑条 / 色散单测
- **保留** `contentEdgePull` / `contentRampEnd` / `refractionMaxTan`（折射塑形，与色散无关）

## 7. 若将来重做，建议顺序

1. 先把单通道 distortion 做到：直线进圆角连续弯、内容贴边、无大黑缝
2. 再在**折射 UV**上做 1–3px 级 CA，mask 跟折射梯度或内容轮廓走
3. 最后才加 Fresnel/Bloom
4. 验收以 QQ 顶弧是否「细青蓝膜」为准，而不是 RGB 分离是否「够大」

---

*文档对应删除前工作树中的 `liquidglass.frag` 实现；删除提交见后续 git history。*
