# Phase 0 验收清单

Phase 0 的产物是可信基线，不是可运行游戏。验收人按职责泳道签结果，不填写虚构姓名。允许的状态只有 `PASS / FAIL / BLOCKED / NOT_RUN`。

## A. 指令与范围

- [ ] 记录用户最新顶层指令，并声明其优先于旧仓文档。
- [ ] 明确当前允许/禁止修改路径；当前文档轮未触及 `Source/`、`Plugins/`、`Config/`、`.uproject`、`SourceAssets/`。
- [ ] 首个 vertical slice 固定为 Pacific Coast / 海岸巡检，范围、路线、三英雄区清楚。
- [ ] 非目标明确：不一次交付全机型、全场景、全平台、在线多人、实时空域或 XR。

**Owner：**PMO-01、UI-01（UX 共担）。
**证据：**产品设计、Phase 文档、决策日志。

## B. UE 工具链事实

- [ ] 选定并记录精确 UE 5.x 版本、安装来源与升级策略。
- [ ] Windows 编译器/SDK、项目生成和命令行工具路径已实测。
- [ ] macOS Xcode/SDK、iPadOS/iOS 签名与真机条件分别清点。
- [ ] 所需插件逐项标记 Built-in/项目插件/第三方、版本、许可和平台支持。
- [ ] 给出最小空工程的 Editor 启动、C++ 编译、Cook/Package spike 结果。

**Owner：**RD-01、QA-01、QA-03。
**硬规则：**未安装 UE 或不可调用命令行时，本节为 `BLOCKED_NO_UE`；不能用文档检查、旧 Unity 构建或网页资料替代。此阻塞禁止 Phase 1 实现验收。

## C. 平台与预算

- [ ] 顺序冻结为 Windows → macOS → iPadOS → iOS；其他平台不在当前范围。
- [ ] Windows 参考硬件、RHI、分辨率和 High 质量候选目标版本化。
- [ ] macOS 最低/推荐硬件与 Metal 差异列入 spike。
- [ ] iPadOS/iOS 目标设备、内存、热、触控、安全区和画质降级计划明确。
- [ ] 正式性能协议包含非 Development 构建、120 秒预热、30 分钟回放和三次冷启动。

**Owner：**RD-01、RD-09、QA-03。

## D. 架构与所有权

- [ ] Core、Simulation、Input、Camera、Aircraft、Environment、UI、Evidence、RuntimeEditor、Platform 边界明确。
- [ ] 关键 schema 带版本、未知值 fail closed，并有迁移/回滚策略。
- [ ] Gameplay Tags、Input Actions、UI tokens、Environment State 各有唯一 owner。
- [ ] Blueprint 与 C++ 分工明确；关卡蓝图不承载全局业务真值。
- [ ] 至少列出 UE 版本、渲染/RHI、飞行步长、UI、世界流送、保存、MCP、资产/LFS ADR。

**Owner：**RD-01、RD-03~10；PMO-01 维护 RACI。

## E. 资产、迁移与用户外观门

- [ ] 旧仓资产/功能清单按 Reuse Source、Re-author、Reference Only、Drop 分类。
- [ ] 坐标、单位、pivot、socket、材质、LOD、collision 与来源/哈希合同明确。
- [ ] 具名机型状态机和 fail-closed 行为可审查；未知 ID 不回退成默认机型。
- [ ] 用户逐机确认绑定机型、版本、哈希；新稿默认 `pending_user`。
- [ ] 未确认内容只能在审核棚/Sandbox；任何泳道和自动化均不能代签。

**Owner：**RD-02、QA-03、UI-01、PMO-01（UX 共担）。

## F. 场景与产品门禁

- [ ] G0–G4 定义、顺序和一票否决项已写入验收与缺陷模板。
- [ ] G0–G2 灰模评审先于材质、灯光和性能优化。
- [ ] 正常路线的近/中/远景 Before/After 是主证据；孤立 LookDev 只是补充。
- [ ] Clear 16:30 基准、seed、出生点、路线和相机固定方法明确。
- [ ] Environment State 的天空、雾、湿润、水、VFX、音频消费者清单完整。

**Owner：**RD-07/08、QA-02、UI-01、PMO-01（UX 共担）。

## G. 验证与证据

- [ ] Build ID 与 Evidence Manifest 字段冻结：source、UE、平台/RHI、硬件、资产、地图、环境、相机/FOV、质量、artifact SHA。
- [ ] 测试层次覆盖合同、Automation、Functional、Cook/Package smoke、固定回放、人工视觉与用户门。
- [ ] PASS/FAIL/BLOCKED/NOT_RUN 分开；自动化不会覆盖人工 FAIL。
- [ ] Editor/Development/Shipping 证据清楚区分；旧仓截图不作为 UE5 PASS。
- [ ] 日志、截图、视频、性能和原始数据的保留/忽略/LFS 策略明确。

**Owner：**RD-09、QA-01~05。

## H. MCP 安全

- [ ] 人工 UI 与 MCP 共用 Command API；没有旁路写入。
- [ ] 流程固定为 inspect → preview → snapshot → apply → validate → evidence → transaction commit/rollback。
- [ ] 白名单 Runtime ID/Asset ID/属性和 Sandbox 路径边界明确。
- [ ] 任意脚本、反射、路径、插件加载和正式资产删除被禁止。
- [ ] Phase 4/5/6/7 权限递增清楚；默认产物 `DRAFT_NOT_USER_CONFIRMED`。
- [ ] 并发版本、超时、崩溃、审计和默认回滚有测试设计。

**Owner：**RD-10、QA-01、QA-02。

## I. 团队与冲突

- [ ] `PMO-01`、`RD-01..10`、`QA-01..03`、`UI-01` 被明确为 15 条虚拟职责泳道；UX 由 UI-01 与 PMO-01 共担，不包含虚构姓名。
- [ ] 每个 Phase 有 owner、输入、输出、DoD、依赖和冲突处理。
- [ ] 共享二进制资产和地图采用单 owner/短租约，跨泳道先冻结接口。
- [ ] 资产、UE、SDK、设备、用户确认阻塞时，有可并行事项与解除条件。

**Owner：**PMO-01。

## Phase 0 最终判定

可标记 **Phase 0 Planning Ready** 的条件：A、C–I 全部 PASS，B 已 PASS 或明确为 `BLOCKED_NO_UE`，且没有被伪装成运行通过。只有 B 全部 PASS、最小工程 spike 可复跑，才可批准进入 Phase 1 实现与验收。

签出记录应包含日期、责任泳道、状态、证据链接、未决项、解除条件和下一次复核点；不填写不存在的人员姓名。

### 2026-10-16 工具链签出

- **责任泳道：** RD-01、QA-01、QA-03
- **状态：** PASS（macOS Phase 0 工具链与最小工程 spike）
- **证据：** UE 5.8.3 preflight 全项通过；AeroSimEditor Mac Development 编译通过；`AeroSim.Phase0.Editor.Heartbeat` 与 `AeroSim.Phase0.Runtime.Smoke` 均为 Success；Mac Development Cook/Stage/Package/Archive 退出码为 0；打包应用 NullRHI 启动并正常退出。
- **边界：** 未覆盖 Windows、iPadOS/iOS 签名和真机，也不构成人工视觉 G0–G4 或 Shipping 性能 PASS。
- **已知约束：** 本机 Cook 使用 `-AdditionalCookerOptions='-SkipZenStore'`，避免生成的远程 Zen Store 地址导致连接停滞。
