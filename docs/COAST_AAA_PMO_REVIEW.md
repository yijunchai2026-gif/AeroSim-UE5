# Pacific Coast — R2 现状审查与下一阶段裁决

审查日期：2026-10-18。角色：PMO。基于用户后续工作的实际集成分支，不以旧 v8 评价新版本。本次只做复核与计划调整，没有修改实现、资产、桌面入口或提交代码。

## 当前判断

当前是**可玩的场景 Alpha，完成首轮美术整合，尚未达到高完成度 vertical slice**。与 v8 相比，技术和资产管线已经明显成熟；当前瓶颈是近景形体、材质与生境的一致性、真实航线和运动稳定性。继续增加素材数量或自动化数量，不能代替这些缺口。没有冻结的成品对照和用户认可，不给“3A 完成百分比”。

## 本次直接核对的证据

- 主线：`aime/coast-aaa-integration`，提交 `894ab6a15955409dd72a5f37bb855a5a0d226c91`。自 AAA baseline 起有 31 条可追溯提交，含地形 R2、海天 R3、植被 R2b；这是已经集成的事实，不是待移植计划。
- 当前源码与实机一致：`819c0d34f31dc98b75cf704f390d4b011d5f684940bb8694698192ccfff4cfef`。构建为 UE 5.8.3、Mac Development、Apple M3 Pro / 36 GB、Metal。
- R2 Editor 编译和 Cook/Stage/Package/Archive 日志成功；Python 79 项成功；静态校验日志成功。Automation 为 66 clean + 3 with warnings、0 failed。三个警告分别为 NullRHI 未执行 Metal material compile，以及两次引擎网络探针超时。不能写成“69 项全部 Metal 验证通过”。
- 当前资产 manifest：106 个引擎资产、210 个 source/derived 条目，固定白名单及哈希校验通过。版本字段仍为 `coast-draft-20261018-r1`，不等于当前 R2 包的独立身份；下一批补独立 round/revision/build 绑定，不能仅凭版本名判断新旧。
- 本次重新启动 R2 已打包应用，以 RenderOffscreen + 实际 MCP stdio 抓取，未激活桌面窗口。9 个机位：launch-operator / forest / corridor / overview / cliff / shore / mountain / north / south；forest、cliff、mountain 另拍 lighting_only，共 12 组、22 帧。forest 和 shore 各 6 帧、环境时钟推进，world_paused=false、streaming_wanting_at_capture=0。抓取前后均在 Home，未注入飞行输入。
- 22 个原始 PNG 的 verifier integrity 均 PASS，原图哈希和 canonical manifest 哈希也匹配。审查目录中改名的 PNG 是便利副本，原 manifest 保留原 filename，不能拿改名副本直接通过 filename 校验。原图位置以 `results.json` 记录为准。
- 本次结果：`Artifacts/ProductShell/AAA/R2/pmo-review/results.json`，SHA256 `a12cf8bd81d50ae8488a2acd444f7d9756816a0a97c017fedba7f3c43db951bc`。逐视图原图路径、图像与 manifest 哈希均在其中。全部截图为同源 1920×1080 High、Clear、16:30、seed 21630、vFOV 60。

## 当前水平与缺陷

