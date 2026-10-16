# Phase 5 Editor MCP 威胁模型

## 资产与信任边界

受保护资产包括正式地图/资产、宿主文件系统、凭据、Editor 进程、Runtime 包和审计证据。调用方及其 JSON 均不可信；当前 Editor World、Asset Registry 和编译进插件代码属于进程内受信边界，但其输出仍受大小与超时限制。

## 威胁与控制

| 威胁 | 控制 |
|---|---|
| 协议降级/混淆 | schema 与整数 version 精确匹配；未知版本 fail closed |
| 重放/串线 | 强制 request ID 并原样关联响应/审计；v1 不宣称远端身份认证 |
| 路径遍历/宿主文件读取 | 仅 `/Game/` object path；拒绝 `..`、反斜线、冒号；不提供文件 API |
| 任意 UObject/反射调用 | 无函数名/属性名反射参数；只调用编译时固定 getter；Actor/Component 从当前 World 枚举匹配 |
| 资产加载副作用 | 只用 Asset Registry 元数据；不存在时返回错误，不加载对象 |
| RCE | 无 Shell、Python、console、进程、脚本入口；项目禁用 Python 与 Editor Scripting 插件 |
| 网络外泄 | 无 listener/client/network 依赖或操作 |
| 场景/资产 mutation | mutation-like 操作先于 dispatch 统一返回 `READ_ONLY_DENIED` |
| DoS/数据放大 | 16 KiB 请求、64 KiB 响应、100 条分页、1–2000 ms 超时、game-thread 限制 |
| 审计泄密/覆盖 | 只存摘要和最小字段；进程内 append-only；不写外部敏感目录；不暴露 clear/delete |
| 伪造视觉证据 | Capture 仅生成固定相机请求，明确 `NOT_RUN/PENDING_CAPTURE`；G2/G3 不自动 PASS |
| Runtime 污染 | `.uplugin` 模块 Type=Editor，uproject TargetAllowList=Editor，Runtime Build.cs 无插件依赖 |

## 残余风险

- v1 是进程内 API，不提供跨进程认证、签名或加密传输；不得直接包装为公网服务。
- 审计随 Editor 进程退出而消失。持久化审计需要独立批准的安全 sink，不能在本阶段私自添加文件写入。
- timeout 是协作式截止时间检查，不能抢占 UE 内部同步 getter；因此只允许有界枚举和固定 getter。
- G0/G1 自动结果是辅助证据，不能替代人工场景判断；正式资产仍受 `BLOCKED_ASSET` 门禁。

## Phase 6 边界

本阶段没有 preview/snapshot/apply/rollback、Transform、Gizmo、Snap、Undo/Redo 或 ScenePatch 写入。若未来开放，必须使用独立 Command API、事务、乐观并发、草稿 allowlist、自动回滚与人工批准，不得扩展本只读 dispatcher 形成旁路。
