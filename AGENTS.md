# AeroSim-UE5 协作规则

本仓库是 AeroSim 的 UE5 重建线。所有参与者先读本文件，再读 `docs/PHASES.md` 与当前阶段验收文档。**用户最新顶层指令优先**；旧 Unity 仓库只提供产品意图、证据纪律和迁移线索，不能覆盖新指令，也不能被当作 UE5 已完成事实。

## 1. 当前工作边界

- Phase 3 已实现 Generic Golden Aircraft 与相机质量合同；当前事实见 `PHASE3.md` 与 `docs/PHASE3_VALIDATION.md`。正式 DJI 资产仍为 `BLOCKED_ASSET`，复杂空气动力学、Phase 4 任务内容、MCP 写权限、多人/XR 仍不在范围内。
- 不创建提交，不 push，不合并，不改远端、分支保护或仓库可见性。
- 没有 UE Editor/命令行工具时只能做静态审查；所有引擎编译、Cook、运行、截图和性能结果必须记为 `BLOCKED_NO_UE` 或 `NOT_RUN`，绝不能写成 PASS。

## 2. UE5 技术基线

Phase 0 冻结具体 UE 5.x 版本、工具链和插件清单。在冻结前只采用以下方向，不臆测已安装版本：

- 运行时：UE5、C++ 为核心规则与数据边界，Blueprint 负责组合与表现，不复制核心飞行规则。
- 输入：Enhanced Input；键鼠、手柄、触控使用同一语义动作层。
- UI：CommonUI + UMG/Slate；MVVM 是否启用由 Phase 0 spike 决定。
- 数据：Primary Data Asset/Data Table、Gameplay Tags、SaveGame；资产 ID 稳定且可迁移。
- 场景：World Partition、Data Layers、HLOD、Nanite/Lumen/Virtual Shadow Maps 仅在目标平台验证后启用；移动端必须有独立降级路径。
- 自动化：Automation Framework、Functional Test、Gauntlet/命令行 smoke；Unreal Insights、Stat/CSV 与 RenderDoc 等只记录真实采样。
- 构建：Development 用于诊断，Shipping/等价非开发构建才可作发布性能判定。

禁止把“采用 UE5”当作画质、性能或真实性已经提升的证据。

## 3. 目录责任

| 路径 | 责任 |
|---|---|
| `Source/` | C++ 模块、公共接口与测试；按当前阶段合同修改 |
| `Plugins/` | 项目插件和可隔离能力；Phase 2 未扩展 |
| `Config/` | 平台、输入、渲染和打包配置；禁止保留自动写入凭据 |
| `SourceAssets/` | DCC 源、参考、来源与哈希；正式资产仍不在 Phase 2 |
| `Content/` | UE 资产与关卡；Phase 2 禁止新增项目二进制 uasset |
| `docs/` | 产品、架构、阶段、验收与迁移的事实源 |
| `Saved/`、`Intermediate/`、`DerivedDataCache/`、`Binaries/` | 本地产物，不作为交付证据源，不提交 |

一个共享接口只有一个 owner。跨泳道修改先写接口/ADR，再实施；不得靠复制 Data Asset、Gameplay Tag、输入 Action 或 UI token 绕开冲突。

## 4. 首个 vertical slice

首片固定为 **Pacific Coast / 海岸巡检**：约 `1 km × 1 km` 可见环境、`300 m × 500 m` 核心验收区、60–90 秒“起飞坪→草土过渡→岩岸湿边→栈道/栏杆→低空水面→林缘→返航”主线，三英雄区为起飞坪地表、岩岸湿边、栈道/林缘。

桌面 High 是首个艺术目标，Clear 16:30 为基准；Overcast/Rain 从同一 Environment State 派生。旧仓程序化内容只是行为与构图参考，不直接等价迁移为正式 UE 资产。

## 5. 场景与外观门禁 G0–G4

必须严格按顺序判断；上游失败立即停止下游美化：

- **G0 存在合理性**：元素为什么在这里，服务什么任务，来源/身份是否可信。
- **G1 空间、尺度与重力**：地貌连续，物体有支撑，比例、碰撞和玩家路线可信。
- **G2 正常游戏视角**：主路线近/中/远景轮廓、构图、可读性和接地感成立。
- **G3 材质、光照、天气与音画**：PBR 语义、曝光、水岸、湿润、VFX、声音同状态。
- **G4 LOD、性能、诊断与工程完整性**：降级、HLOD/LOD、内存、长稳、打包和证据可复跑。

