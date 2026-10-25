# QA-WILDLIFE R16

**范围**：Pacific Coast 新增鲸鱼、狼、熊、鹿、牛，全部使用运行时程序化轻量网格，不依赖新增外部资产。

## 数量与布置

| 物种 | 数量 | 区域 | 识别特征 |
|---|---:|---|---|
| 鲸鱼 | 2 | 海面外侧 | 流线身体、头部、尾柄/双尾叶、胸鳍、背部喷气柱姿态 |
| 狼 | 3 | 北侧山地草坡 | 尖耳、长吻、长尾、行走步态 |
| 熊 | 1 | 北侧山地草坡 | 厚重躯干、圆耳、短吻、粗腿 |
| 鹿 | 5 | 山地开阔草坡 | 细腿、长颈、短尾，其中 2 只有分叉鹿角 |
| 牛 | 4 | 山地开阔草坡 | 宽体、低头颈部、双角、垂尾 |

## 工程约束

- 每个物种按一个 `UProceduralMeshComponent` 批处理；鹿角单独一批，共 6 个组件。
- 所有动物关闭碰撞，不改变飞行、导航和任务路线。
- 不启用场景 Tick；动物目前使用确定性静态动作姿态，避免持续 CPU 开销。
- 总三角形数受自动化测试约束，必须小于 20,000。
- 新增 `wildlife-whale`、`wildlife-predators`、`wildlife-herd` 三个固定评审视角。

## 验证

- Python：92/92。
- UE 5.8.3 Mac Development Build：通过。
- Wildlife 定向 Automation：1/1，0 failed。
- 全量 Automation：93 项，0 failed（88 succeeded，5 succeeded with warnings）。
- BuildCookRun：ExitCode 0。
- 实机 Metal 截图：`Artifacts/ProductShell/AAA/R16/qa-wildlife/`。
- 最终包：`Artifacts/ProductShell/AAA/R16/Package/Mac/AeroSim.app`。

## 视觉判断

鲸鱼已能在贴近海面的评审视角中清楚辨认，背部和尾部穿出水面；狼群和熊位于同一山地草坡，轮廓、体量与耳/吻/尾差异可辨；鹿群和牛群集中在相邻开阔草坡，鹿角、长颈、牛角和宽体差异可辨。

当前模型采用与场景一致的低多边形程序化风格，尚未加入骨骼动画、毛发、AI 漫游或动物音效。本轮只确认功能、布置和可辨识度，不声明整体视觉验收通过，最终判断由用户完成。
