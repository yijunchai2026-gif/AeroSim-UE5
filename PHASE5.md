# AeroSim UE5 Phase 5 — Editor MCP 只读观测协议

## 1. 结论与范围

Phase 5 在不开放任何场景写操作的前提下，将既有 `AeroSimEditor` heartbeat 扩展为进程内、结构化、可发现、可审计的 Editor MCP 只读协议。Runtime 身份升级为 `aerosim.runtime.phase5/v1`；Editor MCP 仍只存在于 `AeroSimEditor` 的 `Editor` 模块，打包 Runtime 不依赖也不包含该模块。

本实现遵循本轮更严格的用户边界：**不实现 Phase 6 写能力，也不开放 Phase 5 Transform**。所有疑似 mutation，包括 spawn/delete/set/apply/transform/save/import/脚本/console，统一返回 `READ_ONLY_DENIED`。

## 2. 交付

- 复用 heartbeat；增加 project、world/map、actor、component、asset metadata、Data Layer、World Partition 检查；
- `validate_scene` 依序报告 G0–G4；自动检查不能替代 G2/G3 人工正常视角与 G4 工程证据；
- `capture_request` 只接受 Phase 3 六个固定 Evidence Camera ID，生成可测试 manifest；不会伪造截图或写文件；
- request/response schema、版本、request ID、错误码、capability discovery；
- `/Game/` object path allowlist；16 KiB 请求、64 KiB 响应、最大 100 条分页、最大 2 秒超时；
- 进程内 append-only 审计，仅记录 UTC、request ID、operation、result、耗时与请求摘要，不记录原始请求，不写外部位置；
- 禁用项目中的 Python 与 Editor Scripting 插件入口；协议无网络 listener。

## 3. 明确拒绝

Shell、Python、console command、进程执行、网络访问、任意文件访问、任意反射、任意 UObject 调用、对象加载以及所有 mutation 均不在协议能力内。Asset 检查只读 Asset Registry 元数据，不加载对象；Actor/Component 只能从当前 Editor World 枚举并精确匹配稳定 ID。

## 4. 验证入口

```bash
python3 Scripts/phase5_static_validate.py
python3 -m unittest discover -s Scripts/tests -v
```

UE Automation：`AeroSim.Phase5`；完整回归：`AeroSim`。编译、Automation、Cook/Package 与 NullRHI smoke 的实际结果见 `docs/PHASE5_VALIDATION.md`。
