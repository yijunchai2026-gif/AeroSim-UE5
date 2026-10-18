# AAA-R3-FLIGHT-01：真实航线可达性只读审查

日期：2026-10-18。基线：integration `894ab6a15955409dd72a5f37bb855a5a0d226c91`，以及 `docs/COAST_AAA_PMO_REVIEW.md`。本次只读 Source 和现有测试，另对源码中的控制点作坐标/折线长度计算；未启动 UE、未运行包、未注入输入、未修改实现/配置/资产、未提交。开始时已有 PMO 文档改动，保持原样。本报告是该任务唯一写入文件。

## 裁决

**P0 / FAIL（实现合同），真实玩家通关 NOT_RUN。** 当前正常进入 Flight 后，不能从起飞点下降到约 65 m 下方的低空海面；现有任务也没有把真实海岸和完整森林通道纳入同一条可计分航线。只放开一个 Z 钳位不能修好：必须同时处理真实支撑/降落、模型与世界坐标、安全体/恢复、任务点与环境路径、碰撞后的状态一致性。

保留“起飞→林缘/岩岸→低空海面→返航”的终目标，尤其低空海面不能删掉来获得通过。推荐 PMO 将 AAA-R3-FLIGHT-02 设为单 owner 的跨文件关键路径。其验收必须来自正常任务入口和真实语义输入；offshore review 相机、设置 Pawn review pose、直接给 Mission.Observe 喂坐标都不作玩家可达证据。

## 源码确定的缺口

| 项目 | 已读实现与可确定结论 |
|---|---|
| 起点与高度基准 | `FlightPawn.h` 明确 Model.Position 是相对 reset 点的厘米坐标。`AeroSimGameMode.cpp:162` 以 `(55,79,land+1.8)` 初始化 mat 支撑面；`FlightPawn.cpp:871` 再按实际支脚 bounds 提高 actor origin 并固定 InitialTransform。正常 Tick 用 `InitialTransform.TransformPosition(Model.Position)` 得世界位置。不能把 model Z=0 当世界海平面。 |
| 海面不可达 | `PacificCoastGraybox.h:103` 的 SeaLevelCm=-6500。`FlightPawn.cpp:151` 把 Flight 状态的 Model.Position.Z 钳到≥0。起点在 mat 上，世界 Z 略为正，因此正常已进入 Flight 的飞机原点不会到世界负 Z。若取先前记录的起点 origin≈10.8682 cm，仅作说明：海上5 m的世界Z=-6000，对应model Z≈-6010.8682，明确被当前 Flight 钳位排除。该示例不是本次实测起点值。 |
| 无支撑也会自动降落 | `FlightPawn.cpp:119` 在 Flight、Throttle≤-0.55、model Z≤600 时进入 Landing，不检查脚下是否有地形、是否在家或是否在海上。Landing 固定向前缓至250 cm/s、垂直下降250 cm/s，在model0就Grounded。其分支没有正油门取消/复飞出口。Grounded每帧又把Z设0、速度归零。悬空在起点等高平面“落地”，与真实支撑不是同一件事。 |
| Takeoff 并非全状态非负不变量 | Takeoff 在 `Advance` 内提前 return，未经过最终 Flight 的Z钳位。其姿态旋转局部速度，若起飞中输入pitch/roll可能产生异常轨迹。这是应补回归的状态机漏洞；不能把它当正常下海通道，也不能声称本次已穷举所有输入证明任意状态永不负Z。当前P0结论针对正常完成起飞后 Flight/Landing 路线。 |
| 安全体第二道阻挡 | `Phase4Mission.cpp:44` 的 IsInsideSafetyVolume 独立要求Z≥0；GetRecoveryPoint又将Z钳到300..2200 cm。即便飞行模型修复，负Z水面会触发Boundary，Inspection只在bInside时处理检查点。安全体还限定相对X±25000、Y±15000。 |
| LOW WATER 名不副实 | `PacificCoastGraybox.cpp:31` 仍是正X旧灰盒点；`Phase4Mission.cpp:7` 仅把它们依次命名GRASS/SOIL、TIDAL ROCK、BOARDWALK、LOW WATER、FOREST EDGE。LOW WATER=(9000,2000,700)，全部5点X=1800..9000、Z=400..700；`GetCliffEdgeX` 最小14500，LOW WATER仍在内陆且世界高度约比海面高72 m。即使考虑25 m检查半径和约半米起点偏移，它也不是低海面检查点。 |
| 森林通道与任务脱节 | `PacificCoastTrees.cpp:707` 的实际 corridor 从约(-15500,-6600)环至(-12600,-23000)后回(-5100,-6400)，完全不是正X任务点。12个控制点中5个Y<-15000，完整通道超出任务安全体。控制折线约426.10 m（不是Catmull-Rom真实曲线长度）；现5点任务总折线约213.24 m。完整林间通道没有任务点，也没有真实输入通关证据。 |
| 碰撞与记分位置不一致 | `FlightPawn.cpp:514` 使用真实Actor sweep，但碰撞后只NeutralizeInput，不把被阻挡的真实actor位置/速度同步回FlightModel。`RecordTrace`还在sweep之前；GameMode收到的是已推进的model坐标/速度。于是碰撞时物理Actor、trace和mission的权威位置可能分叉。Phase4的collision latch仅Reset清零，不能由多次碰撞场景推断完整计数正确。 |
| 回家和落点合同过宽 | Phase4以model XY距0≤1800 cm进入Landing；随后只要FlightPhase=Grounded就CompleteMission，没有重新确认实际脚底支持或当前位置在mat内。视觉mat约2 m，旧判定半径18 m。Landing还保留最少前向速度，不能保证按其“descend vertically”提示垂直落在小垫。H只更新任务RTH阶段/提示，不是自动驾驶回航。 |

