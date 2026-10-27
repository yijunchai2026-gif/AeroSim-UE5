# QA R19 — Third-party Bear, Deer and Whale

**范围**：按用户确认的物种收敛方案，只保留熊、鹿和鲸鱼；移除狼与牛；用可追溯的第三方骨骼资产替换程序化动物，并验证导入、动画、碰撞、打包和评审视角。

## 本轮结果

- 场景动物固定为 2 条鲸鱼、1 只熊和 5 只鹿；狼与牛数量均为 0，HUD 也不再提示这两个物种。
- 三类动物均使用导入的 `USkeletalMeshComponent` 和循环 `AnimSequence`，不再生成 R18 的程序化动物批次；资产缺失时仍保留安全回退路径。
- 所有动物保持 `NoCollision` 且关闭 overlap，不改变飞行碰撞、15 段任务路线和输入焦点。
- 鲸鱼使用 BaseColor、Roughness、Normal、AO 重建 PBR 材质；最终调整模型翻滚方向与水位，使深色背部朝上并避免腹部白面误朝天空。
- 熊与鹿评审相机移近动物，便于检查模型轮廓、材质和落地状态。

## 来源与完整性

三套资产均来自 Sketchfab，授权为 CC BY 4.0；作者、原始页面、归档 SHA-256、导入修改说明记录在 `THIRD_PARTY_ASSETS.md`。`docs/third-party-wildlife-assets.json` 固化来源 receipt 与 105 个 `.uasset` 的 SHA-256；环境资产门禁会逐项校验路径、授权、归档和文件 hash。

| 物种 | 资产 | 作者 | 下载包 SHA-256 |
|---|---|---|---|
| 熊 | Animated Bear 3D Model | AnimalMesh3D | `ca8d8d929df51410732c1612cb9f47b7f6b598e2199bda5ac0dabcbea34e63e7` |
| 鹿 | Animated Deer | igor-lir | `296d6ef0cc008470a211b895cf01b6f3184feed43715debf852cb3d30454c574` |
| 鲸鱼 | Humpback Whale (Swimming) | Connlan_Immure | `f7f5480b890ca944933f3af11b240cb5d7281050cc8d197bd22522856e1c4f37` |

## 验证

| 门禁 | 结果 |
|---|---|
| Python unittest | 92/92，通过 |
| 静态门禁 | 10/10，通过 |
| UE 5.8.3 Mac Development Build | 通过 |
| 全量 Automation | 94/94：89 success、5 warning、0 failed |
| Wildlife Composition | 8 个 Skeletal Mesh 组件；鲸鱼 2、熊 1、鹿 5；狼/牛 0；全部 NoCollision |
| BuildCookRun | 通过 |
| Shader 检查 | 通过 |
| 打包实机焦点 | `SViewport`、viewport focus、mouse capture、mapping active 均正常 |

## 证据与产物

- 最终包：`Artifacts/ProductShell/AAA/R19/Package/Mac/AeroSim.app`
- 动物截图：`Artifacts/ProductShell/AAA/R19/qa-wildlife/whale.png`、`bear.png`、`deer.png`
- 集成日志：`Artifacts/ProductShell/AAA/R19/logs/`
- 第三方授权：`THIRD_PARTY_ASSETS.md`
- 资产完整性清单：`docs/third-party-wildlife-assets.json`

本轮完成的是可复现的技术验收和评审证据，不替代用户对真实感、镜头效果和最终视觉质量的判断，也不声明达到 3A。
