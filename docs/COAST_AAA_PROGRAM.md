# Pacific Coast AAA 场景计划（PMO 编制与执行规则）

2026-10-18，R2 复核后修订。PMO-01（root 集成）按用户指令编制。目标是把 Pacific Coast 场景推进到 3A 观感；**AAA 是否达成只由用户判定**，本计划和任何自动化、MCP 截帧都不能代签。G0→G4 顺序、`DRAFT_NOT_USER_CONFIRMED`、证据纪律仍按 `AGENTS.md`。


当前基线更新为 `aime/coast-aaa-integration` / `894ab6a`，源码与本次 R2 实机指纹 `819c0d34f31d…fff4cfef` 一致。阶段判断：可玩的场景 Alpha、首轮美术整合，未达到高完成度 vertical slice 或 AAA。详细实机评估、证据、分批 owner 和退出条件见 [R2 PMO 审查与后续裁决](COAST_AAA_PMO_REVIEW.md)。该审查的批次优先级取代原先从 R0 出发的宽泛并行安排。

最新工程验证已推进到R5：海岸负高度控制、真实支撑/碰撞状态和触水重试完成限定验证，79项Automation、Python82、编译/Cook/Package与Metal专项通过；正常菜单实际按键证据已冻结。具体身份与范围见 [R5控制工程结果](tasks/AAA-R5-CONTROL-RESULT.md)。R2继续作为历史视觉起点；R4实际G2仍FAIL。M1仍缺合法中间航路与完整输入试玩，当前不得称3A候选。后续优先级为导航连接→单处岩壁扫描及三代表树冠→运动和性能，具体合同均已派发。

实际派工与状态见 [3A 执行看板](COAST_AAA_TASK_BOARD.md)。当前 P0 三条任务已派给独立 owner；先完成本批，不同时启动全场美化。

最近执行顺序：P0 修运行时 UV streaming ensure、水面矩形色块与真实航线可达性 → P1 精修代表树/林间通道及一个岩岸低湾 → 海岸运动与性能收口。R2 代码已经集成的内容不再重复派单。新增素材和整片增密暂缓，保持现有成果用于 A/B。

## 1. 用户问题清单（本计划的验收对象）

| # | 用户反馈 | 可检验目标（艺术目标，非工程标准） | 主要视角 |
|---|---|---|---|
| U1 | 树根、树皮不真实 | 去掉“蜘蛛腿”式支脚；根颈平缓外扩（≤约 1.3× 树干半径），在低机位下与地面无缝接触、无悬空和穿插；树皮使用 CC0 照片级 PBR（颜色/法线/粗糙度），近景可见纵向纹理，不是灰色光滑圆柱 | forest、launch-low |
| U2 | 树林密度（用于训练无人机） | **一个固定的高密度**：森林区上层乔木约 300–400 株/公顷，林下灌木和幼树填充；树冠覆盖读起来连续；保留一条可飞的训练通道（通道内树干最小间距约 2.5–4 m，保证有挑战但可通过）；树干有碰撞；确定性 seed；按实例渲染并带 LOD，不再合并成几块巨型程序网格（旧 R0 的合并网格问题已由实例/LOD 实现处理；新批仍须量化实际成本） | forest、overview、player |
| U3 | 海面特别假 | 非重复多频波浪（不能出现平行长条纹）；深浅水颜色和吸收随深度变化；浪花和泡沫只出现在真实岩石接触处和浅滩，断续不连成一条线；岩石近水处变暗变湿；远处反射平稳，不出现满屏银色闪点；连续帧中能看到运动 | cliff、shore、north/south |
| U4 | 天空不像加州 | 10 月 16:30 的加州海岸晴空：天顶深蓝、地平线偏暖的薄霾、太阳低角度暖色；云量少（零星积云或卷云），可选离岸的低海雾带；远山有大气透视（越远越蓝、越淡） | overview、mountain、cliff |
| U5 | 地面很模糊 | 近地纹理密度足够（玩家/低机位不出现被放大的糊图）；加细节纹理和宏观变化来打破平铺；近处有草叶或碎屑等实物几何；草地与碎石、土路自然过渡 | launch-low、launch-operator、forest |
| U6 | 山要像湾区的山 | Santa Cruz Mountains / Diablo Range 风格：圆润主脊，加分叉的冲沟和层叠山脊；按坡向分布植被：向阳坡金黄干草，背阴坡和沟谷是深色橡树林和灌丛（chaparral），局部有针叶林；远处的层叠山脊带雾气 | mountain、overview |

## 2. 团队编制（3 开发 + 2 素材 + 2 视觉测试 + PMO）

均为虚拟职责泳道上的执行者，不代表真人。

