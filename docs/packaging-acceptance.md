# 最终打包与实际中文验收

验收日期：2026-10-07。最终运行源码为 `4fed93cb60e0cda102b0f67edfc7733e4c6fe77c`（v6）；后续文档提交不改变运行源码身份。#16、#17 与实际中文显示补充任务按用户修订后的范围完成。DLSS5／NR 的模式及独立请求保留，功能、consumer、checkbox、组合与 GPU 验证为 **WAIVED（用户豁免，未验证）**，绝不记 PASS。GitHub 凭据失效，当前提交及验收说明保存在本地；PR18 远端仍为 draft，尚未同步本轮更新。

## 验收矩阵

下表的 PASS 只覆盖本行明确列出的范围。原规格故事编号沿用 [User Stories](dlss-rt-language-spec.md#user-stories)。

| 故事／任务 | 结果 | 实际证据与范围 |
| --- | --- | --- |
| 1–11：官方库、版本、备份、部署、固定依赖 | PASS | 既有 #2 事务升级／损坏缓存／失败保留／备份恢复；v6 七目录原生部署与当前 DLL 签名、AMD64、摘要和 310.9.1.0 版本核验。SDK v310.9.1 固定 commit `374959484e79a640feaba44c93ac8cfb0a03f5b5`。 |
| 12–17：SR 开关、回退、相机输入、历史及叠加 | PASS | 既有 #4／#5／#9 与 #16 四种真实键盘 RT／SR 组合；最终同进程读档、换关和传送，v6 完整客户区、英文对照及消费者画面。 |
| 18–27：点光阴影、材质反射、动态门／平台／地板、透明与天空 | PASS | #12–#15 原始组件证据保留身份；集成 v5（源码 `00e0d5a6`）十条最终回放、实际门 Use／读档／换关；原始 WAD alpha 独立重算 28,526 次零差异，屏外 A／B 13,440 个最终像素变化。v6 只改变 CJK 墨色，几何／材质／shader／RT 消费者源码与 v5 相同，不把旧运行重标成 v6。 |
| 28–30、45–46、48–50：RT／SR 菜单、偏好、能力及失败 | PASS（RT／SR） | #16 真实键盘、退出重启、CLI 覆盖、单字段保存；#17 缺 DLL／shader 恢复、WARP 能力不可用、四种明确标记的 synthetic SR API 失败后保留 RT 同 tic 颜色；v5 材质 Create／Map 暂时失败的显式 off→on 两条真实菜单恢复。NR 子句见 WAIVED 行。 |
| 31：耗时与显存 | PASS（观察记录） | 固定回放的 GPU 阶段 timestamp／本地显存与原始 CSV，见下表。不承诺交互 FPS 或跨设备结果。 |
| 32–39、51：语言选择／保存、缺键／坏包、消费者、边界 | PASS（非 NR） | 历史七进程语言／偏好与坏包／缺键路线；v6 en／es-ascii／zh-cn 三种真实消费者及独立缺／坏字体、真实未编译 SR 原因、偏好保存失败的中文长提示。字体 289 个非 ASCII 码点、12px、OFL 离线部署；图片菜单、剧情及 Unicode 输入仍未迁移。 |
| 40：同时间线比较 | PASS | 原组件固定 demo 回放与最终 v5 对照；v6 六模式使用同一个不可变 east-A 静止启动 fixture，在 game_tic 70／175 的 12 张 PNG 字节与 RGB 均与历史 `79785860` 相同。此十二图不是 `-playdemo` 运行，不混用两种证据。 |
| 41–42：真实硬件与构建／桌面分别验证 | PASS | RTX 5070 Ti Laptop／驱动 32.0.16.1714；fresh 原生 build.cmd --all 与 Interactive／Limited Session 1 真实菜单、焦点及全窗口记录分别提供。 |
| 43：官方功能与外部扩展分开报告 | PASS（身份及证据区分）；NR WAIVED | SR、RR 的 synthetic 输入、DLAA／载体和 NR 始终分别报告；其他功能成功不代替 NR GPU 执行。 |
| 44、#17：里程碑、七模式离线产物、说明与许可 | PASS | 七目录、精确源码 ZIP、版本与文件 SHA-256、GPL／NGX／AMD FSR2／Anime4K／OFL 及所用头文件许可；无 IWAD、TTC 或系统字体依赖。各阶段来源、失败和限制明确保留。 |
| 47 及所有 NR／DLSS5 实测子句 | WAIVED | 用户 2026-10-07 明确“DLSS5不用验证了，其他继续”。未启动 DLSS5 模式、未进行 NR consumer／checkbox／GPU 验证，未验证状态与原模式保留。 |
| 实际中文显示补充任务全部八项 | PASS | v6 真实 pickup／Options／RT+SR／F6／F9／MY SAVE 混排／退出 N 和 Y／缩小视口清除；缺／坏字体整体可读英文；实际 gamma 及 HUD 12px 清除，详见下文。 |

## v6 原生与实际游戏

从准确 Git archive 构建：330 个 tracked 输入逐一匹配 archive／manifest／Windows 实际源码；四次独立原生编译／链接，七个模式目录共 105 个部署文件，每目录带中文 INI、8128-byte CJK 字体、完整 OFL、五个 DXR shader、默认光源与材质。CMake 4.4.0、Visual Studio 17 2022 x64、Windows SDK 10.0.26100.0／DXC。构建日志包含 2020 条既有 warning diagnostics，零 error diagnostics；不宣称无警告。

五个 NVIDIA DLL 条目均为当前 `Valid` Authenticode、AMD64 和文件版本 310.9.1.0；SR SHA-256 为 `3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983`，RR 为 `4bc7ea5fcb2f32cf86bc2cb072e8d2860914a0be511cbdcb2fc206c1d9d83b80`。RR DLL 只在 dlss3.5 目录。离线字体 SHA-256 为 `85723080bc39beab24796438f56c3a924ac5ebe4727b05a07e1db1300b96cfb3`。

| 实际启动模式 | 当前观察 |
| --- | --- |
| original | 普通显示，RT 默认关闭，官方 SR 关闭 |
| anime4k | Fast Mode C 的 320→640→1280，官方 SR 关闭 |
| fsr2 | FSR2 2.2.1，320×200→1280×800，官方 SR 关闭；不把 FSR2 当 DLSS SR |
| ngx-dlss3.5 | RR 使用 synthetic material inputs，菜单诚实显示 SR fallback；不声明正式 RR 材质集成 |
| ngx-dlss4 | 官方 SR active，模式文件 k |
| ngx-dlss4.5 | 官方 SR active，模式文件 l |
| ngx-dlss5 | 仅静态构建／部署，模式文件 dlss5，运行验收 WAIVED |

六个默认模式的真实菜单共有 72 个 SendInput pairs、144 个匹配 WM 事件和 42 张焦点有效 capture。三种语言／缺字体／坏字体／长提示的六个消费者进程共有 168 pairs、3 次实际保持按键、342 个匹配 WM/SYSKEY 事件、75 张 capture、6 次正常读档历史 reset；全部通过菜单 Quit＋Y，exit 0。N 只用于确认框取消，未在图形菜单切 NR。

所有 capture 的客户区与引擎帧均为 1280×800，客户区完整且在 workarea 内，至少等待三张新完成导出帧。75 张消费者图的脸部与操作系统圆角之外的 HUD 差异均为 0；六模式稳定世界／返回图也为 0。FSR2 初次 face 有 1024 个异步像素差、窗口圆角每图 112 个差异分别保留，不宣称全客户区每一像素同时一致。

CJK 实际亮红字形采用原 A patch 中亮度最高的 opaque 调色板色；不会更改 ASCII patch 或整块透明区域的叠加标记。已查看真实拾取“拾取了护甲”、Options 消息状态、图形请求／实际状态、MY SAVE 混排、快速保存／读档／退出提示。长中文真实 `not_compiled` 理由两行及真实偏好 `save_failed` 两行均在 HUD 之前安全显示／截断，返回世界没有残字。独立缺／坏字体进程记录诊断并整体回退原英文文本／字形。

gamma 的三种语言各等待前一 F8 优先消息至少 300 张新帧后再按 F11；截图确实显示 gamma 1。额外 gamma／HUD 三进程共 39 pairs 和 21 张 capture，正常 Quit exit 0。缩小视口后 F8 顶部消息改变 7,712（en）、14,352（es）及 2,960（zh）个输出像素；消息过期后的顶部 48 行与消息前逐像素相同，残留为 0，覆盖中文 12px 全高度。

## 保留的集成场景与性能观察

v5 准确源码 `00e0d5a65389a8e4ebbe6900bbfb9bc0447dc692` 的实际 held-key 两进程共有 44 pairs、7 holds、20 captures。门打开、正常存／读档两次、暂停及恢复、门反向关闭、RT 关闭期间改变世界、重新启用后使用当前几何，以及原地图 MAP01→MAP02→MAP01 均经根代理独立复核。最初仅 pair 的移动试跑不是最终世界操作通过依据。

十条 v5 回放覆盖原地图门／平台／MAP06 地板、栅栏／窗洞／有限 V／天空和屏外反射、真实 teleport special 97 契约。special97 使用明确的矩形 synthetic fixture，不当作原地图物理几何证明。移动几何同帧 SR reset 分别地板 30 帧、门 24 帧、平台 34 帧；静态几何不执行 AS 工作。alpha 使用原始 WAD posts 独立重算 28,526 次零差异；屏外 A／B 各 10,296 行关键字段与历史实测相同，各有 1,128 个目标命中。

以下为 v5 固定 game_tic 70–175 期间 106 个采样的中位毫秒和该路线观察到的最大本地显存；包含导出成本，只描述本次设备和 fixture。

| 路线 | 总 GPU ms | RT ms | SR ms | 反射 trace ms | 反射 filter ms | 本地显存 MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| MAP06 地板 RT＋SR | 1.856 | 0.414 | 0.364 | 0.066 | 0.683 | 189.70 |
| 门 RT＋SR | 1.878 | 0.424 | 0.364 | 0.079 | 0.682 | 194.93 |
| 平台 RT＋SR | 1.887 | 0.411 | 0.365 | 0.093 | 0.682 | 194.93 |
| 透明栅栏 RT | 1.525 | 0.448 | 关闭 | 0.065 | 0.682 | 124.12 |
| 屏外反射 A | 1.074 | 阴影关闭 | 关闭 | 0.065 | 0.681 | 95.67 |

## 证据、审查与失败排除

完整原始证据、normalized 文件和复核材料保存于协调目录 `/home/awang/tmp/doom-implementation-g6ir9es3/`。交付包另带摘要、关键帧、JSON 复核及离线说明；原始 CSV／全部 captures 保存在该目录，不将摘要代替原数据。

- v6：`packaging-17-build-result-v6.json`、`packaging-17-build-v6-all.log`、`packaging-17-native-extra-v6.json`；`root-packaging-native-v6-review.json`。
- 默认六模式：`packaging-17-evidence-final-default-v6.zip`、`root-packaging-defaults-v6-review.json`、`packaging-17-final-English-v6-comparison.json`。
- 实际语言／中文：`packaging-17-evidence-final-consumer-v6-r2.zip`、`root-packaging-consumer-v6-review.json`、`packaging-17-final-consumer-v6-HUD-review.json`。
- gamma／完整 HUD 清除：`packaging-17-evidence-final-gamma-v6.zip`、`packaging-17-final-gamma-v6-analysis.json`、`root-packaging-gamma-v6-review.json`。
- v5 世界／场景：`packaging-17-evidence-final-lifecycle-held-v5.zip`、`packaging-17-evidence-final-replay-v5.zip`；`root-packaging-lifecycle-replay-v5-review.json`、`root-packaging-original-post-alpha-v5-review.json`、`root-packaging-geometry-offscreen-v5-review.json`。
- 材质显式恢复：`root-material-retry-source-review.json`、`root-material-retry-v5-reflection-review.json` 及 lighting-only 对照。
- 三轴源码审查：`root-final-review-4fed93c.md` 及其引用的完整／增量报告，结果见 [最终源码审查](final-source-review.md)。

v4 staging 漏字体/OFL 为保留的红例，未启动游戏。v5 暗红 CJK（RGB 92,1,1）不作为最终中文可读性通过；v5 `gamma-feedback` 实际是前一 F8 消息，明确排除。v6 首次 SearchHost 前台阻塞在任何按键前安全停止，保留失败证据；用户将桌面留空后使用新的 r2 输出，未覆盖原尝试。所有最终一次性 owned task 已按精确身份移除，游戏正常退出；没有终止未知进程／服务、改系统 DPI／IME／安全设置、安装或注入第三方组件。

仓库未找到 `feedback-loops.md`，此次依据现有 `build.cmd`、菜单／语言／打包文档和已授权实机接受路径执行。后续建议使用 `setup-feedback-loops` 固定这些入口；本次没有假定缺失的规则或把该建议作为交付阻塞。

## 使用与限制

菜单、配置优先级、资源路径和备份恢复见 [打包使用说明](packaged-usage.md)。包不包含 IWAD；请传入合法游戏资源的绝对路径。语言重启生效，字体离线随包，无需系统安装。中文只覆盖当前翻译的 289 个非 ASCII 码点，含文字图片菜单、剧情、任意 Unicode 输入和运行时热切换仍非目标。原七模式表、默认行为及 DLSS5／NR 模式身份保持原约定；NR 为用户豁免的未验证功能。
