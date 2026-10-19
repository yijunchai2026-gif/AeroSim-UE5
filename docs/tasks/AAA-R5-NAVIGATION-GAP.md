# AAA-R5：计分目标之间的可飞导航合同

2026-10-18，开发者辅助审查。本文只冻结最小设计/验证要求，未实现导航、未运行 UE。当前结论是“端点与任务状态已有测试，连续可飞连接尚未证明”，不能写成已经完成真实路线。实际输入全路线、60–90 秒时长、视觉引导可读性与目标性能均为 **NOT_RUN**。

## 当前源码能证明什么

`Source/AeroSim/Private/PacificCoastGraybox.cpp` 的 `RoutePoints` 和 `Phase4Mission.cpp::MakePacificCoastV1` 给出以下 WORLD cm 坐标；`SeaLevelCm=-6500`。这些是五个计分目标，保留编号、身份与顺序，不把导航中间点算成新增检查点。

| 节点 | 当前 WORLD 坐标/规则 |
|---|---|
| Home | `(55,79,GetCoastalLandHeight(55,79)+1.8)`，实际 Pawn 原点另加脚深；落地仍须实际 mat 支撑证明 |
| C1 GRASS / SOIL | `(4500,0,500)` |
| C2 TIDAL ROCK | `(GetCoastalShoreX(-12000)+2500,-12000,-5300)` |
| C3 LOW WATER | `(GetCoastalShoreX(-6000)+3500,-6000,-6100)`；另有 sea+300..500 高度窗 |
| C4 FOREST ENTRY | `(-15500,-6600,GetCoastalLandHeight(-15500,-6600)+250)` |
| C5 FOREST CORRIDOR | `(-17600,-10600,GetCoastalLandHeight(-17600,-10600)+250)` |

`Phase4Mission.cpp::IsInsideSafetyVolume` 的 core 要求 `|X|<=25000, |Y|<=15000, Z>=0`；近海带要求 `|Y|<=15000` 且 `Shore(Y)+300<=X<=Shore(Y)+8500`；林间另用有限 corridor 距离和贴地高度带。全局上下界仍有效。`IsValid` 只检查目标点，不检查点间航迹；`GetRecoveryPoint` 当前从少数目标/返航点选最近合法点，也没有证明到它的直线可飞。

C1→C2 若直接线性指引，`Z(t)=500-5800t`，约 `t=.0862` 后已经低于 0，横向仍未完成越崖。C3→C4 的直连则在完成海上爬升前切向崖壁。具体哪一个样本越界/撞地，必须通过实际地形查询与扫掠输出坐标证明；此处不伪报已执行的碰撞结果。每个端点 PASS、纯模型可负 Z、全 corridor 中心线合法，均不能推出上述连接 PASS。

## 最小引导航迹：五个目标不变

以下是候选拓扑和确定坐标来源，**不是已验收航路**。`Shore(Y)` 指当前几何对应的 `GetCoastalShoreX(Y)`，`Edge(Y)` 指 `GetCliffEdgeX(Y)`；不能把它们视为同一条线。`Hout/Hin` 必须由同包真实碰撞表面的跨崖航段采样求得，至少高于该航段地形/结构 300 cm，并满足现安全上界。无合法高度或窄通道不够时报告 FAIL，调整局部航迹，不扩大整个安全世界。

