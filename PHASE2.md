# AeroSim Phase 2 — Pacific Coast Vertical Slice

## 运行时合同

- UE 5.8；Project Version `0.2.0-phase2`。
- Runtime Contract：`aerosim.runtime.phase2/v1`；Build ID：`AeroSim-Phase2-20261016`。
- 复用 Phase 1 的七动作原子输入、轻量飞行模型、程序化场景和 Canvas HUD 合同。

## 交付范围

- 纯 C++/Engine BasicShapes 构建 1 km × 1 km Pacific Coast；核心验收区 300 m × 500 m。
- 三个 Hero Zone：分缝和路缘可读的起飞坪、干湿分带与潮线可读的岩岸湿边、具有桥面/栏杆/林下层次的栈道林缘。
- `Hangar → Preflight → Flight → Result → Replay` 可测试流程；`M` 推进，Canvas 显示当前状态。
- Nose（保留 Phase 1 API 名 `FPV`）、Near Follow（保留名 `Chase`）、Far Follow；Follow 使用 SpringArm Camera channel 球形探针防穿模。
- vFOV 是唯一持久真值；hFOV 按视口宽高比派生；支持 Preview / Apply / Cancel / Undo。
- 版本化 `FEnvironmentState`：Clear 16:30、Seed 21630；`T` 在 Clear / Overcast / Rain 间切换，并一致驱动光照、雾、曝光、饱和度和湿润参数。Rain 基线包含确定性世界雨线、相机前景雨幕和湿润高光覆盖，保证三种天气同机位肉眼可分。
- Pause 立即回中；恢复后 3 秒倒计时持续拒绝旧轴输入。
- 0.1 秒固定采样的确定性轨迹、Result 时间/距离/最大高度/采样数和 Replay 插值。
- 自适应 4%（最小 24 px）Canvas 安全区。

## Automation

`AeroSim.Phase2` 覆盖流程、vFOV 事务/宽高比、三相机与跟随碰撞配置、天气同步、三 Hero Zone/核心尺寸/安全区、暂停恢复回中、Result/Replay。Phase 0/1 测试保留。

## 明确边界

不实现正式 DJI 资产、复杂空气动力学、MCP 写权限、多人或 XR；不新增项目二进制 uasset。Rain 使用纯 C++ 世界雨线、Canvas 前景雨幕、湿润覆盖和状态驱动后处理作为基线替代体，不包含正式粒子、动态积水、水花、音频或物理风场。

## G0–G3 退出门

- **G0 存在合理性**：三 Hero Zone 均服务海岸巡检路线并有明确标签和构件语义。
- **G1 空间/尺度/重力**：1 km 边界、300 m × 500 m 核心、桥柱/栏杆/岩带/路缘均有几何和碰撞支撑。
- **G2 正常视角**：Nose/Near/Far 三档，Follow 防穿模；需以固定视角 Metal 截图和人工观感最终签字。
- **G3 材质/光照/天气**：Clear/Overcast/Rain 同一状态驱动；本阶段只验基线一致性，不宣称正式 PBR/VFX/音频完成。

验证命令和最终事实记录见 README；任何未真实执行项必须标为 `NOT_RUN` 或 `BLOCKED`。
