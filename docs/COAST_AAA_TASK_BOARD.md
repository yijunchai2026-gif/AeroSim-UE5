# Pacific Coast 3A 执行看板

2026-10-18，PMO 按用户最新授权派工。最终目标是 **3A 级场景观感和正常飞行试玩体验**；Alpha 只描述当前阶段，不降低最终标准。本看板以 COAST_AAA_PROGRAM.md / COAST_AAA_PMO_REVIEW.md 的实机裁决为依据。没有完成证据不得标 DONE，用户最终验收保持 pending_user。

## 冻结基线与当前批次

集成分支 `aime/coast-aaa-integration`，基线 `894ab6a15955409dd72a5f37bb855a5a0d226c91`，源码指纹 `819c0d34f31dc98b75cf704f390d4b011d5f684940bb8694698192ccfff4cfef`。R2/pmo-review 的 22 个原始帧为本批 A/B 起点。旧目录失败 WIP、art-flora 新派生物与 terrain 未提交 uasset 不属于此基线，不覆盖、不清理。

当前活动上限四人含 PMO；本批为 PMO + STAB + WATER + FLIGHT。三条任务已通过本会话执行者派发；不是新建用户聊天。负责人只在任务范围内工作。此前的共享目录宽泛美化任务已被本批取代。

| 任务 | 优先级 / 负责人 | 状态 | 本批交付 | 关闭条件 |
|---|---|---|---|---|
| AAA-R3-STAB-01 | P0 / DEV-FLORA（vegetation_programmer） | VERIFIED_SCOPED | 在 flora 工作树查明并最小修复 runtime UStaticMesh 材质 UV streaming metadata 未初始化；对应回归检查与报告 | 新包三次冷启动、正常进出任务、capture view 切换无原 ensure；无默认材质回退、无关闭 streaming/ensure 规避；日志与包同源 |
| AAA-R3-WATER-01 | P0 / DEV-SEASKY（terrain_programmer 本批兼任） | VERIFIED_SCOPED | 在 seasky 工作树查明 north/south 矩形海水色块，最小修复实际 shelf/floor/水材质衔接 | north/south/cliff/shore 同包 lit+辅助光照视图、静止及平移/转向无矩形断层、无新悬空岸或硬台阶；保留水体深浅与反射，不能回退不透明蓝片 |
| AAA-R3-FLIGHT-01 | P0 / 任务可玩性（ui_art_review 本批兼任） | DIAGNOSIS_DONE / FLIGHT-02_ACTIVE | 读真实飞行状态机/碰撞/任务，给下降到海面、林间穿行与返航的可达性报告和最小修复合同 | 本任务只关闭“根因与修复合同”；真实输入回归另由 FLIGHT-02 关闭，不将源码分析写成玩家航线 PASS |
| AAA-R3-INTEGRATE-01 | P0 / PMO | WAITING_ON_ABOVE | 独占构建队列、审查差异、集成、重导入必要资产、独立 round manifest、编译/测试/Cook/Package、同包证据及缺陷裁决 | 任务逐项给 PASS/FAIL/NOT_RUN；可独立验收工程批次才按仓库授权 commit/push，不合并 main；P0 失败不得替换桌面试玩入口 |
| AAA-R3-FLIGHT-02 | P0 / PMO 指定 Flight owner + QA-MOTION | DEPENDS_ON_FLIGHT-01 | 按通过的合同修下降/贴地/碰撞并跑正常任务输入链 | 正常入口起飞→林缘训练→岩岸/近海→返航；无调试相机代飞、无穿地/树、暂停失焦仍清零；同 build 正常玩家连续证据；不通过则保留阻塞，不删最终低海面目标 |

负责人安排是本批任务归属，不表示这些角色已经完成任务，也不表示有 7 个执行者并行。角色兼任结束后回到原艺术/开发职责。

## 后续里程碑

| 里程碑 | 范围与负责人 | 应提交的可评审成果 | 不允许的替代交付 |
|---|---|---|---|
| M1 稳定可达候选 | 本批 P0 + Flight owner | 正常入口、相机、输入、安全暂停、地形/海岸、可回航；稳定新包及独立身份记录 | 只展示 offshore 相机或旧包截图；用编译成功掩盖 runtime ensure |
| M2 三个英雄区域 | DEV-FLORA + DEV-TERRAIN，ART 按需供稿，QA-HERO | 起飞区近地表面；30–50 m 林缘与清楚训练入口；一个低湾岩岸。每区先灰模/形体，再表面/接地。树先评三个代表 archetype 的近中远轮廓，通过才铺全林 | 继续全场增密、仅增加贴图分辨率、用光雾掩盖光滑槽面/片状树冠 |
| M3 完整生态与运动 | DEV-FLORA / DEV-SEASKY / DEV-TERRAIN，QA-MOTION | 阳坡干草、谷地 woodland/chaparral、局部针叶林在正常视角构成连续层次；碎浪/湿边、风动、高光、LOD 切换在运动中一致；扩散已通过的英雄区做法 | 新增品种数量代替生境真实；短静止序列代替真实飞行；泡沫白带或银点覆盖海面 |
| M4 画质与运行成本收敛 | PMO + 开发 + 两类 QA | 同 build 明确平台/分辨率/质量档的 CPU/GPU/内存/streaming 证据；无截帧开销的性能/长稳；保留 High 艺术目标与明确降级路径 | CPU 视锥 LOD0 估计充当 GPU draw calls；Mac Development 截图帧率外推 Windows Shipping |
| M5 用户最终验收 | PMO 组织，用户确认 | 能从桌面完整看/玩，三个英雄区与整条 60–90 秒路线均经正常玩家视角审查；最终固定 build 与资产哈希；确认项和未决项清楚 | PMO/QA/AI 代签 3A 或具名机型外观；用阶段标签宣称整体成品完成 |

