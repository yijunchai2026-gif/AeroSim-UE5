# AeroSim-UE5 团队执行计划

## 1. 人力口径

`1 PMO、10 RD、5 QA、10 UI、2 UX` 是 **27 条职责泳道**，用于并行拆分、评审和文件所有权，不代表 27 位已确认真人，也不应生成姓名、账号、工时或到岗承诺。实际执行时，一个人可以承担多条泳道；同一泳道也可分阶段换人，但 owner 标识和交接记录必须保留。

PMO 维护阶段状态、依赖、风险、证据和冲突裁决，不代替技术/视觉/用户签字。

## 2. 长期泳道

### RD（10）

| 泳道 | 责任 | 主要接口 |
|---|---|---|
| RD-01 | 工程基线、Build ID、ADR、CI/证据骨架 | 全部泳道前置 |
| RD-02 | 机型身份、DCC/导入门、资产 manifest | RD-03/05/06 |
| RD-03 | 飞行 Simulation 与碰撞 | RD-04/08/QA |
| RD-04 | 相机、FOV、回放 snapshot | RD-03/05/UI |
| RD-05 | Enhanced Input、安全暂停、平台设备 | RD-03/04/UI |
| RD-06 | UI 运行时框架、状态绑定、保存 | UI/UX/RD-04 |
| RD-07 | 世界、Environment State、流送和天气 | RD-08/09 |
| RD-08 | Pacific Coast 内容集成、音画 | RD-03/07/09 |
| RD-09 | Visual Lab、性能、Evidence | QA/RD-07/08 |
| RD-10 | Runtime Editor、MCP、事务与安全 | RD-01/07/09 |

### QA（5）

- QA-01：合同、编译、Automation 与回归。
- QA-02：飞行/输入/相机、超宽和设备中断。
- QA-03：资产身份、用户确认链、迁移完整性。
- QA-04：G0–G4 视觉/场景门、固定镜头与正常路线。
- QA-05：目标平台、Cook/Package、性能、内存与长稳。

### UI（10）

UI-01 Design System/tokens；UI-02 Hangar；UI-03 Preflight；UI-04 Flight HUD；UI-05 Camera/FOV；UI-06 Pause/Settings；UI-07 Result/Replay；UI-08 Visual Lab；UI-09 Runtime Editor；UI-10 响应式、可访问性和平台适配。UI 泳道交付规格、状态、资源和评审，不默认等同 10 名前端工程师。

### UX（2）

- UX-01：信息架构、主路径、文案、错误恢复与新手理解。
- UX-02：操控、可访问性、研究方案、用户外观确认与最终验收入口。

## 3. Phase 0–7 执行矩阵

表中的 Owner 是责任泳道，不是虚构个人。

