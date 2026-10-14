# AeroSim 从旧仓迁移到 UE5 的计划

## 1. 迁移原则

这是产品和数据迁移，不是 Unity API 到 UE API 的逐行翻译。旧仓提供可验证的行为、术语、路线、资产来源和 QA 思路；任何旧构建、截图、测试 PASS 或程序化资产都不自动成为 UE5 事实。

迁移优先级：产品语义与稳定 ID → 数据/资产来源 → 行为基线 → UI 流程 → 场景构图 → 平台优化。每批可回退、可双跑、可解释。

## 2. 输入基线

旧仓可作为参考的内容包括：

- 六机型目录与 Prototype/Generic 身份边界；
- Mode 2、键鼠、手柄、触控语义和暂停/失焦清零；
- Nose、Near/Far 等相机/FOV 产品合同；
- Hangar、Flight、Visual Lab 和主路径信息架构；
- Pacific Coast 路线、三英雄区、Environment State 与 G0–G4 纪律；
- 固定捕获、性能与 Evidence Manifest 思路；
- Runtime Editor/MCP 的事务、安全和审计要求。

旧仓中的 Unity Library、Builds、Logs、UserSettings、缓存、生成截图、平台工程和绝对路径不迁移。

## 3. 四类处理决定

每项进入清单后必须选一类：

1. **Reuse Source**：来源明确的 DCC、纹理、音频、图标、文案和数据，按 UE 导入规则重新生成引擎资产。
2. **Re-author**：飞行、相机、输入、保存、UI、Shader、天气、场景和工具逻辑在 UE 架构中重写。
3. **Reference Only**：Unity Prefab/Scene、程序化原型、截图、测试脚本只用于行为/视觉比对。
4. **Drop**：缓存、构建、过时分支产物、无来源或与新范围冲突内容。

不得把 Unity `.meta` GUID 当作 UE 长期 ID；先建立独立 catalog/runtime ID 映射。

## 4. 分批迁移

### Batch 0：盘点与冻结（Phase 0）

输出 `legacy-inventory`、来源/许可/哈希、功能行为清单、ID 映射、已知缺陷和“不要复制”清单。旧仓保持只读基线；关键截图绑定旧 build，仅作参考标签。

**退出：**每个候选文件/功能都有 owner、处理类别、目标位置、验证和回退。

### Batch 1：数据与合同（Phase 1）

迁移机型目录、输入动作、相机/FOV、Environment State、质量档和保存字段的语义，不搬运行时代码。建立 UE Data Asset/DTO 与版本迁移器，使用黄金 JSON/CSV 样本做跨实现比对。

**退出：**ID 无静默回退；未知/损坏数据 fail closed；旧存档只在明确支持的 schema 上导入，原文件不覆盖。

### Batch 2：可飞行为（Phase 1–2）

在 UE 重写飞行状态、碰撞、输入安全、暂停、Nose/Follow 和结果/回放最小链。用固定输入序列比较事件顺序、路线完成与安全状态；不要求逐帧数值与 Unity 完全相同，差异必须由产品合同解释。

**退出：**UE 原生测试与打包 smoke 通过；旧仓仅作 oracle 之一，不把旧 bug 复制为兼容性。

### Batch 3：UI/UX（Phase 2–3）

从旧信息架构和概念图重建 CommonUI/UMG 页面。迁移文案、状态语义和交互，不导入 Unity UI 层级或像素坐标。先 Design System，再 Hangar/Preflight/HUD/Camera/Result/Replay。

**退出：**主路径、响应式、安全区、焦点、错误恢复和可访问性通过；用户最新视觉要求高于旧截图。

### Batch 4：机型资产（Phase 3）

只从 DCC/源贴图迁移，Unity FBX/材质可作比对但不是唯一源。转换坐标、单位、pivot、socket、骨骼、LOD、collision、PBR 通道和压缩；生成新的 UE 导入报告和哈希。

**退出：**工程资产门 + 用户逐机外观确认。程序化 Unity 壳体保持 Reference Only/Prototype，不得升级成正式具名机。

