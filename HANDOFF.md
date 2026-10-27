# Aceware — handoff

Written 2026-10-25 for picking this project up in another chat or with another model.
Reconciled 2026-10-26 against the real tree — §2, §3, §7, §8 and §9 had drifted after the workspace
was collapsed from an older multi-folder layout into this single folder.

> **Housekeeping, unresolved:** this file was meant to stay out of the public repo, but it is tracked
> and was pushed in the initial upload. Decide and act: `git rm --cached HANDOFF.md` plus a
> `.gitignore` entry (there is currently no `.gitignore` at all) to make it local again, or accept
> publishing it — it does expose internals and author notes.

---

## 1. What this is

Aceware is a **Geode mod for Geometry Dash 2.2081 (Windows)** that buffers input: if you click
slightly early, the click is held and fired on the first tick where the hidden simulated player
survives. Mod id `ace.aceware`, developer `Ace`, current version **v1.4.4** (`mod.json` is the single
source of truth; the in-game badge is derived from it at runtime).

It started as a derivative of an earlier project of the same kind, which is
**intentionally scrubbed from the tree** (name, strings, comments, header filenames, identifiers).
Do not reintroduce that name.

---

## 2. Folder map (workspace root = `C:\Users\ariel\Aceware`)

The old split — an untouched upstream clone + a separate build mirror + the public repo folder —
public folder — is **gone**. This folder is the working tree *and* the public repo, and it builds in
place. The pristine upstream clone is gone too, so diffs against upstream are no longer possible
locally; keep edits small and read them back.

```
Aceware\                                  repo root = workspace = build root
├─ src\                                    authoritative source; edit and build here
│   ├─ main.cpp                            the mod (~15.9k lines) — anchors in §5
│   ├─ BufferCore.hpp                      searchDelay, InputQueue, liveStep
│   ├─ AceTheme.hpp                        namespace aw: shapes, glows, CourseView
│   ├─ AceCourse.hpp                       course / trajectory helpers
│   ├─ OnlineURL.hpp                       empty URL; network paths are stubs
│   └─ ProjectDefs.hpp                     offline-build defines (not used by CMake)
├─ resources\fonts\                        Poppins-Bold.ttf + OFL.txt
├─ tools\local-build.ps1                   the offline build (expects ..\..\toolchain)
├─ .github\workflows\build.yml             CI: build + artifact upload (no release job)
├─ release\Aceware-1.4.4.geode             shipped binary
├─ CMakeLists.txt                          standard Geode build (the CI path)
├─ mod.json                                version single source of truth
├─ README.md, CHANGELOG.md, about.md, logo.png
├─ build\                                  made by local-build.ps1 (obj\, DLL, *.log) — absent for now
└─ HANDOFF.md                              this file — tracked in git, see the note at the top
```

`tools\local-build.ps1` derives the toolchain as the **sibling of the repo root** (`$np` = repo root,
`$tc` = its parent + `\toolchain`), so it belongs at `C:\Users\ariel\toolchain\` — not inside the repo,
and not under `Downloads\`. It is **not present on this machine**, so there is no local build until it
is restored (§7).

**Source of truth:** `src\` here. There is no mirror to keep in sync any more, and no `package.ps1`.

---

## 3. Build / package / release

Two paths. **A** is offline and currently unavailable; **B** is CI.

**A — local, offline** (needs the toolchain restored first):

```powershell
powershell -ExecutionPolicy Bypass -File C:\Users\ariel\Aceware\tools\local-build.ps1
# -> C:\Users\ariel\Aceware\build\ace.aceware.dll
```

Incremental after the first run: ~20 s, against ~25 min cold (it compiles 941 generated + 933 inline
binding sources).

`local-build.ps1` does five passes:
1. **gen** — `src\main.cpp` + 941 generated binding sources.
2. **inline** — the 933 inline binding sources, in a **separate object folder** (the two sets share
   file names, so they must not share `\Fo`).
3. **tools** — `entry.cpp` from the Geode SDK (defines `geodeImplicitEntry` + `geode::getMod`), the
   arc/asp/fmt pieces the import library does not export, asp `Notify`/`Thread`.
4. **arc** — the arc runtime units the entry initialises.
5. **link** — all objects + `Geode.lib` + SDK `libcocos2d/libExtensions/fmod/glew32/gd-libcurl`
   + `opengl32 user32 gdi32 winmm shell32 advapi32 ws2_32 ole32 version`.

**Packaging.** There is no `package.ps1` in this tree. A `.geode` is just a zip whose root holds the
DLL, the manifest, `about.md` and `resources\` — exactly the five entries inside
`release\Aceware-1.4.4.geode`:

```
about.md    ace.aceware.dll    mod.json    resources\fonts\OFL.txt    resources\fonts\Poppins-Bold.ttf
```

Verified recipe — stage those four items in an empty folder, then from that folder:

```powershell
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory(
    (Get-Location).Path,
    'C:\Users\ariel\Aceware\release\Aceware-<version>.geode',
    [IO.Compression.CompressionLevel]::Optimal, $false)
