# AeroSim 海岸场景 3A 迭代：给后续 AI 的完整交接指令

- **最后更新：** 2026-10-27。交接人是 Claude（PMO / 集成）。
- **仓库内副本：** `docs/AI_HANDOFF.md`，随 `main` 推送。
- **本机工作副本：** `/Users/bytedance/Downloads/AeroSim-worktrees/HANDOFF-FROM-CLAUDE/AI-HANDOFF.md`。这份不在 git 里，可能比仓库内副本新。
- **用户沟通：** 用中文。结论先行，数字要真实，不夸大。

---

## 0. 目标与当前结论

**目标：** 把 UE 5.8.3 无人机模拟器 AeroSim 的 Pacific Coast 场景迭代到用户认可的「3A 级」。用户原话列出的问题：

- 树的树根、树皮
- 树林密度（用来训练 drone，必须是**单一固定高密度**）
- 海面太假
- 天空不像加州
- 地面模糊
- 山要像湾区

另外，无人机必须能用**真实按键**飞完约 2 分钟（100–130 s）的任务航线，并降落在停机坪上。

**当前结论（2026-10-27，R19）：** 还没有达到 3A。

- R0–R21 已完成多轮视觉 QA，每轮都有可量化改进，但最终验收只由用户本人签字。
- **任何 AI 都不得写「3A 已达成」或「验收通过」。**

**最新完成的一轮是 R21**：加入老鹰与动物警觉/逃离/攻击状态，并修复鹿资产没有骨骼 track、动物移动与动画不同步的问题。

- 数量固定为鲸鱼 2、熊 1、鹿 5、老鹰 1；狼和牛均为 0；全部 `NoCollision`。
- 来源、作者、CC BY 4.0 attribution 和归档 hash 见 `THIRD_PARTY_ASSETS.md`；导入资产的逐文件 hash 见 `docs/third-party-wildlife-assets.json`。
- UE 5.8.3 Mac Development Build、Automation 90/90（0 failed）、Package 通过；相关 Python 测试 26/26、静态检查 966/966。
- 详细证据见 `docs/coast-aaa-qa/R21/qa-wildlife-interaction.md`；最新本地包在 `Artifacts/ProductShell/CoastalPackage/Mac/AeroSim.app`。
- 这些结果只构成技术验收，不替代用户最终视觉验收。

**上一轮 R13**（集成提交 `47a652a`）：

- 用户明确要求后已推送到 `origin/aime/coast-aaa-integration`。
- 完整报告在 `docs/coast-aaa-qa/R13/`。

**远端 `main`：** 2026-10-24 经用户同意，R11、方向键操控补丁（`4aa1be9`）和本文档已推送到 `origin/main`。以后再推送仍要先征得用户同意，推送前用 `git ls-remote origin refs/heads/main` 核对远端当前在哪个提交。

**快速运行最新包（给人看）：**

```bash
# 最新本地包：R21 worktree（真实骨骼动画与状态化运动）
open -n "/Users/bytedance/Downloads/AeroSim-worktrees/r14/Artifacts/ProductShell/CoastalPackage/Mac/AeroSim.app" --args -windowed -ResX=1920 -ResY=1080
```

- 在首页一直按 Enter 进入 Flight，然后按 Left Shift 或 ↑ 起飞。
- 按键：W/S 前后，A/D 左右，Left Shift/Left Ctrl 或 ↑/↓ 升降，Q/E 或 ←/→ 转机头，I/K/J/L 云台，C 相机，H 返航，R 重置，Esc 暂停。
- 需要 AI 控制或截图时，在参数里加 `-AeroSimControlBridge`。
- 包在 `Artifacts/` 下，不在 git 里，要在本机重新生成：`bash tools/pmo/integrate.sh <轮次>`。

---

## 1. 硬性规则（违反即停）

1. **先读文档。** 依次读 `AGENTS.md`（仓库总规则）、`docs/COAST_AAA_PROGRAM.md`（团队章程、U1–U6、MCP 视觉测试协议、用户裁决记录）、`docs/COAST_AAA_TASK_BOARD.md`，然后读本文件。
2. **不准碰用户主检出。**
   - `/Users/bytedance/Downloads/AeroSim` 是用户本人使用的检出，分支是 `aime/20261016-1435-product-shell`。2026-10-24 用户自己把它 reset 到了 `origin/main`（`f079290`），工作区干净，以前那 79 个未提交文件已由用户有意丢弃。不管里面现在有什么改动，都视为用户本人的工作。
   - 不要在里面 checkout、merge、reset、stash、clean 或编辑任何文件。
   - 唯一允许的是纯 ref 操作：`git -C /Users/bytedance/Downloads/AeroSim branch -f main <commit>`，前提是 `main` 没有被任何工作区检出（先用 `git worktree list` 确认）。
   - 其余工作都在 `AeroSim-worktrees/` 下的 worktree 中进行。
3. **Git 权限。**
   - 禁止 force push。禁止修改远端、分支保护或仓库可见性。
   - **push 和合并到 `main` 都必须先得到用户在对话中的明确同意。** 每次都要重新确认，同意不跨任务延续。
   - 用户已同意的推送记录：2026-10-23 到 24 推送 `main`（R10b 和本交接文档）。
4. **凭据卫生。**
   - 提交里不得含凭据。
   - UAT 打包可能往 `Config/DefaultEngine.ini` 写入 `AndroidFileServer` 段（含 token），提交前必须删除。`tools/pmo/integrate.sh` 会提醒。
5. **证据纪律。**
   - 没有真实运行就写 `NOT_RUN`。MCP 调用返回 COMMITTED 不等于视觉通过。
   - 开发者自测数据只是参考，以 QA 的测量为准。
6. **资产文件不准手工合并。**
   - 不要手工合并 `.uasset` 和 `docs/environment-draft-assets.json`。
   - 冲突时取一方，然后重跑导入 commandlet（`CoastDraftImport`、`HabitatDraftImport`）。
   - 再用 `Scripts/write_environment_draft_manifest.py` 重新冻结清单，并跑 `Scripts/environment_draft_policy.py`。
7. **控制桥不得激活或抢占窗口前台**（AGENTS.md 安全要求）。
8. **协调人。** Codex 和 Claude 轮流担任 PMO。开工前看最新提交和 `docs/COAST_AAA_TASK_BOARD.md`，确认当前没有别人在当 PMO。不确定就问用户。

---