| 顺序 | 当前引导目标/必要约束 | 是否计分 |
|---|---|---|
| 起飞→C1 | 实际 mat 垂直起飞到脚底净空约 300 cm，再接 C1；检查初始脚深与原点坐标转换 | 仅 C1 |
| C1→出海前上崖点 | 候选 `(Edge(-12000)-800,-12000,Hout)`；内陆接入航段须逐米验证，有阻挡就添加有界转折点 | 否 |
| 出崖→海上下降顶点 | `(Shore(-12000)+2500,-12000,Hout)`；跨崖段保持足够高，不提前向 C2 拉斜线 | 否 |
| 海上垂直下降→C2 | XY 锁定该海上点附近，以同一 XY 降至 `-5300`；下降前减速，平面速度<=100 cm/s、偏离轴<=100 cm；不得瞬移/自动吸附 | 仅 C2 |
| C2→C3 | 在近海安全带内沿岸走，而非任意两点弦线：候选 `Y=-12000+6000u, X=Shore(Y)+2500+1000u, Z=-5300-800u`；曲线也逐米采样，不只检两端 | 仅 C3 |
| C3→海上爬升顶点 | 同 C3 XY，先垂直爬升至 `Hin`，不得立刻指向陆上 C4；平面偏移越限时先引导回安全爬升柱 | 否 |
| 返陆上崖点 | `(Edge(-6000)-800,-6000,Hin)`，先完成越崖，再接林缘入口；不能把“海上离岸 35 m”当作已经越过整面崖壁 | 否 |
| 接入 C4→C5 | 入口即现有 `CorridorControl[0]=(-15500,-6600)`；C5 对应控制点 `(-17600,-10600)`。跟随 `CoastFlora::GetLayout().Corridor` 实际采样折线/曲线，Z按贴地 250 cm生成；不能跨过弯道用直线切树 | 仅 C4、C5 |
| C5→Home | 先按已验证 corridor 反向回入口，再用已验证的林外接入段去 Home 上方；减速后垂直落 mat。禁止只给一个穿林的 HOME 直线目标 | 否；最后完成支撑落地条件 |

当前通用模型最大平面速度 1800 cm/s、响应 1.8/s，中性制动距离约 10 m。故下降柱和最终落点前要有足够预减速距离和提示，不能以 1 m 中间点半径要求玩家从满速瞬停。候选点若落入真实树枝、桥栏杆或其它碰撞，改航迹并保留 G1/G2 检查，不借修改 forest 密度开出新通道。

## 导航状态与评分分开，但只有一个当前引导点

最小独立导航记录包含稳定 `leg_id/guide_id`、WORLD position、leg 类型（transit/vertical/corridor/landing）、可接受半径/高度窗/速度窗，以及可选 `scored_checkpoint_id`。五个检查点列表仍是计分事实源；中间引导点不给分、不算漏检、不改变 checkpoint 数量。

导航进度只依据 **扫掠解决后的实际 Pawn 位姿**。above-cliff、offshore、vertical-ascent 等门需同时满足位置与高度条件，不能使用 C1/C2 的 25 m 评分半径提前跳过它们。中间点建议先以平面 150 cm、高度±75 cm作为验证候选；允许依据实际操控试验调整并记录，不宣称此阈值已有用户验证。靠近路径但仍未完成先决越崖/爬升，不得直接把引导切到后方计分点。

计分检查点只有其所属导航段已经激活、前置安全门完成后，才根据原有 radius/LOW WATER 高度窗判定；输入暂停、碰撞冻结与触水不得推进导航。恢复后继续未完成的当前 leg，不清除已赚检查点，也不能靠进入未来目标范围越序跳过安全门。

Mission 提供一个只读 `CurrentGuidanceTarget`，HUD 主箭头、距离/高度差、方向外框、重试反馈与恢复点均消费它。同一状态内不能 HUD 指 C4、反馈指最近 C2、地面标记又指 Home。原有 `Feedback.RecoveryPoint` 应来自这个导航输出，不再自行在计分端点中选最近点。边界恢复可先选择当前已验证 leg 上的可达安全锚点，并把该锚点设为当前 recovery guide；接回原 leg 后恢复正常目标。若从事故实际位姿到锚点也未证明可达，显示受控停止/重试，不给穿崖直线。水事故保持显式用户 Reset 语义，不能自动传送或继续水下航行。

RTH 是导航 leg 切换，不是自动飞行；保留真实输入、速度和碰撞。任何自动位置设定、review camera、逐点 teleport 或注入计分 Observe 不能算真实任务完成证据。合法复位后导航/任务一起重建，已暂停的事故状态不得遗留旧索引。

## 逐米几何、安全与实际操控验证

先冻结相同 source/build、seed、环境状态、引导航迹与 collider 版本。每段按实际弧长至多 100 cm 间隔采样，包含所有端点、转弯和垂直段。采样必须打印首个失败的 leg/距离/XYZ/实际 surface/sea/normal/clearance 及原因，不用“所有目标合法”替代。

