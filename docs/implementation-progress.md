# 实施进度与续做入口

状态：用户于 2026-10-06 请求保存进度并暂停。下次明确说继续后再开始实现、合并或远程验证。

## 已完成并合入集成分支

- 官方 NVIDIA DLSS SDK 固定 v310.9.1，SR DLL 文件版本 310.9.1.0；可信来源、摘要、签名与架构检查、备份恢复、缓存修复和运行版本报告已完成。不能从 DLL 版本推断 DLSS 5 全部功能。
- RTX Windows 基线回放、真实 NGX 运行和软件适配器回退已验证。
- 相机采样、运动矢量、历史重置、场景与武器／HUD／菜单分层已完成。
- 外部语言包、保存语言偏好、英文回退、安全模板，以及退出／选项／拾取／存档提示消费者已完成。首版不包含中文字体。
- DXR 硬件能力、最小 BLAS/TLAS、inline RayQuery、诊断显示与失败回退已完成。

## 已实现但尚未合入

- 真实静态地图（#11）：已提交 `00a6f0f73efac82c726abee917b3009be8905d3f`；工作树 `map-scene` 干净。原始 Freedoom MAP01 4600 三角形；10 项实机矩阵均正常退出。深度／法线总体对齐，差异集中在软件光栅边界；屏幕外实际地图命中及两次读档、MAP01→MAP02→MAP01 生命周期已验证。
- 时间超分深度修正：独立工作树 `temporal-depth`，为 SR／DLAA／FSR 提供 near0/far1 的 device-Z；保留光追所需原始深度，旧 RR 使用单独 linear depth。19 项公开数值检查及三种 Windows 构建已通过；实机 5 项回放及 758 帧交互验证均已正常退出（SR／FSR 各执行 286 次）；已保存证据，尚待独立下载核对、图像检查和提交。可选插件未加载时不能宣称 SR+DLAA 堆叠路径已经实测。

## 仍需完成

1. 核对并合入静态地图与深度修正的确切已测提交。
2. #12：点光源硬阴影、真实未照明 albedo、线性色彩合成及共享材质／纹理基础。
3. #13–#15：动态门／平台／地板与邻接墙更新、透明栅栏／天空、屏幕外单次反射。
4. #16：游戏菜单内光追、超分、DLSS5 三个开关，偏好保存、真实状态、组合显示与失败回退（SR 失败须保留已合成 RT 画面）。
5. #17：干净打包、图形菜单及状态／失败原因语言键、完整 RTX 组合与真实菜单操作验收和最终审查。
6. 恢复 GitHub 认证后推送最终分支、更新 PR 并转为 ready；当前 PR 保持 draft。

## 恢复上下文

- 仓库：`awangs1986/DOOM-DLSS-5`
- 集成分支：`feat/dlss-rt-language-spec`
- 已合入功能的 HEAD（进度文档提交前）：`1d2d323d333a8a2b7664db449ff1fefc14a2c7fa`
- 最后成功推送：`504589b`；之后的提交保存在本地。
- Draft PR：https://github.com/awangs1986/DOOM-DLSS-5/pull/18
- 规格：`docs/dlss-rt-language-spec.md`
- 菜单开关修订任务：`docs/tasks/16-rt-sr-menu.md`、`docs/tasks/17-packaging-menu-acceptance.md`；对应 GitHub #16／#17，目前待认证恢复后同步。
- 全部任务、研究、回放证据、工作树及续做检查点：`/home/awang/tmp/doom-implementation-g6ir9es3/`
- 先读 `pause-checkpoint.md`、`current-checkpoint.md`、`gpu-pause-checkpoint.md`、`temporal-depth-pause-checkpoint.md`；再读 `integration-acceptance-plan.md`。
- 所有工作树和远程隔离证据目录保留；没有清理源码或用户数据。
- 所有实施代理已停止；本次远程游戏与临时计划任务已结束，清理核对为 0 个进程／0 个任务。

## 远程执行约束

只使用用户确认的 Windows RTX 电脑（192.168.100.164:22，用户 awang，runner ID c2837d6a-bba0-4fc7-b09e-ede11b0e0ee3）。每次操作重新核对 SSHME 对话 runner 配置，通过协调目录的 `rtx-runner.py` 执行 PowerShell；不在 Linux 主机代替 GPU 实测。SSH Session0 不能创建游戏 swapchain；交互验证使用已授权的临时任务并收尾清理。凭据不写入进度文档。

## 当前限制

- 完整规格尚未交付，阴影／反射与最终组合验收仍待实施。
- GitHub 认证已失效；未完成的推送及 PR 更新不影响本地保存。用户恢复工作后可在工作台重新连接 GitHub 账户。
- 反射无法仅用主表面运动矢量可靠重投影；已确定首版保守全局 reset 策略及收敛／闪烁代价，不能使用现代 DLSS 不支持的 bias mask 作保证。
