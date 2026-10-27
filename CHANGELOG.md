# Changelog

All notable changes to Aceware. Version numbers follow `mod.json`; the in-game badge shows the
short form (`v1.3` for `1.3.0`).

## 1.4.3

- An update now ends in a prompt: Aceware updated / Aceware <ver> is installed. Restart the game to
  use it? with **Later** and **Restart**. Restart calls the game's own restart, so the new build loads
  immediately instead of waiting for the next launch.
- The check runs when the main menu appears, i.e. on opening the game; if the download finishes before there
  is a scene to put the prompt on, the menu asks as soon as it exists. It never pops over a level - there
  you get a notification and the prompt waits.

## 1.4.2

- Extreme is marked **(Buggy)** everywhere you meet it: the Stage tab button reads "Extreme (buggy)",
  the line under the stage buttons says "EXTREME: 20 frames (12 wave, 2 ship) - BUGGY", the in-game
  status pill reads "EXTREME (BUGGY)", the F6 notification carries the warning, and the stage
  setting's description says it too. It is still selectable - the label is a warning, not a lock.
## 1.4.1

Naming sweep: the only names anywhere in the repo are Aceware and Ace.

- the status banner reads `ACEWARE  STAGE 2`, and the window strips the matching 9-character prefix
- Geode log lines use the `[Aceware]` tag; the pause-menu button's node ID is `aceware-button`
- the bundled font resource is `aceFont`, and the source header, README, `about.md`, `mod.json`'s
  description and this file no longer reference the earlier project at all
- internal identifiers follow the same rule: the namespaces, the label class, the snapshot macros and
  the saved-run header all carry Aceware's name now

## 1.4.0

Auto-update.

- On the main menu, once a session, Aceware asks GitHub for `fuzzyace/Aceware`'s latest release. If its
  tag is newer than the installed version, it downloads that release's `.geode` and writes it over the
  installed one, then tells you to restart. No buttons to press.
- New setting **Auto Update** (on by default). Turn it off and the mod makes no network requests at all.
- The check used to point at a different project's repository, so its download could have replaced
  Aceware with another mod; it also read a `check-updates` key that `mod.json` never declared. Both
  fixed.
- The download's UI work now happens on the main thread; before, notifications and the window were
  touched from the web worker that fetched the file.
- Tags are parsed with their leading `v` stripped (`v1.4.0`), so version comparison actually works.

## 1.3.6

- **Raw Wave Spam now only takes over above 7 clicks/s.** Below that the wave is corrected like every
  other mode, so a normal wave rhythm still gets its clicks fixed. The rate is measured from your last
  several presses (over the last second, needing at least four to judge), and resets each attempt, so
  the first taps of a burst are still corrected before it goes raw.
- The Assists description for the toggle says so: "Above 7 clicks/s in wave: goes in exactly as you
  click".

## 1.3.5

Session memory — the mod now remembers the clicks that worked, for that level, until you leave it.

- Every press that gets you through is remembered by where it landed (position, vertical speed,
  gamemode, gravity). On later attempts a click at the same spot fires straight away instead of being
  re-decided, and a spot that killed you before is held and re-planned rather than repeated.
- It is memory, not a file: nothing is written to disk, and opening the level again starts empty. The
  old per-level `spots/*.txt` files are no longer read or written.
- Assists tab has a **Session Memory** toggle (on by default). Turning it off forgets the level on the
  spot.
- Ship and wave only use the "fire on a remembered spot" half: the other two halves hold the input,
  and holding changes the line in those modes, so they skip them.
- Recording now covers every gamemode, not just mini cube, because the memory can't outlive the level
  and go stale.

## 1.3.4

- The corner resize mark is now invisible until you actually drag the corner. It was drawn at rest
  with a 9-slice sprite smaller than the sprite's own corner caps, which renders as a thick diagonal
  bar rather than three thin strokes. It is drawn as lines now and only while dragging.

## 1.3.3