- 所有样本在现有有限安全体内、值有限；验证实际连接段，不仅 corridor 原有中心线。
- 土地/崖顶用真实 WorldStatic 碰撞面，不能对海上点错误套用延伸的 `GetCoastalLandHeight`。在各样本按实际 Pawn 包围形状、脚深和允许姿态验证净空；转弯还检查旋转后的占用。
- 常规出返海 transit 候选最小地形/结构净空 300 cm；既有林间低飞段单列贴地 250 cm目标，实际脚底净空与侧向树干/栏杆余量需验证，不能强行套同一 300 cm阈值。垂直起飞/落地最终贴地段独立验证真实支撑。
- 低水面以实际 SeaLevel 和脚深计算；C3 保留 sea+300..500 的原点评分窗，同时确保脚底不触水。低水、垂直柱与沿岸段均不得穿崖/水下；不存在地面支撑不等于可以忽略水面。
- 邻接样本间用实际碰撞体连续 sweep，排除逐米点全在空气而中间穿薄栏杆/枝干的情况；检查整个训练入口/窄门，碰撞代理与可见几何差异另做真实视角复核。
- 分别构造“未爬升就朝内陆”“下降前仍高速漂移”“反向接回 corridor”“中断/恢复/水 Reset”等反例，验证导航不给出危险跳跃、评分不提前、HUD与恢复点一致。

这些检测通过才得到限定范围的几何/状态 PASS；仍需同包由菜单进入、实际语义输入完成起飞→五目标→返航→mat 落地，记录连续运动、暂停恢复、碰撞和水面往返证据。先记录真实时长，60–90秒当前 **NOT_RUN**；若可飞合理路线超时，报告目标冲突，不能删低水目标、瞬移、调高速度或把时间 fixture 当真实通过。

## 固定 plateau footprint 与林木布局

集成源码已把原七点土地开敞 footprint 冻结为 `GetPlateauClearancePoints()`：`(0,0,100),(1800,0,400),(4000,-1200,600),(6500,-500,700),(9000,2000,700),(6500,4000,600),(0,0,100)`。`Trees.cpp::IsProtected` 改读该接口；实际任务 `GetRoutePoints()` 的海上/林间点专供飞行，不再间接生成全林 26 m 避让带。这是职责解耦，不是以删除植被解决飞行问题；原土地/接地环境断言保留，只更新其读到的正确 footprint 来源。

后续必须在相同 flora 几何/seed 下比较解耦前后 **全部已放置树** 的稳定顺序、archetype、Base、Scale、Yaw、LeanPitch、LeanYaw、near/flank 标志及对应实例 transform，生成可复跑 digest；报具体差异实例。单比 count、密度下限、HISM数量或漂亮截图不能证明没有重新撒树。把“任务引导/计分点变化”和“冻结 plateau footprint”做对照测试：仅改变任务路点，layout digest 必须不变。真实 digest 对比当前仍 **NOT_RUN**，本作者本轮只写此文档，没有运行引擎或修改源/资产。

## R5 NAV 实施（DEV-FLIGHT，2026-10-18）

分支 `aime/coast-aaa-flight`，在合入 integration `f52d913` 之后实现。只改 `Phase4Mission.{h,cpp}`、`FlightHUD.cpp`（只读引导面板/箭头）、`AeroSimControlBridge.cpp`（Inspect 只读引导字段）与测试；**未改** FlightPawn、GameMode、PacificCoastGraybox.h、地形/植被/海洋代码。五个计分检查点的 id/顺序/数量/半径/LOW WATER 高度窗不变；任务版本仍为 v2。

### 接口

- `EPhase4GuideKind`：Takeoff/Transit/VerticalDescent/VerticalAscent/Corridor/Landing/Hold。
- `FPhase4GuidanceLeg`：稳定 `LegId`/`GuideId`、WORLD 折线 `Path`（`Path[0]`=上一段终点）、终点接受窗（平面半径、下/上高度、平面速度上限）、沿段跟踪容差与跟踪速度上限、lookahead、可选 `ScoredCheckpointIndex/Id`、`bReturnLeg`。
- `FPhase4TaskDefinition::BuildGuidanceRoute()`（确定性，`BeginMission` 时构建，`Reset` 清空）与 `ValidateGuidanceStructure()`（id 唯一、连续、计分顺序/身份、竖直柱 XY 锁定、每 ≤100 cm 样本在原有限安全体内，输出首个失败 leg/距离/XYZ/原因；`BeginMission` 失败即拒绝开始）。
- `FPhase4MissionSession::GetCurrentGuidanceTarget()`：唯一只读输出（steer 点、guide 点、窗口、剩余距离、`bRecovery/bHold`）。HUD 箭头/距离/高度差、`Feedback.RecoveryPoint`、碰撞/漏检/未完成 RTH/边界/触水反馈均读它。

