# R5：海岸飞行与事故重试工程批次

2026-10-18。限定结论：海岸负高度控制、真实支撑落地规则、Sweep后状态同步、触水显式重试和事务回滚工程验证通过。完整五目标可飞导航仍未通过；本批不是3A候选或用户视觉验收。

源码身份：基线 `cfa8e16a1d1c5da2d263c819a653fc1da83a3aec` 加本批草稿，fingerprint `0b27cbbe38fe6e4398c4895f0f8bdc78dfcbfa9bd277e51d51b2f2fa1d578426`，source_dirty=true。UE5.8.3、Mac Development、Metal、Apple M3 Pro/36GB，1920×1080 High、Clear 16:30、seed21630、vFOV60。build ID保留 `AeroSim-Phase7-ProductShellA-20261016-v1`，必须与fingerprint及文件哈希联合辨认，不能单凭该旧名称识别包。

实现移除了起飞面下方的隐形地板。通用训练四旋翼仍用原语义键位，增加中性减速悬停；真实WorldStatic脚底查询决定可落地支撑，水不可落地。碰撞后的实际Actor位姿回写模型、trace和评分，未实现的请求位置不计分。未进入合法Landing的支撑穿透一律报告不安全接触，包括横向高速缓降贴地。Home完成须实际mat支撑、低速、姿态与足迹约束；原18m完成半径不再有效。它们是Generic原型辅助控制，不是具名机型物理真实性通过。

触水暂停后，普通active-mission重启guard只对明确的Flight页+触水latch+Paused+UnsafeLanding开放显式重试；R清除失败任务，返回Hangar与原mat位置，重新进行preflight。MCP的事务快照同步保存新water/collision flags；heartbeat只重试短暂缺失/未完整JSON的读操作，不重放不确定的写命令。新增生产回滚callback测试验证两种方向恢复。

新WORLD任务采用真实海岸/海面/林间坐标，任务版本v2区分成绩/replay。固定plateau clearance七点单独提供给环境生成，防止任务路点间接改变全林避让；原环境接地/开敞断言保留。当前目标间直接连线仍不安全，见AAA-R5-NAVIGATION-GAP.md；导航中间引导另批实施。

## 真实验证

| 项目 | 结果 |
|---|---|
| 差异检查、10个适用静态验证器 | PASS |
| Python | PASS，82项 |
| Editor编译 | PASS；已有SetMaterialUsage弃用warning保留 |
| NullRHI完整Automation | PASS，79项=76clean+3warning、0failed；其中Metal专项明确NOT_RUN，另行执行 |
| 真实MetalMaterialsCompile专项 | PASS，1clean、0warning、0failed |
| Cook/Package/Archive | PASS |
| 真实Pawn floor/wall Sweep、模型/trace实际位置、触水暂停 | PASS，Automation fixture；不是完整玩家路线 |
| active GameMode任务的ResetAction、普通重启guard、原mat复位 | PASS，Automation fixture |
| 正常菜单+真实Slate/Enhanced Input按键 | SCOPED PASS，起飞/近海飞行/负高度悬停/触水/R重试 |
| 完整五目标路线、60–90秒、运动画质/LOD、目标平台发布性能 | NOT_RUN |
| 树冠、海面与海湾整体视觉 | FAIL，当前实际画面未达到目标；R4形体增量仍是草稿 |
| 正式具名飞机及用户外观确认 | BLOCKED_ASSET / pending_user |

按键smoke经Home→Hangar→Mission→Preflight→Flight。Shift起飞、W飞至近海、Ctrl下降；world Z约-6110.806cm仍Flight且未暂停，HUD实际显示水面净空约3.8m。继续下降后world Z约-6491.073cm进入触水安全暂停；真实R后Hangar、Grounded、world unpaused，Actor回到(55,79,10.868cm)。各控制租约均有实测mapped/semantic峰值；所有结果与PNG确认同fingerprint。四张canonical PNG及manifest完整性PASS，PMO已查看起飞、低水与触水实际图像。

该smoke沿X到40040cm，用于检验控制，出现明确OUTSIDE SAFE ZONE反馈；它不是沿五目标的合法全路线，不将其重命名为任务PASS。未采用review camera/teleport/直接Observe来完成按键证据。全程RenderOffscreen，不激活用户窗口。Runtime日志无旧UV metadata ensure/invalid、无Handled ensure/Fatal/Error。

本机ignored证据冻结于 `Artifacts/ProductShell/AAA/R5/`：build身份/结果/日志、Automation index、source.patch与新增源/hash、MetalAutomation及console、controls原始canonical截图/manifest/完整MCP事件和回放脚本、Package及package-hashes.json。controls/results.json SHA256 `1c7f1945af2e0c9554da6679be5359d21e7e38eb60d550fc1af27ab8f56a2fc3`。早期两轮失败报告完整保留，不覆盖成PASS。发布性能与跨平台未测，截图读回耗时不充作性能结论。

下一批是合法中间导航与单处扫描受控试装；用户原工作树/既有未登记资产不覆盖，桌面旧试玩入口在完整航路P0关闭前不自动替换。本工程提交不代签正式外观或最终3A。
