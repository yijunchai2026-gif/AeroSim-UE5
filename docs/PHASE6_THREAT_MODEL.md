# Phase 6 受控 AI 场景编辑威胁模型

## 资产与信任边界

受保护资产：production 地图/资产、宿主文件系统与凭据、Editor 进程、Runtime 包、草稿状态、snapshot、evidence、audit。调用方 JSON、request ID、ScenePatch、revision/hash 均不可信。唯一写边界是编译期固定的 Sandbox Map + Data Layer + `draft:*` 状态模型。

## 威胁与控制

| 威胁 | 控制 |
|---|---|
| 协议/patch 降级 | envelope 与 ScenePatch schema 精确匹配；未知版本 fail closed |
| 重放或 request 串线 | 写请求 `request_id + request SHA256` 幂等；同 ID 不同摘要拒绝 |
| lost update/并发覆盖 | `base_revision + base_hash` 双前置；单事务写锁 |
| 半应用/崩溃/超时 | apply 前 snapshot；2 秒 deadline、30 秒 idle expiry；失败默认完整恢复 opening state |
| snapshot/evidence 篡改 | canonical draft、snapshot manifest、evidence manifest、commit manifest 全部 SHA256；rollback 后复核 opening hash |
| 越权 production 写入 | 编译期 Map/Data Layer/`draft:*` allowlist；无 promote；commit 固定 pending-review |
| 任意资产/属性修改 | 5 个 approved spawn ID；6 个 material scalar ID；有限 action enum；无路径式写 API |
| RCE | 无 shell、Python、console、process、脚本、import；项目禁用 Python/Editor Scripting |
| 任意 UObject/反射 | 无函数/类/属性反射参数；事务状态机不调用任意 UObject |
| 文件/网络外泄 | 无文件 API、listener、client、socket、HTTP 或外部 sink |
| DoS | request/response/page/patch/actor/tag/material 配额；同步 deadline；bounded idempotency cache |
| 审计删除/改写 | API 仅 inspect；append-only SHA256 hash chain；无 clear/delete |
| 伪造视觉证据 | 只接受固定相机；before/after 分别绑定 opening/current draft hash；结构化 manifest 明示 capture required |
| 跳过视觉门 | G0→G4 固定顺序；上游失败停止下游并回滚；自动检查不等于人工签字 |
| Runtime 污染 | 插件 module `Type=Editor`、uproject `TargetAllowList=Editor`、Runtime Build.cs 无依赖 |

## 回滚不变量

rollback、timeout、idle expiry、validation failure 均恢复 opening state（revision、actors、transforms、material scalars、tags），并比较 canonical SHA256。trust anchor 不匹配时返回 `ROLLBACK_TRUST_FAILURE`，不得 commit。

## 残余风险与明确非目标

- 当前状态、审计和 pending-review manifest 为 Editor 进程内模型；未宣称跨进程 durability。持久化 sink 需单独安全设计与批准。
- evidence manifest 不是截图；人工固定视角捕获仍待受控 runner。
- 未提供 production promotion、用户确认、正式资产交付、Windows 冻结硬件性能/长稳，因此 Phase 6 产品退出门仍有阻塞。
- 不实现 Phase 7 AI 自动布局、光照、LOD 或性能产品化建议。
