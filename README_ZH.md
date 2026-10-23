# Neural Coprocessor · 中文分支（MGPU Bridge）

> 这是 **maohgad-web/Neural-coprocessor 的个人分支**，基于作者 **0.4.0**（提交 `69d6e9d`）加上我们自己的改动。
> 主项目、原始文档与发布包在作者仓库：<https://github.com/maohgad-web/Neural-coprocessor>
>
> 说明：本分支的中文文档由 AI 起草、人工核对内容；语气可能偏正式，见谅。

---

## 这是什么

让**一张显卡渲染游戏、另一张 NVIDIA 显卡跑 DLSS 神经网络**（降噪 / 超分），再由神经卡直出显示器。
渲染卡可以是 A 卡或 N 卡；**神经卡必须是 N 卡**（要跑 NGX）。

```
游戏（渲染卡，D3D12）→ 桥抓帧/深度/运动向量
   → 跨适配器共享堆（PCIe）→ 神经卡跑 DLSS-NR
   → 神经画面直出（显示器接神经卡）
```

---

## 本分支比作者原版多了什么

| 功能 | 键 | 本分支默认 | 说明 |
|---|---|---|---|
| **中文精简面板** | `PanelSimple` / **CTRL+ALT+F4** | **开（1）** | 只显示可调参数，隐藏日志与计数；布局写在 `panel_simple.txt`，可自己改 |
| 面板画在神经画面上 | `PanelOverPicture` | **开（1）** | Home 直接盖在神经层，而不是退回原生画面 |
| 面板信息与高度 | `PanelStats` / `PanelAutoHeight` | **开（1）** | 游戏呈现节奏 / 自适应高度 |
| 画风键总开关 | `Tuning` | **开（1）** | 才读取 `Style / Tone / Structure / Skin` |
| **隔帧运动向量拷贝** | `MVecEveryOther=N` | **关（0）** | 每 N 帧只拷 1 次向量；FC6 4K 实测 `2` = **70 → 75 帧（+7.1%）**，无明显拖影。默认关，因为这是画质/帧率的取舍项 |
| 呈现限速（仪器） | `PresentMaxFps=N` | 关 | 未武装时 present 会空转（240–489 次/秒），这是给它加的上限；**不提升帧率** |
| 拷贝/等待判别（仪器） | `MVecCopyOnly=1` | 关 | 拷贝照跑、神经端拿零向量场，用来测"拷贝本身 vs 等待"的成本 |
| 半分辨率向量（已否决） | `MVecHalf`、`MVecReconstruct` | 关 | 省 3/4 带宽但**画面变糊**（含静态菜单），实测两次否决 |
| 按需拷贝（已否决） | `MVecCopyOnDemand=1` | 关 | 消费端只评估最新帧，生产端无法预知，会饿死车道 |
| 面板内 present A/B（诊断） | 面板勾选框 | 勾选 | 注销 present 会让面板/热键失效，仅用于确认"present 分发是承重的" |

> 除上表标"开"的几项外，其余开关默认关闭。
> **每个键的意思、改错会有什么后果、遇到问题该改哪一项 → 见 [`KEYS_AND_RISKS_ZH.md`](KEYS_AND_RISKS_ZH.md)。**

> 注意：`Depth=1`（本分支与上游一致）要求 ReShade preset 里启用 **`MGPU_DepthTap`**，否则神经端拿不到深度，面板会提示 `Depth=1 needs MGPU_DepthTap`。

---

## 系统要求

| 项 | 要求 |
|---|---|
| 系统 | Windows 10 1809+ / Windows 11 |
| 显卡 | 渲染卡 + **NVIDIA 神经卡**（Turing 及以上） |
| 显示器 | **接在神经卡上** |
| 游戏 | D3D12 |
| 软件 | ReShade 6.x + 本分支的 `nvngx.dll_mgpu_bridge.addon64` |

---

## 安装

1. 给游戏装好 ReShade 6.x（D3D12）；
2. 把 `nvngx.dll_mgpu_bridge.addon64` 放到游戏 exe 同目录；
3. 同目录放 `mgpu.ini`（作者发布包里有）；
4. 想用中文精简面板：同目录放 `panel_simple.txt`（本仓库 `assets/panel_simple.txt` 是模板），
   并在 `ReShade.ini` 里设：
   ```ini
   Font=C:\Windows\Fonts\msyh.ttc
   ```
   不设也能用，面板会自动显示英文标签。
