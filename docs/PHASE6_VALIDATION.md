# Phase 6 验证记录

验证日期：2026-10-16（CST）  
目标：UE 5.8 Mac Development；Editor-only 受控 Sandbox 事务  
Runtime：`aerosim.runtime.phase6/v1`  
资产/最终用户出口：**BLOCKED_ASSET / BLOCKED_USER_CONFIRMATION / BLOCKED_WINDOWS_CANDIDATE**

## 验证矩阵

| 项目 | 状态 | 证据 |
|---|---|---|
| Phase 6 静态合同 | PASS | `81 checks` |
| Python 全套与 Phase 0–5 回归 | PASS | `23/23` |
| UE 5.8 Mac Development 编译 | PASS | `AeroSimEditor` 编译成功，ExitCode 0 |
| `AeroSim.Phase6` Automation | PASS | 3/3：lifecycle / rollback+concurrency / fail-closed policy；含标准 SHA256 test vectors |
| 全套 `AeroSim` Automation | PASS | Phase 0–6，32/32，ExitCode 0 |
| Cook/Stage/Package/Archive SkipZenStore | PASS | UAT BuildCookRun；Cook 实参含 `-SkipZenStore`；ExitCode 0 |
| 打包应用 NullRHI smoke | PASS | Runtime=`aerosim.runtime.phase6/v1`，正常初始化并退出，无 Fatal/Assertion |
| Editor-only 未进入 Runtime 包 | PASS | package 文件扫描无 `AeroSimEditor`/MCP/Transaction 模块文件；descriptor/依赖静态门通过 |
| token/敏感配置清理 | PASS | Config + Phase 6 package 扫描无 credential assignment |

## 自动化范围

1. inspect→dry-run→snapshot→apply→validate→evidence→commit/rollback 顺序；
2. Sandbox Map/Data Layer/`draft:*`、approved primitive/prefab、有限 action allowlist；
3. revision/hash 冲突、幂等 request、单写锁、timeout/idle expiry 与固定配额；
4. snapshot/manifest SHA256、完整 rollback、append-only hash-chain audit；
5. G0→G4 上游失败停止；固定相机 before/after 与 opening/current draft hash 绑定；
6. commit 仅 `PENDING_REVIEW_SANDBOX`，`production_touched=false`；
7. Runtime/Editor 模块隔离及危险执行面静态否定。

## 不可自动签字的退出门

Mac Editor/NullRHI 结果不能替代 Phase 6 定义的 Windows 非 Development 候选、冻结硬件 120 秒预热、30 分钟固定回放、三次冷启动、真实性能与长稳、人工正常视角 G0–G4、正式资产和用户外观确认。上述项保持 BLOCKED/NOT_RUN，不以结构化 evidence manifest 越门。