灰模先过 G0–G2，再做 G3；G0–G2 未通过时禁止用材质、雾、后处理、Nanite、LOD 或性能分数掩盖问题。自动检测只作辅助，用户观感与正常游戏视角证据优先。

## 6. 用户外观门禁

- 每个具名机型必须由用户按“机型 + 资产版本 + 内容哈希”单独确认外观。
- 新稿状态一律为 `pending_user`；PMO、RD、QA、UI、UX、自动化与 AI 均不得代签。
- 未确认资产可以进入隔离审核棚和开发 Sandbox，但不得进入正式选机、正式任务或发布候选。
- 几何、材质、贴花、颜色或可识别轮廓变化会使原确认失效，必须重新确认。
- 缺正式资产时必须显示 `PROTOTYPE / UNVERIFIED` 或 `CONTENT_NOT_DELIVERED`，禁止回退到另一机型冒充。

## 7. MCP 事务与安全

人工编辑器 UI 与 AI MCP 必须调用同一 Command API。所有写操作执行：

`inspect → dry-run/preview → transaction snapshot → apply → validate(G0→G4) → evidence → commit transaction 或 rollback`

这里的 `commit transaction` 只指运行时事务落盘，**不是 Git commit**。

最低安全要求：

- 仅允许稳定 Runtime ID、白名单 Asset/Prefab ID 和显式开放属性；路径必须归一化并限制在 Sandbox 草稿区。
- 禁止任意 C++/Blueprint/Python/Shell 执行、任意反射、任意磁盘访问、加载外部插件、改正式地图或删正式资产。
- 删除只允许草稿对象；写正式内容必须经过受控导入与人工批准。
- 每个命令有操作者、时间、输入、前后值、事务 ID、验证结果、证据和回滚结果；超时、崩溃、验证失败默认回滚。
- 默认产物是 `DRAFT_NOT_USER_CONFIRMED`。MCP 调用成功不等于视觉、产品或用户验收通过。

阶段开放顺序：Phase 4 只读 Inspect/Capture；Phase 5 受控 Selection/Hierarchy/Transform；Phase 6 Gizmo、Snap、Undo/Redo、序列化和验证；Phase 7 才允许 AI 布局、光照、LOD/性能建议，且仍受事务和门禁约束。

## 8. 目标平台顺序

1. Windows 64-bit 桌面参考构建（High、1440p 候选）；
2. macOS 桌面功能与视觉相容；
3. iPadOS 真机；
4. iOS 真机；
5. Android、Linux、XR 仅在另立范围后进入。

后序平台不能反向降低首平台内容正确性。移动端必须单独验证触控、安全区、热、内存、帧率和画质降级；桌面缩窗不是移动实测。

## 9. 验证与证据

每项结论分为 `PASS / FAIL / BLOCKED / NOT_RUN`。证据至少冻结：source revision、build ID、UE 版本、平台/硬件、RHI、质量档、地图、天气/时间/seed、相机/FOV、分辨率、资产版本、命令、日志与文件哈希。

验证顺序：文档/静态合同 → 编译 → Automation/Functional → Cook/Package → smoke → G0–G4 正常视角 → 目标硬件性能/长稳 → 用户外观/最终验收。截图、视频和性能必须来自同一可追溯 build；Editor 截图、平均 FPS、短跑和旧仓结果不能替代目标构建证据。

任何没有 UE 安装、SDK、签名、目标设备、正式资产或用户确认导致的缺口，都必须显式阻塞，允许不依赖该项的泳道继续，但禁止跨门宣称完成。

## 10. Git 交付

- 开始与结束都检查工作树；保留用户已有改动。
- 只暂存明确授权的文件，不使用盲目的 `git add .`。
- 提交应小而可回退，消息关联 Phase、REQ/ADR 与验证状态；大二进制按仓库 LFS 策略处理。
- 禁止提交凭据、签名、缓存、构建、崩溃、全量原始捕获和本机绝对路径。
- 只有用户明确要求时才 commit/push/建 MR；push 后核对远端 SHA。未经授权不合并、不 force-push、不改历史。
- 本轮特别约束：**不 commit，不 push。**
