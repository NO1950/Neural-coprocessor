# 附：本分支的默认配置、每个键的意思，以及改错会怎样

> 这一节对应 `assets/mgpu.ini`（以及你游戏目录里那份）的默认值。
> **改之前先读一行**：所有键都在**武装时（CTRL+ALT+F10）**读取，改完要么重新武装、要么重启游戏。
> 表里"风险"一列不是吓人：标 ⛔ 的那几项，改错会直接黑屏、糊屏或掉一半帧。

---

## 1. 本分支与上游的默认值差异

| 键 | 上游默认 | 本分支 | 说明 |
|---|---|---|---|
| `PanelSimple` | 无（关） | **1** | 启动即进入中文精简面板 |
| `PanelOverPicture` | 无（关） | **1** | 面板画在神经画面上（Home 直接盖在神经层，而不是退回原生） |
| `PanelStats` | 无（关） | **1** | 面板显示游戏呈现节奏 |
| `PanelAutoHeight` | 无（关） | **1** | 两个框用 overlay 高度 |
| `Tuning` | `0` | **1** | 才读取 `Style / Tone / Structure / Skin` 这些画风键 |
| `MVecEveryOther` | 无（关） | **0** | 隔帧拷贝。**本分支默认关**，因为它是画质/帧率的取舍项 |
| `Perf11` | 注释（关） | **1** | 预取路径的总开关 |
| `PerfPrefetch` / `Max` | 注释（关） | **1 / 2** | 预取目标帧与深度 |
| `MVecTap` / `MVecSource` / `MVecArmWait` / `MVecScaleMul` | 注释/无 | **1 / 0 / 1800 / 1.0** | 向量来自 tap、按句柄钉住源、武装等待上限、尺度倍率 |
| `NRScale` | 注释（关） | **1** | 第一遍处理的分辨率缩放（见下方风险） |
| `NRMode` | 无 | **1** | 1 = 残差模式（推荐）；0 = 直接模式 |
| `NRDetailGain` / `NRSharpness` / `NRColorStrength` | 无 | **1 / 1.00 / 0.90** | 细节 / 锐化 / 颜色强度 |
| `Style` / `Tone` / `Structure` / `Skin` | `0 / 1.0 / 1.0 / 1.0` | **0 / 1.0 / 1.0 / 1.0** | 画风（需 `Tuning=1` 才生效） |
| `SRUpscale` | 注释（关） | **0** | 上游超分（DLSS SR）交给神经卡。没有 DLSS 的游戏无效 |
| `MVecProbe` | `0` | **0** | 向量探针（诊断） |
| `Depth` / `DepthTransport` / `MvecFromEval` | `1 / 3 / 2` | **1 / 3 / 2** | 与上游一致：开深度、走契约优先的向量路线 |
| `ResidualRender` | 注释 | **删除** | 第二遍 light/full 的选择键，本分支不设 |

---

## 2. 每个键的意义与"改错会怎样"

### ⛔ 会立刻毁掉画面的

| 键 | 意义 | 改错的后果 |
|---|---|---|
| `NRScale` | 第一遍神经处理的分辨率缩放。**越小越省**，越大越贵 | `1.00` = 全分辨率，**最贵**（我们实测 ≈47 帧）；`0.84` ≈52；`0.66` ≈70；`0.30` ≈73。设成 `0` 相当于关掉这条路径 |
| `NRMode` | `1` = 残差（只在原生画面上叠加神经差异）；`0` = 直接（完全用神经结果） | 直接模式更"猛"，容易糊、颜色漂移 |
| `MVecEveryOther` | 每 N 帧只拷 1 次运动向量，其余帧复用 | `2` 省 5 帧、无明显拖影；`3` 帧率相同但拖影风险更大；**不要设 >4** |
| `MVecHalf` / `MVecReconstruct` | 半分辨率向量 / GPU1 重建 | **我们实测两次都糊（含静态菜单）**，别开 |
| `SRUpscale` | 把游戏原生 DLSS 交给神经卡 | 游戏没有 DLSS 时无效；有 DLSS 时若分辨率/预设不匹配会报错或花屏 |
| `Depth` | 传深度给神经端 | **`1` 时 preset 里必须有 `MGPU_DepthTap`**，否则面板会提示 `Depth=1 needs MGPU_DepthTap`，神经端拿不到深度 |
| `DcompOverlay` | 合成进游戏窗口（1）或桥自己开窗（0） | `0` 会出现两个窗口；某些游戏全屏独占下会黑屏 |