### 引导拓扑（WORLD cm；Home=(55,79,1.8) 为 mat 面）

| leg_id | 类型 / 计分 | 终点 | 接受窗 |
|---|---|---|---|
| nav-01-takeoff-column | Takeoff | (55,79,321.8) | R100，-40/+700（起飞相位过冲），≤200 cm/s |
| nav-02-grass-soil | Transit / C1 | (4500,0,500) | R300 ±150 |
| nav-03-inland-to-cliff | Transit | (Edge(-12000)-800=16806,-12000,350) | R300 ±150 |
| nav-04-cross-cliff-out | Transit（越崖保持高度） | (23867,-12000,350) | R150 ±75，**≤100 cm/s 后才下降** |
| nav-05-offshore-descent | VerticalDescent / C2 | (23867,-12000,-5300) | 轴偏≤100、跟踪≤100 cm/s、R100 ±75 ≤100 |
| nav-06-alongshore-low-water | Transit / C3 | 24 节点 `Shore(Y)+offset`，Y -12000→-6000，Z -5300→-6100，终点 C3 (25493,-6000,-6100) | 跟踪≤300，R150 ±75 ≤100 |
| nav-07-sea-climb | VerticalAscent | (25493,-6000,350) | 轴偏≤100、≤100 cm/s |
| nav-08-cross-cliff-in | Transit | (Edge(-6000)-800=19765,-6000,350) | R300 ±150 |
| nav-09-field-to-forest-mouth | Transit | (-14950,-5313,350)=C4−入口切线×1400 | R300 ±150 ≤600 |
| nav-10-forest-entry | Corridor / C4 | (-15500,-6600,250) | R150 ±75 ≤600，跟踪≤150 |
| nav-11-corridor-follow | Corridor / C5 | `CoastFlora::GetLayout().Corridor` d∈(0,4522] 实际样本，Z=地表+250，终点 C5 (-17600,-10600,250) | 跟踪≤150 |
| nav-12-corridor-reverse | Corridor / 返航 | 同折线反向至 C4 | R150 ±75 ≤600 |
| nav-13-forest-exit | Corridor / 返航 | (-14950,-5313,350) | R150 ±75 ≤600 |
| nav-14-field-return | Transit / 返航 | (55,79,350) | R100 ±75 **≤100 cm/s** |
| nav-15-land-on-mat | Landing / 返航 | (55,79,1.8) | 完成仍为原规则：Grounded+实际 mat 支撑+pad 半径+<40 cm/s |

巡航高度由沿段解析地形 +300+50 求得（当前地面≈0 → 350），再由真实碰撞验证；不对海上点套用延伸的 `GetCoastalLandHeight`。

### 状态规则

- 进度只来自 GameMode 收到的 sweep 解决后实际位姿，在 `[progress-2 m, progress+10 m]` 弧长窗内投影，且需在跟踪容差/跟踪速度内；不能靠接近同折线后段或未来检查点跳进度。
- 计分：只有当前 leg 拥有的检查点按原半径/高度窗判定；进入未来检查点半径只记 MissedCheckpoint 诊断（不计分、不移动引导）。计分 leg 需"已计分 + 终点接受窗"才完成，故 C2 原 25 m 半径计分不会提前结束下降柱。
- `Paused` 相位、碰撞接触帧、触水 incident 均不推进；触水后引导为 Hold（无箭头），仅显式 R 复位；`Reset` 清空引导，`BeginMission` 重建。
- 边界：恢复锚点取当前已验证 leg 上的点（最近点/当前进度/leg 终点），并用保守解析地形（崖顶延伸到 shore+3 m）+300 cm 检查直线连接；不通过则 Hold"受控停止/缓慢爬升"。只有回到安全体且重新进入当前 leg 跟踪容差才清除 incident。
- RTH（第五点计分后自动或 H）= 切换到第一条返航 leg（nav-12），不是自动飞行；第五点前 H 仍 ROUTE INCOMPLETE 且不换 leg。Landing 阶段只在 nav-15 激活后开始。

### 验证（本批实际运行）

