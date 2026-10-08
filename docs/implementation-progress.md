# 实施进度与续做入口

状态：2026-10-07 已完成用户修订后的非 NR 实施、实际中文扩展及最终实机验收；最终运行源码为 `4fed93cb60e0cda102b0f67edfc7733e4c6fe77c`。七目录离线交付与准确源码、许可及摘要见最终验收记录。GitHub 凭据失效，当前提交仍仅本地保存，PR18 远端保持 draft，同步及远端 ready 状态尚未完成。NR 为用户豁免的未验证项。以下历史记录及红例保留，最新结论优先。

## 最终接受记录（2026-10-07，v6）

- fresh `build.cmd --all`：330 个 Git／archive／manifest／实际源输入一致，四次原生编译／链接，七目录 105 个部署文件；每目录带中文字体／OFL 与五个 DXR shader，五 DLL 当前签名 Valid、AMD64、310.9.1.0。2020 条既有 warnings，零 errors。
- v6 六非 NR 默认模式：6 个进程 exit 0、72 个 pairs／144 个 WM／42 张 capture；同静止 east-A fixture 的 tic70／175 十二张 PNG 字节与 RGB 与历史797源完全一致，不冒称 `-playdemo`。
- v6 实际 en／es／zh 消费者和缺／坏字体／长提示：6 个进程 exit 0、168 pairs＋3 holds／342 个匹配 WM／75 张 capture／6 次正常读档 reset；亮色中文 pickup、Options、RT＋SR、MY SAVE／F6／F9／退出、2＋2 行长理由与保存失败均实际可读，缺／坏字体整体英文。HUD 脸部／窗口圆角外差异 0。
- v6 实际 gamma／缩小视口 HUD 清除：3 进程 exit 0、39 pairs／78 WM／21 张 capture；F8 后等待至少300新帧再 F11，确认为 gamma1；顶部48行消息过期后的残字差异0，包含完整12px中文高度。
- 历史精确00e源最终 v5 held／replay 经根独立复核：门 Use、正常存读档、暂停、RT off 改世界再 on、MAP01→MAP02→MAP01，十条动态／透明／天空／屏外／传送回放；独立原 WAD alpha28,526检查0差异。v6 只改变中文墨色，原几何／RT源相同，旧 GPU 运行仍标 v5。
- Standards 0 硬性违反（此前3个非阻塞维护建议），Spec 0 已确认未解决源码缺陷，Security0 已确立可利用问题且保留审计范围限制。最终三轴增量与运行分别核验。
- v4字体staging遗漏、v5暗色CJK、旧gamma被F8消息遮盖、v6首次SearchHost焦点阻塞、最初pair-only世界试跑全部保留并排除对应通过结论。最终 owned tasks 精确移除，游戏正常退出，不动用户应用／系统设置。
- [最终验收矩阵](packaging-acceptance.md)覆盖故事1–51及#16／#17／中文扩展，NR-only行明确 WAIVED。七目录、准确源码ZIP、许可、关键帧、版本与SHA-256交付；无IWAD／TTC。GitHub本轮同步及PR18远端ready仍待凭据恢复，本地实现和运行不因此回退为未完成。

## 2026-10-07 最新范围修订

用户确认 RTX 电脑开着，要求逐步完成全部剩余工作，并明确“DLSS5不用验证了，其他继续”。保留原 DLSS5／NR 模式、独立请求及诚实的未验证状态；不再开展 NR／DLSS5 功能、consumer、checkbox 或 GPU 验证，原验收项由用户豁免而非 PASS，不再作为 #16／#17 完成阻塞。历史证据保留，SR／DLAA 仍不能替代 NR 证据。其他 RT／SR、菜单、打包、语言与真实画面验收继续，已有远端收尾／可关机记录是此前轮次状态，本次已获继续授权。

中文译文已完成，实际 CJK 显示作为初始首版之外的补充扩展继续实施；`-lang zh-cn` 须实际绘制包内汉字，保持界面边界及英文像素，坏／缺字体安全回退，随包交付字体与许可资源。详见 `docs/chinese-display-task.md`；不要求完整剧情、图片菜单汉化或 Unicode 输入。当前文档修订不表示字体已实现或菜单候选已合并。

## 已完成并合入集成分支

