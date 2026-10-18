# Pacific Coast AAA 场景计划（PMO 编制与执行规则）

2026-10-18。PMO-01（root 集成）按用户指令编制。目标是把 Pacific Coast 场景推进到 3A 观感；**AAA 是否达成只由用户判定**，本计划和任何自动化、MCP 截帧都不能代签。G0→G4 顺序、`DRAFT_NOT_USER_CONFIRMED`、证据纪律仍按 `AGENTS.md`。

## 1. 用户问题清单（本计划的验收对象）

| # | 用户反馈 | 可检验目标（艺术目标，非工程标准） | 主要视角 |
|---|---|---|---|
| U1 | 树根、树皮不真实 | 去掉“蜘蛛腿”式支脚；根颈平缓外扩（≤约 1.3× 树干半径），在低机位下与地面无缝接触、无悬空和穿插；树皮使用 CC0 照片级 PBR（颜色/法线/粗糙度），近景可见纵向纹理，不是灰色光滑圆柱 | forest、launch-low |
| U2 | 树林密度（用于训练无人机） | **一个固定的高密度**：森林区上层乔木约 300–400 株/公顷，林下灌木和幼树填充；树冠覆盖读起来连续；保留一条可飞的训练通道（通道内树干最小间距约 2.5–4 m，保证有挑战但可通过）；树干有碰撞；确定性 seed；按实例渲染并带 LOD，不再合并成几块巨型程序网格（当前 forest 视锥内 13 个组件、约 345 万三角形） | forest、overview、player |
| U3 | 海面特别假 | 非重复多频波浪（不能出现平行长条纹）；深浅水颜色和吸收随深度变化；浪花和泡沫只出现在真实岩石接触处和浅滩，断续不连成一条线；岩石近水处变暗变湿；远处反射平稳，不出现满屏银色闪点；连续帧中能看到运动 | cliff、shore、north/south |
| U4 | 天空不像加州 | 10 月 16:30 的加州海岸晴空：天顶深蓝、地平线偏暖的薄霾、太阳低角度暖色；云量少（零星积云或卷云），可选离岸的低海雾带；远山有大气透视（越远越蓝、越淡） | overview、mountain、cliff |
| U5 | 地面很模糊 | 近地纹理密度足够（玩家/低机位不出现被放大的糊图）；加细节纹理和宏观变化来打破平铺；近处有草叶或碎屑等实物几何；草地与碎石、土路自然过渡 | launch-low、launch-operator、forest |
| U6 | 山要像湾区的山 | Santa Cruz Mountains / Diablo Range 风格：圆润主脊，加分叉的冲沟和层叠山脊；按坡向分布植被：向阳坡金黄干草，背阴坡和沟谷是深色橡树林和灌丛（chaparral），局部有针叶林；远处的层叠山脊带雾气 | mountain、overview |

## 2. 团队编制（3 开发 + 2 素材 + 2 视觉测试 + PMO）

均为虚拟职责泳道上的执行者，不代表真人。

| 成员 | 泳道 | 负责的问题 | 唯一写入范围（单 owner） |
|---|---|---|---|
| DEV-TERRAIN | RD-08 地形 | U5、U6 | `PacificCoastTerrain.cpp`、`PacificCoastLandscape.cpp` 中的地面和材质部分、地形测试；从 WIP 移植海岸侵蚀海湾 |
| DEV-FLORA | RD-08 植被 | U1、U2 | `PacificCoastTrees.cpp`、新的 habitat 源文件、`HabitatDraftImportCommandlet`、树木和密度测试；从 WIP 补完 `BuildCoastalHabitat` |
| DEV-SEASKY | RD-07 环境 | U3、U4 | `PacificCoastOcean.cpp`；`PacificCoastGraybox.cpp` 中的天空、光照、雾和 `ApplyEnvironmentState` 部分；从 WIP 移植湿斑移除 |
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
- **构建资源**：开发只编译自己 worktree 的 Editor 目标（`-MaxParallelActions=3`），只跑和自己相关的 NullRHI Automation。Cook、打包和启动统一由 PMO 串行完成，避免 7 个成员争抢 CPU。
- **集成列车**：每一轮依次是 开发/素材提交 → PMO 合并到 `aime/coast-aaa-integration` → 静态/Python/Automation/Cook/Package → 启动带 bridge 的包 → 两名 QA 用 MCP 评审 → PMO 汇总缺陷，开始下一轮。
- **门禁顺序**：G0–G2（形体、支撑、构图）不过，就不做 G3 材质美化。QA 必须先用 `lighting_only` 判断形体，再看 `lit`。

## 4. MCP 视觉测试协议（必须看连续帧）

工具是 `Scripts/aerosim_control_client.py` 驱动的 `aerosim_capture_view`。它是真实的 MCP stdio 调用，不截桌面，也不激活窗口。

- 每个英雄视角至少拍：`lit` 连续 6 帧（`frames=6, interval_ms=300`），外加一次 `lighting_only`。水面和树冠视角另加一组 8 帧序列。
- 视角集合：`launch-low`、`launch-operator`、`forest`、`bridge`、`cliff`、`shore`、`mountain`、`overview`、`north`、`south`、`player`。机位与 temporal frame-023 完全一致，可以和 v8 直接对比。
- 报告逐条对应 U1–U6，每条给出 `PASS / FAIL / NOT_RUN` 和理由，附上帧路径、`image_sha256`、source fingerprint 和逐帧 RGB 差值（这个差值只作辅助）。单张静止图不能证明运动；`world_paused=true` 的序列不能作为水面动画证据。
- QA 不能把 Automation PASS、几何预算或差值统计写成视觉通过，也不能签 AAA 或用户验收。

## 5. 当前已知基线事实（R0）

- 视觉 MCP 已在打包版上实测通过：`lit`、`lighting_only`、`detail_lighting`、`unlit` 正常，连续帧序列正常，相机/HUD/视图模式能恢复，runtime 白名单能拒绝越权请求。`wireframe` 在打包版中只渲染出黑屏和水面，已从白名单移除；拓扑问题请在 Editor 里检查。
- Metal 上 draw call 无法测量（引擎原因，见 `adr/RUNTIME_CONTROL_MCP.md`），改用视锥几何预算作为参考。
- R0 中性光照下的问题：树是阶梯状轮生枝的南洋杉形态，根部是蜘蛛腿；地面平坦无细节；悬崖是一整面直立幕墙；海面无岸边交互。
