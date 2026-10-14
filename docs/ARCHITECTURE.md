# AeroSim-UE5 架构

## 1. 架构目标

架构服务三件事：飞行规则可验证、内容可替换、所有人工与 MCP 修改可回滚。UE5 能力通过适配层接入，产品状态不直接散落在 Actor、Widget 或关卡蓝图里。

## 2. 模块边界

建议在 Phase 0 以 spike 验证后冻结模块名；职责先按下列边界执行：

| 层 | 责任 | 禁止 |
|---|---|---|
| Core | 稳定 ID、结果类型、版本、事件、时间与序列化合同 | 引用具体关卡/UI |
| Simulation | 飞行状态、控制语义、安全暂停、碰撞结果 | 直接读取 Widget 或设备按键 |
| Input | Enhanced Input 映射、设备仲裁、回中与断连 | 复制飞行规则 |
| Camera | Context/Family/Preset、FOV、跟随碰撞、snapshot | 用布尔值承载多相机 |
| Aircraft | catalog、资产状态、profile、socket、装配验证 | 未验证时回退成别的机型 |
| Environment | Environment State、天气派生、时间、质量档 | 各系统各自维护天气真值 |
| UI | CommonUI/UMG 表现、焦点、安全区、可访问性 | 在 Widget 内写核心业务状态 |
| Evidence | build/设备/场景元数据、捕获、性能与 manifest | 自动把采集结果判为视觉 PASS |
| RuntimeEditor | Selection、Command、Transaction、Validation、草稿保存 | 直接写正式地图或任意资产 |
| Platform | 文件、手柄、触控、图形 RHI 与平台能力适配 | 将平台差异泄漏到产品规则 |

依赖方向为 UI/Platform/RuntimeEditor → 应用服务 → Core 领域；Simulation、Camera、Aircraft、Environment 通过接口和只读快照协作。关卡蓝图只编排，不充当全局服务容器。

## 3. 关键数据合同

- `AircraftDefinition`：catalog/runtime ID、身份、资产版本、能力、socket、LOD/碰撞与用户确认引用。
- `AircraftRuntimeState`：姿态、速度、控制输入、损伤/碰撞和当前 profile；不保存展示文案。
- `CameraState`：context、family、preset、capability、pose、input owner、vertical FOV、lens/link profile。
- `EnvironmentState`：sun、cloud、fog、wind、precipitation、wetness、puddle、water、VFX、audio、seed。
- `QualityProfile`：平台预算、可伸缩项和降级原因；不得删除任务、碰撞、警告或输入安全。
- `EvidenceManifest`：source/build、UE、平台/RHI、资产、地图、环境、相机、画质、硬件、命令、artifact SHA。
- `ScenePatch`：schema、base revision、命令列表、前置条件、目标 Runtime ID、资产白名单和签名状态。

Schema 必须带版本和迁移器。未知枚举、丢失 ID、哈希不符或版本过新时 fail closed，并保留可诊断错误。

## 4. UE 对象与生命周期

- 跨地图但属于一次运行的协调放 `UGameInstanceSubsystem`；只属于 World 的服务放 `UWorldSubsystem`。
- 玩家输入和相机所有权由 PlayerController/Pawn 协调，核心状态留在可测试组件/服务中。
- 数据定义优先 Primary Data Asset；运行时可变状态不得回写资产。
- 保存走版本化 SaveGame/JSON DTO，异步写临时文件后原子替换；崩溃不覆盖上一份有效存档。
- Gameplay Tags 表达可组合能力/状态，不把任意字符串当协议。
- Event Dispatcher/Delegate 订阅必须在生命周期结束时解绑；地图切换与 50 次循环检查泄漏。

## 5. 飞行与输入

设备事件先映射为规范化动作，再经过 dead zone、曲线、仲裁和安全门进入 Simulation。暂停、失焦、断连和 UI 独占会生成显式 `NeutralizeInput`，不能只依赖下一帧设备值。固定步长/物理策略、重放确定性和网络需求在 Phase 1 spike 后由 ADR 决定；没有证据前不宣称确定性回放。

