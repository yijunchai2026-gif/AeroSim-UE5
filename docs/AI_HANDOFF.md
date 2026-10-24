# AeroSim 海岸场景 3A 迭代 — 给后续 AI 的完整交接指令

最后更新：2026-10-23，交接人 Claude（PMO / 集成）。
仓库内副本：`docs/AI_HANDOFF.md`；最新的工作副本在 `/Users/bytedance/Downloads/AeroSim-worktrees/HANDOFF-FROM-CLAUDE/AI-HANDOFF.md`（不在 git 里）。

---

## 0. 一句话目标

把 UE 5.8.3 无人机模拟器 AeroSim 的 Pacific Coast 场景迭代到用户认可的「3A 级」。改动范围：

- 树：树根、树皮
- 用于 drone 训练的高密度树林（密度固定）
- 真实的海面
- 加州天空
- 清晰的地面
- 湾区风格的山

同时保证无人机能用真实按键飞完约 2 分钟的任务航线并降落。

**是否达到 3A 只由用户本人签字。** 任何 AI 都不得写「3A 已达成 / 验收通过」。

---

## 1. 必读与硬性规则（违反即停）

1. **读仓库规则。** 先读 `AGENTS.md`，再读 `docs/COAST_AAA_PROGRAM.md`（团队章程、U1–U6、MCP 协议、用户决策）和 `docs/COAST_AAA_TASK_BOARD.md`。
2. **不准碰用户主检出。** `/Users/bytedance/Downloads/AeroSim` 的分支是 `aime/20261016-1435-product-shell`，里面有 79 个未提交文件。不要在里面 checkout、merge、reset、stash 或编辑。
   - 只允许 `git branch -f main <commit>` 这类纯指针操作，前提是 `main` 没有被检出。
   - 所有工作都在 `AeroSim-worktrees/` 下的 worktree 里做。
3. **Git 权限。**
   - 禁止 force push。
   - 禁止改远端、分支保护或可见性。
   - 推送前必须得到用户在对话里的明确同意。2026-10-23 用户已同意推送过一次 `main`（到 `7ed9a3b`），这次同意不延续到以后。
   - 合并到 `main` 也要先问用户。
4. **凭据卫生。** 提交里不得含凭据。UAT 打包可能往 `Config/DefaultEngine.ini` 写入 `AndroidFileServer` 段，提交前必须删掉。
5. **证据纪律。** 没有真实运行就写 `NOT_RUN`，不要伪写 PASS。MCP 调用成功不等于视觉通过。
6. **资产文件。** 不要手工合并 `.uasset` 或 `docs/environment-draft-assets.json`。冲突时重跑导入 commandlet（`CoastDraftImport` / `HabitatDraftImport`），再用 `Scripts/write_environment_draft_manifest.py` 重新冻结清单。
7. **控制桥不得激活窗口。** 不要用任何方式把游戏窗口抢到前台。
8. **协调人。** Codex 和 Claude 轮流担任 PMO。开始写代码前先看最新提交和 task board，确认当前没有别人在当 PMO，必要时问用户。

---

## 2. 仓库与 worktree 布局

| 路径 | 分支 | 用途 |
|---|---|---|
| `AeroSim-worktrees/integration` | `aime/coast-aaa-integration` | 集成分支，由 PMO 合并、打包、验证 |
| `AeroSim-worktrees/terrain` | `aime/coast-aaa-terrain` | DEV-TERRAIN：地形、悬崖、岩石、海湾、地面材质 |
| `AeroSim-worktrees/flora` | `aime/coast-aaa-flora` | DEV-FLORA：树、树林密度、走廊、落叶 |
| `AeroSim-worktrees/seasky` | `aime/coast-aaa-seasky` | DEV-SEASKY：海面、泡沫、天空、云、光照（含林下补光） |
| `AeroSim-worktrees/flight` | `aime/coast-aaa-flight` | DEV-FLIGHT：飞行模型、导航航段、任务判定 |
| `AeroSim-worktrees/art-flora`、`art-surface` | 资产泳道 | CC0 资产调研与导入建议，目前空闲 |
| `AeroSim-worktrees/r6-hero-cove` | Codex 的 | 不要动 |

**远端：** `origin = https://github.com/yijunchai2026-gif/AeroSim-UE5.git`。`main` 已快进到 `7ed9a3b`（R10b），推送可能仍在进行中，用 `git fetch origin && git rev-parse --short origin/main` 核对。

**已就绪、尚未合并进 integration 的第 11 轮提交：**

