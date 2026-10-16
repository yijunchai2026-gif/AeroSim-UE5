# AeroSim UE5 Phase 7 — 产品化、受控优化与 Mac Candidate

## 1. 范围与身份

Runtime：`aerosim.runtime.phase7/v1`；Build ID：`AeroSim-Phase7-20261016`。

Phase 7 在 Phase 6 Editor-only 事务边界上增加**只建议、不自应用**的布局、光照、LOD 与性能建议。所有建议固定为 `DRAFT_NOT_USER_CONFIRMED`；仅布局建议可由人工转换为既有有限 ScenePatch，仍必须执行 inspect → dry-run → snapshot → apply → G0–G4 validate → evidence → commit/rollback。光照、LOD、性能建议没有新增写权限。

## 2. 产品化合同

- Mac Development High：Metal SM6、100% screen percentage、HLOD、2 GiB streaming pool；Nanite 默认关闭，待目标内容和硬件实测再开启。
- Windows 1440p High：候选档，`NOT_RUN`；不得从 Mac 外推。
- iPad：独立 30 fps 候选档、75% screen percentage、Low scalability、768 MiB streaming pool、Nanite Off；真机 `NOT_RUN`。
- 设置采用 Preview/Apply/Cancel/Rollback；唯一持久化相机真值仍是 vertical FOV。
- settings schema v2；v1 迁移补齐质量档、音量和 telemetry opt-in 默认值；未来版本/坏值 fail closed。
- 保存先写同目录 `.tmp` 并 flush，再保留 `.bak` 后替换；失败恢复上一份有效文件。
- 日志对 token、secret、password、API key、Bearer、邮箱和用户目录脱敏。

## 3. 稳定性与性能

- 固定 seed `21630`、60 Hz 整数状态演进；30 分钟为 108,000 steps，重复 digest 必须一致。
- `Scripts/phase7_soak.py` 运行打包程序的真实 wall-clock smoke/soak，必须同时满足退出码 0 与 `PHASE7_SOAK_PASS`；输出 JSON 与日志 SHA256。
- 性能合同覆盖 frame/game/render/GPU P95、驻留内存、draw calls、streaming pool/尖峰；预算不是实测。
- 发布性能门要求非 Development、目标硬件、120 秒预热、30 分钟完整回放、三次冷启动。缺任一 provenance 或指标即 fail closed。

## 4. 资产与发布门

- 正式机型：`BLOCKED_ASSET`。
- 用户逐机外观确认：`BLOCKED_USER_CONFIRMATION`。
- Windows 与 iPad：`NOT_RUN`。
- 目标硬件正式性能：`BLOCKED_TARGET_HARDWARE_EVIDENCE`。
- 最终产品截图/UX：没有正式资产和用户确认，不生成伪候选截图，保持阻塞。

## 5. 验证入口

```bash
python3 Scripts/phase7_static_validate.py
python3 -m unittest discover -s Scripts/tests -v
python3 Scripts/phase7_release_manifest.py --output Artifacts/Phase7/release-manifest.json
```

UE Automation：`AeroSim.Phase7`；全套回归：`AeroSim`。实际执行结果仅记录在 `docs/PHASE7_VALIDATION.md`，未执行不得写 PASS。