- `AeroSim.Flight.Navigation.PerMetreRouteClearsRealCollision`：生成真实 Graybox（地形/崖/树干代理 WorldStatic）和真实 Pawn；Pawn 盒 (16,14,5)，按偏航不变外接盒+18°倾斜 (21.3,21.3,9.9) 扫掠，脚深 9.07 cm。15 段 + C5 过冲缓冲段共 1186 m、1212 个 ≤100 cm 样本，**0 段失败**：transit/竖直全向 ≥300 cm 且脚底净空 ≥300；低水最小脚到水 391 cm；corridor 段脚底 241 cm（150–400 带）且侧向 ≥50 cm；mat 段 sweep 清晰且有真实支撑。首轮失败只有校验器本身把 mat 面当作原点（起飞/落地段 sweep 初始穿地），已改为原点不低于着陆原点；未加任何局部绕行点、未放宽安全体、未改植被。可见树冠（非碰撞）诊断 0 个样本。
- `AeroSim.Flight.Navigation.ScriptedSemanticInputTraversal`：带真实 GameMode 的菜单启动任务，脚本控制律只写 `SetSemanticInput` 到真实 Pawn，实际 Tick/sweep/触水/着陆支撑：完成 Result，5/5、碰撞 0、越界 0、漏检 0、未触水，**仿真时长 126.0 s**（下降柱 15.3 s、爬升柱 17.4 s、田野横越 20.6 s）。这是工程可达性 fixture，不是玩家/真实输入 PASS。
- `GuidanceContractKeepsFiveCheckpoints`、`CounterexamplesNeverSkipSafetyGates`：五点不变、稳定 id、未爬升即向内陆、下降前高速漂移、反向接回、暂停/碰撞/触水/Reset、边界锚点与单一输出一致、未完成 RTH。

### 时长冲突（需 PMO/产品裁决）

现模型平面 18 m/s、竖直 4 m/s；必要竖直量约 113 m（≥28 s），水平约 1000 m（≥55 s），加上合同要求的 ≤1 m/s 制动点，脚本最优约 126 s，超出 60–90 s。按合同不删低水目标、不提速、不瞬移；需由产品决定调整目标时长或检查点高度。

### 仍为 NOT_RUN

打包后菜单进入的真实按键全路线、60–90 s、HUD 引导可读性、树冠/栏杆视觉与碰撞代理差异的正常视角复核、目标平台性能。

## R5b 用户裁决：提高竖直速度上限（2026-10-19）

**用户决定**：针对 126 s 路线，保留地形、检查点与航迹拓扑，提高竖直速度上限。实施（DEV-FLIGHT，合入 integration `b315565` 之后）：

- `FFlightModel`（FlightPawn.cpp，仅 Flight 相位的语义油门映射）：爬升上限 800 cm/s、下降上限 600 cm/s（下降 < 爬升，保留涡环余量）；竖直改为一阶响应 2.2/s 并限制竖直加速度 ≤700 cm/s²，起停平滑且不过冲。Takeoff 相位（900 cm/s²）与 Landing 相位（固定 250 cm/s 软着陆）未改。
- 引导：竖直柱终点、离岸制动点、C3、mat 上方制动点新增竖直速度窗 ≤150 cm/s；竖直柱与沿岸低水段在剩余高度 ≤10 m 时 HUD 标签前缀 `BRAKE VERTICAL SPEED`，HUD 同时显示 `VERT <=1.5`。
- 合入 terrain R3+R5 后 shore 几何变化，C2/C3 由同一 `GetCoastalShoreX` 规则得到 (23656,-12000,-5300)/(25358,-6000,-6100)；检查点定义未改，离岸下降柱随之为 X=23656。

### 实测（NullRHI Automation，同源码）

| 项 | 结果 |
|---|---|
| 纯模型 | 满爬升 4 s 达 800 cm/s，峰值竖直加速度 700 cm/s²；满下降 600 cm/s；6 m/s 松杆 0.93 s / 2.74 m 降到 ≤1 m/s，不反弹爬升；0.1 s Ctrl 轻点峰值 70 cm/s、总位移 34 cm；2 m 高满 Ctrl 进入 Landing 以 250 cm/s 触地、无不安全接触 |
| 逐米碰撞校验 | 1182 m、1207 样本、0 失败段（最小值同上：transit ≥300、低水脚到水 391、corridor 脚 241/侧向 ≥50） |
| 脚本语义输入全程 | Result，5/5，碰撞 0、越界 0、漏检 0、未触水；**113.1 s**（原 126.0 s） |
| 分段 | 起飞 1.1、C1 3.1、内陆到崖 10.2、越崖制动 6.9、**下降柱 10.7**（原 15.3，峰值 580 cm/s）、沿岸 C3 7.4、**爬升柱 9.4**（原 17.4，峰值 780 cm/s）、回崖 3.7、田野到林口 20.6、C4 3.7、走廊 9.6、反向 10.0、出口 3.3、返场 12.2、落地 1.3 s |

