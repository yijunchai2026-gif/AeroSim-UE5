# PHASE 0 冻结与验证门槛

UE5.8 是 Phase 0 冻结版本。详细验收顺序和阻断条件见 `PHASE0_ACCEPTANCE.md`。

`AeroSim.Target.cs` 保持纯声明；它不检查磁盘，不调用 `xcrun`，也不启动进程。普通 Game 编译不应被 Cook/Package 环境门槛阻断。

在显式 Build/Cook/Package 前依次运行：

1. `python3 Scripts/phase0_static_validate.py`
2. `python3 Scripts/phase0_preflight.py --ue-editor <UE5.8 UnrealEditor path>`
3. 仅当前两步退出码均为 `0` 时调用 UE5.8 Build/Cook/Package

当前机器已安装 UE 5.8.3，Xcode、macOS SDK、Metal Toolchain 和 100 GiB 磁盘门槛均通过。2026-10-16 已完成 AeroSimEditor Mac Development 编译、2 项 Phase 0 Automation、Mac Development Cook/Stage/Package/Archive，以及打包应用的 NullRHI 启动冒烟；正式画面与人工 G0–G4 验收仍需后续场景内容。
