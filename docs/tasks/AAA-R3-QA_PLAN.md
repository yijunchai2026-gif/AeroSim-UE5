# AAA-R3 QA 与植被形体限定规格

2026-10-18。只读复核与后续执行计划，不授权本批扩散植被、采购素材或改动已冻结实现。本文作者参与过 AAA-R3-STAB-01 的 UV streaming 修复，因此以下属于开发者辅助审查，**不能由本人代签 STAB 最终 PASS 或独立视觉验收**；PMO/另一 QA 必须核对新包实际日志和像素，用户外观仍为 pending_user。

## 已看证据与具体问题

本次实际逐张看过 R2 `Artifacts/ProductShell/AAA/R2/pmo-review/` 的全部 22 张 PNG：forest-lit 0–5、shore-lit 0–5；launch-operator/corridor/overview/north/south 各 1；forest/cliff/mountain 的 lighting_only 各 1；cliff/mountain 的 lit 各 1。22 个便利副本的内容 SHA256 均与 `results.json` 相应 `image_sha256` 一致。便利副本文件名不等于原 manifest 文件名，此校对不是重新宣称完整原图 verifier 通过。

来源为 R2 revision `894ab6a15955409dd72a5f37bb855a5a0d226c91`、source `819c0d34f31dc98b75cf704f390d4b011d5f684940bb8694698192ccfff4cfef`。结果与运行日志分别为同目录 `results.json` / `runtime.log`。runtime.log 含两项已记录的 UV metadata handled ensure；截图成功不能关闭启动稳定性缺陷。

| 真实画面 | 具体缺陷 | 有界下一步；没有据此签通过 |
|---|---|---|
| forest-lit 0–5，forest-lighting_only | 画面上半部的大片绿叶像水平层架，薄片边缘与整片同向明暗很显眼；多处长直树干相似粗细、相似锥度，形成重复柱列。lighting_only 仍显示层架，不能只调绿色/曝光解决 | 先修三个代表 archetype 的主枝曲线、簇体积和干形；保留全场密度、现有材质与贴图，先不扩散到所有 seed |
| corridor-lit | 地面有空隙，但正前方多个树干与重叠枝片占据视线；仅凭这张图难判断入口、下一个开口和可飞方向。不能把数学中心线净空测试直接当成可读通道 | 对准实际入口和第一个转折做低、中、高三层镜头；先让连续开口从正常玩家高度可辨。仅在确有遮挡的入口少量移位到边缘、保留相邻林群总量，不全林稀释、不加发光箭头代替构图 |
| overview-lit | 两个近林群已有冠层总量，但顶层形状近似重复的平面碎片，林缘至草地的过渡较急。山坡绿色块边界和种群尺度仍脱节 | 每个试验林缘限制在 30–50 m，先核对大树、幼树、灌草与空隙的连续层次；不继续增百万叶片或铺整树 billboard |
| mountain-lit / lighting_only | 树木仍沿绿色区成带排列，局部较亮、较小的叶片簇与旁边高树不协调；大山形可读，但地表生境边界依然像涂色 | 保留现有山形，挑一条林缘过渡段与三个 archetype 同批复核，不能仅靠给整坡加树签生态可信 |
| north/south、cliff/shore | 两翼海色有明显直线矩形边界；shore 是一条贴岸细纹，低湾在 lighting_only 下是宽滑槽面。森林改善不能抵消这些失败 | 保留给 SEASKY/TERRAIN 的独立缺陷。R3 重拍同机位，不能拿植被截图代签海岸 |

forest 六帧实际 elapsed 为约 0.002/0.397/0.694/0.996/1.297/1.660 秒；shore 为 0.002/0.323/0.632/0.957/1.263/1.591 秒。它们是固定位置的短诊断，不是平移、转弯、LOD 往返或完整飞行视频。

## 三个代表 archetype 的形体规格

固定选现库 `CoastalFloraFirMatureA`、`CoastalFloraPineMatureA`、`CoastalFloraOakA`，沿用现有来源和物种声明；这些名称来自代码，不表示能从 R2 单张像素可靠反认某棵树。实施前记录其实际实例 ID、变换、包围盒、碰撞和 LOD 面数。先检查三株，不新增类别/素材，且保留未改 seed 作对照。

