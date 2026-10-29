# QA-HERO R15

**采集条件**

- 基于 R14 分支未提交工作树构建；Metal 固定 review camera，`world_paused=false`。
- 门禁：Python 92/92；UE 5.8.3 Mac Development Build 通过；全量 Automation 92 项、0 failed；最终海天定向 Automation 7 项、0 failed；BuildCookRun ExitCode 0。
- 证据：`Artifacts/ProductShell/AAA/R15/qa-final/`，共 24 张独立 PNG；mountain、north、shore、cliff、overview、forest、corridor、ground 各 1 张，shore / cliff 各另有 8 帧动态序列。
- 捕获清单记录的 review viewport 为 1280×720；这是控制桥评审截图口径，不用于重新裁决已单独验证过的窗口/全屏原生分辨率修复。

**结论：本轮继续弱化了 R14 遗留的规则边界和重复纹理，但不声明 3A 或用户验收通过。**

| 项目 | 裁决 | 证据 / 判断 |
|---|---|---|
| U1 根/树皮 | PARTIAL（改善） | LOD2 树干由 5 边提升至 8 边，并保留 3 边简化地表根；中距离圆柱感降低，但 forest / corridor 仍能看出规则直干。 |
| U2 密度 | PARTIAL（改善） | north grove 的生成与生境范围向西、南北扩展，硬截断减弱；overview 仍能辨认局部树群块状边界。 |
| U3 海面 | PARTIAL（保持） | shore / cliff 各 8 帧均有唯一图像哈希，没有静止复制；本轮未重做海水与泡沫。远山根部再下沉 20 m，并扰动边缘过渡以减弱“岛片插海”硬边。 |
| U4 天空 | PARTIAL（改善） | 三层卷云加入跨尺度 shear、旋转坐标与低频 drift，并提高稀疏阈值；长丝比首轮 R15 捕获更少、更断续，但 mountain / north 顶部仍可看到弱平行节奏。 |
| U5 地面 | PARTIAL（改善） | 开阔地以 4% 接受率布置低成本 exposed-grass tufts，增加远离树林区域的视差；宏观地面仍偏平，不能替代近景碎石和 decal。 |
| U6 山/崖 | PARTIAL（改善） | 岸壁每层阈值加入近/远两档扰动，远山边界加入 700 m 级低频扰动并降低周期噪声；横向层带与规则远山边减弱，但低模轮廓仍存在。 |

**运行数据**

- 最终 QA 24/24 图像哈希唯一；捕获诊断 `frame p95` 最大 20.90 ms，仅作同步截图诊断，不作正式性能结论。
- 最终包：`Artifacts/ProductShell/AAA/R15/Package/Mac/AeroSim.app`。
- 本轮没有重跑约 8 分钟的真实按键全航线；飞行、任务和输入代码均未修改，沿用 R14 的闭环航线证据。若下一轮触碰飞行或导航，必须重跑。

## 后续优先级

1. 远山仍需要更高质量的低频轮廓或大气透视，当前只是弱化规则边和海面硬接缝。
2. 卷云应进一步从“几条横向细丝”转成大小不同的断续云羽，避免顶部残留平行节奏。
3. 开阔地需要真正的近景碎石、枯草簇与 decal；继续堆实例会触碰既有几何预算。
4. cliff / shore 前景仍有低模大平面和横向层理，应结合网格细分与连续材质处理。

最终视觉验收仍由用户决定。
