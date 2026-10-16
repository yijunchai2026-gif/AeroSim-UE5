# Unity PRD → UE5 产品迁移矩阵（第一批 P0）

> 基线：旧 Unity PRD v4.0（2026-10-11）→ UE5 Runtime `aerosim.runtime.phase7/v1` + 兼容层 `aerosim.product-ux/v1`。本矩阵只记录当前代码事实；程序化机体和场景仍是原型，不代表正式资产或发布候选。

## 1. 第一批迁移范围

| Unity PRD 页面/能力 | REQ / 原意 | UE5 产品入口 | UE5 技术组件 | 本批状态 | 证据与边界 |
|---|---|---|---|---|---|
| Hangar / Aircraft Bay | REQ-UI-002、REQ-UX-001 | 启动首屏 `01 HANGAR`，右侧 Aircraft Bay，主按钮 `Enter / Space` | `AFlightHUD` Canvas 产品层、`FPhase4MissionSession::Hangar`、`FAircraftDefinition` | 已实现 P0 产品入口 | 显示 `AEROSIM GENERIC`、`PROTOTYPE / UNVERIFIED`；正式 DJI 资产仍 `BLOCKED_ASSET` |
| Preflight | REQ-UX-001/002 | `02 PREFLIGHT` 同屏检查输入、相机/FOV、环境、显示 | `FPhase4PreflightChecklist`、`AAeroSimGameMode::AdvanceMission` | 已实现 | Enter/Space 自然继续；`M` 仅保留调试兼容，不在主按钮中 |
| 明确起飞 | 新手 Happy Path | Preflight 完成后进入 `TAKEOFF`，HUD 显示下一动作 | `FPhase4MissionSession::BeginMission`、`FFlightModel`、HUD Next Action | 已实现 | 提示 `HOLD LEFT SHIFT` 至 3 m；不宣称复杂空气动力学 |
| Flight HUD | REQ-HUD-001/002 | 四周信息层，中心飞行走廊留空 | `AFlightHUD::DrawHUD`、safe margin、`FFlightHudData` | 已实现本批信息架构 | 包含阶段、目标、相机、vFOV、速度、高度、下一动作和设置入口 |
| 完整键位 | REQ-UI-001、REQ-UX-001 | Hangar/Preflight 明示，飞行底栏常驻关键键 | Enhanced Input + Pawn 明确按键绑定 | 已实现键鼠/基础手柄提示 | 键鼠：W/S、A/D、Q/E、Shift/Ctrl、C、H、R、Esc/F10；手柄完整产品化仍在后续批次 |
| Camera & Comfort / Display Settings | REQ-UI-004、FOV 事务意图 | Esc/F10/P 打开安全暂停设置；右侧面板不遮中心 | `FProductDisplaySettings`、`UGameUserSettings`、`FPhase7SettingsTransaction` | Display P0 已实现；FOV 旧事务保持 | 1280×720、1440×900、1920×1080；Windowed/Borderless/Fullscreen；Preview/Apply/Cancel/Rollback |
| 设置持久化 | PRD Preview/Apply/Cancel/Undo + 崩溃丢弃草稿 | Apply 后持久化，Preview 不落盘，Cancel 恢复，Rollback 回上一应用值 | settings schema v2 + `FPhase7SaveCodec::WriteCrashSafe` | 已实现兼容扩展 | `.tmp` flush → `.bak` → replace；启动优先 final，失败读 backup；旧 v2 缺显示字段使用安全默认值 |
| 暂停/失焦/断连安全 | REQ-UX-002 | 设置打开显示 `SAFE PAUSE / INPUT NEUTRAL`，关闭后 3 秒恢复 | `AFlightPawn::SetSafePaused`、`NeutralizeInput`、resume countdown | 保持 Phase 5/6/7 安全边界 | 失焦/断连既有安全逻辑未移除；设置 UI 不获得 RuntimeEditor 写能力 |
| Result / Replay | v3 基线、REQ-CAM-007 | Enter/Space 打开 Replay | `FPhase4MissionSession`、deterministic trace | 保持既有骨架 | 正式回放/发布证据能力不在本批虚报完成 |
| Visual Lab | REQ-UI-005 | 顶层工作区规划 | Phase 5/7 Evidence/Advisor | 非本批 | 只读建议保持 `DRAFT_NOT_USER_CONFIRMED`，不扩大写权限 |
| 正式机型/资产门 | REQ-AST-* | Hangar 状态标签 | `FAircraftDefinition`、Phase 3 gates | `BLOCKED_ASSET` | 通用程序化代理不能冒充 DJI 正式资产 |
| Windows 参考 Candidate | 平台顺序与 Candidate 门 | 发布链 | UBT/UAT/目标硬件证据 | `NOT_RUN` | Mac Development 结果不得外推 Windows |
| iPadOS/iOS | 移动独立验证 | 后续平台批次 | Device Profile/真机触控与性能 | `NOT_RUN` | 桌面缩窗不等于移动真机通过 |

## 2. 产品流程与输入所有权

```text
Launch
  → 01 HANGAR              [Enter / Space]
  → 02 PREFLIGHT           [Enter / Space]
  → TAKEOFF                [Hold Left Shift]
  → INSPECTION / RTH       [H]
  → RESULT                 [Enter / Space]
  → REPLAY

任意产品页/飞行：Esc / F10 / P → Display Settings（安全暂停、输入清零）
设置：↑↓ 导航，←→ 改值，Enter/Space 执行，Esc/F10 Cancel 并关闭
M：仅调试兼容快捷键，不属于用户 Happy Path。
```

## 3. Schema 与兼容性

