# R4：实际画面复审与工程记录

2026-10-18，基于 cfa8e16 的草稿源码，fingerprint `5d33f46f4c477dbcbadb9a175fc0b61bd55922259747d43f7e9bc35f3a58fb67`。UE 5.8.3，Mac Development，Metal，1920×1080 High，Clear 16:30。本次不包含后续 Flight Surface 改动。

工程验证：Editor build、Cook/Package PASS；Automation 73 项（71 clean、2 warnings、0 failed），Python 82 PASS。包与原始构建记录冻结在本机 ignored `Artifacts/ProductShell/AAA/R4/`；具体哈希见 build/hashes.json。截图通过实际 packaged app 的 MCP 获取，9 组共 19 帧；lit 与 lighting_only 使用同批构建。快捷重命名图仅用于审阅，证据验证必须使用原始 canonical 文件名及 manifest。

真实画面结论：G2 FAIL。三种树的分枝、冠幅和叶团倾角有所调整，但正常林缘视角仍能看到大片水平叶片棚和重复直干，未达到自然树冠要求。海湾新增侵蚀沟与岩肩，但光照关闭后仍可见平滑土帽和明显 U 形断面，当前程序化改动不足以解决英雄区形体。没有以材质、雾或工程测试代签美术通过。G3/G4 最终验收仍未运行，不宣称 3A。

下一步按依赖推进：先修复真实海面低空航线、支撑落地和 sweep 后模型同步；美术侧评估有明确来源与 LOD 的真实岩壁扫描裁片，先在灰模视角验证与闭合地貌的连接，再决定受控导入。新资产均为草稿，未经用户确认不作正式交付。

PMO 已实际查看离线裁片的斜视 clay：裂隙、破碎岩块与岩脚体积优于现有平滑断面，可进入单处低湾受控试装。Poly Haven coastal_cliff_01（官方 CC0）裁片维持 40×10.01×9.66 m；LOD0/1/2 为 49,995/11,997/3,591 三角。开放背面必须有原闭合地形承托，不得拉伸成 65 m 整墙。顶部仍偏平，需要接合设计。本批尚未导入 UE，不能据此改变游戏 G2 FAIL。来源与原始/派生哈希已在 terrain 泳道 ignored ScanFeasibility/FEASIBILITY.md、download-receipt.json、export-summary.json 冻结；离线报告 SHA256 `f6a9dea3000ce3a6aa543ce9bf6e203aa2e071820073002bd6b6c0f7a9e55997`。
