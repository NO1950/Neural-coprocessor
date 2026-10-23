# Neural Coprocessor · Chinese fork (MGPU Bridge)

> **This is the English translation of [`README.md`](README.md), which is the Chinese document GitHub
> shows on the repository page. The author's original README is kept unchanged as
> [`README_original.md`](README_original.md).**
>
> This repository is a **personal fork of maohgad-web/Neural-coprocessor**, based on the author's
> **0.4.0** (commit `69d6e9d`) plus our own changes. The upstream project, its documentation and its
> releases live at <https://github.com/maohgad-web/Neural-coprocessor>.
>
> Chinese documents: [`TRAPS_AND_FIXES_ZH.md`](TRAPS_AND_FIXES_ZH.md) (every problem we hit and how it
> was solved, across ER / CP2077 / KCD2 / BO3 / FC6), [`KEYS_AND_RISKS_ZH.md`](KEYS_AND_RISKS_ZH.md)
> (what each key does and what breaking it costs), [`BRANCH_GUIDE_ZH.md`](BRANCH_GUIDE_ZH.md)
> (branches in PowerShell and GitHub).
>
> A note on the prose: the Chinese documents were drafted with AI and checked by hand, and this
> English version is a translation of them. The content is right; the tone may read stiff.

---

## What this is

One GPU renders the game, a second **NVIDIA** GPU runs the DLSS neural network (denoise / upscale),
and the neural card drives the display. The render card can be AMD or NVIDIA; **the neural card must
be NVIDIA** (it has to run NGX).

```
game (render card, D3D12) → the bridge captures frame / depth / motion vectors
   → cross-adapter shared heap (PCIe) → the neural card runs DLSS-NR
   → the neural picture is presented directly (display connected to the neural card)
```

---

## What this fork adds to the author's version

| Feature | Key | Fork default | Notes |
|---|---|---|---|
| **Chinese simple panel** | `PanelSimple` / **CTRL+ALT+F4** | **on (1)** | Only the adjustable parameters; counters and log readouts hidden. Layout lives in `panel_simple.txt` and is yours to edit |
| Panel drawn over the neural picture | `PanelOverPicture` | **on (1)** | Home draws over the neural frame instead of stepping aside to the native picture |
| Panel readouts and height | `PanelStats` / `PanelAutoHeight` | **on (1)** | game present cadence / auto height |
| Master switch for the style keys | `Tuning` | **on (1)** | without it `Style / Tone / Structure / Skin` are ignored |
| **Copy the motion field every Nth frame** | `MVecEveryOther=N` | **off (0)** | one copy in N; FC6 4K measured `2` = **70 → 75 fps (+7.1%)** with no perceptible trailing. Off by default because it is a quality/frame-rate trade |
| Present rate cap (instrument) | `PresentMaxFps=N` | off | the unarmed present loop spins (240–489 presents/s); this caps it. **It does not raise the frame rate** |
| Copy-vs-wait discriminator (instrument) | `MVecCopyOnly=1` | off | the copy runs, the neural side gets the zero field — separates "the copy" from "the wait" |
| Half-resolution motion vectors (rejected) | `MVecHalf`, `MVecReconstruct` | off | saves 3/4 of the bytes but the picture **goes blurry** (static menus too). Rejected twice on the picture |
| Copy on demand (rejected) | `MVecCopyOnDemand=1` | off | the consumer evaluates only the newest frame, so the producer cannot know what will be used and the lane starves |
| Present-event A/B in the panel (diagnostic) | panel checkbox | ticked | unregistering present breaks the panel and the hotkeys; it exists only to prove the subscription is load-bearing |

> Everything not marked "on" above is off by default.
> **What each key means, what breaking it costs, and which key to change for a given symptom →
> see [`KEYS_AND_RISKS_ZH.md`](KEYS_AND_RISKS_ZH.md) (Chinese).**

> Note: `Depth=1` (the upstream default, kept here) requires **`MGPU_DepthTap`** to be enabled in the
> ReShade preset, otherwise the neural side gets no depth and the panel reports
> `Depth=1 needs MGPU_DepthTap`.

---

## Requirements

| Item | Requirement |
|---|---|
| OS | Windows 10 1809+ / Windows 11 |
| GPUs | a render card + an **NVIDIA neural card** (Turing or newer) |
| Display | **connected to the neural card** |
| Game | D3D12 |
| Software | ReShade 6.x + this fork's `nvngx.dll_mgpu_bridge.addon64` |

---

## Installation