- The grey line next to the settings cog is gone. It was the resize handle: three 45° strokes drawn
  in the bottom-right corner, which is also where the cog sits, so they crossed the cog and ran up
  and out of the window. The cog has moved one slot left and the resize corner owns the last 11 px,
  so the handle no longer touches it.
- The corner is actually draggable again. The resize hot zone started 16 px from the edge, which put
  almost all of it underneath the cog — clicking there opened the settings instead of resizing. It
  now starts past the cog.

## 1.3.2

Fixes for three things that were wrong in the shipped build:

- the window no longer looks washed out. It was reading a Window Opacity setting that `mod.json`
  never declared; the missing key resolved to 0, so every panel, glow and the starfield were drawn
  at 30 % alpha. The setting now exists (default 1.0, range 0.3 – 1.0).
- resizing the window by its corner now survives a rebuild. The drag used to save the live scale
  into Window Size, which is a multiplier, so the window came back at half the size you dragged it
  to. The drag works in the same units as the setting now.
- an on-schedule ship or robot release no longer logs "window ended, nothing safe found" in orange.
  Those inputs have no window at all, so they fire on their own tick by design; the log says
  "on schedule" and stays green.
- the direct-input fallback (used when another input mod re-times your clicks) is re-tried at the
  start of each level instead of staying latched for the rest of the session.

## 1.3.1

- Source tree cleaned up: no leftover project names in strings, comments, header names or
  identifiers. the two header files are now `AceTheme.hpp` / `AceCourse.hpp`, the
  helper namespace is `aw`, and the remaining internal names are Aceware's.
- README credits simplified to the current author.

## 1.3.0

Animation pass — eleven new motions, all drawn on existing nodes:

- the version badge under the wordmark breathes
- a light sweeps the accent line every 7.5 s
- resize strokes pulse while dragging the corner
- scrollbar brightens while scrolling, settles after 1.4 s
- the page fades in when the window opens
- the page title pops on every tab change
- list rows stagger in on a tab change
- the selected stage button pops when the Stage page opens
- toggle knobs squash and spring when flipped
- the pause-menu button breathes
- the selected tab highlight breathes

## 1.2.1

- Hover no longer turns purple: `K_HOVER` was violet at opacity 130, now a neutral white lift at 46.
- Accent / background presets removed; saved theme values are ignored and reset, so the fixed
  palette is the only look.

## 1.2.0

- New palette: cool charcoal with a single mint accent, matching the pause-menu button.
- Extreme is deep mint instead of hot pink.
- Rounder cards (7 px outline, 6.2 px fill), quieter outlines, softer hover glow.
- Neutral hairlines for rules and stripes; mint scroll thumb.
- Wordmark glows mint instead of violet.

## 1.1.1

- **Releases fixed** (three causes): queued releases had no fallback deadline, so a refused check
  held the button down; the live check demanded two safe ticks in a row, impossible in a one-tick
  release window; the release picker looked two ticks past your timing.
- Badge no longer renders as `vv1.1`.
- The level-scan card is fully removed from the Assists tab.
- More ghosts per click: every tick across the first frame, then every second tick to three frames.

## 1.1.0

- Prediction picker: several safe ticks tested, the one with the most clearance wins, ties random.
- Assists tab renamed and rebuilt; level-scan UI removed; scan runner no longer spawned.
- F7 toggles buffering without changing the stage.
- Copy Report and Hide Window actions.
- Log: removed the Discord/author line; the rest of the first-run text kept.
- Logo: tighter letters, version centred below the wordmark.

## 1.0.0

- First Aceware release: rebranded from the upstream project, own id (`ace.aceware`), version badge,
  taskbar-safe name.
- No network: online counter, telemetry and update checks removed.
- Tabs: Log / Stats / Stage / Modes / Assists.
- Forget Level clears the log; nothing is remembered per level.
- Resizable from the window corner; toggle key Right Shift.
- Pause-menu button drawn (mint disc + dot) instead of loading a PNG.
