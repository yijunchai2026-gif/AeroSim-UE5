# R21 动物骨骼运动与交互 QA

日期：2026-10-28

## 结论

本轮修复了“动物仅整体位移、没有可信骨骼运动”的核心问题。原鹿资产虽然带有 `AnimSequence`，但源 GLB 没有 skin，UE 动画也没有 bone track；现已替换为带 50 个 joints、Idle/Walk/Run 三套骨骼动画的 Deer Animation 资产。熊、鲸鱼和老鹰的现有动画也重新核验了骨骼 track，并把巡游速度、转向和动画播放速率绑定起来。

这些结果证明骨骼动画链路和行为状态切换已工作，但不替代用户的最终视觉验收，也不声明达到 3A。

## 根因与修复

- 原鹿 GLB：1 个动画、0 个 skin；动画只有 morph `weights` channel。UE 中 `deer_animation_Anim` 的 bone track 数为 0，因此整体 transform 移动会呈现滑动和漂移感。
- 新鹿 GLB：3 个动画、1 个 skin、50 个 joints；对应 Idle、Walk、Run 三种状态。警觉时 Idle，巡游时 Walk，逃离时 Run。
- 熊：巡游使用 Walk，警觉时平滑减速并切换 Idle，同时转向无人机。
- 鲸鱼：切换为 Forward Swim 骨骼动画，并保持海域巡游和水位约束。
- 老鹰：保留骨骼拍翼动画，降低攻击与恢复速度，缩小巡游半径并平滑加速、减速和转向。
- 所有移动动物：使用加速度限制与旋转插值，动画 PlayRate 根据水平速度在 0.72–1.28 范围内同步，减少滑步和瞬时转向。
- 自动化新增硬门禁：8 个关键 `AnimSequence` 必须存在、播放时长大于 0.1 秒，并在 Editor 测试中含有至少一个 bone track。

## 行为与构成

- 数量：鲸鱼 2、熊 1、鹿 5、老鹰 1；狼和牛保持移除。
- 鹿群保留无人机接近后的警觉、逃离、个体避让和恢复。
- 熊保留警觉、停步和面向无人机。
- 老鹰保留巡航、攻击和恢复。
- 行为路线证据仍可触发 `wildlife_alert=true` 与 `eagle_attacking=true`。

## 验证结果

- UE 5.8.3 Mac Development Build：通过。
- Product Package `CoastalPackage`：ExitCode 0，归档成功。
- UE 全量 Automation：90 succeeded，0 failed。
- Python 相关测试：27 tests，全部通过。
- Phase 0 静态检查：966 checks，全部通过。
- Wildlife Composition 定向测试：通过。
- `git diff --check`：通过。

## 视觉证据

动态序列已分别覆盖：

- 鹿巡游和逃离：`deer-final-01.png` 至 `deer-final-08.png`、`deer-flee-new-01.png` 至 `deer-flee-new-08.png`
- 熊巡游：`bear-motion-01.png` 至 `bear-motion-06.png`
- 鲸鱼前游：`whale-motion-01.png` 至 `whale-motion-06.png`
- 老鹰拍翼与巡航：`eagle-final-01.png` 至 `eagle-final-08.png`

截图能够看到跨帧关节姿态变化：鹿腿部步态、熊四肢步态、鲸鱼尾部摆动、老鹰双翼拍动均发生变化，不再只是静态模型整体平移。

## 已知限制

- 鹿源 GLB 本身没有内嵌图片纹理；本轮已补充可追溯的鹿毛 Base Color 源图与独立 `M_DeerCoat` 材质，但眼鼻、鹿角分材质和近景毛发仍有限。
- 鲸鱼近景材质偏亮，水面交界仍需要后续视觉微调。
- 当前没有脚底 IK；复杂坡面上仍可能出现轻微脚底悬空或穿地。
- 行为是确定性状态机与领地运动，不是完整感知/导航行为树。
- 最终运动自然度和视觉效果仍由用户在运行包中验收。

## 贴地与纹理修复（2026-10-29）

用户复核发现鹿步态仍需贴地且毛皮纹理消失。诊断确认两项根因：新骨骼鹿源 GLB 的 `images` 与 `textures` 均为空；此前为了避免白模又用统一颜色材质覆盖了导入材质。贴地方面此前只按组件根节点设置地形高度，没有按当前动画姿态读取蹄部骨骼。

- 新增 `T_DeerFur_Albedo` 和独立 `M_DeerCoat`，运行时明确绑定鹿毛材质，不再使用统一颜色覆盖。
- 保留源图 `SourceAssets/Wildlife/Deer/deer_fur_albedo.png`，便于重新导入和审计。
- 每帧读取四个 toe-tip 骨骼的世界坐标，分别查询脚下地形高度，按最大穿透量修正骨骼组件 Z；修正使用插值并限制在基准高度 -12 至 +35 cm，避免动画切换时上下跳动。
- 自动化新增鹿毛 Texture 资产存在性与运行时材质绑定检查；另创建测试 World、推进 90 帧 Walk 动画与 Actor Tick，逐鹿验证四个 toe-tip 的最小离地间隙保持在 -1 至 4 cm。
- 最新 `wildlife-herd` 8 帧序列显示鹿恢复棕灰毛皮明暗细节，四蹄随坡面与步态保持接地。