- 官方 NVIDIA DLSS SDK 固定 v310.9.1，SR DLL 文件版本 310.9.1.0；可信来源、摘要、签名与架构检查、事务部署、备份恢复、缓存修复和实际运行版本报告已完成。DLL 版本不代表 NR 已执行。
- RTX Windows 基线、NGX SR 运行、不可用及失败回退、相机采样、运动矢量、device-Z、历史重置、场景与武器／HUD／菜单分层已验证。
- 外部语言包、语言偏好、英文回退、安全模板，以及退出／选项／拾取／存档提示消费者已完成。UTF-8 解析与中文字形显示分别处理。
- 用户于 2026-10-07 要求提前制作中文翻译，替代原先“完整规格交付后再制作中文包”的安排。`gpt-6-luna` 已完成 `languages/zh-cn.ini` 与 `docs/chinese-language-pack.md`，批准提交 `2139fe927bb8b865db0e94d207dbd99dd34d6cb1`（包含 `29be97f`）已通过合并提交 `f096136cc3efdabd1deebc5663c7f09d3860328e` 纳入集成分支。静态核对为 93 个 strings 键：54 个基础键来自 `9f1a2e486b65df97ad7881e2e6209dcbc51bdcec`，39 个图形键来自 `6de9d1bd6000b2c0944dd94bbcf64a784b59ff63`；无重复、漏键、多余键或空值，存档模板各含一次 `{save_name}`，diff 检查通过。本次仅新增翻译与说明，CJK 字形和游戏内真实中文显示仍待后续实施与验收。
- DXR 硬件能力、最小 RayQuery、真实静态地图与复制材质已完成；真实地图合并 `9ccaebf`，时间深度修正合并 `cc0291f`。
- #12 点光源硬阴影已合并 `625fdaf`。25 次真实 RTX 回放正常退出，原调色板／gamma 的线性合成、普通及 SR 显示、HUD／武器叠加、缺 shader 回退已核对。详见 `docs/dxr-lighting.md`。
- #13 动态门／平台／地板及邻接墙更新已合并 `a3eaaf3`。精确源 `b67dba8` 的 13 次 RTX 回放均正常退出；门、平台及 MAP06 地板的可见阴影变化、同帧历史重置、真实存读档／换关路径已取得证据。等高 MID 激活修正 `d55562e` 已通过公开 CPU 契约及两种原生构建，已纳入 #14 的确切源码 GPU 验收，完整动态组合仍由 #17 复测；不能把 b67 证据改称 d555 GPU 证据。详见 `docs/dxr-dynamic-map.md`。

- #14 透明栅栏／窗洞、有限纹理范围和天空规则已合并 `60e9787`。实测源 `384c1ac` 包含 d555，37 次最终 RTX 回放正常退出；104,890 条原始 WAD alpha 采样零差异、31 组覆盖层对照零差异、关闭 RT／缺 shader 回退逐 tic 一致。最终失焦的窗口 capture 明确排除，颜色结论由 GPU readback 与同 tic 引擎 PNG 支持。详见 `docs/rt/masked-alpha-sky.md`。

- #15 指定材质的屏幕外单次反射已合并 `a3c699b`。实测源 `79055b0` 的 51 次 RTX 运行正常退出；精确几何法线、原始纹理／alpha／活动调色板、动态门／平台反射命中与 SR 组合已独立核对。203 个编译输入匹配，集成非文档源码与该版本一致。实机 client 图仅作外观证据，完整窗口 HUD 布局由 #16／#17 继续验收。详见 `docs/dxr-reflection.md`。

- #16 已按用户最新 NR 验证豁免范围接受，并通过 `a10e04824e21ab8db5768facc66b5ba2fa78adb5` 合并批准菜单 HEAD `9ec83e1cdbcd2aa9240abb3d11d544476fa5690d`。实测 runtime 源为 `9c762acf1a237d9e91ff089e29da1f87dd04626a`，其后为文档更新；native v5 的 316 个输入与 exe 身份已独立核对。根代理 `root-menu-final-rt-sr-data-review.json` 复核 12 条非 DLSS5-only 路线、267 个 paired actions、534 个 WM 事件、93 张 captures、46 次单项转换首应用帧 reset bit 512，以及 8 条同 tic 故障对照，全部记录哈希匹配。根代理实际查看 RT＋SR active 菜单、缺 DLL 后恢复 SR、缺 RT shader 后恢复 RT 菜单及固定 tic RT 画面，确认清晰且 HUD 完整。NR 原验证项由用户豁免，仍未验证，不标 PASS。详见 `docs/graphics-menu-validation.md`。
- #16 HUD 稳定性由 `root-menu-layout-v5-stable-baseline-review.json` 独立核对：以正常 post-close 图为基准，后续 layout 6 图与 warning 3 图的 face 外 HUD 差异为 0。初始 frame 39／34 均为 tic 1、scene 0，不能宣称最初差异为 0；其中 656 个输出像素为原武器栏首次／重复透明背景刷新差异，其余初始差异为 wipe 黑区。初始红例与后续稳定证据分别保留。
- 合并后按批准菜单 HEAD 核对其 292 个 tracked 非 docs 文件（排除 `AGENTS.md` 及中文包），blob 与模式全部一致；集成额外非 docs 文件仅 `languages/zh-cn.ini`，93 个中文译文及现有 NR 豁免规格／任务修订均保留。

