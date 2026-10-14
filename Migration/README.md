# AeroSim → UE5 Phase 0 源资产迁移

本目录记录从只读旧仓 `AeroSim-runtime-work` 到 `AeroSim-UE5` 的 Phase 0 最小源资产迁移。实际资产仅放在 `SourceAssets/`；本目录只放迁移记录和导入验收说明。

## 交付范围

### P11 HeroCliff

- `SourceAssets/HeroCliffP11/Models/HeroCliff_P11.fbx`
  - 选用旧仓最终 P11 FBX，而非 graybox candidate。
  - FBX 内含 `HC_LOD0`、`HC_LOD1`、`HC_LOD2`、`HC_LOD3`；LOD0 为 92,160 triangles。
  - 上游状态是 `GENERATED_NOT_REVIEWED`，不是 `ProductionReady`。
- `SourceAssets/HeroCliffP11/Textures/`
  - 七张 2048² 烘焙图：BaseColor、Normal、Roughness、AO、Height、Curvature、WetnessCavityMask。
- `SourceAssets/HeroCliffP11/Documentation/`
  - 上游 README 与 production manifest，保留生成、LOD、bake、通道和原始哈希证据。
- 未复制 `HeroCliff_P11_Source.blend`：大小 74,757,872 bytes，Phase 0 不需要；若未来纳入，因超过 50 MiB 必须使用 Git LFS。

### 最小海岸可复用参考

- `SourceAssets/CoastReusable/CliffSideCC0/T_HeroCliff_P11_DetailBaseColor.jpg`
- `SourceAssets/CoastReusable/CliffSideCC0/SOURCE.md`

这是 Poly Haven **Cliff Side** 的最小 BaseColor 参考，上游记录为 CC0、物理宽度 1.8 m。Phase 0 未复制配套 Normal/Roughness/Height，因此不能把它声明为完整 PBR set。

### 飞机占位参考

- `SourceAssets/AircraftReference/aircraft-asset-manifest-v1.json`
- `SourceAssets/AircraftReference/Mini4/intake-manifest.placeholder.json`
- `SourceAssets/AircraftReference/Mini4/mini-shell-v3.png`
- `SourceAssets/AircraftReference/Mini4/mini-shell-v3-SOURCE.md`

**全部为 `PLACEHOLDER_REFERENCE_ONLY`。** 旧仓没有正式无人机 FBX/GLB；manifest 明确是 programmatic prototype / unverified，Mini 4 intake 状态为 `CONTENT_NOT_DELIVERED`。Shell PNG 只是项目生成的 albedo/material swatch，不是机型几何、正式 UV atlas 或完整 PBR 材质，不得伪装为正式机型资产。

## Unity → UE5 迁移基线

| 项目 | Unity | UE5 | 处理 |
|---|---|---|---|
| 轴 | X right, Y up, Z forward | X forward, Y right, Z up | 位置映射：`UE_cm=(100*Unity_z,100*Unity_x,100*Unity_y)` |
| 单位 | meter | centimeter | 比例 ×100；优先让 FBX importer 读取轴/单位，避免重复转换 |
| BaseColor | sRGB | sRGB | 保持 sRGB |
| Normal | Linear；Blender/OpenGL 候选 | UE tangent normal | 实测凹凸方向，通常需要 Flip Green Channel |
| AO/Roughness/Height/Mask | Linear | Linear / Masks | 关闭 sRGB；按独立通道接入 |
| WetnessCavityMask | R=低位湿润，G=cavity，B=breakup | 自定义材质输入 | 不按普通 RGB 颜色纹理处理 |

导入后必须用 1 m 标尺和正面朝向标记验证；不要在 importer 已转换后再手工旋转/缩放一次。

## 可追溯性

- 完整逐文件源路径、目标路径、SHA-256、字节数、类型、坐标/单位/材质注意、授权/来源和状态见 [asset-migration-manifest.json](asset-migration-manifest.json)。
- UE5 导入与验收步骤见 [IMPORT_CHECKLIST.md](IMPORT_CHECKLIST.md)。
- 未复制 Unity `.meta`、`Library`、`Builds`、临时目录或 verification 临时证据。
- 当前迁入文件均不超过 50 MiB，当前交付不要求 LFS。