## 2. 仓库、分支、worktree

| 路径 | 分支 | 负责人 / 用途 |
|---|---|---|
| `/Users/bytedance/Downloads/AeroSim` | `aime/20261016-1435-product-shell` | **用户主检出，禁止触碰** |
| `AeroSim-worktrees/integration` | `aime/coast-aaa-integration` | R13 集成分支；已按用户要求推送到 `47a652a` |
| `AeroSim-worktrees/r14` | `aime/1792907214-r14-visual-fixes` | R14 开发、打包、视觉 QA 与真实输入航线 |
| `AeroSim-worktrees/HANDOFF-FROM-CLAUDE/` | 不在 git 里 | 交接文档工作副本、`pmo-tools/` 脚本备份、旧 diff |

旧 terrain / flora / seasky / flight / art worktree 已在 R14 开工前按用户要求删除；有 dirty 内容的旧 worktree 先 stash 保存后再删除。

**远端：** `origin = https://github.com/yijunchai2026-gif/AeroSim-UE5.git`，使用 Git LFS，`.git/lfs/objects` 约 7 GB。

- 推一次包含资产的提交可能要上传几百 MB（R0→R10b 约 660 MB，318 个 LFS 文件），需要 10–30 分钟。
- 推送时不要中断 `git-lfs pre-push`。

**integration worktree 中有两个未跟踪文件：** `docs/tasks/AAA-R6-HERO-COVE-*.md`。它们属于 Codex，不要提交。

---

## 3. 构建、测试、打包

- **UE 安装路径：** `/Users/Shared/Epic Games/UE_5.8`（UE 5.8.3，Mac，Metal RHI）。
- **PMO 流水线：** `tools/pmo/integrate.sh`。备份在 `HANDOFF-FROM-CLAUDE/pmo-tools/`。

```bash
# 默认对 AeroSim-worktrees/integration 运行；可用环境变量改路径
AEROSIM_INTEGRATION=/Users/bytedance/Downloads/AeroSim-worktrees/integration \
  bash tools/pmo/integrate.sh R11
```

**流水线步骤与期望结果（R10b 的实测值）：**

1. 来源指纹（`Scripts/product_shell_build.py`）写入 `Artifacts/ProductShell/AAA/<R>/logs/identity.txt`。
2. `python3 -m unittest discover -s Scripts/tests`：92 OK。
3. `Scripts/*static_validate.py` 加 `product_shell_validate.py`：10/10 PASS。
4. 编辑器目标编译（`Build.sh AeroSimEditor Mac Development -WaitMutex -MaxParallelActions=6`）。
5. NullRHI 全量 Automation（`Automation RunTests AeroSim`）：**0 failed**。warning 可以接受，例如外网探测超时、NullRHI 跳过 shader、FlightPawn 清理日志。
6. `RunUAT BuildCookRun`：
   - 参数 `-SkipZenStore` 和 `-UbtArgs="-WaitMutex -MaxParallelActions=6"`。
   - 先等其他 worktree 的 `UnrealBuildTool.dll` 进程结束。UAT 多目标调用会忽略 WaitMutex，冲突时会报一个误导性的 `Error_SDKNotFound`，脚本会自动重试。
7. 包日志里出现 "Failed to compile Material" 即 **SHADER FAIL**：cooked 材质会回落成棋盘格默认材质。

**产物：**

- 包：`Artifacts/ProductShell/AAA/<R>/Package/Mac/AeroSim.app`
- 日志：`Artifacts/ProductShell/AAA/<R>/logs/`
- `Artifacts/` 被 .gitignore 忽略

**磁盘：** 每个包约数 GB，磁盘曾经写满过一次，可以删除旧轮次的 `Package/` 目录。

**已知引擎坑：**

- Metal 下 `GNumDrawCallsRHI` 恒为 0，draw call 不可测。清单里 `diagnostic_draw_calls_source` 会标注 UNAVAILABLE。
- `r.ForceDebugViewModes` 只能在 `SetViewMode` 前后临时打开，常开会让 cooked 截图崩溃。wireframe 在 cooked 包里是全黑，已从视图模式中移除。
- 材质 Custom 表达式的输入只传 RGB，alpha 需要从 output 4 单独接。R4 的水面棋盘格事故就是 `Mask.a` 造成的。
- `FinishCompilation`、`GetCompileErrors` 和 3 参数 `FStaticMaterial` 只能在编辑器里用，必须用 `WITH_EDITOR` 包住，游戏目标里用 2 参数版本。
- Unity build 下会报 `-Wshadow`，局部变量不要和其他文件的同名。
- **网格距离场在 R7c 已关闭**（`7d83945`）。开启时运行几分钟后渲染线程 CPU 会从 22 ms 涨到 75 ms，出现延迟卡顿。不要再开，flora 和导入资产也都设置为 resolution 0。
- 关闭距离场后，软件 Lumen GI 在本项目里基本没有效果，seasky 已按视图把 GI 关掉。
  - **林下亮度靠 `CoastalBounceFill`**：向下照射、无阴影、0 specular 的补光，强度 0.28，颜色 (1, .80, .62)，只用于 Clear 天气。sky light 保持中性 0.80。
  - **不要靠调高 sky light 提亮林下。** R9 就是这么做的，结果海面反射发白、发米色。
  - 后处理的 indirect lighting 强度只影响 lightmap，对本项目无效。
- `Config/DefaultEngine.ini [/Script/Engine.RendererSettings]`：`r.DefaultFeature.AutoExposure=False`，`r.DynamicGlobalIlluminationMethod=1`（实际按视图关闭），`r.ReflectionMethod=1`。

---

## 4. 运行环境坑（视觉 QA 和真实按键测试必看）

**Mac 必须处于解锁状态。**

- 锁屏时 `ioreg -n Root -d1 | grep IOConsoleLocked` 输出 `Yes`。这时打包的 app 会卡在第 0 帧，日志里的帧号一直是 `[0]`，`session.json` 永远不会写出，桥也就不会上线。
- 解锁需要用户密码，AI 不能代劳，只能等用户。

**必须窗口化启动。**

- 用户的外接显示器拔掉后，全屏启动会读回纯黑帧，旧 build 也一样。启动命令：
  ```bash
  S="$HOME/Library/Containers/com.YourCompany.AeroSim/Data/Library/Application Support/Epic/AeroSim/Saved/ControlBridge"
  rm -f "$S/session.json"
  open -n ".../AeroSim.app" --args -AeroSimControlBridge -windowed -ResX=1920 -ResY=1080
  ```
