# QA R17 — Input Focus, Wildlife Detail and Flight HUD

**范围**：修复打包应用启动后飞控按键不响应；继续提升鲸鱼、狼、熊、鹿、牛的程序化模型识别度；补充低干扰飞行控制与动物距离提示。

## 结果摘要

- 打包应用默认直接进入 `Flight`，不再把飞控藏在四层菜单确认之后；使用 `-AeroSimStartMenu` 或显式 `ProductUxView` 时仍保留完整产品壳流程。
- Flight 状态会停用 CommonUI 产品 Widget，并将 Slate 焦点交给 `SViewport`；离开 Flight 后重新启用产品 Widget。
- 实机状态确认：`slate_focus=SViewport`、`viewport_focus=true`、`mouse_captured=true`、`mapping_active=true`。
- 12 个键位 `W/S/A/D/Q/E/LeftShift/LeftControl/Up/Down/Left/Right` 的 `mapped_peak` 与 `semantic_peak` 均为 1。
- `LeftShift` 800 ms 实测将飞行器高度从 10.9 cm 提升到 579.1 cm，并推进到下一航段。
- 真实按键闭环航线覆盖全部 15 段，437.3 s 到达 `Result`；0 press error、0 capture error、0 safe pause。

## 动物建模增量

- 鲸鱼：新增腹部浅色、眼、口线和喷水轮廓；主身体与细节批次加入低幅、低频海面起伏。
- 狼：新增浅色胸腹、鼻、眼、足掌并加强耳、吻、尾的物种轮廓。
- 熊：新增胸腹层次、鼻、眼和宽掌，维持厚重体量。
- 鹿：新增眼鼻、耳、蹄、尾部浅色、身体斑点和分叉鹿角。
- 牛：新增眼鼻、耳、蹄、浅色头部与身体斑块。
- 所有动物继续保持 `NoCollision`，不改变飞行碰撞、导航和任务路线；自动化测试继续约束数量、组件和三角形预算。

本轮模型仍是运行时程序化低多边形实现，没有骨骼蒙皮、毛发、复杂步态、AI 漫游或动物音效。因此只记录相对 R16 的形体、材质分区和生命感增量，**不声明达到 3A，也不声明视觉验收通过**。

## UI 增量

Flight HUD 底部新增两类信息：

- 键盘控制提示：`W/S` 俯仰、`A/D` 横滚、`Q/E` 偏航、`Shift/Ctrl` 油门，以及方向键替代映射。
- 最近动物提示：显示 `WILDLIFE: NEAREST ... / 距离`，不创建可交互控件，不抢占飞行焦点。

## 验证

| 门禁 | 结果 |
|---|---|
| Python unittest | 92/92，通过 |
| UE 5.8.3 Mac Development Build | 通过 |
| 全量 Automation | 94/94，0 failed |
| BuildCookRun | ExitCode 0 |
| 打包实机焦点 | `SViewport` / viewport focus / mouse capture 均正常 |
| 12 键映射矩阵 | 全部 mapped/semantic peak = 1 |
| 真实按键完整航线 | 15/15 航段，437.3 s 到 Result，0 输入/截图错误 |

## 证据与产物

- 最终包：`Artifacts/ProductShell/AAA/R17/Package/Mac/AeroSim.app`
- 输入矩阵：`Artifacts/ProductShell/AAA/R17/qa-input/key-matrix.json`
- Flight HUD：`Artifacts/ProductShell/AAA/R17/qa-input/flight-hud.png`
- 动物截图：`Artifacts/ProductShell/AAA/R17/qa-wildlife/`
- 真实输入航线：`Artifacts/ProductShell/AAA/R17/qa-route/route-log.json`
- Automation 日志：`Artifacts/ProductShell/AAA/R17/logs/automation-final4.log`
- Package 日志：`Artifacts/ProductShell/AAA/R17/logs/package-final.log`

## 下一轮优先级

1. 把每个物种从合并程序化批次升级为独立实例/骨骼结构，支持不同相位的呼吸、头尾和步态动画。
2. 为近景动物引入高质量网格、法线与材质层，优先提升鹿、狼、熊的脸部与四肢连接。
3. 加入受预算约束的群体漫游、避障、饮水/觅食状态和距离分级更新。
4. 增加动物发现卡片、物种图鉴和可关闭的观察模式，但继续确保 UI 不抢飞控焦点。
5. 由用户对本轮截图和实机试飞作最终视觉判断。
