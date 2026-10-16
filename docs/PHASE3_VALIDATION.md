# Phase 3 验证记录

验证日期：2026-10-16（CST）  
目标：UE 5.8 Mac Development；Generic Golden Aircraft 与相机质量  
资产出口：**BLOCKED_ASSET**（旧仓无正式无人机 FBX/GLB；不因工程测试通过而解除）

## 1. 自动验证矩阵

| 项目 | 状态 | 证据口径 |
|---|---|---|
| Phase 3 静态合同 | PASS | 39 checks；`python3 Scripts/phase3_static_validate.py` |
| Python 全套 | PASS | 10 tests；`python3 -m unittest discover -s Scripts/tests -v` |
| UE 5.8 Mac Development 编译 | PASS | `Build.sh AeroSimEditor Mac Development` |
| `AeroSim.Phase3` Automation | PASS | 5/5：机型/相机/证据门 |
| Phase 0–2 回归 | PASS | 15/15：Phase0 2、Phase1 6、Phase2 7 |
| 全套 `AeroSim` Automation | PASS | 20/20，exit code 0 |
| Cook / Stage / Package / Archive | PASS | `BuildCookRun` + `-SkipZenStore`，ExitCode 0 |
| 打包应用 NullRHI smoke | PASS | `aerosim.runtime.phase3/v1`，正常初始化与退出 |
| Metal 固定截图 6 张 | PASS | 逐 preset 启动打包 Metal 应用；通过 CoreGraphics 精确验证唯一 `owner=AeroSim`、标题 `AeroSim (64-bit Development SF_METAL_SM6)`、`layer=0` 的 window ID，再以 `screencapture -l <window-id>` 捕获；六图已逐张画面复核，仅含 AeroSim 窗口 |
| AndroidFileServer/token 清理 | PASS | 最终 `Config/DefaultEngine.ini` 无该段或 token |

## 2. 工程验收含义

- G0：通用身份、来源和用途合理；**不得外推为具名生产机型**。
- G1：自动化验证尺寸、重心、支撑、显隐和碰撞合同；人工截图复核接地与尺度。
- G2：FPV、云台、近/远跟随和机体四分之三视图使用固定 preset 复核。
- G3：仅验证程序化材质槽语义与 Metal 可见结果；不声明认证 PBR 或正式机型材质。
- G4：编译、Automation、Cook/Package、smoke 和 LOD proxy；不以短跑替代发布性能门。

## 3. 截图计划

最终文件应位于共享目录 `Artifacts/Phase3/Screenshots/`：

1. `01-aircraft-three-quarter.png`
2. `02-fpv.png`
3. `03-gimbal-down.png`
4. `04-near-follow.png`
5. `05-far-follow.png`
6. `06-scale-support.png`

截图来自 Metal 运行应用并绑定 `AeroSim-Phase3-20261016`。捕获不是桌面区域或前台焦点截图：每一张均先按 owner、窗口标题、layer 唯一定位 AeroSim window ID，再按该 ID 捕获；保存后逐张检查窗口标题、HUD 相机模式与画面内容。若任一步失败必须保持 `NOT_CAPTURED`，不可用旧仓、Editor 或其他应用窗口顶替。

## 4. 保持阻塞

正式资产、生产来源哈希、授权审批和用户逐机确认均未提供。因此即使本页所有工程项通过，Phase 3 正式机型出口仍为 `BLOCKED_ASSET`，阶段不得标为正式资产 PASS。
