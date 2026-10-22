# Field report: Far Cry 6 (no native DLSS) on a 2× RTX 5070 Ti MGPU bridge

**Audience:** the MGPU Bridge author. Everything below is measured on one machine, one game, over ~40 rounds
across 2026-10-18 … 2026-10-22. Every claim is tied to a log line or a single-variable A/B; where it is not,
it says so.

**Build under test:** 0.4.0 + three local patches (R330/R331/R332, §7). Binary `sha16 AFDC12D9471DF556`.
**Driver:** 616.56 (`32.0.16.1656`) for everything after 2026-10-21; the 2026-10-18 archive is 617.42
(`32.0.16.1742`) — see §5.
**GPUs:** 2× RTX 5070 Ti (device `0x2C05`, 15995 MB each), game on adapter 0, neural stage on adapter 1.
**Game settings:** 3840×2160, **anti-aliasing OFF**, no DLSS/FSR, motion blur off, vsync on. The title has
no NGX feature of its own, so the bridge synthesises the contract (see §4 for why we then removed it).

---

## 0. TL;DR

1. **The bridge can run with no NGX contract at all.** On this title the bridge's own D3D12 tap reads the
   game's velocity buffer (a four-channel, scene-size target that FC6 writes every frame even with AA off),
   derives UV-space vectors, and the lane carries 100% into the neural pass. `[R101] resolved=0` and NR still
   runs. No feeder, no Lumenite, no synthetic DLAA contract.
2. **Frames evaluated while the MV lane is empty are the one condition that visibly destroys the picture.**
   The only round that ever went "unusable" was the one that evaluated 256 frames with no vector bound while
   the queue was deep (`PerfPrefetchMax=4`, age 44→61 ms): menu flickers, gun-sight reticle unreadable.
   Everything else that looked bad had a dead lane (`0% carried`) or a broken scale, not a picture problem.
3. **Slow-pan shadow jitter is not the bridge, not the timing, not the vectors, not the depth, not the work
   resolution.** It reproduces identically on a single card with a completely different host (RenoDX DLSS,
   same 165 MB NR runtime, 2534×1426). It is the NR model on this content: surface/optical-flow motion vectors
   cannot describe where a shadow edge is going.
4. **A feeder that writes its own NR result back into the backbuffer costs sharpness.** Single-variable A/B:
   same MV path, feeders on/off → the feeder-on frame is visibly softer (fine knurling on the gun sight
   unreadable). We removed the feeder; the picture is the best it has been.
5. **Inference (not proven):** the operator reports the picture became much more stable right after a GPU
   driver rollback on ~2026-10-18/19 (617.42 → 616.56). That is the only environment change that matches the
   date. We did not A/B it, so treat it as correlation, not causation.
6. **The contributed `NRScale` path explains why 0.66 stays sharp** (this path is ours, so this is a note, not a
   question): it is a *residual composite* — NR runs at 0.66 and only its **difference** is added back onto the
   native colour, so the high frequencies never leave the native image. A proxy that enlarges a low-res NR
   output at the same fraction does not have that property (§6).

---

## 1. Method (so the numbers can be trusted)

* One key per round, `mgpu.ini` written before launch, game restarted, and the **actual resolved values
  re-read from the log** before any verdict (`[OURS][NRS] scale`, `[R78]`, `[R330]`, `[R332]`, `[R301]`).
  Twice a round was silently a different configuration than intended (a launch race and an ini key that the
  calibrator overrides); both are now gated by a harness that refuses to arm while the game is running and
  prints `ROUND VALID: yes/no`.
* Frame captures via a global hotkey (10 Hz, 640×400 ROI) so the operator never has to alt-tab; objective
  metrics: slow/fast high-frequency ratio, edge-gradient temporal variance.
* Verdict language is the operator's own, in Chinese, recorded verbatim in the observation log.

---

## 2. Which keys/conditions produce obvious, unusable flicker

Ranked by how bad it looked. This is the part we most want upstreamed as guards.