仍超 90 s：剩余主要是水平（约 1000 m、18 m/s 平面上限，田野横越 20.6 s、走廊往返 19.6 s）。按用户裁决本批不改水平速度/地形/拓扑；是否继续缩短交产品裁决。真实按键 60–90 s 仍 NOT_RUN。

### 打包 smoke 按键参考（不按 Q/E，机头保持 +X；W=+X S=−X D=+Y A=−Y，Shift 升 Ctrl 降）

Enter、Enter → Shift ≈1 s 起飞 → W 穿 C1 → W+A ≈9.5 s 至崖上点 → W ≈3.5 s，提前约 10 m 松开，制动到 <1 m/s → Ctrl ≈9 s（HEIGHT 剩约 −3 m 时松开，出现 BRAKE 提示，VERT ≤1.5 时接受）→ D 略带 W 沿岸到 C3 停稳 → Shift ≈8 s（剩约 4 m 松开）→ S ≈3 s → S ≈19 s 到林口（<6 m/s）→ A+S 进入 C4 → 跟箭头走廊到 C5 → D+W 反向、出口 → W+D ≈9 s 到 mat 上方停稳 → Ctrl 至落地，出 Result。MCP Inspect 观察 `guide_leg_id/guide_progress_cm`。

## R6a 真实输入发现后的接受窗拆分（2026-10-19）

**PMO 发现**（打包 R6a，integration `445ed8f`）：闭环真实按键驱动 25 s 到达 C1 与 nav-03 引导点 (16806,-12000,350)，之后在其 1–2 m 内以 1–4 m/s 绕行 6.5 分钟仍未完成该段。复核：该点当时窗口为 R300 ±150、无实际速度限制，样本本应满足；卡住的直接原因是**进度窗**——进度只在 `[progress-2 m, progress+10 m]` 内、且偏离直线 ≤6 m 时增长，驾驶员早段偏出管道后在终点附近重新入管，进度落后 >10 m，再也追不上。另有驱动器阻尼不足。按合同"可依据实际操控试验调整并记录"，本批（DEV-FLIGHT，合入 integration `2e1c600` 之后）调整：

- **按类型拆分接受**：Transit/转向点（nav-02 C1、03、08、09）改为 pass-through：平面 ≤600 cm 且高度 ±200 cm，**或**在 ±200 cm 高度内、2×半径内越过过引导点、垂直于该段末段平面方向的门平面；无速度要求。Corridor 段（nav-10..13）沿折线 pass-through：在 150 cm 管道内到达折线末端（R150 ±150），无速度要求。
- **安全门不变**：起飞柱、nav-04 离岸下降柱顶（R150 ±75，平面 ≤100、竖直 ≤150 cm/s）、nav-05 下降柱底 C2、nav-06 终点 C3/爬升柱底、nav-07 爬升柱顶、nav-14 mat 上方（R100 ±75，≤100/≤150）与 nav-15 落地保持原紧窗与竖直制动窗。计分规则、未来检查点诊断、恢复/触水/碰撞/暂停规则未变。
- **Transit 进度追赶**：只对直线 Transit 段，若有界窗内不在管道，则在 `[progress, L]` 上再投影，回到 ≤6 m 管道即续接进度（直线无自交，不能借此跳过竖直或走廊安全门）；Column 与 Corridor 仍为有界窗。
- HUD 显示 `PASS THROUGH 6 m / +-2 m` 或 `STOP 1.5 m / +-0.8 m <=1.0 m/s VERT <=1.5`；MCP Inspect 新增只读 `guide_pass_through`、`guide_accept_radius_cm`、`guide_accept_height_cm`、`guide_accept_planar_speed_cm`、`guide_accept_vertical_speed_cm`。