这些点共同构成一个真实航线合同缺口。不能靠扩大/缩小截图范围、把相机移到海边、改LOW WATER标签或直接把所有负Z标safe来回避。

## 林间通道：已经有的工程基础与尚不能下结论的部分

植被有真实 trunk collision proxy：可见树实例NoCollision，隐藏圆柱代理BlockAll，近场树干/粗枝加入碰撞。`AeroSimCoastalTreesTest.cpp:188` 对实际代理在离地1.5 m/4 m处测通道横向净隙，检查中心无干碰撞、最小约2.5 m、典型4–6 m及局部约束；并检查flank冠基高度。飞机碰撞根是32×28×10 cm盒体。这支持“存在静态几何缝隙”的设计基础，**不支持已证实真实玩家能穿越**。

未知项包括：摄像机避碰会否遮挡入口；局部转弯和高度变化是否与实际动力学匹配；细枝/冠层是否穿机；terrain collision cooking与实际mesh一致性；窄口实际对齐容差；碰撞后Actor/Model分叉造成的卡墙或越点。模型Flight最低前速5 m/s、中立约11.5 m/s，yaw rate50°/s，对应稳态简化转弯半径约5.7/13.2 m（未含速度响应和路径控制）；不能用“缝隙宽过机身”替代弯道可飞性。Roll本身不改变当前前向速度方向，真正转向取决于yaw/pitch，因此输入说明也要与模型一致。

本次没有做森林全路径 swept volume、UE运行或真人操控，均为NOT_RUN。超出安全体不直接把Actor传送回来，故“物理上可能能继续飞但任务判越界”与“可无罚完成任务”必须区分。

## AAA-R3-FLIGHT-02 最小跨文件合同与 owner

**建议唯一 owner：PMO指定 DEV-FLIGHT，统一负责 FlightPawn/FModel、Phase4Mission、GameMode与路线共享接口；QA-MOTION独立执行包内输入验证。** DEV-TERRAIN提供只读真实表面/海面/可降落地支持采样及一段有支撑的岸脚通道；DEV-FLORA提供corridor中心线/净空/碰撞事实，不各自修改飞行规则。UI根据新合同显示高度基准和恢复动作，不改物理规则。先冻结接口/ADR再实现，不能并行各自补一个Z常数。

