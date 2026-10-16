# Phase 4 验证记录

验证日期：2026-10-16（CST）  
目标：UE 5.8 Mac Development；Phase 4 海岸巡检训练产品闭环  
Runtime：`aerosim.runtime.phase4/v1`  
资产出口：**BLOCKED_ASSET**

## 1. 自动验证矩阵

| 项目 | 状态 | 证据口径 |
|---|---|---|
| Phase 4 静态合同 | PASS | `phase4_static_validate.py`：49 checks |
| Python 全套 | PASS | 15 tests，全部通过 |
| UE 5.8 Mac Development 编译 | PASS | `Build.sh AeroSimEditor Mac Development`，Result: Succeeded |
| `AeroSim.Phase4` Automation | PASS | 5/5：任务/恢复/评分/Replay/HUD |
| Phase 0–3 回归 | PASS | 全套 `AeroSim` Automation：25/25 |
| Cook / Stage / Package / Archive | PASS | `BuildCookRun -SkipZenStore`，ExitCode=0 |
| 打包应用 NullRHI smoke | PASS | 退出码 0；日志包含 `aerosim.runtime.phase4/v1` |
| Metal 固定截图 6 张 | PASS | 唯一 AeroSim layer-0 window ID；逐张尺寸、内容和 SHA-256 自检 |
| AndroidFileServer/token 清理 | PASS | 插件禁用；配置/源码/脚本/文档无 token |

> 本表只在命令真实完成后更新；失败或缺环境不得写 PASS。

## 2. Automation 覆盖

`AeroSim.Phase4` 固定 5 组：

1. 版本化任务、目标、五 checkpoint、安全区和恢复点；
2. Takeoff→顺序巡检→RTH→Landing→Result；
3. 越界、碰撞、漏门与进度保留；
4. Safety/Precision/Efficiency/Smoothness 与 Result/Replay digest 对齐；
5. HUD 文本/形状可访问提示与渐进引导。

全套 `AeroSim` 同时回归 Phase 0–3。

## 3. Metal 截图合同

最终截图目录：`Artifacts/Phase4/Screenshots/`：

1. `01-hangar.png`：任务卡、正式资产阻塞；
2. `02-preflight.png`：四项检查；
3. `03-flight-hint.png`：顺序 checkpoint、编号+形状提示、中心视野无遮挡；
4. `04-boundary-recovery.png`：越界文字、chevrons、恢复动作；
5. `05-result.png`：四维评分与 Replay digest；
6. `06-replay.png`：Replay 标识、与 Result 相同分数/digest。

| 截图 | Window ID | 尺寸 | SHA-256 |
|---|---:|---:|---|
| `01-hangar.png` | 139026 | 3104×2088 | `806c81b9aa03b666095bc14664a39215fdc65d85158f7c419260fd85f5de17f6` |
| `02-preflight.png` | 139035 | 3104×2088 | `d5c45c87639da0fe5116d12ff35dcf1b065bbbad453fe61012628795bc065d07` |
| `03-flight-hint.png` | 139044 | 3016×2000 | `4209b269867bbb954e1c1caf1ef065f9fc9652ad9457d33bf137c0a9ccb7cc59` |
| `04-boundary-recovery.png` | 139054 | 3016×2000 | `489994ec65470b7d353d503653e7630e5777e35642431782e1af99cf86c37ed3` |
| `05-result.png` | 139063 | 3016×2000 | `a0071d26c5a76ceadab6a6c92c32a10209a768a9686122ba4fe5aa90493e0528` |
| `06-replay.png` | 139072 | 3016×2000 | `78dfd22f823f854a9f0b851a80fb63868254195533ad6499fa7c1037211baf27` |

Result 与 Replay 自检均显示四维分数 `100 / 087 / 095 / 094`、总分 `094`，且跨独立进程使用相同 digest `D9099C59`。

每张必须从打包 Metal 应用启动参数 `-AeroSimPhase4View=Hangar|Preflight|FlightHint|BoundaryRecovery|Result|Replay` 获取。捕获前用 CoreGraphics 枚举并验证唯一 `owner=AeroSim`、标题包含 `AeroSim`、`layer=0` 的 window ID，再执行 `screencapture -l <window-id>`。禁止桌面区域截取、前台焦点截取、Editor 截图或其他应用替代。保存后检查 PNG 尺寸、可见窗口标题、对应状态文字、非纯色内容和文件 SHA-256。

## 4. 边界

- Generic proxy 始终显示 `BLOCKED_ASSET / GENERIC PROXY`；工程通过不解除正式资产门。
- Phase 4 不开放 MCP/AI 写操作；Editor API 无 Python、Shell、console command、文件写入、进程执行或 Transform。
- 本轮不 branch/commit/push。