- **terrain `6a839ec`（R11）**：门禁全过。
  - 开发者自测里锈色格子已消失。
  - 海湾补回 24 块 4–6 m 圆石。
  - 北岬改为平缓岩层，人字纹不确定是否消除。
  - 「海岸岩色降饱和」这一步在自测之后才加，还没在游戏里看过。
  - 崖沿锈带、岸边 22% 无特征面积本轮未处理。
  - north 视图里的红色物体不属于地形：射线只打到海面，可能是 HUD、海面或飞行相关的标记。
- **flora `b572662`（R11）**：树冠闭合、加强树根隆起、北边树林轮廓。
  - 林缘闭合那一步还没在游戏里实测。
- **seasky `49aadfa`（R11）**：泡沫常驻边缘、残留、向岸漂移。
  - 游戏内泡沫自测在 2026-10-23 正在进行，结果可能产生新 hash，以海天开发者的最终报告为准。

---

## 3. 构建 / 测试 / 打包

PMO 工具备份在 `HANDOFF-FROM-CLAUDE/pmo-tools/`，包括：

- `integrate.sh`
- `real_input_route.py`
- `perf_soak.py`
- `live_vision_test.py`
- `make_baseline.sh`

建议先复制到自己的 scratchpad 再用。scratchpad 曾被子代理清空过一次。

```bash
cd /Users/bytedance/Downloads/AeroSim-worktrees/integration
bash /Users/bytedance/Downloads/AeroSim-worktrees/HANDOFF-FROM-CLAUDE/pmo-tools/integrate.sh R11
```

**`integrate.sh` 依次执行：**

1. Python 单测，应为 92 OK。
2. 10 个静态校验，应为 10/10。
3. 编辑器构建。
4. NullRHI Automation，必须 0 failed。
5. `BuildCookRun` 打包，参数为 `-SkipZenStore` 和 `-UbtArgs=-WaitMutex -MaxParallelActions=6`。打包前会等待其他 UBT 进程结束，遇到 mutex 冲突会重试。
6. 检查包日志中有没有 "Failed to compile Material"，有就算失败，因为 cooked 材质会回落成棋盘格。

打包产物在 `integration/Artifacts/ProductShell/AAA/<ROUND>/Package/Mac/AeroSim.app`。

**已知引擎坑：**

- Metal 下 `GNumDrawCallsRHI` 恒为 0，draw call 不可测。
- `r.ForceDebugViewModes` 只能在 `SetViewMode` 前后临时开启，常开会让 cooked 截图崩溃。
- Custom 材质表达式的输入只传 RGB，alpha 要从 output 4 单独接。
- `FinishCompilation`、`GetCompileErrors` 和 3 参数 `FStaticMaterial` 只能在编辑器里用，需要 `WITH_EDITOR` 包住。
- Unity build 下会出现 `-Wshadow` 报错，局部变量不要和成员同名。
- **网格距离场已在 R7c 关闭**，原因是它导致延迟卡顿：渲染线程 CPU 从 22 ms 涨到 75 ms。不要再开。
- 关闭距离场后，软件 Lumen GI 对这个项目基本无效。林下亮度靠 seasky 的 `CoastalBounceFill`：向下、无阴影、0 specular 的补光，加上中性的 sky light。不要靠调高 sky light 来提亮，那会让海面发白。

---

## 4. 运行环境坑（视觉 QA 必看）

- **Mac 必须解锁。** 锁屏时（`ioreg -n Root -d1 | grep IOConsoleLocked` 输出 `Yes`），打包的 app 会卡在第 0 帧，`session.json` 永远不会写出。解锁要用户本人来做。
- **必须窗口化启动。** 外接显示器拔掉后全屏会读回纯黑帧。启动命令：
  ```bash
  ".../AeroSim.app/Contents/MacOS/AeroSim" -AeroSimControlBridge -windowed -ResX=1920 -ResY=1080
  ```
  窗口会被限制为 1512×949 并带黑边，测量前先裁掉黑边、缩放到 1920×1080。
- **全黑帧保护。** 提交 `5ea60ca` 之后，全黑捕获会被游戏拒绝，不再报 COMMITTED。
- **同一时间只能有一个控制桥。** 两个活跃桥会让所有调用报 `LIVE_BRIDGE_REQUIRED`。
  - 检查命令用 `pgrep -f "MacOS/AeroSim |UnrealEditor.*-game"`。`pgrep -f AeroSimControlBridge` 会匹配到你自己的 shell，导致自锁。
  - 开发自测排队顺序：terrain → flora → seasky，由 PMO 发 "bridge free"。