1. **坐标和高度单真值。** 明确模型相对坐标、世界坐标、起点/海面、AGL与起点相对高度。任务、安全体、恢复点、HUD和trace用显式转换，不能同一个FVector在不同消费者里被当成不同基准。保留正确mat出生与reset；支持世界负Z的正常空中飞行。
2. **支撑驱动Grounded/Landing。** 把“低于起点6 m/负油门”与“真正接触可落地表面”分开。无脚下支撑时负油门是受控下降，不能变成起点等高Grounded。明确安全下降/复飞语义及水面接触后果，不把海水当隐形起降坪。地形/箱/桥/岩壁碰撞后状态须一致；归航落地需低速、正确支撑和实际mat/允许区域判定。
3. **路线与安全体同步。** 从现起飞点连接真实林间入口/出口、真实岩岸、海上低空目标及回航。低海面目标建议先冻结为真实SeaLevel上方3–5 m的有限区域，具体XY必须由terrain提供并查无岩/树。将完整合法路径纳入安全体，保留下限/最高高度/横向边界及恢复策略；不能只放大一个无语义大盒子。保持越界会恢复、进度可保留，不能把合法下海判越界或恢复到崖壁内部。
4. **物理碰撞位置权威。** sweep完成后以实际Actor pose反投回模型；速度、trace、任务计分和恢复从最终解析姿态产生。禁止模型穿墙推进、Actor被挡但任务自动越点；连续碰撞应可稳定停止和再起步。
5. **路线时长实测。** 当前426 m林间控制折线还不含出发、海岸连接、约65 m下降/爬升和返航。60–90秒是否合理须实测而不是把TargetDuration=75当事实。可优化真实路线次序/长短，不能删除低海面终目标；若完整长通道不在首片时长内，明确一个可走的代表段及独立全通道测试，并经PMO冻结。

## 必须补齐的回归

现有 `Phase1.Flight.StateMachine` 测试恰好把Landing→model0视为成功；Phase4 mission loop是直接Observe各检查点坐标的逻辑测试；LaunchSite测试验证静态mat支脚和reset；corridor测试测静态横向代理间距。这些有效但均不覆盖本P0，不得复用其PASS证明真实航线。

最低回归要求：

- 正常Advance从起点上空下降跨越model0到世界SeaLevel+3..5 m，全程Flight且没有无支撑Grounded；空中低负油门与正油门复飞；水接触、低湾地面和mat支持三种不同结果。
- 初始pose非零偏移时，世界/模型/任务/安全/恢复/trace一致；合法海面目标safe、完整corridor合法段safe、真实越界仍unsafe，恢复点不在岩内/树内/水下。
- 使用真实Pawn+世界碰撞测试岩壁/树干/地面阻挡；Actor与model/trace无发散，不能隔墙计分；碰撞停止后有可用恢复路径。
- 实际mat中心及边缘降落、垫外降落和空中假Grounded不可冒充完成；起飞/暂停/失焦/断连/中立倒计时/reset原安全合同保持；负Z处暂停恢复不被重置到model0。
- 用真实语义输入推进代表森林弯道和至少一条完整任务，不直接Teleport/ConfigureReviewFlightState，不向Observe喂预设坐标替代飞行；截图/录像来自同一新包。

## 资源窗口后的真实输入验证步骤

1. 冻结新source/build/asset identity、UE/Mac/RHI/质量档、地图/16:30/seed/FOV和分辨率。正常打开Home→Hangar→Mission→Preflight→Flight；禁止review相机入口。记录起点Actor、model、支脚支持点和海平面，图上确认飞机确在垫上。
2. 用实际键盘或同一语义输入API起飞：Shift升空，W/S姿态、Q/E转向、Ctrl下降。先在开阔区域验证速度/转向/减速，不假定H自动回家。全程记录时间戳、输入、flight phase、实际Actor/model/world/AGL、碰撞与任务进度。
3. 经正常入口飞到森林，沿冻结代表段/完整通道逐个弯道通过；在至少一个2.5–4 m窄口减速。记录入口可读性、摄像机避碰、机体与树碰撞及退出点。无人工瞬移或镜头跳转。
4. 到真实崖缘外有安全余量位置，再下降到SeaLevel+3..5 m；必须看到Actor世界Z为负、距离水面确实只有数米、仍是Flight、无Boundary假报。执行短平移/转向、停降/复飞，并确认未穿崖。不能只按HUD的相对ALT推断离海高度。
5. 自己操控爬升65 m回到作业区，走任务RTH流程，减速落到真实垫内。检查最终支撑/速度/位置而非只看Result出现。记录60–90秒目标是否达成；失败如实FAIL并保留路线，不修改时长数据凑数。
6. 分开做安全负例：岩壁/树碰撞、真实越界、垫外降落、下海水接触、负Z暂停/失焦/断连和reset。不要把危险负例插入成功路线录像后仍宣称无碰撞通过。

本报告退出状态：**只读诊断完成；实现修复、UE回归、包内可达性、连续玩家视频、用户确认均NOT_RUN / pending_user**。等待PMO的FLIGHT-02 owner及QA资源窗口，不自行启动包或占用串行构建资源。