### 🟡 改错会掉帧/白忙的

| 键 | 意义 | 建议 |
|---|---|---|
| `Perf11` / `PerfPrefetch` | 预取：让消费端等"拷贝队列已武装的那一帧" | **永远别关 `Perf11`**：它会静默连带关掉 `PerfPrefetch`，我们在另一个游戏上实测掉 12% |
| `MVecTap` / `MVecSource` / `MVecArmWait` | 向量来源与武装等待 | `MVecSource` 改为固定句柄会把向量钉死在一个旧缓冲上，移动物体拖影明显 |
| `MVecScaleMul` | 向量尺度倍率 | 偏离 `1.0` 等于告诉模型"运动比你看到的快/慢 N 倍"，错一点就整屏拖 |
| `FrameCap` | 限制游戏与生产端帧率 | 设太低会直接锁死帧率；`0` = 不限制 |
| `PresentMaxFps` | 给"未武装时"的呈现循环加上限 | 仪器键，**不提升帧率**；默认 0 即可 |

### 🟢 只会改变观感/面板的

| 键 | 意义 |
|---|---|
| `Style` / `Tone` / `Structure` / `Skin` | 画风预置与强度（需 `Tuning=1`）；`Style` 0/1/2 = A/B/C |
| `NRDetailGain` / `NRSharpness` / `NRColorStrength` | 细节/锐化/颜色；锐化过高会有白边 |
| `PanelSimple` / `CTRL+ALT+F4` | 精简面板；不设则一直是完整面板 |
| `PanelOverPicture` | 面板开在神经画面上（推荐 1，Home 才能看到真实画面） |
| `PanelStats` / `PanelAutoHeight` | 面板信息与高度，纯显示 |
| `MVecProbe` | 诊断探针，正常玩建议 0 |
| `Tuning` | 画风键的总开关；0 = 上面四个键全部忽略 |

---

## 3. 我们踩过的坑 → 遇到什么改什么

| 症状 | 先看 | 建议改动 |
|---|---|---|
| **帧率明显偏低** | 面板 `ON SCREEN` | `NRScale` 降到 0.66 或 0.45；`MVecEveryOther=2`；确认 `Perf11=1 PerfPrefetch=1` |
| **画面发糊、菜单也糊** | 是否开了半分辨率 | `MVecHalf=0`、`MVecReconstruct=0`；`NRScale` 不要低于 0.45 |
| **运动物体拖影/错位** | `[R78]` 与 `[R300a5]` | 关 `MVecEveryOther`；`MVecScaleMul=1.0`；`MVecSource=0` |
| **画面闪烁、尤其静态菜单** | 是否 `MVec=0` | 必须有向量：`MVec=3`；`MVec=0` 连菜单都会闪 |
| **屏幕全黑/只有游戏窗口** | `[V49]` / `[T5]` | `DcompOverlay=0` 试试；或 `CTRL+ALT+F6` 确认视觉有没有被藏起来 |
| **面板打不开 Home 没反应** | 是否注销过 present | 重启游戏（面板注销 present 后不会自己恢复） |
| **中文显示成方块** | `ReShade.ini` | 加 `Font=C:\Windows\Fonts\msyh.ttc`，重启 |
| **精简面板空白** | `panel_simple.txt` | 文件必须在游戏目录；点"重新加载"；未知 id 看 `[R353]` |
| **`[R78]` 的 carried 不是 100%** | 向量源 | `MVecTap=1`；换 `MVecSource`；确认游戏关闭 AA |
| **面板提示 `Depth=1 needs MGPU_DepthTap`** | preset | 在 ReShade 面板里启用 `MGPU_DepthTap`，或把 `Depth=0` |
| **启动就崩/进游戏掉驱动** | 最近改的键 | 一次只改一个键；从 `Depth=0`、`MVec=0`、`NRScale` 小值开始二分 |

---

## 4. 最保守的一套（排错起点）

```ini
Neural=1
MVec=0            ; 先不要向量，确认"有没有神经画面"这一层
Depth=0
NRScale=0.66
Perf11=1
PerfPrefetch=1
MVecEveryOther=0
DcompOverlay=1
```

画面正常、帧率正常之后，再按 `MVec=3 → MVecTap=1 → Depth=1(配 tap preset)` 的顺序逐项加回来。
每一步之间按 **Home** 看一眼画面、读一次面板 `ON SCREEN`——不要连续改三个键再试。