- **启动前删旧 session。** 先删 `~/Library/Containers/com.YourCompany.AeroSim/Data/Library/Application Support/Epic/AeroSim/Saved/ControlBridge/session.json`。macOS 会复用 PID；桥现在要求心跳在 8 秒内才算有主，但删掉更稳。
- **页面与 world_paused。**
  - Result 页上 `world_paused=true`，只适合静态视图，动态 QA 需要回到 Flight 页。
  - 从首页进入 Flight：循环调用 `aerosim_ui primary`，直到 `page=='Flight'`。

---

## 5. MCP 控制桥（让模型「看到」画面）

- **组成。**
  - 服务端：`Scripts/aerosim_control_mcp.py`，stdio MCP。
  - 客户端：`Scripts/aerosim_control_client.py`，用法 `Client().call(name, **args)`。
- **工具。**

  | 工具 | 用途 |
  |---|---|
  | `aerosim_inspect` | 读状态：`page`、`actor_cm_*`（世界坐标）、`guide_*`（世界坐标）、`mission_stage`、`next_checkpoint`、`mission_incident` |
  | `aerosim_capture` | 截玩家视角 |
  | `aerosim_capture_view` | 截固定评审视角，参数见下 |
  | `aerosim_press_key` | 真实按键，按住 `duration_ms` |
  | `aerosim_release` | 松开所有按键 |
  | `aerosim_ui` | 操作界面，如 `action='primary'` |

  `aerosim_capture_view` 的参数：
  - `view`，可选值：overview、ground、south、north、forest、corridor、glint、cliff、bridge、mountain、shore、launch-low、launch-operator、launch-return。
  - `view_mode`：lit、lighting_only、detail_lighting、unlit。wireframe 已移除，cooked 包里它是黑的。
  - `frames`：1–8 帧连续采样，配合 `interval_ms` 指定间隔。
- **坐标系。** `position_cm` 是模型空间，相对世界约有 (55, 79, 11) cm 偏移。驾驶和测量一律用 `actor_cm`。
- **航线与按键。** 停机坪中心在世界坐标 (55, 79)，半径 90 cm。航线共 15 段（nav-01 起飞 … nav-15 降落），5 个检查点。按键映射如下，不用 Q/E，机头保持 +X：

  | 轴 | 正向 | 反向 |
  |---|---|---|
  | X | W | S |
  | Y | D | A |
  | Z | LeftShift | LeftControl |

- **降落技巧。** 先悬停对中，水平偏差 < 30 cm 且水平速度 < 20 cm/s，再下降。
  - 在垫子外落地后横移无效，需要 Shift 起飞重新对中。
  - 落在垫子外时 HUD 会提示 OFF PAD。

---

## 6. 团队结构与每轮工作流（3 开发 + 2 资产 + 2 视觉 QA + 1 PMO）

用户批准的结构：3 名开发（另外加了 1 名飞行开发）、2 名资产美术、2 名 MCP 视觉 QA，再加 PMO/集成。

**每一轮 Rn 的步骤：**

1. **出简报。** PMO 根据上一轮两份 QA 报告，给每个开发泳道写简报，列出缺陷、证据帧路径、量化目标和不得回归的项。
2. **开发。**
   - 在自己的 worktree 改代码。
   - 合并最新 integration。
   - 跑门禁：编辑器、游戏目标、Automation、Python、静态校验、Metal shader 检查。
   - 按排队顺序做一次窗口化自测并测量。
   - 提交，报告 hash 和数字。
3. **合并与打包。** PMO 按顺序 `git merge --no-ff <hash>` 进 integration，提交信息末尾加署名行，然后跑 `integrate.sh Rn`。
4. **飞行验证。** PMO 启动打包 app（窗口化），运行 `real_input_route.py <integration> <out>` 跑真实按键全航线并降落。
   - 用 `aerosim_inspect` 确认 `mission_stage==6`、`next_checkpoint==5`、`mission_incident==0`。
5. **QA-HERO。** 独占 app，跑 13 个视图：lit 6 帧加 lighting_only，另有 unlit 和 detail_lighting。
   - 按 U1–U6 逐项给 PASS/FAIL，附量化表和 Top 5 缺陷，每项写明归属泳道。
6. **QA-MOTION。** 独占 app，必须在 Flight 页、未暂停的状态下做。
   - 用 300/600 ms × 6/8 帧序列检查泡沫推进、闪烁、云、glint、涌浪、植被闪烁和 p95。
7. **存报告。** 子代理不能写报告文件，由 PMO 转录到 `Artifacts/ProductShell/AAA/Rn/qa-hero/report.md` 和 `qa-motion/report.md`。
8. **释放并进入下一轮。** 关掉 app，发 "bridge free"，进入 Rn+1。

