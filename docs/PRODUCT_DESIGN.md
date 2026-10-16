# AeroSim-UE5 产品设计

## 1. 产品判断

AeroSim 是面向无人机训练、摄影练习和飞行复盘的本地优先模拟器。UE5 重建线的价值不在“换引擎”，而在建立一条可信的内容生产与验收链：机型身份不冒充、相机语义不混淆、场景从常识开始验收、证据可以复跑。

首个成果不是全机型、全场景、全平台，而是一条完整 vertical slice。只有切片通过产品、视觉、工程和用户门禁，才复制到更多机型与地图。

## 2. 目标用户与核心任务

- 新手：在三分钟内完成选机、飞前检查、起飞并安全返航。
- 摄影用户：理解机鼻、图传、云台和跟随相机的差异，练习构图与路径。
- 进阶飞手：使用键鼠、手柄或触控完成低空巡检，并查看可信回放与遥测。
- 内容/QA：在 Visual Lab 中用固定状态比较画质、性能与场景变化。

产品不宣称真实飞控、空气动力、法规、图传协议或天气达到 1:1；未实现能力必须显示原因，不以隐藏入口或改名制造完成感。

## 3. 首个 vertical slice：Pacific Coast / 海岸巡检

### 3.1 范围

- 可见环境约 `1 km × 1 km`，核心验收区约 `300 m × 500 m`。
- 主线 60–90 秒：起飞坪 → 草/土过渡 → 岩岸湿边 → 栈道/栏杆 → 低空水面 → 林缘 → 返航。
- 三英雄区：起飞坪与地表；岩岸与潮湿边；栈道与林缘交界。
- Clear 16:30、固定 seed 为基准；Overcast 与 Rain 共用一个 Environment State 派生。
- 首片只要求一架可识别且获用户确认的黄金机；正式资产未到时可用清楚标识的通用训练机完成系统链，但不得声称具名机完成。

### 3.2 成功体验

用户从机库看到资产真实状态，完成输入回中、相机/FOV、环境和画质检查；飞行中可在 Nose 与 Near/Far Follow 间安全切换，HUD 不遮挡中心路线；碰撞、暂停、失焦和设备断开都进入安全状态；返航后可回放路线并看到本次 build、机型、相机、环境与结果。

### 3.3 明确不在首片内

在线多人、实时空域、全球排行榜、完整 DJI 目录、VR/XR、真实图传链路、全局开放世界以及四个具名机同时量产。它们不得阻塞首片，也不得作为已存在能力展示。

## 4. 信息架构

顶层保持三个清晰工作区：

1. **Hangar**：身份、外观、质量状态、机型选择与飞前入口；
2. **Flight**：实时飞行，不显示顶层导航；设置和退出经安全暂停；
3. **Visual Lab**：环境、画质、A/B、调试视图、性能和证据捕获。

主路径：`Hangar → Aircraft/Scene → Preflight → Flight → Result → Replay`。错误恢复保留已选机型、场景和已应用设置，不把用户丢回首页。

## 5. 飞行、相机与输入语义

- 输入动作语义统一：Pitch、Roll、Yaw、Throttle、Pause、Camera、Capture、RTH；设备只提供映射。
- 暂停、失焦、设备断开、完整设置打开时清零四轴；恢复前要求回中并倒计时。
- 相机由 Context + Family + Preset + Capability 表达，不能退化为 Chase 布尔值。
- 首片 P0：Nose/Pilot 与 Near/Far Follow；Gimbal/Observer 分阶段；Video FPV 只有链路、镜头与元数据真实可测时才能开放。
- FOV 的持久化真值为 vertical FOV，horizontal FOV 由宽高比派生；Preview、Apply、Cancel、Undo 是一个事务。
- HUD 在安全 2D 层渲染，不随镜头畸变或景深变形；关键状态不只依赖颜色。

## 6. 机型、资产和用户外观门

具名机型的 runtime ID、catalog ID、资产版本、飞行/相机 profile 必须独立。正式接入包包括 DCC 源、FBX/GLB 或批准格式、LOD、碰撞、Pivot/关节、PBR 源、单位/轴向、来源和 SHA-256。

状态至少包含：`CONTENT_NOT_DELIVERED → UNVERIFIED → REFERENCE_LOCKED → GEOMETRY_VERIFIED → PRODUCTION_READY → USER_CONFIRMED`，失败可进入 `REJECTED`，不能靠改标签越级。

**用户逐机外观确认是一票门禁。**确认绑定机型、版本和哈希；轮廓、颜色、材质或贴花变更后重新确认。未确认资产可在审核棚查看，但不能进入正式任务或发布候选。Racer/Micro 等通用机持续标为 AeroSim Generic，不映射成具名产品。

## 7. 场景质量 G0–G4

| 门 | 用户要看到的事实 | 一票否决示例 |
|---|---|---|
| G0 存在合理性 | 元素符合地点、任务和来源 | 无理由出现的巨石/设施、身份冒充 |
| G1 空间/尺度/重力 | 地貌连续、物体接地、路线可飞 | 悬浮、断崖墙片、错误尺度、碰撞失配 |
| G2 正常视角 | 近中远景轮廓和任务构图清楚 | 只在 LookDev 角度成立、正常路线像贴片 |
| G3 材质/光照/天气 | 水岸、湿润、雾、VFX、音频同源 | 雨天只换天空、曝光抽动、材质语义混乱 |
| G4 性能/工程 | 降级不删规则，构建稳定可复跑 | HLOD/LOD 爆跳、内存爬升、打包缺资产 |

