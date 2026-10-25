# QA-HERO R13

**采集条件**

- 集成提交 `ceb31d7`，源码指纹 `26bc063ce077`；Metal、1920×1080、固定 review camera，`world_paused=false`。
- 门禁：Python 92/92、静态 10/10、Build、81 项 Automation、Package 与 cooked shader 检查全部通过。
- 真实按键闭环航线：到达 Result，最终腿 `nav-15-land-on-mat`，538.75 s，未使用 teleport；`mission_incident=0`。
- 证据：`Artifacts/ProductShell/AAA/R13/qa-final-v2/` 与 `route-final-v2/route-log.json`。

**结论：用户指出的三项都有可见改善，但 R13 仍不能声明 3A 或验收通过。** 地面从单一枯草扩展为更可读的干草、低灌木、蕨类和林下落叶分区；天空形成低层海雾云、破碎积云与高空卷云的层次；树根从规则放射支柱改为不对称、弯曲并埋入地面的低矮根脊。当前最大遗留是大尺度开阔地仍显稀疏、云体仍有程序化重复感、树基在中距离仍偏圆柱。

| 项目 | 裁决 | 证据 / 判断 |
|---|---|---|
| U1 根/树皮 | FAIL（改善） | 根瓣由规则 4–6 向改为树种相关 3–5 向，角度和强度随机；根脊末端埋入地面 22 cm，不再形成明显星形支柱。forest 中景仍主要读成圆柱树基，近景树种差异还不够。 |
| U2 密度 | FAIL（保持） | forest / corridor 仍保持高密度和可飞走廊；本轮没有解决 north grove 方块轮廓和上层冠层空洞。 |
| U3 海面 | FAIL（保持） | 沿用 R12 的 4.2 s 泡沫周期；shore / cliff 4.29 / 4.26 s 序列持续变化，无静止复制帧。本轮不主改泡沫块尺寸。 |
| U4 天空 | FAIL（改善） | 晴天云底从 1.6 km 降至 0.75 km、云层厚度增至 3.8 km；加入第二层高空细卷云并在复核中降低其强度。视觉上比 R12 更有湾区海洋层与破碎积云，但卷云仍略规则。 |
| U5 地面 | FAIL（改善） | launch / ground 可见更大的低灌木与草簇，林下蕨类和落叶分区更清楚；训练走廊、停机坪、步道保护区保持清晰。宏观开阔地仍有较大裸露面，需要后续增加低成本地表 patch，而不是继续堆高三角形实例。 |
| U6 山/崖 | FAIL（未主改） | 保持 R12 地形；岸壁锈带、远山规则纹理和 shore 顶部沙丘感仍在。 |

**运行数据**

- 9 个 lit review view 的 diagnostic frame p95：21.76–27.73 ms；本轮高于 R12 的 20.6–23.3 ms，不能宣称性能改善。
- 真实按键航线完整闭环，但 538.75 s 明显高于产品目标 100–130 s，也高于 R12 的 361.62 s；本轮视觉改动未触碰飞行模型，仍需单独排查自动驾驶脚本与走廊 recovery 的耗时。
- Flora 首次加密方案触发 `AeroSim.Coastal.Flora.FrustumGeometryBudget`，已回退实例数量，仅保留更大且更易读的地表植被尺度；最终 Automation 通过。

## 下一轮优先级

1. 用低三角形地表 decal / material patch 增加 chaparral、干草、针叶堆和碎石的连续覆盖，避免再次突破 frustum geometry budget。
2. 为固定 review pose 增加树基近景证据，并按树种强化 collar silhouette；保持根脊低矮、埋地和不对称。
3. 降低卷云长直线重复，增加海雾云在海岸—内陆方向的覆盖梯度，同时保留暖阳与海面 glint。
4. 单独复核真实输入航线 538.75 s 的 recovery 段，不能把它归因为视觉通过。

没有用户验收，不作 3A 或完成声明。