## #16 实施历史与证据

#16 此前从 `a3c699b` 创建独立工作树，实现游戏菜单 RT、SR、DLSS5／DLSSNR 三个独立请求、偏好保存、真实状态、安全切换及失败回退；SR 失败须保留已合成 RT 画面。以下记录保留当时状态，不替代上面的最新接受结论。

- 菜单候选当时已完成原版和 NGX 两种 Windows 原生构建，独立请求、profile sidecar 和可选 NR 控制仍在独立工作树，尚未合并或宣称菜单通过。
- 首轮真实 SendInput 菜单验收失败。观察源 `36114a8` 的按键日志证实快速成对和保持按键的字母／Enter 都变成 `VK_PROCESSKEY`，30 个窗口事件中有 11 个此类按下事件，未产生图形请求；原始失败画面和日志保留。当时只调整游戏自身 HWND 的 IME 关联后，需重新构建及复测，不修改系统输入法设置。
- 实际窗口当时超出 workarea 并裁剪底部 HUD；引擎导出包含完整 HUD。窗口 DPI／尺寸修复与输入修复分别验证，不能将旧截图当完整窗口通过证据。
- 此前一轮 owner 报告：精确源 `9c762ac` 的 13 次菜单矩阵运行均正常退出，8 个故障对照与 2 次窗口布局运行已完成；游戏 HWND 的 IME 输入修复与窗口修复已有分别取得的证据。当时结果已下载 Linux，Windows 测试轮次已收尾，用户可以关机，该轮不再启动远端。当时根代理完整复核与菜单合并仍待，未标记最终 PASS；以上首轮失败日志和裁剪画面仍保留。

## #17 与中文显示当前集成进展

2026-10-07 已顺序合并打包候选 `319e0c598db9b897e55c35481e2051e1936c8dde` 和完整 CJK 候选 `73d348c0cc8b97eb21e861e38118eaa994a22c2e`，合并后的 runtime 源为 `e24ba88671ea16acf22ed5616ff6245b8e98758b`。329 个 tracked 文件与已批准候选的 blob／mode union 完全一致；原七模式表、93 键中文译文、NR 验证豁免及规格／任务修订均保留。该源正在做最终 fresh Windows 构建及实机验收，不能将此前构建证据重新标为此版本。

- 打包源码更新补齐西语图形键、修订实际 RT／菜单使用说明；状态标题由会安全截短的 `Estado real` 改为 `Real`。历史源 `79785860` 的 fresh `build.cmd --all` 已构建七目录，321 输入和 91 资源条目已独立核对；实际启动仅六个非 DLSS5 模式，默认、Anime4K 输出、FSR2／RR／SR 身份分别核对。构建有既有警告，不声明 warning-free。
- 根代理直接复核语言／偏好七进程：119 个 SendInput pairs、238 个按序匹配 WM 事件、34 张可用 owned client captures、12 次转换首应用帧 reset512 且 history invalid；退出重启、CLI 覆盖及单键保存优先级正确。最初两张同名 capture 被后续覆盖，明确排除，不用作画面证据。旧长状态标题的截图不作为新短标题证明。复核材料：`root-packaging-language-v1-data-review.json`。
- 源 `319e0c5` 的失败／恢复八进程：112 pairs、224 WM、42 captures、12 次同帧 reset512；四种 synthetic SR API 失败和缺 DLL 的十张 tic70／175 PNG 与 native RT 基准字节及全 RGB 完全相同，RT 反射仍执行。真实 DLL／shader 恢复后 RT／SR active，WARP 显示 no_dxr 且游戏可继续。synthetic 结果不称真实硬件故障，WARP 不称硬件性能。复核材料：`root-packaging-failures-v1-data-review.json`；两组 normalized 的 479 文件与原始 ZIP 全部相同。
- 用户指定 `gpt-6-luna` 的中文消费者及字体已合并：289 个非 ASCII 码点、12×12 稀疏位图、完整 OFL 许可、严格格式／CRC／覆盖校验和整体英文回退；英文保持原字体路径。根独立运行 font loader、实际 menu／pickup／HUD／prompt consumer、实际 blitter 和 fresh-process startup 测试均通过 ASan／UBSan。HUD 12px 擦除缺口已修复并新增清空后完整擦除的断言。字体 SHA-256 为 `85723080bc39beab24796438f56c3a924ac5ebe4727b05a07e1db1300b96cfb3`。复核材料：`root-cjk-final-source-review.json`。以上 CPU／源码证据不替代实际中文可读性证据。