| Phase | Owner（主/协） | 输入 | 输出 | DoD | 依赖 | 主要冲突与裁决 |
|---|---|---|---|---|---|---|
| **0 事实冻结** | PMO；RD-01/02/07/09；QA-01/03/05；UI-01；UX-01 | 用户最新指令、旧仓 PRD/门禁/资产清单、本机工具事实 | UE 版本与平台 ADR、目录/模块图、vertical slice 合同、资产与证据 manifest schema、风险/阻塞表 | `PHASE0_ACCEPTANCE` 全部签出；UE 缺失明确 `BLOCKED_NO_UE`；不把旧仓结果写成 UE PASS | 无 | UE 版本、渲染方案、命名冲突由 spike+ADR 裁决；产品语义由最新用户指令裁决 |
| **1 工程与可飞灰盒** | RD-01/03/05/07；QA-01/02；UI-01/04；UX-02 | Phase 0 冻结合同、灰盒路线、输入动作表 | 可启动工程、灰盒世界、基础 Pawn、输入中和、最小 HUD、自动化骨架 | Windows Development 可编译/Cook/smoke；起飞—路线—返航可运行；失焦/暂停四轴清零；无 UE 则保持阻塞不越级 | P0 | RD-03 与 RD-05 对输入真值冲突：Action 属 RD-05，飞行动力响应属 RD-03；UI 不持有规则 |
| **2 飞行/相机切片** | RD-03/04/05/06；QA-02；UI-03/04/05/06；UX-01/02 | 可飞灰盒、Camera/FOV schema、HUD token | Nose、Near/Far、FOV 事务、飞前、暂停/恢复、结果/回放骨架 | 主路径端到端；相机不穿模；vFOV 不被宽高比改写；安全区与输入中断矩阵通过 | P1；RD-02 socket 合同 | RD-04 与 UI-05 不重复保存 FOV；Camera Service 是真值，UI 只编辑事务 |
| **3 资产/机库门** | RD-02/04/06；QA-03；UI-02/03/05；UX-01/02；PMO 管用户门 | 正式/占位资产包、来源与哈希、相机 socket、机库规格 | 导入检查、审核棚、Hangar/Quality Rail、逐机确认记录、黄金机装配 | 工程门通过；具名机只有 `USER_CONFIRMED` 才进正式任务；未交付内容 fail closed；确认记录可追溯 | P0、P2 | 视觉通过与工程通过互不代签；用户未确认时 PMO 不允许排期压力越门 |
| **4 海岸内容与只读 MCP** | RD-07/08/09/10；QA-04；UI-08；UX-02 | 灰盒路线、Environment State、黄金机/通用机、G0–G4 | 三英雄区、Clear/Overcast/Rain、音画基础、固定镜头、只读 inspect/capture | G0–G2 先通过，再评 G3；天气同 revision；MCP 不能写；正常路线 Before/After 完整 | P2；P3 可用资产或明确通用机替代 | RD-08 不能为画面绕过 RD-07 状态；QA-04 的 G0–G2 FAIL 优先于 LookDev 评分 |
| **5 Visual Lab 与受控编辑** | RD-09/10/07；QA-04/05；UI-08/09；UX-02 | 固定场景、性能预算、Command API、Sandbox | A/B/捕获/Profiler；Selection/Hierarchy/Transform；preview/snapshot/rollback | 只允许草稿对象；每次写操作事务闭环；失败自动回滚；证据绑定 build；目标硬件初测完成 | P4 | UI-09 与 MCP 必须复用命令；性能采集不自动写 PASS；正式地图保持只读 |
| **6 收敛候选** | PMO；RD-01~10；QA-01~05；UI-01~10；UX-01/02 | 功能完整切片、缺陷/证据清单、用户确认记录 | Gizmo/Snap/Undo/Redo/序列化/验证；RC 候选；回归与长稳包 | P0 缺陷为 0；G0–G4 通过；Windows 非开发构建 30 分钟/三冷启与循环通过；同 build 证据齐 | P0–P5；正式资产和 UE 工具链 | 只允许修缺陷，不临时加范围；任一 P0 重开退回相应 Phase；豁免不能覆盖用户门 |
| **7 AI/平台扩展/复制决策** | PMO；RD-01/07/09/10；QA-05；UI-09/10；UX-02 | 已验收候选、事务编辑器、平台预算、复制成本 | 受控 AI 布局/光照/LOD 建议、macOS/iPadOS/iOS 证据、下一机型/场景决策 | AI 产物保持草稿直至人工/用户门；各平台独立声明；复制成本、返工率和回退路径获批准 | P6；SDK/设备/签名 | 平台特性不能破坏 Windows 基线；AI 评分与用户观感冲突时以用户/证据为准并回滚 |

## 4. 依赖与并行方式

主链：`P0 → P1 → P2 → P3 → P4 → P5 → P6 → P7`。阶段内允许并行，但不能提前宣称阶段完成：

- P1 的工程骨架、输入、灰盒可并行，公共动作与坐标合同先冻结。
- P3 资产等待期间，P2 系统和 P4 场景可使用清楚标记的通用/占位资产继续；正式任务门保持关闭。
- P4 的场景内容与 P5 的工具可在独立 Sandbox 并行；工具不得写正式地图。
- UI 资源按页面 owner 分区，共享 token 只由 UI-01 修改；UX 对流程和语义有评审权，但不直接覆盖实现文件。
- QA 从 Phase 0 参与，不在末期才接手；每个阶段至少有一条独立 QA 泳道维护退出证据。

## 5. 冲突管理

1. **单一 owner**：公共 schema、Gameplay Tags、Input Actions、UI tokens、Environment State 各只有一个写 owner。
2. **先接口后并行**：跨泳道改动先提交 ADR/接口 diff；未经 owner 接受不得复制定义。
3. **短租约**：共享地图、Data Asset、Build 配置按任务登记租约，结束即释放；避免二进制资产并改。
4. **集成列车**：按“数据合同 → 核心逻辑 → UI → 内容 → 测试”合入候选；失败退回责任泳道，不在集成分支热修未知来源。
5. **裁决顺序**：安全/用户门 > 产品语义 > G0–G4 > 工程便利 > 排期。PMO 记录决定，不自行改技术结论。

## 6. 状态与报告

每个工作项记录：Phase、泳道 owner、输入版本、输出路径、DoD、依赖、风险、验证状态、Evidence ID、用户确认（如需要）。周报只报告事实：Done、In Review、Blocked、Not Run；不以“完成百分比”掩盖硬门。没有 UE、资产、设备或用户确认时写清阻塞对象、可并行事项和解除条件。