| 代表 | 必须改变的具体形体 | 检查距离/反例 |
|---|---|---|
| FirMatureA | 主轴保留向上收束，侧枝由弯曲承重枝过渡到下垂小枝，针簇沿真实末梢产生前后/上下层次；打破整圈同高、同向铺平的枝叶，树冠空隙大小随高度变化。干颈与中干有连续渐变，不能只给圆柱叠树皮 | 近处从枝下与侧面都能看出簇深度；中距仍是有尖顶、不同长度侧枝的树形，不是水平搁板；远 LOD 保留主要空隙和冠宽，不变成稀疏针链 |
| PineMatureA | 与 fir 明确区分为偏冠、分支不等长的较开放高冠；主枝有粗细层级和弯曲，自承重分叉可读，支梢聚簇而不是把所有叶片摊成宽平扇。树干偏转连续，不能只整体旋转同一根柱 | 正侧视不能拥有几乎同一圆锥外形；绕到冠下不能出现一整块平面天花板；不以增加一圈相同卡片增加“体积” |
| OakA | 先让低位分叉、偏重枝和宽展的几个不等大冠团组成可信骨架，再在团内布置不同朝向的小枝/叶簇；必须能沿视线看到冠团的前后遮挡和支撑枝。根颈落地连续，不做球团/圆盘层叠 | 近距卡片不能读作大张贴纸，中距多个冠团不能合成统一水平圆盘；far LOD 保留宽冠分组与主分叉。代码名不是新增正式橡树资产确认 |

把现有卡片重新集中到合理支梢并调整曲率/朝向，优先于加数量。三株的 LOD0 顶点/三角数均不得增加超过对照；场景实例数和冠层总体覆盖保持当前目标，碰撞必须随真正干形匹配。树干变化有结构依据，不能用随机大抖动、缩小碰撞或额外雾掩饰。先在正常材质下确认体积与轮廓，再用 lighting_only 辅助；two-sided foliage 的该模式本身有局限，不把其发白/发黑当作物理材质结论。

## R3 新包的无窗口激活检查

本文件没有启动引擎、占用 MCP、切换窗口或注入输入。以下由 PMO 在独占列车上执行，当前结果均为 NOT_RUN。

1. **身份与稳定性先行。** 冻结本轮 revision、source fingerprint、round/build ID、asset manifest hash、平台/RHI、画质、Clear 16:30、seed 21630、1920×1080、vFOV60；两次独立冷启动。保留完整新日志。检查 13 archetype ×2 slot 的正有限 UV0 密度记录，两个 UV ensure 均不得重现，亦不得有 INVALID/fallback/新 ensure。正常 Home→Hangar→Mission→Preflight→Flight→退出任务→Home，再切 forest/corridor/overview/mountain。本人不能凭自己写的日志字符串代签关闭。
2. **同机位静态对照。** 用现有 `aerosim_inspect` 和 `aerosim_capture_view`，后台离屏读取引擎 viewport，保持无 OS 激活。R2 各 preset 的真实姿态见下表；先同姿态、同设置重拍 R2 的 22 张，再加 forest/corridor 的 unlit 与 detail_lighting 单帧定位形体/底色/法线原因。等 streaming wanting=0 并记录 settle 实际时长；超时保持失败/阻塞，不能默默换低画质或默认材质。
3. **固定相机时域诊断。** forest 与 shore 各一组 `frames=8, interval_ms=2000`，约 14 秒以上实际跨度，world_paused=false、输入 neutral。保留每帧真实 elapsed；作为慢风动、整片翻面、叶边闪点、材质串色、岸脚定位的排查。它仍不是连续 30/60fps 视频，不能按帧数给运动 PASS。前后 inspect 应保留页面/输入/玩家姿态的恢复事实。
4. **同包回滚条件。** 任何 UV ensure、空材质、树变稀/默认灰、可见的 alpha 覆盖突变，都阻止 STAB 候选交付；先保留坏帧/日志和实际身份，再由 PMO 决定回退。修启动问题不能顺带改变画质来藏问题。

R2 重点复现姿态（cm，pitch/yaw/roll，来自 results 原 manifest）：

| preset | 位置 | 朝向 |
|---|---|---|
| forest | -15360,-6820,420 | 4,-120,0 |
| corridor | -15753,-7183,250 | -3,-117.3,0 |
| overview | 37600,-37200,16000 | -17,145,0 |
| mountain | -40960,-34360,12000 | 3,175,0 |

## 移动、转向与 LOD 往返：必须单列的验收

现有 `capture_view` 只有固定 preset 或当前 player，单次最多 8 帧；不能请求任意相机路径。现有 temporal hold/move/hold 是八个离散位置，适合找视差/片面，不代表恒速摄影。`press_key` 是实际 Slate/Enhanced Input 的有限按键，但串行截图会扰动时序，也不等于物理键盘或无停顿视频。不要编造不存在的录制/相机 MCP 参数。

