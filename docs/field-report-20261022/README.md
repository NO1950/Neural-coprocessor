# Field report package — Far Cry 6 on a 2× RTX 5070 Ti MGPU bridge (2026-10-22)

**English-only, trimmed.** Everything here was measured on one machine: 2× RTX 5070 Ti, driver 616.56,
Far Cry 6 at 3840×2160 with **anti-aliasing off** and no native DLSS, bridge 0.4.0 + three local patches
(`sha16 AFDC12D9471DF556`).

## Files

| File | What it is |
|---|---|
| `ISSUE.md` | The report as a GitHub-issue body: summary, which conditions make the picture unusable, the slow-pan shadow investigation, feeder removal vs sharpness, the driver inference, why the `NRScale` residual path stays sharp at 0.66, and one bug + three patches with risk and off-switches. **Start here.** |
| `evidence/01…07` | One file per claim, each a plain extraction of the log lines that prove it (no interpretation added). |
| `logs/key_runs.zip` | Four complete run folders: `ReShade.log`, `dlss5-feed.log` and the exact `mgpu.ini` for D1 (feeder off), D1c (feeder on, same MV path), E7 (feeder-free baseline) and M2 (the dead-lane round). |
| `patches/0001-mvec-guards-R330-R331-R332.patch` | `src/gpu1_context.cpp` diff against 0.4.0: R330 arm hold + `MVecArmWait`, R331 provider-change reset, R332 `MVecScaleMul`. |
| `patches/0002-mvec-probe-R75.patch` | `src/probe.cpp` diff: read-only `[R75]`/`[R76]` diagnostics (not proposed for upstream). |
| `patches/README.md` | How the changes are kept on a fork branch and how a PR would be split. |

## Evidence index

| File | Claim it supports |
|---|---|
| `01_dead_lane_and_R330_hold.txt` | The arm took a 1280×720 loading-screen contract against a 3840×2160 scene; every copy was size-rejected and the round carried **0.0%**. Shows `[R330]` withholding and the bounded take. |
| `02_R105_overrides_tap_scale.txt` | With a contract present, `[R105]` replaced the tap's learned UV scale (3840×2160) with the table's 1.0 (`[R300a1] scale_ngx=1.000`). |
| `03_feeder_writes_backbuffer.txt` | The feeder delivers its own NR result into the game backbuffer every frame (`frame N delivered`), so the bridge consumed an already-filtered colour. |
| `04_tap_field_no_contract.txt` | Feeder-free baseline: `[R280]` picks a four-channel scene-size target, `[R278c]` learns the UV scale, `[R101] resolved=0` (no contract), `[R75]` reads a real field, `[R78]` 100% carried. |
| `05_T2_unusable_round.txt` | The only round that looked unusable: 256 frames evaluated with no vector bound while the queue was deep. |
| `06_driver_timeline.txt` | Every archived round's `adapter[0] driver=`: 2026-10-18 = 32.0.16.1742, 2026-10-21 onward = 32.0.16.1656. |
| `07_D1_vs_D1c.txt` | The single-variable sharpness A/B: same MV path, same keys, only the feeder on/off, with both install manifests. |

## How to read the numbers

* `[R78]` is the only line that says whether the velocity lane carried. A round at `0.0%` has **no** picture verdict.
* `[R300a2]` says whether the vector and the colour belong to the same game frame (100.00% in every round cited).
* `[R75]` reads the content of the published field; `[R278c]` says which units it is in; `[R300a1]` prints the scale that reached NGX.
* `[R330]` is our local arm guard; `[R101] resolved=` tells you whether any NGX contract exists at all.
