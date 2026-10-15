# AeroSim Phase 1

AeroSim 当前冻结 **Unreal Engine 5.8**，并在 Phase 0 工程骨架上实现 Phase 1 最小飞行闭环。升级或降级引擎必须单独评审，不能静默改写 `EngineAssociation`。

## Phase 1 运行时闭环

- `AFlightPawn` 的 `FFlightInputSuite` 以一个原子单元声明 Pitch、Roll、Yaw、Throttle、Camera、Pause、Reset 七个设备无关语义动作。默认 C++ Mapping Context 同时提供键鼠和手柄映射；引用缺失、ValueType 不符、Context 缺动作或混入外来动作都会 fail closed。
- `FFlightModel` 是不依赖 World 的确定性轻量模型，状态为 Grounded、Takeoff、Flight、Landing、Paused。它提供起飞、惯性趋近、姿态控制、降落与重置，不模拟复杂空气动力学。
- Pause、应用失焦、输入设备断连都会同步执行 `NeutralizeInput`；恢复前再次保持中和，避免旧轴值继续驱动飞机。
- Camera 在 FPV 与 Chase 间切换；`FFlightHudData` 提供状态、相机、相对高度、速度、油门、输入中和状态和 Build ID。`AFlightHUD` 用左上角半透明 Canvas 面板显示关键飞行数据，不遮挡中心视界。
- `APacificCoastGraybox` 仅用 C++ 和 Engine BasicShapes 生成 1 km × 1 km 训练场：起飞坪、陆地/海面分区、海岸线、林缘和五个路线门；没有项目二进制 uasset。
- Runtime contract 为 `aerosim.runtime.phase1/v1`，Build ID 为 `AeroSim-Phase1-20261016`，项目版本为 `0.1.0-phase1`。

详细实现边界与验证记录见 [`PHASE1.md`](PHASE1.md)。

## 验证命令

在仓库根目录执行：

```bash
python3 Scripts/phase0_static_validate.py
python3 -m unittest discover -s Scripts/tests -v
python3 Scripts/phase0_preflight.py --ue-editor "/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor"

"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
  AeroSimEditor Mac Development "$PWD/AeroSim.uproject" -WaitMutex

"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor" \
  "$PWD/AeroSim.uproject" -unattended -nop4 -nosplash -NullRHI -NoSound \
  -ExecCmds="Automation RunTests AeroSim.Phase1;Quit" \
  -TestExit="Automation Test Queue Empty" -log
```

如后续需要 Cook，必须给 `BuildCookRun` 传入 `-AdditionalCookerOptions='-SkipZenStore'`。

## 当前边界

Phase 1 不包含 Phase 2+ 环境视觉、正式资产、天气、MCP、复杂空气动力学或项目二进制 uasset。当前相机没有 Phase 2 的完整 FOV 设置链；HUD 是最小 Canvas 表现而非正式 UI。Mac 自动化工程退出门已通过；Windows Development 和真实画面下的人工灰盒 G0/G1 仍需在对应环境验证。