## 最终源码、原生构建与审查

最终 runtime 源更新为 `00e0d5a65389a8e4ebbe6900bbfb9bc0447dc692`。原 `e24ba886` 的 Windows v4 编译完成，但资源门槛发现 `build.cmd` 只复制 INI，漏 `zh-cn.cjk`／OFL；该轮未启动游戏，保留 `packaging-17-build-result-v4-red.json` 与编译日志。已合并最小 staging 修复 `a47aa0a`，以及当前 CJK 部署说明 `251a7f3`，CMake 原有目录复制无需修改。

规格审查发现共享材质 Create／Map 的暂时失败缓存不会因 RT off／on 清除。修复 `fce03e7` 和诊断 ABI／测试补充 `d7ec73f` 只在效果实际 false→true 时清除失败签名，不释放借用资源，也不在每帧重试。根代理实际运行公开 `RtMaterials` API 的 diagnostics OFF／ON ASan／UBSan 回归：分配／初始 Map／metadata Map 失败抑制、相同场景明确重试、健康资源保留、单次故障生命周期及安全收尾通过。实际键盘菜单的两种效果路线恢复仍待实机证据，不能仅凭测试关闭该运行验收项。

精确源 `00e0d5a6` 的 fresh Windows v5 已通过：330 个 Git／archive／manifest 及实际构建输入完全匹配；四次实际编译／链接包含全部改动的消费者和 RT 单元；七目录 105 部署条目均带 8128-byte CJK 字体及完整 OFL。五个 SR／RR DLL 条目的哈希匹配此前固定 x64、Valid Authenticode 回执，当前 PE 文件版本规范化为 `310.9.1.0`；RR DLL 仅在 RR 目录。编译日志有 2020 条 warning diagnostics，无 error diagnostics，不声明 warning-free。独立记录为 `root-packaging-v5-source-review.json`、`root-packaging-native-v5-review.json`。

三个审查方向已固定 `00e0d5a6`：Standards 0 硬性违反、3 项无实际缺陷的维护建议；Spec 原 P2 已修，0 已确认未解决源码问题；Security 五类检查未发现可证实的可利用问题。报告位于协调目录 `final-standards-review-00e0d5a.md`、`final-spec-review-00e0d5a.md`、`final-security-review-f333560.md`（正文固定最新源）。安全结论限定源码和本地依赖一致性，未查询漏洞公告，不是完整安全审计；三份报告均不替代最终运行验收。

## 最终 v6 前的实施计划（历史，已完成）

1. #17 owner 独占 RTX 桌面，使用已通过 fresh 构建的精确 runtime 源 `00e0d5a6`，实际启动六个非 DLSS5 模式；检查英文相同 demo 的 12 张逐像素对照及实际菜单恢复。
2. 真实键盘中文／西语消费者、缺／坏字体整体回退、长理由与偏好 warning、同进程存读档／换关／传送、RT off 改世界再 on，以及动态／alpha／sky／屏外反射关键帧仍待最终集成实测。DLSS5／NR 验证继续按用户豁免执行。
3. 所有剩余验收完成后核对审查源与最终产物，若运行发现新缺陷，则修复、重建并复审增量；交付七目录离线包、许可、版本／哈希及逐项证据。PR18 保持 draft，GitHub 凭据失效导致新提交仍只本地保存，不提前声明最终实机通过。

## 兼容与状态边界

