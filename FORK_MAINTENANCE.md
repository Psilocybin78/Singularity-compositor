# Singularity Compositor Fork — Maintenance Guide

This directory is a git submodule pointing at
`Psilocybin78/Singularity-compositor`, a fork of `hyprwm/Hyprland`.
The `singularity` branch is what Singularity consumes; it lives off a
pinned upstream tag and holds every Singularity-specific patch.

The fork exists so upstream Hyprland changes can never silently break
the Singularity shell. New releases are adopted on our schedule, behind
a smoke check — not on Hyprland's.

**Bare-metal install right now is stock Hyprland from Arch repos.** This
fork is strategic infrastructure, not active runtime. When we have a
concrete reason to diverge (plugin ABI hardening, deeper world-surface
integration, animation hooks for agents) we start committing to
`singularity` and eventually ship `singularity-hyprland` alongside — or
replacing — the distro binary.

---

## Repo layout

```
apps/compositor/                           # this submodule
  ├── (upstream Hyprland tree)
  ├── FORK_MAINTENANCE.md                  # this file
  └── patches/                             # Singularity-specific patches
       ├── 0001-keyboard-and-pointer-leave-off-currentSurface.patch
       ├── 0002-input-method-relay-no-ime-text-input-enter.patch
       └── 0003-input-manager-refocus-on-layer-drift.patch
```

### Remotes

- **`origin`** → `git@github.com:Psilocybin78/Singularity-compositor.git`
- **`upstream`** → `https://github.com/hyprwm/Hyprland.git`

### Branches

- **`main`** — mirror of `hyprwm/Hyprland:main`. Never merge into this;
  GitHub keeps it synced automatically. Used as the target of
  rebase-onto when upgrading the pin.
- **`singularity`** — the branch every Singularity build consumes.
  Based on a tagged upstream commit (currently `v0.54.3`) with the
  Singularity-specific patches (cursor/seat focus delivery + text-input-v3
  enter/leave + layer-drift refocus) stacked on top.

### Current pin

| Field | Value |
|---|---|
| Upstream version | `v0.54.3` |
| Upstream commit | `521ece463c4a9d3d128670688a34756805a4328f` |
| Forked on | `2026-04-22` |
| Singularity patches | 3 (cursor/seat focus, text-input-v3 enter, layer-drift refocus) |

---

## Adopting a new upstream release

Run these when you want to move the pin from e.g. `v0.54.3` → `v0.55.0`:

```bash
cd apps/compositor

# 1. Fetch upstream tags
git fetch upstream --tags

# 2. Rebase singularity patches onto the new tag
git checkout singularity
git rebase v0.55.0

# 3. Resolve any conflicts (they'll be rare until we have our own patches).
#    When conflicts happen, document them in patches/REBASE_NOTES.md with
#    date + one-line summary so "why did we touch this?" is answerable.

# 4. Smoke check (see below) before pushing.

# 5. Push the rebased branch. Force is required because history rewrote;
#    protected branch rules on `singularity` should be disabled for us.
git push --force-with-lease origin singularity

# 6. Bump the submodule pointer in the main repo
cd ../..
git add apps/compositor
git commit -m "compositor: bump pin to Hyprland v0.55.0"
```

---

## Smoke checklist

Before pushing a rebase OR shipping a new singularity-hyprland build,
verify each item on the current bare-metal box:

```bash
# Build the fork
cd apps/compositor
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

| # | Check | How |
|---|---|---|
| 1 | Build succeeds clean | `cmake --build build` green |
| 2 | Binary runs `--version` | `./build/Hyprland --version` |
| 3 | Hyprbars plugin still loads | `hyprpm list` shows `hyprbars: loaded` after session |
| 4 | `hyprctl -j clients` / `hyprctl -j monitors` / `hyprctl -j workspaces` all return valid JSON | compare to previous pin |
| 5 | `zwlr_foreign_toplevel_manager_v1` advertised | `wayland-info` or `singularity-taskbar` connects |
| 6 | `zwlr_layer_shell_v1` advertised | `singularity-topbar` + `singularity-taskbar` attach |
| 7 | `singularity-desktop` paints the wallpaper | visual |
| 8 | `singularity-topbar` workspace pills react to `hyprctl dispatch workspace N` | visual |
| 9 | `singularity-taskbar` running-window pills update on window open/close | visual |
| 10 | `winctl maximize` respects `mon.reserved` | `hyprctl -j clients` after clicking green dot |

If any check fails: revert the pin (keep the old commit in the
submodule), file the upstream regression, and stay on the prior pin.

---

## Policy

1. **Don't modify this tree unless the change genuinely needs to live
   inside Hyprland.** Most Singularity shell work belongs in
   `apps/shell/`, `apps/topbar/`, `apps/taskbar/`, or `apps/bridge/`.
2. **Each Singularity patch is one focused commit on `singularity`.**
   Squashed, conventional-commit subject line, longer body describing
   *why* upstream won't want it (or tracking an upstream PR that would
   obviate it).
3. **Upstream-worthy fixes go as PRs to `hyprwm/Hyprland` FIRST.** Only
   land on `singularity` if upstream rejects or can't land fast enough.
4. **Never ship `singularity-hyprland` to the Arch install without a
   release report** in `docs/` and a rollback path (i.e. stock Hyprland
   still installable from pacman).

---

## Building and installing

Not wired up yet — we haven't diverged. When we start shipping this
build:

1. Produce `singularity-hyprland` binary (CMake rename target).
2. Ship via a `deploy/systemd/singularity-hyprland.service` user unit
   OR a systemd session preset, coexisting with stock Hyprland so
   the user can choose at login.
3. Document the swap in `docs/BARE_METAL_COMPOSITOR_SETUP.md` (new,
   mirroring `BARE_METAL_STORAGE_SETUP.md`).

Until then: `apps/compositor/` is a reference + safety net only.