| 要关闭的缺陷 | 有界采样路线与观测 | 执行和证据要求 |
|---|---|---|
| 片面冠层与重复树干 | 三代表各近距侧移约 6–10 m、相机高度约1.7/3/6 m；沿枝下观察，再绕主干约60°；正常 lit 与相同姿态的辅助形体图。具体姿态由真实树/地形安全范围生成，不在墙内/树内摆相机 | 现 preset temporal 可做首轮离散诊断；三高度/绕树路径超出现工具能力，须由 QA-MOTION 与 GameMode owner 提供有界录制或真实玩家轨迹后执行，未实现前标 NOT_RUN。不得用本文件当成工具已支持 |
| 通道入口与连续空隙 | 从入口前10–15 m，以实际2–4 m/s进入至少50–80 m，经过首个转折/收窄，回头一次；低、中、高观察层复核可飞高度与头顶枝条 | 经正常任务入口，用真实输入驱动 pawn，记录 before/after 与连续中间 pose、碰撞、输入峰值；先由 Flight owner 确认可达。调相机摆拍不关闭可飞性 |
| LOD 替换、卡片翻转、剔除突跳 | 对同三株逐一沿相同视线由近到远、再原路回来；每条至少两次往返，速度一慢一正常。实际距离覆盖全部现有 LOD 阈值及幼树/灌草剔除边界，边界前后留停留观测 | 按真实 bounds、instance scale 和现 screen-size 阈值生成预期距离，不把20/60/150m等任意固定距离假称LOD交界。记录实际时间/pose与预期LOD，CPU预期并非GPU实际LOD证明。LOD往返必须是同包，不手改阈值制造好图 |
| 海色矩形边界与岸脚 | north/south 对同一色断层横向靠近/穿越/返回，shore 沿岸平移并缓慢转向；对照固定镜头排除仅view-dependent反射解释 | SEASKY owner 的同源 lit/unlit、相同水钟与姿态序列；固定图里无直线不够，移动出现方形色块仍失败 |
| 真实试玩 | 60–90秒起飞→林缘通道→岩岸→返航；低海面是否进入路线由Flight P0实测决定 | 正常产品入口、实际输入、不中途传送/重置/切审查相机。全程实际时间和不间断视频/高频原始记录必须存在；目前同步PNG序列不能满足该要求 |

若现有管线不能无激活地给出真实速度的连续记录，由 PMO 给 QA-MOTION 安排有界引擎录制工作；本轮不扩张成新的 MCP 大功能。必须保留该缺口，不把稀疏 PNG 拼成固定帧率电影后宣称顺滑。连续运动检查至少覆盖风动、平移、转向、全部 LOD 往返与正常路线；截图哈希验证只证明来源完整。

## 性能审查与最终裁决的边界

R2 forest/corridor/overview 的 `scene_frustum_triangles_lod0` 分别为 7,646,039 / 7,366,557 / 23,463,006；它们是可见 bounds 内所有实例展开 LOD0 的 CPU 估计，不是选中LOD的GPU三角数、draw calls或显存。独特mesh很小与HISM使用正确，也不能代替整场渲染代价。

R2 forest 首帧诊断 p95约42.73ms、GPU约30.95ms，corridor约50.80ms/32.50ms，均来自 Development+同步读回，只作定位。不能换算成稳定FPS或发布性能 PASS，也不拿不同程序运行中的rolling p95直接作A/B百分比。Metal draw_calls=0仍为NOT_MEASURED。

下一次性能样本应与视觉样本来自同一个冻结包，但单独在无截图读回、相同输入轨迹下采样：先热身，记录完整试玩及LOD往返，再复跑；保留CPU game/render/GPU帧时间、峰值/分位、streaming/pool/内存与卡顿时刻。工具不支持的指标留NOT_MEASURED。Mac Development只作诊断，Windows目标非Development/Shipping性能门继续独立阻塞。若冠层修形只是让所有树读入更高LOD或扩大alpha过度绘制，必须记录代价，不能用三株好图替代G4。

交付矩阵应逐项分列：STAB工程关闭、三个代表形体、通道可读性/可达性、风动与LOD连续运动、海岸缺陷、性能。各项有新包证据才可变化状态；STAB通过不代表森林或3A达标。本文不对尚未生成的R3图像或任何最终产品结果签PASS。
