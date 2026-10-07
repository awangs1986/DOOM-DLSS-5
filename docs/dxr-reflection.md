# 指定地图材质的单次反射

本功能是地图光追反射。它不表示 DLSS 5 NR、DLSS SR、DLAA 或 Ray Reconstruction 成功运行；这些路径有各自的实际运行状态。

## 启用与配置

启动时传入 `-rt-materials <文件>`。不提供配置时默认不启用反射；配置中未列出的材质不反射、不自发光。`-rt-reflections-off` 保留配置但关闭反射，`-nort` 同时关闭地图 RT 消费者。示例：

```text
version 1
environment 0.02 0.03 0.05
material wall RTMIRROR 1 0 0.5 0 0 0
material flat FLOOR0_1 1 0.1 0.04 0 0 0
```

`material` 字段依次为命名空间（`wall` 或 `flat`）、原始材质名称、反射开关（0/1）、roughness、specular、线性 emissive RGB。名称为 1–8 个 ASCII 字符且不区分大小写，墙纹理与 flat 分别匹配；动画材质按原始名称配置，采样使用当前解析后的纹理。roughness 范围 0–0.25，specular 范围 0–1，emissive/environment 各通道范围 0–16。文件限 64 KiB、128 条材质规则；允许空白与 `#` 行注释，重复材质、重复 environment、未知字段、非有限数或格式错误拒绝整个文件，并保留原场景或已完成的基础照明。

## 渲染与边界

- 只向有效墙面与地板接收者发出一次确定性镜面射线，不反射 sprite，不递归反射。天花板可配置自发光，但不发出反射射线。
- 复用共享地图 TLAS、材质 atlas、三角形 UV 和透明 post 采样。次级命中从地图材质读取真实 texel；不从当前屏幕 RGB 获取命中颜色。
- 基础照明为命中 sector lightlevel 对应的复制 COLORMAP 行、当前未应用 gamma 的 PLAYPAL 与配置自发光；可叠加同一配置的单点光及共享 alpha 阴影。主场景照明使用本帧借用的线性结果，合成后仅编码/gamma 一次。零新增颜色的像素保留原字节。
- 每帧最多 64,000 条反射射线，加最多 64,000 条命中点阴影射线；每条最多 64 次 `RayQuery.Proceed`。`-rt-ray-budget 1..64` 可降低反射/命中点阴影预算。耗尽预算或非法命中贡献零；只有真实 miss 使用环境色。
- specular 是加法艺术近似的 Fresnel F0，roughness 是有界 3×3 空间近似，非物理路径追踪 BRDF。空间过滤拒绝不同主材质、kind、法线/位置以及不同次级状态、表面/距离的样本。
- 反射模块无跨帧累积。只要可见反射接收者，当前 SR/FSR 评价参数请求全局 reset，避免仅凭主表面运动矢量复用不可靠反射历史。此保守策略牺牲超分时间稳定性，未承诺 NR 私有历史控制。冻结 GB 输入在反射评价前结束；其反射 reset 原因通常在下一帧记录，不能作为当前评价参数的替代证据。
- GPU/材质/配置错误保留原场景或基础照明。只支持当前 320×200 场景、1280×800 输出的构建路径。

## 观察

`-rt-reflection-stats <新文件>` 记录实际 rays/hit/miss/error/exhausted、alpha 拒绝、命中阴影、预算、变色数量，以及反射 reset 请求、实际时间评价 reset 参数、成功 NGX/FSR 路径和冻结输入状态。

`-rt-reflection-pixels <新文件> -rt-reflection-pixel-tic <tic>` 记录一次所选 game tic 的真实接收者/命中世界坐标、UV、surface/triangle/instance/geometry、源 palette/COLORMAP 索引、次级 radiance、最终颜色与遍历次数。输出路径必须不存在；观察文件失败不阻断渲染。

`-gpu-timing <新文件>` 新增 `reflection_trace_ms` 与 `reflection_filter_ms`。trace 包含 GPU trace dispatch，filter 段包含空间合成、输出拷贝和开启时的观察 readback，故不是纯 shader 指令耗时。原 `rt_ms` 仍是点照明段。

原生构建通过只证明源码与 shader 可编译；地图反射、材质、透明、动态、调色板、UI 与 SR 组合结果必须以实际运行证据单独报告。
