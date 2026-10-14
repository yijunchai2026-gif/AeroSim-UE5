# Phase 0 UE5 Import Checklist

> 勾选前先核对 `asset-migration-manifest.json`。P11 当前为 `GENERATED_NOT_REVIEWED`；飞机内容全部为 `PLACEHOLDER_REFERENCE_ONLY`。

## A. 文件与溯源

- [ ] 仅从 `SourceAssets/` 取文件，不把 `Migration/` 文档导入 Content Browser。
- [ ] 对每个迁入文件重新计算 SHA-256，并与 manifest 一致。
- [ ] 确认没有 Unity `.meta`、`Library/`、`Builds/`、Temp 或临时证据进入提交。
- [ ] 确认没有复制 `HeroCliff_P11_Source.blend`。
- [ ] 如后续加入任何单文件 > 50 MiB，先配置并验证 Git LFS tracking，再暂存文件。

## B. P11 HeroCliff FBX

- [ ] 导入 `HeroCliff_P11.fbx` 为 Static Mesh，不导入 skeletal animation。
- [ ] 启用/核对 FBX `Convert Scene` 与 `Convert Scene Unit`，不要重复应用手工 ×100 或额外轴旋转。
- [ ] 用 UE5 1 m 标尺验证尺寸；Unity meter 应对应 UE 100 cm。
- [ ] 验证正面与上方向：Unity `+Z forward/+Y up` 应到 UE `+X forward/+Z up`。
- [ ] 检查 LOD 名称/层级包含 `HC_LOD0/1/2/3`；不要误把 High 或 graybox 当 LOD0。
- [ ] 核对三角数：LOD0 92,160；LOD1 49,536；LOD2 21,868；LOD3 7,728（主体统计以 production manifest 为准）。
- [ ] 检查 UV0 唯一图集、无明显重叠/退化；按 UE lightmap 策略另建 UV channel，不覆盖烘焙 UV0。
- [ ] 设置 LOD screen size 并在运动相机下检查 popping。
- [ ] 根据玩法创建/复核 UE collision；不要假定旧仓 compound box 数据已随 FBX 完整迁移。
- [ ] 完成人工视觉、阴影、Nanite/传统 LOD 取舍、目标平台性能验收后，才讨论提升状态。

## C. P11 材质与纹理

- [ ] `BaseColor`：sRGB 开；连接 Base Color。
- [ ] `Normal`：Normal compression，sRGB 关；检查凹凸方向，必要时启用 Flip Green Channel。
- [ ] `Roughness`：sRGB 关；直接连接 Roughness，不做 Unity smoothness 反相。
- [ ] `AO`：sRGB 关；连接 Ambient Occlusion。
- [ ] `Height`：16-bit 源；sRGB 关；根据材质方案决定 Bump Offset/Parallax/Displacement，避免未经标定的过大位移。
- [ ] `Curvature`：sRGB 关；仅作可选 edge/cavity 调制。
- [ ] `WetnessCavityMask`：sRGB 关；按 `R=low-height wetness, G=cavity, B=breakup` 解包。
- [ ] 检查所有纹理寻址模式，唯一烘焙图默认不应被当作平铺宏观纹理。

## D. 海岸最小参考

- [ ] `CliffSideCC0` BaseColor 以 sRGB 导入。
- [ ] 保留并核对 `SOURCE.md`：Poly Haven Cliff Side、CC0、1.8 m 物理宽度。
- [ ] 明确这是最小参考，不是完整 PBR set；不要自动生成“已认证”的 Normal/Roughness/Height 声明。
- [ ] 若用于平铺材质，实测边界、texel density 和 1.8 m 比例。

## E. 飞机引用边界

- [ ] `aircraft-asset-manifest-v1.json` 仅作合同/预算/Socket 参考，不作为正式模型交付证据。
- [ ] `intake-manifest.placeholder.json` 保持 `CONTENT_NOT_DELIVERED`。
- [ ] `mini-shell-v3.png` 仅作 albedo/material swatch；sRGB 开，1254×1254 NPOT 导入策略需显式决定。
- [ ] 不从 shell texture、programmatic prototype 或 manifest 名称推断正式机型几何、尺寸或身份。
- [ ] 在正式 FBX/GLB、完整贴图、来源哈希和批准证据到齐前，所有飞机相关内容保持 `PLACEHOLDER_REFERENCE_ONLY`。
- [ ] 不创建或命名任何“正式 Mini 4 Pro”UE Asset/Blueprint 来掩盖缺失的正式模型。

## F. 完成条件

- [ ] UE5 Import Log 无轴、单位、退化切线或缺失纹理错误。
- [ ] HeroCliff 在标准光照下完成近/中/远距离检查。
- [ ] 材质通道与色彩空间逐项截图/记录。
- [ ] 迁移 manifest 哈希复核通过。
- [ ] 状态标签未越级：HeroCliff 仍为 `GENERATED_NOT_REVIEWED`，飞机仍为 `PLACEHOLDER_REFERENCE_ONLY`。