| 领域 | 已经成立的进步 | 本次实图或代码仍暴露的缺口 | 裁决 |
|---|---|---|---|
| 工程与协作 | 独立 worktree、分支集成、素材来源/哈希、实例与 LOD、无激活视觉 MCP 已落地 | 旧目录、WIP、已集成 R2 的身份仍容易混淆；启动日志出现 ensure | 管线可继续，候选稳定性不通过 |
| 起飞区 / 地面 U5 | 薄垫、箱子与砂砾过渡保留；近地细节明显优于 v8 糊图；有真实草本层 | 大片空地像平坦砂土，草簇分布稀且与主体地面关系弱，中景尚未形成可信草甸 | 部分改善，视觉未通过 |
| 树根树皮 U1 | 蜘蛛腿形根部已不再主导；树皮深色纵向细节清楚，树脚尺度改善 | 树干仍较规则，地面接触/落叶与根颈缺少可信局部形体；不能仅用 collar 系数签外观 | 部分改善，近景未通过 |
| 森林 U2 | 树冠覆盖、密度、多层植物及枝干轮廓显著强于 v8；已有 HISM/LOD 实现 | 近处绿卡片像平片拼接，树木外形与粗细仍重复；corridor 机位看不到足够清楚的入口和连续可飞空隙 | 密度保留，形体与可读性需返工 |
| 山体 U6 | 主脊、支沟、前后层叠、金黄向阳坡与绿色谷地已可读 | 绿色区边界仍像涂色掩码，局部树种/尺寸不连贯，缺少能在正常飞行中成立的 chaparral/woodland 过渡 | 大形提升，生境未完成 |
| 岩岸 U3 | 已打断等高崖顶，出现低湾与支撑坡，岸脚有接触泡沫 | lighting_only 也可见低湾为大块光滑槽面；崖段仍是规则竖切片，重复纹理明显，草土帽与岩面交接突兀 | G2 未通过；暂停进一步靠材质掩盖 |
| 海面 U3 | 深浅色、岸边泡沫可见；6 帧水面有变化 | north/south 都有明显矩形深浅色分界；宽波峰仍近乎平行；shore 的浪花像贴着岸脚的细纹，缺少可信冲散/回落 | 明确视觉失败，优先修复 |
| 天空 U4 | 本次晴空重新可见，远山冷暖层次与大气透视比 R1b 改善 | 金黄地面与绿色冠层的色彩统一仍不足；短序列没有证明完整太阳方向下的反光稳定 | 保留当前方向，暂不再大改天空 |
| 运动与性能 | 本次可直接取得未暂停序列；LOD/剔除与 CPU 几何估计有代码和测试 | 两组约 1.6 秒静止相机序列不能覆盖转向、风动、LOD 穿越或全路线；同步读回影响帧时间 | 未测真实体验与发布性能 |

本次运行日志实际出现：`Ensure condition failed: GetStaticMaterials()[MaterialIndex].UVChannelData.bInitialized`，堆栈经过 UStaticMesh 和 UInstancedStaticMeshComponent 的材质 streaming。应用继续运行，不能写成崩溃，也不能因为截图成功而忽略。必须由 DEV-FLORA 查明哪些运行时构建 mesh/material slot 未初始化，并完成新包无该 ensure 的回归。

forest/shore 的全图缩小 RGB 差值分别约 0.17–0.21 / 1.05–1.19，只说明像素变化，不能直接和旧报告的 canopy ROI 统计比较，不能签“闪烁已解决”。本次没有重新证明 60–90 秒玩家航线。静态代码仍将 flight-model Position.Z 钳到 ≥0，并在低高度负油门进入降落；起飞地面到海面约 65 m 高差，因此低空海面路线有明确实现风险，须作为 P0 可达性 spike 验证，不能用 offshore review 相机替代。

CPU `scene_frustum_triangles_lod0` 是可见界限内实例全部按 LOD0 计算的估计。本次 overview/cliff 约 23.5M/25.4M；这不是当前选中 LOD 的 GPU 实际三角数，更不是 draw calls。帧时间与 GPU 数据来自 Development + 同步截图，仅可定位问题。Metal draw counter 缺失仍保持 NOT_MEASURED，不能用三角数“替代通过”性能门。

## 下一阶段顺序与退出条件

