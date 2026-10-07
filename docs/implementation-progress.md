# 实施进度与续做入口

状态：2026-10-07 用户已恢复实施，完整规格尚未交付，PR18 保持 draft。历史暂停记录保留；当前协调入口为外部 `root-progress-20261007-next.md` 及其后续记录。

## 已完成并合入集成分支

- 官方 NVIDIA DLSS SDK 固定 v310.9.1，SR DLL 文件版本 310.9.1.0；可信来源、摘要、签名与架构检查、事务部署、备份恢复、缓存修复和实际运行版本报告已完成。DLL 版本不代表 NR 已执行。
- RTX Windows 基线、NGX SR 运行、不可用及失败回退、相机采样、运动矢量、device-Z、历史重置、场景与武器／HUD／菜单分层已验证。
- 外部语言包、语言偏好、英文回退、安全模板，以及退出／选项／拾取／存档提示消费者已完成。UTF-8 解析与中文字形显示分别处理。
- DXR 硬件能力、最小 RayQuery、真实静态地图与复制材质已完成；真实地图合并 `9ccaebf`，时间深度修正合并 `cc0291f`。
- #12 点光源硬阴影已合并 `625fdaf`。25 次真实 RTX 回放正常退出，原调色板／gamma 的线性合成、普通及 SR 显示、HUD／武器叠加、缺 shader 回退已核对。详见 `docs/dxr-lighting.md`。
- #13 动态门／平台／地板及邻接墙更新已合并 `a3eaaf3`。精确源 `b67dba8` 的 13 次 RTX 回放均正常退出；门、平台及 MAP06 地板的可见阴影变化、同帧历史重置、真实存读档／换关路径已取得证据。等高 MID 激活修正 `d55562e` 已通过公开 CPU 契约及两种原生构建，仍须纳入后续集成 GPU 验收；不能把 b67 证据改称 d555 GPU 证据。详见 `docs/dxr-dynamic-map.md`。

## 正在实施

1. #14：透明栅栏／窗洞、有限纹理范围与天空规则；已完成探索性 GPU 校准，正在包含 d555 的确切源码上做最终验收。
2. #15：指定材质的屏幕外单次反射；普通和 SR 原生构建已通过，准备真实 GPU 反射／材质／alpha／sky／调色板矩阵。编译成功不是画面验收。
3. #16：前三票完成后实现游戏菜单 RT、SR、DLSS5／DLSSNR 三个独立请求、偏好保存、真实状态、安全切换及失败回退；SR 失败须保留已合成 RT 画面。
4. #17：干净打包、图形菜单语言键、真实键盘菜单操作、完整 RTX 组合及最终独立验收与审查。
5. 完整规格交付后，再按用户要求调用 `gpt-6-luna` 子代理制作中文语言包及实际 CJK 显示支持。

## 兼容与状态边界

保留用户提供的原目录、启动参数和默认效果。`windoom-ngx-dlss5 --ngx-dlss5` 仍先执行 preset-L SR，再由已安装的原 Swapper 路线执行可选 NR；没有安装第三方 Swapper 或系统注入组件。

DLSS5／DLSSNR 是神经渲染增强，分别于 SR、DLAA、RR 记录身份和实际证据。菜单分别保存 RT／SR／NR 请求；后端组合限制以具体实现及实测为准。carrier 成功、checkbox 回读与真实 NR GPU 执行是不同证据，当前不能声称已验证 NR 执行。

## 保存与协调入口

- 仓库：`awangs1986/DOOM-DLSS-5`；分支：`feat/dlss-rt-language-spec`。
- 本文更新前集成 HEAD：`a3eaaf37cc9bb36d727fc23dbeb902b613affa84`，源码及合并记录已保存在本地。
- Draft PR：https://github.com/awangs1986/DOOM-DLSS-5/pull/18 。GitHub 认证已恢复；#1／#16／#17 菜单与 NR 修订已同步。此前认证失效记录是历史状态。
- 规格：`docs/dlss-rt-language-spec.md`；菜单任务：`docs/tasks/16-rt-sr-menu.md`、`docs/tasks/17-packaging-menu-acceptance.md`。
- 协调与完整证据：`/home/awang/tmp/doom-implementation-g6ir9es3/`。
- 当前 #13 交付入口：`dynamic-result.md`、`root-dynamic-matrix03-review.json`、`root-dynamic-followup-review-20261007.json`；#16／#17 准备入口：`menu-frontier-implementation-notes.md`、`existing-swapper-menu-plan.md`。
- 任务保留至完整 PR 验收；各工作树和隔离证据未清理。

## 远程执行约束与限制

只使用用户确认的 Windows RTX 电脑（192.168.100.164:22，用户 awang，runner ID c2837d6a-bba0-4fc7-b09e-ede11b0e0ee3）。每次操作经 `rtx-runner.py` 重新核对 SSHME 对话配置，使用 PowerShell。通用 runners 配置的另一个目标不是本次授权机器。SSH Session0 不能创建游戏 swapchain，交互桌面一次只由一个代理使用，临时游戏／任务按所属范围收尾。

动态几何每次变化仍有约 4–5ms CPU 场景提取成本；未承诺帧率。反射首版使用保守全局历史 reset，仍有收敛／闪烁代价。生命周期外部截图若失焦或裁剪不完整，不作为最终画面通过证据；场景颜色优先核对 engine-export PNG 与 GPU readback。最终集成验收和完整规格交付仍在进行。
