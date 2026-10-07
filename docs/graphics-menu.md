# 图形菜单和偏好

入口：选项 → Graphics。保留原菜单的上下选择、左右／Enter 切换、Backspace 返回上一级、Escape 关闭菜单，以及原有控制器输入。三个开关分别保存请求：光追（RT）、官方 DLSS Super Resolution（SR）、项目 DLSS5／DLSSNR 神经渲染增强（NR）。某个开关不可用时，不清除请求或其他开关的选择。

## 请求和实际状态

请求的 ON／OFF 与实际状态分开显示。实际状态由本帧使用的后端和执行结果决定：关闭、等待应用、启用、不可用、回退、执行未验证、实际未知、非正常场景暂缓。选中某行时，下方显示该功能的状态及原因；底部独立显示偏好保存错误。

RT 总开关请求硬阴影和已配置的材质反射，保留 `-rt-light-off`、`-rt-reflections-off`、`-rt-shadows-off` 的原子效果覆盖。诊断视图不代替普通游戏光追。原版／Anime4K／FSR2 未编译 NGX，SR 请求仍可保存，但不会凭请求宣称官方 SR 可用；这些模式原有显示默认保持不变。既有 RR 模式按遗留 Ray Reconstruction 单独标识，不算 NR。

NR 的消费者加载、控制支持、原 checkbox 回读、DLAA carrier 执行和实际 NR GPU 执行是不同事实。模块或 checkbox 成功不能推导 NR ACTIVE；实际执行无证据时显示未验证。已加载但无法安全控制的消费者，即使请求关闭，实际状态也只能显示未知。缺消费者时显示不可用（请求开启）或关闭（请求关闭）。SR 关闭保留 NR 请求；当前 native carrier 不供给 SR 输出时显示该路线输入／支持未验证，不把它写成所有 NR 后端的通用依赖。

可选消费者只在它已真实加载且匹配固定构建、ABI、指纹及合法回调条件时尝试原 checkbox 控制。本项目不安装、加载或注入第三方消费者。外部 ReShade add-on 注册及真正 NR 执行仍需实际运行证据，不能以静态源码或缺依赖路线代替。

## 偏好优先级和 profile

有效请求顺序：模式默认 → 保存的可选字段 → 明确启动参数。模式默认保留原文件夹／flag／模式路线，RT 默认关闭。主开关支持 `-rt`／`-nort`、`-sr`／`-nosr`（既有 `-dlss`／`-nodlss`）及 `-nr`／`-nonr`；既有灯／材质 CLI 仍会形成显式 RT 请求。

图形偏好跟随现有 `defaultfile`／`-config` 的 profile 身份：解析成绝对路径后追加 `.graphics.cfg`。例如 `a.cfg.graphics.cfg` 和 `b.cfg.graphics.cfg` 独立。工作目录及相对路径按现有 `-config` 约定解析。语言仍沿用原 exe-relative `language.cfg` 语义。

文件最多 4096 字节，schema 1，`rt`／`sr`／`nr` 仅允许 0 或 1；未知键、重复键、NUL、超长或坏值整体拒绝。缺省字段保留当前模式默认。启动及普通退出不写图形偏好；菜单主动修改只保存该项，其他项的 CLI 覆盖和缺省字段不会写入。

保存先在同目录排他创建临时文件，核对写入／flush／close，备份原文件的完整有限字节，再原子替换。不可读取或过大的原文件拒绝替换，仍应用本次会话请求并报告未保存。备份为 `.bak.<pid>.<sequence>`；不自动删除原备份。

```ini
schema=1
rt=0
sr=1
nr=0
```

## 可配置的效果资源

exe 旁的 `rt-light.cfg` 仅配置一盏有限点灯；`rt-materials.cfg` 沿用材质 schema。读取有效默认资源不自动开启 RT。显式 `-rt-light` 和 `-rt-materials <path>` 分别优先。直接 CMake 及既有 mode staging 仅在目标缺文件时复制默认例子，不替换玩家已有资源。

```ini
schema=1
light=656 -160 -24 1 1 1 1048576 512
```

## 生效和回退

菜单只提交 CPU 请求，renderer 在 GPU fence 后统一应用：必要 teardown／重建、消费者请求、相关 history reset，然后冻结本帧输入并评估效果。显式 off／on 可以重新检查失败的可选路径；不会在每帧自动重试并隐藏失败。RT 先在线性场景上合成，再交给 SR。SR 初始化／创建／执行失败保留已完成的 RT 颜色及原 weapon／HUD／menu 覆盖；RT 失败保留普通场景及其他可用显示。可选 high-resolution／DLAA carrier 部分资源失败非致命，释放本次部分 owned 资源后保留成功 SR 或 RT／native。

## 只读验收观察

`-graphics-stats <new.csv>` 排他创建新观察文件并保留同一文件句柄；已有路径不覆盖。每行记录请求、实际状态／原因、reset、RT／SR 评估、实际 NGX feature、carrier 评估、消费者／checkbox／NR 执行分离字段。`-gpu-timing` 继续记录各 GPU 阶段、进程本地显存及预算。启动日志另外报告 engine、client、window、workarea 和 DPI awareness，用于区分实际窗口与截图坐标；这些日志本身不改变 DPI 行为。

实现和观察接口已落地；原生构建、真实菜单组合、重启／故障回退及窗口完整画面证据应以对应验收记录为准，不凭本说明宣称完成。