| Condition | What the operator saw | Log evidence |
|---|---|---|
| **Frames evaluated with no vector bound while the queue is deep** (`PerfPrefetchMax=4`, age 44→61 ms) | "markedly blurrier, the reticle graduations are gone, the main menu flickers" — the only *unusable* round of the whole campaign | `[R87]` worst 256 frames with no vector yet still evaluated; `[R300a2] missing` |
| **`MVec=0`** (MV parameter not bound at all) | slow pans unusable, everything smears | deliberate control round; immediately reversed |
| **`TemporalResetK=1`** (force `Reset=1` on every evaluate) | "flicker starts in the menu; in game it is everywhere, eye-watering" — unjudgeable | `resets_forced=4390` |
| **Dead MV lane** (`0% carried`, region 1280×720 vs a 3840×2160 scene) | looked soft/ghosty; **not a picture verdict** — we nearly recorded one as a result | `[R78] 0.0% carried`, `size-rejected=12488` |
| **Scale override on a tap-sourced (UV) field** — `R105` replaced the learned `3840×2160` with the game table's `1.0` | heavy ghosting (MV effectively 1/3840 of intended) | `[R300a1] scale_ngx=1.000`, `[R278c] learned UV 3840×2160` |
| `MVecScaleX/Y` in ini with a resolved contract | silently discarded — "the ini said 1.0300, the game says 1.0000" | `[R105]` |

Things that produced **no** visible flicker (each a single-key round, judged blind to which was active):
`NRScale` 0.66→1.00; `Depth` 1→0; MV scale ×0.925 / ×0.714 / ×0.571; age 1.63→2.98→3.6 game frames;
`PerfPrefetchMax` 1/2/4 at a live lane; `TemporalDiag=1` (behaviour-neutral).

---

## 3. Slow-pan shadow jitter — what it is not

Phenomenology: **only while the camera moves, worst at slow pans, gone the instant it stops**; shadow
boundaries shimmer/crawl, and the same frame is stable when static. The operator can see it in the gun/arm
silhouette shadows indoors.

Excluded, one axis at a time (all with `tuple_ok=100%`, `overwritten=0`, `missing=0` unless noted):

| Hypothesis | Round(s) | Result |
|---|---|---|
| vector/colour from different frames | 12+ rounds | `same_tuple_all=99.9–100%` — excluded |
| input age / queue depth / temporal span | `gf_old` 1.63 / 2.12 / 2.98 / 3.6 | no perceptible change |
| zero vs real vectors | own tap with real UV field vs feeder's optical flow vs zero | no change to shadow jitter (real vectors only fixed smearing) |
| depth (precision, format, presence) | 16↔32 bit, `Depth=1` vs `0` | no change; `Depth=0` leaves `ngx` time identical (11.26 vs 11.27 ms) ⇒ NR does not consume it |
| work resolution / residual upscale | `NRScale=1.00` | "about the same", at 22 ms / 38 fps |
| cross-GPU transport | **RenoDX DLSS single card**, same NR runtime, same 2534×1426 | **"about the same"** ⇒ not a cross-GPU artefact |
| temporal history | reset-every-frame | unjudgeable (global noise) |

Conclusion we are confident in: **it is the model on this content.** Shadow-edge motion is not in the motion
field (surface/optical flow both describe the receiver, not the shadow), so the temporal kernel has nothing
correct to reproject against. If the author knows a knob that changes the NR temporal kernel's behaviour on
disocclusion-like regions, that is the only remaining lever we can see.

---

## 4. Removing the feeder made the picture sharper (single-variable)

We had been running a feeder (RenoDX DLSS5-Feed) that creates a synthetic DLAA contract for titles without
DLSS, and writes its own NR result back into the game's backbuffer. We measured it:

* D1 — feeder off, bridge's own tap vectors, `Calib=2`: gun-sight knurling **clearly legible**.
* D1c — feeder on, **same tap vectors**, `Calib=0` (so `[R105]` could not touch the learned UV scale),
  everything else identical: **visibly softer**, same content, same position.