- **必须用 `open -n` 让 app 脱离当前 shell。** 如果在同一条命令里用 `nohup ... &` 启动 app 再跑别的脚本，命令结束时窗口会被关闭（日志里是 `ViewportClosed`）。R11 就因此丢过一次 QA。
- 窗口会被限制为 1512×949，画面是带黑边的 16:9（大约第 50–899 行是画面）。测量时先裁掉黑边，再缩放到 1920×1080。
- 提交 `5ea60ca` 之后，全黑捕获会被游戏拒绝，不再报 COMMITTED。

**同一时间只能有一个控制桥。**

- 两个活跃桥会让所有调用报 `LIVE_BRIDGE_REQUIRED`。
- 检查进程用 `pgrep -f "MacOS/AeroSim |UnrealEditor.*-game"`。`pgrep -f AeroSimControlBridge` 会匹配到自己的 shell，曾导致开发者一直等不到空闲。
- 开发者自测的排队顺序：terrain → flora → seasky。由 PMO 发 "bridge free"。

**`session.json`：** macOS 会复用 PID。桥现在要求心跳在 8 秒以内才算有人占用，但启动前删掉旧文件更稳。

**相关路径：**

| 内容 | 路径 |
|---|---|
| 游戏日志 | `~/Library/Containers/com.YourCompany.AeroSim/Data/Library/Logs/AeroSim/AeroSim.log` |
| 截图 | `~/Library/Containers/com.YourCompany.AeroSim/Data/Library/Application Support/Epic/AeroSim/Saved/Screenshots/Product/*.png`，每张配一个 `.json` 清单 |
| 编辑器 `-game` 的桥目录 | `<worktree>/Saved/ControlBridge` |

**页面状态：**

- 首页 → Flight：循环调用 `aerosim_ui action='primary'`，直到 `page=='Flight'`。
- Result 页 `world_paused=true`，只能截静态图，动态 QA 必须重新进入 Flight。
- Result 页的 player 相机停在 south 姿态，不能用来做比较。

**异常记录：**

- 有一次真实按键运行，开局就悬空在 z=491、处于 nav-02，按 W 向后飞了 290 m，撞边界后暂停。驱动连续按 Enter，把暂停菜单导航到 Settings，游戏随即退出。
- 这个现象没有复现。驱动现在连续暂停时最多按 3 次 Enter，然后中止。

---

## 5. MCP 控制桥（让模型「看到」画面）

**组成：**

- 运行时：`Source/AeroSim/Private/AeroSimControlBridge.cpp`。用 opt-in 文件邮箱通信（`session.json` 心跳、`request.json`/`response.json`），启动参数 `-AeroSimControlBridge`，审计日志写 `audit.jsonl`。
- MCP 服务端：`Scripts/aerosim_control_mcp.py`（stdio）。
- Python 客户端：`Scripts/aerosim_control_client.py`，用法 `Client().call(name, **args)`，出错时抛 RuntimeError。

**工具：**

| 工具 | 用途 |
|---|---|
| `aerosim_inspect` | 读状态（字段见下） |
| `aerosim_capture` | 玩家视角截图 |
| `aerosim_capture_view` | 固定评审视角截图（参数见下） |
| `aerosim_press_key` | 真实按键，按住 `duration_ms`（50–2000），租约到期自动释放 |
| `aerosim_release` | 松开所有按键 |
| `aerosim_ui` | 界面操作，如 `action='primary'` |

`aerosim_inspect` 的字段：

- 页面与暂停：`page`、`world_paused`、`phase`（4 = Paused）。
- 位置：`actor_cm_x/y/z`，世界坐标。
- 导航：`guide_leg_id`、`guide_leg_index`、`guide_point_cm_*`、`guide_target_cm_*`、`guide_recovery`。
- 任务：`mission_stage`（6 = Result）、`next_checkpoint`、`mission_incident`（0 = None）。
- 恢复相关：`resume_pending`、`physical_neutral`、`application_active`。

`aerosim_capture_view` 的参数：

- `view`：overview、ground、south、north、forest、corridor、glint、cliff、bridge、mountain、shore、launch-low、launch-operator、launch-return。位置定义在 `AAeroSimGameMode::ResolveReviewView`。
- `view_mode`：lit、lighting_only、detail_lighting、unlit。
- `frames`：1–8，配合 `interval_ms` 截连续帧。

每张截图的清单里有：`diagnostic_frame_p95_ms`、`diagnostic_cpu_render_ms`、`diagnostic_gpu_ms`、`world_paused`、`source_fingerprint`、`build_id`、`viewport_width/height`、`image_sha256`。

**坐标：** `position_cm` 是模型空间，和世界坐标有约 (55, 79, 11) cm 的偏移。驾驶和测量一律用 `actor_cm`。

**航线：**

- 停机坪中心在世界坐标 (55, 79)，半径 90 cm。
- 15 段航线：nav-01 起飞 → 草地 → 内陆到悬崖 → 出海 → 下降 → 贴水沿岸 → 爬升 → 回到悬崖 → 田野 → 进入森林 → 走廊 → 走廊折返 → 出森林 → 返航 → nav-15 降落。
- 共 5 个检查点。
- 降落判定：Grounded、在垫子半径内、速度 < 40 cm/s。

**按键映射：** 不用 Q/E，机头保持 +X。

| 轴 | 正向 | 反向 |
|---|---|---|
| X | W | S |
| Y | D | A |
| Z | LeftShift 或 Up | LeftControl 或 Down |
| 机头（不用于驾驶脚本） | E 或 Right | Q 或 Left |

方向键是给笔记本用户准备的，2026-10-24 按用户要求加入，相当于 Mode 2 左摇杆。云台从方向键改到了 I/K/J/L。控制桥的 `aerosim_press_key` 也接受 `Up`/`Down`/`Left`/`Right`。

**飞行参数（R8 之后）：**

- 加速 650 cm/s²，刹车 1500 cm/s²，不会超调。
- 60 ms 点按位移约 17 cm，100 ms 点按约 29 cm。
- 垂直速度：爬升 8 m/s，下降 6 m/s。
- 全杆最高约 17 m/s。

**降落技巧：**

- 先悬停对中：水平偏差 < 30 cm，水平速度 < 20 cm/s，然后再下降。
- 如果落在垫子外，落地后横向按键无效。HUD 会显示 OFF PAD 提示，需要按 Shift 起飞重新对中。

