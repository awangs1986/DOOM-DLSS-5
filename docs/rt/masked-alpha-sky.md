# 遮罩纹理与天空的光追边界

对应 GitHub Issue #14。本文记录实现契约与真实 RTX 验收结果。

## 遮罩表面的身份

双侧 linedef 的中间纹理占用原有稳定 surface 槽位：
`line * 6 + side * 3 + MAP_WALL_MID`。每个 sidedef 分别保留材质、偏移、
法线与 UV。只在前后 sector 存在正高度公共开口时产生几何；几何覆盖公共
开口，贴图 post 本身的透明度由候选命中决定。

#13 的 grouped BLAS 按拥有者 sector 划分，先排列 opaque geometry，再排列
masked geometry；range 表保留实际 descriptor 索引。共享 range 表把
`(instance, geometry, primitive)` 映射到全局
triangle；primitive 的 surface 字段再映射到稳定 surface。全局 vertex 与
index 缓冲保留相同排列，供重心坐标插值 UV。

## 原始贴图规则

- U 是 sidedef `textureoffset` 加沿完整有向 linedef 的距离。
- 非 `DONTPEGBOTTOM` 的 V anchor 是前后 ceiling 的较低值。
- `DONTPEGBOTTOM` 的 V anchor 是前后 floor 的较高值加当前解析纹理高度。
- V 是 anchor 加 `rowoffset` 再减世界高度。
- 横向采样使用 `floor(U) & width_mask`。
- 遮罩 post 在 V 小于 0 或不小于纹理高度时透明，不做垂直重复。
- alpha 独立于调色板索引：存在的 post 即使索引为 0 也可遮挡。

原始 software renderer 的遮罩墙柱记录墙体深度、物理法线与原始材质样本；
空洞保留后方已绘制表面的样本。精灵保持 billboard 规则。武器和 UI 在场景
之后合成，不改写不可变场景输入。

## 共享候选处理

`alpha-trace.hlsli` 提供
`TraceAccepted(RayDesc, RtTraceBounds, bool shadow, bool opaqueDiagnostic)`。
调用者显式传入 range、triangle、vertex、index、surface、descriptor 与 pixel
数量；root SRV 不携带元素数量，因此 helper 不依赖 `GetDimensions`。

每条射线最多调用 64 次 `Proceed`，预算可在 1..64 内指定。候选处理检查
range 身份、所有数组范围、有限 UV、descriptor 尺寸和 atlas 范围，再读取
二值 post alpha。两侧遮罩几何共面，按现有绕序选择接收者所在侧，避免把
另一侧材质错误用于同一候选。

返回值区分 MISS、HIT、ERROR、EXHAUSTED，并记录步数、alpha 检查/拒绝次数
和最后候选的 surface、UV、alpha。阴影把 ERROR/EXHAUSTED 作为保守遮挡；
共享反射消费者必须把失败结果作为无新增反射处理。失败状态保留在观测数据中。

## 天空

sky floor/ceiling 按原地图规则省略相应平面；两个相邻 ceiling 都是 sky 时，
不会因 ceiling 高度不同添加上层封闭墙。室内 ceiling、实际地板与有限实体
墙保留原边界。没有添加虚构 sky cap 或替换 WAD 几何。

## 材质与场景颜色

共享 atlas 从原始 WAD post 复制 indexed+alpha 数据，最多 4096 个 descriptor、
16M 个 pixel、1M 个 surface metadata 槽位。材质引用或动画解析发生变化时
重建 atlas；只改变 UV 或 sector lightlevel 时更新 surface metadata，避免
重新复制整套贴图。

光照输出同时提供当前帧 raw linear RGB 和 packed color 的借用视图。linear
资源处于 `NON_PIXEL_SHADER_RESOURCE`，packed 资源处于
`COPY_SOURCE | NON_PIXEL_SHADER_RESOURCE`。消费者不得修改借用资源或改变
其跟踪状态；最终场景先合成，再加原始覆盖层。

## 诊断入口

- `-rt-alpha-debug`：输出候选通过/遮挡与失败状态颜色。
- `-rt-alpha-opaque`：故意把有效遮罩候选整个开口作为实体的对照模式。
- `-rt-alpha-budget N`：指定 1..64 遍历预算，便于观察预算耗尽的保守结果。
- `-rt-light-pixels PATH -rt-light-pixel-tic TIC`：记录 receiver 世界位置、
  candidate UV/alpha、trace status 与最终颜色。

