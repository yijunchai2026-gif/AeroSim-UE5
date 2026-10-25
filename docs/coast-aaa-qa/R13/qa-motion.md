# QA-MOTION R13

**来源**：`Artifacts/ProductShell/AAA/R13/qa-final-v2/`，提交 `ceb31d7`，源码指纹 `26bc063ce077`。

- shore：600 ms × 8，实际跨度 4.288 s。
- cliff：600 ms × 8，实际跨度 4.262 s。
- 每帧均为独立 Metal 证据 PNG，`world_paused=false`。

| 项目 | 裁决 | 数据 / 说明 |
|---|---|---|
| Shore 动态 | PASS（保持） | 8 帧覆盖泡沫带的推进和回落，序列不是静止复制；R13 未修改 R12 的 4.2 s surge/recede 周期。 |
| Cliff 动态 | PASS（保持） | 8 帧中崖脚泡沫连续变化，没有整块单帧闪现。 |
| 天空层次 | PARTIAL | 低层 broken cumulus 与高空卷云均可见；固定晴天基线下云体不平移，便于对比，但高空细丝仍有长直线重复感。 |
| 冠层摆动 | PASS（保持） | 延续 0.065 wind cap；序列未见大幅冠层摆动。 |
| 性能 | PARTIAL | 序列 diagnostic frame p95：shore 27.72–27.95 ms，cliff 27.87–28.30 ms；高于 R12 主视角区间，需继续观察，不能声明性能改善。 |

**遗留**：本轮重点是地表、树根与天空；海面泡沫粒度和 cliff 向岸光流没有新增量化结论。没有用户验收，不作 3A 声明。