---

## 6. 团队与每轮工作流

**用户批准的编制：** 3 名开发（另有 1 名飞行开发）、2 名素材、2 名 MCP 视觉 QA，加 PMO/集成。

**每一轮 Rn：**

1. **写简报。** PMO 根据上一轮两份 QA 报告给每个开发泳道写简报，包含：缺陷、证据帧路径、量化目标、根因猜测、**不得回归项**。
2. **开发者干活。** 在自己的 worktree 修改，合并最新 integration，然后跑门禁：
   - 编辑器加游戏目标编译。
   - NullRHI 全量 Automation。
   - Python 测试和静态校验。
   - 改了材质的话，用 Metal 跑 `AeroSim.Environment`，0 "Failed to compile"。
   - 资产有变化的话重新导入、重新冻结清单。

   之后按排队顺序做**一次**窗口化 `-game` 桥自测并测量，测完退出进程，提交，回报 hash、门禁结果和数字。
3. **合并与打包。** PMO 依次执行 `git merge --no-ff <hash> -m "merge: DEV-<LANE> Rn (…)"`，提交信息末尾带 AI 署名行，然后跑 `tools/pmo/integrate.sh Rn`。
4. **真实按键验证。** PMO 窗口化启动 app，运行：
   ```bash
   python3 tools/pmo/real_input_route.py <integration> <integration>/Artifacts/ProductShell/AAA/Rn/route
   ```
   用 `aerosim_inspect` 确认 `mission_stage==6`、`next_checkpoint==5`、`mission_incident==0`。
5. **QA-HERO。** 独占 app，Result 页的静态画面也可以。
   - 截 13 个视图：lit 6 帧加 lighting_only；forest、corridor、cliff、shore、overview 加 unlit；forest、cliff、shore、launch-low 加 detail_lighting。
   - 按 U1–U6 逐项给 PASS/FAIL，附量化表和 Top 5 缺陷，每项写明归属泳道。
6. **QA-MOTION。** 独占 app，**必须在 Flight 页、未暂停**的状态下进行。
   - 截 300/600 ms × 6/8 帧序列。
   - 检查泡沫推进与持续、闪帧、浪花、云是否静止、glint、涌浪、植被闪烁、LOD 跳变和 p95。
7. **存报告。** 子代理不能写报告文件，由 PMO 转录到 `Artifacts/ProductShell/AAA/Rn/qa-*/report.md`。**同时复制一份到 `docs/coast-aaa-qa/Rn/`**，因为 Artifacts 不进 git。
8. **收尾。** 关掉 app，发 "bridge free"，进入 Rn+1。

**量化指标（QA 口径，裁黑边后缩放到 1920×1080）：**

| 指标 | 测法 | 目标 | R10b 实测 |
|---|---|---|---|
| 树冠绿色占比 | unlit forest/corridor，前 400 行，G 比 R 和 B 都高 10% 以上 | ≥ 45% | 50.5% / 57.0% |
| 林下地面 lit RGB | forest/corridor 底部区域 | ≈ (45, 42, 32) | (47, 44, 34) / (47, 43, 32) |
| 森林露天比例 | lit forest，前 400 行 | 尽量低，R9 是 4.5% | 19.6%（回归） |
| glint 视图下部海面 RGB | lit glint | 蓝色，不发白 | (46, 75, 91) |
| 岸边无特征面积 | lighting_only shore | 越低越好 | 22.3% |
| 泡沫 | shore 序列 | 峰值约 6–12% 近水面；泡沫区内开阔水面 > 50%；浪与浪之间也要看到碎泡沫边 | 峰值 2.5%，浪间为 0 |
| p95 帧时间 | 1512×949 | ≈ 15–24 ms，长时间运行不爬升 | 15.4–24.3 ms |
| 性能长测 | `tools/pmo/perf_soak.py <integration> <out> 10` | 10 分钟不爬升 | R7c：23–25 ms，稳定 |

---

## 7. 轮次历史（简要）

完整 QA 报告见 `docs/coast-aaa-qa/R*/`，其中 R3、R4 两轮没有报告。

| 轮次 | 主要合并内容 | QA 关键结论 |
|---|---|---|
| R0 | 基线，无合并 | 灰模，全部 FAIL |
| R1 | Gerstner 海面和加州天空；实例化湾区树林、树根领、CC0 树皮；湾区山脊和沟壑、CC0 分层地面 | 大幅改观 |
| R2 | 云量修复、Metal 材质编译测试；逐像素生境地面、十月配色、反平铺；更饱满的树冠、深色树皮 | — |
| R3–R5 | 阳光 glint、泡沫推进、浪群；走廊可读、尺寸分级；岩石湿润、沟壑、英雄海湾 CC0 扫描；导航引导层、按米校验路线；垂直速度上限 8/6 m/s，路线约 113 s | — |
| R6 | Lumen GI 覆盖、advect 泡沫；17 种树型全部用枝条树冠；扫描法线、海湾崩塌体；可穿越的过渡门 | — |
| R7 | **性能：静态天空捕获、半质量 GI**；远景树冠代理；阶梯式悬崖；R7c 关闭距离场，延迟卡顿消失 | — |
| R8 | 去掉崩塌体托盘、岩面按面投影；圆形远景树冠壳、树干粗细变化；定向卷云、远海涌浪；**飞行：限制点按幅度和刹车、OFF PAD 提示** | 地面变暗（回归）；全屏黑帧事故 |
| R9 | 天光补林下、按地形计算泡沫距离；树根隆起、不规则林缘；岩带色调 | 两项量化目标首次达标；出现不透明泡沫片和果园行 |
| R10 / R10b | 大理石纹泡沫、按天空方向映射的卷云、中性天光加 0-specular 补光；错开走廊退距；R10b 岩石投影改为 >35° 生效 | 海面发白修复；出现**山坡锈色格子**；泡沫过少 |
| R11 补丁（`4aa1be9`） | 笔记本方向键布局：↑/↓ 升降，←/→ 转向，云台改到 I/K/J/L。方向键加入暂停恢复时的按键回中检查、控制桥按键白名单和 HUD 提示。在 R11-arrows 包上用控制桥实测通过 | 用户反馈「操控不太对」后追加；场景没有变化 |
| R11 | 岩石投影只限海岸带、内陆碎石按坡度采样、恢复 24 块海湾圆石；树冠闭合、树根加粗；常驻碎泡沫边、浪后残留、漂移 | 格子消失；海湾和泡沫迄今最好；树冠仍偏稀，摆动变大；林下略暗；岸边仍像沙丘 |