5. 启动游戏，按 **Home** 打开面板。

---

## 操作

| 热键 | 作用 |
|---|---|
| **Home** | 打开/关闭面板 |
| **CTRL+ALT+F10** | 武装（arm）；设置多数在武装时读取 |
| **CTRL+ALT+F4** | **完整面板 ↔ 中文精简面板**（本分支新增） |
| CTRL+ALT+F6 | 隐藏/恢复神经画面（peek） |
| CTRL+ALT+F7 | 循环切换呈现模式（output / input / split） |
| CTRL+ALT+F8 / F9 / F11 | 强度：切目标 / 降一档 / 升一档 |
| CTRL+ALT+← / → | split 模式的分割线（加 Shift 粗调） |

---

## 中文精简面板怎么用

1. 按 **Home** 打开面板 → 滚到最下方 `Simple panel`；
2. 勾上 `PanelSimple`（或直接按 **CTRL+ALT+F4**）；
3. 改同目录的 `panel_simple.txt`，格式：

   ```txt
   id | 中文标签 | 说明（写 \n 表示换行）
   ==== 分区标题 ====
   ```

   - 不写进文件的行**不会显示**；顺序就是显示顺序；
   - 以 `=` 开头的是**分区标题**；
   - 面板里的 `重新加载` 按钮可热重载，不用重启游戏；
   - 未知 id 会在日志里报 `[R353]` 并跳过。

4. 可用 id 见 `assets/panel_simple.txt` 的分组注释（共 25 个）。

---

## 如何测试（不靠感觉，看日志）

游戏目录的 `ReShade.log`，按标签 grep：

| 标签 | 看什么 |
|---|---|
| `[T2]` | 两张卡是否都被枚举 |
| `[R265]` / `[R185]` | 显示器是否被认为接在神经卡 |
| `[P4.0] stream arm hr=` | 武装是否成功（`hr=0x0`） |
| `[R101]` | 校准器：`resolved=0` 表示游戏没有 DLSS 契约（无 DLSS 游戏正常） |
| `[R78] MVEC TRANSPORT` | `copies` / `missing` / `region` / `bytes` |
| `[R300a5]` | `vector_present`（应接近 100%） |
| `[R349]` | 隔帧拷贝是否生效（`MVecEveryOther=N`） |
| `[R351]` / `[R351b]` / `[R352]` | 精简面板加载 / 热键 / 模式切换 |
| `[R353]` | 布局文件里写了未知 id |

性能读数**以面板 `ON SCREEN` 为准**（那是呈现节奏 = 眼睛看到的）；
`[R333]` 日志是它最近 120 次呈现的中位数，和面板瞬时值会差几帧。

---

## 我们做过的验证（摘要）

| 项目 | 结果 |
|---|---|
| FC6 4K（无 DLSS，AA 关）原生 | 98 帧 |
| + 桥存在（`Neural=0`） | 85 帧（−13） |
| + 运动向量拷贝（全分辨率） | 73 帧（−9） |
| + NR 0.66 | 70 帧（−3） |
| **+ 隔帧拷贝（本分支）** | **75 帧（+7.1%）**，无可见拖影 |
| 半分辨率向量 | 80–81 帧，但**糊 + 闪**，否决 |
| 深度传输 | 0 帧代价、0 画质收益（保持关闭） |
| 呈现链（DComp/独立窗口/vsync/限速） | 均无改善，属架构成本 |

详细数据见 `30_experiments/FC6/` 与给作者的报告。

---

## 已知限制

- 只有 **N+N** 组合被完整验证；**A 卡渲染 + N 卡神经尚未实测**（测试步骤见我们的 A+N 测试文档）；
- 只有 PCIe 通道，没有 NVLink，带宽是硬上限；
- 显示器必须接神经卡；
- 中文标签依赖 ReShade 加载的字体；
- 本分支的实验键可能随时改动，**不要当成稳定 API**。

---

## 与作者的关系

- 上游：<https://github.com/maohgad-web/Neural-coprocessor>（作者 Marcelo Guibout Rojas）；
- 我们向上游提交的 PR 只包含**单独、可评审**的功能（例如中文精简面板），不包含实验键；
- 问题优先看作者的 README、`docs/` 与我们给作者的 issue。

## 许可

跟随上游项目。原始代码与文档版权归作者所有；本分支新增部分同样按上游许可发布。
