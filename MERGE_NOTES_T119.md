# Merge candidate: live 0.3.1 (MIRROR NEW, 2026-10-12) + T1.19 as Dom tested it

Not for upload as-is: delete this file before committing.

Source of T1.19: 0.3.1MERGE\Neural-coprocessor-test-main.zip (the test repo main Dom ran, build 0.3.1-t1-test-T119b).
The zip's code equals the test-repo mirror checked on 2026-10-12 (Oursprivatetestbranch0.3.2); extra in the zip: .github (same workflows) and R-CHANGES only.

## What this tree is
- Base: Neural-coprocessor-main 0.3.1 MIRROR NEW, every file.
- Taken from the zip: src/calibrator.cpp/.hpp, dllmain.cpp, gpu1_context.cpp/.hpp, probe.cpp, sl_tags.cpp/.hpp; new src/t119_keys, temporal_diag, perf_diag, feature_diag, producer_diag, producer_gpu_timers (.hpp/.cpp); tests/temporal_diag_test.cpp, perf_diag_test.cpp, feature_diag_test.cpp, producer_diag_test.cpp, check_hlsl_names.py, check_killswitch.py; CMakeLists.txt; assets/mgpu.ini.
- Changed from the zip: dllmain.cpp MGPU_VERSION_STR back to the live "0.3.1-t1" (set the release version yourself).
- Left out: src/mgpu_version.hpp, tests/check_version.py (not used by the build), assets/perf_runs (Dom's run inis), R-CHANGES, .github/test (empty file).

## Checked (2026-10-13)
- diff vs live = the T1.19 file list only; diff vs the zip = the version line + the three left-out items.
- 2026-10-13 14:20: diffed against 0.3.1MERGE\0.3.1LIVEBEFOREMERGE (byte-identical to MIRROR NEW). Isolation audit: every replaced live line reduces to live with keys absent; every added block is behind its key; mgpu.ini +76 lines, all comments, 0 removed. mgpu.ini DcompMultiDisplay comment added (text only).
- clang mingw syntax check, live vs merge: identical error sets (header-shim limits, same as CI-built live); the six new .cpp files clean.
- Dom's four unit tests: all passed (g++).

## Single revision R300 (2026-10-13 14:30)
- docs/R300_T119.md: the public document for the revision (parts, keys, tags R300-R360, adaptations, not merged, checks, how to test).
- "test repository" wording replaced by "R300" in comments, the ini header, CMake comment, check_killswitch.py docstring, and the [R360] log text (tag unchanged). Syntax check re-run: identical to live; check_killswitch.py and check_hlsl_names.py pass.
- REPO_MANIFEST.md: six module rows, tests row, docs row, update date.

## Open before commit
- R numbers: written as R300 owning R300-R360, next free R361 (docs/R300_T119.md). Update MASTER_STATUS "next free" when committed.
- Run 0 on this build (or the test CI artifact, same code but the version string) vs live 0.3.1.
- CONTRIBUTORS.md entry (draft: T119-Review\CONTRIBUTORS_entry_Dom.md, full name to confirm).

## 2026-10-13 14:45 - tags renamed into R300 (Marcelo: one revision, letter + line number)
- Rebuilt from the zip, then: version line restored, the 101 log tags renamed (R300->R300a1 ... R360->R300, BC1/W19/T117/PERSIST -> R300j/k/l/m; table in docs/R300_T119.md), "test repository" wording -> R300, DcompMultiDisplay comment re-applied. Previous tree kept as Merge-0.3.1-T119-public.bak_20261013_e.
- Verified: no old tag left; tag-normalised diff vs the previous tree = only the [R300] summary text; syntax check identical to live; new files clean; Dom's 4 tests pass (asserting the new tags); both scripts pass; no log buffer near its size.
- CONTRIBUTORS.md: entry under the handle only (public name not confirmed). REPO_MANIFEST rows use R300a/e/f/g/h. Next free: R301.
