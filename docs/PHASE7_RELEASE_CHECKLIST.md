# Phase 7 Release Checklist

状态定义：`PASS / FAIL / BLOCKED / NOT_RUN`。Development 可作为 Mac 工程候选，不可作为正式发布性能结论。

| 门 | 预期 | 当前状态 |
|---|---|---|
| Runtime | `aerosim.runtime.phase7/v1` | PENDING_VALIDATION |
| Phase 0–7 static/Python | 全回归 | PENDING_VALIDATION |
| Mac Development compile | UE 5.8 | PENDING_VALIDATION |
| Automation | `AeroSim.Phase7` 与 `AeroSim` | PENDING_VALIDATION |
| Cook/Package | Mac Development、SkipZenStore | PENDING_VALIDATION |
| Packaged NullRHI smoke | 新 package、退出码 0 | PENDING_VALIDATION |
| Packaged Metal launch | 新 package、真实 Metal 初始化 | PENDING_VALIDATION |
| 30 分钟 wall-clock soak | 固定 replay、同 digest、无 crash | PENDING_VALIDATION |
| 产品设置/迁移/回滚 | Automation | PENDING_VALIDATION |
| 日志脱敏 | Automation + 静态扫描 | PENDING_VALIDATION |
| Editor MCP Phase 6 安全 | Editor-only、事务与 rollback 不回退 | PENDING_VALIDATION |
| 正式机型资产 | 来源、许可、版本、SHA256 | **BLOCKED_ASSET** |
| 用户逐机外观确认 | 机型+版本+哈希 | **BLOCKED_USER_CONFIRMATION** |
| Windows 1440p High | 非 Development、冻结硬件 | **NOT_RUN** |
| iPadOS | 真机触控/安全区/热/内存/RHI | **NOT_RUN** |
| iOS | iPadOS 之后独立真机 | **NOT_RUN** |
| 正式性能门 | CPU/GPU/frame/memory/draw/streaming | **BLOCKED_TARGET_HARDWARE_EVIDENCE** |
| 最终产品截图/UX | 正式资产、同 build、用户确认 | **BLOCKED_USER_CONFIRMATION** |

## 发布 manifest

`Scripts/phase7_release_manifest.py` 为本轮文件、资产来源记录和 manifest 自身生成 SHA256；`source_dirty=true` 是当前禁止 commit 下的预期事实。manifest 不会把未执行平台改写为通过。

## 回滚

- 质量档：Mac/iPad 的 Nanite 默认 Off；Windows High 可回退至 Off/Low。
- 设置：Cancel 恢复 opening snapshot；Apply 后允许一次 rollback；坏存档保留上一份 `.bak`。
- AI 建议：默认不应用；若转 ScenePatch，沿用 Phase 6 完整 rollback、单写锁、乐观并发和 G0→G4。
