# Editor MCP Protocol v1

## Envelope

请求 schema 固定为 `aerosim.editor.mcp.request/v1`：

```json
{
  "schema": "aerosim.editor.mcp.request/v1",
  "version": 1,
  "request_id": "client-0001",
  "operation": "inspect_world",
  "timeout_ms": 250,
  "params": {}
}
```

`request_id` 为 1–64 个 ASCII 字母、数字、点、短横线或下划线。响应固定为 `aerosim.editor.mcp.response/v1`，成功含 `data`，失败含 `{code,message,retryable}`。调用仅允许 Editor game thread 内同步执行，不提供端口、HTTP、stdio server 或远程 listener。

## Capability discovery

`capabilities` 返回 `aerosim.editor.mcp.capabilities/v1`、`aerosim.runtime.phase5/v1`、read-only 标志、操作清单、拒绝面、路径 allowlist、大小/分页/超时限制以及 audit 存储模型。

## 只读操作

| operation | params | 结果 |
|---|---|---|
| `capabilities` | `{}` | 能力和安全限制 |
| `heartbeat` | `{}` | 复用既有模块健康状态 |
| `inspect_project` | `{}` | 项目、引擎、Editor 与 Runtime 合同 |
| `inspect_world` | `{}` | 当前 map/world、类型、Actor 数、WP 状态 |
| `list_actors` | `cursor, limit` | 最大 100 条的 Actor 摘要页 |
| `inspect_actor` | `actor_id` | 固定属性与组件清单 |
| `inspect_component` | `actor_id, component_id` | 固定组件属性；无反射字段参数 |
| `inspect_asset_metadata` | `asset_path` | `/Game/` 下已索引且不加载对象的元数据 |
| `inspect_data_layers` | `{}` | 当前已加载 Actor 所引用 Data Layer |
| `inspect_world_partition` | `{}` | WP/streaming 只读状态 |
| `validate_scene` | `{}` | G0→G4 顺序结果 |
| `capture_request` | `camera_id,width,height` | 固定相机请求 manifest，不写截图 |
| `inspect_audit` | `offset,limit` | 脱敏进程内审计页 |

固定相机 ID：`aircraft-three-quarter`、`fpv`、`gimbal-down`、`near-follow`、`far-follow`、`scale-support`。

## 主要错误码

`INVALID_JSON`、`SCHEMA_UNSUPPORTED`、`INVALID_REQUEST_ID`、`INVALID_OPERATION`、`OPERATION_NOT_SUPPORTED`、`READ_ONLY_DENIED`、`PATH_NOT_ALLOWED`、`ACTOR_NOT_FOUND`、`COMPONENT_NOT_FOUND`、`ASSET_NOT_FOUND`、`CAMERA_NOT_ALLOWED`、`EDITOR_WORLD_UNAVAILABLE`、`EDITOR_THREAD_REQUIRED`、`REQUEST_TOO_LARGE`、`RESPONSE_TOO_LARGE`、`TIMEOUT`。

## Gate 与 Capture 语义

G0/G1 自动检查仅覆盖存在性和有限变换；G2/G3 必须是 `PENDING_MANUAL_REVIEW`，G4 必须等待真实 build/package/performance 证据。Capture 返回 `EDITOR_CAPTURE_REQUIRED`、`capture_result=NOT_RUN` 和 `artifact_sha256=PENDING_CAPTURE`，禁止把结构化请求误报成真实截图。