诊断模式均在日志中明确标记，不作为普通最终阴影效果。

## 当前范围

采用最近 texel 的二值 post alpha，没有软阴影或半透明混合。几何级候选预算
保证有界执行；本实现保留失败观测与保守降级，没有增加错误比例自动关闭策略。
共享反射调用规则属于消费者契约；本票的最终画面对照验收是点光源硬阴影。
下面固定相机、静态回放场景的测量不替代 #13 动态场景或 #15 反射的单独验收。

## 实机验收身份

最终运行时源码为 `384c1ac5383db0f7161eba8e9ac07b560cace4f2`，包含 #13 的
等高 MID 激活时几何 revision 修复。本文件随后提交，未改变已验收运行时源码。
Windows 原生 original 与 SR 两个目标均重新编译 `map_source.c` 并链接成功；
本地与远端 275 个源码文件一致（排除生成的 NGX install lock），独立复核的
211 个编译输入没有缺失或差异。只改 alpha include 的依赖探针触发了 shader
重编译，恢复后 helper 与 shader 哈希均恢复。

设备为 NVIDIA GeForce RTX 5070 Ti Laptop GPU（vendor `10de`、device `2f18`），
DXR tier 1.2、Shader Model 6.5；37 次最终回放运行于真实交互 Session 3。

| 产物 | SHA256 |
| --- | --- |
| original `windoom.exe` | `74CECCE13F94A86CA9150B8C33ADC2F5C5B5981F98CB59D171490FEBE59C04C4` |
| SR `windoom.exe` | `B4AC4CFE15FDA894E5CE825DCA7488ACDDC0A2EFA905809026EEDB1EB61A7869` |
| `dxr_lighting.cso` | `980D86EAA533A98A064EB9B6B291012DD92858B10B8D6C569B14D6DE22568BE7` |

## 原始资源与场景矩阵

验收 IWAD 来自原始资源，SHA256 为
`a8772e088847032510d97ba2312406a6998f21cbab44d4ff10696faa9c0ecd4b`。
每个 fixture 仅修改 player 1 的 THINGS x/y/angle 六字节，未修改地图几何、
BSP 或纹理。20 个 WAD/demo 远端哈希与不可变本地 fixture 一致。

位置按地图坐标书写；light height 是世界高度。最终回放覆盖普通 alpha、
强制 opaque、候选调试、关闭阴影及覆盖层对照，另含预算耗尽、RT 关闭、
`-nort`、缺失 lighting shader 与 SR+RT。

| 场景 | 地图 | 相机 x,y,yaw | 光源 x,y,height |
| --- | --- | --- | --- |
| fence | MAP03 | 2576,-1160,180 | 2464,-1160,180 |
| west | MAP03 | 2416,-1160,0 | 2560,-1160,180 |
| window-back | MAP01 | 1088,752,90 | 1088,916,140 |
| rail-back | MAP03 | -720,96,180 | -856,80,32 或 80 |
| sky-back | MAP01 | 1088,1168,270 | 1088,1000,360 |
| unequal-sky-side | MAP01 | 48,-1768,0 | 256,-1793,200 或 120 |

## 验收结果

使用 GPU receiver/candidate readback，独立解码原始 WAD texture posts 检查
候选 UV 对应的 alpha，共 104,890 条检查、零差异。默认 64 步预算的所有
观测帧 ERROR 与 EXHAUSTED 均为零。以下数量取 `game_tic=175`；HIT/MISS
包含实体及开放边界，alpha 检查只计有效遮罩候选。

| 场景 | 射线 | 原始 alpha 检查 | HIT | MISS |
| --- | ---: | ---: | ---: | ---: |
| fence-alpha | 19096 | 2914 | 3146 | 15950 |
| west-alpha | 4776 | 569 | 309 | 4467 |
| window-back-alpha | 24751 | 18040 | 12568 | 12183 |
| rail-back-alpha | 25533 | 6853 | 7090 | 18443 |
| rail-back-above | 25544 | 7572 | 5240 | 20304 |
| sky-back-alpha | 33315 | 549 | 17040 | 16275 |
| unequal-sky-side-alpha | 20878 | 0 | 2824 | 18054 |
| unequal-sky-low-alpha | 20878 | 0 | 3299 | 17579 |

