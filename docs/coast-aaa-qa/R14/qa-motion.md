# QA-MOTION R14

**来源**：`Artifacts/ProductShell/AAA/R14/qa-final-v8/`，实现提交 `6f14695`，源码指纹 `0ceaa0092278`。

- shore：600 ms × 8，名义跨度 4.2 s。
- cliff：600 ms × 8，名义跨度 4.2 s。
- 每帧均为独立 Metal 证据 PNG，`world_paused=false`。

| 项目 | 裁决 | 数据 / 说明 |
|---|---|---|
| Shore 动态 | PASS（保持） | 8 帧覆盖泡沫带推进和回落，序列不是静止复制；R14 未改海面 4.2 s surge/recede 周期。 |
| Cliff 动态 | PASS（保持） | 8 帧中崖脚泡沫持续变化，没有整块单帧闪现。 |
| 天空层次 | PARTIAL（改善） | 卷云改为非整数方位缩放和 domain warp，固定视角下周期重复减弱；广角仍能看到长丝平行节奏。 |
| 冠层摆动 | PASS（保持） | 延续既有 wind cap，序列未见异常大幅摆动。 |
| 岸壁稳定性 | PARTIAL（改善） | 木纹式 world projection 已替换为有界非周期岩色，逐三角顶面材质缝已移除；横向层带、低模折线和局部岸脚硬边仍可见。 |

**遗留**：motion 证明海面/冠层没有静止或闪烁回退，但不能替代静态材质与地形验收。没有用户验收，不作 3A 声明。