每轮视觉比较固定出生点、路线、相机、天气、时间和质量，提供正常游戏视角的 Before/After；诊断通道只能补充。

## 8. Visual Lab 与证据

Visual Lab 展示 Scene、Build、RHI、Quality、Weather、Time、Seed、Camera、FOV 和设备；状态改变后标为 Dirty。支持 A/B Split、Wipe、质量对比、Debug View 与 Capture Drawer。只允许一个变量不同；未捕获状态必须写 `NOT_CAPTURED`。

性能看 P50/P95/P99、CPU/GPU、内存/驻留、Draw/Primitive、GC/分配和流送尖峰，不以平均 FPS 单独判定。正式证据绑定不可变 build 和文件哈希；Editor、Development、旧仓截图和静态扫描均不能成为发布 PASS。

## 9. 平台与性能口径

顺序为 Windows 桌面 → macOS 桌面 → iPadOS → iOS；其他平台另立范围。Windows 1440p High 以 60 fps 为候选目标，Phase 0 必须冻结参考硬件与 RHI。正式性能要求非 Development 构建、120 秒预热、固定回放 30 分钟、三次冷启动；具体帧时和内存上限在硬件冻结后版本化。

移动端使用同一产品语义但独立内容档和 UI 布局，必须真机验证多点触控、安全区、热降频、内存、后台/来电中断和手柄；桌面缩窗只可做几何预检。

## 10. 完成定义

首片只有在以下事实同时成立时才可称 Candidate：

- Phase 0–6 对应 P0 全部 PASS，阻塞项为零；
- 黄金机资产通过工程门并获用户逐机外观确认；
- G0–G4 无 P0，主路径与安全输入回归通过；
- 同一 Shipping/等价构建的截图、视频、日志、性能和 manifest 完整；
- Windows 参考构建通过，macOS 差异有结论；移动未实测则明确不在该 Candidate 声明中；
- 最终用户验收仍由用户完成，团队不能代签。


## 11. 第一批 P0 产品迁移（Unity PRD → UE5）

本批把旧 Unity PRD 的用户 Happy Path 转成可直接操作的 UE5 产品流程，而不是继续要求用户知道调试键：

`01 HANGAR → 02 PREFLIGHT → TAKEOFF → FLIGHT → RESULT → REPLAY`

- **唯一可见主动作**：Hangar、Preflight 和 Result 均使用 `Enter / Space`；`M` 仅保留为调试兼容键，并明确标注，不再承担产品引导。
- **Hangar**：显示 Aircraft Bay、任务、天气/质量、机型身份和真实资产状态。当前训练机持续显示 `AEROSIM GENERIC · PROTOTYPE / UNVERIFIED`，不将程序化代理写成正式 DJI 资产。
- **Preflight**：同屏显示输入回中、Camera/vFOV、Environment 和 Display；主按钮写明 `ARM & START`，下一动作明确为 `HOLD LEFT SHIFT TO TAKE OFF`。
- **Flight HUD**：下一动作、阶段、目标、进度、相机、vFOV、高度、速度和输入入口位于外围；中心飞行走廊保持无遮挡。完整键鼠主路径为：`W/S Pitch`、`A/D Roll`、`Q/E Yaw`、`Left Shift/Left Ctrl Throttle`、`C Camera`、`H RTH`、`R Reset`、`Esc/F10/P Settings`。
- **输入/暂停状态**：Display Settings 打开时安全暂停并清零四轴，面板明确显示 `SAFE PAUSE / INPUT NEUTRAL`；关闭后沿用三秒回中恢复合同。失焦和设备断开仍保持既有安全暂停，不回退 Phase 5/6 边界。

### 11.1 Display Settings

Display Settings 可由 `Esc`、`F10` 或既有 `P` 打开，右侧停靠，不覆盖中心视野；`↑/↓` 导航，`←/→` 改值，`Enter/Space` 执行，`Esc/F10` 取消并关闭。

最低 P0 选项：

- Resolution：`1280×720`、`1440×900`、`1920×1080`；
- Window Mode：`Windowed`、`Borderless`、`Fullscreen`；
- Transaction：`Preview`（仅当前窗口、不保存）、`Apply + Save`、`Cancel`、`Rollback`。

设置继续使用 `aerosim.settings/v2`，兼容新增 `resolution_width`、`resolution_height`、`window_mode`。Apply 同时确认 UE GameUserSettings，并通过 `.tmp + flush + .bak + replace` 保存 JSON；启动时 final 失败会尝试 backup。旧 v1 迁移与缺显示字段的旧 v2 文件保持可读，未来版本/非法枚举/不支持分辨率 fail closed。

### 11.2 产品合同与真实性边界

Runtime 主身份保持 `aerosim.runtime.phase7/v1`，新增兼容产品交互合同 `aerosim.product-ux/v1`，不改写既有 Phase 7、Phase 6 ScenePatch 或 Phase 5 安全合同。完整映射见 [Unity PRD → UE5 产品迁移矩阵](UNITY_TO_UE5_MIGRATION_MATRIX.md)。

本批截图和 Automation 只能证明 Mac Development 下的产品流程、显示事务、输入提示和程序化原型表现；正式机型仍为 `BLOCKED_ASSET`，用户外观确认仍为 `BLOCKED_USER_CONFIRMATION`，Windows 与 iPadOS 仍为 `NOT_RUN`，不得从 Mac 结果外推。