- rail height 80 对照中，line 189 的 5,740 个实际候选 V 超出纹理高度，
  全部 alpha=0、全部通过，证明遮罩不发生垂直重复。height 32 没有越界
  候选，因此不把该普通场景当成越界证据。
- unequal-sky low lamp height 120 位于相邻天空 ceiling -128 与 160 之间。
  729 条实际 MISS 地板 receiver 射线在这两个高度之间跨过原始 line 925，
  证明不同 sky 高度没有生成虚假上层封闭墙。
- budget 1 对照在 tic 175 有 2,729 个 EXHAUSTED receiver，全部按保守
  遮挡处理；它是主动失败注入，未混入默认预算成功统计。
- 31 组武器/UI 对照在每组 381,056 个覆盖层像素内零差异。
- RT 关闭与 `-nort`、缺失 lighting shader 各有 220 个共同游戏 tic，
  最终 PNG 哈希全部一致。缺失 shader 对照仍有 requested=1 日志；不声称
  它输出了日志中不存在的具体缺失原因。
- SR+RT 有 219 次官方 SDK/runtime 310.9.1 SR evaluation 成功、零失败。
  这是超分证据，不作为 DLSS5 / DLSSNR 的 NR 执行证据。

在 1280×800 最终引擎 PNG 上，alpha/opaque 的不同像素分别为 fence 19,360、
west 5,264、window 96,944、rail 61,152、rail above 88,096；sky/no-shadow
不同像素为 270,784，unequal-sky/no-shadow 为 41,824。候选调试图与最终
阴影 PNG 均保留在对应回放证据目录，不能仅以像素差数量判断物理正确性。

可移植 ASan/UBSan 检查覆盖原始遮罩 wall post 采样（含 opaque palette 0、
空洞、深度/法线、sprite 排除及 overlay 不变）、双侧 portal/UV/pegging/
sky 网格和最终 revision 合约，均通过。

## 测量及证据限制

每个性能样本取 tic 10..210 的 201 个导出帧。下表是 GPU timestamp 成本，
不是交互 FPS 保证；本票未声称覆盖所有地图、显卡或动态状态。

| 场景 | RT 中位 ms | RT p95 ms | GPU 中位 ms | 最大 local memory 字节 |
| --- | ---: | ---: | ---: | ---: |
| fence | 0.445920 | 0.456320 | 0.768160 | 112074752 |
| window | 0.443616 | 0.452320 | 0.766496 | 105877504 |
| rail | 0.414016 | 0.426272 | 0.736960 | 112074752 |
| sky | 0.429088 | 0.440672 | 0.750624 | 105877504 |
| unequal sky | 0.419904 | 0.430720 | 0.741920 | 105877504 |
| SR+fence | 0.444864 | 0.449248 | 1.122592 | 192528384 |

最终六份 client metadata 全部为 `captured=false`、`ownedForeground=false`，
因此不作为最终窗口显示证据。早期 pilot 的实际 client capture 为 1280×781，
只能作为早期 UI 定性观察，不能声称与最终源码的 1280×800 引擎导出逐像素一致。
这里的最终像素与 alpha 结论基于同游戏 tic 的引擎导出及真实 GPU readback，
不依赖桌面焦点。独立复核也检查了四组最终 PNG 与 receiver readback 的对应，
排除武器覆盖 receiver 后零差异。

验收材料保存在本次工作目录 `/home/awang/tmp/doom-implementation-g6ir9es3/`：
`alpha-result.md` 提供完整证据索引；`alpha-evidence-06/` 包含 34 次回放，
`alpha-evidence-07/` 包含三次低光源天空回放。主要汇总为
`alpha-final-analysis.json`，关闭 RT 对照为 `alpha-all-frame-off-comparison.json`，
源码、产物身份及原生编译日志分别为 `alpha-v5-source-hashes.json`、
`alpha-v5-build-hashes.json`、`alpha-v5-build-original.log` 与
`alpha-v5-build-sr.log`。这些外部证据不是仓库内长期可移植资源。
