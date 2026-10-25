# QA-MOTION R12

**来源**：`Artifacts/ProductShell/AAA/R12/qa-final/`，提交 `cb3c4e8`，源码指纹 `410838f7b176`。

- shore：600 ms × 8，实际跨度 4.255 s。
- cliff：600 ms × 8，实际跨度 4.298 s。
- 每帧均为独立 Metal 证据 PNG，`world_paused=false`。

| 项目 | 裁决 | 数据 / 说明 |
|---|---|---|
| Shore 完整涌入-消退 | PASS | 下半屏白泡沫占比 1.03→0.17→0→0→0→5.07→1.78→0.97%，覆盖完整低谷、峰值和回落。 |
| Cliff 完整涌入-消退 | PASS | 1.16→1.24→1.06→0.63→0.19→0.13→0.17→0.55%，覆盖回落和再次建立。 |
| 帧间变化 | PASS | shore mean abs RGB 0.92–5.24；cliff 0.59–0.82；没有静止复制帧。 |
| 泡沫流向 | PARTIAL | Shader 中 shore/drift 均为 1.2 m/s shore-dominant current；序列可见位置变化，但 cliff 视角缺少可靠光流量化，不能仅凭目测签 PASS。 |
| 单帧闪烁 | PASS | 序列中没有孤立整块泡沫瞬间出现/消失；变化与 4.2 s 周期连续。 |
| 冠层摆动 | PASS（代码/观感） | wind cap 从 0.15 降到 0.065；序列观感稳定，无 R11 的大幅冠层摆动。未复算 changing px/MP。 |
| 性能 | PASS（本轮分辨率） | 8 个主视角 diagnostic frame p95 为 20.6–23.3 ms（1920×1080）。 |

**遗留**：shore 峰值泡沫仍偏大块；cliff 向岸方向需后续用 optical flow 或可视锚点复核。没有用户验收，不作 3A 声明。
