# QA-MOTION R15

**来源**：`Artifacts/ProductShell/AAA/R15/qa-final/`。

- shore：8 帧，控制桥连续 held-camera 序列。
- cliff：8 帧，控制桥连续 held-camera 序列。
- 16 张动态证据均有不同图像哈希，`world_paused=false`。

| 项目 | 裁决 | 数据 / 说明 |
|---|---|---|
| Shore 动态 | PASS（保持） | 8 帧不是静止复制；本轮未改海浪谱和泡沫时序。 |
| Cliff 动态 | PASS（保持） | 8 帧持续变化，没有整块单帧闪现。 |
| 天空层次 | PARTIAL（改善） | 卷云保持确定性和静止天气基线，跨尺度 shear / drift 打散了部分长丝；广角顶部仍可见弱平行节奏。 |
| 冠层摆动 | PASS（保持） | forest / corridor 未见明显闪烁或异常 LOD 跳变；本轮未改风参数。 |
| 岸壁稳定性 | PARTIAL（改善） | 分层阈值加入空间扰动，规则横带减弱；低模折线和前景大平面仍可见。 |

Motion 证据只说明没有明显静止、闪帧或时序回退，不能替代用户的最终视觉验收。
