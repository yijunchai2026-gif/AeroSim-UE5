# QA R18 — Wildlife Independent Motion and Geometry Refinement

**范围**：在 R17 动物基础上继续提高生命感与近景轮廓质量，建立逐只动物、跨材质层同步的独立动画数据，并保持飞行输入、碰撞和任务路线不回归。

## 本轮结果

- 15 只陆地动物与 2 条鲸鱼拥有确定性的独立相位，不再整批同步起伏。
- 10 个动物材质批次全部进入动画更新，47 个跨材质实例范围保持身体、深色细节、浅色细节和鹿角同步。
- 鲸鱼：独立海面起伏、前后游移和尾部横摆。
- 狼、熊、鹿、牛：物种差异化呼吸频率、头部微动、尾部摆动与低幅交替承重。
- 狼、熊、鹿、牛和鲸鱼的主要躯干/头部曲面提高细分，近景轮廓由明显多面体进一步向连续曲面收敛。
- Flight HUD 在接近动物区域时显示 `LIVING HABITAT / OBSERVE QUIETLY`，仍使用非交互 Canvas，不抢占飞控焦点。
- 所有动物保持 `NoCollision`，不改变飞行碰撞和 15 段任务路线。

本轮仍使用程序化网格和 CPU 顶点形变，没有引入真实扫描动物资产、骨骼蒙皮、毛发系统、复杂行为树、环境交互或实录音效。因此这是向真实感推进的一轮，不声明已经与真实一致，也不替代用户验收。

## 预算与验证

| 门禁 | 结果 |
|---|---|
| Python unittest | 92/92，通过 |
| UE 5.8.3 Mac Development Build | 通过 |
| Wildlife Automation | 1/1，通过 |
| 全量 Automation | 94/94，0 failed |
| 动物三角形预算 | 21,850 / 30,000 |
| BuildCookRun | ExitCode 0 |
| 打包实机焦点 | `SViewport`、viewport focus、mouse capture、mapping active 均正常 |
| 12 键输入矩阵 | mapped/semantic peak 全部为 1 |
| 动态视觉证据 | 鲸鱼、捕食者、鹿牛群各 6 帧连续序列 |

## 证据与产物

- 最终包：`Artifacts/ProductShell/AAA/R18/Package/Mac/AeroSim.app`
- 输入矩阵：`Artifacts/ProductShell/AAA/R18/qa-input/key-matrix.json`
- 动态动物序列：`Artifacts/ProductShell/AAA/R18/qa-wildlife/`
- 全量 Automation：`Artifacts/ProductShell/AAA/R18/logs/automation-full-final.log`
- Package：`Artifacts/ProductShell/AAA/R18/logs/package-final.log`

## 下一轮优先级

1. 从 CPU 顶点遮罩形变升级为真正的分层骨骼/关节结构，解决肩胛、髋、膝、踝与颈部连接。
2. 引入近景高质量动物资产或程序化重拓扑，增加眼球、口腔、鼻镜、皮毛法线和各向异性材质。
3. 建立待机、行走、奔跑、进食、警觉、逃离和群体跟随状态机，并做脚底 IK 与坡面适配。
4. 引入距离分级更新、LOD 与动画预算，确保更高质量动物不影响飞行帧率。
5. 增加空间化环境声和物种实录音效，但仅在可信资产准备后启用。
