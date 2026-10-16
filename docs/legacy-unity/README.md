# 旧 Unity 产品文档原文索引

> **定位：历史产品意图与迁移输入，不覆盖 UE5 当前事实。**
>
> 本目录保存从旧 Unity 仓库逐字节复制的直接相关原始文档，仅供需求追溯、冲突审查和迁移映射。UE5 当前实现、平台状态、验证结果与阻塞事实以新仓 `docs/PRODUCT_DESIGN.md`、`docs/ARCHITECTURE.md`、`docs/PHASES.md`、当前阶段验证文档和代码为准。旧文档中的 Unity 已完成状态、团队配置、截图、平台结论或目标值不得直接写成 UE5 PASS。

## 收录规则

- 收录：旧仓顶层指令、唯一 PRD、开发/美术方向、PRD 冲突与重组审查、UX/UI 审计、范围/屏幕/技术方案。
- 不收录：生成页面（HTML）、运行产物、测试日志、截图/视频证据、缓存、内部 skill、与产品迁移无直接关系的实现副产物。
- 原文文件不做改写；本 README 和新仓迁移矩阵是唯一新增索引/状态层。

## 原文清单与 UE5 使用方式

| 原文（旧仓相对路径） | 新仓原文副本 | 类型 | UE5 使用方式 |
|---|---|---|---|
| `AGENTS.md` | [AGENTS.md](AGENTS.md) | 旧仓顶层协作指令 | 仅作历史约束输入；新仓 AGENTS 与用户最新指令优先 |
| `飞行模拟器产品设计文档.md` | [飞行模拟器产品设计文档.md](飞行模拟器产品设计文档.md) | Unity v4.0 唯一 PRD | 页面、Happy Path、相机/FOV、资产门、G0–G4 与 QA 意图主来源 |
| `DEVELOPMENT_PLAN.md` | [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) | 旧开发计划 | 用于排期/依赖追溯，不继承 Unity 完成状态 |
| `ART_DIRECTION_V2.md` | [ART_DIRECTION_V2.md](ART_DIRECTION_V2.md) | 美术顶层方向 | 用于视觉意图和资产门，不证明 UE5 正式资产完成 |
| `docs/V125_PRD_CONFLICT_REVIEW.md` | [docs/V125_PRD_CONFLICT_REVIEW.md](docs/V125_PRD_CONFLICT_REVIEW.md) | PRD 冲突审查 | 用于识别语义冲突和禁止越门项 |
| `docs/V125_PRD_REORGANIZATION_QA.md` | [docs/V125_PRD_REORGANIZATION_QA.md](docs/V125_PRD_REORGANIZATION_QA.md) | PRD 重组 QA | 用于需求可追溯性和验收结构 |
| `docs/V125_UX_FLOW_AUDIT.md` | [docs/V125_UX_FLOW_AUDIT.md](docs/V125_UX_FLOW_AUDIT.md) | UX 流程审计 | 用于 Hangar→Preflight→Flight 主路径迁移 |
| `docs/V125_UX_UI_OPTIMIZATION_RESEARCH.md` | [docs/V125_UX_UI_OPTIMIZATION_RESEARCH.md](docs/V125_UX_UI_OPTIMIZATION_RESEARCH.md) | UX/UI 研究 | 用于信息架构、可读性和交互候选，不直接视为实现 |
| `docs/V125_UX_UI_RESEARCH_QA.md` | [docs/V125_UX_UI_RESEARCH_QA.md](docs/V125_UX_UI_RESEARCH_QA.md) | UX/UI 研究 QA | 用于研究结论的证据边界 |
| `docs/V125_UI_MOBILE_AUDIT.md` | [docs/V125_UI_MOBILE_AUDIT.md](docs/V125_UI_MOBILE_AUDIT.md) | 移动 UI 审计 | 仅作后续 iPadOS/iOS 输入；当前真机仍 NOT_RUN |
| `docs/V1_SCOPE_REVIEW.md` | [docs/V1_SCOPE_REVIEW.md](docs/V1_SCOPE_REVIEW.md) | v1 范围审查 | 用于 scope lineage；由用户最新范围覆盖冲突项 |
| `docs/V1_SCREEN_SPEC.md` | [docs/V1_SCREEN_SPEC.md](docs/V1_SCREEN_SPEC.md) | 页面规格 | 用于屏幕/响应式映射；当前 Canvas 实现只覆盖首批 P0 |
| `docs/V1_TECHNICAL_PLAN.md` | [docs/V1_TECHNICAL_PLAN.md](docs/V1_TECHNICAL_PLAN.md) | Unity 技术方案 | 只迁移产品意图；Unity API/架构不直接移植 |

逐文档 UE5 状态见 [Unity PRD → UE5 产品迁移矩阵](../UNITY_TO_UE5_MIGRATION_MATRIX.md#6-旧文档逐项-ue5-状态)。
