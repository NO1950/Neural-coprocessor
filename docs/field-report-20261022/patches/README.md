# Keeping the changes on a fork branch (and how a PR would go later)

Only item 7.1 in `ISSUE.md` is a bug report. 7.2/7.3 are guards the maintainer may not want to carry, so the
changes live on our fork first; a PR is optional and, if opened, is split by item.

## Current state

`src/gpu1_context.cpp` is modified (R330/R331/R332, uncommitted in the checkout) and `src/probe.cpp` carries
read-only diagnostics we do **not** propose upstream. Both diffs are exported here as `0001-*.patch` and
`0002-*.patch` against 0.4.0.

## Branch and commits

```powershell
cd <checkout-of-0.4.0>

git status --short                 # expect only src/gpu1_context.cpp and src/probe.cpp
git switch -c field/fc6-20261022   # do not rename this branch once a PR is open: renaming closes the PR

git add src/gpu1_context.cpp
git commit -m "MVec: hold the arm for a plausible source (R330/MVecArmWait), reset on provider change (R331), add MVecScaleMul (R332)

- R330: a title can build its contract at a loading-screen extent and keep the
  same handle across the 4K rebuild, so the lane size-rejects every frame while
  a picture verdict is still taken. Hold until the published extent is plausible
  (MVecArmWait frames), then take it and say so once. MVecArmPlausible=0 restores
  the old arm.
- R331: a changed provider identity is a different history (GITC rule). Require
  12 consecutive frames of the new handle, then force one reset hint.
- R332: R105 replaces the ini scale with the game table's value, so a units
  question is untestable on titles whose table resolves. Multiply on top of
  whatever R105 resolves; default 1.0."

git add src/probe.cpp
git commit -m "probe: read the published MV field's content and units ([R75]/[R76])

Read-only diagnostics: sample four 32x32 patches, report coverage and worst
magnitude, and print the learned scale ([R278c])."

git push -u origin field/fc6-20261022
```

If upstream moves, `git fetch upstream && git rebase upstream/main` on this branch; do not force-push a branch
that already has a PR open (it moves the review comments).

## Deciding what to send

| Situation | Action |
|---|---|
| The maintainer only wants the bug | Do not open a PR for 7.2–7.4; put the 7.1 repro and patch sketch in the issue and let them write the fix. |
| They want the guards | Open one PR containing **only the `gpu1_context.cpp` commit**; keep `probe.cpp` local. |
| They want it split | Use `git format-patch` and open one PR per item (R330 / R331 / R332), each independent. |

```powershell
gh pr create --repo <owner>/<repo> `
  --head <our-fork-owner>:field/fc6-20261022 `
  --title "MVec: hold the arm for a plausible source, reset on provider change, make the scale testable" `
  --body-file .\ISSUE.md
```

## Two rules we learned the hard way

1. **Never rename the branch while a PR is open** — GitHub treats it as delete + recreate and closes the PR;
2. **Never drag a folder into the web uploader** (it lands as `commit2/src/...`); use *Add file → Create new file*
   and type the full path into the filename box.
