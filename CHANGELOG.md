# Changelog

All notable changes to Aceware. Version numbers follow `mod.json`; the in-game badge shows the
short form (`v1.3` for `1.3.0`).

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