```

`$false` = no base directory, so the entries sit at the archive root. This writes `resources\fonts\...`
with backslashes, which is what every artifact shipped so far uses. It **throws** if the target file
already exists — delete the old `.geode` first (that quirk is why the recipe is written out here).

**B — CI.** `.github\workflows\build.yml` runs `geode-sdk/build-geode-mod@main` with `combine: true` on
`windows-latest` for every push, then uploads an `Aceware` artifact. It has **no tag → release job**, so
pushing a tag does not publish a binary (see §8).

Specific build quirks already solved (do not "clean these up"):
- Three generated files need a forced include to resolve their own types:
  `BoomListView.cpp` + `CCIndexPath.hpp`, `EnterEffectInstance.cpp` + `EnterEffectAnimValue.hpp`,
  `GJUINode.cpp` + `UIButtonConfig.hpp`.
- `DialogLayer.cpp`, `GameStatsManager.cpp` need `/FI"sys/timeb.h"` (they use `_ftime64`).
- The old MSVC `Geode\cocos\platform\win32\compat` folder must be **excluded** from includes (it
  shadows the modern `stdint.h` and breaks the STL).
- `ProjectDefs.hpp` supplies `GEODE_MOD_ID`/`GEODE_COMP_GD_VERSION` for this offline build only.
  In the CMake/CI path `setup_geode_mod` supplies them.

**Verification after every build:**
```powershell
dumpbin /exports build\ace.aceware.dll | findstr geodeImplicitEntry   # must print one export
dumpbin /dependents build\ace.aceware.dll                              # Geode.dll + libcocos2d.dll
```
A DLL with no `geodeImplicitEntry` will be silently ignored by Geode.

---

## 4. How the engine works (mental model)

Entry point is `TrajectoryBufferSim` (§7 of `main.cpp`). Per input:

1. **Hook** `GJBaseGameLayer`/`PlayLayer`/`PlayerObject` call into the sim on a press or release.
2. `push` = press, `!push` = release. Releases only matter in hold-based modes.
3. **Ctx / makeCtx** builds a snapshot: player state, mode flags (`ModeInfo`: cube/ship/wave/ufo/
   robot/spider/swing/ball, mini), and the sim horizon.
4. **runSim(delay)** clones the player into a hidden simulation and steps it `delay` ticks without the
   input, then applies the input and steps forward through the horizon.
5. **verdictFor(delay, result)** → `Good` / `Partial` / `Bad` / `Budget`. `Budget` = ran out of the
   per-click CPU allowance (default ~1.8–6 ms, scaled to frame time; `Config::budgetUs`).
6. **searchDelay** (`BufferCore.hpp`) scans delays: dense near 0, then a stride, then bisection, with a
   CBF guard (the tick after must also be safe). Returns the earliest `Good` delay.
7. **8b-pick (Aceware)** then tests extra candidates: every tick across the first frame after the
   earliest safe tick, then every second tick out to three frames (releases: one tick). Each must be
   `Good`; the winner is the one with the largest `SimResult::clear` (px clearance), ties random.
8. If the click is safe as made → passed through untouched. If a delay is chosen → the input goes into
   the **input queue** (`InputQueue`) and is fired by **liveStep** (`BufferCore.hpp`) which re-checks
   each tick:
   - `latest` = hard "fire by" time,
   - `need()` = consecutive good ticks required (**releases: 1**, presses: 2 when safety margin on),
   - a queued **release** gets `latest = planned + eps`, so it can never be held past your timing,
   - if it never fires, the deadline fires it as made.
9. **Extreme** widens the window via `windowTicks()` — but **presses only**; releases are capped at
   8 ticks (`if (!push && c.extreme) maxDelay = min(maxDelay, scaleTicks(8, s))`).
10. Logs: `ClickNote` + `pushLog` feed the window and the console log.

**Session memory (v1.3.5):** `Config::load()` reads the saved value `session-memory` (default true,
Assists tab toggle) into `c.sessionMemory`, and `c.learnSpots` follows it. Every press that survives
is recorded by position / vertical speed / mode / gravity (`rememberInput` → `OutcomeTracker` →
`SpotMemory`), and later attempts fire on those spots: `liveStep` returns `Proven` on a good spot and
`Unsafe` on a bad one, while `interceptInput` may hold a click for a remembered spot ahead. The memory
is **memory only** — `loadSpots()` clears it when a level opens, `saveSpots()` writes nothing, so the
old `spots/*.txt` files are gone. Ship and wave only use the `Proven` half (the hold paths would move
their line). Everything else about attempt-memory (the learned margin) stays off behind
`Config::learnedMargin`.

**No level scan:** there is no scan UI and `ScanRunner` is never spawned (the engine code still exists
but is unreachable), same for the autoplay/route subsystem — `setAutoplay()` has no call site.

---

## 5. Where things are in `src\main.cpp` (~15.9k lines)

| Section | Anchor | What lives there |
|---|---|---|
| 1 Settings | `//  1. Settings` (~1255) | `StageDef STAGES[]`, `Config::load()`, `baseTicks/scaleTicks` |
| 2 Gamemode | `//  2. Gamemode profile` | `ModeInfo`, `modeOn()`, `MODE_TOGGLES[]` |
| 3 Classification | `//  3. Object classification` | hazard detection, prediction cut-offs |
| 4 Decisions | `//  4. Decisions...` | `Decision` enum, `ClickNote`, stats types |
| 5 CSV | `//  5. CSV log` | optional per-click CSV |
| 6 Colours | `//  6. Colors...` | log tag colours |
| 7 Engine | `//  7. TrajectoryBufferSim` (~2167) | `decide()`, `runSim`, `verdictFor`, search + picker, queue, `liveStep` driving, all hooks' logic |
| 9 Window | `//  9. The window` (~12639) | `BufferWindow` (CCLayer), palette `K_*`/`C_*`, `aa::` draw helpers, `applyPalette`, `buildTabs/showTab/buildSettings/buildModes/buildTools/buildLog`, `card()`, `rowToggle`, animations in `update()` |
| 8 Hooks | `//  8. Geode hooks` (~16115) | `$on_mod(Loaded)`, `BufPlayLayer`, `BufPlayerObject`, `BufBaseGameLayer`, `BufKeyboard` (F7/window key), `BufMouse`, `BufPauseLayer` (the green button), `AceOnlineDelegate`, `AceAutoLevel` |

Helpers:
- `src\BufferCore.hpp` — pure logic: `searchDelay`, `InputQueue`, `LiveState`/`liveStep`, `Clearance`,
  `SpotMemory` (dormant), `Point2`.
- `src\AceTheme.hpp` — namespace `aw` (renamed from `nv`): smooth shapes, `roundBox`, glows, starfield,
  `CourseView`, bloom, theme plumbing. **Has its own `themed()` layer; the theme is forced to index 0
  now (presets removed).**
- `src\AceCourse.hpp` — course/trajectory helpers used by the engine.
- `src\OnlineURL.hpp` — empty URL; network paths are stubs.
- `src\ProjectDefs.hpp` — offline-build defines (not used by CMake).

UI palette lives in `BufferWindow` as `K_*` constants, mirrored into `C_*` by `applyPalette()`:
frame `{15,16,20}`, sidebar `{18,20,25}`, card `{23,25,31}`, button `{32,36,42}`, accent mint
`{95,211,160}`, muted `{150,158,170}`, dim `{108,116,130}`. Hover is neutral white at opacity 46.

---

## 6. Conventions (non-negotiable, requested by the author)

1. **Bump the version on every edit.** `mod.json` → `vX.Y.Z`. Patch = fix, minor = feature/look.
   The badge follows automatically (`titleBadge()` strips a leading `v` and trims the patch).
2. **One accent colour.** Mint `{95,211,160}`. No violet, no per-tab colours, no presets.
3. **No telemetry.** Nothing reports on you. The one network call is the auto-update (1.4.4): it asks
   GitHub for this repo's latest release and writes it over the installed `.geode`. Setting
   **Auto Update** turns it off. Do not add anything else that phones home.
4. **No level scan.** (Session memory exists since 1.3.5: clicks that worked are reused until you leave
   the level; it is in RAM only and toggleable in Assists.)
5. **Everything stays inside the window.** Lists clip; no row may draw outside the panel.
6. **Branding is Aceware / Ace.** No old names, anywhere (including comments and identifiers).
7. Update `CHANGELOG.md` and, when user-facing, `about.md` and `README.md`.
8. Rebuild with `tools\local-build.ps1`, repackage into `release\Aceware-<version>.geode`, and verify
   the export. This folder is the repo — there is no publish-copy step any more.

---

## 7. Known issues / risks

- **Never run in game from this machine.** Geometry Dash cannot create a window in this sandbox
  (`GLFW Error 65544`), so every release is verified statically only (compiles, links, exports,
  manifest, string scans). **Anything runtime is unverified.**
- **The hook section was rebuilt once.** During the name scrub, ~450 lines of `main.cpp` were truncated
  and restored by splicing the upstream tail and re-applying three edits (migration removal, scan
  spawn removal, hook renames). A diff against upstream showed only those diffs, but that region is
  historically the highest-risk area. If something odd happens at mod load, look there first.
- **Release behaviour has been "fixed" twice and never observed in game.** Three mechanisms are in play
  (press-only Extreme widening, 8-tick release cap, `latest = planned` for queued releases, `need()==1`
  for releases). If a release still hangs, instrument `liveStep` with a log line of
  `(now, st.earliest, st.planned, st.latest, st.deadline, st.good)`.
- **Prediction picker cost.** Up to ~9 sims per press. The budget can run out (`Verdict::Budget`), in
  which case it falls back to the plan or the raw click. Watch the log for budget falls on heavy levels.
- **Pause button texture** was replaced with drawn shapes (no PNG) because the original path showed a
  missing texture. That fix is also unverified visually.
- **mod id changes reset saved data** (settings keyed per id). Do not change `ace.aceware`.
- **Two DLLs in `mods\`** — if a `.geode` from before the rename is still installed, both load and
  fight over the window key. Tell the user to delete it.
- **Toolchain fragility:** the toolchain is hand-assembled (MSVC packages via `vsdownload.py` patched
  for this sandbox, Windows SDK via NuGet, bindings via `CodeGen Win64`). A `vsdownload` re-run needs
  `--major 17` and the serial-pool patch. If it breaks, `CMakeLists.txt` + a real Geode SDK is the
  fallback (that is also the only path available right now — see the next bullet).
- **Building without a toolchain in the repo.** This folder has no compiler and no `build\`. A build
  was done on 2026-10-26 by invoking MSVC from the old toolchain directly: `cl.exe @inc.rsp` on
  `src\main.cpp`, then `link.exe` with the pre-built binding/arc/tool objects, `Geode.lib` and the
  SDK import libs, then zipping the DLL with `mod.json`, `about.md` and `resources\fonts\`. It
  produced a 1,214,464-byte DLL exporting `geodeImplicitEntry` and depending on `Geode.dll` +
  `libcocos2d.dll`. Otherwise the only build is CI (`.github\workflows\build.yml`).
- **`local-build.ps1` links against `Geode.lib` from the release zip**, which lacks some loader symbols
  (`arc::*`, `geode::getMod`). That is why `entry.cpp` + arc/asp objects are compiled in locally.
  Do not remove them.

---

## 8. Future goals (from the author, in his words where possible)

- **Prediction accuracy**: "make the ghost player find the correct path… make more of them for each
  frame if you have to." Direction so far: denser candidates per tick + clearance ranking. Next steps
  could be: per-tick candidates across the whole first two frames, a short second lookahead pass to
  confirm the chosen delay survives beyond the horizon, and ranking by clearance *slope* rather than
  absolute clearance.
- **Release reliability**: keep watching for holds that outlive the plan; consider a hard rule that a
  release never waits more than N ticks past `planned`, plus a log reason code when it does.
- **UI polish**: more tasteful animation, still one accent, still entirely inside the window. The
  author likes: breathing/pulsing idle motion, staggered row reveals, springy toggles.
- **Quality of life**: he keeps asking for "useful stuff in the menu". Ideas not yet done — a
  per-session buffered/saved counter in the footer, a "Reset settings" action, a log filter by tag,
  an on-screen mini-indicator of the current stage while playing.
- **Publishing**: the repo is already live — `origin` = `https://github.com/fuzzyace/Aceware.git`,
  three commits pushed (`initial upload` + the two workflow commits). What is actually left: **no tag
  exists and no release has been cut.** Also, the workflow has no tag → release job (it only uploads
  an artifact), so pushing tag `v1.3.1` will not attach the binary — either add a release step to
  `build.yml` or create the GitHub Release by hand and upload `release\Aceware-<version>.geode`.
  README/CHANGELOG are written.

---

## 9. Onboarding checklist for a new model

1. Read this file, then `README.md` and `CHANGELOG.md` in the same folder.
2. Open `src\main.cpp` at the anchors in §5; skim `BufferCore.hpp` for `searchDelay` and
   `liveStep`, those two govern everything.
3. Never trust line numbers in this document — they drift. Search by the §5 anchors.
4. To change behaviour: edit `src\`, rebuild with `tools\local-build.ps1` (needs the toolchain, §7),
   package, verify export. The tree is the repo — no mirror to sync, no publish copy.
5. To test what you cannot test: add temporary `log::info` lines and ask the user for
   `Geometry Dash\geode\logs\Geode <date>.log`, or have them hit **Quick → Copy Report** in the
   Assists tab (it copies a full engine report to the clipboard).
6. Bump the version, update CHANGELOG, refresh `release\Aceware-<version>.geode`, and say plainly what
   was verified vs. only compiled.
