# QA-HERO R14

**采集条件**

- 实现提交 `6f14695`，源码指纹 `0ceaa0092278`；Metal、1920×1080、固定 review camera，`world_paused=false`。
- 门禁：Python 92/92、UE 5.8.3 Mac Development Build、Automation（0 failed）、Package 与 cooked material 检查通过。
- 真实按键闭环航线：到达 Result，最终腿 `nav-15-land-on-mat`，492.7 s，未使用 teleport；终态 `mission_incident=0`。
- 证据：`Artifacts/ProductShell/AAA/R14/qa-final-v8/`、`real-input-route/route-log.json`。

**结论：R14 修复了主要错误来源，但视觉结果仍不能声明 3A 或验收通过。** 山体不再使用周期性木纹 mask，岩面颜色改为非周期程序噪声，山体顶面也不再按单个三角形切换材质 section；远山基部改为沉入海面并平滑过渡；卷云、north grove、远景树列和 LOD2 树基均做了去规则化。最终图中木纹和大块三角材质缝明显减弱，但低模地形轮廓、开阔地覆盖、卷云重复、中距离树基、north grove 外轮廓和岸壁横向层带仍能被辨认。

| 项目 | 裁决 | 证据 / 判断 |
|---|---|---|
| U1 根/树皮 | FAIL（改善） | LOD2 树基保留不对称 collar，而不再退化成纯 smooth flare；forest / corridor 中近距离仍主要读成圆柱，根土交界覆盖不足。 |
| U2 密度 | FAIL（改善） | north grove 密度噪声补入 Y 维度，远景 ridge 从棋盘抽样改为随机接受；矩形林缘已被打散，但 overview 仍可读出大块边界。 |
| U3 海面 | FAIL（保持） | shore / cliff 均采集 600 ms × 8 动态序列，泡沫持续变化；本轮不改海面周期。远山与海面交界已下沉并平滑，但局部岸脚硬边仍可见。 |
| U4 天空 | FAIL（改善） | 卷云使用非整数方位缩放和 domain warp，重复周期较 R13 弱；广角下仍能看到平行长丝和程序化节奏。 |
| U5 地面 | FAIL（改善） | 地表加入低成本 open-patch 色彩变化，避免继续增加实例触发几何预算；ground / overview 仍显示宏观 3D 地被偏稀。 |
| U6 山/崖 | FAIL（明显改善） | 移除周期 ForestLitterMask、木纹式岩石投影和顶面逐三角 material section；远山基部下沉 50 m 并用 interior fade 收口。木纹与大块三角材质缝明显减弱，但低模山脊规则轮廓、岸壁横向层带和局部硬边仍未关闭。 |

**运行数据**

- 真实按键航线 492.7 s，较 R13 的 538.75 s 缩短 46.05 s，但仍远高于 100–130 s 产品目标；视觉改动没有触碰飞行模型，不能把耗时变化归因于本轮视觉修复。
- 最终包源码指纹为 `0ceaa0092278`；QA、动态序列和真实航线来自同一份内容。提交后重新构建的包只更新 commit identity，不改变源码内容。
- 启动 QA 时发现持久化 fullscreen 设置会卡在 macOS `ApplyResolutionSettings`；移走本机 `settings-v2.json*` 后按 windowed 默认值启动。该问题不属于 R14 场景内容修复。

## 后续优先级

1. 用连续低频材质与雾化遮蔽继续弱化远山低模阶梯和海面交界，不再按三角形分材质。
2. 为中距离树基加入低成本 ground collar / litter occlusion，而不是增加高面数根系。
3. 用材质 patch 或 decal 提高大尺度 chaparral 覆盖，保持 FrustumGeometryBudget 不回退。
4. 继续拆散卷云长丝和 north grove 外轮廓；岸壁横向层带单独作为地形网格与着色联合问题处理。

没有用户验收，不作 3A 或完成声明。
