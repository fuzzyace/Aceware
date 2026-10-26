# Aceware — handoff

Written 2026-10-25 for picking this project up in another chat or with another model.
**Do not add this file to the public repo** (`Aceware/`) — it is workspace-only.

---

## 1. What this is

Aceware is a **Geode mod for Geometry Dash 2.2081 (Windows)** that buffers input: if you click
slightly early, the click is held and fired on the first tick where the hidden simulated player
survives. Mod id `ace.aceware`, developer `Ace`, current version **v1.3.1** (`mod.json` is the single
source of truth; the in-game badge is derived from it at runtime).

It started as a derivative of the upstream project *Nova / Input Buffer by AlQuerez23*, which is
**intentionally scrubbed from the tree** (name, strings, comments, header filenames, identifiers).
Do not reintroduce that name.

---

## 2. Folder map (workspace root = `C:\Users\ariel\Downloads\GDbuffer`)

```
GDbuffer\
├─ src\, resources\, mod.json, ...        upstream clone (READ-ONLY REFERENCE — untouched original)
│                                          useful as: pristine upstream main.cpp for diffs
├─ NovaPrime\                              WORKING BUILD WORKSPACE (where you compile)
│   ├─ build.ps1                           the build (uses ..\toolchain, not CMake)
│   ├─ package.ps1                         zips build\ace.aceware.dll into Aceware.geode
│   ├─ build\                              objects (obj\gen, obj\inline, obj\tools, obj\arc), DLL, logs
│   ├─ src\                                mirrored source — see §5
│   └─ Aceware.geode                       latest packaged artifact (build output)
├─ Aceware\                                PUBLIC REPO FOLDER (what gets pushed to GitHub)
│   ├─ README.md, CHANGELOG.md, logo.png, about.md, mod.json
│   ├─ CMakeLists.txt                       standard Geode build (needs a real Geode SDK)
│   ├─ .github\workflows\build.yml          CI: windows build, tag → release
│   ├─ src\                                 authoritative source (synced from here)
│   ├─ resources\fonts\                     Poppins Bold + OFL licence
│   ├─ tools\local-build.ps1                the offline build (expects ..\..\toolchain)
│   └─ release\Aceware-1.3.1.geode          shipped binary
└─ toolchain\                              self-assembled MSVC + SDK + Geode headers (NOT shipped)
    ├─ msvc\VC\Tools\MSVC\14.44.35207\      cl.exe, link.exe, CRT headers/libs
    ├─ sdk_nuget\microsoft.windows.sdk.cpp{,.x64}\   Windows SDK 10.0.26100
    ├─ dl\sdk-extract\geode-5.11.2\loader\include\   Geode public headers + link\win64\*.lib
    ├─ codegen-out\                         bindings generated for GD 2.2081 (CodeGen Win64)
    ├─ deps\{fmt,asp2,arc,json,result,nontype_functional,TulipHook}\   pinned deps
    └─ geode-lib\Geode.lib                  import lib from the Geode v5.11.2 release zip
```

**Source of truth:** `Aceware\src\`. `NovaPrime\src\` is a copy for the offline build. When you edit,
edit `Aceware\src`, then copy to `NovaPrime\src` and build (or edit both).

---

## 3. Build / package / release

```powershell
# build (incremental, ~20 s after the first full run which is ~25 min)
powershell -ExecutionPolicy Bypass -File C:\Users\ariel\Downloads\GDbuffer\NovaPrime\build.ps1

# package
powershell -ExecutionPolicy Bypass -File C:\Users\ariel\Downloads\GDbuffer\NovaPrime\package.ps1
# -> C:\Users\ariel\Downloads\GDbuffer\NovaPrime\Aceware.geode

# publish copy
copy NovaPrime\Aceware.geode Aceware\release\Aceware-<version>.geode
```

`build.ps1` does five passes:
1. **gen** — `src\main.cpp` + 941 generated binding sources.
2. **inline** — the 933 inline binding sources, in a **separate object folder** (the two sets share
   file names, so they must not share `\Fo`).
3. **tools** — `entry.cpp` from the Geode SDK (defines `geodeImplicitEntry` + `geode::getMod`), the
   arc/asp/fmt pieces the import library does not export, asp `Notify`/`Thread`.
4. **arc** — the arc runtime units the entry initialises.
5. **link** — all objects + `Geode.lib` + SDK `libcocos2d/libExtensions/fmod/glew32/gd-libcurl`
   + `opengl32 user32 gdi32 winmm shell32 advapi32 ws2_32 ole32 version`.

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

**No learned memory:** `Config::load()` forces `c.learnSpots = false`. All spot/attempt-memory paths are
gated on that flag, so they are dormant. There is no level scan UI and `ScanRunner` is never spawned
(the engine code still exists but is unreachable).

---

## 5. Where things are in `src\main.cpp` (~16.5k lines)

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
3. **No network.** Anything that phones home (telemetry, update check) stays removed.
4. **No learned spots, no level scan.**
5. **Everything stays inside the window.** Lists clip; no row may draw outside the panel.
6. **Branding is Aceware / Ace.** No old names, anywhere (including comments and identifiers).
7. Update `Aceware\CHANGELOG.md` and, when user-facing, `about.md` and `README.md`.
8. Rebuild, repackage, copy the `.geode` into `Aceware\release\`, and verify the export.

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
- **Two DLLs in `mods\`** — if an older `NovaPrime.geode` is still installed, both load and fight over
  the window key. Tell the user to delete it.
- **Toolchain fragility:** `toolchain\` is hand-assembled (MSVC packages via `vsdownload.py` patched for
  this sandbox, Windows SDK via NuGet, bindings via `CodeGen Win64`). A `vsdownload` re-run needs
  `--major 17` and the serial-pool patch. If it breaks, `Aceware\CMakeLists.txt` + a real Geode SDK is
  the fallback.
- **`build.ps1` links against `Geode.lib` from the release zip**, which lacks some loader symbols
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
- **Publishing**: GitHub repo is ready (`Aceware\`); needs `git init`, remote, push, then tag `v1.3.1`
  for CI to attach the binary. README/CHANGELOG are written.

---

## 9. Onboarding checklist for a new model

1. Read this file, then `Aceware\README.md` and `Aceware\CHANGELOG.md`.
2. Open `Aceware\src\main.cpp` at the anchors in §5; skim `BufferCore.hpp` for `searchDelay` and
   `liveStep`, those two govern everything.
3. Never trust line numbers in this document — they drift. Search by the §5 anchors.
4. To change behaviour: edit `Aceware\src`, copy to `NovaPrime\src`, build, package, verify export.
5. To test what you cannot test: add temporary `log::info` lines and ask the user for
   `Geometry Dash\geode\logs\Geode <date>.log`, or have them hit **Quick → Copy Report** in the
   Assists tab (it copies a full engine report to the clipboard).
6. Bump the version, update CHANGELOG, copy the artifact to `Aceware\release\`, and say plainly what
   was verified vs. only compiled.