---

## 8. 第 11 轮（R11）：已完成（合并、打包、真实按键验证、两轮 QA）

**R11 之后的操控补丁（`4aa1be9`）：**

- 用户用笔记本试飞，按 Shift/Ctrl 不方便，于是加了方向键：↑/↓ 升降，←/→ 转机头，相当于 Mode 2 左摇杆；WASD 不变。
- 原本在方向键上的相机云台改到 I/K/J/L。
- 包 `flight/Artifacts/ProductShell/AAA/R11-arrows` 的门禁全部通过：Python 92、静态 10/10、Automation 0 失败、打包和 shader 通过。
- 用控制桥在包里实测：↑ 起飞爬升约 13 m，→ 转向 50.6°，W 沿新机头方向前进，↓ 下降。
- 用户还没给出手感反馈。如果要调爬升、下降、转向速度或刹车，参数在 `FlightPawn.cpp` 的 `AeroSimFlight` 命名空间里，见 R8 精确悬停。
- **已知未改：** 手柄映射不是标准 Mode 2。现在转向在右摇杆 X，平移在左摇杆 X，和习惯相反。是否改成标准布局要先问用户。

**集成过程：**

- 依次合并 terrain `6a839ec`、flora `b572662`、seasky `2261044`，得到集成提交 `02874d5`。
- seasky 合并时，水面材质 uasset 和资产清单冲突：terrain 重新导入时带进了旧版水面。
  - 水面材质取 seasky 版本（sha 前缀 `f0caa91e`）。
  - 清单用 `write_environment_draft_manifest.py` 重新生成，117 个资产，policy 校验通过。
- 门禁：Python 92 OK，静态 10/10，编译 PASS，Automation 83 成功 + 9 警告、0 失败，打包 PASS，SHADERS PASS。
- 真实按键全航线：Result 页，stage 6，5/5 检查点，0 事故，418 s。
- 第一次 QA 前 app 窗口被关闭了：在同一条命令里用 nohup 启动 app 并跑驾驶脚本，命令结束时触发了 `ViewportClosed`。之后改用 `open -n` 启动，见第 4 节。
- QA-HERO 在 Flight 页（未暂停）截了 100 帧。QA-MOTION 离线分析 QA-HERO 的实时 6 帧序列，帧间隔约 0.31 s。600 ms × 8 的长窗口序列没有截，是 NOT_RUN。

**R11 QA 结论：U1–U6 仍全部 FAIL。** 相比 R10b：

| 指标 | R10b | R11 | 目标 |
|---|---|---|---|
| 树冠绿色占比（forest / corridor） | 50.5 / 57.0% | 50.7 / 51.7% | ≥45% ✔ |
| 森林露天比例 | 19.6% | 11.7% | ≈5% ✘ |
| 林下 RGB（forest） | 47,44,34 | 42,41,30 | ≈45,42,32（略暗） |
| 林下 RGB（corridor） | 47,43,32 | 40.5,38,29 | ≈45,42,32（略暗） |
| glint 海面颜色 | 46,75,91 | 47,75,91 | 保持 ✔ |
| 泡沫像素，shore / cliff | 0 / 0.03% | 1.7 / 0.8% | 静帧可见 ✔ |
| 山坡锈色像素 | 1.2% | 0.0% | ✔ |
| 岸边无特征面积 | 22.3% | 22.1% | — |

**已解决：**

- 山坡锈色格子消失，山坡呈金草加淡紫碎石，有湾区感。
- 海湾约 24 块 4–6 m 圆石恢复，泡沫沿礁石分布。QA 评价「迄今最好的海湾」。
- 岸边碎泡沫边常驻，开阔水面 > 50%，静帧可见。
- 走廊树根隆起可见。
- north 视图的红色物体消失。
- 运动方面：卷云、glint、涌浪、泡沫细丝都不闪，p95 15.3–23.9 ms。

**R12 简报要点：**

1. **[terrain]**
   - 岸边上部仍是平滑黄褐色沙丘，需要断裂岩体的形态，不能只调颜色。
   - 崖沿锈带。
   - 岸边 22% 无特征。
   - 北岬人字纹仍在，岩层形状没有变化。
2. **[flora]**
   - 上层树冠仍稀疏（11.7%，目标约 5%）。
   - 北边树林仍是方块。
   - **森林树冠摆动幅度是 R10b 的约 4 倍：** 每 MP 变化像素从 279 升到 1,178。摆动是平滑的，但需要检查风的幅度。
   - 树根张开大多不到 1.3 倍半径。
3. **[seasky]**
   - 林下又暗了约 10%，需要恢复。
   - 岸边视角下泡沫是沿岸方向漂移（约 60 px/s），看不到向岸的分量；悬崖的泡沫边不漂移。需要按 1.2 m/s 向岸的设计核对流向。
4. **[terrain/flora，U5]** 近地面草和碎屑几何仍然缺失，起飞点和玩家视角高度都看得出来。
5. **[PMO]** 补测 shore 和 cliff 的 600 ms × 8 实时序列，覆盖完整的浪涌—消退周期。

## 9. 未解决问题总表（按优先级）

1. **[terrain]** U5 地面：近景草土细节、碎石、反平铺。这一项各轮几乎没有变化，需要专门立项，可以让素材泳道引入 CC0 近景地表和 decal。
2. **[terrain]** 岸边上部的橙色沙丘感和崖沿锈带；北岬人字纹；岸边 22% 无特征（英雄扫描旁边的平面代理和扶壁，加起伏会挡住扫描）。
3. **[flora]** 北边树林轮廓；1.3 倍树根张开；远景树冠和近景树冠的一致性。
4. **[seasky]** 泡沫向岸流向（R11 中看起来是沿岸漂移），林下亮度回调；南岸浪花太远无法判断，需要一个更近的评审姿态（PMO 在 `ResolveReviewView` 里添加）；海洋层和卷云的细节。
5. **[flight / 产品]**
   - 真实按键驱动是单键脉冲，跑完约 400 s；脚本化控制约 114 s。需要验证人类飞手能做到约 2 分钟，比如做多键同时按的驱动，或者请用户试飞。
   - 「开局悬空、W 向后飞」的异常需要定位。
