# Phase 0 Acceptance

## 冻结范围

- EngineAssociation：Unreal Engine 5.8
- 平台基线：macOS、Apple M3 Pro、Metal SM6
- Runtime 与 Editor-only 模块边界保持独立
- 固定 Evidence 相机默认锁定评审变换
- `AeroSim.Target.cs` 仅声明 Target，不执行 preflight 或启动进程

## 强制执行顺序

1. 运行 `python3 Scripts/phase0_static_validate.py`。
2. 运行 `python3 Scripts/phase0_preflight.py --ue-editor <UE5.8 UnrealEditor path>`。
3. 仅当两者退出码均为 `0` 时，显式调用同一 UE5.8 安装中的 Build/Cook/Package 工具。
4. 任一脚本返回非零时立即停止，不得继续 Cook/Package，也不得将失败包装成成功。

建议用 `&&` 连接三步，使 preflight 的非零退出码直接阻断后续 `RunUAT.sh BuildCookRun`。

## Static Validate 验收项

`Scripts/phase0_static_validate.py` 必须验证：

- `.uproject`、`.uplugin` 和迁移 manifest JSON 可解析
- EngineAssociation 固定为 `5.8`
- Phase 0 必需目录存在
- Runtime/插件代码中不存在约定的危险执行 API
- `AeroSim.Target.cs` 不含进程启动或 `System.Diagnostics`
- `.gitattributes` 覆盖 uasset、umap、FBX 和纹理 LFS 规则
- 迁移 manifest schema、必填字段、唯一 ID/目标路径、安全相对路径、文件存在性、字节数和 SHA-256 均一致
- `SourceAssets` 根、manifest 目标文件及其任一父级都不是 symlink；resolve 后的真实目标仍位于真实 `SourceAssets` 根内
- 输入原子套件验证与相机 Transform/Rotation/FOV fail-closed 合同源码标记完整

## Cook / Package Preflight 验收项

`Scripts/phase0_preflight.py` 必须以固定参数、`shell=False` 检查：

- UnrealEditor 必须位于可反向定位的 Engine 根中；同根 `Build/Build.version` 必须声明 `MajorVersion=5`、`MinorVersion=8`
- 同一 Engine 根下必须存在非 symlink 的 `Build/BatchFiles/RunUAT.sh`
- 工程所在卷至少有 100 GiB 可用空间
- `/usr/bin/xcode-select -p` 返回有效 developer directory
- `/usr/bin/xcrun --sdk macosx --show-sdk-path` 返回有效 SDK 目录
- `/usr/bin/xcrun --find metallib` 返回有效文件

脚本只接受 UnrealEditor **路径**，不接受命令字符串或额外 shell 参数。

## 当前诚实状态

本次执行环境没有可用 UE5.8；磁盘约 56 GiB，低于 100 GiB；`metallib` 无法定位。故 preflight 应失败并阻断 Cook/Package。只有静态验证脚本可预期通过。

UE5.8 到位后仍须编译 AeroSimEditor Mac Development，并运行：

- `AeroSim.Phase0.Runtime.Smoke`
- `AeroSim.Phase0.Editor.Heartbeat`

完成前不得标记为“编译、Automation、Cook 或 Package 通过”。