### Batch 5：Pacific Coast（Phase 4）

保留路线、地标职责、构图和环境状态，重新制作 UE 地形/网格、材质、水、植被、光照、天气、VFX 与音频。Unity 程序化场景和旧截图只帮助识别缺口；优先灰模 G0–G2，再进入 G3/G4。

**退出：**固定路线正常视角通过 G0–G4，Clear/Overcast/Rain 同状态，Low/Off 不删除碰撞或任务。

### Batch 6：工具与证据（Phase 4–6）

将旧捕获/MCP 的合同迁成 UE Build ID、Automation/Gauntlet、Insights/CSV、Visual Lab 和事务 Command API。旧日志格式只在需要历史对照时保留转换器。

**退出：**证据来自 UE 同一 build；MCP 写操作仅 Sandbox、可回滚、可审计；旧 PASS 不进入新 gate 汇总。

### Batch 7：平台（Phase 6–7）

先冻结 Windows 候选，再验证 macOS；随后对 iPadOS、iOS 做独立内容档、触控、安全区、Metal、内存和热预算。移动平台不是桌面设置的简单下调。

**退出：**每个平台独立构建、smoke、性能和已知限制；缺 SDK、签名或设备时标阻塞。

## 5. 技术映射提示

| Unity 概念 | UE5 候选 | 迁移注意 |
|---|---|---|
| ScriptableObject | Primary Data Asset/Data Table | 先稳定 ID 与 schema，不依赖资产路径 |
| MonoBehaviour/Manager | Actor Component/Subsystem/纯 C++ 服务 | 生命周期和依赖显式化，避免全局单例复制 |
| Prefab | Blueprint Class/Level Instance/Packed Level Actor | 正式/草稿资产分区，避免二进制并改 |
| Scene | Map + World Partition/Data Layers | 坐标、流送、引用和 HLOD 重新验证 |
| Input System | Enhanced Input | 动作语义迁移，设备绑定重做 |
| UI Toolkit | CommonUI + UMG/Slate | 流程迁移，不复制层级布局 |
| Shader/Material | UE Material/Instance/Function | PBR 通道、色彩空间、移动兼容重审 |
| PlayerPrefs/JSON | SaveGame + 版本化 DTO | 原子写、迁移、损坏恢复 |
| Unity Test/自定义 smoke | Automation/Functional/Gauntlet | 用例意图迁移，测试结果不继承 |

## 6. 坐标、单位与资产转换

旧 Unity 通常为米制、Y-up、+Z forward；UE 为厘米制、Z-up、+X forward。具体 DCC 轴向须从源文件实测，不能只凭默认值。转换应在导入规则中一次完成，根节点保持可解释变换；飞行方向、左右手性、桨旋向、相机 socket、碰撞和遥测坐标分别用基准资产验证。

纹理要重新核对 sRGB、Normal 方向、ORM 打包、alpha、mip 与平台压缩。材质视觉相似不代表数值通道正确。

## 7. 双跑与差异台账

每批保留一组不可变输入，分别运行旧仓参考与 UE 候选，记录：预期不变、允许改变、意外回归、证据和决定。视觉用相同任务路线和近似物理相机比较，不追求通过移动旧相机“对齐截图”。

差异优先级：安全/数据损坏 > 身份/用户门 > 主路径 > G0–G4 > 性能 > 细节。旧实现与用户最新要求冲突时，以最新要求为准并在台账中明确废弃旧行为。

## 8. 回退与完成定义

迁移批次通过独立分支/变更集和版本化 schema 交付；源资产不覆盖，转换产物可重建。发生 ID 错配、存档损坏、外观未确认、G0–G2 退化或性能不可接受时，回退该批，不用后续批次补丁掩盖。

迁移完成不是“文件都复制了”，而是：目标范围内每项都有处理决定；UE 原生构建和测试通过；资产来源与用户确认完整；Pacific Coast 在目标构建过 G0–G4；Windows→macOS→iPadOS→iOS 的实际声明与证据一致。没有 UE 安装时，只能完成盘点与计划，不能完成任何引擎迁移批次。
