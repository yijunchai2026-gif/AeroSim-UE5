# AeroSim-UE5 团队执行计划

## 1. 虚拟职责泳道口径

本项目固定使用 **15 条虚拟职责泳道**：`PMO-01`、`RD-01..10`、`QA-01..03`、`UI-01`。它们用于拆分责任、评审、文件所有权和交接，**不代表 15 位已确认真人**，也不生成姓名、账号、工时或到岗承诺。一个人可以承担多条泳道，同一泳道可分阶段换人，但 owner 标识和交接记录必须保留。

- `PMO-01`：维护产品语义、阶段状态、依赖、范围、风险、证据、用户门和冲突裁决；与 UI-01 共同承担 UX，不代替技术、QA、视觉或用户签字。
- `RD-01..10`：覆盖工程、资产、飞行、相机、输入、UI Runtime、场景、内容、证据和 Runtime Editor。
- `QA-01..03`：覆盖合同/自动化、体验/视觉、迁移/平台/发布三大质量域。
- `UI-01`：统一负责 Design System、全部页面、响应式、可访问性与交互规格；与 PMO-01 共同承担信息架构、主路径、文案、研究和错误恢复职责。

旧的多 QA、多 UI 和独立 UX 配置已删除并合并；不存在独立 UX 泳道。

## 2. 长期泳道

### PMO（1）

| 泳道 | 责任 | 主要接口 |
|---|---|---|
| PMO-01 | 产品事实源、范围/阶段、依赖与风险、用户门、UX 共同 owner、最终状态裁决 | 全部泳道；UI-01 为 UX 协作 owner |

### RD（10）

| 泳道 | 责任 | 主要接口 |
|---|---|---|
| RD-01 | 工程基线、Build ID、ADR、CI/证据骨架 | 全部泳道前置 |
| RD-02 | 机型身份、DCC/导入门、资产 manifest | RD-03/05/06 |
| RD-03 | 飞行 Simulation 与碰撞 | RD-04/08/QA |
| RD-04 | 相机、FOV、回放 snapshot | RD-03/05/UI-01 |
| RD-05 | Enhanced Input、安全暂停、平台设备 | RD-03/04/UI-01 |
| RD-06 | UI Runtime、状态绑定、设置与保存 | UI-01/PMO-01/RD-04 |
| RD-07 | World、Environment State、流送和天气 | RD-08/09 |
| RD-08 | Pacific Coast 内容集成、音画 | RD-03/07/09 |
| RD-09 | Visual Lab、性能、Evidence | QA/RD-07/08 |
| RD-10 | Runtime Editor、MCP、事务与安全 | RD-01/07/09 |

### QA（3）

- **QA-01 — 工程与合同质量**：静态合同、编译、Automation、回归、输入安全与 schema 兼容。
- **QA-02 — 产品体验与视觉质量**：Hangar/Preflight/Flight/Settings、相机/FOV、可访问性、G0–G4、固定镜头和正常路线。
- **QA-03 — 迁移、平台与发布质量**：资产身份/用户确认链、旧文档迁移完整性、Cook/Stage/Package、目标平台、性能、内存和长稳。

### UI（1，含 UX 共担）

- **UI-01 — Product UI/UX**：Design System/tokens；Hangar、Preflight、Flight HUD、Camera/FOV、Pause/Settings、Result/Replay、Visual Lab、Runtime Editor；响应式、可访问性、平台适配、信息架构、主路径、文案、错误恢复、研究方案和验收入口。
- UX 决策由 **UI-01 + PMO-01** 共担：UI-01 对交互/视觉规格负责，PMO-01 对产品语义、范围和用户门负责；二者均不能覆盖 QA FAIL 或用户确认门。

## 3. Phase 0–7 执行矩阵

表中的 Owner 是虚拟职责泳道，不是虚构个人。