## 3A 目标的工作标准

这轮不会另创一个“通过自动化就算 3A”的分数。使用同一套可重复的判定：近景有可信尺度、几何细节与材质响应；中景群落和岩体无复制阵列/贴纸感；远景有湾区脊谷和坡向生境层次；运动中不出现浮空、穿模、闪烁、突现、明显 LOD 跳变或水面矩形色断层；任务信息克制清楚、操控可靠。最终按用户认可的艺术方向和实际成品对比审查。正式具名资产和目标平台证据仍按 AGENTS 独立关门。

M1→M2→M3→M4→M5 按证据退出，不给虚构工期；每批 PMO 报“关掉哪些缺陷、真实画面改变、还剩什么”，不只报提交/测试数量。不依赖失败区域的工作可继续，但资源和共享接口必须先排队。

## 文件与资源 owner

- flora：Trees / Habitat 与相关测试；不改资产/importer/共享 header。本批保持现有密度和冠层外观，避免稳定性修复混入艺术变更。
- seasky：Ocean 与 CoastDraftImportCommandlet 的 BuildWaterMaterial 函数及相关测试；不改 Terrain、天空、importer 其余函数或用户 dirty uasset。
- FLIGHT-01：只写 `docs/tasks/AAA-R3-FLIGHT-01.md`。FlightPawn / GameMode 的实现修改在合同审查后由 PMO 统一授权给单一 owner。
- PMO：共享 header、manifest、集成、构建排队、包身份、桌面交付。任何 UE 编译/导入/Automation/Cook 先向 PMO申请独占窗口；叶子执行者当前仅源与静态工作。
- ART-FLORA / ART-SURFACE：本批暂停广泛采集。保留已有未提交资产，只有明确缺陷需要才提供定向方案、许可/来源/哈希与资产棚图。
- QA-HERO / QA-MOTION：从 PMO 冻结的新包开始，分别审查静态形体/构图和正常输入航线/运动，不共用一句“QA PASS”。不激活用户窗口，不同时驱动同一 live bridge。


## 当前批首次根因回报（尚未验证关闭）

- STAB-01：13 个 runtime archetype 在 fast BuildFromMeshDescriptions 后，Bark/Foliage material slot 没有初始化真实 UV density metadata；Editor 路径补齐不代表 cooked runtime 补齐。正在限定范围内修复，不通过关闭 streaming 或仅设空标志绕过。
- WATER-01：外海 Depth 掩码初始浅水值与矩形范围限制共同把没有 shelf/land 支撑的区域当作 250 cm 浅水，进而进入吸收/散射。正在改成依照真实 sea/floor/shelf/land 支撑取深度，并补两翼与负 X 外海回归。
- 两项均为静态根因回报，不是新包验收；修复差异由 PMO 审查后进入唯一引擎队列。

- FLIGHT-01 首次源码回报：非负 model Z、低高度自动 Landing、Phase4 safe bounds/recovery 与 sea/corridor 坐标脱节；LOW WATER 旧点仍在内陆、世界海面上方约 72 m，corridor 一部分越出旧 safe Y。不能只松开 Z clamp 就宣称修好。FLIGHT-02 必须由一个 owner 同时维护高度/真实地面、任务点、安全恢复与碰撞合同，列为艺术扩散前的关键路径。源码确认这些限制；实机全路线验收仍 NOT_RUN。


## 持续执行模式

用户明确要求持续工作直到可用额度耗尽或达到可供最终验收的3A候选。本会话已启用持续目标。PMO不在小批次之间等待“继续”：一个批次进入串行构建时，其他owner处理不冲突的源码/设计/独立QA准备；构建后立即进入真实包评审，失败回到责任owner，成功进入下一批。最终外观确认仍由用户完成，不能用持续运行替代用户门。

R3首批实际进度：两项修复已审查并集成；Mac Editor编译PASS；71项Automation=68clean+3warning、0failed；Python79成功。Cook/Package进行中，UV cooked启动无ensure和海面色块实图尚NOT_RUN。FLIGHT-02已启动独立flight工作树，低湾hero几何与独立QA规格并行。


R3独立工程批次已完成源码/静态/Python/71项NullRHI Automation/编译/Cook/Package/真实Metal专项及3次冷启动/同包29帧检验；详见tasks/AAA-R3-ENGINEERING_RESULT.md。STAB和矩形水色缺陷按该限定范围VERIFIED_SCOPED；整个海面/植被视觉门未关闭。FLIGHT-02实现、低湾hero源增量、三个代表树形体迭代继续，不在此批收口后停等用户。