**量化指标**，QA 口径：裁黑边，缩放到 1920×1080。

| 指标 | 测法 | 目标 |
|---|---|---|
| 树冠绿色占比 | unlit 的 forest/corridor 视图，前 400 行 | ≥ 45% |
| 林下地面颜色 | lit 视图，底部区域 | ≈ (45, 42, 32) |
| 森林视图露天比例 | lit forest 视图，前 400 行 | R9 时约 5%，越低越好 |
| 泡沫 | shore 视图 | 峰值约 6–12% 近水面，泡沫区内开阔水面 > 50%，浪与浪之间也要能看到碎泡沫边 |
| 帧时间 p95 | 1512×949 下 | ≈ 15–24 ms，长时间运行不爬升 |

性能复核可用 `perf_soak.py` 跑 10 分钟。

---

## 7. 当前状态（截至 R10b QA，2026-10-23）

R0–R10b 每轮 U1–U6 全部 FAIL，但改进稳定。

**已解决（QA 确认）：**

- 飞行：真实按键 15 段、5/5 检查点、零事故、降落在垫子上。悬停超调已修。
- 树冠绿色 50–57%；林下地面颜色达标。
- 走廊树木不再排成果园行；走廊小路可读。
- 海面不再发白；远处涌浪和 glint 通过。
- 卷云同向、无旋涡、无放射。
- 海湾崖面的锈色平板和巨型楔形石已去除。
- 性能稳定，没有延迟卡顿。

**待解决（第 11 轮已在处理，提交见第 2 节）：**

1. **[terrain]** 岩石投影把内陆碎石坡画成锈色格子，50–150 m 大小，是当前头号缺陷。
2. **[seasky]** 泡沫占空比太低，静帧几乎看不到；泡沫只在原地翻滚，不向岸漂移。
3. **[flora]**
   - 树根隆起看不见：实际是太细，被地形盖住了。
   - 森林视图树冠稀疏，露天比例 19.6%。
   - 北边树林仍是平行四边形。
4. **[terrain]**
   - 海湾 4–6 m 礁石被删掉了，需要重建。
   - 岸边上部呈平滑橙色沙丘状，崖沿有一条锈带。
   - 北岬底部有人字纹。
   - 岸边崖面 22% 无特征。
   - north 视图底部有一个可疑的红色物体，可能是调试标记。
5. **[terrain]** U5 地面一直没有改善：需要近景草土细节、碎石、反平铺。
6. **[flight/产品]**
   - 真实按键驱动单键脉冲约需 400 s 才走完，脚本控制约 114 s。目标约 2 分钟（100–130 s），需要验证人类飞手能达到。
   - 有一次运行开局就悬空在 z=491、W 键向后飞，原因未复现、未定位。
7. **[PMO / 工具]**
   - TAA 镜头切换残影一直 NOT_RUN，因为桥在捕获前会先等画面稳定。
   - Result 页的 player 视角相机停在 south 姿态。
   - 南岸浪花太远，判断不了，需要一个更近的评审视角。

---

## 8. 可直接粘贴的指令模板

### 8.1 给新 PMO / 集成 AI

```text
你是 AeroSim 海岸场景 3A 迭代的 PMO/集成负责人。先完整阅读
/Users/bytedance/Downloads/AeroSim-worktrees/HANDOFF-FROM-CLAUDE/AI-HANDOFF.md，
再读 integration worktree 里的 AGENTS.md、docs/COAST_AAA_PROGRAM.md、docs/COAST_AAA_TASK_BOARD.md，
以及最近一轮 Artifacts/ProductShell/AAA/R10b/qa-hero/report.md 和 qa-motion/report.md。
不要修改 /Users/bytedance/Downloads/AeroSim 主检出。不要 push、不要合并 main，除非用户在对话里明确同意。
不要声称达到 3A，验收只由用户本人签字。
下一步：
1. 收集 terrain(6a839ec 或更新)、flora(b572662 或更新)、seasky(49aadfa 或其自测后的新 hash) 的 R11 报告；
2. 依次 merge --no-ff 进 aime/coast-aaa-integration，运行 pmo-tools/integrate.sh R11；
3. 确认 Mac 已解锁，窗口化启动 R11 包，运行 real_input_route.py 跑真实按键全航线并降落；
4. 依次派 QA-HERO 和 QA-MOTION（Flight 页、未暂停），把报告转录进 Artifacts；
5. 按报告给各开发泳道写 R12 简报（缺陷 + 证据帧 + 量化目标 + 不得回归项），循环直到用户验收。
```

