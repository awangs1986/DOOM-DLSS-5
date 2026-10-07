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
- 主接收者使用独立 float3 几何法线：墙面来自本帧真实有向 seg，地板/天花板来自实际平面法线；不把压缩诊断法线解码后用作镜面几何。遮罩无效和覆盖层像素不使用该数据。
- 复用共享地图 TLAS、材质 atlas、三角形 UV 和透明 post 采样。次级命中从地图材质读取真实 texel；不从当前屏幕 RGB 获取命中颜色。
- 基础照明为命中 sector lightlevel 对应的复制 COLORMAP 行、当前未应用 gamma 的 PLAYPAL 与配置自发光；可叠加同一配置的单点光及共享 alpha 阴影。主场景照明使用本帧借用的线性结果，合成后仅编码/gamma 一次。零新增颜色的像素保留原字节。
- 每帧最多 64,000 条反射射线，加最多 64,000 条命中点阴影射线；每条最多 64 次 `RayQuery.Proceed`。`-rt-ray-budget 1..64` 可降低反射/命中点阴影预算。耗尽预算或非法命中贡献零；只有真实 miss 使用环境色。
- specular 是加法艺术近似的 Fresnel F0，roughness 是有界 3×3 空间近似，非物理路径追踪 BRDF。空间过滤拒绝不同主材质、kind、法线/位置以及不同次级状态、表面/距离的样本。
- 反射模块无跨帧累积。只要可见反射接收者，当前 SR/FSR 评价参数请求全局 reset，避免仅凭主表面运动矢量复用不可靠反射历史。此保守策略牺牲超分时间稳定性，未承诺 NR 私有历史控制。冻结 GB 输入在反射评价前结束；其反射 reset 原因通常在下一帧记录，不能作为当前评价参数的替代证据。
- GPU/材质/配置错误保留原场景或基础照明。只支持当前 320×200 场景、1280×800 输出的构建路径。

## 观察

`-rt-reflection-stats <新文件>` 记录实际 rays/hit/miss/error/exhausted、alpha 拒绝、命中阴影、预算、变色数量，以及反射 reset 请求、实际时间评价 reset 参数、成功 NGX/FSR 路径和冻结输入状态。

`-rt-reflection-pixels <新文件> -rt-reflection-pixel-tic <tic>` 记录一次所选 game tic 的真实接收者/命中世界坐标、UV、surface/triangle/instance/geometry、源 palette/COLORMAP 索引、次级 radiance、最终颜色与遍历次数。`reflection_candidate_*` 单独记录反射遍历中最后一次实际 alpha 候选的 UV、surface 与 alpha；没有候选时 surface 为 `4294967295`。`geometry_revision`、`shading_revision` 与已有 generation/material revision 来自同一当帧借用场景，可与 `-map-updates` 按 frame/game tic 对照。输出路径必须不存在；观察文件失败不阻断渲染。

纹理取样使用 GPU 实际插值后的 float UV，再按共享规则取整。解析式恰好落在整数边界时，GPU 的浮点插值可能落到相邻 texel；验收须保留这些样本，并用实际候选 UV 读取原始 post。不能把所有边界样本删除后报告透明裁剪通过。

`-gpu-timing <新文件>` 新增 `reflection_trace_ms` 与 `reflection_filter_ms`。trace 包含 GPU trace dispatch，filter 段包含空间合成、输出拷贝和开启时的观察 readback，故不是纯 shader 指令耗时。原 `rt_ms` 仍是点照明段。

原生构建通过只证明源码与 shader 可编译；地图反射、材质、透明、动态、调色板、UI 与 SR 组合结果必须以实际运行证据单独报告。

## 2026-10-07 验收范围

实际编译和运行的源码为 `79055b0f6dcbe000ca8ee9ace18fd26c2218a57b`；此后本节及观察字段说明只修改文档。Windows 原生 original/SR 两个构建完成，51 次独立运行均正常退出：22 个主场景、17 个 floor/alpha/阴影/伤害场景、10 个门/平台场景、2 个实机窗口场景。每次运行的 EXE 与现有 CSO 的 SHA-256 均与该构建对应。

- 屏外材质 A/B：关闭反射时 original/SR 输出均无差异；启用后分别有 13,440/159,654 个最终输出像素变化。10,296 个实际主场景接收者的纹理、COLORMAP、PLAYPAL/gamma、反射合成复算无差异；覆盖层验收的 381,056 个像素保持原字节。
- floor 的屏外隐藏目标有 2,304 个实际命中；次级 alpha 的 1,653 个实际候选与原始 post 无差异。6 个整数边界差异逐个保留，均由实际 GPU UV 的行 86 与理想解析式的行 87 区别解释。真实伤害 tic 163 的 10,296 个接收者只匹配活动 PLAYPAL 第 2 组。
- 正常 Use/转向 demo 触发真实门、平台运动。108 个静止主墙接收者随门打开改变次级命中，6,417 个随平台下降改变次级命中；门关闭/平台上升后，所选次级字段全部恢复。对应原场景像素未改变，GPU 的 frame/generation/material/geometry/shading revision 与同帧地图高度记录一致。
- 粗糙度验收复算 9,383 个实际过滤样本，15,419 个邻居被拒绝；合成通道误差为零。静止区域 164,736 个输出像素在 tic 70/175 间无变化。实际 SR 路径报告 NGX 成功且当帧 reset 参数为 1；这仍不表示 NR 路径运行。
- 2 次实机窗口包含游戏、暂停和菜单，7 次捕获均确认该进程拥有的窗口为前台。实际 client 为 2560×1562，导出图为 1280×800；实机截图仅用于操作和外观证据，逐像素验收使用 GPU readback 与引擎导出图。

开启观察的 219 个主场景帧，反射 trace 平均约 0.065 ms，filter/copy/readback 平均约 0.689 ms；original/SR 全部 GPU 段平均约 1.117/1.494 ms。数据包含导出及观察成本，不是无观察的游戏性能承诺。该场景整进程 local memory 在所有观测帧保持 100,319,232 字节；SR 为 181,731,328 字节。反射自身逻辑 buffer 的静态上限约为 18,314,496 字节，启用观察再加 9,472,000 字节；不含 D3D12 分配对齐、驱动与 pipeline 开销，也不重复计入借用的场景、TLAS、atlas 或照明资源。

完整原始 CSV、运行参数、源码/构建/runtime SHA-256、导出图、独立复算及早期失败记录作为该验收的外部证据保留。当前结论限于本节列出的构建、配置和实际场景；递归反射、sprite 反射、任意分辨率和无全局 reset 的反射历史均未实现。