* Mechanism confirmed in the feeder log: `frame N delivered (3840x2160, reset=0, same-device)` — every frame.
  The bridge then consumes a colour buffer that has already been through an NR pass, i.e. **NR applied twice**.

After removing the feeder the whole stack is also simpler: no contract, no Lumenite headers, no MV provider
selection. `[R278c]` learns the UV scale from the tap source (3840×2160), the lane carries 100%.

---

## 5. Inference: the "it got stable" date matches a driver rollback (not proven)

Archive `adapter[0] driver=` values:

| Date | Rounds | Driver |
|---|---|---|
| 2026-10-18 | all 35 archived rounds | **32.0.16.1742 (617.42)** |
| 2026-10-21 → today | all rounds | **32.0.16.1656 (616.56)** |

The operator independently reports: "around 10-18, after the ShortFuse deployment failed a few times and I
rolled the driver back, the arm/gun shadow jitter was gone." That is the only environment change matching the
date; our own code changes are diagnostics and guards and do not touch the NR inputs or parameters (§7).
**We did not A/B the driver**, so this is an inference. Every round now records the driver version in its meta
so a future A/B is cheap.

---

## 6. Why the `NRScale` path stays sharp at 0.66 where a proxy's `ResolutionScale` does not (this path is ours)

`NRScale` / `NRScaleResolveDepth` is our contribution (PR #60; `nrs_mode=1` = residual composite), so this
section is a note on what we learned implementing it, not a question.

| | our `NRScale` path | a proxy's `ResolutionScale` path (ShortFuse) |
|---|---|---|
| what the model sees | native colour downsampled to `NRScale` (2534×1426 at 0.66) | the colour downsampled to ~0.7 |
| what comes back | the model's **difference** from its input, upsampled and **added back onto the native frame** (`nrs_mode=1`) | the model's **low-res output**, enlarged to 4K by an enlargement shader |
| high frequencies | never leave the native image — the residual only adds what the model changed | discarded at the downsample; the enlargement cannot invent them |
| shaping | `NRSharpness=0.70`, `Structure/Tone/Skin` strengths, `NRColorStrength`, `NRMode`, `Style=2` | `Sharpness=0.20`, `Style=0` |

Consequence: at the same fraction the residual path keeps the native detail and only borrows the model's
denoise/detail pass at low resolution; the enlarge path throws the detail away first. Our measurements agree:
`NRScale` 0.66 vs 1.00 is "about the same" to the eye here, while the proxy below 0.70 is obviously soft.

**Cost: the residual path is cheaper, not more expensive.** At 0.66 the neural pass is 11.3–13.7 ms of `ngx`
(54–62 neural fps) against **22.1 ms / 38 fps** at 1.00, and the composite is one full-screen pass.
Note also the `[C2-SR]` warning in the log: with the game at native (the DLAA case) there is nothing for SR to
enlarge, so the sharpness here comes from the residual composite, not from an SR pass.

---

## 7. One bug and three patches: problem → evidence → patch → risk

Our final configuration takes the vectors from **your own D3D12 tap** and the depth from **your own depth tap**
— no feeder, no external provider, no changes to the acquisition path. So each item below is written so you can
decide **independently**: what went wrong, the log evidence, what the patch changes, what it could regress, and
how to switch it off. We are happy to split them into separate PRs, or to keep them local if you would rather
not carry them.

### 7.1 Bug: `[R105]` overwrites a tap-derived scale — motion divided by the scene width

* **Symptom.** Severe ghosting on a slow pan; everything trails as if the camera moved 1/3840th as far as it did.
* **What happens.** The MV comes from the tap (UV units), so `[R278c]` correctly derives `MVecScale = 3840×2160`
  from source extent / vector extent. But when a contract also exists (native DLSS, or any synthetic DLAA
  feature), `[R105]` replaces that scale with the game table's `1.0`:
  `[R278c] learned UV 3840×2160` → `[R300a1] scale_ngx=1.000, scale_passed=3.000`.
  The model then receives 1/3840 of the intended motion.
* **Repro.** `MvecFromEval=0`, `MVecTap=1`, contract present, `Depth=1`, any scene: watch `[R278c]` then
  `[R300a1]`. Two different sources of truth for one number.
* **Patch.** Make the scale follow the **source that actually published**, not the mere existence of a contract:
  when the published handle is the tap's virtual target, keep the `[R278c]` learned scale; otherwise leave
  `[R105]` exactly as it is today.
* **Scope — this does not change which source is used.** A title with native DLSS keeps taking its vectors from
  the contract (`MvecFromEval=2`), and therefore keeps the table's scale: nothing changes for it. The fix only
  separates two decisions that are currently coupled — *which field* (`MVecTap` / `MvecFromEval`) and *what
  units that field is in*.
* **Risk.** Very low: contract-sourced fields keep the contract's scale; only a tap-published field changes, and
  today that field is simply wrong.
* **Workaround we use today.** `Calib=0` (turns the whole calibrator off) — which also loses the contract-side
  diagnostics, hence the request.

### 7.2 R330 + `MVecArmWait` — the startup-contract trap

* **Symptom.** A whole round reads as "the picture is bad" when in fact the velocity lane carried nothing:
  `[R78] region 1280x720 ... copies=0 ... size-rejected=12488 ... 0.0% of sealed frames carried vectors`.
* **What happens.** FC6 builds its contract at a loading-screen extent (1280×720), the handle then stays
  *identical* for hundreds of frames while the game rebuilds at 3840×2160, so the arm sizes the region
  1280×720 against a 4K scene and size-rejects everything afterwards.
* **Patch.** Withhold the arm while the published extent is under a quarter of the scene, bounded by
  `MVecArmWait` frames (default 900; we run 1800), then take it anyway and say so once
  (`[R330] taking the small velocity source anyway`). Also prints a one-line "this run carries no vectors"
  verdict so a dead lane cannot be mistaken for a picture result.
* **Risk.** A title whose MV is honestly low-resolution waits once for `MVecArmWait` frames.
  `MVecArmPlausible=0` restores the previous behaviour exactly.

### 7.3 R331 — a changed provider is a different history

* **Symptom.** After a contract is recreated (menu, device reset, resolution change) the picture keeps a
  ghost of the previous buffer for a while; tuple counters are 100% because the lane is self-consistent —
  it is just a *different* buffer.
* **Patch.** Taken from GITC-Uplift's provider rule: require 12 consecutive frames of the new handle, then
  force one reset hint. The hysteresis exists because some titles alternate pool members every frame.
* **Risk.** Low; the 12-frame window is the only tunable, and a title that never changes source never sees it.

### 7.4 R332 `MVecScaleMul` — diagnostic only, optional

* **Why.** `[R105]` makes the ini's `MVecScaleX/Y` untestable on any title whose table resolves, so a units
  question ("is the field in pixels or in UV?") could not be asked at all. The multiplier applies on top of
  whatever `[R105]` resolves.
* **Result here.** ±50% is objectively worse (slow/fast HF ratio 0.925 / 0.714 / 0.571) and **invisible to the
  eye** — i.e. the units were already right, which is exactly the kind of negative result this key exists to
  produce. If debug keys are unwelcome, drop this one.

**Our suggestion:** take 7.1 regardless; 7.2 and 7.3 only if you want the guards for the contract path
(most users are on that path); 7.4 is yours to judge.

---
## 8. Reproducing

* `70_tools/verify/timing-round.ps1` — arms a round (refuses while the game runs), writes `mgpu.ini`, records
  the driver, verifies the *resolved* values after launch, prints `ROUND VALID: yes/no`.
* `70_tools/verify/deploy-check.ps1` — one-second PASS/FAIL on bridge hash, feeder state, nine ini keys,
  preset contents, driver fingerprint.
* `70_tools/verify/hotkey-capture.ps1` + `flicker-metrics.py` / `shadow-flicker.py` — captures and metrics.
* Raw logs, per-round `mgpu.ini.armed` and `meta.txt` for all rounds cited above are kept alongside each run
  directory (`30_experiments/FC6/runs/<date>-<round>/`).
