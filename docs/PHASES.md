# AeroSim-UE5 Phase 0–7

阶段按证据退出，不按日期自动前进。`PASS` 只表示给定 build、平台和范围内通过；任何 P0 重开都退回产生该事实的阶段。

## Phase 0 — 事实冻结与生产决策

**目标：**把“想做 UE5”变成可执行、可阻塞、可验证的基线。

交付：冻结 UE 5.x 版本/工具链、目标平台与参考硬件；模块/目录和 ADR；Pacific Coast 范围、路线、三英雄区与固定状态；资产身份/用户确认、证据 manifest、G0–G4、风险与迁移清单。

退出：见 `PHASE0_ACCEPTANCE.md`。若本机没有冻结版 UE，则引擎验证保持 `BLOCKED_NO_UE`，Phase 0 可以完成“规划就绪”，但 **Phase 1 实现不得标为已开始或通过**。

## Phase 1 — 工程骨架与可飞灰盒

**目标：**建立最小可构建、可 Cook、可操作、可测试的 UE 工程。

交付：模块骨架、Build ID、灰盒 World、基础飞行 Pawn、Enhanced Input 语义层、安全暂停、最小 HUD、Automation/Functional smoke。主路线用灰盒完成起飞、穿越和返航。

退出：Windows Development 编译、Cook、Package 与启动通过；输入失焦/暂停/断连清零；灰盒 G0/G1 成立；缺 UE 时全部保持阻塞。

**当前实现状态（2026-10-16）：**七动作语义输入、fail-closed Input Suite、飞行状态机、Safe Pause、FPV/Chase、可见 Canvas HUD、纯 C++ 1 km × 1 km Pacific Coast 灰盒和五门返航路线合同已落地。UE 5.8 Mac Development、Phase 1/Phase 0 Automation、Cook/Stage/Package/Archive 和打包应用 NullRHI 冒烟通过，因此本轮 Mac 自动化工程退出门为 PASS；Windows Development 仍未运行，真实画面的人工 G0/G1 保持 PENDING。实现与边界见 `../PHASE1.md`。

## Phase 2 — 飞行、相机与 HUD vertical slice

**目标：**形成第一条用户可完成的端到端飞行链。

交付：Hangar 到 Preflight、Flight、Result、Replay 骨架；Nose 与 Near/Far Follow；跟随碰撞；vertical FOV 单真值及 Preview/Apply/Cancel/Undo；HUD 安全区与设备仲裁。

退出：固定路线端到端；相机无穿模/原点闪现；16:9/21:9/32:9 不偷改 vFOV；暂停恢复与输入矩阵通过；正常游戏视角 G2 可评。

## Phase 3 — 机型资产门、机库与用户确认

**目标：**保证“这是什么机型”与“用户是否认可外观”都有证据。

交付：资产 manifest、DCC/导入检查、标准审核棚、LOD/碰撞/socket 检查、Hangar Quality Rail、用户确认记录。首架黄金机优先；正式内容未到时保留通用机系统链。

退出：正式机型工程门全通过，且用户按机型+版本+哈希确认；未确认内容不进入正式任务。没有正式资产时允许阶段标记 `BLOCKED_ASSET`，不得靠程序化壳体越门。

## Phase 4 — 海岸内容、天气音画与只读 MCP

**目标：**把灰盒升级成可判断的内容切片，同时先建立安全观察能力。

交付：三英雄区、连续地貌、水岸与林缘、Environment State 驱动的 Clear/Overcast/Rain、基础空间音频、固定 A–H/距离镜头；MCP 仅提供 scene/asset/performance inspect 和 capture。

退出：严格按 G0→G1→G2→G3；正常路线近/中/远景通过，无悬浮、突现地貌或任务不可读；天气各消费者 revision 一致；MCP 无写权限。

## Phase 5 — Visual Lab、性能与受控编辑

**目标：**让比较和修改都可复现、可审计、可回滚。

交付：Visual Lab A/B、Wipe、Debug、捕获与性能栈；Sandbox Selection/Hierarchy/Inspector/Transform；Command API 的 inspect、preview、snapshot、apply、validate、evidence、rollback。

退出：编辑只作用于草稿对象；并发冲突/超时/验证失败自动回滚；人工 UI 与 MCP 共用命令；目标硬件初始性能包完整，但采集成功不自动等于 PASS。

## Phase 6 — 事务编辑器与候选收敛

**目标：**封闭 P0 缺陷，产出可交用户评审的 Windows 候选。

交付：Gizmo、Snap、Undo/Redo、ScenePatch 序列化、验证和审计；完整主路径、G0–G4、回归、性能、长稳和证据包。

退出：P0=0；用户外观确认有效；Windows 非 Development 构建在冻结硬件上完成 120 秒预热、30 分钟固定回放、三次冷启动及定义的循环回归；同 build 证据齐全。最终用户验收仍未被团队代签。

## Phase 7 — 受控 AI 优化、平台扩展与复制决策

**目标：**在已稳定切片上扩能力，而不是用 AI 或多平台掩盖基础缺陷。

交付：AI 布局、光照、LOD/性能建议（默认 `DRAFT_NOT_USER_CONFIRMED`）；macOS、iPadOS、iOS 的独立构建和验证；下一机型/场景的成本与复用报告。

退出：所有 AI 写操作经过事务、G0–G4、人工审查和必要的用户确认；macOS 后再做 iPadOS、iOS 真机，桌面证据不外推；是否复制由吞吐、缺陷、性能和返工成本决定。Android/Linux/XR 不自动纳入。

## 跨阶段硬规则

- 平台顺序：Windows → macOS → iPadOS → iOS。
- 视觉顺序：G0 → G1 → G2 → G3 → G4。
- 内容状态：占位/原型绝不冒充正式资产；具名机型逐机用户确认。
- MCP 顺序：P4 只读，P5 受控 Transform，P6 完整事务编辑，P7 才开放受控 AI 建议。
- 验证事实：没有 UE、SDK、设备、资产或用户签字，就写 Blocked/Not Run。
- 每个阶段新增高成本能力必须有 Low/Off 路径和回退方案。


## 当前阶段事实（Phase 2）

Phase 2 已实现 Pacific Coast 程序化 vertical slice、三 Hero Zone、五态任务流程、三相机/vFOV 事务、版本化环境状态、暂停恢复回中、Result/Replay 和 `AeroSim.Phase2` Automation。合同与验证见仓库根目录 `PHASE2.md` 及 `docs/PHASE2_VALIDATION.md`。Phase 3+ 能力不得据此视为已完成。