| 成员 | 泳道 | 负责的问题 | 唯一写入范围（单 owner） |
|---|---|---|---|
| DEV-TERRAIN | RD-08 地形 | U5、U6 | `PacificCoastTerrain.cpp`、`PacificCoastLandscape.cpp` 中的地面和材质部分、地形测试；已有 R2，后续精修低湾形体和地表衔接 |
| DEV-FLORA | RD-08 植被 | U1、U2 | `PacificCoastTrees.cpp`、新的 habitat 源文件、`HabitatDraftImportCommandlet`、树木和密度测试；Habitat 已整合，先修 ensure、冠层轮廓和通道 |
| DEV-SEASKY | RD-07 环境 | U3、U4 | `PacificCoastOcean.cpp`；`PacificCoastGraybox.cpp` 中的天空、光照、雾和 `ApplyEnvironmentState` 部分；现含海天 R3，先修实图水色边界 |
| ART-FLORA | RD-02 植被素材 | U1、U2 素材 | `Scripts/acquire_coast_habitat.py`、`acquire_coast_tree.py`、`SourceAssets/EnvironmentDraft/{HabitatDraft,PineTree01,Flora*}` |
| ART-SURFACE | RD-02 地表素材 | U3、U5、U6 素材 | `Scripts/acquire_coast_{surfaces,gravel,grass_ground}.py`、`SourceAssets/EnvironmentDraft/{Ground,Rock,GrassGroundDraft,Water*,Sky*}` |
| QA-VISION-HERO | QA-02 | 全部 U1–U6 的静态构图和形体 | 只写 `Artifacts/ProductShell/AAA/<round>/qa-hero/` 和评审报告 |
| QA-VISION-MOTION | QA-02 / RD-09 | U2 航线、U3 水面运动、闪烁和稳定性 | 只写 `Artifacts/ProductShell/AAA/<round>/qa-motion/` 和评审报告 |
| PMO（root） | PMO-01 | 集成、构建、打包、裁决、用户门 | `PacificCoastGraybox.h`、共享 manifest、`docs/environment-draft-assets.json`、本计划和执行状态 |

共享接口规则：
- `PacificCoastGraybox.h` 只有 PMO 能改。开发需要新增声明时，在报告里写明签名，由 PMO 合并。
- 新素材只能来自 CC0 来源（首选 Poly Haven），必须记录 URL、作者、大小、MD5/SHA256，并写进 `ENVIRONMENT_ASSET_SOURCES.md`。来源不明或许可不清的一律拒收。

## 3. 工作方式

- **隔离**：每个开发和素材成员在自己的 git worktree 和分支（`aime/coast-aaa-<lane>`）上工作，起点是基线分支 `aime/20261018-coast-aaa-baseline`。habitat 等半成品在分支 `aime/20261018-coast-aaa-habitat-wip`，由对应泳道用 `git checkout <wip> -- <file>` 取回自己负责的文件。
- **构建资源**：职责角色不等于同时运行数量；当前最多 4 个活动执行者（含 PMO），按依赖批次安排。开发只做自己的编译和相关测试；引擎编译/导入/Cook/打包统一排队，不允许多个 worktree 同时抢占同一引擎。NullRHI 不执行真实 Metal shader gate，该 gate 须另跑并留同源结果。
- **集成列车**：每一轮依次是 开发/素材提交 → PMO 合并到 `aime/coast-aaa-integration` → 静态/Python/Automation/Cook/Package → 启动带 bridge 的包 → 两名 QA 用 MCP 评审 → PMO 汇总缺陷，开始下一轮。
- **门禁顺序**：G0–G2 失败的区域先修形体与可达性，再做其 G3；不依赖该区域的其他泳道可以继续。lighting_only 为辅助，几何/碰撞/正常玩家路线仍须独立核实。

## 4. MCP 视觉测试协议（必须看连续帧）

工具是 `Scripts/aerosim_control_client.py` 驱动的 `aerosim_capture_view`。它是真实的 MCP stdio 调用，不截桌面，也不激活窗口。

- 短帧序列是快速复现，不足以关闭运动缺陷；关闭必须包含实际速度的平移/转向、LOD 往返及正常完整试玩航线。每个英雄视角至少拍：`lit` 连续 6 帧（`frames=6, interval_ms=300`），外加一次 `lighting_only`。水面和树冠视角另加一组 8 帧序列。
- 视角集合：`launch-low`、`launch-operator`、`forest`、`bridge`、`cliff`、`shore`、`mountain`、`overview`、`north`、`south`、`player`。机位与 temporal frame-023 完全一致，可以和 v8 直接对比。
- 报告逐条对应 U1–U6，每条给出 `PASS / FAIL / NOT_RUN` 和理由，附上帧路径、`image_sha256`、source fingerprint 和逐帧 RGB 差值（这个差值只作辅助）。单张静止图不能证明运动；`world_paused=true` 的序列不能作为水面动画证据。
- QA 不能把 Automation PASS、几何预算或差值统计写成视觉通过，也不能签 AAA 或用户验收。

## 5. 当前已知基线事实（R2）

- 当前主线源码、R2 package 和本次真实 MCP capture 指纹一致；22 个原始帧 integrity PASS，9 个固定视角、12 组，含 forest/shore 6 帧未暂停序列及三组中性光照。详见 PMO 审查证据。
- Python 79 成功；Automation 为 66 clean + 3 warning，Metal shader test 在该 NullRHI 批次未运行；Editor 编译和打包成功。启动存在 UV streaming ensure，构建成功不能覆盖它。
- 林木密度、真实树皮、近地细节、山脊层叠、晴空和岸脚泡沫已有进步。片状树冠、弱通道可读性、光滑湾槽和南北矩形水色边界仍是实际图像缺陷，不能沿用“R3 已修”的文字结论关闭。
- CPU 视锥/LOD 几何只作辅助预算，不能替代 draw calls、真实 GPU 或目标平台非 Development 性能门。旧 R0/R1 报告保留作历史；当前等级与批次以 R2 PMO 审查为准。

## 用户裁决记录

- 2026-10-19：可飞航线实测最短 126 s，超出原定 60–90 s。用户先选择提高垂直速度（爬升 8 m/s、下降 6 m/s，R5b 实测 113 s），随后将任务时长目标改为**约 2 分钟（100–130 s）**；水平极速保持 18 m/s，不为凑时长删减海面/林间训练段。
- 2026-10-19：项目启用 Lumen 全局光照与反射（`Config/DefaultEngine.ini` RendererSettings），用于修复林下仅受蓝色天光照明的问题；Mac 目标帧成本须在实机评审中测量，不以截图代替性能门。