| Phase | Owner（主/协） | 输入 | 输出 | DoD | 依赖 | 主要冲突与裁决 |
|---|---|---|---|---|---|---|
| **0 事实冻结** | PMO-01；RD-01/02/07/09；QA-01/03；UI-01 | 用户最新指令、旧仓 PRD/门禁/资产清单、本机工具事实 | UE 版本与平台 ADR、目录/模块图、切片合同、资产/证据 schema、风险表 | `PHASE0_ACCEPTANCE` 签出；工具缺失明确 Blocked；不把旧仓结果写成 UE PASS | 无 | 工程由 spike+ADR 裁决；产品语义由用户最新指令和 PMO-01 裁决；UX 与 UI-01 共审 |
| **1 工程与可飞灰盒** | RD-01/03/05/07；QA-01/02；UI-01 | 冻结合同、灰盒路线、输入动作表 | 可启动工程、灰盒、Pawn、输入中和、最小 HUD、自动化 | 对应平台编译/Cook/smoke；主路线可运行；失焦/暂停四轴清零 | P0 | Action 属 RD-05，飞行动力属 RD-03；UI-01 不持有飞行规则 |
| **2 飞行/相机切片** | RD-03/04/05/06；QA-01/02；UI-01；PMO-01 | 灰盒、Camera/FOV schema、HUD token | Nose、Near/Far、FOV 事务、飞前、暂停/恢复、结果/回放骨架 | 主路径端到端；相机/FOV/安全区/中断矩阵通过 | P1；RD-02 socket | Camera Service 是真值；UI-01 只编辑事务；主路径由 UI-01/PMO-01 共审 |
| **3 资产/机库门** | RD-02/04/06；QA-02/03；UI-01；PMO-01 | 资产包、来源/哈希、socket、机库规格 | 导入检查、审核棚、Hangar/Quality Rail、逐机确认 | 正式机工程门和用户门均通过；缺内容 fail closed | P0/P2 | 工程、视觉、用户确认互不代签；PMO-01 不允许越门 |
| **4 海岸内容与只读 MCP** | RD-07/08/09/10；QA-02；UI-01；PMO-01 | 路线、Environment State、资产、G0–G4 | 英雄区、天气、音画、固定镜头、只读 inspect/capture | G0→G3 顺序；同 revision；MCP 无写权限 | P2/P3 | QA-02 的 G0–G2 FAIL 优先于 LookDev 评分 |
| **5 Visual Lab 与受控编辑** | RD-09/10/07；QA-01/02/03；UI-01 | 场景、预算、Command API、Sandbox | A/B/捕获/Profiler、受控 Transform、rollback | 草稿限定；事务闭环；目标硬件初测 | P4 | UI 与 MCP 复用命令；采集不自动写 PASS |
| **6 收敛候选** | PMO-01；RD-01..10；QA-01..03；UI-01 | 完整切片、缺陷/证据、用户确认 | 事务编辑器、候选、回归与长稳包 | P0=0；G0–G4；Windows 非开发候选和同 build 证据 | P0–P5 | 只修缺陷；P0 重开回退；豁免不覆盖用户门 |
| **7 AI/平台扩展/复制** | PMO-01；RD-01/07/09/10；QA-03；UI-01 | 已验收候选、事务、平台预算、成本 | 受控建议、平台证据、复制决策 | AI 保持草稿至人工/用户门；各平台独立声明 | P6 | 平台不破坏 Windows 基线；用户/证据优先并可回滚 |

## 4. 依赖与并行

主链为 `P0 → P1 → P2 → P3 → P4 → P5 → P6 → P7`。阶段内可以并行，但不得提前宣称完成：

- P1 工程骨架、输入、灰盒可并行，公共动作/坐标合同先冻结。
- P3 等待资产时，系统和场景可使用清楚标记的通用代理；正式任务门保持关闭。
- P4 内容与 P5 工具只在独立 Sandbox 并行；工具不得写正式地图。
- 所有页面与共享 token 由 UI-01 统一 owner，避免原多 UI 泳道并改；产品/UX 变更由 UI-01 与 PMO-01 双向评审。
- QA 从 Phase 0 参与：QA-01 守工程合同，QA-02 守产品/视觉，QA-03 守迁移/平台/发布。

## 5. 冲突管理

1. **单一 owner**：公共 schema、Input Actions、UI tokens、Environment State 各只有一个写 owner。
2. **先接口后并行**：跨泳道先更新 ADR/接口，不复制定义。
3. **短租约**：共享地图、Data Asset、Build 配置按任务登记，结束即释放。
4. **集成列车**：数据合同 → 核心逻辑 → UI → 内容 → 测试；失败退回 owner。
5. **裁决顺序**：安全/用户门 > 产品语义 > G0–G4 > 工程便利 > 排期。PMO-01 记录裁决，不代签技术/QA/用户结论。

## 6. 状态与报告

每项记录 Phase、泳道 owner、输入版本、输出路径、DoD、依赖、风险、验证状态、Evidence ID 和用户确认。报告只使用 Done、In Review、Blocked、Not Run；没有 UE、资产、设备或用户确认时写清阻塞对象、可并行事项和解除条件。
