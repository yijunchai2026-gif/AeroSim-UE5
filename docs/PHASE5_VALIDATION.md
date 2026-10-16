# Phase 5 验证记录

验证日期：2026-10-16（CST）  
目标：UE 5.8 Mac Development；Editor-only MCP 只读协议  
Runtime：`aerosim.runtime.phase5/v1`  
资产出口：**BLOCKED_ASSET**

## 验证矩阵

| 项目 | 状态 | 证据 |
|---|---|---|
| Phase 5 静态合同 | PASS | `52 checks` |
| Python 全套与 Phase 0–4 回归 | PASS | `19/19` |
| UE 5.8 Mac Development 编译 | PASS | `AeroSimEditor` 编译完成 |
| `AeroSim.Phase5` Automation | PASS | Editor MCP 4/4 协议、安全、Evidence、Gate |
| 全套 `AeroSim` Automation | PASS | Phase 0–5 共 29/29 |
| Cook/Stage/Package/Archive | PASS | `BuildCookRun` + `-SkipZenStore`，ExitCode 0 |
| 打包应用 NullRHI smoke | PASS | 正常初始化并退出，无 Fatal/Assertion |
| Editor MCP 未进入 Runtime 包 | PASS | Editor-only 模块，Runtime 无反向依赖 |
| token/敏感配置清理 | PASS | Config 与 Phase 5 Package 最终扫描无匹配 |

> 仅在命令真实完成且退出码/日志可核验后改为 PASS。结构化 capture request 不等于真实截图，目标硬件性能和人工 G0–G4 不由本轮自动签字。

## Automation 范围

1. schema/version/request ID、capability discovery 与 heartbeat 复用；
2. mutation、脚本、console 与非法路径统一 fail closed；
3. 请求/响应/分页/超时边界和 append-only 脱敏审计；
4. 固定 Evidence Camera 请求 manifest 与 G0→G4 状态纪律。

## 命令

```bash
python3 Scripts/phase5_static_validate.py
python3 -m unittest discover -s Scripts/tests -v

"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
  AeroSimEditor Mac Development "$PWD/AeroSim.uproject" -WaitMutex

"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor" \
  "$PWD/AeroSim.uproject" -unattended -nop4 -nosplash -NullRHI -NoSound \
  -ExecCmds="Automation RunTests AeroSim;Quit" \
  -TestExit="Automation Test Queue Empty"
```

Cook/Package 使用 `RunUAT BuildCookRun` 并传 `-AdditionalCookerOptions="-SkipZenStore"`；完成后删除 UAT 可能写入的 AndroidFileServer 段和 token，再复跑静态检查。
