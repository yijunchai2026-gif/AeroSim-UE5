# QA-HERO R12

**采集条件**

- 集成提交 `cb3c4e8`，源码指纹 `410838f7b176`；Metal、1920×1080、Flight 页面、`world_paused=false`。
- 门禁：Python 92/92、静态 10/10、Build、81 项 Automation、Package、Shaders 全部通过。
- 真实按键闭环航线：到达 Result，最终腿 `nav-15-land-on-mat`，361.62 s，未使用 teleport。
- 证据：`Artifacts/ProductShell/AAA/R12/qa-final/` 与 `route-final/route-log.json`。

**结论：U1–U6 仍不能判定通过；R12 不是 3A 候选。** R12 的确定性收益是完整浪涌/消退、林下亮度、冠层色块、近地草和低树冠摆动；最大遗留仍是地形造型与森林轮廓。

| 指标 | R11 | R12 | 目标 / 判断 |
|---|---:|---:|---|
| Forest open sky（同一简化 mask） | 8.5% | 10.5% | 约 5%，未达 |
| Corridor open sky | 1.1% | 0.6% | 保持可飞，达成 |
| Forest canopy green | 29.3% | 32.3% | 改善 |
| Corridor canopy green | 26.3% | 31.6% | 改善 |
| Forest floor RGB | 41.3,39.1,29.4 | 48.0,45.4,32.7 | 恢复，略高于约 45,42,32 |
| Corridor floor RGB | 42.7,38.8,29.7 | 51.6,46.4,34.4 | 恢复但偏亮 |
| Frame p95（8 个主视角） | 15.3–23.9 ms | 20.6–23.3 ms | 1920×1080 下稳定 |

| 项目 | 裁决 | 证据 |
|---|---|---|
| U1 根/树皮 | FAIL（改善） | 根张开上限固定为 1.30×；中远景仍难读出真实根系层次。 |
| U2 密度 | FAIL（改善） | 走廊保持封闭且可飞，树冠色块增大；forest 顶部仍有明显天空洞，north grove 仍偏矩形。 |
| U3 海面 | FAIL（改善） | shore 和 cliff 均出现完整涌入/消退；泡沫仍偏大块，cliff 的向岸位移肉眼不够明确。 |
| U4 天空 | FAIL（未主改） | 加州方向性薄云保持，但本轮没有建立新的天空验收证据。 |
| U5 地面 | FAIL（改善） | launch/ground 的草密度提高；碎枝、石屑等近地几何仍不足。 |
| U6 山/崖 | FAIL | 首轮激进改动造成环带后已回退；最终保持 R11 基线并做小幅断裂，但 shore 上部仍偏沙丘，崖沿锈带和远山规则纹理仍可见。 |

## 下一轮优先级

1. 地形：直接重做 shore 顶部轮廓与 rim section，不再叠加全局高频 relief；消除锈色横带与远山条纹。
2. 植被：只在 forest review pose 附近补上层 crown mass，并以不规则 grove 边界替代矩形密度场。
3. 地表：加入可读的碎枝、针叶堆和石屑实例，不只提高草密度。
4. 海面：保留 4.2 s 周期，减小泡沫块尺寸并量化 shore/cliff 光流方向。

没有用户验收，不作 3A 或完成声明。