6. **[PMO / 工具]**
   - TAA 镜头切换残影一直没测：桥在捕获前会先等画面稳定，需要新增一种不等稳定的捕获模式。
   - Result 页的 player 相机停在 south 姿态。
7. **[性能]** 每轮在 1512×949 下 p95 都正常。正式性能判定需要 Shipping 构建加原生分辨率加 10 分钟长测，目前还没做。

---

## 10. 经验教训（避免重复踩坑）

- **先找「谁在画」，再改。** 崖沿锈色平板改了三轮材质都没变化，最后用 `AeroSimReviewPoseOwnershipTest` 射线探针才发现那是 47–50° 的地面三角形，走的是碎石层，而不是岩石材质。遇到「改了没变化」时，先用探针确认归属。
- **一个改动会影响多个指标。** 调高 sky light 让林下变亮，同时也让海面发白；岩石投影修好了崖面，同时把内陆碎石画成了格子；泡沫从「一片白」修到「几乎没有」。简报里必须写明「不得回归项」，QA 每轮都要回测。
- **所有成功都要有画面证据。** 打包门禁全绿也可能是全黑帧，所以加了全黑帧拒绝。开发者自测数字和 QA 的数字口径可能差 4–6 个百分点，以 QA 为准。
- **子代理会触发用量上限中断。** 中断后用原 agent 继续，不要新开。scratchpad 可能被别的子代理清空，重要脚本要放进仓库（`tools/pmo/`）或备份目录。
- **桥只能单占。** 两个以上的进程要用桥时必须排队，并显式发 "bridge free"。

---

## 11. 可直接粘贴的指令模板

### 11.1 给新 PMO / 集成 AI

```text
你是 AeroSim 海岸场景 3A 迭代的 PMO/集成负责人，用中文和用户沟通。
先完整阅读 /Users/bytedance/Downloads/AeroSim-worktrees/integration/docs/AI_HANDOFF.md
（或本机更新的副本 /Users/bytedance/Downloads/AeroSim-worktrees/HANDOFF-FROM-CLAUDE/AI-HANDOFF.md），
再读 AGENTS.md、docs/COAST_AAA_PROGRAM.md、docs/COAST_AAA_TASK_BOARD.md、docs/coast-aaa-qa/R11/。
硬性约束：不修改 /Users/bytedance/Downloads/AeroSim 主检出；push 和合并 main 前必须得到用户的明确同意；
不 force push；不声称达到 3A。
R11 已完成（见第 8 节）。第一步：按第 8 节「R12 简报要点」和 docs/coast-aaa-qa/R11/ 给各开发泳道写 R12 简报
（缺陷、证据帧、量化目标、不得回归项），然后按第 6 节流程执行：开发 → 合并 → tools/pmo/integrate.sh R12
→ 确认 Mac 已解锁，用 open -n 窗口化启动包 → tools/pmo/real_input_route.py 真实按键全航线
→ QA-HERO、QA-MOTION → 报告存到 Artifacts 和 docs/coast-aaa-qa/R12/。循环迭代，直到用户验收。
```

### 11.2 给开发 AI（把 `<LANE>` 换成 terrain / flora / seasky / flight）

```text
你是 AeroSim 的 DEV-<LANE> 开发者。工作目录 /Users/bytedance/Downloads/AeroSim-worktrees/<LANE>
（分支 aime/coast-aaa-<LANE>）。先读 integration/docs/AI_HANDOFF.md 第 1、3、4、5、8、9、10 节，
以及最新的 QA 报告 docs/coast-aaa-qa/<最新轮>/qa-hero.md 和 qa-motion.md。
本轮简报：<PMO 粘贴缺陷清单、证据帧、量化目标、不得回归项>。
规则：
- 只改本泳道负责的代码和资产；
- 不手工合并 uasset 和资产清单（用导入 commandlet 加 write_environment_draft_manifest.py）；
- 不开网格距离场；不加任何随运行时间累积的效果；不 push。
完成标准：
1. 合并最新 aime/coast-aaa-integration；
2. 编辑器和游戏目标编译通过；
3. NullRHI 全量 Automation 0 failed；
4. python3 -m unittest discover -s Scripts/tests 通过；
5. 全部 *static_validate.py 和 product_shell_validate.py 通过；
6. 如果改了材质，用 Metal 跑 AeroSim.Environment，0 "Failed to compile"；
7. 等 PMO 发 "bridge free" 并确认 pgrep -f "MacOS/AeroSim |UnrealEditor.*-game" 为空后，
   做一次窗口化 -game 桥自测（-windowed），测量相关指标，测完退出进程；
8. 提交，回报 hash、门禁结果、实测数字和未解决项。
```

### 11.3 给视觉 QA AI

```text
你是 AeroSim 的 QA-<HERO|MOTION>。只用 MCP 控制桥（Scripts/aerosim_control_client.py）看画面，不按飞行键，
不重启 app（由 PMO 窗口化启动，你独占使用）。
先读 integration/docs/AI_HANDOFF.md 第 4、5、6、8、9 节，以及 docs/coast-aaa-qa/ 里上一轮的同名报告。
HERO：
- 用 aerosim_capture_view 截 13 个视图：lit 6 帧加 lighting_only；forest、corridor、cliff、shore、overview 加 unlit；
  forest、cliff、shore、launch-low 加 detail_lighting。
- 先裁掉黑边、缩放到 1920×1080 再测量。
- U1–U6 逐项给出 PASS/FAIL，并和上一轮对比。
- 附量化表：绿色占比、林下 RGB、露天比例、glint 海面颜色、岸边无特征面积。
- 附证据帧路径和 Top 5 缺陷，每项写明归属泳道。
MOTION：
- 必须在 Flight 页、world_paused=false 的状态下进行。
- 用 frames 6/8、interval 300/600 ms 的序列检查 glint、shore、south、cliff、north、forest、corridor、overview。
- 逐项判断：泡沫推进与持续、闪帧、浪花、云是否静止、glint、涌浪、植被闪烁、LOD 跳变、p95。
报告正文直接回复给 PMO（子代理不能写报告文件）。不得声称达到 3A 或验收通过。
```

---

## 12. 关键文件索引

