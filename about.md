# Aceware

Input buffer for Geometry Dash.

Click a hair too early? Instead of dying, the click waits for a tick where it actually survives, then goes through. Clicks only ever move later, never earlier, and nothing is pressed for you.

Every decision is made live, while you play. Aceware does not remember your earlier attempts, so nothing from a previous run can bias the current one.

## Keys

| Key | What it does |
|-----|--------------|
| **Right Shift** | show / hide the window |
| **F6** | cycle stage |
| **F7** | buffer on / off, keeps the stage you were on |
| drag title bar | move the window |
| drag corner | resize the window |

## The window

Tabs in the sidebar:

- **Log** — what happened to each click: time, tag, text.
- **Stats** — prediction accuracy, your rhythm, window sizes.
- **Stage** — stage buttons, a line with every stage's frames as **cube / ship / wave**, options, Forget Level, Custom frames.
- **Modes** — one tile per gamemode; buffer on or off for each.
- **Assists** — orb helper, raw wave spam, hitboxes, show rays. A **Keys** card and a **Quick** card (Copy Report, Hide Window) sit on the right.

Accent and background presets live in the sidebar colour card.

## Stages

Frames are 60fps frames (1 frame = 16.7 ms).

| Stage | Cube | Ship | Wave |
|-------|------|------|------|
| Off | raw input | raw input | raw input |
| Stage 1 | 1 | 1 | 1 |
| Stage 2 | 2 | 2 | 2 |
| Stage 3 | 3 | 2 | 3 |
| Stage 4 | 5 | 2 | 4 |
| Extreme | 20 | 2 | 12 |
| Custom | yours | yours | yours |

## How it decides

- Each click is played forward on a hidden copy of your player: jumps, orbs, pads, portals, moving objects.
- Safe already? It goes through untouched.
- Deadly? More ghosts than before are tested: every tick across the first frame after the earliest safe one, then every second tick out to three frames. Every candidate has to read **Good**, and the one with the most room around it wins. Equally good candidates are picked at random.
- A **release** is never held past the moment you made it: it fires at its scheduled tick at the latest, and only needs one proven-safe tick to go early.
- Nothing survives? It goes through exactly as you clicked it.

## Notes

- No network: no server, no telemetry, no update checks.
- No learned spots, no per-level memory, no level scan.
- Windows, Geode 5.11.2, Geometry Dash 2.2081.