新增反例：偏离 nav-03 管道 15 m 飞行不记进度；在终点 1.5 m 处以 3 m/s 绕行（复现 R6a）→ 完成；以 17 m/s、侧偏 4 m 穿过 C1 → 越门完成且 C1 按原半径计分；nav-04 下降柱顶以 5 m/s 经过 → 不完成。

**实测**（合入 terrain R6 后 shore 微调，航线 1176 m）：逐米真实碰撞 1201 样本、0 失败段；脚本语义输入全程 Result、5/5、碰撞/越界/漏检/触水均 0，**111.5 s**（R5b 113.1 s）：起飞 1.1、C1 2.8、内陆到崖 10.0、越崖制动 6.6、下降柱 10.7、沿岸 C3 7.4、爬升柱 9.4、回崖 3.4、田野到林口 20.0、C4 3.6、走廊 9.6、反向 10.0、出口 3.3、返场 12.2、落地 1.3 s。真实按键全程与 60–90 s 仍 NOT_RUN；按键参考同 R5b，转向点无需停稳，只在 BRAKE/STOP 提示处制动。

## R6a 根因与安全走廊重设计（2026-10-19，DEV-FLIGHT）

**PMO 根因**（打包 R6a，真实按键，阻尼驾驶）：停在 (16823,-12026,339)、速度 0，距 nav-03 引导点 17/26/11 cm，`guide_progress_cm=501`，而该段约 17,200 cm。进度只在"贴着中心线的细管内"增长，接受又要求进度接近段尾——170 m 空中直线段要求全程待在细管内，正常玩家任何绕行/斜向修正都会静默卡死。

**重设计**：
- 每段一个**安全走廊**（平面半宽 + 竖直带），进度 = 位姿在走廊内时对该段的**最远投影**（直线 transit/竖直柱在整个剩余段投影；弯曲林间折线保持有界窗 15 m，防止跳到同一走廊后段）；无速度门（竖直柱仍 ≤100 cm/s）。
- 走廊宽度按段由逐米真实碰撞测量导出并在 Automation 中证明：整段包络（含 Pawn 外接盒、余量 transit/竖直 100 cm、林间 30 cm）无 WorldStatic 重叠、包络角点都在原有限安全体内、海上包络最低脚底距水 ≥1 m。

| 段 | 声明半宽 / 竖直带 | 实测最小可用半宽 |
|---|---|---|
| nav-02/03/04/08 空中 transit | ±15 m / −2 +8 m | ≥29.9 m（测量上限） |
| nav-09 田野到林口 | ±15 m / −2 +8 m | 17.6 m |
| nav-14 田野返场 | ±15 m，最后段漏斗收窄到 ±3 m（斜率 0.1） / −2 +8 m | mat 前 5 m 处起降区器材限制，369 cm（余量 19 cm） |
| nav-06 沿岸低水 | ±10 m / −2.5 +3 m | 13.2 m |
| nav-10/13 林口；nav-11/12 走廊 | ±1.8 m / ±1.5 m | 林口 27 m；走廊 205 cm（树门处，余量 25 cm） |
| 竖直柱、起飞、落地 | ±1 m（起飞 ±1.5 m） | ≥29.9 m |

- **离开走廊**＝可见恢复引导，不再静默：`bOffCorridor` + `bRecovery`，HUD 红条 `OFF CORRIDOR (+-W m) - NO PROGRESS UNTIL YOU RETURN TO THE ARROW`，箭头指回进度附近的已验证点（空中 transit 需通过保守解析连线检查，否则受控停止；竖直柱/林间为局部回中）。离开有限安全体仍走原 Boundary 恢复。
- **完成**：transit 越门/进窗（±6 m、±2 m），林间沿折线到达末端；竖直柱、离岸制动、C2/C3、mat 上方、落地保持紧窗与竖直 ≤150 cm/s。Inspect 新增 `guide_off_corridor`、`guide_safety_half_width_cm`。

新增测试：L 形与之字形（±80% 半宽、±1.5 m 高度）路径完成 nav-03；PMO 停泊位姿完成 nav-03；越崖时降到带下 1.5 m 不完成并显示恢复；C1 直插离岸下降点的弦线不完成越崖门、不计 C2。

**实测**：逐米真实碰撞 1176 m / 1201 样本、0 失败段；脚本语义输入全程 Result、5/5、碰撞/越界/漏检/触水 0，**111.6 s**（分段同上一节，nav-10 3.8 s）。真实按键全程仍 NOT_RUN。
