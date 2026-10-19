# 单处低湾扫描试装合同（待执行）

2026-10-18。PMO 已查看真实裁片离线 clay，采纳 DEV-TERRAIN 的只读集成调查。状态 `NOT_IMPORTED / UE_NOT_RUN / G2_NOT_VERIFIED`；当前场景 G2 FAIL 不变。先结束飞行工程批次，再独占同一 UE 导入/构建队列。

仅使用已冻结的 Poly Haven `coastal_cliff_01` CC0 派生 40m 裁片，三档 49,995 / 11,997 / 3,591 tris。来源与下载、crop、meshoptimizer 0.25 MIT、派生 SHA 已冻结在 terrain 泳道 ignored R4/ScanFeasibility，产品导入前必须将必要来源记录与固定派生源转入可追溯 SourceAssets；不依赖本机临时目录作最终交付。

现 Habitat importer 读取小型 `.mesh.json`，全量运行会重写既有纹理、材质、网格。新实现必须有 `HeroCliffOnly` 独立分支，只读取固定六个 glTF/bin，校验 SHA、大小、accessor/stride/索引边界、有限法线/UV、固定 URI；禁止路径扩张与外部材质。只写一个新的 `SM_CoastHeroCliffScanDraft`，三档原始 LOD 直接导入。不要放宽原植被网格限制或重导全部 106 资产。manifest/policy 只增一个登记项与六文件固定源合同，同名未登记资产立即停止。

glTF 米/Y-up → 本地 UE 厘米轴置换 `P=(100z,100x,100y)`，法线同置换、三角 B/C 交换一次。实际导入后核对正反面、宽4000cm、高约1001cm、UV/材质槽 streaming metadata 与三档真实三角数；离线轴推导不能替代引擎观察。

仅试装中心 Y=-19000cm、约[-21000,-17000]范围。根据原崖体实际三角求刚体位置，不猜 X、不拉伸成65m整墙、不重复阵列。先保证裁片最低处在海面以上，原闭合崖脚与精确海平面交线不变；顶底和两端埋合必须在实际正常视角验证。裁切面外露、Z-fight、顶帽接缝或悬浮都判 FAIL。

扫描厚度9.66m，NoCollision仅能用于隔离静态评审，不能进入可飞候选。若真实刚体嵌合不能让可见岩面与原 Sweep 对齐，后续只在该hero范围匹配原闭合崖面；保留外围接缝、上部landheight、原水线及3cm回归要求。无法做到即停止试装并返回几何设计，不以材质、雾或背后碰撞蒙混过门。

允许增量：固定导入函数、policy/manifest writer及测试、六文件和来源记录、新mesh、Graybox单实例/共享header、必要的局部Terrain及几何碰撞测试。编辑前记录各目标基线哈希；失败只撤销本批明确增量和新包、恢复manifest。不动既有HeroCliffP11及其它用户资产，不全量清理/重导。

验收顺序：固定源与导入合同→实际LOD/bounds/正面→原地形闭合/水线→正常shore/cliff/lighting_only与两侧近景→真实Pawn Sweep→Cook/Package→同包运动/LOD成本。每项独立 PASS/FAIL/NOT_RUN，用户外观仍 pending_user。
