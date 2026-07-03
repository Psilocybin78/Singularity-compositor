# Rebase notes

Per FORK_MAINTENANCE.md § Adopting a new upstream release, step 3:
every pin move that hits conflicts records them here, date + one-line
summary per decision, so "why did we touch this?" stays answerable.

## 2026-07-01: v0.54.3 → v0.55.4

- Used `git rebase --onto v0.55.4 v0.54.3 singularity` (plain
  `git rebase v0.55.4` tried to replay upstream 0.54.x release-branch
  commits because the two tags sit on different release lineages).
- DROPPED patch 0001 (keyboard + pointer leave off m_currentSurface,
  fork commits 58ad234b + 1739906d + the two archive-roll commits):
  merged upstream in this exact form as hyprwm/Hyprland#14143
  (upstream commit c8c66642, included in v0.55.x). Upstream carries
  both the keyboard and pointer halves.
- Patch 0002 (input-method-relay no-IME text-input enter) applied
  clean; upstream did not touch InputMethodRelay.cpp in 0.54.3..0.55.4.
- Patch 0003 (input-manager refocus on layer drift) applied clean
  despite heavy upstream InputManager.cpp churn. Note upstream #14018
  ("keep pointer focus on layer surfaces during keyboard refocus")
  works the other direction (pointer-on-layer during keyboard refocus)
  and does not obsolete 0003; re-verify R3/R4 behavior at smoke.
- Em-dash sweep commit: kept sweep hunks on fork-local files
  (FORK_MAINTENANCE.md, patches/, our patch comment in
  InputMethodRelay.cpp); dropped hunks on pure upstream files
  (src/i18n/Engine.cpp Arabic strings, src/xwayland/XWM.cpp log
  string, src/managers/SeatManager.cpp now-upstream comment) to avoid
  permanent cosmetic divergence and future rebase friction.
