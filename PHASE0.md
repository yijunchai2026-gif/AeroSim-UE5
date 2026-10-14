# PHASE 0 冻结与验证门槛

UE5.8 是 Phase 0 冻结版本。详细验收顺序和阻断条件见 `PHASE0_ACCEPTANCE.md`。

`AeroSim.Target.cs` 保持纯声明；它不检查磁盘，不调用 `xcrun`，也不启动进程。普通 Game 编译不应被 Cook/Package 环境门槛阻断。

在显式 Build/Cook/Package 前依次运行：

1. `python3 Scripts/phase0_static_validate.py`
2. `python3 Scripts/phase0_preflight.py --ue-editor <UE5.8 UnrealEditor path>`
3. 仅当前两步退出码均为 `0` 时调用 UE5.8 Build/Cook/Package

当前机器没有可用 UE5.8，约 56 GiB 可用空间低于 100 GiB 门槛，且 `metallib` 无法定位。因此当前只能记录静态验证，不能声明编译、Automation、Cook 或 Package 通过。
