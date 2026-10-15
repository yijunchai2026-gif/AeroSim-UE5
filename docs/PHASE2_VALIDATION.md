# Phase 2 验证与证据

验证日期：2026-10-16（CST）；平台：macOS / Apple M3 Pro；引擎：UE 5.8.3；配置：Development；Runtime Contract：`aerosim.runtime.phase2/v1`。

## 验证矩阵

| 项目 | 结果 | 事实 |
|---|---|---|
| Phase 0 静态验证 | PASS | 540 checks |
| Phase 2 静态验证 | PASS | 24 checks |
| Python unittest | PASS | 6/6 |
| UE 5.8 Mac Development 编译 | PASS | UBT `Result: Succeeded` |
| `AeroSim.Phase2` | PASS | 7/7 |
| Phase 0 回归 | PASS | 2/2 |
| Phase 1 回归 | PASS | 6/6 |
| 全套 `AeroSim` Automation | PASS | 15/15 |
| Cook / Stage / Package / Archive | PASS | `-SkipZenStore`，UAT ExitCode 0 |
| 打包应用 NullRHI smoke | PASS | Pak/IoStore 挂载、Game Engine initialized、`aerosim.runtime.phase2/v1` 输出、正常退出 |
| Metal 可视化打包应用 | PASS（证据采集） | Development `SF_METAL_SM6`，4 张固定视角 + 1 张同机位天气对比 |
| 视觉门禁修复复核 | PASS（工程自评） | Rain/Nose 具有前景与世界雨线、雨幕、湿润覆盖和独立雾/曝光/饱和度；栈道标签已产品化 |
| AndroidFileServer/token 清理 | PASS | 最终 `DefaultEngine.ini` 不含该段或 token |

## Metal 固定视角

- `Phase2-Pad-Clear-Near.png`：Clear / Near Follow；起飞坪和路线支撑。
- `Phase2-Rock-Rain-Nose.png`：Rain / Nose；岩岸湿边；前景雨线、世界雨线、暗冷雨幕、湿润覆盖与浓雾均可见。
- `Phase2-Boardwalk-Overcast-Near.png`：Overcast / Near Follow；栈道林缘；区域标识已改为低饱和产品化样式，不再显示绿色调试文字。
- `Phase2-Overview-Clear-Far.png`：Clear / Far Follow；核心区总览。
- `Phase2-Weather-SameView-Comparison.png`：同一 Rock/Nose 机位的 Clear / Overcast / Rain 纵向对比；固定 16:30 与 Seed 21630，用于证明三种天气肉眼可分。

截图来自同一个最终 Development Archive。固定视角由只读命令行参数 `-AeroSimReview=<Pad|Rock|Boardwalk|Overview>` 与 `-AeroSimWeather=<Clear|Overcast|Rain>` 选择，不引入运行时写权限。

## 视觉门禁修复

独立视觉门禁首次复核指出 Rain/Nose 缺少可见降雨，以及 Boardwalk 标签仍呈绿色调试样式。本轮在 Phase 2 基线边界内完成：

1. Rain 增加 120 条确定性世界空间雨线，以及 Canvas 相机前景雨幕/雨线，Nose 视角不再只依赖 HUD 天气文字。
2. Rain 独立降低曝光和饱和度、提高雾密度并使用冷色调；地表增加湿润高光覆盖。Clear、Overcast、Rain 同机位比较可直接区分。
3. 三个 Hero Zone 标签统一为 `ZONE 01/02/03` 产品化文案和低饱和颜色；Boardwalk 不再显示突兀绿色文字。
4. `AeroSim.Phase2.Environment.WeatherSynchronization` 覆盖雨线数量/可见性、应用雾密度、Rain 曝光和饱和度阈值。

## G0–G3 自评

- **G0 PASS（工程自评）**：三 Hero Zone 与海岸巡检路线有明确语义、产品化标签和空间角色。
- **G1 PASS（工程/自动化）**：1 km 边界、300 m × 500 m 核心、三个 Hero Zone、构件支撑和碰撞合同均由 Automation 覆盖。
- **G2 PARTIAL / PENDING 人工签字**：三相机、vFOV、跟随碰撞和四个固定视角已验证；尚未由人完整飞行 60–90 秒并签署构图/操控观感。
- **G3 BASELINE PASS（工程自评）/ PENDING 独立复核**：同机位对比确认 Clear/Overcast/Rain 肉眼可分；Rain 具备可见雨线/雨幕、湿润覆盖以及独立雾/曝光/饱和度。该结论只覆盖 Phase 2 基线，不宣称正式 PBR、粒子 VFX、音频或水体完成。

## 剩余风险

1. Engine BasicShapes/基础材质在强光下偏亮，Hero Zone 主要依赖轮廓、密度和阴影，需正式材质阶段提升色彩与粗糙度可读性。
2. 跟随碰撞 Automation 验证 SpringArm 探针配置；尚缺高速贴岸全路线的人机穿模压力测试。
3. Replay 保证同一采样轨迹的确定性插值，不宣称跨架构位级确定性或网络同步。
4. 仅验证 Mac Development；Windows 参考构建、移动真机、安全区矩阵、性能和长稳不在本轮证据内。
5. 天气基线的雨线/雨幕是纯 C++ 程序化替代体；不包含正式粒子、动态积水、水花、风场和音频。
