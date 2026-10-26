# Changelog

All notable changes to Aceware. Version numbers follow `mod.json`; the in-game badge shows the
short form (`v1.3` for `1.3.0`).

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