| 文件 | 内容 |
|---|---|
| `Source/AeroSim/Private/AeroSimControlBridge.cpp` | 控制桥：评审视图捕获、视图模式、心跳与所有权 |
| `Source/AeroSim/Private/AeroSimGameMode.cpp` | `ResolveReviewView` 评审姿态、捕获清单、诊断字段、全黑帧拒绝 |
| `Source/AeroSim/Private/FlightPawn.cpp` | 飞行模型（R8 精确悬停） |
| `Source/AeroSim/Private/Phase4Mission.cpp` | 15 段航线、走廊、接受窗口、降落判定 |
| `Source/AeroSim/Private/PacificCoast*.cpp` | 地形、悬崖、海洋、树、灰盒。各泳道在文件头维护 `// DEV-<LANE>` 注释块 |
| `Source/AeroSim/Private/Tests/AeroSimReviewPoseOwnershipTest.cpp` | 从评审视角射线探测每个网格单元属于哪个组件和材质 |
| `Source/AeroSim/Private/Tests/AeroSimCoastal*Test.cpp` | 生境、海天、地形等自动化测试 |
| `Scripts/aerosim_control_mcp.py`、`Scripts/aerosim_control_client.py` | MCP 服务端和客户端 |
| `Scripts/environment_draft_policy.py`、`Scripts/write_environment_draft_manifest.py` | 草稿资产白名单和清单 |
| `tools/pmo/integrate.sh`、`real_input_route.py`、`perf_soak.py` | PMO 流水线、真实按键闭环驾驶、10 分钟性能长测 |
| `docs/COAST_AAA_PROGRAM.md` | 章程和用户裁决：约 2 分钟航线、Lumen、单一固定高密度、垂直速度上限 |
| `docs/COAST_AAA_TASK_BOARD.md` | 任务板 |
| `docs/tasks/AAA-R5-NAVIGATION-GAP.md` | 飞行和导航的历次根因分析（含 R8 悬停） |
| `docs/coast-aaa-qa/R*/qa-hero.md`、`qa-motion.md` | 历轮 QA 报告归档 |
| `Artifacts/ProductShell/AAA/R*/` | 本机产物：包、日志、路线日志、证据帧（gitignored） |


---

## 13. R15 连续视觉优化（2026-10-25）

R15 在 R14 分辨率修复之后继续处理六项残留，工作分支仍为 `aime/1792907214-r14-visual-fixes`，未经用户要求不 push。

- 地形：岸壁层理阈值加入近/远空间扰动；远山边界加入低频 jitter，根部从海平面下 50 m 调到 70 m，并降低规则噪声。
- 树木：LOD2 树干从 5 边提升到 8 边；LOD2 增加低成本三边地表根；north grove 生境和生成范围扩展。
- 地被：开放地以 4% 接受率布置低成本 exposed-grass tufts，避免恢复 R13 曾触发预算的高密度实例方案。
- 天空：卷云三层纤维使用不同 shear / rotated coordinates / low-frequency drift，方位和仰角 warp 进一步分离，并提高稀疏阈值。
- 门禁：Python 92/92；Mac Development Build 通过；全量 Automation 92 项 0 failed；最终 SeaSky 定向 Automation 7 项 0 failed；Package ExitCode 0。
- 证据：`Artifacts/ProductShell/AAA/R15/qa-final/` 24 张；`docs/coast-aaa-qa/R15/qa-hero.md`、`qa-motion.md`。
- 最终包：`Artifacts/ProductShell/AAA/R15/Package/Mac/AeroSim.app`。

当前仍不能声明 3A 或用户验收通过。主要残留是远山低模轮廓、卷云顶部弱平行节奏、开阔地缺少真正近景碎石/decal，以及 cliff / shore 前景大平面。


---

## 14. R16 Pacific Coast 动物（2026-10-25）

R16 在 R15 环境基础上加入确定性程序化动物，不引入外部资产，也不改变飞行碰撞与任务逻辑。

- `PacificCoastWildlife.cpp`：2 条鲸鱼、3 只狼、1 只熊、5 只鹿、4 头牛；每个物种合并为一个程序化网格批次，鹿角单独批次。
- 鲸鱼位于外海，狼/熊和鹿/牛位于北侧相邻的山地草坡。动物使用可辨识的静态动作姿态，不开启 Tick。
- 动物组件全部 `NoCollision`，总三角形预算由 `AeroSim.Coastal.Wildlife.Composition` 自动化测试限制在 20,000 以下。
- 新增控制桥评审视角：`wildlife-whale`、`wildlife-predators`、`wildlife-herd`。
- 验证：Python 92/92；Mac Development Build 通过；Wildlife 定向 Automation 1/1；全量 Automation 93 项 0 failed；Package ExitCode 0。
- 证据：`Artifacts/ProductShell/AAA/R16/qa-wildlife/`；报告：`docs/coast-aaa-qa/R16/qa-wildlife.md`。
- 最终包：`Artifacts/ProductShell/AAA/R16/Package/Mac/AeroSim.app`。

当前没有骨骼动画、AI 漫游、毛发或音效；这些属于后续独立范围。最终视觉验收仍由用户决定。

## 15. R17 输入焦点、动物细节与飞行 HUD（2026-10-25）

R17 修复打包应用进入 Flight 后仍由 CommonUI Widget 持有焦点、导致 Enhanced Input 收不到键盘输入的问题，并完成动物模型与 HUD 的一轮增量。

- 输入：普通打包启动默认直接进入 Flight；`-AeroSimStartMenu` 和显式产品视图仍保留菜单。Flight 会停用 ProductWidget、使用 GameOnly input mode，并把 Slate 焦点交给 `SViewport`；非 Flight 页面重新启用 Widget。
- 实机：`slate_focus=SViewport`、`viewport_focus=true`、`mouse_captured=true`、`mapping_active=true`。12 个键位 `W/S/A/D/Q/E/LeftShift/LeftControl/Up/Down/Left/Right` 的 mapped/semantic peak 均为 1。
- 航线：真实按键闭环覆盖 15/15 航段，437.3 s 到达 Result，0 press error、0 capture error、0 safe pause。
- 动物：新增鲸鱼腹部/眼/口线与海面起伏；狼、熊、鹿、牛新增眼鼻、耳、蹄/掌、胸腹、斑点等细节批次。仍为 NoCollision 的程序化低多边形模型。
- UI：Flight HUD 底部新增完整键盘控制提示和最近动物/距离提示，使用 Canvas 绘制，不创建抢焦点的交互控件。
- 门禁：Python 92/92；Mac Development Build 通过；全量 Automation 94/94、0 failed；Package ExitCode 0。
- 报告：`docs/coast-aaa-qa/R17/qa-input-wildlife-ui.md`；包：`Artifacts/ProductShell/AAA/R17/Package/Mac/AeroSim.app`。