1. Install ReShade 6.x (D3D12) for the game.
2. Put `nvngx.dll_mgpu_bridge.addon64` next to the game executable.
3. Put `mgpu.ini` in the same directory (the author's release package includes one).
4. To use the Chinese simple panel, put `panel_simple.txt` there too (`assets/panel_simple.txt` in
   this repository is a template) and set in `ReShade.ini`:
   ```ini
   Font=C:\Windows\Fonts\msyh.ttc
   ```
   Without that line the panel still works — it falls back to the English labels automatically.
5. Launch the game and press **Home**.

---

## Controls

| Hotkey | Action |
|---|---|
| **Home** | open / close the panel |
| **CTRL+ALT+F10** | arm; most settings are read when the bridge arms |
| **CTRL+ALT+F4** | **full panel ↔ Chinese simple panel** (added by this fork) |
| CTRL+ALT+F6 | hide / restore the neural picture (peek) |
| CTRL+ALT+F7 | cycle the present mode (output / input / split) |
| CTRL+ALT+F8 / F9 / F11 | intensity: cycle the target / one step down / one step up |
| CTRL+ALT+← / → | the split seam (hold Shift for coarse steps) |

---

## Using the Chinese simple panel

1. Press **Home** and scroll to the bottom, to the `Simple panel` section.
2. Tick `PanelSimple` (or just press **CTRL+ALT+F4**).
3. Edit `panel_simple.txt` next to the add-on. Format:

   ```txt
   id | 中文标签 | 说明（write \n for a line break）
   ==== section title ====
   ```

   - a row the file does not name **does not appear**; the file's order is the on-screen order;
   - a line starting with `=` is a **section title**;
   - the `reload` button in the panel re-reads the file — no restart needed;
   - an unknown id is reported once in the log (`[R353]`) and skipped.

4. The available ids are listed, grouped, in `assets/panel_simple.txt` (25 of them).

---

## How to test (read the log, not the feeling)

Grep `ReShade.log` in the game directory by tag:

| Tag | What to look at |
|---|---|
| `[T2]` | both adapters enumerated |
| `[R265]` / `[R185]` | which card the display is actually on |
| `[P4.0] stream arm hr=` | did the arm succeed (`hr=0x0`) |
| `[R101]` | the calibrator: `resolved=0` means the game has no DLSS contract (normal for a non-DLSS title) |
| `[R78] MVEC TRANSPORT` | `copies` / `missing` / `region` / `bytes` |
| `[R300a5]` | `vector_present` (should be close to 100%) |
| `[R349]` | is the every-Nth-frame copy active (`MVecEveryOther=N`) |
| `[R351]` / `[R351b]` / `[R352]` | simple panel loaded / hotkey / mode switch |
| `[R353]` | the layout file named an id this build does not have |

Quote the panel's `ON SCREEN` figure as the frame rate — that is the present cadence, i.e. what the
eye gets. The `[R333]` log line is the median of the last 120 presents and can differ from the
panel's instantaneous value by a few frames.

---

## What we measured (summary)

| Case | Result |
|---|---|
| FC6 4K (no DLSS, AA off), native | 98 fps |
| + the bridge existing (`Neural=0`) | 85 fps (−13) |
| + motion-vector copy (full resolution) | 73 fps (−9) |
| + NR at 0.66 | 70 fps (−3) |
| **+ every-other-frame copy (this fork)** | **75 fps (+7.1%)**, no visible trailing |
| half-resolution motion vectors | 80–81 fps, but **blurry and flickering** — rejected |
| depth transport | 0 fps cost, 0 visible benefit (left off) |
| the present chain (DComp / own window / vsync / rate cap) | no improvement from any of them; an architectural cost |

The full data lives in `30_experiments/FC6/` and in the report we sent to the author.

---

## Known limitations

- Only the **NVIDIA + NVIDIA** combination has been fully verified. **AMD render + NVIDIA neural is
  not tested yet** (our test plan exists but has not been run);
- PCIe only, no NVLink — bandwidth is a hard ceiling;
- the display must be connected to the neural card;
- the Chinese labels depend on the font ReShade loaded;
- the experimental keys in this fork may change at any time — **they are not a stable API**.

---

## Relationship to upstream

- Upstream: <https://github.com/maohgad-web/Neural-coprocessor> (Marcelo Guibout Rojas);
- our pull requests carry only **self-contained, reviewable** features (for example the Chinese simple
  panel), never the experimental keys;
- for problems, read the author's README and `docs/` first, then the issues we filed.

## License

Follows the upstream project. The original code and documentation are the author's; the additions in
this fork are released under the same license.