## 6. 相机和 FOV

相机选择与镜头参数正交。状态迁移由 Camera Service 完成，同帧切换 HUD profile；跟随相机通过碰撞查询处理遮挡。唯一持久化值是 vertical FOV，horizontal FOV 根据 aspect 计算。Preview 写事务草稿，Apply 才落盘，Cancel/关闭恢复 opening snapshot，Undo 在定义窗口内恢复。

## 7. 资产与内容管线

`SourceAssets/` 保存 DCC 源、参考、来源许可/版本和哈希；UE `Content/` 保存导入结果。导入规则检查米制、轴向、根变换、命名、材质槽、socket、LOD、collision 和 Nanite/移动兼容性。正式机型只有在工程状态 `PRODUCTION_READY` 且用户状态 `USER_CONFIRMED` 时可进入正式任务。

World Partition/Data Layer/HLOD、Nanite、Lumen、VSM 与水体方案必须分别做 Windows、macOS 和移动 spike。高端特性要有 Low/Off 回退；渲染管线选择写 ADR，禁止以一次 Editor 截图决定主干。

## 8. Environment 单一真值

所有天气消费者从不可变 `EnvironmentState` 或其版本化快照读取。变更先 staging，再在安全边界提交；天空、雾、光照、地表湿润、水、VFX 和音频必须报告同一 state revision。任何消费者落后或不支持时标 `PARTIAL_STATE`，证据不能 PASS。

## 9. Runtime Editor 与 MCP

人工 UI 和 MCP 都生成相同的 `IEditorCommand`：

- `Inspect` 返回只读对象、开放属性、前置条件和版本；
- `Preview` 在草稿 World/Data Layer 计算 diff，不触碰正式内容；
- `Apply` 必须持有事务 ID、snapshot 和乐观并发版本；
- `Validate` 依次执行 G0–G4 与工程规则；
- `CommitTransaction` 只写独立 ScenePatch/草稿保存；失败或超时回滚；
- `Promote` 是独立人工流程，不能由 MCP 自调用。

Command 白名单初期覆盖 inspect、list assets、spawn draft、modify draft transform、delete draft、validate、capture、save sandbox、rollback 和 inspect performance。禁止任意脚本、反射和路径。审计日志追加写，包含操作者、命令、前后值、结果与证据哈希。

## 10. 证据与可观测性

构建生成不可变 Build ID，并把 source revision、UE 版本和内容 manifest 写入运行时。日志使用结构化类别与关联 ID；崩溃、输入中和、资产阻断、环境 revision、相机切换和 MCP 事务必须可追踪。

性能捕获区分 Editor/Development/Shipping。正式门只消费目标硬件、固定回放和相同配置的原始数据；P50/P95/P99、内存驻留与流送趋势由 QA 评审，采集服务不自动签字。

## 11. 测试金字塔

1. 纯 C++/数据合同：schema、FOV 公式、状态机、迁移、命令前置条件；
2. Automation：Subsystem、资产注册、输入仲裁、SaveGame；
3. Functional：关卡路线、相机碰撞、暂停/恢复、天气联动；
4. Cook/Package smoke：缺资产、启动、主路径、退出码；
5. Gauntlet/固定回放：性能、长稳、跨平台；
6. 人工 G0–G4、可用性和用户外观确认。

没有冻结版 UE 时第 2–5 层均为 `BLOCKED_NO_UE`，不能被静态 lint 替代。

## 12. 关键 ADR

Phase 0 至少形成：UE 版本与升级策略、模块/插件边界、渲染与 RHI、飞行步长与回放、World Partition/HLOD、UI 技术、资产导入/LFS、保存格式、MCP 传输与认证、目标平台构建链。ADR 必须写选项、证据、决定、后果和回退，不用“行业最佳实践”代替实测。
