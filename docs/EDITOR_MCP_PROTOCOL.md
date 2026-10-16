# Editor MCP Protocol v1 — Phase 6 扩展

## Envelope 与传输

请求/响应继续使用 Phase 5 冻结的 `aerosim.editor.mcp.request/v1` 与 `aerosim.editor.mcp.response/v1`。调用仅允许 Editor game thread 内同步执行；无端口、HTTP、stdio server、远程 listener 或 network client。Runtime 合同为 `aerosim.runtime.phase6/v1`。

事务写请求使用 1–64 字符 `request_id`。相同 ID 与相同请求 SHA256 返回首次响应；相同 ID 携带不同内容返回 `REQUEST_ID_REUSE`。

## 只读兼容操作

Phase 5 的 `capabilities`、heartbeat、project/world/actor/component/asset metadata/Data Layer/World Partition inspect、`validate_scene`、固定相机 `capture_request` 和 `inspect_audit` 保持可用。新增：

| operation | 结果 |
|---|---|
| `transaction_inspect` | Sandbox revision、draft SHA256、Map/Data Layer、actor count |
| `transaction_audit` | SHA256 hash-chain append-only 事务审计 |

## 事务操作

| operation | 关键 params | 前置条件 |
|---|---|---|
| `transaction_dry_run` | `transaction_id, patch` | 空闲锁；revision/hash 匹配；完整 allowlist/配额预检 |
| `transaction_snapshot` | `transaction_id` | dry-run 成功 |
| `transaction_apply` | `transaction_id` | snapshot trust anchor 存在；再次检查 revision/hash |
| `transaction_validate` | `transaction_id` | apply 成功；G0→G4 严格有序 |
| `transaction_evidence` | `transaction_id,moment,camera_id` | validate 成功；moment=before/after；固定相机 |
| `transaction_commit` | `transaction_id` | before/after 均绑定 hash；仅 pending-review Sandbox |
| `transaction_rollback` | `transaction_id,reason` | 恢复 opening snapshot 并核对 SHA256 |
| `transaction_undo/redo` | `transaction_id` | 无活动事务；仅 committed Sandbox 历史 |

## ScenePatch v1

```json
{
  "schema": "aerosim.scene-patch/v1",
  "base_revision": 0,
  "base_hash": "64-char-sha256",
  "actions": [
    {
      "type": "spawn_approved",
      "actor_id": "draft:cube-1",
      "asset_id": "primitive.cube",
      "snap": true,
      "transform": {
        "location_cm": {"x": 0, "y": 0, "z": 100},
        "rotation_deg": {"x": 0, "y": 0, "z": 0},
        "scale": {"x": 1, "y": 1, "z": 1}
      }
    }
  ]
}
```

动作仅限 `spawn_approved`、`set_transform`、`set_material_scalar`、`add_tag`、`remove_tag`、`delete_draft`。不存在函数名、类名、任意属性名、文件路径、脚本或可执行 payload。

## 状态与错误

提交状态固定为 `PENDING_REVIEW_SANDBOX`，并明确 `production_touched=false`。常见错误：`PATCH_SCHEMA_UNSUPPORTED`、`REVISION_CONFLICT`、`HASH_CONFLICT`、`REQUEST_ID_REUSE`、`TRANSACTION_LOCKED`、`TRANSACTION_EXPIRED`、`TIMEOUT`、`TARGET_NOT_ALLOWED`、`ASSET_NOT_ALLOWED`、`ACTION_NOT_ALLOWED`、`*_QUOTA_EXCEEDED`、`VALIDATION_FAILED_ROLLED_BACK`、`ROLLBACK_TRUST_FAILURE`。

固定 Evidence Camera 沿用 Phase 5 六个 ID。结构化 evidence 返回 `EDITOR_CAPTURE_REQUIRED`，不会伪造 PNG。