保留用户提供的原目录、启动参数和默认效果。`windoom-ngx-dlss5 --ngx-dlss5` 仍先执行 preset-L SR，再由已安装的原 Swapper 路线执行可选 NR；没有安装第三方 Swapper 或系统注入组件。

DLSS5／DLSSNR 是神经渲染增强，分别于 SR、DLAA、RR 记录身份和实际证据。菜单分别保存 RT／SR／NR 请求；后端组合限制以具体实现及实测为准。carrier 成功、checkbox 回读与真实 NR GPU 执行是不同证据，当前不能声称已验证 NR 执行。

本轮依据用户最新修订停止 NR／DLSS5 验证，包括 consumer、checkbox 和 GPU；上述证据区分与未验证限制继续保留，豁免不转为通过结论。

## 保存与协调入口

- 仓库：`awangs1986/DOOM-DLSS-5`；分支：`feat/dlss-rt-language-spec`。
- 本文更新前集成 HEAD：`f096136cc3efdabd1deebc5663c7f09d3860328e`，源码及合并记录已保存在本地。本次更新前的进度文档备份：`/home/awang/tmp/doom-implementation-g6ir9es3/implementation-progress-before-translation-20261007-nFFSyt.md`。
- Draft PR：https://github.com/awangs1986/DOOM-DLSS-5/pull/18 。GitHub 认证此前曾恢复，#1／#16／#17 菜单与 NR 修订已同步；2026-10-07 本次推送返回 `Invalid username or token`，当前凭据再次失效。`gh` wrapper 的认证状态及移除 `GH_TOKEN` 后 `local/bin/gh` 的保存认证均确认 invalid；本次未登录、注销或重复推送。
- 本次中文翻译合并 `f096136cc3efdabd1deebc5663c7f09d3860328e` 与进度记录 `e930b7ae7d230352ad8c4776a306e90cd9b29d9c` 已保存在本地，尚未推送。本次凭据失效记录更新前的进度文档备份：`/home/awang/tmp/doom-implementation-g6ir9es3/implementation-progress-before-auth-failure-20261007-VAKOlb.md`。
- 本次范围修订前 HEAD：`f107cd0d7f627742949667f5cec503d493b8fdab`；规格、#16／#17 任务及进度原文已备份到 `/home/awang/tmp/doom-implementation-g6ir9es3/docs-before-scope-revision-20261007-FvAoho/`。本次 NR 验证豁免及实际中文显示补充任务先本地保存，GitHub 凭据仍失效，待同步。
- 本次菜单接受记录更新前集成 HEAD：`a10e04824e21ab8db5768facc66b5ba2fa78adb5`；进度原文备份：`/home/awang/tmp/doom-implementation-g6ir9es3/implementation-progress-before-menu-merge-20261007-H5vCco.md`。菜单合并及本次进度记录仍仅本地保存，未推送。
- 规格：`docs/dlss-rt-language-spec.md`；菜单任务：`docs/tasks/16-rt-sr-menu.md`、`docs/tasks/17-packaging-menu-acceptance.md`。
- 协调与完整证据：`/home/awang/tmp/doom-implementation-g6ir9es3/`。
- 当前 #15 交付入口：`reflection-result.md`、`root-reflection-v6-all51-ledger-review.json` 及 main／extra／dynamic／source／native 独立复核；#13 交付入口：`dynamic-result.md`、`root-dynamic-matrix03-review.json`、`root-dynamic-followup-review-20261007.json`；#16／#17 准备入口：`menu-frontier-implementation-notes.md`、`existing-swapper-menu-plan.md`。
- 任务保留至完整 PR 验收；各工作树和隔离证据未清理。

## 远程执行约束与限制

只使用用户确认的 Windows RTX 电脑（192.168.100.164:22，用户 awang，runner ID c2837d6a-bba0-4fc7-b09e-ede11b0e0ee3）。每次操作经 `rtx-runner.py` 重新核对 SSHME 对话配置，使用 PowerShell。通用 runners 配置的另一个目标不是本次授权机器。SSH Session0 不能创建游戏 swapchain，交互桌面一次只由一个代理使用，临时游戏／任务按所属范围收尾。

动态几何每次变化仍有约 4–5ms CPU 场景提取成本；未承诺帧率。反射首版使用保守全局历史 reset，仍有收敛／闪烁代价。生命周期外部截图若失焦或裁剪不完整，不作为最终画面通过证据；场景颜色优先核对 engine-export PNG 与 GPU readback。最终集成验收和完整规格交付仍在进行。
