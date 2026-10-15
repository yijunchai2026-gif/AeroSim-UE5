# AeroSim Phase 1 — 可飞灰盒自动化退出门

## 1. 范围与运行时合同

本阶段交付可构建、可 Cook、可启动、可自动化验证的最小飞行闭环，不实现 Phase 2+ 环境视觉、正式资产、天气或 MCP。

| 项目 | 值 |
|---|---|
| UE | 5.8 |
| Project Version | `0.1.0-phase1` |
| Runtime Contract | `aerosim.runtime.phase1/v1` |
| Contract Version | `1` |
| Build ID | `AeroSim-Phase1-20261016` |
| 运动单位 | cm、s、degree |

Build ID 是 Phase 1 源码合同标识，不替代 Git revision、内容哈希或发布流水线生成的不可变构建编号。

## 2. 已实现

### 输入与飞行

`FFlightInputSuite` 原子提供 Pitch、Roll、Yaw、Throttle、Camera、Pause、Reset 七个语义 Action。Simulation 只消费归一化语义值；缺引用、ValueType 错误、Context 缺动作或混入外来 Action 均拒绝整个 Suite。

`FFlightModel` 是无 World 依赖的值类型状态机：

`Grounded → Takeoff → Flight → Landing → Grounded`

任一活动状态可进入 `Paused` 并保留恢复状态。轻量模型提供起飞加速、飞行惯性趋近、姿态控制、固定安全下降和原子 Reset，不宣称复杂空气动力学、跨平台位级确定性或正式回放能力。

Pause、应用失焦、输入设备断连、Pawn restart 和 Reset 都执行 `NeutralizeInput()`。Paused 状态拒绝新语义轴输入，解除暂停前仍保持输入为零。

### 1 km × 1 km Pacific Coast 灰盒

`APacificCoastGraybox` 在 `AAeroSimGameMode::StartPlay()` 中生成，只引用 Engine `/Engine/BasicShapes/Cube`，不创建项目二进制 uasset：

- 合同范围：`100000 cm × 100000 cm`；
- 起飞坪：原点附近 30 m × 30 m；
- 陆地、海面按 100 m × 100 m 分块，形成可读尺度网格和连续海岸分界；
- 西侧八段林缘体块；
- 起飞坪四角布置四个 1.8 m 标准尺度参照，所有门柱具有落地基座；
- 五个高对比编号路线门，六段航线共 18 个方向标记（返航段使用独立颜色）；
- 路线顺序：起飞坪 → 五门 → 返回起飞坪并落地；
- Key/Fill light、投射阴影和接触阴影用于强化灰盒支撑关系。

`AFlightPawn` 由 Engine BasicShapes 组成通用灰盒飞行器 shell；Chase 模式显示机身、双翼和尾翼，FPV 模式隐藏 shell，避免遮挡前视视界。

`FPacificCoastGrayboxContract` 纯函数验证尺寸、路线点数量、所有点在边界内及首尾闭环；`FPacificCoastRouteProgress` 验证起飞、顺序穿门和返航落地合同。

### 可见最小 HUD

`AFlightHUD` 使用 UE Canvas 在左上角 330 × 82 px 半透明面板绘制：

- Flight Phase；
- FPV / Chase；
- 高度与速度；
- 油门；
- Paused / Input Safe 提示。

面板避开中心视界；它是 Phase 1 最小功能 HUD，不是 Phase 2 正式 CommonUI/UMG 设计。

## 3. Automation 覆盖

`AeroSim.Phase1` 共六项：

1. `Flight.StateMachine`：Grounded、Takeoff、Flight、Landing 与惯性；
2. `Graybox.GeometryContract`：1 km 范围、路线点、五门、18 个方向标记、四个尺度参照和 150 个以上程序化体块；
3. `Graybox.RouteSmoke`：起飞、顺序穿过五门、返航落地以及 HUD GameMode 配置；
4. `Input.NeutralizeAndPause`：输入 clamp、暂停中和、暂停时拒绝输入、恢复中和；
5. `Runtime.CameraResetHud`：FPV/Chase、Chase shell 显示/FPV 隐藏、Reset、HUD 数据和 Build ID；
6. `PureFunctions.BoundsAndReset`：纯输入边界、Neutralize、Paused 不推进、模型 Reset。

Phase 0 Runtime Smoke 同步使用七动作原子 Suite 检查，并保留 Evidence Camera fail-closed 回归。

## 4. 2026-10-16 验证事实

| 验证 | 结果 |
|---|---|
| `Scripts/phase0_static_validate.py` | PASS，522 checks |
| Python 脚本单测 | PASS，3/3 |
| UE 5.8 Mac `AeroSimEditor Development` 编译 | PASS |
| `AeroSim.Phase1` NullRHI Automation | PASS，6/6 |
| `AeroSim.Phase0` 回归 | PASS，2/2 |
| Cook | PASS，使用 `-AdditionalCookerOptions='-SkipZenStore'` |
| Stage / Package / Archive | PASS，AutomationTool ExitCode 0 |
| 可视化打包应用启动及窗口截图 | PASS；Development Metal 窗口显示 Chase shell、HUD、分块地面、START/RETURN 与接触阴影 |
| AndroidFileServer 自动配置清理 | PASS；最终 `DefaultEngine.ini` 无该段和 token |
| Windows Development | NOT_RUN（当前验证平台为 macOS） |
| G0 自评（场景/HUD/起点可见） | PASS |
| G1 自评（尺度、支撑、路线可读性） | PARTIAL / PENDING 人工复核；shell、尺度参照、门基座和路线引导已实现，但单张起点截图不能证明全路线飞行中的编号可读性 |
| G2 自评（完整人工飞行体验） | PENDING；自动化路线合同不能替代真人操控验收 |

Archive 生成于本机临时路径 `/private/tmp/AeroSim-Phase1-G1-Archive/Mac/AeroSim.app`，不进入仓库。可视化证据：`/private/tmp/aime-agent-shared-dir/4e3f96ddfdaf/AeroSim-Phase1-G1-Review.png`。

## 5. 退出门判定

**Phase 1 本轮 Mac 自动化工程退出门：PASS。**

已具备编译、语义输入、飞行状态机、安全暂停、可见 HUD、程序化灰盒、路线合同、Automation、Cook/Stage/Package/Archive 和打包启动的闭环证据。

**人工门自评：G0 PASS；G1 PARTIAL / PENDING 人工复核；G2 PENDING。** 当前真实渲染截图已证明 HUD、Chase shell、起点标识、地面分块和接触阴影可见；自动化已证明五门顺序、方向标记、尺度参照和返航合同。不过单张静态起点截图不能证明飞行中五个编号门始终清晰，也不能替代完整真人操控验收。Windows Development 未在本机执行，不能由 Mac 结果外推。

## 6. 已知边界

- 灰盒使用 Engine BasicShapes 和默认材质，不是环境视觉或正式资产。
- HUD 是最小 Canvas 表现，不包含正式视觉规范、安全区矩阵或可访问性设置。
- 相机不包含 Phase 2 的完整 FOV 设置、预览事务或多档跟随方案。
- 输入断连采用任意设备断连即安全暂停的保守策略，尚未做多用户 ownership 仲裁。
- 运动通过轻量 Actor 变换更新，不覆盖高速碰撞、网络复制或复杂物理解算。
- NullRHI 启动不能代替真实渲染、人机飞行体验、真实性能或 G0/G1 人工签字。