- Runtime 主合同保持 `aerosim.runtime.phase7/v1`，不破坏 Phase 7 Automation、release manifest 或 Phase 5/6 Editor 安全边界。
- 新增 `aerosim.product-ux/v1`，负责 Hangar/Preflight/Flight/Display Settings 的产品交互语义。
- `aerosim.settings/v2` 兼容新增 `resolution_width`、`resolution_height`、`window_mode`；旧 v1 迁移和旧 v2 文件仍可读。
- Display Preview 只作用于当前运行窗口；Apply 才同时确认 UE GameUserSettings 与 crash-safe JSON；Cancel 恢复 opening/applied 状态；Rollback 恢复上一应用值并再次安全保存。

## 4. 自动化与证据要求

- C++ Automation：`AeroSim.ProductUX.Flow.*`、`AeroSim.ProductUX.Display.*`、`AeroSim.ProductUX.Input.*`。
- 静态/Python：`Scripts/product_ux_static_validate.py`、`Scripts/tests/test_product_ux_contracts.py`。
- 产品截图只证明当前 Mac Development 程序化原型 UX，不证明正式资产、Windows、iPadOS、目标硬件性能或用户外观确认。
- 打包、截图和验证状态见 `docs/PRODUCT_UX_VALIDATION.md`；未执行项保持 `NOT_RUN` 或既有 Blocked 状态。

## 5. 后续迁移批次

1. **P0-2：完整 CommonUI/UMG 产品层**：鼠标/手柄焦点环、可访问性缩放、控件语义、设置确认倒计时。
2. **P0-3：完整 Hangar Fleet/Quality Rail**：正式资产接入后 G1–G8、LOD/Collision/证据状态，不越过用户确认门。
3. **P0-4：Camera & Comfort / Visual Lab**：六相机能力状态、FOV 完整页、A/B/性能/捕获 UI。
4. **P0-5：Result/Replay/Records**：与 build、环境、相机 snapshot 对齐的完整产品页。
5. **平台批次**：Windows 冻结硬件非 Development Candidate → macOS 差异收敛 → iPadOS/iOS 真机；当前不得写已完成。


## 6. 旧文档逐项 UE5 状态

原文副本与索引位于 [`docs/legacy-unity/`](legacy-unity/README.md)。以下状态是迁移处理状态，不是旧文档所述功能的发布完成声明。

| 旧 Unity 原文 | UE5 当前状态 | 已迁移/消费 | 未完成或不可外推 |
|---|---|---|---|
| `AGENTS.md` | **已归档 / 部分消费** | 证据纪律、禁止虚报、资产/用户门意图进入新仓规则 | 旧仓指令不覆盖新仓 AGENTS、当前代码或用户最新指令 |
| `飞行模拟器产品设计文档.md` | **P0-1 部分实现** | Hangar→Preflight→Takeoff、HUD、Display 事务、资产真实性标签、G0–G4 语义 | 完整页面、正式资产、Visual Lab、Windows/iPad 和 RC 门未完成 |
| `DEVELOPMENT_PLAN.md` | **已归档 / 重映射** | 依赖和阶段意图映射到 Phase 0–7 与 15 条虚拟职责泳道 | 旧 Unity 排期和完成度不沿用 |
| `ART_DIRECTION_V2.md` | **输入已登记 / 内容未完成** | 视觉语言和资产门进入 PRODUCT_DESIGN/场景门 | 程序化代理不等于正式美术；仍 `BLOCKED_ASSET` |
| `docs/V125_PRD_CONFLICT_REVIEW.md` | **已消费** | 相机/FOV/资产命名冲突进入 schema 与禁止项 | 未关闭的正式内容冲突仍保持 Blocked/后续批次 |
| `docs/V125_PRD_REORGANIZATION_QA.md` | **已消费** | REQ、证据、门禁和事实/声明分离进入迁移矩阵 | 不把旧 QA 结果复用为 UE5 PASS |
| `docs/V125_UX_FLOW_AUDIT.md` | **P0-1 部分实现** | Enter/Space 主路径、显式 Preflight/Takeoff、错误恢复方向 | 完整可用性研究、P90 数据和所有设备仍未执行 |
| `docs/V125_UX_UI_OPTIMIZATION_RESEARCH.md` | **设计输入 / 部分实现** | 外围 HUD、中心视野留空、清晰主动作、状态文案 | 完整 CommonUI/UMG、鼠标/手柄焦点与高级页面在 P0-2+ |
| `docs/V125_UX_UI_RESEARCH_QA.md` | **验收输入 / 未作用户结论** | 静态/Automation 覆盖主流程、事务、持久化和提示 | 人工可用性、舒适性、用户签字仍 `NOT_RUN/BLOCKED` |
| `docs/V125_UI_MOBILE_AUDIT.md` | **已归档 / 平台未运行** | 安全区和响应式要求进入后续计划 | iPadOS/iOS 真机仍 `NOT_RUN`；桌面缩窗不替代真机 |
| `docs/V1_SCOPE_REVIEW.md` | **已重映射** | 首个 Pacific Coast vertical slice 和非目标进入新仓范围 | 旧范围与最新用户指令冲突时以后者为准 |
| `docs/V1_SCREEN_SPEC.md` | **P0-1 部分实现** | Hangar、Preflight、Flight HUD、Display Settings 首批屏幕 | Result/Replay/Visual Lab/完整响应式仍后续迁移 |
| `docs/V1_TECHNICAL_PLAN.md` | **意图迁移 / 技术不直译** | 数据合同、事务、证据和测试意图进入 UE5 C++/Canvas/SaveCodec | Unity API、组件和平台结论不直接移植或声称完成 |

所有原文保持不改写；新增 README 与本矩阵只提供来源、优先级和 UE5 状态。团队配置统一以 `PMO-01 + RD-01..10 + QA-01..03 + UI-01` 为准，UX 由 UI-01 与 PMO-01 共担。
