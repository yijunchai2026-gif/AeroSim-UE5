# QA R20 — Wildlife Autonomous Roaming

**范围**：在 R19 的熊、鹿和鲸鱼骨骼资产基础上加入轻量、确定性的环境单位活动，使动物像即时战略游戏中的环境单位一样在各自领地内持续巡游，同时保持飞行路线、碰撞和输入不回归。

## 实现结果

- 8 个已接受动物实例全部注册自主活动状态：2 条鲸鱼、1 只熊、5 只鹿。
- 鹿群在各自出生点附近以略有差异的速度和椭圆半径结伴游走；每帧重新采样地形高度，并保留 42 cm 蹄部安全偏移。
- 熊使用慢走动画，在山地领地内独立巡游；每帧贴合地形高度。
- 鲸鱼在各自海域进行大半径、低速度巡游，并叠加低幅水下升沉；始终以海平面为基准。
- 每只动物根据运动切线持续调整朝向；初始朝向通过 yaw bias 保持，避免加载首帧跳变。
- 活动由固定初始位置、相位、半径和速度驱动，不使用随机数，重复运行结果一致。
- 所有动物继续保持 `NoCollision`，不改变无人机碰撞和 15 段任务路线。

## 验证结果

| 门禁 | 结果 |
|---|---|
| Python unittest | 92/92，通过 |
| 静态门禁 | 10/10，通过 |
| UE 5.8.3 Mac Development Build | 通过 |
| 全量 Automation | 94/94：90 success、4 warning、0 failed |
| 自主活动注册 | 8/8 个实例 |
| BuildCookRun | ExitCode 0 |
| Shader 检查 | 未出现 `Failed to compile Material` |
| 打包实机焦点 | `SViewport`、viewport focus、mouse capture、mapping active 均正常 |
| 动态视觉对照 | 15 秒前后，鹿、熊、鲸鱼均有明确位移或朝向变化 |

视觉对照确认：鹿脚与坡面贴合，无明显陷地或悬空；熊四肢贴地；鲸鱼保持合理水位；三类动物均未出现明显穿模、翻转或镜头丢失。

## 证据路径

- 动态截图：`Artifacts/ProductShell/AAA/R20/qa-wildlife/`
- 最终包：`Artifacts/ProductShell/AAA/R20/Package/Mac/AeroSim.app`
- 集成日志：`Artifacts/ProductShell/AAA/R20/logs/`

本轮目标是环境单位式自主活动，不包含捕食、攻击、繁殖或复杂行为树；最终视觉与行为自然度仍由用户验收。