### 8.2 给开发 AI（把 <LANE> 换成 terrain / flora / seasky / flight）

```text
你是 AeroSim 的 DEV-<LANE> 开发者。工作目录 /Users/bytedance/Downloads/AeroSim-worktrees/<LANE>
（分支 aime/coast-aaa-<LANE>）。先读 HANDOFF-FROM-CLAUDE/AI-HANDOFF.md 第 1、3、4、5、7 节，
以及最新 QA 报告 integration/Artifacts/ProductShell/AAA/<最新轮>/qa-hero/report.md、qa-motion/report.md。
本轮简报：<PMO 粘贴缺陷清单、证据帧、量化目标、不得回归项>。
规则：只改本泳道负责的代码和资产；不手工合并 uasset 或清单，改用导入 commandlet 加清单写入脚本；
不开启网格距离场；不加任何随运行时间累积的效果；不 push。
完成标准：
- 合并最新 aime/coast-aaa-integration；
- 编辑器和游戏目标编译通过；
- NullRHI 全量 Automation 0 failed；
- python3 -m unittest discover -s Scripts/tests 通过；
- 全部 *static_validate.py 和 product_shell_validate.py 通过；
- 改了材质的话，用 Metal 跑 AeroSim.Environment 测试，0 "Failed to compile"；
- 等 PMO 发 "bridge free" 并轮到你后，做一次窗口化 -game 控制桥自测，测量相关指标，测完退出进程；
- 提交，回报 hash、门禁结果、实测数字和未解决项。
```

### 8.3 给视觉 QA AI

```text
你是 AeroSim 的 QA-<HERO|MOTION>。只用 MCP 控制桥（Scripts/aerosim_control_client.py）看画面，不按飞行键。
先读 HANDOFF-FROM-CLAUDE/AI-HANDOFF.md 第 4、5、6、7 节和上一轮同名报告。
app 由 PMO 启动，已窗口化，你独占使用；不要重启它。
HERO：用 aerosim_capture_view 截 13 个视图（lit 6 帧加 lighting_only；forest/corridor/cliff/shore/overview 加 unlit；
forest/cliff/shore/launch-low 加 detail_lighting）。先裁黑边、缩放到 1920×1080 再测量。
按 U1–U6 逐项给 PASS/FAIL 和对比上一轮的结论，附量化表（绿色占比、林下颜色、露天比例、glint 海面颜色、岸边无特征面积）、证据帧路径，以及 Top 5 缺陷并写明归属泳道。
MOTION：必须在 Flight 页、world_paused=false 的状态下，用 frames 6/8、interval 300/600 ms 的序列检查
glint、shore、south、cliff、north、forest、corridor、overview。逐项判断泡沫推进与持续、闪帧、浪花、云是否静止、glint、涌浪、植被闪烁、LOD 跳变和 p95。
报告正文直接回复给 PMO（子代理不能写报告文件）。不得声称达到 3A 或验收通过。
```

---

## 9. 关键文件索引

| 文件 | 内容 |
|---|---|
| `Source/AeroSim/Private/AeroSimControlBridge.cpp` | 控制桥：评审视图捕获、视图模式、心跳 |
| `Source/AeroSim/Private/AeroSimGameMode.cpp` | `ResolveReviewView` 评审姿态、捕获清单、全黑帧拒绝 |
| `Source/AeroSim/Private/FlightPawn.cpp` | 飞行模型：加速度 650、刹车 1500 cm/s²，垂直 8/6 m/s |
| `Source/AeroSim/Private/Phase4Mission.cpp` | 15 段航线、走廊、降落判定（半径 90 cm、<40 cm/s） |
| `Source/AeroSim/Private/PacificCoast*.cpp` | 地形、海洋、树、灰盒，以及各泳道的 `// DEV-<LANE>` 头注释块 |
| `Source/AeroSim/Private/Tests/AeroSimReviewPoseOwnershipTest.cpp` | 从评审视角射线检测每个像素属于哪个组件和材质，定位「谁在画这一块」时用 |
| `Config/DefaultEngine.ini` | 渲染配置：AutoExposure=False，DynamicGI=1（实际按视图关闭），Reflection=1，无距离场 |
| `docs/COAST_AAA_PROGRAM.md` | 章程和用户决策：约 2 分钟航线、Lumen、单一固定高密度 |
| `docs/tasks/AAA-R5-NAVIGATION-GAP.md` | 飞行和导航的历次分析 |
| `Artifacts/ProductShell/AAA/R*/` | 各轮包、日志、路线日志、QA 报告和证据帧 |
