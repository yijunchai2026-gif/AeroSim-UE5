# AeroSim Phase 0

AeroSim 的 Phase 0 冻结引擎版本为 **Unreal Engine 5.8**。升级或降级引擎必须单独评审，不能静默改写 `EngineAssociation`。

## 验证顺序

在仓库根目录执行：

```bash
python3 Scripts/phase0_static_validate.py
python3 Scripts/phase0_preflight.py --ue-editor "/Applications/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
```

只有两个脚本都返回 `0`，才能继续显式调用 UE Build/Cook/Package。调用链必须使用成功依赖，例如：

```bash
python3 Scripts/phase0_static_validate.py && \
python3 Scripts/phase0_preflight.py --ue-editor "$UE_EDITOR" && \
"$RUN_UAT" BuildCookRun -project="$PWD/AeroSim.uproject" -platform=Mac -build -cook -stage -pak -package
```

`UE_EDITOR` 只能是 UnrealEditor 可执行文件路径；`RUN_UAT` 只能是同一 UE5.8 安装中的 `Engine/Build/BatchFiles/RunUAT.sh`。不得把未经校验的文本作为命令传入脚本。

## macOS 基线与阻断条件

- Apple M3 Pro 与 Metal SM6 适合作为当前开发基线。
- 审查基线约 53 GB；本次检查约 56 GiB 可用，低于 Phase 0 的 100 GiB Cook/Package 门槛。
- 本次检查无法通过 `/usr/bin/xcrun --find metallib` 定位 `metallib`。
- `Scripts/phase0_preflight.py` 从 UnrealEditor 路径反向定位同一 `Engine/Build/Build.version`，只接受 `MajorVersion=5`、`MinorVersion=8`，并要求同一 Engine 根下存在非 symlink 的 `Build/BatchFiles/RunUAT.sh`；同时使用固定、只读检查验证磁盘空间、Xcode developer directory、macOS SDK 和 `metallib`。
- Preflight 不调用 UE、不写文件、不使用 shell，也不接受任意命令字符串。任一检查失败即返回非零，后续 Cook/Package 必须停止。
- `AeroSim.Target.cs` 是纯声明，不执行磁盘检查，也不启动进程；普通 Game 编译不受打包预检误伤。

## 原生输入基线

`AFlightPawn` 在 C++ 中提供原子 `FFlightInputSuite`，默认套件无需 Blueprint 或 Input DataAsset 即可使用。MappingContext 与 Move、Look、Throttle、Reset Actions 只能整套覆盖；应用和绑定前会验证引用完整、ValueType 正确，并确认 Context mappings 恰好引用当前四个 Actions。部分覆盖、外来 Action 或不一致 Context 会记录 error/ensure 并 fail closed。

## Evidence 相机合同

三种评审相机默认启用 `bLockToReviewTransform` 并强制使用固定评审变换。捕获前必须调用 `CanGenerateFormalEvidence()`；它会以小容差校验当前 Location、Rotation 和 Camera FOV。显式关闭锁定或任一状态漂移都会 fail closed，相关画面不得作为正式 Evidence。

## 无 UE 环境说明

当前机器没有可用 UE5.8，且 preflight 会因 UE Editor、空间和 `metallib` 条件失败。因此这里只能声明静态验证结果，不能声明编译、Automation、Cook 或 Package 通过。
