# AeroSim UE5 Phase 3 — 机型资产门与相机质量

## 1. 阶段结论

Phase 3 已建立可替换的机型合同、Generic Golden Aircraft 程序化代理、四类运行相机与固定证据相机合同。旧仓没有正式无人机 FBX/GLB，故阶段资产出口保持 **`BLOCKED_ASSET`**：本实现不是正式 DJI 资产，不具备 `PRODUCTION_READY` 或 `USER_CONFIRMED` 资格，也不得进入正式任务。

Runtime 合同：`aerosim.runtime.phase3/v1`；Build ID：`AeroSim-Phase3-20261016`。

## 2. AircraftDefinition / AssetStatus

`FAircraftDefinition` 是稳定替换边界，包含：

- 独立 `runtimeId`、`catalogId`、显示身份、资产版本与 variant；
- 来源 URI、授权说明、披露、内容 SHA-256；
- `EAircraftAssetStatus` 全状态：`CONTENT_NOT_DELIVERED / UNVERIFIED / REFERENCE_LOCKED / GEOMETRY_VERIFIED / PRODUCTION_READY / USER_CONFIRMED / REJECTED / BLOCKED_ASSET`；
- 实体尺寸、重心、支撑面；
- LOD 名称、screen size、三角预算和 proxy 标志；
- Shell / Metal / Lens / Propeller / NavigationEmission 材质槽；
- 已过与阻塞质量门；
- `ReplacementAsset` + `ReplaceableStaticMeshPackage` 正式替换接口。

`CanEnterFormalTask()` fail closed：只有正式替换包、64 位源哈希、零阻塞门且状态为 `USER_CONFIRMED` 才可进入正式任务。程序化代理不会因结构或自动化通过而越门。

机器可读合同：`SourceAssets/AircraftReference/phase3-aircraft-definition.json`。

## 3. Generic Golden Aircraft

`AFlightPawn` 使用纯 C++ 和 Engine Basic Shapes 组装原创消费级相机四旋翼：

- 紧凑机身、四条对角臂、四个电机、四副可旋转螺旋桨；
- 四点落地支撑、前下方两轴云台、相机舱与镜头；
- 前绿/后红导航灯；
- 约 51 × 47 × 13 cm 外包络，重心位于机身内，支撑面低于重心；
- 程序化 MID 区分壳体、金属、镜头、桨叶和导航灯语义；
- LOD0 细节装配与 LOD1 proxy 可互斥切换；
- FPV/云台视角隐藏机体，跟随视角显示机体；正式资产可沿同一 definition/runtime ID 替换。

该代理只用于通用系统链、开发、Automation 和隔离审核，HUD 永久显示 `BLOCKED_ASSET / GENERIC PROXY`。

## 4. 相机质量

相机 family/preset：

1. `FPV / NOSE`：镜头位于机体前部，无拖尾臂，无机体遮挡；
2. `GIMBAL`：独立 pitch/yaw，pitch `[-90°, +30°]`、yaw `[-75°, +75°]`，时间平滑，绝对旋转清除 roll 以保持地平线；
3. `NEAR FOLLOW`：SpringArm 碰撞探针、防穿模、相机滞后；
4. `FAR FOLLOW`：更长跟随距离，同样启用碰撞；
5. 相机继续复用 Phase 2 的 `FVerticalFovTransaction`，唯一持久化真值为 vFOV，Preview/Apply/Cancel/Undo 语义不分叉。

`C` 循环相机；方向键调整云台目标。HUD 显示机型、资产状态、相机、vFOV、云台 pitch/yaw。

## 5. 证据合同

`FPhase3EvidenceCameraContract::FixedSet()` 固定六个复核位：

- `aircraft-three-quarter`；
- `fpv`；
- `gimbal-down`；
- `near-follow`；
- `far-follow`；
- `scale-support`。

每项冻结 ID、Pawn transform、preset、vFOV 与云台 pitch，并 fail closed 校验已知 preset、FOV 与云台限位。命令行对应 `-AeroSimReview=Aircraft|FPV|Gimbal|NearFollow|FarFollow|ScaleSupport`。

## 6. Automation 与阶段边界

`AeroSim.Phase3` 覆盖：定义与正式任务 fail-closed、云台限位/平滑/地平线、相机显隐与跟随碰撞、LOD proxy、vFOV 事务复用、尺度/重心/支撑、材质槽、固定证据相机。

严格不实现：

- 正式 DJI 模型、商标贴图、伪造授权或用户确认；
- Phase 4 场景内容/天气音画扩展、任务训练；
- Phase 5 MCP 或受控编辑；
- 复杂空气动力学、真实图传或产品级飞控声明。

## 7. 资产风险

`BLOCKED_ASSET` 的解除必须由独立正式资产接入完成：DCC 源、批准格式模型、完整材质源、轴/单位、LOD、碰撞、socket、来源授权、SHA-256、工程门、机型+版本+哈希用户确认缺一不可。当前 shell PNG 仍只是 reference swatch，不参与本代理材质，也不构成正式几何证据。
