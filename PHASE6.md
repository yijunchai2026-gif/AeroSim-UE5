# AeroSim UE5 Phase 6 — 受控 AI 场景编辑事务

## 1. 范围与结论

Phase 6 将 Phase 5 的进程内 Editor MCP v1 扩展为同一 Command API 下的受控 Sandbox 事务编辑。Runtime 身份为 `aerosim.runtime.phase6/v1`；实现仅位于 `AeroSimEditor` Editor 模块，不进入 Cook/Package Runtime。

本阶段只产出 `PENDING_REVIEW_SANDBOX / DRAFT_NOT_USER_CONFIRMED` 草稿，不提供 production promotion，不代签用户外观、人工 G0–G4 或 Windows 候选退出门，不实现 Phase 7 的 AI 布局、光照、LOD/性能优化建议。

## 2. 固定事务顺序

`transaction_inspect → transaction_dry_run → transaction_snapshot → transaction_apply → transaction_validate → transaction_evidence(before/after) → transaction_commit | transaction_rollback`

- 写锁：进程内单事务写锁；超时或 idle expiry 自动完整恢复 opening state。
- 乐观并发：`base_revision + base_hash(SHA256)` 必须同时匹配。
- 幂等：事务 MCP 的 `request_id + request SHA256` 缓存响应；同 ID 不同内容 fail closed。
- trust anchor：snapshot manifest、草稿状态、evidence manifest 和 pending-review manifest 均使用 SHA256。
- 验证：固定 G0→G4 顺序；上游失败后续标记 `SKIPPED_UPSTREAM_FAILURE`，并自动回滚。
- 证据：before/after 都绑定固定相机 ID、对应草稿 hash 和 snapshot hash；结构化 manifest 不伪装成截图，返回 `EDITOR_CAPTURE_REQUIRED`。
- commit：只生成 pending-review Sandbox manifest，不写 production，不是 Git commit。

## 3. 写入 allowlist

- Map：`/Game/AeroSim/Sandbox/Maps/AI_Review_Sandbox`
- Data Layer：`AeroSim_AI_Draft`
- Actor ID：仅 `draft:*`
- spawn：`primitive.cube/sphere/cylinder`、`prefab.review-marker/route-gate`
- 动作：`spawn_approved`、`set_transform`（Gizmo/Snap）、`set_material_scalar`（固定参数）、`add_tag/remove_tag`（仅 `draft.*`）、`delete_draft`
- Undo/Redo：只作用于已 commit 的 Sandbox 草稿状态。

禁止 shell、Python、console、进程、网络、任意文件、任意反射/UObject 调用、任意资产路径、正式地图/资产修改、import、promote。

## 4. 配额

每 patch 最多 64 动作；最多 256 draft actors；每 actor 最多 16 tags、8 material scalars；事务执行 deadline 2 秒，idle 30 秒；MCP 保持 16 KiB 请求、64 KiB 响应、100 条分页限制。

## 5. 验证入口

```bash
python3 Scripts/phase6_static_validate.py
python3 -m unittest discover -s Scripts/tests -v
```

UE Automation：`AeroSim.Phase6`；完整回归：`AeroSim`。编译、Automation、Cook/Package、NullRHI smoke 与残余阻塞见 `docs/PHASE6_VALIDATION.md`。