R17 仍没有骨骼蒙皮、毛发、复杂步态、AI 漫游或音效，不能声明达到 3A，也不代表用户视觉验收通过。下一轮优先将物种拆为独立实例/骨骼结构，引入不同相位动画和近景高质量材质。

## 16. R18 动物独立生命感与曲面细化（2026-10-26）

R18 将 R17 的整批动物起伏改为逐只动物、跨材质层同步的确定性动画，并提高主要躯干和头部曲面细分。

- 动画组织：10 个程序化材质批次、47 个实例范围；身体、深色/浅色细节与鹿角按同一动物相位同步。
- 动作：鲸鱼独立起伏、游移和尾摆；陆地动物具备物种差异化呼吸、头尾微动及交替承重。
- 形体：提高鲸鱼、狼、熊、鹿、牛主要躯干和头部的环向/纵向细分，减少近景多面体轮廓。
- UI：接近动物栖息地时显示 `LIVING HABITAT / OBSERVE QUIETLY`，保持 Canvas 非交互绘制。
- 预算：动物总计 21,850 triangles，自动化上限 30,000；全部保持 NoCollision。
- 门禁：Python 92/92；全量 Automation 94/94、0 failed；Package ExitCode 0；最终包 12 键映射全部 mapped/semantic peak=1。
- 报告：`docs/coast-aaa-qa/R18/qa-wildlife-motion.md`；包：`Artifacts/ProductShell/AAA/R18/Package/Mac/AeroSim.app`。

当前仍是程序化网格与 CPU 顶点形变，没有真实扫描资产、骨骼蒙皮、毛发、完整行为树或实录音效，不能声明已经与真实一致。下一轮应优先升级关节/骨骼结构、脚底 IK、行为状态机和近景材质。


## 17. R19 熊、鹿、鲸鱼第三方资产替换（2026-10-27）

R19 按用户确认的物种范围只保留熊、鹿和鲸鱼，移除狼与牛，并用可追溯的第三方骨骼资产替换 R18 程序化动物。

- 数量：2 条鲸鱼、1 只熊、5 只鹿；狼与牛为 0。
- 实现：`USkeletalMeshComponent` + 循环 `AnimSequence`；全部 `NoCollision`，资产缺失时保留程序化安全回退。
- 资产：三套 Sketchfab CC BY 4.0 资产；作者、来源、归档 hash 见 `THIRD_PARTY_ASSETS.md`，105 个导入 `.uasset` 的逐文件 hash 见 `docs/third-party-wildlife-assets.json`。
- 鲸鱼：重建 BaseColor / Roughness / Normal / AO PBR 材质，并修正翻滚方向与海面高度，使深色背部朝上。
- 门禁：Python 92/92；静态 10/10；Automation 94/94、0 failed；Mac Development Build、Package、Shader 检查通过；打包实机输入焦点正常。
- 报告：`docs/coast-aaa-qa/R19/qa-third-party-wildlife.md`；包：`Artifacts/ProductShell/AAA/R19/Package/Mac/AeroSim.app`。

R19 的自动化与截图是技术证据，不构成最终视觉验收，也不能据此声明达到 3A。


## 18. R20 动物自主巡游（2026-10-28）

R20 在 R19 的熊、鹿和鲸鱼骨骼资产上加入确定性的领地活动，使动物像即时战略游戏中的环境单位一样持续移动。

- 8 个实例全部注册自主活动状态：鲸鱼 2、熊 1、鹿 5；狼和牛仍为 0。
- 鹿群在出生区域附近以不同相位和速度结伴巡游，每帧重新采样地形高度并保持 42 cm 蹄部偏移。
- 熊切换为慢走动画，在山地领地内巡游并贴合地表。
- 鲸鱼在各自海域执行大半径低速巡游与低幅升沉，保持海平面约束。
- 所有运动由固定锚点、半径、速度和相位驱动；运动切线控制朝向，重复运行结果一致。
- 门禁：Python 92/92；静态 10/10；Automation 94/94、0 failed；Mac Development Build、Package、Shader 检查通过；打包实机焦点和 Enhanced Input 正常。
- 15 秒前后对照截图确认三类动物均发生位置或朝向变化；鹿与熊贴地、鲸鱼水位合理，无明显穿模、悬空、翻转或镜头丢失。
- 报告：`docs/coast-aaa-qa/R20/qa-wildlife-autonomy.md`；包：`Artifacts/ProductShell/AAA/R20/Package/Mac/AeroSim.app`。

R20 实现的是领地巡游与朝向变化，尚未加入无人机接近后的警觉/逃离、个体避让、捕食或复杂行为树；最终行为自然度仍由用户验收。

## 19. R21 动物骨骼运动与交互（2026-10-28）

R21 加入老鹰、鹿群警觉/逃离/个体避让、熊警觉和老鹰攻击，并针对用户指出的“不是骨骼动画、只是乱移动”完成资产与运行时修复。

- 根因：R19 鹿源 GLB 没有 skin，UE `AnimSequence` 的 bone track 数为 0；现已替换为带 50 个 joints、Idle/Walk/Run 三套骨骼动画的 Deer Animation 资产。
- 状态动画：鹿按 Alert/Walk/Flee 切换 Idle/Walk/Run；熊按警觉/巡游切换 Idle/Walk；鲸鱼使用 Forward Swim；老鹰保持骨骼拍翼。
- 运动同步：用加速度限制和平滑旋转替代瞬时速度/朝向变化，并按实际水平速度调节动画 PlayRate，降低滑步与漂移。
- 门禁：关键动物动画必须存在、时长有效，并在 Editor Automation 中包含 bone tracks。
- 验证：Mac Development Build 通过；Product Package ExitCode 0；UE 全量 Automation 90 succeeded、0 failed；Python 相关测试 27/27；静态检查 966/966。
- 报告：`docs/coast-aaa-qa/R21/qa-wildlife-interaction.md`；本地包：`Artifacts/ProductShell/CoastalPackage/Mac/AeroSim.app`。

R21 证明骨骼动画链路和行为状态切换已工作，但当前仍没有脚底 IK、近景毛发和完整导航行为树；最终视觉与运动自然度仍由用户验收。
