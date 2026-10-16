# AeroSim UE5 Phase 4 — 海岸训练任务与产品闭环

## 1. 阶段结论

Phase 4 在既有 Pacific Coast 三英雄区、连续灰盒地貌、Clear/Overcast/Rain 同源环境、固定复核相机和只读 Editor Inspection 基础上，补齐可训练、可失败、可恢复、可评分、可复盘的产品闭环。Runtime 合同为 `aerosim.runtime.phase4/v1`，Build ID 为 `AeroSim-Phase4-20261016`。

正式机型资产继续保持 **`BLOCKED_ASSET`**。当前 AeroSim Generic 程序化训练代理只可用于系统链、训练验证和截图，不会进入正式机型任务资格，也不伪造来源、哈希或用户确认。

## 2. 任务合同

`FPhase4TaskDefinition` 固定：

- Mission ID：`pacific-coast-inspection`；定义版本：`1`；
- 目标：起飞，按 1→5 顺序巡检草土、潮岩、栈道、低空水面、林缘，返航并在起飞坪降落；
- 5 个稳定 checkpoint ID，每个同时提供编号、文字和不同形状提示；
- 安全区：核心 500 m × 300 m，最大高度 25 m；越界恢复点向内收 15 m；
- 目标时长 75 秒，产品口径 60–90 秒；
- 流程：`Hangar → Preflight → Takeoff → Inspection → ReturnToHome → Landing → Result → Replay`；
- 飞前检查：输入回中、相机/vFOV、环境、质量四项完整后才可开始。

## 3. 训练、失败与恢复

- 渐进提示：先解释“编号+形状”，再提示 RTH 前置条件；稳定飞行后降噪；
- 越界：文字 `OUTSIDE SAFE ZONE` + inward chevrons + 明确恢复点；已完成进度保留；
- 碰撞：文字 `COLLISION / HOLD POSITION` + stop octagon；输入清零，允许重试当前点；
- 漏门/乱序：未来门不会推进进度，指回当前编号和形状；
- 返航过早：保持 Inspection 并高亮缺失 checkpoint；
- 降落：HOME diamond 到达后进入 Landing，提示减速、垂直下降和保持水平；
- 暂停/完整设置/失焦/设备断开沿用 Safe Pause：四轴清零，恢复前回中并倒计时 3 秒。

所有恢复均 fail closed，但不无故清空已完成 checkpoint，不把用户丢回 Hangar。

## 4. 评分与 Replay

Result 至少给出：

| 维度 | 权重 | 主要输入 |
|---|---:|---|
| Safety | 35% | 越界、碰撞、漏门 |
| Precision | 30% | 通过 checkpoint 时相对中心距离 |
| Efficiency | 20% | 相对 75 秒目标的超时、绕行 |
| Smoothness | 15% | 速度变化、俯仰/横滚累积、碰撞 |

分数只从量化后的确定性任务输入生成；`ReplayDigest` 绑定 mission/version、完成数、事件数、0.1 秒量化时长和精度输入。只有 Result 完整且 digest 仍匹配时才能进入 Replay；Replay 保留与 Result 完全相同的四维分数和 digest。

## 5. HUD 与输入

HUD 只占屏幕左上、右上和底边安全区，中心飞行走廊保持无遮挡。关键状态始终同时给出文字与图形语义，不依赖颜色：盾牌/目标/时钟/波形用于四维评分，chevrons/octagon/checkmark/diamond/numbered shape 用于告警与路线。

控制语义设备无关：Confirm、RTH、PauseSettings、ResumeCentered、RetryCheckpoint、OpenReplay；键鼠与手柄仅提供映射。当前可见映射包括 `M` Confirm、`H` 或手柄 D-pad Down RTH、`P` Pause/Settings、`C` Camera。

## 6. Phase 4 范围边界

- 保留 Phase 2 已建立的三英雄区、连续地貌、水岸/林缘和 Environment State 天气消费者同步；
- 保留 Phase 3 固定证据相机和机型门禁；
- Editor 观察面仍只允许 heartbeat/inspect 元数据，无 Python、Shell、console command、进程、文件写入、Transform 或 AI 编辑入口；
- 不实现 Phase 5 受控编辑，不添加正式二进制内容资产，不把短跑当作发布性能结论。

## 7. 验证入口

- 静态：`python3 Scripts/phase4_static_validate.py`
- Python 全套：`python3 -m unittest discover -s Scripts/tests -v`
- Automation：`AeroSim.Phase4`；全回归：`AeroSim`
- 构建、Cook/Package、NullRHI smoke、Metal 窗口 ID 截图命令和结果见 `docs/PHASE4_VALIDATION.md`。