| 顺序 | 负责人 | 工作与交付 | 退出条件 |
|---|---|---|---|
| R2 收口 / P0 | PMO + DEV-FLORA | 冻结本次 revision/source/asset/build，定位并修复 UV streaming ensure；为包生成独立 round manifest | 新包冷启动、进入/退出任务、视角切换无该 ensure、无回退默认材质；不能复用旧包日志 |
| R2 收口 / P0 | DEV-SEASKY + DEV-TERRAIN | 修 north/south 矩形水色边界，联查 shelf/floor/水体吸收与衔接；用白模和 unlit 区分几何/材质原因 | 同包两翼、cliff、shore 静态与移动连续镜头无直线矩形色断层；岸线保持有支撑，不重引入硬台阶 |
| R2 收口 / P0 | PMO 指定飞行 owner + QA-MOTION | 查实际下降/贴地规则、碰撞和训练通道；完成起飞→林缘→岩岸/低空海面→返航一次真实输入录像 | 从正常任务入口可达，不改相机冒充飞行，不穿地/树；如果低海面暂不可达，明确删出本轮试玩路线并保留问题，不宣称闭环 |
| R3 / P1 | DEV-FLORA + ART-FLORA | 在同密度目标下先精修 3 个代表性乔木/幼树 archetype 的近中远轮廓，解决片状冠层、规则树干；塑造可读通道入口 | forest/corridor/player 的低中高机位都可判断可飞开口；近景无大片贴纸感，运动无蓝点/明显 edge shimmer；验证后才扩散全林 |
| R3 / P1 | DEV-TERRAIN + ART-SURFACE | 只精修一个岩岸低湾与一段 30–50 m 林缘；打破光滑槽面和齐整切片，做接地过渡 | lighting_only 先过形体，再看 lit；正常航线近/中机位无光滑大槽、重复竖墙、草土帽硬接缝；再向其他海岬复制 |
| R4 / P1 | DEV-SEASKY + QA-MOTION | 在可信岸形上精修碎浪、湿边、反射、太阳方向下的高光；保持晴空基线 | 固定镜头、平移、转向、接近岸边连续序列中接触位置正确；不出现泡沫白带、移动弧片、摩尔纹或满屏银点 |
| 候选收口 / P2 | PMO + QA | 同包标准 QA、真实任务体验、性能与长稳、桌面入口和回滚 | 正常试玩 60–90 秒不靠提示外人工调试即可完成；静态英雄图与运动都达标；Windows 非 Development 目标验证与正式内容门单列，缺项保持阻塞 |

这不是排定完成日期。每批开始前先给出限定范围与预期对比机位，每批结束给同源 A/B 和缺陷关闭/回退结果。优先顺序是**先稳定和可达，再把三个小区域做对，随后扩散**。暂缓更多材质品种、整片森林再次增密、impostor 扩张、新天气、新平台和 MCP 大功能；新素材只有在关闭具体缺陷、经过资产棚审查后才能进入候选。

## 执行纪律调整

- 保留现有 3 开发、2 素材、2 QA 职责，不再让 7 个角色同时改主线。当前会话最多 4 个活动执行者（含 PMO），按依赖批次安排；开发、素材可在各自 worktree 继续，集成和打包由 PMO 单列车串行执行。
- 增加“飞行/任务可达性 owner”，由 PMO 指定一个现有开发兼任；FlightPawn / GameMode 的修改只有该 owner 与 PMO 集成，不允许环境泳道各自补飞行规则。
- 为共享 importer 细分函数 owner；同一 CoastDraftImportCommandlet 文件不允许 terrain/seasky/art-surface 同时直接改。ART 交付 source + receipt + 验货图，运行时和材质实现分别由唯一 DEV owner 接收。
- 不把“至少 6/8 帧”当成运动验收。保留短序列做快速复现，运动关闭必须加入真实速度的移动/转向、LOD 距离往返及至少一条完整试玩路线，并记录实际时间间隔。连续视频/帧必须同 build。
- lighting_only 对水和 two-sided foliage 有自身渲染限制，作为辅助，不把该模式的水色或叶片明暗当成物理真值；G1 支撑以几何/碰撞/实际路径共同检查。
- G0–G2 有失败时，只允许其修复及不依赖它的独立区域继续；未失败区域不必全部停工。修好之后才在该区域进入 G3，避免整个项目无效串行或用材质遮问题。
- 将已完成的 R1/R2 代码从 TODO 移至历史证据。art-flora 未提交的新 impostor/live-oak 派生物和 terrain 工作树重生成 uasset 均为 WIP，不能并入当前 R2 完成声明，也不得覆盖或清理用户改动。
- 正式具名机型、Windows/iPad 和用户外观确认仍是独立门，不阻止环境工程继续，但不因环境测试通过而解锁。
