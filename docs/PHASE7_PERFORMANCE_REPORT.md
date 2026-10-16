# Phase 7 Performance Budget and Evidence Report

## 1. 预算合同（不是实测）

| Profile | frame P95 | game CPU P95 | render CPU P95 | GPU P95 | 驻留内存 | draw calls | streaming pool | streaming spike P95 | Nanite |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| Mac Development High v1 | 16.67 ms | 8 ms | 8 ms | 14 ms | 4096 MiB | 2000 | 2048 MiB | 4 ms | Off，待实测 |
| Windows 1440p High candidate v1 | 16.67 ms | 8 ms | 8 ms | 14 ms | 6144 MiB | 2000 | 2048 MiB | 4 ms | Candidate，必须可 Off |
| iPad device candidate v1 | 33.33 ms | 14 ms | 14 ms | 28 ms | 2048 MiB | 800 | 768 MiB | 8 ms | Off |

`FPhase7PerformanceGate` 要求不可变 build ID、平台、硬件 ID、RHI、匹配质量档、目标硬件、非 Development、120 秒预热、1800 秒 replay 和三次冷启动。缺项、零值、NaN 或超预算均不 PASS。

## 2. Streaming / LOD / HLOD / Nanite 策略

- World Partition 与 HLOD 为桌面/移动候选方向，必须逐平台验证 cell 边界、突现、内存和任务规则连续性。
- 正式机型与 Hero Cliff 保留显式 LOD0–LOD3/Cull；比较 silhouette、collision 与正常路线 pop，不允许只看诊断镜头。
- Mac 与 iPad 默认 Nanite Off；Windows High 仅候选 On，Low/Off 是强制回退。
- iPad 使用独立 Low 档，不以桌面缩窗替代真机；不删除碰撞、任务、警告或输入安全。
- streaming 同时看 pool 峰值、P95 spike、missing asset 和 30 分钟趋势；禁止只报平均 FPS。

## 3. 实际证据

| 平台/模式 | CPU/GPU/frame | 内存/draw/streaming | 判断 |
|---|---|---|---|
| Mac Development | 尚未采集符合正式协议的性能数据 | 尚未采集 | **BLOCKED_TARGET_HARDWARE_EVIDENCE** |
| Windows 1440p High | `NOT_RUN` | `NOT_RUN` | **NOT_RUN** |
| iPadOS 真机 | `NOT_RUN` | `NOT_RUN` | **NOT_RUN** |

Development 编译、Automation、NullRHI、Metal 启动或短时 smoke 即使通过，也不构成性能 PASS。只有同一非 Development build 的原始 CSV/Insights、硬件身份、RHI、质量档和完整协议可关闭本门。

## 4. 长稳口径

确定性合同将 30 分钟 replay 表达为 108,000 个 60 Hz steps；Automation 可快速证明重复 digest，但不能替代 wall-clock 长稳。wall-clock harness 通过仍只证明该 build/RHI 下无 crash 且 replay marker 完整，不自动证明 CPU/GPU 或画质达标。
