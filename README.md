# AeroSim Phase 2

AeroSim 冻结 **Unreal Engine 5.8**，当前运行时合同为 `aerosim.runtime.phase2/v1`，项目版本为 `0.2.0-phase2`。Phase 2 复用 Phase 1 输入/飞行合同，交付无项目二进制 uasset 的 Pacific Coast C++ vertical slice。

## 当前交付

- **环境**：1 km × 1 km 可见切片、300 m × 500 m 核心区；起飞坪地表、岩岸湿边、栈道林缘三个程序化 Hero Zone。
- **流程**：`Hangar → Preflight → Flight → Result → Replay`，Canvas 显示当前状态；`M` 推进流程。
- **相机/FOV**：Nose、Near Follow、Far Follow；vFOV 唯一真值，hFOV 由视口宽高比派生；Preview/Apply/Cancel/Undo；Follow SpringArm 使用 Camera channel 碰撞探针。
- **环境状态**：版本化 Clear 16:30 / Seed 21630 基线；`T` 切换 Clear/Overcast/Rain，统一驱动光照、雾、曝光、饱和度和湿润参数；Rain 另有确定性世界雨线、相机前景雨幕和湿润高光覆盖。
- **安全输入**：Pause 立即回中；恢复经过 3 秒倒计时，期间拒绝旧轴输入。
- **结果/回放**：0.1 秒固定采样轨迹、确定性插值和时间/距离/最大高度/采样数指标。
- **UI/安全区**：最小可见 Canvas HUD，按视口 4% 且不小于 24 px 布局。

详细合同、边界和 G0–G3：[`PHASE2.md`](PHASE2.md)。验证矩阵：[`docs/PHASE2_VALIDATION.md`](docs/PHASE2_VALIDATION.md)。Phase 1 历史事实仍见 [`PHASE1.md`](PHASE1.md)。

## 验证

```bash
python3 Scripts/phase0_static_validate.py
python3 Scripts/phase2_static_validate.py
python3 -m unittest discover -s Scripts/tests -v

"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
  AeroSimEditor Mac Development "$PWD/AeroSim.uproject" -WaitMutex

"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor" \
  "$PWD/AeroSim.uproject" -unattended -nop4 -nosplash -NullRHI -NoSound \
  -ExecCmds="Automation RunTests AeroSim;Quit" \
  -TestExit="Automation Test Queue Empty"
```

Cook/Stage/Package/Archive 必须通过 `RunUAT BuildCookRun` 并传入 `-AdditionalCookerOptions="-SkipZenStore"`。UAT 可能自动写入 AndroidFileServer 设置；验证完成后必须从 `Config/DefaultEngine.ini` 删除完整段及 token。

## 明确不做

Phase 2 不实现正式 DJI 资产、复杂空气动力学、MCP 写权限、多人或 XR。天气仅为一致状态驱动的基线表现；不宣称正式 PBR、Rain VFX/音频、真实水体或性能发布门完成。
