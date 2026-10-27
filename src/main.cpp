// ============================================================================
//  Aceware                                 (Geode mod for Geometry Dash)
//
//  WHAT THIS MOD IS
//  ----------------
//  Aceware holds an early click back and fires it on the first tick that survives.
//
//  In vanilla GD a click that lands a hair too early or too late is simply a
//  death. This mod sits between your input and the game. For every click it
//  asks one question:
//
//      "If I let this click through right now, does the player die?"
//
//  It answers by simulating the next few hundred physics ticks on hidden copies
//  of the player (the "sim players"). If the click is safe it passes through
//  untouched. If it would kill you, the click is HELD and re-fired after the
//  smallest number of physics ticks that survives, as long as that delay fits
//  inside the window you configured. Nothing is ever "pressed for you": a
//  click only ever moves later, never earlier, and never out of thin air.
//
//  HOW THE FILE IS LAID OUT
//  ------------------------
//    1. Settings, presets and the F6 cycle stages
//    2. Gamemode profile (cube / ship / wave / ... incl. mini)
//    3. Object classification + the "prediction cut-off" geometry
//         (what makes the lookahead stop at an orb / pad / portal)
//    4. Decisions, death reasons, statistics
//    5. CSV log
//    6. Shared colors
//    7. TrajectoryBufferSim  - the engine (simulate / search / buffer queue)
//    8. Geode hooks
//    9. The window (BufferWindow)
//
//  NEW IN 1.3
//  ----------
//    * Top-right live log panel (F7): mode, TPS, window, every decision,
//      every death with its reason, and exactly WHICH object cut a prediction.
//    * Fixed false "orb/portal - raw input": objects BEHIND the player (the
//      portal you just flew through) no longer stop the lookahead. Pads and
//      portals only count if they have not been touched yet; orbs only count
//      while they can actually be clicked. Hitboxes come from the objects.
//    * Smallest-delay search is now exact at any TPS (stride + bisection).
//    * A candidate that survives until an orb/pad/portal now counts as a fix
//      instead of being thrown away.
//    * Hold length is preserved: a release queued behind a buffered press is
//      delayed by the same amount (matters for ship / wave / robot).
//    * Tick length is taken from the recent maximum, so Click-Between-Frames
//      sub-steps no longer skew the delay.
//    * EXTREME mode (F8): wide window (default 50 ticks @240) for every mode,
//      aims for the middle of the safe window instead of its edge.
//    * Settings are cached instead of re-read on every click.
//
//  NEW IN 1.3.1 (high-FPS / Click Between Frames setups, e.g. 170fps + CBF)
//  ------------------------------------------------------------------------
//    * Held clicks are scheduled in GAME TIME, not in update calls. CBF adds
//      short extra physics steps when a click lands mid-frame; counting calls
//      made buffered clicks, the spam guard and death timings run fast.
//    * CBF timing guard: a held click can only be re-fired on a step boundary,
//      so the real fire lands 0-1 tick after the chosen delay. A delay is only
//      used if the tick after it is safe too.
//    * Auto CPU budget: the lookahead may use at most ~30% of your measured
//      frame time (50% in extreme), so a click never costs a visible frame.
//      170fps -> ~1.8 ms. The old fixed 3 ms (6 ms extreme) was half to a full
//      170fps frame.
//    * Sim steps now use the level's time warp, matching the real physics.
//    * Panel shows fps, whether CBF is loaded and the current CPU budget.
//
//  NEW IN 1.4.0 (accuracy / latency pass; scheduler + search in BufferCore.hpp)
//  ------------------------------------------------------------------------
//    * No dropped taps: every input that arrives while one is held is replayed
//      in order with the same shift (old code kept only one queued release and
//      lost a double tap). A release made after the delayed press fired also
//      moves with it, so hold length is kept.
//    * Clean sim button state: each test starts from the game's real button
//      state with a fresh press edge (tests used to inherit the last test's).
//      Also syncs position, y-velocity, gravity, speed and used orbs.
//    * Fewer false alarms in ship / wave / robot / UFO / swing: a death more
//      than 100 ms after the click only counts if no follow-up input could
//      avoid it, so clicks you would have steered out of are left alone.
//    * Delays are only used when the delayed path is actually safe (UFO/swing
//      used to delay clicks whose delayed version still died, just later).
//    * Velocity orbs (yellow / pink / red / black) are modelled when the click
//      reaches one within 125 ms, so early orb clicks get buffered instead of
//      passing raw. Orb side effects are blocked and restored during tests.
//    * Each delay is simulated at most once per click (cached search).
//    * Extreme: earliest safe delay + margin by default (was mid-window, adding
//      ~7 ticks to every save), no spam guard, wider default windows.
//    * flipGravity hook moved to GJBaseGameLayer, the class that declares it.
//
//  NEW IN 1.5.0
//  ------------------------------------------------------------------------
//    * Live accuracy: every prediction is checked against the real player
//      100 ms after the click lands; hit rate in the panel and session log.
//    * Death replay: a click no delay in the window could save is probed
//      further, so the death line says how early it really was.
//    * (Timing coach - removed again in 1.7.0.)
//    * Auto windows: per-mode windows fitted to your measured timing error,
//      kept across sessions.
//    * Prediction through pads (strength learned from your real hits), speed
//      portals, gravity portals and blue / green orbs.
//    * Trajectory overlay, practice-only mode and an on-screen badge.
//
//  NEW IN 1.6.0
//  ------------------------------------------------------------------------
//    * Menu with Log / Stats / Settings tabs (replaced by the window in 1.7.0).
//      The Log keeps the last 300 lines even with every other log off.
//
//  NEW IN 1.7.0
//  ------------------------------------------------------------------------
//    * Draggable live window (section 9) replaces the F7 panel, the F9 coach
//      page and the popup menu. (1.8.1: Left Shift shows / hides it; no pause button.)
//    * Cube / ball / spider: the prediction lets go after your measured tap
//      length (holding re-jumps on landing in 2.2; holding forever predicted
//      bunny-hops and caused false saves).
//    * Ship / wave: "could you still save it" also tries a full release +
//      re-press cycle at your measured rhythm.
//    * Stats: accuracy per mode (cube, mini cube, ship, wave, ...) and your rhythm.
//
//  NEW IN 1.8.0
//  ------------------------------------------------------------------------
//    * One "Stage" setting: Off, Stage 1-4, Extreme. F6 cycles it, the window's
//      Stage tab sets it. Everything else is fixed defaults. (F6 used to guess
//      the current stage by comparing 8 window sliders and fell back to Stage 1
//      as soon as anything differed - "stages sometimes not working".)
//    * Search checks every tick in the first 12 ticks after a click before
//      striding: a coarse stride stepped over narrow safe gaps (Extreme missed
//      9 of 12 one-tick gaps in tests).
//    * Every jump input counts: mouse, keyboard, remapped keys, whichever
//      player flag GD delivers them with (player-2-flagged inputs used to pass
//      raw). Key auto-repeat / a second held key is never a new click.
//    * "Could you still save it" checks stop just past the original death:
//      far cheaper, so dense levels don't run out of CPU time and go raw.
//    * Orbs count a quick tap GD hasn't used up yet (its jump buffer).
//
//  NEW IN 1.9.0 (Extreme)
//  ------------------------------------------------------------------------
//    * Live re-planning (BufferCore liveStep): a held input is re-tested every
//      tick from the REAL game state and fires at the first safe tick, instead
//      of a delay predicted at click time (which drifts over long delays).
//      Releases and every input queued behind a held one get the same check.
//    * Cube / ball / spider / robot landing rule: a click is judged on getting
//      back to the ground; a death after landing is the next jump's job (dense
//      levels made clicks look doomed and pass raw).
//    * Extreme ship / wave window 6 -> 10 frames.
//    * Death counted once per death (practice mode / noclip called it every tick
//      and reset the buffer); deaths blamed on the press, not a cube release.
//    * Window key: Left Shift. Pause-screen button removed. Window half size.
//
//  NEW IN 1.9.1
//  ------------------------------------------------------------------------
//    * A buffered input is checked to have reached the player on the tick the
//      prediction chose. Input mods (Click Between Frames) can hold back or
//      re-time inputs they didn't create; if that happens the input is applied
//      to the player directly (and from then on always directly).
//    * "Show paths" (Stage tab): your click as made (red if deadly) and the
//      buffered one (green), redrawn from the real state when it fires.
//
//  NEW IN 1.10.0
//  ------------------------------------------------------------------------
//    * Wider windows: Stage 1 1f, 2 2f, 3 4f (3 ship/wave), 4 8f (5),
//      Extreme 20f (12).
//    * Show paths is live: the path of your next input right now (press, or
//      release while holding), with the player hitbox at both ends; green safe,
//      red deadly, yellow cut short. While a click is held, the path being
//      tested for it.
//
//  NEW IN 1.10.1
//  ------------------------------------------------------------------------
//    * The lookahead is never shortened to save CPU time any more. It used to
//      shrink to ~1/3 after budget overruns, so predictions couldn't see the
//      next spike: "safe" clicks died ~250 ms later and buffered clicks were
//      planned on too short a view. Out of time now = only that click is raw.
//    * More CPU time per click (half a frame, 3/4 in Extreme, up to 8 ms);
//      lighter "how early" probe; the path preview gets a third of the budget.
//
//  NEW IN 1.10.2
//  ------------------------------------------------------------------------
//    * Cube live check uses your REAL hold length (queued release, or how long
//      you've held so far) instead of the typical tap: holding through a landing
//      re-jumps in 2.2, which the typical-tap guess missed ("buffered still died").
//    * Log: too-late clicks say so ("too late: dies in 5t, no delay helps");
//      buffered / raw / death lines show the gamemode.
//
//  NEW IN 1.11.0  (accuracy)
//  ------------------------------------------------------------------------
//    * Clearance: every test also measures how close the path comes to a
//      hazard. A "safe" path that misses a spike by 0.3px is a coin flip in the
//      real game (the sim is off by ~1-2px). While a click is held, the live
//      check now waits up to 4 base ticks for a moment that clears the hazard by
//      the room this gamemode needs (1.25 x its measured prediction error,
//      1-4px); if none is coming it fires at the close one as before.
//    * Moving hazards: hazards near you are sampled every physics step; ones
//      that move (move / rotate triggers) are tested where they WILL be at each
//      sim step instead of frozen where they are now. Only visible,
//      axis-aligned, enabled hazards; anything else keeps GD's own test.
//    * Custom stage: your own frames for cube-type modes, ship and wave, set in
//      the window with Apply. Uses Extreme's engine. In the F6 cycle.
//
//  NEW IN 1.12.0  (learn from attempts)
//  ------------------------------------------------------------------------
//    * Spot memory: every input that reaches the game is remembered by where
//      it landed (position, vertical speed, gamemode, press / release,
//      gravity). Alive a moment later (100 ms in ship / wave / UFO / swing,
//      landed again in cube-type modes) = good spot; died first = bad spot.
//    * Next attempts: a held input fires the moment it reaches a good spot
//      (and waits for one coming up, if waiting is safe), and never fires on a
//      bad spot, whatever the prediction says. A click on a bad spot that the
//      sim calls safe is held and re-planned; a "no fix" click with a good spot
//      ahead in the window is held for it. Kept for the level, all attempts.
//      Stage tab: "Learn from attempts" (on by default).
//
//  NEW IN 1.12.1  (fixes)
//  ------------------------------------------------------------------------
//    * Out of CPU time on a deadly click: held and re-planned live (one cheap
//      test per tick) instead of going through raw ("raw: CPU budget" deaths).
//    * Releases: inputs queued behind your click only get the plain safety
//      check (no near-miss / learned-spot waits), so hold lengths stay yours.
//      Ship / wave / robot checks use your real release (or next press) when
//      it's already known, instead of "held forever".
//    * Cube: a learned spot in the air never fires a press (it wouldn't jump).
//    * Log: every held click says when it really fired and why (safe, learned
//      spot, window ended...); deaths say if it hadn't fired yet.
//    * Window: lives on Geode's overlay (menus, level select, levels), and the
//      Window Key is read straight from the keyboard (Left Shift works alone).
//
//  NEW IN 1.12.2
//  ------------------------------------------------------------------------
//    * Window Key on Windows is polled every frame (only while GD is focused),
//      because input mods like Click Between Frames take the keyboard events.
//    * Closing the window with X says how to bring it back.
//
//  NEW IN 1.12.3
//  ------------------------------------------------------------------------
//    * Window Key setting has a new id ("window-key-v3", default Left Alt):
//      older installs had kept F10 from an early version. Window switched on
//      once after updating.
//
//  NEW IN 1.13.0  (cube / mini cube: held clicks that never fired)
//  ------------------------------------------------------------------------
//    * The click-time search found a safe moment (+Nt), but the tick-by-tick
//      live check refused every tick until you died ("held click hadn't fired
//      yet"). Now a held click fires on its click-time plan at most 2 base
//      ticks after it, however often the live check refuses. The live check can
//      still fire it EARLIER (first safe tick) - it just can't hold it forever.
//    * Log: a click fired on its plan, or at the end of the window, says what the
//      live check objected to ("live: said no 14x: died 12 (dies 9t after
//      firing, landed at 30t), memory 2") so the disagreement can be fixed.
//
//  NEW IN 1.13.1
//  ------------------------------------------------------------------------
//    * Learned spots are saved per level (<save dir>/spots/online-<id>.txt or
//      local-<hash>.txt), written at every death and when you leave, and loaded
//      when you open the level again. Stats shows the file and how many came
//      from earlier sessions. Stage tab: "forget level" wipes them.
//
//  NEW IN 1.13.2
//  ------------------------------------------------------------------------
//    * A held click the live check keeps refusing fires EXACTLY on its
//      click-time plan (was plan + 2 ticks: too late in narrow gaps).
//    * Moving hazards: speed measured over a whole frame, not step to step
//      (uneven / CBF steps made it flip between 0 and 3x), and only trusted
//      once it has predicted the hazard's position correctly twice in a row.
//    * Refusal log says what the predicted jump hit ("hit MOVING spike at
//      +40px height") so a wrong prediction can be traced.
//    * New window look: rounded panel, stage-colored accent line and status
//      pill, highlighted tab, striped rows, GD close button.
//
//  NEW IN 1.14.0
//  ------------------------------------------------------------------------
//    * Modes tab (next to Stage): switch the buffer on / off per gamemode,
//      normal and mini separately (cube, ship, ball, UFO, wave, robot, spider,
//      swing). Off = that mode's clicks go through raw. "all on" / "all off".
//
//  NEW IN 1.14.1  (wave / ship releases)
//  ------------------------------------------------------------------------
//    * Learned spots are presses only: deaths in wave got blamed on the last
//      release, and saved "bad releases" held good releases back.
//    * Near-miss waits are presses only (wave corridors are always close).
//    * An input queued behind a held one (your release) is held at most 2
//      frames past its time, not the whole Extreme window.
//
//  NEW IN 1.14.2  (wave / ship on Stage 3, 4, Extreme)
//  ------------------------------------------------------------------------
//    * A ship / wave input is judged against YOUR rhythm: a press only counts
//      as deadly if it dies before your usual release (+4 ticks), or no
//      release at your rhythm saves it; same for a release and your usual gap.
//      Before, it assumed you'd hold at least 50 ms, so normal wave taps looked
//      deadly and were delayed - on the bigger stages by many ticks, with the
//      release shifted along ("late jumps", "holds feel off").
//
//  NEW IN 1.14.3  (wave)
//  ------------------------------------------------------------------------
//    * Ship / wave clicks that run out of CPU time go through raw again (as
//      before 1.12.1). Holding them for the live check left wave presses held
//      the whole 48-tick window ("held click hadn't fired yet ... planned +0t").
//      Cube-type modes keep the live fallback.
//    * Any held click with no plan to fall back on gives up after 1 frame
//      instead of waiting out the window.
//
//  NEW IN 1.15.0  (two engines)
//  ------------------------------------------------------------------------
//    * Mini cube keeps the current engine (it was tuned on Final Destination):
//      moving hazards, near-miss waits, learn from attempts, live fallback.
//    * Every other gamemode is back to the 1.10.2 behaviour: none of those, no
//      release cap, no rhythm judging, out of CPU = raw. See modernEngine().
//    * Both keep: a held click fires on its click-time plan at the latest.
//
//  NEW IN 1.15.1
//  ------------------------------------------------------------------------
//    * Inputs queued behind a held one (your release) fire exactly on their
//      scheduled time - your hold length, no re-check. The re-check could hold
//      wave releases for the whole window ("releases very late").
//
//  NEW IN 1.16.0  (wave spam)
//  ------------------------------------------------------------------------
//    * Wave inputs less than ~150 ms apart are a "stream". Once a stream has a
//      shift (an input got buffered), every following input gets the SAME
//      shift - or 1 tick less when that's safe too, so it winds down smoothly -
//      on a fixed schedule. Before, each input was judged alone (+3t, raw, +2t,
//      raw...), so hold lengths jumped around: the jitter.
//    * Those inputs are logged only with "Log: show safe clicks" on.
//
//  NEW IN 1.16.1
//  ------------------------------------------------------------------------
//    * "forget level" is always in the Stage tab (it only appeared when the tab
//      was opened inside a level); outside a level it says to open one.
//
//  NEW IN 1.16.2  (wave wobble)
//  ------------------------------------------------------------------------
//    * Wave judges presses / releases against your own hold / gap rhythm again
//      (1.14.2's rule, lost when 1.15.0 put wave back on the old engine):
//      short spam taps no longer look deadly and get delayed a tick or two.
//    * A stream keeps exactly the same shift until you pause (no 1-tick
//      wind-down per tap, which shortened one gap per tap).
//
//  NEW IN 1.17.0
//  ------------------------------------------------------------------------
//    * Changes tab in the window (every version, newest first) and changelog.md
//      (Geode shows it on the mod's page). (The in-game Changes tab was removed in 1.25.6.)
//
//  NEW IN 1.17.1
//  ------------------------------------------------------------------------
//    * Sim players now also copy position / last portal / last ground / landing
//      / sliding / direction / dash state from the real player (copyAttributes
//      doesn't). Wave tests were "dying" right after a release at impossible
//      heights, holding good releases back ("randomly goes up").
//
//  NEW IN 1.18.0  (performance: less chop)
//  ------------------------------------------------------------------------
//    * Held-click re-checks run once per physics tick (not on every CBF
//      sub-step too) with their own budget: 30% of a frame (min 0.8 ms).
//      A click due on its plan / window end still goes in on time.
//    * No extra full test of the chosen delay per buffered click (only drawn
//      for Show paths now).
//    * "How early was it" probe (log only): 1/8 budget, 8 coarse candidates.
//    * Moving-hazard bookkeeping only while in mini cube.
//    * Stats > PERFORMANCE: re-check cost, frame time, worst mod cost / frame.
//
//  NEW IN 1.18.1  (inputs never lost: Grief / Slaughterhouse reports)
//  ------------------------------------------------------------------------
//    * A repeated "press" while the mod thought you were already holding was
//      swallowed. After pausing while holding (the release happens in the pause
//      menu and never reaches us), that ate the next real press. Now only
//      swallowed while an input is actually waiting to fire.
//    * reconcile() never presses on its own (a stale "held" made it press).
//    * Pausing drops the held state and fires anything still waiting.
//    * If the physics-step hook (which fires held inputs) isn't running - another
//      mod replaced it, a long hitch - inputs go straight through.
//    * Watchdog: a held input waiting > 600 ms (wall clock) is fired.
//    * CPU budget checked every 4 sim steps (was 16): object-heavy levels can't
//      overrun it by much.
//
//  NEW IN 1.18.2  (heavy levels: "raw: CPU budget" spam)
//  ------------------------------------------------------------------------
//    * The first test of a click always gets >= 6 ms (it couldn't finish on
//      Grief / Slaughterhouse, so every click went raw); the search after it
//      gets a fresh budget of its own.
//    * Cube-type tests stop once you've landed and passed the 4-tick grace
//      (every verdict treats later deaths as the next jump's): ~40 fewer
//      collision-checked steps per test, same results.
//    * Search candidates get 1 "could you still save it" sample (was 2).
//    * Auto budget floor 2.5 ms (was 1.5). Stats: cost per predicted step.
//
//  NEW IN 1.18.3  (never give up on a deadly click)
//  ------------------------------------------------------------------------
//    * Compared with the original Predictive Buffer (which works on Grief /
//      Slaughterhouse for everyone): it has NO time limit and no follow-up
//      tests - every click is checked to the end, just simply. This one gave up
//      ("raw: CPU budget") when the full checks didn't fit the frame budget.
//    * Now: out of time on a deadly click -> finish with quick checks (no
//      follow-up tests; a death after your usual next input counts as yours to
//      steer out of) up to 12 ms from the click, instead of going raw.
//      Stats counts these clicks.
//
//  NEW IN 1.18.4  (diagnostics: "nothing in the Log")
//  ------------------------------------------------------------------------
//    * Inputs passed raw because the physics hook isn't running now say so in
//      the Log (once) - they used to pass silently, leaving it empty.
//    * Stats > CLICKS: inputs that reached the buffer (0 while clicking = another
//      mod handles inputs first).
//
//  NEW IN 1.18.5
//  ------------------------------------------------------------------------
//    * Clicks on an orb you're touching (within 6px, like GD's generous orb
//      area) are predicted through the orb instead of going raw
//      ("raw: pink orb (touching)" on Amethyst's UFO).
//
//  NEW IN 1.18.6  (orb clicks)
//  ------------------------------------------------------------------------
//    * The real bug: after a test used an orb, the same orb (still being
//      touched) stopped the prediction on the next tick, so every click on an
//      orb went raw. A used orb, or one GD already used this contact, is now
//      ignored for the rest of the test.
//
//  NEW IN 1.18.7
//  ------------------------------------------------------------------------
//    * "too late" / "no fix" lines say what the predicted death hits and whether
//      the prediction used an orb (to find wrong predictions).
//
//  NEW IN 1.18.8
//  ------------------------------------------------------------------------
//    * Undid 1.18.6's "skip orbs in GD's touched-rings list": GD lists an orb
//      there as soon as you overlap it, so the orb being clicked was ignored and
//      every orb click said "too late, no delay helps".
//
//  NEW IN 1.19.0  ("too late" even when clicking early)
//  ------------------------------------------------------------------------
//    * The search checks every tick only for the first 12 after a click, then
//      every few ticks (every 5 on Extreme), and the "how early" probe even
//      coarser. A narrow safe moment far from an early click was stepped over,
//      and the click was called "too late". Now, in cube / UFO / robot / ball /
//      spider / swing, a click the quick search can't fix is HELD and checked
//      every tick for the whole window, firing on the first safe tick (no
//      margin). Ship / wave keep the old behaviour.
//    * No-fix lines no longer claim "too late" (the coarse probe couldn't know).
//
//  NEW IN 1.19.1  (UFO: held clicks that never fired)
//  ------------------------------------------------------------------------
//    * UFO / swing: an orb only fires in the sim on a fresh click (press within
//      ~3 ticks), not because the button is still held.
//      Holding made every "fire now" test hit the orb and die, so nothing was
//      ever safe and the held click never fired.
//    * A held click checked every tick fires 2 ticks before doing nothing would
//      kill you, at the latest (it used to wait while you died).
//
//  NEW IN 1.19.2
//  ------------------------------------------------------------------------
//    * UFO / swing: no tick-by-tick hold for clicks the quick search can't fix
//      (raw again). In Amethyst's UFO part the prediction was wrong about both
//      pressing and not pressing, so held clicks never fired.
//
//  NEW IN 1.20.0  (portals)
//  ------------------------------------------------------------------------
//    * Gamemode and size portals are switched through in the sim (PlayerObject
//      toggle*Mode / togglePlayerScale on the hidden sim player, no effects)
//      instead of stopping the prediction. Clicks just before a portal - like an
//      orb right after a UFO portal - can be buffered now. The sim player is
//      put back in the real mode / size at the start of every test.
//    * Still not modelled (prediction stops): dual, mirror, teleport portals,
//      dash / spider / teleport orbs. The gamemode area's floor / ceiling after
//      a fly portal is not modelled either.
//
//  NEW IN 1.20.1
//  ------------------------------------------------------------------------
//    * 1.20.0 REVERTED: switching the hidden sim player's gamemode (toggle*Mode)
//      messed up the real portals. Gamemode / size portals stop the prediction
//      again, exactly as in 1.19.2.
//
//  NEW IN 1.20.2  (diagnostics only)
//  ------------------------------------------------------------------------
//    * Accuracy per mode also shows WHICH WAY predictions are off on average
//      (sideways vs up/down), and each "prediction off" log line too. Normal
//      UFO was off ~5px every time while mini UFO was exact.
//
//  NEW IN 1.21.0  (line up clicks the prediction can't see)
//  ------------------------------------------------------------------------
//    * Learn from attempts also records UFO presses (was mini cube only).
//    * An early click the prediction can't line up - blind past a portal or orb
//      ("raw: ... ahead"), or "no safe moment found" - is held when a spot where
//      a press WORKED on an earlier attempt is ahead within the window, in any
//      gamemode (spots match by position, so a spot past a UFO portal is found
//      from before it). It fires when you reach that spot (no prediction used),
//      or 2 ticks after its estimated arrival at the latest.
//    * Spots files get a 7th column (older files still load).
//
//  NEW IN 1.21.1
//  ------------------------------------------------------------------------
//    * UFO gets the rest of learn from attempts: a held UFO click fires on a
//      spot that worked before (even if the prediction disagrees), and a click
//      on a spot that killed you before is held and re-planned.
//
//  NEW IN 1.22.0  (moving spikes / orbs everywhere but ship / wave)
//  ------------------------------------------------------------------------
//    * Moving-hazard prediction (was mini cube only) now runs in every mode
//      except ship / wave: a spike that moves out of the way is tested where it
//      WILL be, so the small window it opens can be buffered ("no safe moment
//      found" while a spike was about to move). Same proof rule as before (a
//      speed is only used after it predicted the object right twice).
//    * Moving orbs too: sampled like hazards and used at their predicted spot.
//    * Near-miss clearance stays mini cube only (CPU).
//
//  NEW IN 1.22.1
//  ------------------------------------------------------------------------
//    * Moving objects: acceleration tracked (move triggers usually ease in /
//      out, which a constant-speed model never matched - they were never
//      trusted); slowing objects stop instead of reversing; trusted after one
//      correct prediction (within 3px / 25%), speed measured over half a frame.
//
//  NEW IN 1.22.2
//  ------------------------------------------------------------------------
//    * Removed the 1.19.0 "no safe moment found -> hold and check every tick"
//      behaviour: such clicks go straight through again, at once.
//    * "no safe moment found" / "no fix" log lines only with "Log: show safe
//      clicks" on.
//
//  NEW IN 1.23.0  (prediction: exact orbs + one-tick self-check)
//  ------------------------------------------------------------------------
//    * Exact orb contact: orbs the sim can use (yellow / pink / red / black /
//      blue / green) go through GD's own collision for the sim player, which
//      reports the exact tick it overlaps the orb's real hitbox
//      (playerTouchedRing, swallowed for sim players). Once seen, the box-based
//      orb guess is off and modelled orbs never cut the prediction.
//    * One-tick self-check (diagnostics only): every 8 ticks the sim predicts one
//      tick from your real state and compares with the game. Stats shows, per
//      mode, how often it's exact and the average x / y / y-speed difference; the
//      Log notes the first real mismatch per mode.
//
//  NEW IN 1.23.1
//  ------------------------------------------------------------------------
//    * Sim set-up: button state BEFORE speed / position. pushButton is a new
//      press (a flap in UFO), and coming last it gave every test made while you
//      held an extra flap (UFO one-tick check 72% exact, y-speed off by ~1).
//    * Learned-spot holds: at most 16 base ticks ahead (a 79-tick hold in UFO
//      let you fall, so the press landed below the orb), and only if not
//      pressing until then doesn't kill you first.
//
//  NEW IN 1.23.2
//  ------------------------------------------------------------------------
//    * Sim set-up copies all of the real player's plain jump / fall / slope /
//      force / timer state after the button (see syncSimPlayer), not just a
//      few fields: the "press waiting" state pushButton leaves behind made the
//      sim flap on its first tick (UFO one-tick check stayed ~67%).
//
//  NEW IN 1.23.3  (tick grid)
//  ------------------------------------------------------------------------
//    * With Click Between Frames a click lands mid-tick. The sim used to step
//      whole ticks from that moment, but held inputs can only fire on tick
//      boundaries, so "+9t" fired at the boundary after 9.x ticks - up to a
//      whole tick late (frame-perfect orbs missed / pressed low, live check
//      disagreeing with the plan). Now the mod tracks where it is inside the
//      tick (m_phase), every sim's first step is the REST of the current tick,
//      and held inputs are scheduled on the real tick grid.
//
//  NEW IN 1.24.0  (portal hold)
//  ------------------------------------------------------------------------
//    * A press with a gamemode / size portal at most 40px ahead used to go raw
//      ("raw: size portal ... 6px ahead"). Now it's held until you're through
//      the portal(s); then the live check (which can see again) fires it on the
//      first safe tick. Not in ship / wave. If no safe tick comes in the window
//      it fires at the window end.
//
//  NEW IN 1.24.1
//  ------------------------------------------------------------------------
//    * UFO / swing press while already touching an orb: the sim uses the orb on
//      that same tick with no flap, like GD. It flapped first and used the orb a
//      tick later, so the orb arc started late and high ("no fix ... hits object
//      at +22px, after using an orb"; accuracy check -6.8px up/down).
//    * Learn from attempts: mini cube only again (UFO spots fired presses
//      +17..+42 ticks late at spots from older attempts).
//
//  NEW IN 1.24.2
//  ------------------------------------------------------------------------
//    * Portal hold tightened: only a press at most 4 ticks before a gamemode /
//      size portal is held (was 40px, ~30 ticks), and it fires at most 6 ticks
//      after reaching the portal (was: whenever the prediction liked, up to the
//      window end - presses fired +17t / +35t late and every click queued
//      behind them shifted with them).
//
//  NEW IN 1.24.3
//  ------------------------------------------------------------------------
//    * Orb press check: a UFO / swing press on an orb is predicted three ways
//      (orb first with no flap / flap then orb / press + orb together). On your
//      own orb presses that go in on time, all three are compared with where you
//      really are 10 ticks later, and the closest one is used from then on
//      (saved). Shown in Stats > ONE-TICK CHECK and as "orb check" log lines.
//    * The orb a press uses at the start must really overlap you and not be
//      used already (a stale "touching" entry could fire a used orb).
//    * Deaths name spikes / saws / blocks / slopes instead of "object".
//
//  NEW IN 1.24.4
//  ------------------------------------------------------------------------
//    * Orb snap: a UFO / swing press that kills you now, with an unused orb
//      coming (reached without pressing, inside the window, before anything
//      kills you), is held and fired on the tick GD has you touching that orb -
//      or the tick the prediction has you touching it. The orb jump itself
//      isn't predicted for this (it was 6-18px off), so a wrong orb arc can't
//      make it say "no fix" any more. The orb check showed all three press
//      models equally off: the orb jump, not the press, is what's off.
//
//  NEW IN 1.24.5
//  ------------------------------------------------------------------------
//    * Orb jump measurement: on an orb press that went in on time, the log
//      shows the tick GD used the orb on and the y-speed it gave, next to the
//      prediction's tick and y-speed ("orb jump [pink orb]: game ... |
//      predicted ..."). Same tick but a different speed: the prediction learns
//      a correction for that orb type (and size) from then on.
//    * Sim players use orbs with the same second ringJump argument GD uses.
//
//  NEW IN 1.24.6
//  ------------------------------------------------------------------------
//    * The measurement showed the sim players' orb call does nothing: pink and
//      green orbs both came out at y-speed 6.87 (a plain flap) while the game
//      gave 4.46 (pink) and -10.62 + gravity flip (green). Every "after using an
//      orb" prediction was really a flap - why the first orb was never
//      bufferable.
//    * Orb table: every orb you really use records the y-speed right after it
//      and whether gravity flipped, per orb type / gamemode / size / gravity.
//      Predictions set exactly that when they use the orb. Saved across
//      sessions; listed in Stats.
//    * Removed 1.24.5's y-speed ratio (learned from that same no-op call).
//
//  NEW IN 1.24.7
//  ------------------------------------------------------------------------
//    * Orb aim replaces orb snap: a deadly UFO / swing press with an orb coming
//      tests EVERY tick you'd overlap the orb (up to 12) and fires on the one
//      whose orb jump keeps you alive longest. The normal search could never
//      pick an orb tick: it wants safe ticks around its pick (margin) and
//      strides over far ticks, and an orb is a 1-2 tick window. 1.24.4's snap
//      ran the window with the full lookahead and ran out of CPU every click
//      (Stats: "held for an orb: 0").
//    * Fires exactly on the tested tick (no waiting on GD's touch list).
//    * Orb press model defaults to GD's order (flap step, then the orb) and only
//      switches if another one is clearly closer; old orb checks (made while the
//      orb did nothing) are discarded.
//
//  NEW IN 1.24.8
//  ------------------------------------------------------------------------
//    * Orb aim never started ("held for an orb: 0"): GD reports orb contact only
//      while the button is down, so the no-press path never saw the orb. Orb
//      contact on that path now comes from the hitboxes; GD's own contact still
//      decides which press ticks really use the orb.
//    * A press held through a size / gamemode portal is aimed at the orb after
//      it once the prediction can see past the portal (it fired at the portal
//      cap into the spikes before).
//    * With "Log: show safe clicks": why orb aim didn't aim (no orb ahead / no
//      press uses it).
//
//  NEW IN 1.24.9
//  ------------------------------------------------------------------------
//    * GD sets an orb's m_isActivated as soon as you TOUCH it, not when it's
//      used. Orb aim skipped "activated" orbs - i.e. the orb you were in - so it
//      always said "no orb ahead", and the same check blocked the "orb first"
//      press model (why all three orb-press models always tied). Used orbs now
//      come from the mod's own record of the orbs you really used this attempt.
//
//  NEW IN 1.25.0
//  ------------------------------------------------------------------------
//    * THE orb bug (since 1.23.0's exact orb contact): only the FIRST test of a
//      click could use an orb. A test touching an orb leaves GD's "already
//      activated" state on it (its per-player flags were never put back, and
//      m_isActivated was snapshotted after the touch), so every later test -
//      the search, orb aim, the live check - was told the orb was gone. So an
//      early click never found the orb tick ("no safe moment" / "too late"),
//      while a click ON the orb (the first test) did use it. Now:
//        - sim players may use any orb you haven't really used this attempt
//          (and not the one the test already used), whatever GD's flags say;
//        - orbs' m_isActivated is saved before GD sees them in a test and put
//          back oldest-first.
//    * "Log: show safe clicks": orb aim lists what every tested tick did
//      ("+3:orb dies 14t  +4:orb ok ...").
//
//  NEW IN 1.25.1
//  ------------------------------------------------------------------------
//    * Size portals no longer stop the prediction. In Amethyst's UFO part the
//      orbs sit right at mini / normal size portals, so EVERY orb click there
//      went "raw: size portal (mini) 6px ahead" and nothing (search, orb aim)
//      ever ran. The sim player's size is set directly (m_vehicleSize + scale)
//      when it overlaps a size portal - no GD portal code is called and the real
//      portal object is never touched (1.20.0's GD toggles broke real portals).
//      Each test starts from your real size again.
//    * Learned orbs: an orb at a size / gravity not learned yet uses the same
//      orb's other size / gravity (mirrored) instead of the sim's own orb.
//    * Stats: "size portals passed inside predictions".
//
//  NEW IN 1.25.2  (log clean-up)
//  ------------------------------------------------------------------------
//    * Orb check / orb jump lines only when the orb prediction is off (> 2px).
//    * Gone: "REL raw (no effect here)", "orb aim: no orb ahead".
//    * Orb aim tests grouped: "+0 orb, dies 11t | +1..+10 flap, dies 7..1t".
//    * A click on an orb's last usable tick that still dies said "PRESS ok
//      (safe 11t)" (and "sim said safe" at the death): it keeps the real
//      verdict now, plus "PRESS too late for the green orb: ... click a bit
//      earlier".
//
//  NEW IN 1.25.3
//  ------------------------------------------------------------------------
//    * "Early UFO clicks go to the next orb" (Stage tab, on by default): a UFO /
//      swing press is aimed at the next orb even when pressing now would be a
//      safe flap - if you'd reach that orb without pressing, inside the window,
//      and pressing on it keeps you alive (whole lookahead). Before, only a
//      press that killed you now was aimed, so an early click far from the orb
//      just flapped. If you need a flap to reach the orb, nothing changes.
//
//  NEW IN 1.25.4
//  ------------------------------------------------------------------------
//    * "too late for the orb" only when nothing later keeps you alive either
//      (it showed up while a later flap was fine).
//    * Orb aim CPU: shorter tests (40t lookahead, 10 ticks max), and a safe
//      press you make while already on the orb isn't re-tested (the frame rate
//      dropped to ~124fps with it running on every UFO click).
//
//  NEW IN 1.25.5  (orb aim, thought through)
//  ------------------------------------------------------------------------
//    * Tests the WHOLE orb overlap, not its first 10 ticks. At 1x you overlap
//      an orb for ~40-50 ticks and the tick that clears the spikes is usually
//      late in it (on-time clicks were always "+0 orb, then flaps" - the end of
//      the overlap), so early clicks only ever tested deadly ticks.
//    * A tick "gets you through" if the orb is used and you survive the
//      lookahead OR reach the next orb with 6 ticks to spare (orb chains: the
//      next orb is your next click's job - like cube's landing rule). Before,
//      a chain's right tick looked deadly and the toggle refused to aim.
//    * Only a tick that gets you through is ever aimed at (it used to aim at
//      the "longest-surviving" tick even if it died, overriding the search).
//    * Fires in the MIDDLE of the longest stretch of working ticks.
//    * Tick picking moved to acecore::pickOrbTick and unit tested: every tick
//      for overlaps up to 24, else every 3rd tick, then every tick +-2 around
//      working ticks, the orb's start / end, and the 3 longest survivors, then
//      (if still nothing) every remaining tick from the end back. Finds a
//      1-tick window at any position of a 50-tick overlap, with or without
//      hints. CPU: 2.5 ms per click at most; a test stops once it's safely at
//      the next orb.
//
//  NEW IN 1.25.6  (window)
//  ------------------------------------------------------------------------
//    * Changes tab removed (changelog.md stays). "Early UFO clicks go to the
//      next orb" is always on - its toggle is gone.
//    * New look: tabs as one bar with 4 equal segments, the Stage tab in three
//      cards (Stage / Custom frames / Options), the Modes tab in two cards, a
//      real scrollbar on Log / Stats, "clear" in the footer, an empty-log hint.
//      key (read from the Window Key setting, e.g. "Left Alt") on the Stage tab.
//
//  NEW IN 1.25.7  (window colours / buttons)
//  ------------------------------------------------------------------------
//    * Midnight + purple: the body is a vertical gradient (purple at the top
//      into midnight blue), a violet-to-pink line under the title, violet
//      selected tab / stage, lavender card titles, purple-tinted rows.
//    * Every tab and button is a rounded box and the WHOLE box is clickable
//      (tabs only reacted on their text before). Stage buttons, Apply, +/-,
//      forget level, all settings, clear, all on / off, scroll arrows - one
//      style (GD's green / grey button sprites are gone).
//    * White title (bigFont) instead of the yellow goldFont.
//
//  NEW IN 1.25.8  (window polish)
//  ------------------------------------------------------------------------
//    * Boxes are drawn as exact rounded polygons (CCDrawNode). The 9-slice
//      texture came out square on small boxes in the game - every button
//      looked like a block.
//    * The status pill text ("EXTREME"), the stage name and the close X are
//      white. The X is a box like the other buttons (GD's close sprite is green).
//    * Stats rewritten: short sections (This level / Prediction / Orbs / Your
//      rhythm / Windows / Performance) of "label  value" rows - label on the
//      left, value right-aligned. Debug numbers moved to a DETAILS section that
//      only shows with "Log: show safe clicks".
//
//  NEW IN 1.25.9
//  ------------------------------------------------------------------------
//    * Removed everything that held or moved a click for an orb (orb aim, the
//      early-click orb snap, aiming through portals), the gamemode-portal hold,
//      and the 3-way orb press check (3 extra predictions per UFO click). Kept,
//      because they only make predictions right and never hold a click: learned
//      orb speeds, "only the first test could use an orb" fix, size portals.
//    * Wave: an input in a spam burst goes in on time whenever that's safe - the
//      burst's delay used to stay on every input until you paused (lag).
//    * Learn from attempts: spots saved for other gamemodes by 1.21-1.24.0 are
//      dropped when a level's spots load (mini cube only, as intended).
//    * Stats: the first section is the level's name; section titles are bold
//      white; Windows shows what clicks really get (Extreme: 20f / 12f - it
//      showed the base 8f / 5f). Learned orbs moved to Details.
//    * Mouse wheel over the window scrolls only the window (CCMouseDispatcher
//      hook) - a level list behind it scrolled too.
//
//  NEW IN 1.26.0  (lighter: less work every frame)
//  ------------------------------------------------------------------------
//    * Moving-object tracking off: it logged every nearby spike / orb on every
//      physics step (240x a second) and never predicted anything in practice.
//    * Diagnostics only with "Log: show safe clicks": the one-tick check (a
//      prediction every 8 ticks) and accuracy tracking (every prediction's path
//      recorded and checked 100ms later), plus the "| live: said no ..." detail
//      on fired clicks.
//    * The window only rebuilds text that changed (header, footer, Log when a
//      line was added; Stats about once a second while a level runs) - all of
//      its labels were rebuilt 4x a second during play.
//
//  NEW IN 1.26.1  (smoother: follow the plan, in-game pill, new Log)
//  ------------------------------------------------------------------------
//    * Held clicks follow the click-time plan (not mini cube): one prediction
//      at click time records the path; each tick only compares your position
//      with it (<= 0.5px) - no full prediction per tick. It fires on the tick
//      the per-tick check would have chosen (the first safe tick with a safe
//      tick after it; wave: the first safe tick). Off the path -> the full
//      per-tick check takes over. Stats: "Held clicks fired on plan".
//    * "Small while playing" (on by default): a small pill (stage + latest Log
//      line) while a level runs, the full window when paused / in menus. The
//      hidden window costs nothing per frame.
//    * Click Log lines are built once, only if they'll be shown (they were
//      built twice, and hidden ones stored too).
//    * Log redesign: "time  TAG  text" - coloured tags (BUFFER, FIRED, DEATH,
//      RAW, QUEUED, OK, NOTE) and short texts.
//    * Polish: the window pops in, the tab highlight slides, the last tab is
//      remembered, Window Opacity setting (panels only, text stays solid);
//      "forget level" / "all settings" moved into the OPTIONS title row.
//    * "V2" badge next to the title (TITLE_BADGE, now "V2.1").
//
//  NEW IN 1.26.2
//  ------------------------------------------------------------------------
//    * Ghost orb fixed: sim players no longer call GD's ringJump - it drew the
//      orb's circle effect where the prediction was (a "random orb" next to the
//      real one) and never moved the sim player anyway. The learned orb table
//      does the jump; blue / green orbs not learned yet still flip gravity.
//      PlayerObject circle effects are also blocked during tests.
//    * Android: mod.json lists android 2.2081; the mouse-wheel hook is desktop
//      only (GEODE_IS_DESKTOP); the keyboard hook is macOS only (Windows polls
//      the key). New on every platform: an "Aceware" button in the pause
//      menu shows / hides the window (the way to open it on Android).
//      .github/workflows/multi-platform.yml builds Windows + Android32/64 and
//      combines them into one .geode.
//    * iOS + macOS: every hooked / called function checked in the 2.2081
//      bindings (no "inline" on ios / imac / m1). macOS uses the keyboard hook
//      for the Window Key and the mouse-wheel hook; iOS the pause-menu button.
//
//  NEW IN 2.1
//  ------------------------------------------------------------------------
//    * Footer left: "<Window Key> to toggle" (white bold) instead of the status
//      line ("ACEWARE  EXTREME" ...); phones: "Pause menu > Aceware".
//    * Stage tab: the "Left Alt: show / hide" credit text is gone; Discord is a
//      violet pill (white bold, click = copy). Card hints, the Modes hint and
//      grey Stats rows are white; "forget level" is violet like the badge.
//    * Pause-menu button (every platform): CircleButtonSprite (green, Medium)
//      with resources/pauseButtonIcon.png on top, sized to the other buttons in
//      right-button-menu.
//    * Phones: window x1.5 (PLATFORM_WINDOW_SCALE); hints name the pause button.
//    * Wave releases (Extreme): held at most max(3 frames, the stage's wave
//      release window) instead of 12 frames (WAVE_RELEASE_MAX_BASE), and a
//      release path cut short by an orb / portal with no death counts as safe
//      (verdictFor). Both made releases come out late or look ignored.
//    * Orb crash on iOS / Apple Silicon Mac fixed: RingObject::triggerActivated
//      is no longer hooked (12 bytes on arm64, too small for a hook - it
//      clobbered powerOnObject next to it). Hooked-function sizes checked on
//      ios / m1 / imac: everything else has 60+ bytes.
//    * Wave engine: the lookahead only reaches a bit past the input's own
//      stretch (x.waveCommit = your next input at your rhythm + grace; horizon
//      max(2x commit, commit + 8, 26) base ticks instead of 65 / 50) - a wave
//      death after that is the next input's job anyway, so the verdicts are
//      the same and every test is ~1.5-2.5x cheaper (spam frame spikes).
//    * Held RELEASES follow their plan too (planDrifted) instead of a full
//      prediction every tick while held.
//    * Wave / ship follow-ups (pickFollowUpTime): an input arriving behind a
//      held one is tested on time vs keeping the spacing; on time when safe.
//      Delays capped at HOLD_DELAY_FRAC of your rhythm (min 6 base ticks);
//      Extreme ship releases capped like wave releases.
//    * "Physics hook isn't running" only after STALL_FRAMES frames with no step
//      while alive (clicks while dead / respawning were flagged).
//    * Stage banner only for the F6 key; no OK / QUEUED log lines.
//    * Tools tab: Orb helper (ORB_HELP_BASE), saved stats (stats-life /
//      stats-<level>), export report (reports/report-*.txt + clipboard, trace
//      of every click).
//    * Level scan (startScan / scanSlice, ScanRunner on the overlay): while
//      paused, a beam search from where you paused - up to SCAN_BEAM versions
//      of the run, each tries "keep" and "switch" every SCAN_STEP_BASE ticks,
//      dead ones dropped, survivors thinned to one per (button, height, fall
//      speed) bucket, fewest inputs first. Each version's state is a
//      PlayerSnap applied on top of the real player (copying into a spare
//      PlayerObject lost state). Best version's inputs -> routes/<level>.txt.
//      Click guide draws it; Snap to scan (routeMatch) holds early inputs
//      until the scanned spot.
//      Gamemode portals: in scan tests only (m_scanSim) the hidden player
//      switches mode with its own toggles (setSimMode) and gets the portal as
//      m_lastPortalPos; the snapshot carries the mode. The report traces the
//      real player's mode switches (lastPortalPos, portalY) to compare.
//    * Show paths: hitboxes at the path ends are circles (drawRing).
//    * System line: the window's footer shows what the game runs on
//      (Windows 10 / 11, Wine / Proton, macOS + chip, iOS / iPadOS, Android +
//      32/64-bit), and Stats has a "System" section (OS build, device, GD,
//      Geode and mod versions). Read once (systemInfo()), cached.
// ============================================================================

#include <Geode/Geode.hpp>
#include <random>
#include <unordered_set>
#include <queue>
#include <cctype>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#ifdef GEODE_IS_DESKTOP
// keyboard hook header: desktop only (it doesn't compile on iOS - no
// CCKeyboardDispatcher constructor address there; only macOS uses the hook)
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCMouseDispatcher.hpp>
#endif
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/AppDelegate.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/RingObject.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "BufferCore.hpp"
#include "AceCourse.hpp"

#if defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS)
#include <sys/sysctl.h>
#elif defined(GEODE_IS_ANDROID)
#include <sys/system_properties.h>
#endif

using namespace geode::prelude;

// ----------------------------------------------------------------------------
//  System info (2.1): what the game is running on, for the window's footer
//  and the Stats "System" section. Read once, then cached.
// ----------------------------------------------------------------------------
namespace {

struct SystemInfo {
    std::string brief;   // footer: "Windows 11", "macOS 15.1 - Apple Silicon", ...
    std::string side;    // sidebar (2.2): "Windows 10 (22H2)" - brief with its version
    std::string full;    // Stats: with build / version numbers
    std::string device;  // Stats: chip / model, "" if unknown
};

#if defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS)
std::string sysctlString(char const* name) {
    char buf[256] = {};
    size_t len = sizeof(buf) - 1;
    if (sysctlbyname(name, buf, &len, nullptr, 0) != 0 || len == 0) return "";
    return std::string(buf, strnlen(buf, len));
}
#endif

#if defined(GEODE_IS_ANDROID)
std::string androidProp(char const* name) {
    char buf[PROP_VALUE_MAX] = {};
    int const n = __system_property_get(name, buf);
    return n > 0 ? std::string(buf, static_cast<size_t>(n)) : std::string();
}
#endif

#if defined(GEODE_IS_WINDOWS) && defined(_MSC_VER)
#pragma comment(lib, "advapi32.lib") // RegGetValueA (Windows version for the sidebar)
#endif
SystemInfo readSystemInfo() {
    SystemInfo si;
#if defined(GEODE_IS_WINDOWS)
    // GetVersionEx lies to apps without a manifest; ntdll's RtlGetVersion doesn't.
    // Windows 11 still says 10.0 - it's told apart by its build number (22000+).
    HMODULE const nt = GetModuleHandleW(L"ntdll.dll");
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof(v);
    if (nt) {
        if (auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(nt, "RtlGetVersion"))) fn(&v);
    }
    std::string name;
    if (v.dwMajorVersion == 10 && v.dwBuildNumber >= 22000) name = "Windows 11";
    else if (v.dwMajorVersion == 10) name = "Windows 10";
    else if (v.dwMajorVersion > 0) name = fmt::format("Windows {}.{}", v.dwMajorVersion, v.dwMinorVersion);
    else name = "Windows";
    si.brief = name;
    si.full = v.dwBuildNumber > 0 ? fmt::format("{} (build {})", name, v.dwBuildNumber) : name;
    // the version people know ("22H2", older: "1909"), else the build number
    {
        char buf[64] = {};
        DWORD sz = sizeof(buf);
        std::string rel;
        if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "DisplayVersion",
                         RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS) rel = buf;
        sz = sizeof(buf);
        if (rel.empty() && RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ReleaseId",
                                        RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS) rel = buf;
        if (rel.empty() && v.dwBuildNumber > 0) rel = std::to_string(v.dwBuildNumber);
        si.side = rel.empty() ? name : fmt::format("{} ({})", name, rel);
    }
    // Wine / Proton (Linux, Steam Deck, Mac): ntdll has Wine's own exports.
    if (nt) {
        using WineVerFn = char const*(__cdecl*)();
        using WineHostFn = void(__cdecl*)(char const**, char const**);
        if (auto wv = reinterpret_cast<WineVerFn>(GetProcAddress(nt, "wine_get_version"))) {
            char const* sys = nullptr;
            char const* rel = nullptr;
            if (auto wh = reinterpret_cast<WineHostFn>(GetProcAddress(nt, "wine_get_host_version"))) wh(&sys, &rel);
            std::string host = sys ? sys : "";
            if (host == "Darwin") host = "macOS";
            char const* ver = wv();
            si.brief = host.empty() ? "Wine" : "Wine on " + host;
            si.full = fmt::format("Wine {}{} (as {})", ver ? ver : "", host.empty() ? "" : " on " + host, name);
        }
    }
#elif defined(GEODE_IS_MACOS)
    std::string const ver = sysctlString("kern.osproductversion");
    std::string chip;
  #if defined(GEODE_IS_ARM_MAC)
    chip = "Apple Silicon";
  #else
    int translated = 0;
    size_t tlen = sizeof(translated);
    bool const rosetta = sysctlbyname("sysctl.proc_translated", &translated, &tlen, nullptr, 0) == 0 && translated == 1;
    chip = rosetta ? "Intel build on Apple Silicon (Rosetta)" : "Intel";
  #endif
    std::string const os = ver.empty() ? "macOS" : "macOS " + ver;
    si.brief = os + " - " + (chip.rfind("Intel build", 0) == 0 ? std::string("Rosetta") : chip);
    si.full = os + ", " + chip;
    std::string const cpu = sysctlString("machdep.cpu.brand_string");
    std::string const model = sysctlString("hw.model");
    si.device = cpu.empty() ? model : (model.empty() ? cpu : cpu + " (" + model + ")");
#elif defined(GEODE_IS_IOS)
    std::string const ver = sysctlString("kern.osproductversion");
    std::string const machine = sysctlString("hw.machine"); // "iPhone15,2", "iPad13,4"
    std::string os = "iOS";
    if (machine.rfind("iPad", 0) == 0) os = "iPadOS";
    si.brief = ver.empty() ? os : os + " " + ver;
    si.full = si.brief;
    if (!machine.empty() && machine.rfind("iP", 0) != 0) si.full += " (on a Mac)";
    si.device = machine;
#elif defined(GEODE_IS_ANDROID)
    std::string const ver = androidProp("ro.build.version.release");
    std::string const sdk = androidProp("ro.build.version.sdk");
  #if defined(GEODE_IS_ANDROID64)
    char const* bits = "64-bit";
  #else
    char const* bits = "32-bit";
  #endif
    std::string const os = ver.empty() ? "Android" : "Android " + ver;
    si.brief = os + " - " + bits;
    si.full = sdk.empty() ? si.brief : fmt::format("{} (API {}), {}", os, sdk, bits);
    std::string const make = androidProp("ro.product.manufacturer");
    std::string const model = androidProp("ro.product.model");
    si.device = make.empty() ? model : (model.empty() ? make : make + " " + model);
#else
    si.brief = si.full = "Unknown platform";
#endif
    return si;
}

SystemInfo const& systemInfo() {
    static SystemInfo const info = readSystemInfo();
    return info;
}

} // namespace

namespace {

using Clock = std::chrono::steady_clock;

// Tick constants are written for GD's default 240 TPS ("base ticks") and are
// scaled to whatever TPS the game is really running at.
constexpr float BASE_DT        = 0.25f; // one 240-TPS tick, in 60fps-frame units
constexpr int   MAX_SIM_STEPS  = 480;   // hard cap on the lookahead length
constexpr int   MIN_HORIZON    = 16;
constexpr int   MAX_CANDIDATES = 16;    // coarse steps over the far part of the window
constexpr int   DENSE_SCAN_BASE = 12;   // first 12 base ticks after a click are checked tick by tick
constexpr int   DT_RING        = 16;    // recent tick lengths kept to find the nominal tick
constexpr int   EXTREME_MIN_BUDGET_US = 6000; // manual-budget floor in extreme mode (auto budget off)
// Auto CPU budget: a share of the measured frame time, so the lookahead never
// costs a visible frame at high FPS (170fps = 5.9 ms per frame -> ~1.8 ms budget).
constexpr float AUTO_BUDGET_FRAC         = 0.50f;
constexpr float AUTO_BUDGET_FRAC_EXTREME = 0.75f;
constexpr int   AUTO_BUDGET_MIN_US       = 2500;
// The first test ("does the click as made kill you?") always gets at least
// this much: on object-heavy levels it couldn't finish inside a small budget
// and every click went raw.
constexpr int   R0_BUDGET_MIN_US         = 6000;
// Out of time on a deadly click: cheap checks may continue up to this (from
// the click), instead of the click going raw.
constexpr int   CLICK_HARD_CAP_US        = 12000;
constexpr int   AUTO_BUDGET_MAX_US       = 8000;
constexpr double TIME_EPS                = 1e-4; // 60fps-frame units
// Live re-check of a held click (every tick): this share of the frame time.
constexpr float LIVE_BUDGET_FRAC         = 0.30f;
constexpr int   LIVE_BUDGET_MIN_US       = 800;
// Safety nets: the physics-step hook counts as "not running" after this long
// without a call; a held input is force-fired after this long (wall clock,
// longer than the biggest window).
constexpr int   STEP_STALL_MS            = 150;
// ...and only called "the hook isn't running" after this many rendered frames
// in a row with the player alive, not paused, and no physics step (2.1).
constexpr int   STALL_FRAMES             = 8;
constexpr int   WATCHDOG_MS              = 600;
// A velocity orb counts as the target of a click if the sim player reaches it
// within this many base ticks after the click lands (125 ms).
constexpr int   ORB_INTENT_BASE          = 30;
// UFO / swing: an orb only fires on a fresh click, i.e. a press at most this
// many base ticks before touching it (GD's own click buffer is about that).
constexpr int   ORB_CLICK_BASE           = 3;
// "Could you still save this yourself?" (see interceptInput):
//   a death within COMMIT base ticks of the click (100 ms) is always the click's
//   fault - nobody corrects that fast, and it is exactly what the buffer is for;
//   a later death only counts if no follow-up input, made at least FOLLOW base
//   ticks (50 ms) after the click, avoids it.
constexpr int   ESCAPE_COMMIT_BASE       = 24;
constexpr int   ESCAPE_FOLLOW_BASE       = 12;
// Landing rule grace: dying within this many base ticks of touching down still
// counts as the click's fault (landing straight on a spike).
constexpr int   LAND_GRACE_BASE          = 4;
// Accuracy check: compare the real player with the prediction this many base
// ticks (100 ms) after the click lands; a hit is within ACCURACY_TOL_PX.
constexpr int   ACCURACY_CHECK_BASE      = 24;
constexpr float ACCURACY_TOL_PX          = 3.0f;
// Auto windows: samples needed, percentile covered, margin, floor (frames @60).
constexpr size_t AUTO_MIN_SAMPLES        = 12;
constexpr float  AUTO_PERCENTILE         = 0.90f;
constexpr float  AUTO_MARGIN_FRAMES      = 0.25f;
constexpr float  AUTO_MIN_FRAMES         = 0.25f;
// No-fix probe: how far past the window we look to tell you how early you were.
constexpr int   PROBE_MIN_BASE           = 40;
// Your input rhythm (60fps frames), used until enough real samples exist:
// typical tap length, and typical time between letting go and pressing again.
constexpr float  DEFAULT_TAP_FRAMES      = 4.0f;  // ~67 ms
constexpr float  DEFAULT_GAP_FRAMES      = 6.0f;  // ~100 ms
constexpr size_t RHYTHM_MIN_SAMPLES      = 6;
// Clearance (v1.11): a path that misses a hazard by less than the target is a
// "near miss". In live re-planning the buffer waits up to CLEAN_LOOK_BASE base
// ticks for a moment that clears it by the target (and by CLEAN_GAIN_PX more
// than now). Target = CLEAR_ERR_SCALE x this mode's measured prediction error.
constexpr float CLEAR_DEFAULT_PX         = 1.5f;
constexpr float CLEAR_MIN_PX             = 1.0f;
constexpr float CLEAR_MAX_PX             = 4.0f;
constexpr float CLEAR_ERR_SCALE          = 1.25f;
constexpr int   CLEAR_MIN_CHECKS         = 8;
constexpr float CLEAN_GAIN_PX            = 0.5f;
constexpr int   CLEAN_LOOK_BASE          = 4;
// Spot memory (learning from attempts): an input is "good" if you're alive
// this long after it (60fps frames): ship / wave / UFO / swing 100 ms; cube-type
// modes until you land again (+ grace), at most 0.5 s.
constexpr double SPOT_WINDOW_HOLD        = 6.0;
constexpr double SPOT_WINDOW_LAND_MAX    = 30.0;
constexpr double SPOT_LAND_GRACE         = 1.0;
// Ship / wave: a death within this many base ticks after your usual next input
// still counts as the click's fault (you can't react faster than your rhythm).
constexpr int    WAVE_GRACE_BASE         = 4;
// Extreme: a wave RELEASE is held at most this many base ticks (12 = 3 frames,
// 50 ms) instead of the 12-frame hold cap (2.1). A held release keeps the wave
// going up, so a long hold read as "late" or "ignored" - past this it goes
// through as you made it.
// Ship, light touch (2.2): a ship input is held at most 8 ticks (2 frames,
// ~33 ms). Holding ship inputs longer pulled you down off a straight fly.
// (2.3: a crash further out is checked by the escape search, not assumed
// to be your next input's job.)
// Wave / ship: the escape search's first follow-up also has to work this
// many base ticks late (WAVE_GRACE_BASE, above).
constexpr int    SHIP_MAX_DELAY_BASE     = 8;
// Wave / ship (2.1): no input is held longer than this share of your own
// rhythm (your usual hold for a press, gap for a release), and never less
// than HOLD_DELAY_MIN_BASE. A delay longer than the gesture itself moved the
// whole zig-zag: a +18t mini-wave press, then its release 18t late, death.
constexpr float  HOLD_DELAY_FRAC         = 0.75f;
constexpr int    HOLD_DELAY_MIN_BASE     = 6;
// Orb helper (2.1): how far ahead (base ticks, 83 ms) an early click looks for
// the next orb to wait for.
constexpr int    ORB_HELP_BASE           = 20;
// Level scan (2.1): see scanSlice().
constexpr int    SCAN_STEP_BASE          = 2;     // a version may switch every 2 ticks (240 TPS)
constexpr float  SCAN_STEP_WAVE          = 1.f;   // wave: every tick - mini wave gaps are tight
constexpr int    SCAN_BEAM               = 64;    // versions kept alive at once (spread over heights)
constexpr float  SCAN_SAFE_CLR           = 12.f;  // closer than this to a hazard costs the route (guide keeps a margin)
constexpr float  SCAN_CLOSE_COST         = 0.25f; // cost per step spent right at a hazard (an input costs 1)
constexpr float  SCAN_Y_BUCKET           = 3.f;   // versions closer than this in height count as the same
constexpr int    SCAN_SLICE_US           = 12000; // CPU per frame while scanning (paused)
constexpr int    SCAN_BEAM_WIDE          = 256;   // "trying harder" after backing up: 4x the versions ...
constexpr float  SCAN_Y_BUCKET_WIDE      = 1.5f;  // ... finer heights, and a switch every tick
constexpr float  SCAN_REWIND_EVERY       = 40.f;  // keep a copy of the versions every 40 px ...
constexpr int    SCAN_REWIND_KEEP        = 12;    // ... the last 12 of them
constexpr float  SCAN_REWIND_PX          = 120.f; // stuck: back up about 4 blocks
constexpr float  SCAN_WIDE_PAST          = 90.f;  // and try harder until 3 blocks past the stuck spot
constexpr float  SCAN_SKIP_PAST          = 30.f;  // last resort: go through the spot, 1 block past it
constexpr int    SCAN_TRIES              = 3;     // (2.2) tries per stuck spot before going through it
// (2.2) Ship: GD players straight fly - quick taps to stay level - instead of
// long holds. A ship version pays a little for flying at an angle (per step,
// full tilt), so of the ways through the scan keeps the level one.
constexpr float  SCAN_SHIP_LEVEL         = 0.03f;
constexpr float  SHIP_TILT_VY            = 6.f;   // m_yVelocity that counts as full tilt
constexpr int    SCAN_ASTAR_MS           = 400;   // (2.2) A* time for one stuck spot (paused)
constexpr int    SCAN_ASTAR_MAX          = 20000; // (2.2) ... and states at most
constexpr float  END_SLACK               = 60.f;  // (2.2) this close to the level end = the end
constexpr float  GROUND_Y                = 90.f;  // (2.2) top of GD's ground (a cube stands at y 105)
constexpr float  LEVEL_MARGIN            = 150.f; // (2.2) how far past every object a scan version may go
constexpr int    KILL_OUT_OF_LEVEL       = -2;    // (2.2) kill type: left the level's height range
constexpr int    KILL_OFF_PATH           = -3;    // (2.2) kill type: nothing of the level near it
constexpr float  OFF_PATH_PX             = 300.f; // (2.2) 10 blocks: a level's path is never further from its obstacles
constexpr float  SIM_EXTRA_PAD           = 45.f;  // (2.2) objects this close to the hidden player always get checked
constexpr int    REAL_DECIDE_TICKS       = 2;     // real-game scan: an input decision every 2 ticks
constexpr int    REAL_CP_EVERY           = 8;     // a checkpoint every 8 decisions (16 ticks)
constexpr int    REAL_CP_KEEP            = 48;    // keep the last 48 (~3 s to back up through)
constexpr int    REAL_MAX_RESETS         = 600;   // give up on a spot after this many tries
constexpr float  REAL_PAST               = 60.f;  // done when 2 blocks past the stuck spot
constexpr float  ANCHOR_STEP             = 30.f;  // real-player state saved every block of travel (scan restarts)
constexpr float  ANCHOR_SAFE             = 90.f;  // ... and only kept if you got 3 blocks past it
constexpr float  ROUTE_OFF_Y             = 300.f; // real player this far (10 blocks) off the scanned path ...
constexpr int    ROUTE_OFF_CHECKS        = 90;    // ... for this many checks (every 4 ticks, ~1.5s) drops that part
// A held click the live check never approves, with no click-time plan to fall
// back on, goes in after this many base ticks (1 frame).
constexpr int    LIVE_GIVEUP_BASE        = 4;
// A press is held through a gamemode / size portal only if the portal is at
// most this far ahead (px) - i.e. you clicked right before it.
constexpr float  PORTAL_HOLD_PX          = 40.f;
constexpr int    PORTAL_HOLD_TICKS_BASE  = 4;  // only presses this close (in ticks) to the portal are held
constexpr float  MINI_SIZE               = 0.6f; // GD's mini scale
constexpr float  PLAN_DRIFT_PX           = 0.5f;  // held click: off the planned path by more than this = re-check (1.26.1)
constexpr int    ORB_AIM_MAX_TESTS       = 64; // orb aim: tests per click at most (the time limit below decides first)
constexpr int    ORB_AIM_FINE_SPAN       = 24; // overlap this short: every tick; longer: every 3rd, then around good ones
constexpr int    ORB_AIM_LOOK_BASE       = 40; // lookahead after each tested press
// Reaching the next orb (hitbox + 6px) at least this long before dying = "gets
// you through" (the 6px pad is ~4 ticks early at 1x: ~2 ticks left to click it).
constexpr int    ORB_CHAIN_SPARE_BASE    = 6;
constexpr long long ORB_AIM_BUDGET_US    = 2500; // orb aim: CPU per click at most
constexpr int    PORTAL_WAIT_AFTER_BASE  = 6;  // ...and fired at most this many ticks after reaching it
// Learn from attempts: an early click is held for a learned spot at most this
// many base ticks ahead (4 frames) - a longer hold changes your path.
constexpr int    SPOT_HOLD_MAX_BASE      = 16;
// Wave spam: inputs less than this apart (60fps frames, ~150 ms) are one stream;
// a stream keeps one shift until a pause.
constexpr double STREAM_GAP_FRAMES       = 9.0;
// Raw Wave Spam only goes raw above this click rate; below it the wave is still
// corrected like any other mode. (7 cps = one press every 8.57 frames @60.)
constexpr double RAW_SPAM_MIN_CPS        = 7.0;
// (2.3) Spam: CPU per input inside a wave / ship stream. A click could take
// 6 ms just for its first test (12 ms worst) - at 240 fps a frame is 4.2 ms,
// so spamming dropped frames every few inputs (reports: "worst click cost
// 193% of a frame") and the inputs after a dropped frame landed late.
constexpr int    STREAM_BUDGET_US        = 1500;
constexpr int    STREAM_HARD_CAP_US      = 2500;
constexpr std::uintmax_t MAX_LOG_BYTES = 5u * 1024u * 1024u;

// ----------------------------------------------------------------------------
//  1. Settings
// ----------------------------------------------------------------------------
enum WinIdx {
    W_CUBE,     // cube / ball / spider
    W_UFO,
    W_SWING,
    W_ROBOT,
    W_SHIP,     // ship press
    W_SHIP_REL, // ship release
    W_WAVE,     // wave press
    W_WAVE_REL, // wave release
    W_COUNT
};

constexpr char const* WINDOW_KEYS[W_COUNT] = {
    "window-cube", "window-ufo", "window-swing", "window-robot",
    "window-ship", "window-ship-release", "window-wave", "window-wave-release",
};

// ----------------------------------------------------------------------------
//  Stages. One setting picks everything: Off, Stage 1-4, Extreme.
//  Windows are in 60fps frames (1.0 = one frame at 60fps) per WinIdx.
// ----------------------------------------------------------------------------
struct StageDef {
    char const* setting; // value in mod.json's "stage" dropdown
    char const* name;    // shown in notifications / the window
    bool   on;
    bool   extreme;
    float  w[W_COUNT];
};
constexpr StageDef STAGES[] = {
    {"Off",     "OFF (raw input)",                        false, false, {0, 0, 0, 0, 0, 0, 0, 0}},
    // order: cube, ufo, swing, robot, ship press, ship release, wave press, wave release
    {"Stage 1", "Stage 1: 1 frame",                       true,  false, {1, 1, 1, 1, 1, 1, 1, 1}},
    {"Stage 2", "Stage 2: 2 frames",                      true,  false, {2, 2, 2, 2, 2, 2, 2, 2}},
    // 2.2: Stage 3 / 4 were 4 / 8 frames - a big jump from Stage 2 that held
    // clicks too long ("too harsh"). Now an even climb: 1, 2, 3, 5 frames.
    {"Stage 3", "Stage 3: 3 frames (2 ship)",             true,  false, {3, 3, 3, 3, 2, 2, 3, 3}},
    {"Stage 4", "Stage 4: 5 frames (4 wave, 2 ship)",     true,  false, {5, 5, 5, 5, 2, 2, 4, 4}},
    {"Extreme", "EXTREME: 20 frames (12 wave, 2 ship) - BUGGY", true, true, {8, 8, 8, 8, 5, 5, 5, 5}},
    // Your own frames (window's Stage tab, Apply). Windows are filled in by Config::load.
    {"Custom",  "CUSTOM",                                 true,  true,  {0, 0, 0, 0, 0, 0, 0, 0}},
};
constexpr int STAGE_COUNT = static_cast<int>(sizeof(STAGES) / sizeof(STAGES[0]));
constexpr int STAGE_EXTREME = 5;
constexpr int STAGE_CUSTOM  = 6;

// Custom stage: frames (60fps) for cube-type modes (cube, ball, spider, UFO,
// swing, robot), ship and wave. Saved values, set from the window.
constexpr float CUSTOM_MIN_FRAMES = 0.5f;
constexpr float CUSTOM_MAX_FRAMES = 30.0f;
struct CustomFrames {
    float cube = 6.f, ship = 4.f, wave = 4.f;
    static float clampF(double v) { return std::clamp(static_cast<float>(v), CUSTOM_MIN_FRAMES, CUSTOM_MAX_FRAMES); }
    static CustomFrames load() {
        auto* mod = Mod::get();
        CustomFrames f;
        f.cube = clampF(mod->getSavedValue<double>("custom-cube", 6.0));
        f.ship = clampF(mod->getSavedValue<double>("custom-ship", 4.0));
        f.wave = clampF(mod->getSavedValue<double>("custom-wave", 4.0));
        return f;
    }
    void save() const {
        auto* mod = Mod::get();
        mod->setSavedValue<double>("custom-cube", cube);
        mod->setSavedValue<double>("custom-ship", ship);
        mod->setSavedValue<double>("custom-wave", wave);
    }
};

// Custom Min / Max options (2.2, Stage tab, Custom stage only): in physics ticks, 240 a second
// (4 ticks = 1 frame; the Log's "+6t" is ticks). Max 80 = no limit (Extreme's 20 frames).
constexpr int MAX_TICKS_LIMIT = 80;

int stageFromSetting(std::string const& v) {
    for (int i = 0; i < STAGE_COUNT; i++) if (v == STAGES[i].setting) return i;
    return 1;
}

// Everything the engine reads. Only the stage, window size and two window
// preferences come from the user; the rest are fixed, tested defaults.
struct Config {
    int   stage        = 1;
    bool  enabled      = true;
    float window[W_COUNT] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}; // 60fps frames
    int   safetyTicks  = 1;     // extra delay margin, in 240-TPS ticks (not used in wave)
    float spamFrames   = 2.0f;  // inputs closer than this count as spam
    int   spamStreak   = 3;     // rapid inputs in a row before the buffer steps aside
    int   budgetUs     = 8000;  // CPU budget per click, microseconds (hard cap)
    bool  autoBudget   = true;  // cap the budget to a share of the measured frame time
    bool  cbfGuard     = true;  // only buffer to a delay where one tick later is also safe
    bool  orbPredict   = true;  // model yellow / pink / red / black / blue / green orbs
    bool  padPredict   = true;  // model pads (strength learned from real hits), speed + gravity portals
    bool  realTaps     = true;  // cube/ball/spider: the sim lets go after your usual tap length
    bool  accuracy     = true;  // compare predictions with what really happened
    bool  autoWindow   = false; // (feature kept in code, not exposed)
    float autoWindowMax = 4.0f;
    bool  calibReset   = false;
    bool  rayView      = false; // Tools (2.2): Show Rays
    bool  orbHelper    = false; // Tools tab (2.1): an early click waits for the next orb
    bool  scanView     = true;  // Tools tab (2.1): draw the scan's versions while it runs
    bool  learnSpots   = true;  // session memory: reuse the clicks that worked (Assists tab)
    bool  sessionMemory = true; // saved value "session-memory", copied into learnSpots above
    bool  learnedMargin = false; // (off) grow the search margin after a death - kept for reference
    bool  predictPortals = true; // (2.3) predictions go through gamemode portals (setting)
    bool  driftCheck   = true;  // (2.3) 100 ms prediction check for the Dev tab (setting)
    // (2.3) autoplay options (Tools tab, saved values)
    bool  apLearn      = true;  // Learning Run: the first run of a level with deaths off, recording what moves
    bool  apNoclip     = false; // Noclip: autoplay never dies (watch it play the whole level)
    bool  apRetry      = true;  // Retry Deaths: practice checkpoints; a death retries that jump with another input
    bool  apReplay     = true;  // Remember: replay the inputs that got it furthest (saved per level)
    bool  apMovers     = true;  // Moving Objects: predictions put moving blocks / spikes where they'll be
    bool  rawWaveSpam  = false; // (2.3) Tools: Raw Wave Spam - wave spam goes in exactly as you make it
    bool  hitboxes     = false; // (2.3) Tools: Hitboxes - your outer (spikes) and inner (blocks) box
    bool  apOverride   = true;  // Your Clicks: your clicks go in too during autoplay; good ones are kept and made again
    bool  practiceOnly = false;
    bool  showBadge    = false;
    bool  debug        = false; // Geode log (developer)
    bool  csv          = false; // CSV file (developer)

    // Window
    bool  hud          = true;  // window shown (saved value, toggled with the window key)
    float hudScale     = 1.0f;  // "Window Size" setting
    bool  hudVerbose   = false; // Log tab also lists safe clicks (saved value)

    // Extreme
    bool  extreme         = false;
    int   extremeWindow   = 80;   // base ticks @240 (20 frames) for every mode except ship/wave
    int   extremeHoldCap  = 48;   // base ticks @240 (12 frames) for ship/wave (live re-planning keeps it safe)
    bool  custom          = false; // Custom stage: your frames, Extreme's engine
    int   minTicks        = 1;     // (2.2) Custom Min: smallest shift a buffered click gets, ticks @240 (Custom only)
    int   maxTicks        = 80;    // (2.2) Custom Max: no click goes in later than this, ticks @240 (Custom only)

    static Config load() {
        auto* mod = Mod::get();
        Config c;
        c.stage = stageFromSetting(mod->getSettingValue<std::string>("stage"));
        StageDef const& st = STAGES[c.stage];
        c.enabled = st.on;
        c.extreme = st.extreme;
        for (int i = 0; i < W_COUNT; i++) c.window[i] = st.w[i];
        if (c.stage == STAGE_CUSTOM) {
            // Exactly your frames, with Extreme's engine (live re-planning,
            // no spam guard, bigger CPU share) - but no 20 / 12 frame floor.
            CustomFrames const f = CustomFrames::load();
            c.custom = true;
            for (int i : {W_CUBE, W_UFO, W_SWING, W_ROBOT}) c.window[i] = f.cube;
            for (int i : {W_SHIP, W_SHIP_REL}) c.window[i] = f.ship;
            for (int i : {W_WAVE, W_WAVE_REL}) c.window[i] = f.wave;
            c.extremeWindow = 0;
            c.extremeHoldCap = 0;
        }
        c.hudScale   = static_cast<float>(mod->getSettingValue<double>("window-size"));
        c.hud        = mod->getSavedValue<bool>("window-shown", true);
        c.hudVerbose = mod->getSavedValue<bool>("log-safe-clicks", false);
        c.rayView    = mod->getSavedValue<bool>("show-rays", false);
        c.maxTicks   = std::clamp(static_cast<int>(std::lround(mod->getSavedValue<double>("max-ticks", 1.0))), 1, MAX_TICKS_LIMIT);
        c.minTicks   = std::clamp(static_cast<int>(std::lround(mod->getSavedValue<double>("min-ticks", 1.0))), 1, c.maxTicks);
        c.orbHelper  = mod->getSavedValue<bool>("orb-helper", true); // on by default since 2.2
        c.scanView   = mod->getSavedValue<bool>("scan-view", true);
        // Session memory (Assists tab): every click that worked on this level is
        // remembered by where it landed, and later attempts fire on those spots
        // instead of deciding again. Memory only - nothing is written to disk, and
        // it is wiped when the level is opened again.
        c.sessionMemory = mod->getSavedValue<bool>("session-memory", true);
        c.learnSpots = c.sessionMemory;
        c.predictPortals = mod->getSettingValue<bool>("predict-portals");
        c.driftCheck = mod->getSettingValue<bool>("prediction-check");
        c.apLearn  = mod->getSavedValue<bool>("auto-learn", true);
        c.apNoclip = mod->getSavedValue<bool>("auto-noclip", false);
        c.apRetry  = mod->getSavedValue<bool>("auto-retry", true);
        c.apReplay = mod->getSavedValue<bool>("auto-replay", true);
        c.apMovers = mod->getSavedValue<bool>("auto-movers", true);
        c.apOverride = mod->getSavedValue<bool>("auto-override", true);
        c.rawWaveSpam = mod->getSavedValue<bool>("raw-wave-spam", false);
        c.hitboxes = mod->getSavedValue<bool>("show-hitboxes", false);
        // Accuracy tracking records the path of every prediction and checks it
        // 100ms later: diagnostics, so only with "Log: show safe clicks" (1.26.0).
        c.accuracy = c.hudVerbose;
        return c;
    }
};

// ----------------------------------------------------------------------------
//  2. Gamemode profile (P1 and, in dual, P2 combined)
//
//  Detection reads the live PlayerObject flags every click, so it is never
//  stale. "Mini" is size-based and applies to every gamemode.
// ----------------------------------------------------------------------------
struct ModeInfo {
    bool wave = false, miniWave = false, ship = false;
    bool ufo = false, swing = false, robot = false;
    bool cube = false, ball = false, spider = false;
    bool mini = false;
    bool cubeLike = false; // cube, ball or spider: one input = one action

    void add(PlayerObject* p) {
        if (!p) return;
        bool any = false;
        bool const isMini = p->m_vehicleSize > 0.f && p->m_vehicleSize < 0.99f;
        if (isMini) mini = true;
        if (p->m_isDart) {
            wave = true;
            any = true;
            if (isMini) miniWave = true;
        }
        if (p->m_isShip)  { ship  = true; any = true; }
        if (p->m_isBird)  { ufo   = true; any = true; }
        if (p->m_isSwing) { swing = true; any = true; }
        if (p->m_isRobot) { robot = true; any = true; }
        if (!any) {
            cubeLike = true;
            if (p->m_isBall)        ball = true;
            else if (p->m_isSpider) spider = true;
            else                    cube = true;
        }
    }

    // Letting go changes the trajectory.
    bool releaseMatters() const { return wave || ship || robot; }
    // Hold-based: the sim holds the button for the whole horizon, so a delayed
    // path naturally shifts the hit time and needs the stricter acceptance rule.
    bool holdBased() const { return wave || ship; }
    // No natural "landing" to end a lookahead early.
    bool noLanding() const { return wave || ship || ufo || swing; }
    // Rapid tapping is normal in these modes.
    bool spamGuarded() const { return wave || ship || ufo || swing; }

    // Lookahead length in base ticks (240 TPS). Hold-based modes use a short
    // horizon because the sim never releases, so far-away "deaths" are fake.
    int baseHorizon() const {
        if (miniWave) return 50;
        if (wave)     return 65;
        if (ship)     return 56;
        if (ufo || swing) return 90;
        return 160;
    }

    // Configured window in 60fps frames (before extreme mode).
    float windowFrames(Config const& c, bool push) const {
        float w = 1e9f;
        if (wave) w = std::min(w, c.window[push ? W_WAVE : W_WAVE_REL]);
        if (ship) w = std::min(w, c.window[push ? W_SHIP : W_SHIP_REL]);
        if (robot) w = std::min(w, c.window[W_ROBOT]);
        if (push) {
            if (ufo)      w = std::min(w, c.window[W_UFO]);
            if (swing)    w = std::min(w, c.window[W_SWING]);
            if (cubeLike) w = std::min(w, c.window[W_CUBE]);
        }
        return (w > 1e8f) ? c.window[W_CUBE] : w;
    }

    // Effective window in physics ticks. Extreme mode only ever WIDENS it.
    int windowTicks(Config const& c, bool push, float dt) const {
        int t = static_cast<int>(std::floor(windowFrames(c, push) / dt + 0.001f));
        if (c.extreme) {
            float const s = BASE_DT / dt;
            int ext = holdBased() ? c.extremeHoldCap : c.extremeWindow;
            ext = static_cast<int>(std::lround(ext * s));
            // Aceware: Extreme's big window is for presses. Letting a RELEASE
            // ride it out is what made the button stay down and kill you.
            if (push) t = std::max(t, ext);
            // (2.2) Releases get the same window as presses. They had a short
            // cap here (3 frames on Extreme, vs 12 for a wave press), so an
            // early release couldn't wait for its moment like a press can.
        }
        return t;
    }

    std::string name() const {
        std::string n;
        auto add = [&](char const* s) {
            if (!n.empty()) n += '+';
            n += s;
        };
        if (wave)   add("wave");
        if (ship)   add("ship");
        if (ufo)    add("ufo");
        if (swing)  add("swing");
        if (robot)  add("robot");
        if (cube)   add("cube");
        if (ball)   add("ball");
        if (spider) add("spider");
        if (n.empty()) n = "?";
        return mini ? ("mini " + n) : n;
    }
};

// ----------------------------------------------------------------------------
//  Per-gamemode on / off (window: Modes tab). Saved values "mode-on-<id>".
// ----------------------------------------------------------------------------
struct ModeToggle { char const* id; char const* label; };
constexpr ModeToggle MODE_TOGGLES[] = {
    {"cube", "Cube"}, {"ship", "Ship"}, {"ball", "Ball"}, {"ufo", "UFO"},
    {"wave", "Wave"}, {"robot", "Robot"}, {"spider", "Spider"}, {"swing", "Swing"},
};
constexpr int MODE_TOGGLE_COUNT = static_cast<int>(sizeof(MODE_TOGGLES) / sizeof(MODE_TOGGLES[0]));

// "cube" / "mini-cube" / "wave" ... for one player.
std::string modeId(PlayerObject* p) {
    if (!p) return "cube";
    char const* base = p->m_isDart ? "wave" : p->m_isShip ? "ship" : p->m_isBird ? "ufo" : p->m_isSwing ? "swing"
                     : p->m_isRobot ? "robot" : p->m_isBall ? "ball" : p->m_isSpider ? "spider" : "cube";
    bool const mini = p->m_vehicleSize > 0.f && p->m_vehicleSize < 0.99f;
    return mini ? fmt::format("mini-{}", base) : std::string(base);
}

bool modeOn(std::string const& id) {
    return Mod::get()->getSavedValue<bool>("mode-on-" + id, true);
}

// Which engine a gamemode uses (1.15.0):
//   * mini cube: the current one (moving hazards, near-miss waits, learn from
//     attempts, live fallback when out of CPU) - tuned on Final Destination;
//   * everything else: the 1.10.2 behaviour, which played better there.
// Both keep "a held click fires on its plan at the latest".
bool modernEngine(ModeInfo const& m) {
    return m.cube && m.mini && !(m.ship || m.wave || m.ufo || m.swing || m.robot || m.ball || m.spider);
}

// The window a click in this mode "belongs" to (auto windows, rhythm).
WinIdx primaryWin(ModeInfo const& m, bool push) {
    if (m.wave)  return push ? W_WAVE : W_WAVE_REL;
    if (m.ship)  return push ? W_SHIP : W_SHIP_REL;
    if (m.robot) return W_ROBOT;
    if (m.ufo)   return W_UFO;
    if (m.swing) return W_SWING;
    return W_CUBE;
}

char const* winLabel(int idx) {
    switch (idx) {
        case W_CUBE:     return "cube/ball/spider";
        case W_UFO:      return "ufo";
        case W_SWING:    return "swing";
        case W_ROBOT:    return "robot";
        case W_SHIP:     return "ship press";
        case W_SHIP_REL: return "ship release";
        case W_WAVE:     return "wave press";
        case W_WAVE_REL: return "wave release";
        default:         return "?";
    }
}

inline int scaleTicks(int baseTicks, float s) {
    return std::max(1, static_cast<int>(std::lround(baseTicks * s)));
}

// ----------------------------------------------------------------------------
//  3. Object classification and prediction cut-offs
//
//  The sim players only model solid blocks, hazards and slopes. Anything that
//  changes mode / gravity / size / speed, or launches the player (portals,
//  pads, orbs, speed changers) is NOT modelled. The lookahead must therefore
//  stop when the player is about to interact with one, because everything
//  after that point would be a guess.
//
//  The old code stopped whenever such an object was inside a fixed 52x72 box
//  around the player, in any direction. That also caught objects BEHIND the
//  player (e.g. the mini-size portal you had just flown through), which made
//  the buffer give up immediately and report "orb/portal - raw input".
//  Now each object is judged by its real hitbox and by whether the player can
//  still touch it (see TrajectoryBufferSim::checkStateChange).
// ----------------------------------------------------------------------------
enum class SimEnd : uint8_t {
    Survived,     // reached the horizon (or landed safely)
    Died,         // hit something lethal
    Truncated,    // reached a portal / pad / orb we can't predict past
    OutOfBudget   // ran out of CPU budget -> result is not trustworthy
};

char const* simEndName(SimEnd e) {
    switch (e) {
        case SimEnd::Survived:    return "survived";
        case SimEnd::Died:        return "died";
        case SimEnd::Truncated:   return "truncated";
        case SimEnd::OutOfBudget: return "budget";
    }
    return "?";
}

enum class ObjClass : uint8_t { Ignore, Collide, StateChange };

ObjClass classify(GameObjectType t) {
    switch (t) {
        case GameObjectType::Solid:
        case GameObjectType::Hazard:
        case GameObjectType::AnimatedHazard:
        case GameObjectType::CollisionObject:
        case GameObjectType::Slope:
            return ObjClass::Collide;

        // Objects that change mode / gravity / size / speed or launch the player.
        case GameObjectType::InverseGravityPortal:
        case GameObjectType::NormalGravityPortal:
        case GameObjectType::ShipPortal:
        case GameObjectType::CubePortal:
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::GravityPad:
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::GravityRing:
        case GameObjectType::InverseMirrorPortal:
        case GameObjectType::NormalMirrorPortal:
        case GameObjectType::BallPortal:
        case GameObjectType::RegularSizePortal:
        case GameObjectType::MiniSizePortal:
        case GameObjectType::UfoPortal:
        case GameObjectType::Modifier:
        case GameObjectType::DualPortal:
        case GameObjectType::SoloPortal:
        case GameObjectType::WavePortal:
        case GameObjectType::RobotPortal:
        case GameObjectType::TeleportPortal:
        case GameObjectType::GreenRing:
        case GameObjectType::DropRing:
        case GameObjectType::SpiderPortal:
        case GameObjectType::RedJumpPad:
        case GameObjectType::RedJumpRing:
        case GameObjectType::CustomRing:
        case GameObjectType::DashRing:
        case GameObjectType::GravityDashRing:
        case GameObjectType::SwingPortal:
        case GameObjectType::GravityTogglePortal:
        case GameObjectType::SpiderOrb:
        case GameObjectType::SpiderPad:
        case GameObjectType::TeleportOrb:
            return ObjClass::StateChange;

        default:
            return ObjClass::Ignore;
    }
}

// Orbs fire on a CLICK while overlapped; everything else fires on contact.
bool isOrb(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::GravityRing:
        case GameObjectType::GreenRing:
        case GameObjectType::DropRing:
        case GameObjectType::RedJumpRing:
        case GameObjectType::CustomRing:
        case GameObjectType::DashRing:
        case GameObjectType::GravityDashRing:
        case GameObjectType::SpiderOrb:
        case GameObjectType::TeleportOrb:
            return true;
        default:
            return false;
    }
}

// Orbs the sim can use itself: speed-only orbs, plus blue / green orbs whose
// gravity flip is applied to the sim player alone (see the flipGravity hook).
// Dash / teleport / spider / custom orbs can't be modelled.
bool isModelledOrb(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::RedJumpRing:
        case GameObjectType::DropRing:
        case GameObjectType::GravityRing:
        case GameObjectType::GreenRing:
            return true;
        default:
            return false;
    }
}

// Pads the sim can use once their strength has been seen on a real hit.
// (Spider pads teleport and are not modelled.)
bool isModelledPad(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::RedJumpPad:
        case GameObjectType::GravityPad:
            return true;
        default:
            return false;
    }
}

// Speed portals by object ID -> player speed value GD uses for them.
bool speedForPortal(int objectID, float& out) {
    switch (objectID) {
        case 200:  out = 0.7f; return true; // 0.5x
        case 201:  out = 0.9f; return true; // 1x
        case 202:  out = 1.1f; return true; // 2x
        case 203:  out = 1.3f; return true; // 3x
        case 1334: out = 1.6f; return true; // 4x
        default:   return false;
    }
}

bool isGravityPortal(GameObjectType t) {
    return t == GameObjectType::NormalGravityPortal || t == GameObjectType::InverseGravityPortal ||
           t == GameObjectType::GravityTogglePortal;
}

// Readable name for the log ("yellow orb", "size portal (mini)", ...).
char const* objectName(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpPad:        return "yellow pad";
        case GameObjectType::PinkJumpPad:          return "pink pad";
        case GameObjectType::RedJumpPad:           return "red pad";
        case GameObjectType::GravityPad:           return "gravity pad";
        case GameObjectType::SpiderPad:            return "spider pad";
        case GameObjectType::YellowJumpRing:       return "yellow orb";
        case GameObjectType::PinkJumpRing:         return "pink orb";
        case GameObjectType::RedJumpRing:          return "red orb";
        case GameObjectType::GravityRing:          return "blue orb";
        case GameObjectType::GreenRing:            return "green orb";
        case GameObjectType::DropRing:             return "black orb";
        case GameObjectType::CustomRing:           return "custom orb";
        case GameObjectType::DashRing:             return "dash orb";
        case GameObjectType::GravityDashRing:      return "gravity dash orb";
        case GameObjectType::SpiderOrb:            return "spider orb";
        case GameObjectType::TeleportOrb:          return "teleport orb";
        case GameObjectType::InverseGravityPortal: return "gravity portal (up)";
        case GameObjectType::NormalGravityPortal:  return "gravity portal (down)";
        case GameObjectType::GravityTogglePortal:  return "gravity toggle";
        case GameObjectType::ShipPortal:           return "ship portal";
        case GameObjectType::CubePortal:           return "cube portal";
        case GameObjectType::BallPortal:           return "ball portal";
        case GameObjectType::UfoPortal:            return "ufo portal";
        case GameObjectType::WavePortal:           return "wave portal";
        case GameObjectType::RobotPortal:          return "robot portal";
        case GameObjectType::SpiderPortal:         return "spider portal";
        case GameObjectType::SwingPortal:          return "swing portal";
        case GameObjectType::InverseMirrorPortal:
        case GameObjectType::NormalMirrorPortal:   return "mirror portal";
        case GameObjectType::RegularSizePortal:    return "size portal (normal)";
        case GameObjectType::MiniSizePortal:       return "size portal (mini)";
        case GameObjectType::DualPortal:           return "dual portal";
        case GameObjectType::SoloPortal:           return "solo portal";
        case GameObjectType::TeleportPortal:       return "teleport portal";
        case GameObjectType::Modifier:             return "speed portal/trigger";
        case GameObjectType::Hazard:               return "spike";
        case GameObjectType::AnimatedHazard:       return "saw";
        case GameObjectType::Solid:                return "block";
        case GameObjectType::Slope:                return "slope";
        default:                                   return "object";
    }
}

// Used only if an object reports a nonsensical hitbox.
struct Extent { float hw, hh; };
Extent fallbackExtent(GameObjectType t) {
    switch (t) {
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::GravityPad:
        case GameObjectType::RedJumpPad:
        case GameObjectType::SpiderPad:
            return {17.f, 8.f};
        default:
            break;
    }
    if (isOrb(t)) return {18.f, 18.f};
    return {16.f, 46.f}; // portals, speed changers, modifiers
}

// What cut a lookahead short.
struct TruncInfo {
    bool           valid = false;
    GameObjectType type  = GameObjectType::Decoration;
    float          gap   = 0.f; // px between the player's leading edge and the object (<= 0: touching)
    int            tick  = -1;  // (2.2) sim step it was found on
    GameObject*    obj   = nullptr; // (2.2) the object (only used while the level is open)
};

std::string describeTrunc(TruncInfo const& t) {
    if (!t.valid) return "orb/portal ahead";
    if (t.gap > 0.5f) return fmt::format("{} {:.0f}px ahead", objectName(t.type), t.gap);
    return fmt::format("{} (touching)", objectName(t.type));
}

struct SimResult {
    int       ticks = 0;     // ticks survived, counted from the moment the input lands
    SimEnd    end   = SimEnd::Survived;
    TruncInfo trunc;
    int       landTick = -1; // cube/ball/spider/robot: first tick back on the ground after the jump
    // Closest the path came to a hazard (px) during the part the click is
    // judged on: until the landing (+ grace), or the first 100 ms in ship /
    // wave / UFO / swing. 1e9 = no hazard nearby.
    float     clear = 1e9f;
    int       killType = -1;  // what killed it (GameObjectType), -1 unknown
    float     killDy = 0.f;   // height at death relative to the start (px)
    bool      orbUsed = false; // the test used a predicted orb
    RingObject* firstRing = nullptr; // first unused orb you touch in this test (GD's contact)
    int       firstRingTick = -1;    // ...on this step (from the start, incl. the delay)
    int       firstRingLast = -1;    // last step of that contact (in a row)
    int       nextRingTick = -1;     // first step you overlap ANOTHER orb after using one
    int       deathStep = -1;  // step (from the start, incl. the delay) it died on
    float     killPen = -1.f;  // (2.3) how deep into what killed it (px, the smaller of the two overlaps): a near miss is small
    float     killX = 0.f, killY = 0.f; // (2.3) where what killed it is
};
// Extra inputs injected into a test at fixed steps (-1 = none).
struct FollowUp {
    // (2.3) up to 8 inputs (the escape search plays a few follow-ups after
    // your known ones); -1 = unused. Ticks count from the start of the test.
    static constexpr int N = 8;
    int  at[N]   = {-1, -1, -1, -1, -1, -1, -1, -1};
    bool held[N] = {false, false, false, false, false, false, false, false};
    int count() const {
        int n = 0;
        while (n < N && at[n] >= 0) n++;
        return n;
    }
    bool add(int t, bool h) {
        int const n = count();
        if (n >= N || t < 0) return false;
        at[n] = t;
        held[n] = h;
        return true;
    }
};

// Per sim player: where it started and how it moves, so objects can be judged
// "ahead" or "behind" and "already touched" or "not yet touched".
struct SimTrack {
    float startX = 0.f;
    float dir    = 1.f; // +1 moving right, -1 moving left
    float stepPx = 0.f; // distance covered per tick
};

// ----------------------------------------------------------------------------
//  4. Decisions, death reasons, stats
// ----------------------------------------------------------------------------
enum class Decision : uint8_t {
    None,
    PassSafe,       // frame 0 survives the horizon
    PassTruncated,  // frame 0 reached an orb / pad / portal (can't predict)
    PassSpam,       // spam guard bypass
    PassRelease,    // release in a mode where release doesn't matter
    PassNoWindow,   // window is shorter than one physics tick
    PassBudget,     // out of CPU budget, fell back to raw input
    PassNoFix,      // frame 0 predicted dead, no delay in the window clears it
    PassEscapable, // frame 0 dies only if you never input again: you can still save it
    Buffered,       // input delayed
    Queued,         // opposite input queued behind a pending buffer
    PassModeOff,    // this gamemode is switched off in the Modes tab
    Count
};

char const* decisionName(Decision d) {
    switch (d) {
        case Decision::None:          return "none";
        case Decision::PassSafe:      return "pass_safe";
        case Decision::PassTruncated: return "pass_truncated";
        case Decision::PassSpam:      return "pass_spam";
        case Decision::PassRelease:   return "pass_release";
        case Decision::PassNoWindow:  return "pass_nowindow";
        case Decision::PassBudget:    return "pass_budget";
        case Decision::PassNoFix:     return "pass_nofix";
        case Decision::PassEscapable: return "pass_escape";
        case Decision::Buffered:      return "buffered";
        case Decision::Queued:        return "queued";
        case Decision::PassModeOff:   return "pass_modeoff";
        default:                      return "?";
    }
}

enum class DeathReason : uint8_t {
    NoRecentClick,    // died with no click in the horizon
    SimMispredict,    // sim said safe, real died inside the horizon
    LaterObstacle,    // died after the horizon
    Truncated,        // orb / pad / portal blocked the prediction
    SpamBypass,       // spam guard stepped aside
    WindowTooSmall,   // sim saw the death, window couldn't fix it
    OutOfBudget,      // CPU budget fallback
    BufferedStillDied,// delayed click still died
    EscapeNotTaken, // click was fine if followed up, the follow-up didn't happen in time
    Other,
    Count
};

char const* deathName(DeathReason r) {
    switch (r) {
        case DeathReason::NoRecentClick:     return "no_recent_click";
        case DeathReason::SimMispredict:     return "sim_mispredict";
        case DeathReason::LaterObstacle:     return "later_obstacle";
        case DeathReason::Truncated:         return "truncated";
        case DeathReason::SpamBypass:        return "spam_bypass";
        case DeathReason::WindowTooSmall:    return "window_too_small";
        case DeathReason::OutOfBudget:       return "out_of_budget";
        case DeathReason::BufferedStillDied: return "buffered_still_died";
        case DeathReason::EscapeNotTaken:    return "escape_not_taken";
        case DeathReason::Other:             return "other";
        default:                             return "?";
    }
}

// Short human text for the on-screen log.
char const* deathText(DeathReason r) {
    switch (r) {
        case DeathReason::NoRecentClick:     return "no recent click";
        case DeathReason::SimMispredict:     return "sim said safe (mispredict)";
        case DeathReason::LaterObstacle:     return "obstacle past lookahead";
        case DeathReason::Truncated:         return "orb/pad/portal cut prediction";
        case DeathReason::SpamBypass:        return "spam guard bypassed buffer";
        case DeathReason::WindowTooSmall:    return "window too small";
        case DeathReason::OutOfBudget:       return "CPU budget fallback";
        case DeathReason::BufferedStillDied: return "buffered click still died";
        case DeathReason::EscapeNotTaken:    return "needed a follow-up input";
        case DeathReason::Other:             return "other";
        default:                             return "?";
    }
}

struct Stats {
    int byDecision[static_cast<int>(Decision::Count)] = {};
    int byDeath[static_cast<int>(DeathReason::Count)] = {};
    int clicks = 0;
    int deaths = 0;
    int simClicks = 0;
    long long sumUs = 0;
    long long maxUs = 0;
};

struct ClickRecord {
    int       needed   = -1; // no-fix probe: ticks the click needed (-1 not probed, 0 not found)
    int       probe    = 0;  // how far the probe looked
    int       window   = 0;  // window in ticks at the time
    Decision  decision = Decision::None;
    SimEnd    f0End    = SimEnd::Survived;
    int       f0Ticks  = 0;
    int       delay    = 0;
    int       horizon  = 0;
    double    time     = -1.0; // game time (60fps frames) when the click arrived
    TruncInfo trunc;
    int       liveTicks = -1;   // buffered: when it really fired (ticks after the click), -1 not yet
    acecore::FireReason liveReason = acecore::FireReason::None;
};

// Everything the logs need to know about one click.
struct ClickNote {
    bool      push        = true;
    Decision  d           = Decision::None;
    int       windowTicks = 0;
    int       horizon     = 0;
    long long us          = 0;
    SimResult r0;
    int       delay       = 0;
    int       evals       = 0; // delays simulated by the search
    int       needed      = -1; // no-fix probe: -1 not run, 0 nothing helps, >0 ticks needed
    std::string modeName;       // for the log
    bool      orb         = false; // the chosen path uses a predicted orb
    bool      stream      = false; // wave spam: kept the stream's steady shift
    bool      rescue      = false; // no safe moment found by the quick search: held, checked every tick
    bool      portal      = false; // held until past a gamemode / size portal (its own log line)
    bool      orbHelp     = false; // held by the orb helper until the next orb (2.1)
};

// ----------------------------------------------------------------------------
//  5. CSV log (bounded size, buffered writes, only touched when enabled)
// ----------------------------------------------------------------------------
class CsvLog {
    std::ofstream m_out;
    int m_pending = 0;

    void openIfNeeded() {
        if (m_out.is_open()) return;
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path path = Mod::get()->getSaveDir() / "buffer-log.csv";
        bool fresh = true;
        if (fs::exists(path, ec)) {
            auto size = fs::file_size(path, ec);
            if (!ec && size > 0 && size < MAX_LOG_BYTES) fresh = false;
        }
        m_out.open(path, fresh ? std::ios::trunc : std::ios::app);
        if (m_out.is_open() && fresh) {
            m_out << "event,t_ms,mode,input,tps,window_ticks,horizon,sim_us,f0_end,f0_ticks,decision,delay,extra\n";
        }
    }

public:
    void row(std::string const& line) {
        openIfNeeded();
        if (!m_out.is_open()) return;
        m_out << line << '\n';
        if (++m_pending >= 32) flush();
    }

    void flush() {
        if (m_out.is_open()) m_out.flush();
        m_pending = 0;
    }

    void close() {
        if (m_out.is_open()) {
            m_out.flush();
            m_out.close();
        }
        m_pending = 0;
    }
};

// ----------------------------------------------------------------------------
//  6. Colors shared by the window and the log.
//     The window itself (section 9) is defined after the engine; the engine
//     only asks it to exist via windowEnsure().
// ----------------------------------------------------------------------------
const ccColor3B COL_WHITE  = {235, 235, 235};
const ccColor3B COL_GREY   = {165, 165, 165};
const ccColor3B COL_GREEN  = {110, 255, 130};
const ccColor3B COL_YELLOW = {255, 225, 90};
const ccColor3B COL_ORANGE = {255, 160, 60};
const ccColor3B COL_RED    = {255, 95, 95};
const ccColor3B COL_CYAN   = {110, 220, 255};
const ccColor3B COL_LAV    = {198, 166, 255}; // section titles (window theme)
const ccColor3B COL_DIM    = {120, 110, 160};

// Log tags (1.26.1): every Log line is "time  TAG  text"; the tag's colour says
// what kind of line it is at a glance.
ccColor3B logTagColor(std::string const& tag) {
    if (tag == "DEATH")  return {255, 95, 95};
    if (tag == "BUFFER") return {110, 255, 130};
    if (tag == "FIRED")  return {80, 220, 170};
    if (tag == "QUEUED") return {110, 200, 255};
    if (tag == "RAW")    return {255, 170, 80};
    if (tag == "HOLD")   return {110, 200, 255};
    if (tag == "OK")     return {150, 150, 170};
    if (tag == "CHECK")  return {255, 225, 90};
    if (tag == "ORB")    return {255, 205, 90};
    if (tag == "SNAP")   return {120, 230, 255};
    return {198, 166, 255}; // NOTE
}

void windowEnsure();   // defined with BufferWindow (section 9)

// ----------------------------------------------------------------------------
//  Log console (2.2, Windows): the Log in its own command-prompt window
//  instead of the window's Log tab. Its close button is taken away - closing
//  a console window would close the game with it; the Log tab's "console"
//  button closes it.
// ----------------------------------------------------------------------------
struct LogConsole {
    static inline bool s_on = false;
    static inline bool s_ours = false; // we opened it (Geode's own console stays)
#ifdef GEODE_IS_WINDOWS
    static inline HANDLE s_out = nullptr;
    static BOOL WINAPI ctrlHandler(DWORD type) {
        return (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) ? TRUE : FALSE; // Ctrl+C doesn't kill the game
    }
#endif
    static bool available() {
#ifdef GEODE_IS_WINDOWS
        return true;
#else
        return false;
#endif
    }
    static bool on() { return s_on; }
    static bool open() {
#ifdef GEODE_IS_WINDOWS
        if (s_on) return true;
        if (!GetConsoleWindow()) {
            if (!AllocConsole()) return false;
            s_ours = true;
        }
        s_out = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, 0, nullptr);
        if (s_out == INVALID_HANDLE_VALUE) s_out = nullptr;
        SetConsoleTitleA("Aceware - Log");
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCtrlHandler(ctrlHandler, TRUE);
        if (HWND w = GetConsoleWindow()) {
            if (HMENU m = GetSystemMenu(w, FALSE)) DeleteMenu(m, SC_CLOSE, MF_BYCOMMAND);
        }
        s_on = true;
        write("Aceware log - every buffered click, release and death shows up here.", {95, 211, 160});
        write("Close it with the console button on the Log tab (closing this window would close the game).", {150, 150, 170});
        return true;
#else
        return false;
#endif
    }
    static void close() {
#ifdef GEODE_IS_WINDOWS
        if (!s_on) return;
        s_on = false;
        if (s_out) CloseHandle(s_out);
        s_out = nullptr;
        if (s_ours) {
            FreeConsole();
            s_ours = false;
        } else if (HWND w = GetConsoleWindow()) {
            GetSystemMenu(w, TRUE); // Geode's console: give its close button back
        }
#endif
    }
    static void write(std::string const& line, ccColor3B col) {
#ifdef GEODE_IS_WINDOWS
        if (!s_on || !s_out) return;
        WORD attr = 0;
        if (col.r > 140) attr |= FOREGROUND_RED;
        if (col.g > 140) attr |= FOREGROUND_GREEN;
        if (col.b > 140) attr |= FOREGROUND_BLUE;
        if (attr == 0) attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        if (std::max({col.r, col.g, col.b}) > 200) attr |= FOREGROUND_INTENSITY;
        SetConsoleTextAttribute(s_out, attr);
        std::string const text = line + "\n";
        DWORD n = 0;
        WriteConsoleA(s_out, text.data(), static_cast<DWORD>(text.size()), &n, nullptr);
        SetConsoleTextAttribute(s_out, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
        (void)line;
        (void)col;
#endif
    }
};

// ----------------------------------------------------------------------------
//  7. TrajectoryBufferSim - the engine
//
//  Flow of one click (interceptInput):
//     read live gamemode  ->  spam guard  ->  simulate "click now" (frame 0)
//       safe?            pass the click through untouched
//       dies?            simulate the same click delayed by 1..window ticks,
//                        keep the smallest delay that survives, hold the click
//                        and re-fire it from onPlayerUpdate when it is due
//       cut by an orb /  pass the click through (we cannot see past it) -
//       pad / portal?    the log says exactly which object did it
// ----------------------------------------------------------------------------
class TrajectoryBufferSim {
    Ref<PlayerObject> m_simPlayer1;
    Ref<PlayerObject> m_simPlayer2;
    gd::vector<GameObject*> m_scratch; // reused every sim step (no per-step allocation)

    // Clearance (v1.11)
    float  m_tickClr  = 1e9f;   // closest hazard in the current sim step (px)
    bool   m_simModern = false; // this test measures near-miss clearance (current engine)
    int    m_simKillType = -1;  // what killed the sim player (object type, -1 unknown)
    float  m_simStartY = 0.f, m_simKillDy = 0.f, m_simKillPen = -1.f;
    double m_simNow   = 0.0;    // game time the test starts at
    float  m_simDtCur = BASE_DT;// step length of the current test
    long long m_cleanWaits  = 0; // live: waited for a cleaner moment

    // Spot memory (v1.12): where inputs landed on earlier attempts of this level
    acecore::SpotMemory     m_spots;
    acecore::OutcomeTracker m_outcome;
    double m_outcomeWindow = SPOT_WINDOW_HOLD;
    bool   m_outcomeLand   = false; // judged on landing (cube-type)
    bool   m_outcomeAir    = false; // left the ground since the input
    double m_outcomeLandAt = -1.0;
    float  m_p1Speed = 0.f, m_p1Dir = 1.f; // real P1 horizontal speed (px / frame), direction
    float  m_p1LastX = 0.f;
    double m_p1LastT = -1.0;
    long long m_spotFires = 0, m_spotBlocks = 0, m_spotSaves = 0;
    std::filesystem::path m_spotFile;  // <save dir>/spots/<level>.txt ("" = not saved)
    std::string m_spotLevelName;
    bool   m_spotsDirty  = false;
    size_t m_spotsLoaded = 0;          // spots that came from earlier sessions
    bool      m_liveWaited  = false;
    int       m_liveWaitWhy = 0; // 1 near miss, 2 waiting for a learned spot
    // Why the live check said "not now" for the held click (diagnostics, per click)
    enum Refuse { RF_DIED, RF_CUT, RF_SPOT, RF_CLEAN, RF_AHEAD, RF_COUNT };
    int       m_refuse[RF_COUNT] = {};
    SimResult m_refuseFirst;      // the first "died" test: how / when it died
    bool      m_refuseHave = false;

    bool m_simulating     = false;
    bool m_firingBuffered = false;
    bool m_simDead        = false;
    TruncInfo m_simTrunc;       // set when the player is about to touch something we can't model
    bool  m_recordPath  = false;         // keep the P1 path of each test (accuracy / overlay)
    std::vector<acecore::Point2> m_path;  // path of the last test
    bool  m_simHeld     = false; // button state inside the current test
    int   m_simTick     = 0; // step index inside the current test
    int   m_simDelay    = 0; // step at which the tested input lands
    bool  m_orbPredict  = false; // this click may model velocity orbs
    int   m_orbIntent   = 0; // steps after the input in which an orb counts as its target
    bool  m_simOrbUsed  = false; // this test already used an orb
    int   m_orbClick    = 3;     // UFO / swing: steps after a press that still count as a click on an orb
    int   m_simPressTick = -1;   // step of the last press inside the current test (-1 none)
    RingObject* m_tickRing[2] = {nullptr, nullptr}; // orb GD says each sim player touches this step
    RingObject* m_prevRing[2] = {nullptr, nullptr}; // orb each sim player touched last step (or the real one now)
    bool  m_gdRings = false;     // GD reports orb contact for sim players (exact orbs on)
    GameObject* m_simOrbObj = nullptr; // ... this one (ignored for the rest of the test)
    std::vector<std::pair<GameObject*, bool>> m_objRestore; // activation flags to put back
    std::vector<GameObject*> m_simUsed;  // pads / portals already applied in this test
    bool  m_padPredict  = false;         // this click may model pads and speed / gravity portals

    // Pad strength as GD passes it to bumpPlayer, learned per pad type from real
    // hits. A type whose strength varied between hits is not predicted.
    struct PadLearn { bool seen = false; bool unstable = false; float bump = 0.f; };
    PadLearn m_pads[64];
    bool     m_padsDirty = false;
    SimTrack  m_track[2];       // [0] = sim player 1, [1] = sim player 2

    // Real physical button state vs what the game currently has.
    bool m_physicalHolding = false;
    bool m_gameHolding     = false;
    bool m_enabled         = true;

    // ---- Game clock --------------------------------------------------------
    // Everything below is scheduled in GAME TIME (60fps-frame units, time warp
    // removed), not in update calls. Click Between Frames splits a frame into
    // extra, shorter physics steps when a click lands mid-frame, so counting
    // update calls would make a "3 tick" delay fire early. Time can't drift.
    double m_gameTime = 0.0;
    // Buffer state: the held input and everything that arrives while it is
    // held, replayed in order with the same shift (see BufferCore.hpp).
    acecore::InputQueue m_queue;

    // Live re-planning: while an input is held back, every tick tests "fire it
    // now?" from the REAL game state and fires at the first safe tick, instead
    // of trusting a delay predicted at click time (long Extreme delays drift).
    // Every input queued behind it gets the same check when it is next.
    bool   m_live = false;          // the live re-planner owns the queue head
    acecore::LiveState m_liveState;  // see BufferCore.hpp (tested there)
    double m_liveArrive = 0.0;      // when the triggering click arrived
    int    m_livePlanTicks = 0;     // planned delay, ticks (for the log)

    // Nominal tick length. CBF sub-steps are shorter, so the nominal tick is the
    // largest of the last few. m_warp is the level's time warp (sim steps use dt*warp).
    float m_frameDt = BASE_DT; // 60fps-frame units per physics tick
    float m_warp    = 1.0f;
    float m_dtRing[DT_RING] = {};
    int   m_dtIdx = 0;

    // Frame-time meter (wall clock between rendered frames) for the auto budget.
    Clock::time_point m_lastFrameAt{};
    bool  m_haveFrame = false;
    float m_frameUs   = 1e6f / 60.f; // smoothed
    int   m_lastBudgetUs = 0;        // what the last click was allowed (for the window)
    bool  m_cbfLoaded = false;

    // Live accuracy: prediction vs reality. A buffered click's check starts
    // when it actually fires, so it is aligned to the real step.
    acecore::PredictionTracker m_tracker;

    // Auto windows (kept across sessions).
    acecore::Calibrator m_calib[W_COUNT];

    // Input rhythm per mode (this session): how long you hold, and how long you
    // stay released before pressing again. Feeds the cube tap model and the
    // ship / wave "could you still save it" cycle.
    acecore::Calibrator m_holdLen[W_COUNT];
    acecore::Calibrator m_gapLen[W_COUNT];
    double m_lastPressAt   = -1.0;
    double m_lastReleaseAt = -1.0;

    // Accuracy per mode name ("cube", "mini wave", ...).
    struct AccCount { int checks = 0; int hits = 0; int deaths = 0; double errSum = 0.0; double dxSum = 0.0, dySum = 0.0; };
    std::map<std::string, AccCount> m_accByMode;
    std::string m_trackMode;        // mode of the check in progress
    float m_autoW[W_COUNT] = {};            // auto window per mode (0 = still learning)
    bool  m_calibDirty = false;

    // Trajectory overlay and badge
    Ref<CCDrawNode> m_overlay;

    // ---- 2.1: orb helper, click guide, saved stats, report ------------------
    bool        m_orbHelpHold = false;         // the held click waits for this orb
    RingObject* m_orbHelpRing = nullptr;
    // Level scan (2.1): the route the scanner found, one entry per input, with
    // the stretch (x0..x1) where that input still works. Saved per level.
    struct RoutePt {
        float x = 0.f, y = 0.f;   // where the input goes in
        bool  push = true;
        float x0 = 0.f, x1 = 0.f; // it works anywhere in here (same path)
        std::string mode;         // ModeInfo::name() when it was found
    };
    std::vector<RoutePt> m_route;              // this level, sorted by x
    std::filesystem::path m_routeFile;
    std::vector<acecore::Point2> m_routePath;   // the scan's path itself (for curved guide lines), sorted by x
    std::vector<std::pair<float, float>> m_scanned; // stretches a scan already went through
    int         m_routeOff = 0;                // route checks in a row far off the scanned path
    struct SavedStats {
        long long attempts = 0, deaths = 0, clicks = 0, buffered = 0, completions = 0;
        float best = 0.f; // best % in normal mode
    };
    SavedStats  m_lvlStats, m_lifeStats;
    std::string m_lvlKey;                      // "online-123" / "local-xxxxxxxx"
    std::string m_lvlName;
    bool        m_attemptPending = false;      // count an attempt on the next physics step
    bool        m_forceDetails = false;        // statsRows: the Details section too (report)
    std::deque<std::string> m_trace;           // every click / log line, for the report
    bool  m_overlayShown = false;
    Ref<CCLabelBMFont> m_badge;
    std::string m_badgeText;
    bool  m_active = true;                  // enabled and allowed here (practice-only)

    bool m_deathReported = false;  // this death already counted (until respawn)
    int  m_directFires = 0;        // buffered inputs another mod held back (applied directly)
    bool m_directMode = false;     // session-wide: apply buffered inputs straight to the player

    // Last input treated as yours (to tell GD's player-2 mirror apart).
    long long m_lastPrimaryTick = -1;
    bool      m_lastPrimaryPush = false;

    // Spam guard
    double m_lastInputTime = -1e9;
    // Performance (1.18.0): live re-checks
    double    m_lastLiveAt = -1e9;
    Clock::time_point m_lastStepWall{};      // last physics-step hook call (wall clock)
    Clock::time_point m_pendingSinceWall{};  // when the current held input started waiting
    long long m_watchdogFires = 0;
    long long m_simStepsTotal = 0;   // sim steps run (for us-per-step in Stats)
    long long m_cheapClicks = 0;     // clicks finished with cheap checks (out of time)
    long long m_inputsSeen = 0;      // inputs that reached the buffer this level
    // One-tick self-check (1.23.0): predicted vs real state after one tick.
    struct OneStep { int n = 0, exact = 0; double sdx = 0, sdy = 0, sdv = 0; float worst = 0.f; bool logged = false; };
    std::map<std::string, OneStep> m_oneStep;
    bool   m_osPending = false, m_osInput = false;
    // Orb press (2.3): a press on an orb uses the orb on the press tick, before
    // the step, with no flap - the model the old orb press check settled on
    // (0.2px from the game). The other two models and the check are gone.
    bool   m_orbModelLoaded = false;
    // what the prediction's orb did (this test)
    int    m_simOrbStep = -1;       // step the sim used its orb on
    // What each orb really does (1.24.6), learned from your own orb hits and
    // saved: the y-speed right after it and whether it flipped gravity. The
    // sim players' own orb call turned out to do nothing (pink and green orbs
    // both predicted 6.87 = a plain flap; the game gave 4.46 and -10.62).
    struct OrbFact { double vySum = 0.0; int n = 0; int flips = 0; };
    std::map<int, OrbFact> m_orbTable;
    bool   m_orbTableDirty = false;
    int    m_orbTableUses = 0; // predictions that used a learned orb
    float  m_osX = 0.f, m_osY = 0.f;
    double m_osVy = 0.0;
    std::string m_osMode;
    float  m_lastStepD = 0.f;        // length of the last real P1 step (60fps-frame units)
    float  m_stepAccum = 0.f;        // time into the current physics tick (60fps-frame units)
    float  m_phase = 0.f;            // ... as a fraction of a tick (0 = on a tick boundary)
    bool      m_liveRescue = false;  // the held click is a rescue (checked every tick, no plan)
    bool      m_liveSpotHold = false; // the held click waits for a learned spot (no prediction)
    // Follow the plan (1.26.1): a held click fires on the click-time plan while
    // you stay on the path the plan predicted - checked by position, no
    // prediction per tick. Off the path: the full per-tick check takes over.
    bool      m_planTrust = false;
    std::vector<acecore::Point2> m_planPath; // predicted position after each step from the click
    double    m_planGrid = 0.0;             // tick grid start of that click
    long long m_planFires = 0, m_planDrifts = 0, m_planTicksSaved = 0;
    // Orbs the real player used this attempt (1.24.9). GD's m_isActivated is set
    // as soon as you TOUCH an orb, so it can't tell which orbs are really used.
    std::vector<GameObject*> m_realUsedOrbs;
    RingObject* m_simFirstRing = nullptr;
    int       m_simFirstRingTick = -1;
    int       m_simFirstRingLast = -1;
    int       m_simNextRingTick = -1;
    float     m_spotTargetX = 0.f, m_spotTargetDir = 1.f; // where that spot is (level x) and travel direction
    long long m_hookMissing = 0;
    int       m_framesSinceStep = 0; // rendered frames since the last physics step (alive, not paused)
    long long m_followOnTime = 0;   // wave / ship follow-ups that went in on time (2.1)
    long long m_followSpacing = 0;  // ... that kept the held input's spacing     // ... passed raw because the physics hook wasn't running
    long long m_simUsTotal = 0;
    long long m_liveUsSum = 0, m_liveChecks = 0, m_liveUsMax = 0;
    // Wave spam ("stream"): one steady shift for the whole stream (1.16.0)
    double m_streamLast  = -1e9; // last wave input (game time)
    double m_streamShift = 0.0;  // shift of the stream's inputs so far (game time)
    int    m_streak        = 0;
    // Press timestamps (game time, 60fps frames) for the "how fast is he clicking"
    // estimate that decides whether Raw Wave Spam may take over.
    static constexpr int PRESS_RATE_N = 12;
    double m_pressAt[PRESS_RATE_N] = {};
    int    m_pressAtN = 0; // samples stored (caps at PRESS_RATE_N)
    int    m_pressAtI = 0; // next slot

    // CPU budget controller
    float m_horizonScale = 1.0f;
    bool  m_hasDeadline  = false;
    Clock::time_point m_deadline;

    // Config cache (re-read at most every 150 ms instead of on every click)
    Config m_cfg;
    Clock::time_point m_cfgAt{};
    bool m_cfgValid = false;

    // Logging / diagnostics
    bool m_logDebug = false;
    bool m_logCsv   = false;
    CsvLog m_csv;

    // Full log for the window's Log tab.
    struct HistEntry {
        std::string tag;    // BUFFER / FIRED / DEATH / RAW / ... (see logTagColor)
        std::string text;
        ccColor3B   color;
        float       t;      // seconds since level start
        int         repeat;
        bool        minor;  // safe pass / irrelevant release (hidden unless "show safe clicks")
    };
    static constexpr size_t MAX_HISTORY = 300;
    std::deque<HistEntry> m_history;
    Stats m_stats;
    ClickRecord m_last;
    std::string m_lastMode;
    long long m_tick = 0;          // update-call counter (death de-dupe, housekeeping)
    long long m_lastDeathTick = -1;
    Clock::time_point m_levelStart = Clock::now();

    // ------------------------------------------------------------------------
    //  Config cache
    // ------------------------------------------------------------------------
    Config const& cfg() {
        auto const now = Clock::now();
        if (!m_cfgValid || now - m_cfgAt > std::chrono::milliseconds(150)) {
            m_cfg = Config::load();
            m_cfgAt = now;
            m_cfgValid = true;
        }
        return m_cfg;
    }

    void invalidateCfg() { m_cfgValid = false; }

    // Enabled, and allowed in this attempt (practice-only mode).
    static bool allowedHere(Config const& c, PlayLayer* pl) {
        if (!c.enabled) return false;
        if (c.practiceOnly && !(pl && pl->m_isPracticeMode)) return false;
        return true;
    }

    void refreshFlags() {
        Config const& c = cfg();
        m_active   = allowedHere(c, PlayLayer::get());
        m_enabled  = m_active;
        m_logDebug = c.debug;
        m_logCsv   = c.csv;
    }

    // Settings with the auto windows applied (when on and learned).
    Config effectiveCfg(Config const& c) const {
        Config e = c;
        if (c.autoWindow) {
            for (int i = 0; i < W_COUNT; i++) if (m_autoW[i] > 0.f) e.window[i] = m_autoW[i];
        }
        return e;
    }

    // ------------------------------------------------------------------------
    //  Auto windows
    // ------------------------------------------------------------------------
    void refreshAuto(Config const& c, int w) {
        auto win = m_calib[w].window(AUTO_MIN_SAMPLES, AUTO_PERCENTILE, AUTO_MARGIN_FRAMES,
                                     AUTO_MIN_FRAMES, std::max(AUTO_MIN_FRAMES, c.autoWindowMax));
        m_autoW[w] = win.value_or(0.f);
    }

    // Median of a rhythm sample set, or a default until there are enough.
    static float rhythm(acecore::Calibrator const& c, float def) {
        return c.count() >= RHYTHM_MIN_SAMPLES ? c.percentile(0.5f) : def;
    }

    // A buffered click needed `frames` (60fps frames) of delay to survive.
    void recordSave(Config const& c, int w, float frames) {
        m_calib[w].add(frames);
        m_calibDirty = true;
        refreshAuto(c, w);
    }

    // A click was too early even for the window (frames < 0: probe found nothing).
    void recordMiss(Config const& c, int w, float frames) {
        if (frames > 0.f) {
            m_calib[w].add(frames);
            m_calibDirty = true;
            refreshAuto(c, w);
        }
    }

    // Calibration + learned pad strengths live in the mod's saved values.
    void loadPersisted(Config const& c) {
        auto* mod = Mod::get();
        if (c.calibReset) {
            for (int i = 0; i < W_COUNT; i++) {
                m_calib[i].clear();
                mod->setSavedValue<std::string>(std::string("calib-") + WINDOW_KEYS[i], "");
            }
            invalidateCfg();
            log::info("[Aceware] calibration data cleared");
        } else {
            for (int i = 0; i < W_COUNT; i++) {
                m_calib[i].deserialize(mod->getSavedValue<std::string>(std::string("calib-") + WINDOW_KEYS[i], ""));
            }
        }
        for (int i = 0; i < W_COUNT; i++) refreshAuto(c, i);

        // pads: "type:bump:unstable;..."
        for (auto& pd : m_pads) pd = PadLearn{};
        std::string const pads = mod->getSavedValue<std::string>("pad-strengths", "");
        size_t pos = 0;
        while (pos < pads.size()) {
            size_t const end = pads.find(';', pos);
            std::string const tok = pads.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            // "type:bump:unstable" (parsed by hand: sscanf is deprecated on MSVC)
            char const* p = tok.c_str();
            char* q = nullptr;
            long const type = std::strtol(p, &q, 10);
            if (q && *q == ':') {
                float const bump = std::strtof(q + 1, &q);
                if (q && *q == ':') {
                    long const unstable = std::strtol(q + 1, &q, 10);
                    if (type >= 0 && type < 64) m_pads[type] = PadLearn{true, unstable != 0, bump};
                }
            }
            if (end == std::string::npos) break;
            pos = end + 1;
        }
        m_calibDirty = false;
        m_padsDirty = false;
    }

    void savePersisted() {
        auto* mod = Mod::get();
        if (m_calibDirty) {
            for (int i = 0; i < W_COUNT; i++) {
                mod->setSavedValue<std::string>(std::string("calib-") + WINDOW_KEYS[i], m_calib[i].serialize());
            }
            m_calibDirty = false;
        }
        if (m_padsDirty) {
            std::string out;
            for (int t = 0; t < 64; t++) {
                if (!m_pads[t].seen) continue;
                out += fmt::format("{}:{:.4f}:{};", t, m_pads[t].bump, m_pads[t].unstable ? 1 : 0);
            }
            mod->setSavedValue<std::string>("pad-strengths", out);
            m_padsDirty = false;
        }
    }

    // ------------------------------------------------------------------------
    //  Trajectory overlay + badge
    // ------------------------------------------------------------------------

public:
    // ------------------------------------------------------------------------
    //  2.1: report trace, saved stats, click guide
    // ------------------------------------------------------------------------
    static constexpr size_t MAX_TRACE = 2000;
    // (2.2) ground / band checks: their own list, so the scan's many trace
    // lines can't push them out before an export
    std::vector<std::string> m_groundLines;
    void groundLog(std::string line) {
        if (m_groundLines.size() < 200) m_groundLines.push_back(line);
        trace(std::move(line));
    }
    void trace(std::string line) {
        // console + "Log: more detail": every click's decision too (log lines are shown already)
        if (LogConsole::on() && line.rfind("log ", 0) != 0 && cfg().hudVerbose) {
            LogConsole::write(fmt::format("{:8.2f}s  {}", levelMs() / 1000.f, line), ccColor3B{150, 150, 170});
        }
        m_trace.push_back(fmt::format("{:9.3f} {}", levelMs() / 1000.f, line));
        while (m_trace.size() > MAX_TRACE) m_trace.pop_front();
    }

    // "online-123" / "local-xxxxxxxx" (same names the learned-spot files use).
    static std::string levelFileKey(GJGameLevel* lvl) {
        if (!lvl) return "";
        int const id = lvl->m_levelID.value();
        if (id > 0) return fmt::format("online-{}", id);
        uint32_t h = 2166136261u;
        for (char ch : std::string(lvl->m_levelName)) { h ^= static_cast<uint8_t>(ch); h *= 16777619u; }
        return fmt::format("local-{:08x}", h);
    }

    static SavedStats parseStats(std::string const& t) {
        SavedStats st;
        SavedStats v;
        if (parseNums(t, ',', v.attempts, v.deaths, v.clicks, v.buffered, v.completions, v.best)) st = v;
        return st;
    }
    static std::string formatStats(SavedStats const& st) {
        return fmt::format("{},{},{},{},{},{:.2f}", st.attempts, st.deaths, st.clicks, st.buffered, st.completions, st.best);
    }
    void loadLifeStats() {
        static bool loaded = false;
        if (loaded) return;
        loaded = true;
        m_lifeStats = parseStats(Mod::get()->getSavedValue<std::string>("stats-life", ""));
    }
    void saveLevelStats() {
        auto* mod = Mod::get();
        mod->setSavedValue<std::string>("stats-life", formatStats(m_lifeStats));
        if (!m_lvlKey.empty()) mod->setSavedValue<std::string>("stats-" + m_lvlKey, formatStats(m_lvlStats));
    }

    void loadLevelExtras(PlayLayer* pl) {
        loadLifeStats();
        GJGameLevel* lvl = pl ? pl->m_level : nullptr;
        m_lvlKey = levelFileKey(lvl);
        m_lvlName = lvl ? std::string(lvl->m_levelName) : std::string();
        m_lvlStats = m_lvlKey.empty() ? SavedStats{} : parseStats(Mod::get()->getSavedValue<std::string>("stats-" + m_lvlKey, ""));
        m_attemptPending = true;
        m_trace.clear();
        trace(fmt::format("level \"{}\" ({})", m_lvlName, m_lvlKey));
        m_groundTraces = 0;
        m_groundSeen = reinterpret_cast<GameObject*>(1);
        m_groundLines.clear();
        releaseRayIndex(); // (2.2) built again from this level on its first wave input
        memLoad();         // (2.3) what autoplay remembers of this level (inputs that got it that far)
        m_apRun.clear();
        m_apRunValid = false;
        m_apBans.clear();
        m_trials.clear();
        m_trialCommits = 0;
        m_rtATries.clear();
        m_rtAOn = false;
        m_rtA = AStarRun{};
        m_rtAStarts = m_rtAFound = m_rtASpliced = 0;
        clicksLoad(); // (2.3) your good clicks on this level (kept from earlier sessions too)
        m_apBestX = -1e9f;
        m_clicksRemoved = 0;
        if (!m_clicks.empty()) trace(fmt::format("loaded {} kept clicks for this level", m_clicks.size()));
        m_pending.clear();
        m_userHeld = false;
        m_forceUntil = -1;
        m_clickAt = 0;
        m_clicksMade = m_clicksKept = m_clicksDropped = m_clicksReplayed = 0;
        m_rtUndoBans.clear();
        m_deathKinds.clear();
        m_apPracticeOurs = false;
        m_apRetries = m_apRetryTotal = 0;
        m_apRetryX = m_apRespawnX = -1e9f;
        // anchors belong to this level session
        m_anchors.clear();
        m_attemptAnchors.clear();
        m_anchorLastX = -1e9f;
        // level scan route
        m_route.clear();
        m_routePath.clear();
        m_scanned.clear();
        m_routeFile.clear();
        if (!m_lvlKey.empty()) {
            std::error_code ec;
            std::filesystem::path const dir = Mod::get()->getSaveDir() / "routes";
            std::filesystem::create_directories(dir, ec);
            m_routeFile = dir / (m_lvlKey + ".txt");
            std::ifstream in(m_routeFile);
            RoutePt r;
            int pu = 0;
            while (in >> r.x >> r.y >> pu >> r.x0 >> r.x1 >> r.mode) {
                r.push = pu != 0;
                std::replace(r.mode.begin(), r.mode.end(), '_', ' ');
                m_route.push_back(r);
            }
            std::ifstream pin(dir / (m_lvlKey + "-path.txt"));
            acecore::Point2 pt;
            while (pin >> pt.x >> pt.y) m_routePath.push_back(pt);
            std::ifstream din(dir / (m_lvlKey + "-done.txt"));
            float a = 0.f, b = 0.f;
            while (din >> a >> b) m_scanned.emplace_back(a, b);
        }
        // (2.2) Runs saved in earlier sessions are NOT loaded any more: on disk
        // they lose what they were touching (level objects only exist while
        // the level is open), and the scan carrying on from one crashed GD in
        // its collision code (a ground / slope it thought it had but didn't).
        // Runs from this session are complete and still used.
    }

    // Level finished (end animation): a completion, and the whole attempt counts for the guide.
    void onLevelEnd(PlayLayer* pl) {
        if (!pl) return;
        if (!pl->m_isPracticeMode) {
            m_lvlStats.completions++;
            m_lifeStats.completions++;
            m_lvlStats.best = 100.f;
        }
        trace("level complete");
        if (m_autoOn && !deathsOff()) memReachedEnd(pl); // (2.3) autoplay beat it: the whole run is remembered
        commitAnchors(0.f, true);
        saveAnchors();
        saveLevelStats();
    }

    // Route from a level scan replaces what it covers ([lo, hi] in x).
    void routeMerge(std::vector<RoutePt> const& found, float lo, float hi,
                    std::vector<acecore::Point2> const& path = {}) {
        if (!path.empty()) {
            std::vector<acecore::Point2> np;
            np.reserve(m_routePath.size() + path.size());
            for (auto const& q : m_routePath) if (q.x < lo || q.x > hi) np.push_back(q);
            for (auto const& q : path) if (q.x >= lo && q.x <= hi) np.push_back(q);
            std::stable_sort(np.begin(), np.end(), [](acecore::Point2 const& a, acecore::Point2 const& b) { return a.x < b.x; });
            m_routePath = std::move(np);
        }
        // stretches scanned (so auto scan knows where it still has to go)
        m_scanned.emplace_back(lo, hi);
        std::sort(m_scanned.begin(), m_scanned.end());
        std::vector<std::pair<float, float>> mg;
        for (auto const& r : m_scanned) {
            if (!mg.empty() && r.first <= mg.back().second + 1.f) mg.back().second = std::max(mg.back().second, r.second);
            else mg.push_back(r);
        }
        m_scanned = std::move(mg);
        std::vector<RoutePt> next;
        next.reserve(m_route.size() + found.size());
        for (auto const& r : m_route) if (r.x < lo || r.x > hi) next.push_back(r);
        for (auto const& r : found) next.push_back(r);
        std::stable_sort(next.begin(), next.end(), [](RoutePt const& a, RoutePt const& b) { return a.x < b.x; });
        m_route = std::move(next);
        routeSave();
    }

    // Writes the route, its path and the scanned stretches for this level.
    void routeSave() {
        if (!m_routeFile.empty()) {
            auto const dir = m_routeFile.parent_path();
            std::ofstream pout(dir / (m_lvlKey + "-path.txt"), std::ios::trunc);
            for (auto const& q : m_routePath) pout << fmt::format("{:.1f} {:.1f}\n", q.x, q.y);
            std::ofstream dout(dir / (m_lvlKey + "-done.txt"), std::ios::trunc);
            for (auto const& r : m_scanned) dout << fmt::format("{:.1f} {:.1f}\n", r.first, r.second);
        }
        if (!m_routeFile.empty()) {
            std::ofstream out(m_routeFile, std::ios::trunc);
            for (auto const& r : m_route) {
                std::string m = r.mode.empty() ? std::string("none") : r.mode;
                std::replace(m.begin(), m.end(), ' ', '_');
                out << fmt::format("{:.2f} {:.2f} {} {:.2f} {:.2f} {}\n", r.x, r.y, r.push ? 1 : 0, r.x0, r.x1, m);
            }
        }
    }

    // The real player is far off the scanned path for a while (the scan started
    // before a start position placed the player, the level moves things, or you
    // take another way): from there on that stretch counts as not scanned - its
    // dots go, and "scan level" from a pause there redoes it.
    void routeCheck(PlayLayer* pl, PlayerObject* p) {
        if (m_routePath.size() < 2 || (m_tick & 3) != 0) return;
        float const x = p->getPositionX();
        auto rit = std::find_if(m_scanned.begin(), m_scanned.end(),
                                [x](std::pair<float, float> const& r) { return x >= r.first && x <= r.second; });
        if (rit == m_scanned.end()) { m_routeOff = 0; return; }
        auto it = std::lower_bound(m_routePath.begin(), m_routePath.end(), x,
                                   [](acecore::Point2 const& q, float v) { return q.x < v; });
        if (it == m_routePath.end() || it == m_routePath.begin()) return;
        acecore::Point2 const a = *(it - 1), b = *it;
        if (b.x - a.x > 200.f) return; // a gap in the path (scan stopped there)
        float const t = b.x - a.x > 0.01f ? (x - a.x) / (b.x - a.x) : 0.f;
        float const py = a.y + t * (b.y - a.y);
        float const off = p->getPositionY() - py;
        if (std::fabs(off) > ROUTE_OFF_Y) m_routeOff++;
        else m_routeOff = 0;
        if (m_routeOff < ROUTE_OFF_CHECKS) return;
        m_routeOff = 0;
        float const cut = std::max(rit->first, x - 30.f);
        float const end = rit->second;
        auto inside = [cut, end](float v) { return v >= cut && v <= end; };
        std::erase_if(m_route, [&](RoutePt const& r) { return inside(r.x); });
        std::erase_if(m_routePath, [&](acecore::Point2 const& q) { return inside(q.x); });
        if (cut <= rit->first + 1.f) m_scanned.erase(rit);
        else rit->second = cut;
        routeSave();
        trace(fmt::format("route: you're {:+.0f}px from the scanned path at x={:.0f} - {:.0f}..{:.0f} is unscanned again",
                          off, x, cut, end));
        pushLog("Your run is nowhere near the scanned path here - that part of the scan is dropped. Pause here and press scan level to redo it.",
                COL_YELLOW, false);
    }

    // Gamemode as a number (0 cube, 1 ship, 2 ball, 3 ufo, 4 wave, 5 robot,
    // 6 spider, 7 swing) and switching a HIDDEN player between them with the
    // player's own toggles (no level / camera / ground code) - level scan only.
    static int modeCode(PlayerObject* p) {
        if (p->m_isShip) return 1;
        if (p->m_isBall) return 2;
        if (p->m_isBird) return 3;
        if (p->m_isDart) return 4;
        if (p->m_isRobot) return 5;
        if (p->m_isSpider) return 6;
        if (p->m_isSwing) return 7;
        return 0;
    }
    // (2.2) The floor / ceiling GD puts at a ship / UFO / wave / swing / ball /
    // spider portal: GD's heights (getGroundHeightForMode: 300, ball 240,
    // spider 270), centred on the portal you came through, never below the
    // ground (y 90). 0 = none (cube, robot).
    static float bandHeight(int mc) {
        switch (mc) {
            case 1: case 3: case 4: case 7: return 300.f;
            case 2: return 240.f;
            case 6: return 270.f;
            default: return 0.f;
        }
    }
    static bool bandOf(PlayerObject* p, float& lo, float& hi, float fallbackY = 0.f) {
        float const h = bandHeight(modeCode(p));
        if (h <= 0.f) return false;
        float const cy = std::fabs(p->m_lastPortalPos.y) > 0.01f ? p->m_lastPortalPos.y : fallbackY;
        if (std::fabs(cy) < 0.01f) {
            // no portal yet: the level starts in this gamemode - GD's band then
            // sits on the ground (Denouement: 90 .. 390, your releases at 384)
            lo = GROUND_Y;
            hi = lo + h;
            return true;
        }
        // (2.3) GD puts the band's floor on the 30px grid (rounded down), not
        // exactly half a band under the portal: Time Machine's ship portal at
        // y 255 -> floor 90 (was 105 - the hidden ship slid over the spikes
        // on the floor at the end that kill you)
        lo = std::max(GROUND_Y, std::floor((cy - h * 0.5f) / 30.f) * 30.f);
        hi = lo + h;
        return true;
    }
    // Scan / A* / pathfinder: keep the hidden player inside its band the way
    // GD keeps you - you slide along the floor / ceiling, you don't fly out.
    // Only when it was inside the band a tick ago (`prevY`): a band guess that
    // doesn't match where you are (a start position, no portal yet) is left alone.
    void bandClamp(PlayerObject* p, float prevY) {
        float lo = 0.f, hi = 0.f;
        if (!p || !bandOf(p, lo, hi)) return; // (no portal yet: the band from the ground, see bandOf)
        CCRect const& r = p->getObjectRect();
        float const hh = std::max(1.f, r.size.height * 0.5f);
        if (prevY - hh < lo - 2.f || prevY + hh > hi + 2.f) return;
        float const y = p->getPositionY();
        bool const flip = p->m_isUpsideDown;
        if (y - hh < lo) {
            p->setPositionY(lo + hh);
            if (p->m_yVelocity < 0.0) p->m_yVelocity = 0.0;
            if (!flip) p->m_isOnGround = true;
            m_bandClamps++;
        } else if (y + hh > hi) {
            p->setPositionY(hi - hh);
            if (p->m_yVelocity > 0.0) p->m_yVelocity = 0.0;
            if (flip) p->m_isOnGround = true;
            m_bandClamps++;
        }
    }
    long long m_bandClamps = 0;
    // Any solid / spike / slope of the level within OFF_PATH_PX of the player?
    // (A level's path always runs along its obstacles.) No index: can't tell, yes.
    bool nearLevel(PlayerObject* p) const {
        if (m_rayObjs.empty() || !p) return true;
        float const x = p->getPositionX(), y = p->getPositionY();
        if (x >= m_scanEndX - END_SLACK) return true; // the end of a level is often empty
        float const lo = x - OFF_PATH_PX, hi = x + OFF_PATH_PX;
        auto it = std::lower_bound(m_rayObjs.begin(), m_rayObjs.end(), lo - m_rayMaxW,
                                   [](RayObj const& a, float v) { return a.x0 < v; });
        for (; it != m_rayObjs.end() && it->x0 <= hi; ++it) {
            if (it->x1 < lo) continue;
            GameObject* o = it->obj;
            if (o->m_isGroupDisabled) continue; // (2.3: off screen counts)
            CCRect const& r = o->getObjectRect();
            float const dx = std::max({r.getMinX() - x, 0.f, x - r.getMaxX()});
            float const dy = std::max({r.getMinY() - y, 0.f, y - r.getMaxY()});
            if (dx * dx + dy * dy <= OFF_PATH_PX * OFF_PATH_PX) return true;
        }
        return false;
    }
    static int portalMode(GameObjectType t) {
        switch (t) {
            case GameObjectType::CubePortal:   return 0;
            case GameObjectType::ShipPortal:   return 1;
            case GameObjectType::BallPortal:   return 2;
            case GameObjectType::UfoPortal:    return 3;
            case GameObjectType::WavePortal:   return 4;
            case GameObjectType::RobotPortal:  return 5;
            case GameObjectType::SpiderPortal: return 6;
            case GameObjectType::SwingPortal:  return 7;
            default: return -1;
        }
    }
    static void setSimMode(PlayerObject* p, int code) {
        int const cur = modeCode(p);
        if (cur == code) return;
        switch (cur) {
            case 1: p->toggleFlyMode(false, true); break;
            case 2: p->toggleRollMode(false, true); break;
            case 3: p->toggleBirdMode(false, true); break;
            case 4: p->toggleDartMode(false, true); break;
            case 5: p->toggleRobotMode(false, true); break;
            case 6: p->toggleSpiderMode(false, true); break;
            case 7: p->toggleSwingMode(false, true); break;
            default: break;
        }
        switch (code) {
            case 1: p->toggleFlyMode(true, true); break;
            case 2: p->toggleRollMode(true, true); break;
            case 3: p->toggleBirdMode(true, true); break;
            case 4: p->toggleDartMode(true, true); break;
            case 5: p->toggleRobotMode(true, true); break;
            case 6: p->toggleSpiderMode(true, true); break;
            case 7: p->toggleSwingMode(true, true); break;
            default: break;
        }
    }
    bool      m_scanSim = false;          // the current test belongs to a level scan
    long long m_scanModeSwitches = 0;
    int       m_scanPortalTraces = 0;
    int       m_lastRealMode = -1;
    float     m_realBandMin = 1e9f, m_realBandMax = -1e9f, m_realBandY = 0.f; // (2.2) band check
    GameObject* m_groundSeen = reinterpret_cast<GameObject*>(1); // (2.2) ground check (pointer only compared)
    int       m_groundTraces = 0;

    // Level scan (2.1): the moving state of a sim player, saved as plain values.
    // A test starts from the REAL player (gamemode, size, hitbox - everything
    // syncSimPlayer copies) and then gets these values on top. Copying into a
    // spare hidden PlayerObject and starting from that lost something: in a
    // mini wave the copy died on its first tick.
#define ACE_SNAP_FIELDS(X) \
    X(m_width) X(m_height) X(m_gravityMod) X(m_isOnGround) X(m_isUpsideDown) X(m_vehicleSize) X(m_playerSpeed) \
    X(m_yVelocity) X(m_gravity) X(m_speedMultiplier) X(m_position) X(m_lastPortalPos) X(m_lastGroundedPos) \
    X(m_lastActivatedPortal) X(m_isOnGround2) X(m_lastLandTime) X(m_stateOnGround) X(m_isGoingLeft) X(m_isSideways) \
    X(m_isDashing) X(m_jumpBuffered) X(m_stateJumpBuffered) X(m_wasJumpBuffered) X(m_stateRingJump) \
    X(m_stateRingJump2) X(m_wasRobotJump) X(m_fixRobotJump) X(m_touchedRing) X(m_touchedCustomRing) \
    X(m_touchedGravityPortal) X(m_touchedPad) X(m_fallSpeed) X(m_isAccelerating) X(m_accelerationOrSpeed) \
    X(m_isOnSlope) X(m_wasOnSlope) X(m_slopeVelocity) X(m_maybeUpsideDownSlope) X(m_slopeRotation) \
    X(m_currentSlopeYVelocity) X(m_isCurrentSlopeTop) X(m_unk3d0) X(m_blackOrbRelated) X(m_unk3e0) X(m_unk3e1) \
    X(m_collidedTopMinY) X(m_collidedBottomMaxY) X(m_collidedLeftMaxX) X(m_collidedRightMinX) X(m_stateFlipGravity) \
    X(m_stateForce) X(m_stateForceVector) X(m_affectedByForces) X(m_lastMovedTime) X(m_playerSpeedAC) \
    X(m_jumpRelatedAC2) X(m_lastJumpTime) X(m_lastFlipTime) X(m_lastSpiderFlipTime) X(m_snapDistance) X(m_totalTime) \
    X(m_yVelocityBeforeSlope) X(m_groundYVelocity) X(m_yVelocityRelated) X(m_yVelocityRelated3) X(m_ringJumpRelated) X(m_maybeIsBoosted) X(m_decreaseBoostSlide) X(m_stateBoostX) X(m_stateBoostY) X(m_maybeStateForce2) X(m_stateNoAutoJump) X(m_hasEverJumped) X(m_fixGravityBug) X(m_slopeStartTime) X(m_slopeEndTime) X(m_slopeFlipGravityRelated) X(m_dashStartTime) X(m_maybeSlidingTime) X(m_maybeSlidingStartTime) X(m_changedDirectionsTime) X(m_maybeReverseSpeed) X(m_maybeReverseAcceleration) X(m_xVelocityRelated) X(m_xVelocityRelated2) \
    X(m_maybeLastGroundObject) X(m_lastCollisionBottom) X(m_lastCollisionTop) X(m_lastCollisionLeft) X(m_lastCollisionRight) X(m_currentSlope2) X(m_preLastGroundObject) X(m_collidedObject) X(m_lastGroundObject) X(m_collidingWithLeft) X(m_collidingWithRight) X(m_isCollidingWithSlope) X(m_currentPotentialSlope) X(m_currentSlope) X(m_collidingWithSlopeId) X(m_objectSnappedTo) X(m_maybeIsColliding) X(m_groundObjectMaterial) X(m_maybeGoingCorrectSlopeDirection) X(m_maybeChangedDirectionAngle) \
    /* (2.3) the rest of the player's plain physics state: route / scan states carried the real player's */ \
    X(m_wasTeleported) X(m_reverseSync) X(m_dashX) X(m_dashY) X(m_dashAngle) X(m_slopeAngle) X(m_slopeSlidingMaybeRotated) X(m_scaleXRelated) X(m_scaleXRelated2) X(m_scaleXRelated3) X(m_scaleXRelated4) X(m_scaleXRelated5) X(m_scaleXRelatedTime) X(m_isBallRotating) X(m_isBallRotating2) X(m_slopeAngleRadians) X(m_rotationSpeed) X(m_rotateSpeed) X(m_isRotating) X(m_yStart) X(m_gameModeChangedTime) X(m_padRingRelated) X(m_maybeIsFalling) X(m_maybeCanRunIntoBlocks) X(m_isOnGround3) X(m_isOnGround4) X(m_maybeTouchedBreakableBlock) X(m_reverseRelated) X(m_shipRotation) X(m_platformerVelocityRelated) X(m_hasEverHitRing) X(m_unk838) X(m_stateScale) X(m_platformerXVelocity) X(m_maybeHasStopped) X(m_isSliding) X(m_maybeSlopeForce) X(m_isOnIce) X(m_physDeltaRelated) X(m_isMoving) X(m_stateDartSlide) X(m_stateHitHead) X(m_isOutOfBounds) X(m_fallStartY) X(m_unk50C) X(m_unk510) X(m_unk669) X(m_unkA29) X(m_unkA99)
    struct PlayerSnap {
#define ACE_SNAP_DECL(f) std::remove_cvref_t<decltype(std::declval<PlayerObject&>().f)> f{};
        ACE_SNAP_FIELDS(ACE_SNAP_DECL)
#undef ACE_SNAP_DECL
        std::vector<int> touchedRings; // gd::unordered_set: copied by hand (like syncSimPlayer does)
        int     mode = 0;              // modeCode() - the scan passes gamemode portals
        CCPoint pos{};
        float   scale = 1.f;
        CCSize  content{};
        float   rot = 0.f;                // (2.3) rotation (not saved to disk: 0 there)
    };
    // (2.3 debug) every field of two states that differs, by name
    using DiffList = std::vector<std::pair<char const*, std::string>>;
    static DiffList snapDiffList(PlayerSnap const& a, PlayerSnap const& b) {
        DiffList out;
        auto add = [&](char const* name, std::string const& v) { out.emplace_back(name, v); };
        auto cmp = [&](char const* name, auto const& x, auto const& y) {
            using T = std::remove_cvref_t<decltype(x)>;
            if constexpr (std::is_floating_point_v<T>) {
                if (std::fabs(static_cast<double>(x) - static_cast<double>(y)) > 1e-6)
                    add(name, fmt::format("{:.4f}/{:.4f}", static_cast<double>(x), static_cast<double>(y)));
            } else if constexpr (std::is_same_v<T, bool>) {
                if (x != y) add(name, fmt::format("{}/{}", x, y));
            } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
                if (x != y) add(name, fmt::format("{}/{}", static_cast<long long>(x), static_cast<long long>(y)));
            } else if constexpr (std::is_same_v<T, cocos2d::CCPoint>) {
                if (std::fabs(x.x - y.x) > 1e-4f || std::fabs(x.y - y.y) > 1e-4f)
                    add(name, fmt::format("({:.3f},{:.3f})/({:.3f},{:.3f})", x.x, x.y, y.x, y.y));
            } else if constexpr (std::is_pointer_v<T>) {
                if (x != y) add(name, "differs");
            }
        };
#define ACE_SNAP_DIFFL(f) cmp(#f, a.f, b.f);
        ACE_SNAP_FIELDS(ACE_SNAP_DIFFL)
#undef ACE_SNAP_DIFFL
        if (std::fabs(a.pos.x - b.pos.x) > 1e-4f || std::fabs(a.pos.y - b.pos.y) > 1e-4f)
            add("pos", fmt::format("({:.3f},{:.3f})/({:.3f},{:.3f})", a.pos.x, a.pos.y, b.pos.x, b.pos.y));
        if (std::fabs(a.rot - b.rot) > 1e-3f) add("rot", fmt::format("{:.2f}/{:.2f}", a.rot, b.rot));
        return out;
    }
    static std::string snapDiff(PlayerSnap const& a, PlayerSnap const& b, int maxN = 24) {
        std::string out;
        int n = 0;
        auto add = [&](char const* name, std::string const& v) {
            if (n++ >= maxN) return;
            out += fmt::format("{}{} {}", out.empty() ? "" : ", ", name, v);
        };
        auto cmp = [&](char const* name, auto const& x, auto const& y) {
            using T = std::remove_cvref_t<decltype(x)>;
            if constexpr (std::is_floating_point_v<T>) {
                if (std::fabs(static_cast<double>(x) - static_cast<double>(y)) > 1e-6)
                    add(name, fmt::format("{:.4f}/{:.4f}", static_cast<double>(x), static_cast<double>(y)));
            } else if constexpr (std::is_same_v<T, bool>) {
                if (x != y) add(name, fmt::format("{}/{}", x, y));
            } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
                if (x != y) add(name, fmt::format("{}/{}", static_cast<long long>(x), static_cast<long long>(y)));
            } else if constexpr (std::is_same_v<T, cocos2d::CCPoint>) {
                if (std::fabs(x.x - y.x) > 1e-4f || std::fabs(x.y - y.y) > 1e-4f)
                    add(name, fmt::format("({:.3f},{:.3f})/({:.3f},{:.3f})", x.x, x.y, y.x, y.y));
            } else if constexpr (std::is_pointer_v<T>) {
                if (x != y) add(name, "differs");
            }
        };
#define ACE_SNAP_DIFF(f) cmp(#f, a.f, b.f);
        ACE_SNAP_FIELDS(ACE_SNAP_DIFF)
#undef ACE_SNAP_DIFF
        if (std::fabs(a.pos.x - b.pos.x) > 1e-4f || std::fabs(a.pos.y - b.pos.y) > 1e-4f)
            add("pos", fmt::format("({:.3f},{:.3f})/({:.3f},{:.3f})", a.pos.x, a.pos.y, b.pos.x, b.pos.y));
        if (std::fabs(a.rot - b.rot) > 1e-3f) add("rot", fmt::format("{:.2f}/{:.2f}", a.rot, b.rot));
        if (n > maxN) out += fmt::format(" (+{} more)", n - maxN);
        return out.empty() ? std::string("no field differs") : out;
    }
    static PlayerSnap snapFrom(PlayerObject* p) {
        PlayerSnap s;
#define ACE_SNAP_GET(f) s.f = p->f;
        ACE_SNAP_FIELDS(ACE_SNAP_GET)
#undef ACE_SNAP_GET
        for (int id : p->m_touchedRings) s.touchedRings.push_back(id);
        s.mode = modeCode(p);
        s.pos = p->getPosition();
        s.scale = p->getScale();
        s.content = p->getContentSize();
        s.rot = p->getRotation();
        return s;
    }
    static void applySnap(PlayerObject* p, PlayerSnap const& s) {
        p->setPosition(s.pos);
        p->setRotation(s.rot);
        p->setScale(s.scale);
        p->setContentSize(s.content);
#define ACE_SNAP_SET(f) p->f = s.f;
        ACE_SNAP_FIELDS(ACE_SNAP_SET)
#undef ACE_SNAP_SET
        p->m_touchedRings.clear();
        for (int id : s.touchedRings) p->m_touchedRings.insert(id);
        // Scan crash fix (2.2): run states saved to disk come back with their
        // level-object pointers empty (they only mean something while the level
        // is open) but the flags that go with them still set - "on a slope"
        // with no slope. When the scan carried on from such a state (stuck on a
        // big level -> "used your run"), GD's slope / collision code followed
        // the empty pointer and the game crashed. A missing object now means
        // "not touching it": the next collision step finds the real one again.
        if (!p->m_currentSlope) {
            p->m_isOnSlope = false;
            p->m_wasOnSlope = false;
            p->m_isCollidingWithSlope = false;
            p->m_isCurrentSlopeTop = false;
        }
    }
    PlayerSnap const* m_simSnap = nullptr; // set: tests start from this state (level scan)
    // (2.2) dual in the level scan: player 2's saved state (set: it's in the
    // test too), a dual / solo portal touched during a test (1 / 2), and
    // whether player 2 was still in it at the end
    PlayerSnap const* m_simSnap2 = nullptr;
    int  m_simDualReq = 0;
    bool m_simP2Live = false;
    bool m_scanTwoPlayer = false;          // a 2-player level: P2 has its own button, the scan leaves it be
    float m_scanClr = 1e9f;                // level scan: closest a test came to a hazard (every tick)

    // ------------------------------------------------------------------------
    //  Level scan (2.1): finds a route of inputs that gets through the level,
    //  using the same hidden-player physics as the predictions.
    //
    //  Beam search: it keeps up to SCAN_BEAM different "versions" of the run at
    //  once. Every SCAN_STEP_BASE ticks each version tries both "keep doing what
    //  you're doing" and "switch" (press / let go), versions that die are
    //  dropped, and the survivors are thinned out so the ones left are spread
    //  over different heights / speeds (so one dead end can't take them all),
    //  preferring fewer inputs. It reaches the end (or a portal it can't pass)
    //  and the best version's inputs become the route.
    //  Runs while the game is PAUSED, a slice per frame. Limits: triggers don't
    //  run (moving parts can be wrong), it stops at gamemode / dual / mirror /
    //  teleport portals, no dual / platformer.
    // ------------------------------------------------------------------------
    struct BeamHist {
        int   parent = -1;
        float x = 0.f, y = 0.f;   // where the input went in (toggled) / where the step started
        bool  toggled = false;
        bool  held = false;
        uint8_t mode = 0;         // gamemode it went in at (index in m_scanModeNames)
    };
    struct BeamNode {
        PlayerSnap snap;
        bool  fromReal = true;    // the very first one: the real player itself
        bool  held = false;
        int   toggles = 0;
        float cost = 0.f;         // inputs + time spent right at hazards (lower = nicer route)
        int   hist = -1;          // its entry in m_beamHist
        BeamHist pend;            // its step, added to m_beamHist only if it's kept
        float x = 0.f, y = 0.f;
        double vy = 0.0;
        int   mode = 0;           // index in m_scanModeNames
        int   modeCode = 0;       // modeCode(): decides tap vs hold inputs
        std::vector<GameObject*> orbs; // orbs this version has used
        bool  mini = false;       // (2.2) mini size (mini wave: finer heights)
        // (2.2) dual: player 2's state - it gets the same inputs and has to live too
        bool  dual = false;
        PlayerSnap snap2;
        float y2 = 0.f;
        // (2.2) a ship tap (press, let go a tick later) is two route inputs
        bool  tapRel = false;
        BeamHist pend2;
    };

    // (2.2) One tick of a sprint (see spSlice): the hidden player(s) after it.
    struct SpTick {
        PlayerSnap s1, s2;
        bool  dual = false;
        float x = 0.f, y = 0.f, y2 = 0.f;
        double vy = 0.0;
        int   modeCode = 0, modeIdx = 0;
        bool  mini = false;
    };
    std::vector<SpTick>* m_simTrail = nullptr; // (2.2) set: every tick's state goes here (the sprint)

    // Drops history no surviving version leads back to (parents always come
    // before children, so one pass in order rebuilds it).
    void compactHist() {
        std::vector<int> remap(m_beamHist.size(), -1);
        auto mark = [&](std::vector<BeamNode> const& v) {
            for (auto const& b : v) {
                for (int h = b.hist; h >= 0 && remap[static_cast<size_t>(h)] == -1; h = m_beamHist[static_cast<size_t>(h)].parent)
                    remap[static_cast<size_t>(h)] = -2;
            }
        };
        mark(m_beam);
        for (auto const& r : m_rewind) mark(r.second);
        std::vector<BeamHist> nh;
        for (size_t i = 0; i < m_beamHist.size(); i++) {
            if (remap[i] != -2) continue;
            BeamHist e = m_beamHist[i];
            e.parent = e.parent >= 0 ? remap[static_cast<size_t>(e.parent)] : -1;
            remap[i] = static_cast<int>(nh.size());
            nh.push_back(e);
        }
        for (auto& b : m_beam) if (b.hist >= 0) b.hist = remap[static_cast<size_t>(b.hist)];
        for (auto& r : m_rewind) for (auto& b : r.second) if (b.hist >= 0) b.hist = remap[static_cast<size_t>(b.hist)];
        m_beamHist = std::move(nh);
    }
    std::vector<std::string> m_scanModeNames;
    int scanModeIndex(PlayerObject* p) {
        ModeInfo mi;
        mi.add(p);
        std::string const nm = mi.name();
        for (size_t i = 0; i < m_scanModeNames.size(); i++) if (m_scanModeNames[i] == nm) return static_cast<int>(i);
        m_scanModeNames.push_back(nm);
        return static_cast<int>(m_scanModeNames.size()) - 1;
    }
    // Anchors (2.1): your REAL player's state, saved every ANCHOR_STEP px while
    // you play (only from stretches you got clearly past). Where the scan can't
    // simulate something (a portal entry, a trigger, a moving part) it carries
    // on from one of these instead of stopping - the real game already did that
    // part right. Kept for this session of the level only.
    struct Anchor {
        float x = 0.f;
        PlayerSnap snap;
        std::string modeName;
        bool held = false;
    };
    std::map<int, Anchor> m_anchors;          // by x / ANCHOR_STEP
    bool m_anchorsDirty = false;
    std::vector<Anchor>   m_attemptAnchors;   // this attempt, not yet confirmed
    float m_anchorLastX = -1e9f, m_attemptLastX = 0.f;
    int   m_scanJumps = 0;
    CCPoint m_simKillPos{};

    void recordAnchor(PlayLayer* pl, PlayerObject* p) {
        float const x = p->getPositionX();
        m_attemptLastX = x;
        if (pl->m_gameState.m_isDualMode || p->m_isPlatformer || m_simulating) return;
        if (std::fabs(x - m_anchorLastX) < ANCHOR_STEP) return;
        m_anchorLastX = x;
        Anchor a;
        a.x = x;
        a.snap = snapFrom(p);
        ModeInfo mi;
        mi.add(p);
        a.modeName = mi.name();
        a.held = m_gameHolding;
        m_attemptAnchors.push_back(std::move(a));
    }
    // The attempt ended at `endX` (death / restart: only what's well before it
    // counts - a state right before a death may already be doomed; finished: all).
    void commitAnchors(float endX, bool all) {
        for (auto& a : m_attemptAnchors) {
            if (!all && a.x > endX - ANCHOR_SAFE) continue;
            m_anchors[static_cast<int>(std::floor(a.x / ANCHOR_STEP))] = std::move(a);
            m_anchorsDirty = true;
        }
        m_attemptAnchors.clear();
        m_anchorLastX = -1e9f;
    }
    Anchor const* anchorAfter(float x) const {
        auto it = m_anchors.lower_bound(static_cast<int>(std::floor(x / ANCHOR_STEP)));
        for (; it != m_anchors.end(); ++it) if (it->second.x > x + 5.f) return &it->second;
        return nullptr;
    }

    // ---- saved run states (2.1): your run's states survive leaving the level.
    // Plain values only (pointers to level objects are dropped - they only mean
    // something while the level is open). Checked against the game version and
    // the field layout, so an update just starts a fresh file.
    template <class T> static void snapPut(std::string& o, T const& v) {
        if constexpr (std::is_pointer_v<T>) { (void)v; }
        else if constexpr (std::is_same_v<T, cocos2d::CCPoint>) { snapPut(o, v.x); snapPut(o, v.y); }
        else if constexpr (std::is_same_v<T, cocos2d::CCSize>) { snapPut(o, v.width); snapPut(o, v.height); }
        else if constexpr (std::is_trivially_copyable_v<T>) { o.append(reinterpret_cast<char const*>(&v), sizeof(T)); }
    }
    template <class T> static bool snapGet(char const*& p, char const* e, T& v) {
        if constexpr (std::is_pointer_v<T>) { v = nullptr; return true; }
        else if constexpr (std::is_same_v<T, cocos2d::CCPoint>) { return snapGet(p, e, v.x) && snapGet(p, e, v.y); }
        else if constexpr (std::is_same_v<T, cocos2d::CCSize>) { return snapGet(p, e, v.width) && snapGet(p, e, v.height); }
        else if constexpr (std::is_trivially_copyable_v<T>) {
            if (e - p < static_cast<std::ptrdiff_t>(sizeof(T))) return false;
            std::memcpy(&v, p, sizeof(T));
            p += sizeof(T);
            return true;
        } else {
            return true;
        }
    }
    static uint64_t snapSig() {
        uint64_t h = 1469598103934665603ull;
        auto mix = [&](std::string_view t) { for (char ch : t) { h ^= static_cast<unsigned char>(ch); h *= 1099511628211ull; } };
#define ACE_SNAP_SIG(f) mix(#f); mix(std::to_string(sizeof(std::declval<PlayerSnap&>().f)));
        ACE_SNAP_FIELDS(ACE_SNAP_SIG)
#undef ACE_SNAP_SIG
        mix(Loader::get()->getGameVersion());
        return h;
    }
    std::filesystem::path anchorsFile() const {
        if (m_routeFile.empty()) return {};
        return m_routeFile.parent_path() / (m_lvlKey + "-runs.bin");
    }
    void saveAnchors() {
        // (2.2) not saved any more - they aren't loaded back (see loadLevelExtras)
        m_anchorsDirty = false;
        if (!m_anchorsDirty) return;
        auto const f = anchorsFile();
        if (f.empty()) return;
        m_anchorsDirty = false;
        std::string o = "ACERUN1";
        snapPut(o, snapSig());
        snapPut(o, static_cast<uint32_t>(m_anchors.size()));
        for (auto const& [k, a] : m_anchors) {
            snapPut(o, a.x);
            snapPut(o, static_cast<uint8_t>(a.held ? 1 : 0));
            snapPut(o, static_cast<uint16_t>(a.modeName.size()));
            o += a.modeName;
            PlayerSnap const& sn = a.snap;
            snapPut(o, sn.mode);
            snapPut(o, sn.pos);
            snapPut(o, sn.scale);
            snapPut(o, sn.content);
#define ACE_SNAP_PUT(f) snapPut(o, sn.f);
            ACE_SNAP_FIELDS(ACE_SNAP_PUT)
#undef ACE_SNAP_PUT
            snapPut(o, static_cast<uint32_t>(sn.touchedRings.size()));
            for (int id : sn.touchedRings) snapPut(o, id);
        }
        std::ofstream out(f, std::ios::binary | std::ios::trunc);
        out.write(o.data(), static_cast<std::streamsize>(o.size()));
    }
    void loadAnchors() {
        auto const f = anchorsFile();
        if (f.empty()) return;
        std::ifstream in(f, std::ios::binary);
        if (!in) return;
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (data.size() < 6 || data.compare(0, 6, "ACERUN1") != 0) return;
        char const* p = data.data() + 6;
        char const* e = data.data() + data.size();
        uint64_t sig = 0;
        uint32_t n = 0;
        if (!snapGet(p, e, sig) || sig != snapSig() || !snapGet(p, e, n)) return;
        int loaded = 0;
        for (uint32_t i = 0; i < n && i < 100000; i++) {
            Anchor a;
            uint8_t held = 0;
            uint16_t len = 0;
            if (!snapGet(p, e, a.x) || !snapGet(p, e, held) || !snapGet(p, e, len) || e - p < len) break;
            a.held = held != 0;
            a.modeName.assign(p, len);
            p += len;
            PlayerSnap& sn = a.snap;
            if (!snapGet(p, e, sn.mode) || !snapGet(p, e, sn.pos) || !snapGet(p, e, sn.scale) || !snapGet(p, e, sn.content)) break;
            bool ok = true;
#define ACE_SNAP_GETF(f) ok = ok && snapGet(p, e, sn.f);
            ACE_SNAP_FIELDS(ACE_SNAP_GETF)
#undef ACE_SNAP_GETF
            uint32_t rc = 0;
            if (!ok || !snapGet(p, e, rc) || rc > 4096) break;
            for (uint32_t j = 0; j < rc; j++) {
                int id = 0;
                if (!snapGet(p, e, id)) { ok = false; break; }
                sn.touchedRings.push_back(id);
            }
            if (!ok) break;
            m_anchors[static_cast<int>(std::floor(a.x / ANCHOR_STEP))] = std::move(a);
            loaded++;
        }
        if (loaded > 0) trace(fmt::format("loaded {} saved run states for the scan", loaded));
    }

    enum class ScanState { Off, Running, Done };
    ScanState   m_scanState = ScanState::Off;
    std::vector<BeamNode> m_beam;
    std::vector<BeamHist> m_beamHist;
    std::vector<GameObject*> m_scanSavedOrbs;  // m_realUsedOrbs before the scan
    float       m_scanStartX = 0.f, m_scanX = 0.f, m_scanEndX = 1.f;
    std::string m_scanMsg;
    ModeInfo    m_scanMode;
    float       m_scanDt = BASE_DT, m_scanS = 1.f;
    long long   m_scanSteps = 0;
    int         m_scanInputs = 0;
    int         m_scanLastMode = -1;           // the beam's gamemode last step (portal re-sync)
    // Never stop (2.1): back up and try harder, then your own run, then the
    // real game (practice), then go through the spot.
    std::deque<std::pair<float, std::vector<BeamNode>>> m_rewind; // copies of the versions, by x
    float       m_rewindLastX = -1e9f;
    float       m_scanWideUntil = -1e9f;       // trying harder until here
    float       m_scanWideAt = -1e9f;          // last spot it backed up for
    int         m_scanWideLevel = 1;           // (2.2) 1..SCAN_TRIES: how hard it's trying at that spot
    bool        m_scanNoclip = false;          // sim: deaths don't count (going through a spot)
    int         m_scanSkips = 0, m_scanBackups = 0, m_scanReal = 0;
    std::vector<std::pair<float, float>> m_scanSkipRanges; // gone through, not found
    std::deque<CCPoint> m_scanDeathPts;        // where versions died lately (scan view)
    Ref<CCDrawNode> m_scanViewNode;
    std::string m_scanLastWhat;

    bool scanning() const { return m_scanState == ScanState::Running; }
    std::string scanStatus() const {
        if (m_scanState == ScanState::Running) {
            float const span = std::max(1.f, m_scanEndX - m_scanStartX);
            return fmt::format("scanning... {:.0f}%  ({} paths alive)", 100.f * std::clamp((m_scanX - m_scanStartX) / span, 0.f, 1.f),
                               m_beam.size());
        }
        if (!m_scanMsg.empty()) return m_scanMsg;
        if (!m_route.empty()) return fmt::format("{} inputs saved for this level", m_route.size());
        return "not scanned yet";
    }

    void startScan(PlayLayer* pl) {
        if (scanning()) return;
        m_scanMsg.clear();
        if (!pl || !pl->m_player1 || pl->m_player1->m_isDead) { m_scanMsg = "open a level and pause it first"; return; }
        if (!pl->m_isPaused) { m_scanMsg = "pause the level first (it scans while paused)"; return; }
        if (m_attemptPending) {
            // Paused before the first tick: a start position hasn't placed the
            // player yet, so the scan would run from the level's default spawn.
            m_scanMsg = "let the attempt start first (unpause for a moment, then pause and scan)";
            return;
        }
        m_scanTwoPlayer = pl->m_level && pl->m_level->m_twoPlayerMode;
        // (2.2) dual is scanned (both players get the same inputs and both have
        // to live); a 2-player level's dual isn't - player 2 has its own button
        if ((pl->m_gameState.m_isDualMode && m_scanTwoPlayer) || pl->m_player1->m_isPlatformer) {
            m_scanMsg = "can't scan 2-player dual or platformer parts";
            return;
        }
        Config const& c = cfg();
        Ctx x;
        if (!makeCtx(pl, true, c, x)) { m_scanMsg = "can't scan from here"; return; }
        ensureSimPlayers(pl);
        if (!m_simPlayer2.data()) m_simPlayer2 = createSimPlayer(pl); // (2.2) for dual portals on the way
        m_scanDt = x.dt;
        m_scanS = x.s;
        m_scanMode = ModeInfo{};
        m_scanMode.add(pl->m_player1);
        m_scanSavedOrbs = m_realUsedOrbs;
        m_scanSteps = 0;
        m_scanInputs = 0;
        m_scanJumps = 0;
        m_scanLastMode = -1;
        m_rewind.clear();
        m_rewindLastX = -1e9f;
        m_scanWideUntil = m_scanWideAt = -1e9f;
        m_scanWideLevel = 1;
        m_astarTriedAt = -1e9f;
        m_scanAStar = 0;
        m_scanSkips = m_scanBackups = m_scanReal = 0;
        m_scanSkipRanges.clear();
        m_scanDeathPts.clear();
        saveAnchors();
        m_scanLastWhat.clear();
        m_beam.clear();
        m_beamHist.clear();
        m_scanModeNames.clear();
        m_scanPortalTraces = 0;
        m_scanModeSwitches = 0;
        BeamNode root;
        root.fromReal = true;
        root.mode = scanModeIndex(pl->m_player1);
        root.modeCode = modeCode(pl->m_player1);
        root.held = m_gameHolding;
        root.x = pl->m_player1->getPositionX();
        root.y = pl->m_player1->getPositionY();
        root.vy = pl->m_player1->m_yVelocity;
        root.mini = pl->m_player1->m_vehicleSize > 0.f && pl->m_player1->m_vehicleSize < 0.99f;
        if (pl->m_gameState.m_isDualMode && pl->m_player2) {
            root.dual = true;
            root.snap2 = snapFrom(pl->m_player2);
            root.y2 = pl->m_player2->getPositionY();
        }
        m_beamHist.push_back(BeamHist{-1, root.x, root.y, false, root.held, static_cast<uint8_t>(root.mode)});
        root.hist = 0;
        m_beam.push_back(std::move(root));
        m_scanStartX = m_scanX = pl->m_player1->getPositionX();
        m_scanEndX = std::max(m_scanStartX + 1.f, pl->m_levelLength);
        m_scanState = ScanState::Running;
        m_wdX = m_scanX;
        m_wdUs = 0;
        m_spResumeX = -1e9f;
        m_spRuns = 0;
        spStart(m_beam.front(), pl); // (2.2) sprint first
        trace(fmt::format("scan start at ({:.1f},{:.1f}) mode={} speed={:.2f} level length={:.0f}", m_scanStartX,
                          pl->m_player1->getPositionY(), m_scanMode.name(), pl->m_player1->m_playerSpeed, pl->m_levelLength));
    }

    // The best version's inputs (furthest, then fewest inputs) -> the route.
    void scanBuildRoute(std::vector<BeamNode> const& from, std::vector<RoutePt>& out,
                        std::vector<acecore::Point2>* pathOut = nullptr) {
        out.clear();
        if (pathOut) pathOut->clear();
        if (from.empty()) return;
        size_t best = 0;
        for (size_t i = 1; i < from.size(); i++) {
            if (from[i].x > from[best].x + 0.5f ||
                (std::fabs(from[i].x - from[best].x) <= 0.5f && from[i].cost < from[best].cost)) best = i;
        }
        if (pathOut) pathOut->push_back(acecore::Point2{from[best].x, from[best].y, 0.f});
        for (int h = from[best].hist; h >= 0; h = m_beamHist[static_cast<size_t>(h)].parent) {
            BeamHist const& e = m_beamHist[static_cast<size_t>(h)];
            if (pathOut) pathOut->push_back(acecore::Point2{e.x, e.y, 0.f});
            if (!e.toggled) continue;
            RoutePt rp;
            rp.x = e.x;
            rp.y = e.y;
            rp.push = e.held;
            rp.x0 = rp.x1 = e.x; // snapping goes to this exact spot
            rp.mode = e.mode < m_scanModeNames.size() ? m_scanModeNames[e.mode] : m_scanMode.name();
            out.push_back(rp);
        }
        std::reverse(out.begin(), out.end());
        if (pathOut) std::reverse(pathOut->begin(), pathOut->end());
    }

    // The scanned stretch so far ([m_scanStartX, furthest]) -> saved route.
    void commitSegment(std::vector<BeamNode> const& from) {
        if (from.empty()) return;
        std::vector<RoutePt> found;
        std::vector<acecore::Point2> path;
        scanBuildRoute(from, found, &path);
        m_scanInputs += static_cast<int>(found.size());
        float hi = m_scanX;
        for (auto const& r : found) hi = std::max(hi, r.x);
        if (hi > m_scanStartX + 1.f) routeMerge(found, m_scanStartX, hi, path);
    }

    // Stuck (or at something it can't simulate): carry on from your own run,
    // past this spot. False: no run of yours got past here yet.
    bool scanJump(char const* why) {
        Anchor const* a = anchorAfter(m_scanX);
        if (!a) return false;
        commitSegment(m_beam);
        float const from = m_scanX;
        m_beam.clear();
        m_beamHist.clear();
        m_rewind.clear(); // their history is gone
        m_rewindLastX = -1e9f;
        BeamNode root;
        root.fromReal = false;
        root.snap = a->snap;
        root.held = a->held;
        root.modeCode = a->snap.mode;
        root.mode = 0;
        {
            auto it = std::find(m_scanModeNames.begin(), m_scanModeNames.end(), a->modeName);
            if (it == m_scanModeNames.end()) { m_scanModeNames.push_back(a->modeName); it = m_scanModeNames.end() - 1; }
            root.mode = static_cast<int>(it - m_scanModeNames.begin());
        }
        root.x = a->x;
        root.y = a->snap.pos.y;
        root.vy = a->snap.m_yVelocity;
        m_beamHist.push_back(BeamHist{-1, root.x, root.y, false, root.held, static_cast<uint8_t>(root.mode)});
        root.hist = 0;
        m_beam.push_back(std::move(root));
        m_scanStartX = m_scanX = a->x;
        m_scanLastMode = a->snap.mode;
        m_scanJumps++;
        trace(fmt::format("scan: {} at x={:.0f} - carrying on from your own run at ({:.1f},{:.1f}) {}", why, from, a->x,
                          a->snap.pos.y, a->modeName));
        return true;
    }

    void finishScan(PlayLayer* pl, std::string msg, std::vector<BeamNode> const* keepFrom) {
        if (m_spOn && !m_spSt.empty()) {
            // stopped mid-sprint: what it found so far still counts (the
            // version list is old then - the sprint has it)
            std::vector<BeamNode> end{spNode(spEnd())};
            commitSegment(end);
            if (keepFrom == &m_beam) keepFrom = nullptr;
        }
        m_spOn = false;
        if (keepFrom) commitSegment(*keepFrom);
        {
            std::string extra;
            auto add = [&](std::string t) { extra += extra.empty() ? t : ", " + t; };
            if (m_scanAStar > 0) add(fmt::format("A* solved {} spot{}", m_scanAStar, m_scanAStar == 1 ? "" : "s"));
            if (m_scanBackups > 0) add(fmt::format("tried harder {}x", m_scanBackups));
            if (m_scanJumps > 0) add(fmt::format("used your run {}x", m_scanJumps));
            if (m_scanReal > 0) add(fmt::format("used the real game {}x", m_scanReal));
            if (m_scanSkips > 0) add(fmt::format("went through {} spot{} it couldn't solve", m_scanSkips, m_scanSkips == 1 ? "" : "s"));
            if (!extra.empty()) msg += " (" + extra + ")";
        }
        m_realUsedOrbs = m_scanSavedOrbs;
        m_beam.clear();
        m_beamHist.clear();
        m_beamHist.shrink_to_fit();
        m_scanMsg = std::move(msg);
        m_scanState = ScanState::Done;
        trace(fmt::format("scan: {} (gamemode portals passed in tests: {})", m_scanMsg, m_scanModeSwitches));
        // short: the full message is on the Tools tab
        Notification::create(fmt::format("Level scan: {:.0f}% done", pctAt(pl, m_scanX)), NotificationIcon::None, 1.5f)->show();
    }

    // Called once per frame by the scan runner.
    void scanSlice() {
        if (!scanning()) return;
        PlayLayer* pl = PlayLayer::get();
        if (!pl || !m_simPlayer1.data() || !pl->m_player1) {
            m_scanState = ScanState::Off; m_beam.clear(); m_beamHist.clear(); m_scanMsg = "stopped (left the level)"; return;
        }
        if (!pl->m_isPaused) {
            finishScan(pl, fmt::format("stopped at {:.0f}% - stay paused while it scans", pctAt(pl, m_scanX)), &m_beam);
            return;
        }
        auto const t0 = Clock::now();
        // (2.2) the sprint goes first; the version search only where it's stuck
        if (m_spOn) {
            spSlice(pl, t0);
            if (scanning()) drawScanView(pl);
            return;
        }
        // (2.2) Watchdog: a scan always finishes. A spot the version search
        // hasn't got 1 block past in 8 seconds of scanning is gone through
        // (marked unsolved), and the scan carries on.
        if (m_scanX > m_wdX + 30.f) { m_wdX = m_scanX; m_wdUs = 0; }
        if (m_wdUs > 8000000) {
            m_wdUs = 0;
            m_wdX = m_scanX + 30.f;
            trace(fmt::format("scan: no progress at x={:.0f} for 8 s - going through it", m_scanX));
            if (scanSkip(pl, m_scanX, false)) {
                if (scanning() && !m_beam.empty()) { spStart(m_beam.front(), pl); m_spResumeX = -1e9f; }
                return;
            }
        }
        // the version search got past the sprint's stuck spot: sprint on
        if (!m_beam.empty() && m_scanX >= m_spResumeX && m_spResumeX > -1e8f && m_scanX >= m_scanWideUntil) {
            size_t best = 0;
            for (size_t i = 1; i < m_beam.size(); i++) {
                if (m_beam[i].x > m_beam[best].x + 0.5f ||
                    (std::fabs(m_beam[i].x - m_beam[best].x) <= 0.5f && m_beam[i].cost < m_beam[best].cost)) best = i;
            }
            m_spResumeX = -1e9f;
            spStart(m_beam[best], pl);
            trace(fmt::format("scan: past the spot at x={:.0f} - sprinting again", m_beam[best].x));
            return;
        }
        struct WdTimer { long long& us; Clock::time_point t; ~WdTimer() { us += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t).count(); } } wdTimer{m_wdUs, t0};
        while (std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count() < SCAN_SLICE_US) {
            // Step length: finer while any version is a wave (all versions stay
            // on the same tick), and every tick while trying harder.
            bool const wide = m_scanX < m_scanWideUntil;
            // (2.2) each retry at the same spot doubles the versions and halves
            // the height buckets (256 / 512 / 1024, 1.5 / 0.75 / 0.375 px)
            int const lvl = std::clamp(m_scanWideLevel, 1, SCAN_TRIES);
            int const beamCap = wide ? SCAN_BEAM_WIDE << (lvl - 1) : SCAN_BEAM;
            float const bucket = wide ? SCAN_Y_BUCKET_WIDE / static_cast<float>(1 << (lvl - 1)) : SCAN_Y_BUCKET;
            bool anyWave = false, anyMiniWave = false;
            for (auto const& b : m_beam) {
                if (b.modeCode == 4) anyWave = true;
                if (b.modeCode == 4 && b.mini) anyMiniWave = true;
            }
            int const k = wide ? 1
                        : anyWave ? std::max(1, static_cast<int>(std::lround(SCAN_STEP_WAVE * m_scanS)))
                                  : std::max(1, scaleTicks(SCAN_STEP_BASE, m_scanS));
            std::vector<BeamNode> next;
            next.reserve(m_beam.size() * 2);
            bool trunc = false;
            TruncInfo truncInfo;
            for (BeamNode const& n : m_beam) {
                // Ship / wave / robot: the button state matters - "keep" or
                // "switch" (press / let go). Everything else: "nothing" or a TAP
                // (press, let go a tick later) - holding a cube just bunny-hops,
                // and the guide wants one dot per jump (2.1).
                // (2.2) Cube holds too: holding re-jumps the moment you land and
                // carries a press into an orb - real tricks the tap-only scan
                // never tried. Cube: keep / switch the button / a tap.
                // (2.2) Ball and spider too: a press held into the landing
                // (GD's buffer click - it fires the moment you touch down).
                // Ship: also a quick tap (press, let go a tick later) - GD's
                // straight fly is taps, not holds.
                bool const cubeHold = n.modeCode == 0 || n.modeCode == 2 || n.modeCode == 6;
                bool const shipTap = n.modeCode == 1;
                bool const holdMode = n.modeCode == 1 || n.modeCode == 4 || n.modeCode == 5 || cubeHold;
                for (int sw = 0; sw < ((cubeHold || shipTap) ? 3 : 2); sw++) {
                    if (sw == 2 && n.held) continue; // a tap needs the button up
                    bool const isTap = (!holdMode && sw == 1) || sw == 2;
                    bool const toggle = sw >= 1;
                    bool const held = isTap ? false : (holdMode ? (sw == 1 ? !n.held : n.held) : false);
                    bool const inputPush = isTap ? true : held;
                    // run k ticks from this version
                    bool const g = m_gameHolding;
                    float const ph = m_phase;
                    bool const rec = m_recordPath;
                    m_gameHolding = holdMode ? n.held : false;
                    m_phase = 0.f;
                    m_recordPath = false;
                    m_hasDeadline = false;
                    m_realUsedOrbs = m_scanSavedOrbs;
                    for (auto* o : n.orbs) m_realUsedOrbs.push_back(o);
                    m_simSnap = n.fromReal ? nullptr : &n.snap;
                    m_simSnap2 = n.dual ? &n.snap2 : nullptr;
                    m_scanSim = true;
                    int const delay = toggle ? 0 : k;
                    FollowUp tap;
                    if (isTap) { tap.at[0] = 1; tap.held[0] = false; } // let go a tick later
                    SimResult const r = simulate(pl, pl->m_player1, nullptr, false, isTap ? true : held, delay,
                                                 k - delay, true, m_scanDt, m_scanS, tap);
                    m_scanSim = false;
                    m_simSnap = nullptr;
                    m_simSnap2 = nullptr;
                    m_gameHolding = g;
                    m_phase = ph;
                    m_recordPath = rec;
                    m_scanSteps += k;
                    if (r.end == SimEnd::Died) {
                        m_scanLastWhat = deathWhat(r) + fmt::format(" - at ({:.0f},{:.0f})", m_simKillPos.x, m_simKillPos.y);
                        m_scanDeathPts.push_back(m_simKillPos);
                        if (m_scanDeathPts.size() > 300) m_scanDeathPts.pop_front();
                        continue;
                    }
                    if (r.end == SimEnd::Truncated) { trunc = true; truncInfo = r.trunc; continue; }
                    PlayerObject* s1 = m_simPlayer1.data();
                    BeamNode c;
                    c.snap = snapFrom(s1);
                    c.fromReal = false;
                    c.held = held;
                    c.toggles = n.toggles + (toggle ? 1 : 0);
                    // nicer route: fewer inputs, and not brushing past spikes
                    float const clr = m_scanClr;
                    float const close = clr < SCAN_SAFE_CLR ? (SCAN_SAFE_CLR - std::max(0.f, clr)) / SCAN_SAFE_CLR : 0.f;
                    c.cost = n.cost + (toggle ? 1.f : 0.f) + SCAN_CLOSE_COST * close;
                    c.x = s1->getPositionX();
                    c.y = s1->getPositionY();
                    c.vy = s1->m_yVelocity;
                    c.mode = scanModeIndex(s1);
                    c.modeCode = modeCode(s1);
                    c.mini = s1->m_vehicleSize > 0.f && s1->m_vehicleSize < 0.99f;
                    // ship: flying at an angle costs a little (straight fly is kept)
                    if (c.modeCode == 1) c.cost += SCAN_SHIP_LEVEL * std::min(1.f, static_cast<float>(std::fabs(c.vy)) / SHIP_TILT_VY);
                    c.dual = m_simP2Live;
                    if (c.dual && m_simPlayer2.data()) {
                        c.snap2 = snapFrom(m_simPlayer2.data());
                        c.y2 = m_simPlayer2->getPositionY();
                    }
                    if (c.modeCode != n.modeCode && m_scanPortalTraces < 40) {
                        m_scanPortalTraces++;
                        trace(fmt::format("scan: mode {} -> {} at ({:.1f},{:.1f}) vy={:.2f} onGround={} lastPortalPos=({:.1f},{:.1f})",
                                          n.modeCode, c.modeCode, c.x, c.y, c.vy, s1->m_isOnGround, s1->m_lastPortalPos.x,
                                          s1->m_lastPortalPos.y));
                    }
                    c.orbs = n.orbs;
                    if (m_simOrbObj) c.orbs.push_back(m_simOrbObj);
                    // a tap is recorded as one press (no release); ship / wave / robot keep both
                    c.pend = BeamHist{n.hist, n.x, n.y, toggle, inputPush, static_cast<uint8_t>(n.mode)};
                    // (2.2) a ship tap: its release too, half a step on
                    c.tapRel = isTap && shipTap;
                    if (c.tapRel) c.pend2 = BeamHist{-1, (n.x + c.x) * 0.5f, (n.y + c.y) * 0.5f, true, false, static_cast<uint8_t>(n.mode)};
                    next.push_back(std::move(c));
                }
            }
            m_realUsedOrbs = m_scanSavedOrbs;
            if (next.empty() && scanRecover(pl, trunc)) {
                if (!scanning()) return;
                continue;
            }
            if (next.empty()) {
                if (trunc) {
                    finishScan(pl, fmt::format("stopped at {:.0f}%: {} - play past it once, then scan again", pctAt(pl, m_scanX),
                                               describeTrunc(truncInfo)), &m_beam);
                } else {
                    // what the last versions looked like, for the report
                    for (auto const& b : m_beam) {
                        trace(fmt::format("scan: stuck version at ({:.1f},{:.1f}) vy={:.2f} mode={} held={} onGround={} inputs={}",
                                          b.x, b.y, b.vy, b.modeCode, b.held, b.fromReal ? false : b.snap.m_isOnGround, b.toggles));
                    }
                    std::string const where = m_beam.empty() || static_cast<size_t>(m_beam.front().mode) >= m_scanModeNames.size()
                        ? m_scanMode.name() : m_scanModeNames[static_cast<size_t>(m_beam.front().mode)];
                    finishScan(pl, fmt::format("stuck at {:.0f}% ({}): every path dies there ({}) - play past it once, then scan again",
                                               pctAt(pl, m_scanX), where, m_scanLastWhat), &m_beam);
                }
                return;
            }
            // Thin out (2.1 rework): one version per (button, height, falling
            // speed, gamemode) bucket - the cheapest - and if that's still too
            // many, keep them SPREAD over the whole height range. (It used to
            // keep the fewest-input ones: all versions ended up as near-copies
            // of "don't press" a few px apart, flew into the same wall, and a
            // late switch couldn't save any of them - "every path dies".)
            std::stable_sort(next.begin(), next.end(), [](BeamNode const& l, BeamNode const& r) { return l.cost < r.cost; });
            std::vector<BeamNode> uniq;
            uniq.reserve(next.size());
            {
                // (2.2) mini wave: twice as steep - heights twice as fine; dual: player 2's height counts too
                float const yb = anyMiniWave ? bucket * 0.5f : bucket;
                std::set<std::tuple<int, int, int, int, int>> seen;
                for (auto& c : next) {
                    auto const key = std::make_tuple(c.held ? 1 : 0, static_cast<int>(std::lround(c.y / yb)),
                                                     static_cast<int>(std::lround(c.vy * 2.0)), c.mode,
                                                     c.dual ? static_cast<int>(std::lround(c.y2 / yb)) + 1 : 0);
                    if (!seen.insert(key).second) continue;
                    uniq.push_back(std::move(c));
                }
            }
            std::vector<BeamNode> kept;
            if (uniq.size() <= static_cast<size_t>(beamCap)) {
                kept = std::move(uniq);
            } else {
                // uniq[0] is the cheapest: always kept. The rest evenly by height.
                kept.reserve(beamCap);
                kept.push_back(std::move(uniq[0]));
                std::vector<size_t> order(uniq.size() - 1);
                for (size_t i = 0; i < order.size(); i++) order[i] = i + 1;
                std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                    return uniq[a].y < uniq[b].y || (uniq[a].y == uniq[b].y && uniq[a].cost < uniq[b].cost);
                });
                size_t const want = static_cast<size_t>(beamCap) - 1;
                size_t last = SIZE_MAX;
                for (size_t i = 0; i < want; i++) {
                    size_t const j = (order.size() - 1) * i / (want - 1);
                    if (j == last) continue;
                    last = j;
                    kept.push_back(std::move(uniq[order[j]]));
                }
            }
            for (auto& c : kept) {
                m_beamHist.push_back(c.pend);
                c.hist = static_cast<int>(m_beamHist.size()) - 1;
                if (c.tapRel) {
                    c.pend2.parent = c.hist;
                    m_beamHist.push_back(c.pend2);
                    c.hist = static_cast<int>(m_beamHist.size()) - 1;
                    c.tapRel = false;
                }
            }
            m_beam = std::move(kept);
            // Through a gamemode portal: GD does more on a real portal entry
            // than the hidden player's switch (Amethyst's UFO died right after
            // it in every version). If your own run has a state just past it
            // in the same gamemode, carry on from that.
            if (!m_beam.empty() && m_beam.front().modeCode != m_scanLastMode) {
                int const nm = m_beam.front().modeCode;
                bool const switched = m_scanLastMode >= 0;
                m_scanLastMode = nm;
                if (switched) {
                    float bx = m_scanX;
                    for (auto const& b : m_beam) bx = std::max(bx, b.x);
                    m_scanX = bx;
                    Anchor const* a = anchorAfter(bx);
                    if (a && a->x <= bx + 3.f * ANCHOR_STEP && a->snap.mode == nm) {
                        scanJump("through a gamemode portal");
                        continue;
                    }
                }
            }
            float maxX = m_scanX;
            for (auto const& b : m_beam) maxX = std::max(maxX, b.x);
            m_scanX = maxX;
            // a copy every few blocks to back up to when stuck
            float bmax = -1e9f;
            for (auto const& b : m_beam) bmax = std::max(bmax, b.x);
            if (bmax >= m_rewindLastX + SCAN_REWIND_EVERY) {
                m_rewind.emplace_back(bmax, m_beam);
                m_rewindLastX = bmax;
                while (m_rewind.size() > static_cast<size_t>(SCAN_REWIND_KEEP)) m_rewind.pop_front();
            }
            // (A version that reached something it can't see past is dropped;
            // the scan only ends there when no version gets around it.)
            if (m_scanX >= m_scanEndX - 1.f) {
                finishScan(pl, "done - to the end of the level", &m_beam);
                return;
            }
            if (m_beamHist.size() > 1500000) compactHist(); // drop dead branches' history
            if (m_beamHist.size() > 1500000) scanRebase();   // still huge: save what's found, carry on fresh
        }
        drawScanView(pl);
    }

    // Save the route so far and carry on with the same versions, fresh history
    // (a very long level - it never stops for that).
    void scanRebase() {
        commitSegment(m_beam);
        m_beamHist.clear();
        m_rewind.clear();
        m_rewindLastX = -1e9f;
        for (auto& b : m_beam) {
            m_beamHist.push_back(BeamHist{-1, b.x, b.y, false, b.held, static_cast<uint8_t>(b.mode)});
            b.hist = static_cast<int>(m_beamHist.size()) - 1;
        }
        m_scanStartX = m_scanX;
    }

    // Every version died (or ran into something it can't simulate). In order:
    //  1. back up ~4 blocks and try harder (4x the versions, every tick),
    //  2. carry on from your own run past the spot,
    //  3. practice mode + "Real game" on: find it with the real game,
    //  4. go through the spot (deaths ignored for a block) and carry on -
    //     that spot gets no dots, everything after it does.
    // So the scan always reaches the end.
    bool scanRecover(PlayLayer* pl, bool trunc) {
        float const stuckX = m_scanX;
        // (2.2) at the end of the level nothing is left to solve: done. (It used
        // to "go through" spot after spot past the end - 39 000 of them.)
        if (stuckX >= m_scanEndX - END_SLACK) {
            finishScan(pl, "done - to the end of the level", &m_beam);
            return true;
        }
        // (2.2) A* first: a tick-by-tick search of the spot from a few blocks back.
        if (!trunc && scanAStar(pl, stuckX)) return true;
        // (2.2) Up to SCAN_TRIES tries per spot, each backing up further (4 / 8 /
        // 12 blocks) with more versions and finer heights - it used to try once
        // and then go through the spot unsolved.
        bool const sameSpot = std::fabs(stuckX - m_scanWideAt) <= 60.f;
        int const tryNo = sameSpot ? m_scanWideLevel + 1 : 1;
        if (!trunc && tryNo <= SCAN_TRIES && !m_rewind.empty()) {
            m_scanWideLevel = tryNo;
            size_t pick = 0;
            float const backPx = SCAN_REWIND_PX * static_cast<float>(tryNo);
            for (size_t i = 0; i < m_rewind.size(); i++) if (m_rewind[i].first <= stuckX - backPx) pick = i;
            std::vector<BeamNode> back = m_rewind[pick].second;
            float const bx = m_rewind[pick].first;
            m_rewind.erase(m_rewind.begin() + static_cast<std::ptrdiff_t>(pick) + 1, m_rewind.end());
            m_rewindLastX = bx;
            m_beam = std::move(back);
            if (!sameSpot) m_scanWideAt = stuckX; // the spot stays where it first got stuck
            m_scanWideUntil = stuckX + SCAN_WIDE_PAST;
            m_scanBackups++;
            trace(fmt::format("scan: every version dies at x={:.0f} ({}) - backing up to x={:.0f}, try {} of {}",
                              stuckX, m_scanLastWhat, bx, tryNo, SCAN_TRIES));
            return true;
        }
        if (scanJump(trunc ? "can't simulate what's ahead" : "every version dies")) {
            m_rewind.clear();
            m_rewindLastX = -1e9f;
            return true;
        }
        return scanSkip(pl, stuckX, trunc);
    }

    // Last resort: the furthest version goes through the spot with deaths
    // ignored until a block past it, and the scan carries on from there.
    bool scanSkip(PlayLayer* pl, float stuckX, bool trunc) {
        if (stuckX >= m_scanEndX - END_SLACK) { // (2.2) never past the end
            finishScan(pl, "done - to the end of the level", &m_beam);
            return true;
        }
        if (m_beam.empty() || !pl) return false;
        size_t best = 0;
        for (size_t i = 1; i < m_beam.size(); i++) {
            if (m_beam[i].x > m_beam[best].x + 0.5f ||
                (std::fabs(m_beam[i].x - m_beam[best].x) <= 0.5f && m_beam[i].cost < m_beam[best].cost)) best = i;
        }
        BeamNode cur = m_beam[best];
        bool const holdMode = cur.modeCode == 1 || cur.modeCode == 4 || cur.modeCode == 5 || cur.modeCode == 0 ||
                              cur.modeCode == 2 || cur.modeCode == 6;
        float const target = stuckX + SCAN_SKIP_PAST;
        for (int chunk = 0; chunk < 900 && cur.x < target; chunk++) {
            bool const g = m_gameHolding;
            float const ph = m_phase;
            bool const rec = m_recordPath;
            m_gameHolding = holdMode ? cur.held : false;
            m_phase = 0.f;
            m_recordPath = false;
            m_hasDeadline = false;
            m_realUsedOrbs = m_scanSavedOrbs;
            for (auto* o : cur.orbs) m_realUsedOrbs.push_back(o);
            m_simSnap = cur.fromReal ? nullptr : &cur.snap;
            m_simSnap2 = cur.dual ? &cur.snap2 : nullptr;
            m_scanSim = true;
            m_scanNoclip = true;
            int const k = std::max(1, scaleTicks(4, m_scanS));
            SimResult const r = simulate(pl, pl->m_player1, nullptr, false, holdMode ? cur.held : false, k, 0, true,
                                         m_scanDt, m_scanS);
            m_scanNoclip = false;
            m_scanSim = false;
            m_simSnap = nullptr;
            m_simSnap2 = nullptr;
            m_gameHolding = g;
            m_phase = ph;
            m_recordPath = rec;
            m_realUsedOrbs = m_scanSavedOrbs;
            PlayerObject* s1 = m_simPlayer1.data();
            if (!s1 || r.end == SimEnd::OutOfBudget) break;
            float const nx = s1->getPositionX();
            cur.snap = snapFrom(s1);
            cur.fromReal = false;
            cur.y = s1->getPositionY();
            cur.vy = s1->m_yVelocity;
            cur.mode = scanModeIndex(s1);
            cur.modeCode = modeCode(s1);
            cur.mini = s1->m_vehicleSize > 0.f && s1->m_vehicleSize < 0.99f;
            cur.dual = m_simP2Live;
            if (cur.dual && m_simPlayer2.data()) {
                cur.snap2 = snapFrom(m_simPlayer2.data());
                cur.y2 = m_simPlayer2->getPositionY();
            }
            if (nx <= cur.x + 0.01f) { cur.x = nx; break; } // not moving (stuck in a wall)
            cur.x = nx;
        }
        if (cur.x <= stuckX + 1.f) {
            finishScan(pl, fmt::format("stopped at {:.0f}%: can't get past x={:.0f} even ignoring deaths{}", pctAt(pl, stuckX),
                                       stuckX, trunc ? "" : (" (" + m_scanLastWhat + ")")), &m_beam);
            return true;
        }
        commitSegment(m_beam);
        m_beam.clear();
        m_beamHist.clear();
        m_rewind.clear();
        m_rewindLastX = -1e9f;
        cur.toggles = 0;
        cur.cost = 0.f;
        m_beamHist.push_back(BeamHist{-1, cur.x, cur.y, false, cur.held, static_cast<uint8_t>(cur.mode)});
        cur.hist = 0;
        float const nx = cur.x;
        m_scanLastMode = cur.modeCode;
        m_beam.push_back(std::move(cur));
        m_scanSkipRanges.emplace_back(stuckX, nx);
        m_scanSkips++;
        m_scanStartX = m_scanX = nx;
        trace(fmt::format("scan: couldn't solve x={:.0f} ({}) - went through to x={:.0f} and carried on", stuckX,
                          trunc ? "can't simulate it" : m_scanLastWhat, nx));
        return true;
    }

    // ------------------------------------------------------------------------
    //  Sprint (2.2): the scan's fast way forward. One hidden player plays on
    //  with the button as it is; when it dies, it goes back a little and tries
    //  the input one step earlier at a time (latest first - like a player who
    //  clicks as late as is safe), keeping the first that survives well past
    //  the death with a little room from hazards. Mostly it's one simulation
    //  per chunk of the level instead of dozens of versions every 2 ticks, so
    //  the scan gets through long levels. Where no input in its last second
    //  saves it, the version search (beam, A*, tries) takes that spot, and the
    //  sprint carries on a few blocks past it.
    // ------------------------------------------------------------------------
    static constexpr int   SP_CHUNK_BASE  = 60;   // ticks (240 TPS) per forward run
    static constexpr int   SP_BACK_BASE   = 240;  // how far back an input is tried (1 s)
    static constexpr int   SP_MARGIN_BASE = 48;   // a try has to live this long past the old death
    static constexpr float SP_SAFE_CLR    = 4.f;  // px from hazards for a try to count as "safe"
    static constexpr int   SP_WINDOW_BASE = 30;   // (2.3) how far before the latest safe input to look for a better one
    // (2.3) Which of the safe inputs before an obstacle to use. Taking the
    // latest one always clicked as late as possible (barely clearing - late
    // clicks on easy levels). Now: the ones with the most room from hazards
    // (within 1px of the best), and of those the middle one - the centre of
    // the window, where a human clicks and where a small timing error still lives.
    static int pickSafe(std::vector<std::pair<int, float>> const& safe) {
        if (safe.empty()) return -1;
        float best = -1.f;
        for (auto const& [t, c] : safe) best = std::max(best, c);
        std::vector<int> top;
        for (auto const& [t, c] : safe)
            if (c >= best - 1.f) top.push_back(t);
        std::sort(top.begin(), top.end());
        return top[top.size() / 2];
    }
    static constexpr float SP_RESUME_PX   = 90.f; // after the version search solved a spot, sprint again this far on
    bool  m_spOn = false;
    std::deque<SpTick> m_spSt;        // states: m_spSt[i] is the state at tick m_spBase + i
    std::deque<uint8_t> m_spHeld;     // the button during tick m_spBase + i
    std::map<int, bool> m_spEvents;   // inputs: tick -> press / release (taps: press only)
    int   m_spBase = 0;
    int   m_spHist = -1;              // the history entry the committed part ends on
    int   m_spLastEvent = -1;         // never tries an input at or before the last one
    int   m_spLastCommit = 0;         // ticks before this are in the history already
    std::vector<GameObject*> m_spOrbs;
    float m_spResumeX = -1e9f;
    float m_wdX = -1e9f;              // watchdog: where the version search last made progress
    long long m_wdUs = 0;             // ... and the scanning time since
    long long m_spRuns = 0;

    int spEnd() const { return m_spBase + static_cast<int>(m_spSt.size()) - 1; } // the newest tick
    SpTick const& spAt(int t) const { return m_spSt[static_cast<size_t>(t - m_spBase)]; }
    bool spHeldAt(int t) const {
        if (t < m_spBase) return false;
        size_t const i = static_cast<size_t>(t - m_spBase);
        return i < m_spHeld.size() ? m_spHeld[i] != 0 : (m_spHeld.empty() ? false : m_spHeld.back() != 0);
    }
    static bool spTapMode(int mc) { return !(mc == 1 || mc == 4 || mc == 5); } // ship / wave / robot hold; the rest tap
    static SpTick spFromBeam(BeamNode const& b, PlayLayer* pl) {
        SpTick t;
        t.s1 = b.fromReal && pl && pl->m_player1 ? snapFrom(pl->m_player1) : b.snap;
        t.dual = b.dual;
        t.s2 = b.snap2;
        t.x = b.x; t.y = b.y; t.y2 = b.y2; t.vy = b.vy;
        t.modeCode = b.modeCode; t.modeIdx = b.mode; t.mini = b.mini;
        return t;
    }

    // Start sprinting from a version (its history goes on from it).
    void spStart(BeamNode const& b, PlayLayer* pl) {
        m_spSt.clear();
        m_spHeld.clear();
        m_spEvents.clear();
        m_spSt.push_back(spFromBeam(b, pl));
        m_spHeld.push_back(b.held ? 1 : 0);
        m_spBase = 0;
        m_spHist = b.hist;
        m_spLastEvent = -1;
        m_spLastCommit = 0;
        m_spOrbs = b.orbs;
        m_spOn = true;
    }

    // One run from tick `t`: `push` from its start (a tap: let go a tick later),
    // `ticks` long. The states it lived through go to `trail`.
    // `keep`: carry on as is (no new press / release - GD would take a second
    // press of a held button as a fresh click).
    SimResult spRun(PlayLayer* pl, int t, bool push, bool tap, int ticks, std::vector<SpTick>& trail, bool keep = false) {
        SpTick const& from = spAt(t);
        trail.clear();
        bool const g = m_gameHolding;
        float const ph = m_phase;
        bool const rec = m_recordPath;
        // the button going into tick t (carrying on: as it is during tick t)
        m_gameHolding = keep ? spHeldAt(t) : (t > m_spBase ? spHeldAt(t - 1) : (m_spHeld.empty() ? false : m_spHeld.front() != 0));
        m_phase = 0.f;
        m_recordPath = false;
        m_hasDeadline = false;
        m_realUsedOrbs = m_scanSavedOrbs;
        for (auto* o : m_spOrbs) m_realUsedOrbs.push_back(o);
        m_simSnap = &from.s1;
        m_simSnap2 = from.dual ? &from.s2 : nullptr;
        m_scanSim = true;
        m_simTrail = &trail;
        FollowUp f;
        if (tap) { f.at[0] = 1; f.held[0] = false; }
        SimResult const r = keep ? simulate(pl, pl->m_player1, nullptr, false, push, ticks, 0, true, m_scanDt, m_scanS)
                                 : simulate(pl, pl->m_player1, nullptr, false, push, 0, ticks, true, m_scanDt, m_scanS, f);
        m_simTrail = nullptr;
        m_scanSim = false;
        m_simSnap = nullptr;
        m_simSnap2 = nullptr;
        m_gameHolding = g;
        m_phase = ph;
        m_recordPath = rec;
        m_realUsedOrbs = m_scanSavedOrbs;
        m_spRuns++;
        m_scanSteps += ticks;
        return r;
    }

    // Keep tick `t`'s states and everything before it, then add `trail` after it.
    void spAppend(int t, bool held0, bool heldRest, std::vector<SpTick>& trail, GameObject* orb) {
        size_t const keep = static_cast<size_t>(t - m_spBase + 1);
        if (m_spSt.size() > keep) m_spSt.resize(keep);
        m_spHeld.resize(keep);
        for (size_t i = 0; i < trail.size(); i++) {
            m_spSt.push_back(std::move(trail[i]));
            m_spHeld.push_back(heldRest ? 1 : 0);
        }
        // m_spHeld[k] is the button during tick base+k: tick t itself
        m_spHeld[keep - 1] = held0 ? 1 : 0;
        m_spEvents.erase(m_spEvents.upper_bound(t - 1), m_spEvents.end());
        float maxX = m_scanX;
        for (size_t i = keep; i < m_spSt.size(); i++) maxX = std::max(maxX, m_spSt[i].x);
        m_scanX = maxX;
        if (orb) m_spOrbs.push_back(orb);
    }

    // History up to tick `upTo` (inputs, and a point every 8 ticks for the line).
    void spCommit(int upTo) {
        upTo = std::min(upTo, spEnd());
        for (int t = std::max(m_spLastCommit, m_spBase); t < upTo; t++) {
            SpTick const& s = spAt(t);
            auto ev = m_spEvents.find(t);
            if (ev != m_spEvents.end()) {
                m_beamHist.push_back(BeamHist{m_spHist, s.x, s.y, true, ev->second, static_cast<uint8_t>(s.modeIdx)});
                m_spHist = static_cast<int>(m_beamHist.size()) - 1;
            } else if ((t & 7) == 0) {
                m_beamHist.push_back(BeamHist{m_spHist, s.x, s.y, false, spHeldAt(t), static_cast<uint8_t>(s.modeIdx)});
                m_spHist = static_cast<int>(m_beamHist.size()) - 1;
            }
        }
        m_spLastCommit = std::max(m_spLastCommit, upTo);
        // drop what's committed and far enough behind (memory: a long level)
        int const back = 2 * scaleTicks(SP_BACK_BASE, m_scanS);
        while (static_cast<int>(m_spSt.size()) > 1 && m_spBase < m_spLastCommit - back && m_spBase < m_spLastEvent) {
            m_spSt.pop_front();
            m_spHeld.pop_front();
            m_spBase++;
        }
    }

    // The sprint's state at tick t as a version (for the version search / the route).
    BeamNode spNode(int t) {
        spCommit(t);
        SpTick const& s = spAt(t);
        BeamNode b;
        b.snap = s.s1;
        b.fromReal = false;
        b.held = t > m_spBase ? spHeldAt(t - 1) : spHeldAt(t); // the button going into tick t
        b.x = s.x; b.y = s.y; b.vy = s.vy;
        b.mode = s.modeIdx; b.modeCode = s.modeCode; b.mini = s.mini;
        b.dual = s.dual; b.snap2 = s.s2; b.y2 = s.y2;
        b.orbs = m_spOrbs;
        b.hist = m_spHist;
        return b;
    }

    // Hand over to the version search from tick t (the sprint is stuck after it).
    void spToBeam(int t) {
        BeamNode b = spNode(t);
        m_spOn = false;
        m_beam.clear();
        m_beam.push_back(std::move(b));
        m_rewind.clear();
        m_rewindLastX = -1e9f;
        m_scanLastMode = m_beam.front().modeCode;
    }

    // One frame of sprinting. Returns when the slice's time is used.
    void spSlice(PlayLayer* pl, Clock::time_point t0) {
        int const chunk = std::max(4, scaleTicks(SP_CHUNK_BASE, m_scanS));
        int const back = std::max(8, scaleTicks(SP_BACK_BASE, m_scanS));
        int const margin = std::max(4, scaleTicks(SP_MARGIN_BASE, m_scanS));
        std::vector<SpTick> trail;
        while (m_spOn && std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count() < SCAN_SLICE_US) {
            int const T = spEnd();
            SpTick const& cur = spAt(T);
            if (cur.x >= m_scanEndX - 1.f) {
                std::vector<BeamNode> end{spNode(T)};
                m_spOn = false;
                finishScan(pl, "done - to the end of the level", &end);
                return;
            }
            bool const held = spHeldAt(T);
            SimResult const r = spRun(pl, T, held, false, chunk, trail, true);
            if (r.end == SimEnd::Survived) {
                spAppend(T, held, held, trail, m_simOrbObj);
                if (spEnd() - m_spLastCommit > 4 * back) spCommit(spEnd() - back);
                continue;
            }
            if (r.end != SimEnd::Died) { spToBeam(T); return; } // something it can't simulate: the version search decides
            // It dies `trail.size()` ticks on. Keep what it lived through, then try
            // an input one step earlier at a time (latest first), from just
            // before the death back up to a second - never before the last input.
            int const death = T + static_cast<int>(trail.size()) + 1;
            m_scanLastWhat = deathWhat(r) + fmt::format(" - at ({:.0f},{:.0f})", m_simKillPos.x, m_simKillPos.y);
            m_scanDeathPts.push_back(m_simKillPos);
            if (m_scanDeathPts.size() > 300) m_scanDeathPts.pop_front();
            spAppend(T, held, held, trail, nullptr);
            int const mc = spAt(spEnd()).modeCode;
            int const step = mc == 4 ? 1 : std::max(1, scaleTicks(2, m_scanS));
            int const lo = std::max({m_spLastEvent + 1, m_spBase, death - back});
            int bestJ = -1;
            float bestClr = -1.f;
            bool bestTap = false, bestPush = false;
            GameObject* bestOrb = nullptr;
            std::vector<SpTick> bestTrail;
            std::vector<std::pair<int, float>> safe; // (2.3) the run of safe inputs (see pickSafe)
            int const window = std::max(2, scaleTicks(SP_WINDOW_BASE, m_scanS));
            for (int j = std::min(death - 1, spEnd()); j >= lo; j -= step) {
                SpTick const& sj = spAt(j);
                bool const tap = spTapMode(sj.modeCode) && !spHeldAt(j > m_spBase ? j - 1 : j);
                bool const push = tap ? true : !spHeldAt(j > m_spBase ? j - 1 : j);
                int const need = death - j + margin;
                SimResult const rj = spRun(pl, j, push, tap, need, trail);
                bool const ok = rj.end == SimEnd::Survived;
                float const clr = ok ? m_scanClr : -1.f;
                if (ok && clr >= SP_SAFE_CLR) {
                    safe.emplace_back(j, clr);
                } else if (!safe.empty()) {
                    break; // the safe run ended
                }
                if (ok && clr > bestClr) { bestClr = clr; bestJ = j; bestTap = tap; bestPush = push; bestTrail = trail; bestOrb = m_simOrbObj; }
                if (!safe.empty() && safe.front().first - j >= window) break;
                if (std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count() > SCAN_SLICE_US * 4) break;
            }
            if (int const pick = pickSafe(safe); pick >= 0 && pick != bestJ) {
                bool const tap = spTapMode(spAt(pick).modeCode) && !spHeldAt(pick > m_spBase ? pick - 1 : pick);
                bool const push = tap ? true : !spHeldAt(pick > m_spBase ? pick - 1 : pick);
                if (spRun(pl, pick, push, tap, death - pick + margin, trail).end == SimEnd::Survived) {
                    bestJ = pick; bestTap = tap; bestPush = push; bestTrail = trail; bestOrb = m_simOrbObj;
                }
            }
            if (bestJ < 0) {
                // no input in reach saves it: the version search takes this spot
                trace(fmt::format("scan: sprint stuck at x={:.0f} ({}) - the version search takes it", spAt(spEnd()).x, m_scanLastWhat));
                m_spResumeX = spAt(spEnd()).x + SP_RESUME_PX;
                // from a second back (its inputs after that are the search's to
                // redo), on a tick without an input of its own
                int h = std::min(spEnd(), std::max(m_spBase, death - back));
                while (h > m_spBase && m_spEvents.count(h)) h--;
                spToBeam(h);
                return;
            }
            bool const after = bestTap ? false : bestPush;
            spAppend(bestJ, bestTap ? true : bestPush, after, bestTrail, bestOrb);
            m_spEvents[bestJ] = bestPush;
            m_spLastEvent = bestJ;
        }
    }

    // ---- scan view (2.1, Tools: "Show scan"): the versions' paths while it
    // runs (yellow), where they die (red), and stretches it went through
    // without a solution (orange). Stays up until your next attempt.
    void drawScanView(PlayLayer* pl) {
        if (!cfg().scanView || !pl || !pl->m_objectLayer) { releaseScanView(); return; }
        if (!m_scanViewNode.data()) {
            m_scanViewNode = CCDrawNode::create();
            if (!m_scanViewNode.data()) return;
        }
        if (m_scanViewNode->getParent() != pl->m_objectLayer) {
            if (m_scanViewNode->getParent()) m_scanViewNode->removeFromParent();
            pl->m_objectLayer->addChild(m_scanViewNode.data(), 9997);
        }
        m_scanViewNode->clear();
        ccColor4F const yellow{1.f, 0.85f, 0.2f, 0.55f};
        ccColor4F const red{1.f, 0.25f, 0.25f, 0.9f};
        ccColor4F const orange{1.f, 0.55f, 0.1f, 0.9f};
        size_t const stride = std::max<size_t>(1, m_beam.size() / 48); // at most ~48 drawn (trying harder runs 256)
        for (size_t bi = 0; bi < m_beam.size(); bi += stride) {
            BeamNode const& b = m_beam[bi];
            CCPoint prev{b.x, b.y};
            int h = b.hist;
            for (int n = 0; h >= 0 && n < 100 && static_cast<size_t>(h) < m_beamHist.size(); n++) {
                BeamHist const& e = m_beamHist[static_cast<size_t>(h)];
                CCPoint const cur{e.x, e.y};
                m_scanViewNode->drawSegment(prev, cur, 0.4f, yellow);
                prev = cur;
                h = e.parent;
            }
        }
        for (auto const& d : m_scanDeathPts) {
            // (2.2) only around where it's working now (10 blocks back): older
            // dots from spots it already solved / backed up from were just noise
            if (d.x < m_scanX - 300.f) continue;
            CCPoint pts[6];
            for (int i = 0; i < 6; i++) {
                float const a = 6.2831853f * static_cast<float>(i) / 6.f;
                pts[i] = CCPoint{d.x + 2.f * std::cos(a), d.y + 2.f * std::sin(a)};
            }
            m_scanViewNode->drawPolygon(pts, 6, red, 0.f, ccColor4F{0.f, 0.f, 0.f, 0.f});
        }
        for (auto const& r : m_scanSkipRanges) {
            m_scanViewNode->drawSegment(CCPoint{r.first, pl->m_player1 ? pl->m_player1->getPositionY() : 0.f},
                                        CCPoint{r.second, pl->m_player1 ? pl->m_player1->getPositionY() : 0.f}, 1.2f, orange);
        }
    }
    void releaseScanView() {
        if (m_scanViewNode.data() && m_scanViewNode->getParent()) m_scanViewNode->removeFromParent();
        m_scanViewNode.swap(nullptr);
    }

    // ------------------------------------------------------------------------
    //  A* pathfinding (2.2): the search behind the live pathfinder (Show Rays)
    //  and the level scan's stuck spots.
    //
    //  A state = the hidden player after some ticks (its full saved state) +
    //  whether the button is held. From each state two moves: keep doing what
    //  you do, or switch (press / let go; cube-type modes: a tap) - each move
    //  is `k` ticks of the real game physics (wave 1 tick, ship 2, the rest 4,
    //  at 240 TPS). Cost g = ticks so far + 6 per input + a penalty for
    //  brushing past spikes; estimate h = ticks still needed to reach the goal
    //  at your speed. It always expands the state with the lowest g + h (ties:
    //  the furthest one), never a state it has already seen (same tick, height,
    //  falling speed, button, gamemode), and a state that dies is dropped - so
    //  it goes deep along the best line, and backs up to the next best the
    //  moment that line dies, instead of keeping a fixed set of versions.
    // ------------------------------------------------------------------------
    struct ANode {
        PlayerSnap snap;
        bool  useSnap = true;     // false: the real player as it is (paused scan start)
        bool  held = false;
        bool  inputPush = true;   // what the move into this state pressed (if toggled)
        bool  toggled = false;
        int   modeCode = 0;
        int   modeIdx = 0;        // scan: index in m_scanModeNames
        float x = 0.f, y = 0.f;
        double vy = 0.0;
        int   tick = 0;
        float g = 0.f;
        int   parent = -1;
        int   hist = -1;          // scan: the start state's history entry
        std::vector<GameObject*> orbs;
        bool  mini = false;       // (2.2) mini size (mini wave: finer heights)
        bool  dual = false;       // (2.2) player 2 is in it too (same inputs)
        PlayerSnap snap2;
        float y2 = 0.f;
        bool  tapRel = false;     // (2.2) the move was a ship tap: a release half a step on
    };
    struct AStarRun {
        std::vector<ANode> nodes;
        std::priority_queue<std::tuple<float, int, int>> open; // (-f, tick, index)
        std::unordered_set<uint64_t> seen;
        float goalX = 0.f, dir = 1.f, speed = 0.05f;
        float dt = BASE_DT, s = 1.f;
        int   expansions = 0;
        int   deepest = -1;       // furthest state so far
        int   goal = -1;          // reached the goal
    };
    static constexpr float ASTAR_INPUT_COST = 6.f;  // an input "costs" 6 ticks: fewer inputs when it can
    static constexpr float ASTAR_CLOSE_COST = 3.f;  // per move spent right at a spike
    static constexpr float ASTAR_Y_BUCKET   = 1.5f; // states closer in height than this are the same

    static int astarStep(int modeCode, float s) {
        if (modeCode == 4) return std::max(1, scaleTicks(1, s)); // wave: every tick (release precision)
        if (modeCode == 1) return std::max(1, scaleTicks(2, s)); // ship
        return std::max(1, scaleTicks(4, s));
    }
    static uint64_t astarKey(ANode const& n) {
        // (2.2) mini wave is twice as steep: heights twice as fine
        float const yb = (n.modeCode == 4 && n.mini) ? ASTAR_Y_BUCKET * 0.5f : ASTAR_Y_BUCKET;
        uint64_t h = static_cast<uint64_t>(n.tick) * 1000003ull;
        h ^= static_cast<uint64_t>(static_cast<int64_t>(std::lround(n.y / yb)) + 1000000) * 0x9E3779B97F4A7C15ull;
        if (n.dual) h ^= static_cast<uint64_t>(static_cast<int64_t>(std::lround(n.y2 / yb)) + 7000000) * 0xD6E8FEB86659FD93ull;
        h ^= static_cast<uint64_t>(static_cast<int64_t>(std::lround(n.vy * 4.0)) + 100000) * 0xC2B2AE3D27D4EB4Full;
        h ^= (n.held ? 1ull : 0ull) << 61;
        h ^= static_cast<uint64_t>(n.modeCode & 15) << 56;
        return h;
    }
    void astarPush(AStarRun& a, ANode&& n) {
        if (!a.seen.insert(astarKey(n)).second) return; // been here
        float const remain = std::max(0.f, (a.goalX - n.x) * a.dir);
        float const f = n.g + remain / std::max(0.05f, a.speed);
        int const idx = static_cast<int>(a.nodes.size());
        int const tick = n.tick;
        bool const furthest = a.deepest < 0 || (n.x - a.nodes[static_cast<size_t>(a.deepest)].x) * a.dir > 0.f;
        a.nodes.push_back(std::move(n));
        if (furthest) a.deepest = idx;
        if ((a.nodes.back().x - a.goalX) * a.dir >= 0.f) { a.goal = idx; return; }
        a.open.emplace(-f, tick, idx);
    }
    // One move from state `n`: 0 keep, 1 switch the button (cube-type: a
    // tap), 2 a tap (cube, button up: cube holds re-jump / carry into orbs,
    // so a cube has both). False = died / can't tell / not a move here.
    bool astarMove(PlayLayer* pl, AStarRun const& a, ANode const& n, int move, std::vector<GameObject*> const& baseOrbs,
                   ANode& out) {
        // (2.2) ball / spider hold into a landing like the cube; the ship also taps (straight fly)
        bool const cubeHold = n.modeCode == 0 || n.modeCode == 2 || n.modeCode == 6;
        bool const shipTap = n.modeCode == 1;
        bool const holdMode = n.modeCode == 1 || n.modeCode == 4 || n.modeCode == 5 || cubeHold;
        if (move == 2 && (!(cubeHold || shipTap) || n.held)) return false;
        bool const toggle = move >= 1;
        bool const isTap = (!holdMode && move == 1) || move == 2;
        int const k = astarStep(n.modeCode, a.s);
        bool const held = isTap ? false : (holdMode ? (move == 1 ? !n.held : n.held) : false);
        bool const g = m_gameHolding;
        float const ph = m_phase;
        bool const rec = m_recordPath;
        m_gameHolding = holdMode ? n.held : false;
        m_phase = 0.f;
        m_recordPath = false;
        m_hasDeadline = false;
        m_realUsedOrbs = baseOrbs;
        for (auto* o : n.orbs) m_realUsedOrbs.push_back(o);
        m_simSnap = n.useSnap ? &n.snap : nullptr;
        m_simSnap2 = n.dual ? &n.snap2 : nullptr;
        m_scanSim = true;
        int const delay = toggle ? 0 : k;
        FollowUp tap;
        if (isTap) { tap.at[0] = 1; tap.held[0] = false; }
        bool const rtWas = m_rtSim;
        m_rtSim = m_astarRoute; // (2.3) autoplay's A*: the route's rules
        SimResult const r = simulate(pl, pl->m_player1, nullptr, false, isTap ? true : held, delay, k - delay, true,
                                     a.dt, a.s, tap);
        m_rtSim = rtWas;
        m_scanSim = false;
        m_simSnap = nullptr;
        m_simSnap2 = nullptr;
        m_gameHolding = g;
        m_phase = ph;
        m_recordPath = rec;
        m_realUsedOrbs = baseOrbs;
        if (r.end != SimEnd::Survived) {
            if (r.end == SimEnd::Died && scanning()) {
                m_scanLastWhat = deathWhat(r) + fmt::format(" - at ({:.0f},{:.0f})", m_simKillPos.x, m_simKillPos.y);
                if (m_scanDeathPts.size() < 300) m_scanDeathPts.push_back(m_simKillPos);
            }
            return false;
        }
        PlayerObject* s1 = m_simPlayer1.data();
        if (!s1) return false;
        out = ANode{};
        out.snap = snapFrom(s1);
        out.useSnap = true;
        out.held = held;
        out.toggled = toggle;
        out.inputPush = isTap ? true : held;
        out.modeCode = modeCode(s1);
        out.modeIdx = scanModeIndex(s1);
        out.x = s1->getPositionX();
        out.y = s1->getPositionY();
        out.vy = s1->m_yVelocity;
        out.tick = n.tick + k;
        out.orbs = n.orbs;
        if (m_simOrbObj) out.orbs.push_back(m_simOrbObj);
        out.mini = s1->m_vehicleSize > 0.f && s1->m_vehicleSize < 0.99f;
        out.dual = m_simP2Live;
        if (out.dual && m_simPlayer2.data()) {
            out.snap2 = snapFrom(m_simPlayer2.data());
            out.y2 = m_simPlayer2->getPositionY();
        }
        out.tapRel = isTap && shipTap;
        float const clr = m_scanClr;
        float const close = clr < SCAN_SAFE_CLR ? (SCAN_SAFE_CLR - std::max(0.f, clr)) / SCAN_SAFE_CLR : 0.f;
        out.g = n.g + static_cast<float>(k) + (toggle ? ASTAR_INPUT_COST : 0.f) + ASTAR_CLOSE_COST * close;
        // ship: an angle costs a little - the level way through (straight fly) first
        if (out.modeCode == 1) out.g += 0.25f * static_cast<float>(k) * std::min(1.f, static_cast<float>(std::fabs(out.vy)) / SHIP_TILT_VY);
        return true;
    }
    // Expand states until `budgetUs` is used, the goal is reached, nothing is
    // left, or `maxExp` states were expanded. True = finished (goal / no way / limit).
    bool astarRun(PlayLayer* pl, AStarRun& a, int budgetUs, int maxExp) {
        auto const t0 = Clock::now();
        auto const baseOrbs = m_realUsedOrbs;
        while (a.goal < 0 && !a.open.empty() && a.expansions < maxExp) {
            if (std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count() >= budgetUs) {
                m_realUsedOrbs = baseOrbs;
                return false;
            }
            int const idx = std::get<2>(a.open.top());
            a.open.pop();
            a.expansions++;
            for (int mv = 0; mv < 3 && a.goal < 0; mv++) {
                ANode child;
                if (!astarMove(pl, a, a.nodes[static_cast<size_t>(idx)], mv, baseOrbs, child)) continue;
                child.parent = idx;
                // speed for the estimate: the fastest move seen (px per tick)
                ANode const& par = a.nodes[static_cast<size_t>(idx)];
                int const kk = std::max(1, child.tick - par.tick);
                a.speed = std::max(a.speed, std::fabs(child.x - par.x) / static_cast<float>(kk));
                astarPush(a, std::move(child));
            }
        }
        m_realUsedOrbs = baseOrbs;
        return true;
    }
    // The states from a start to `end`, oldest first.
    static std::vector<int> astarChain(AStarRun const& a, int end) {
        std::vector<int> out;
        for (int i = end; i >= 0; i = a.nodes[static_cast<size_t>(i)].parent) out.push_back(i);
        std::reverse(out.begin(), out.end());
        return out;
    }

    // Scan (2.2): every path dies at `stuckX` - A* from the versions saved a few
    // blocks back, to a bit past the spot, one tick at a time in wave. While
    // paused, so it may take a moment (at most SCAN_ASTAR_MS).
    float m_astarTriedAt = -1e9f;
    int   m_scanAStar = 0;
    bool scanAStar(PlayLayer* pl, float stuckX) {
        if (!pl || m_rewind.empty() || std::fabs(stuckX - m_astarTriedAt) <= 60.f) return false;
        m_astarTriedAt = stuckX;
        size_t pick = 0;
        for (size_t i = 0; i < m_rewind.size(); i++) if (m_rewind[i].first <= stuckX - SCAN_REWIND_PX) pick = i;
        std::vector<BeamNode> const from = m_rewind[pick].second;
        if (from.empty()) return false;
        AStarRun a;
        a.dt = m_scanDt;
        a.s = m_scanS;
        a.dir = m_p1Dir < 0.f ? -1.f : 1.f;
        a.goalX = stuckX + a.dir * SCAN_WIDE_PAST;
        a.speed = std::max(0.05f, m_p1Speed * m_scanDt);
        for (BeamNode const& b : from) {
            ANode n;
            n.snap = b.snap;
            n.useSnap = !b.fromReal;
            n.held = b.held;
            n.modeCode = b.modeCode;
            n.modeIdx = b.mode;
            n.x = b.x;
            n.y = b.y;
            n.vy = b.vy;
            n.hist = b.hist;
            n.orbs = b.orbs;
            n.mini = b.mini;
            n.dual = b.dual;
            n.snap2 = b.snap2;
            n.y2 = b.y2;
            astarPush(a, std::move(n));
        }
        astarRun(pl, a, SCAN_ASTAR_MS * 1000, SCAN_ASTAR_MAX);
        trace(fmt::format("scan: A* at x={:.0f}: {} states, {}", stuckX, a.expansions,
                          a.goal >= 0 ? "found a way through" : "no way through"));
        if (a.goal < 0) return false;
        // The way through becomes the scan's one version, with its history.
        std::vector<int> const chain = astarChain(a, a.goal);
        int hist = a.nodes[static_cast<size_t>(chain.front())].hist;
        for (size_t i = 1; i < chain.size(); i++) {
            ANode const& par = a.nodes[static_cast<size_t>(chain[i - 1])];
            ANode const& cur = a.nodes[static_cast<size_t>(chain[i])];
            m_beamHist.push_back(BeamHist{hist, par.x, par.y, cur.toggled, cur.inputPush, static_cast<uint8_t>(par.modeIdx)});
            hist = static_cast<int>(m_beamHist.size()) - 1;
            if (cur.tapRel) { // (2.2) a ship tap's release
                m_beamHist.push_back(BeamHist{hist, (par.x + cur.x) * 0.5f, (par.y + cur.y) * 0.5f, true, false,
                                              static_cast<uint8_t>(par.modeIdx)});
                hist = static_cast<int>(m_beamHist.size()) - 1;
            }
        }
        ANode const& end = a.nodes[static_cast<size_t>(a.goal)];
        BeamNode b;
        b.snap = end.snap;
        b.fromReal = false;
        b.held = end.held;
        b.x = end.x;
        b.y = end.y;
        b.vy = end.vy;
        b.mode = end.modeIdx;
        b.modeCode = end.modeCode;
        b.orbs = end.orbs;
        b.hist = hist;
        b.mini = end.mini;
        b.dual = end.dual;
        b.snap2 = end.snap2;
        b.y2 = end.y2;
        m_beam.clear();
        m_beam.push_back(std::move(b));
        m_rewind.erase(m_rewind.begin() + static_cast<std::ptrdiff_t>(pick) + 1, m_rewind.end());
        m_rewindLastX = m_rewind.empty() ? -1e9f : m_rewind.back().first;
        m_scanX = std::max(m_scanX, end.x);
        m_scanAStar++;
        return true;
    }

    // ------------------------------------------------------------------------
    //  Show Rays (2.2, Tools): what the buffer sees and thinks, drawn in the
    //  level as lines while you play.
    //   * white outline: your hitbox;
    //   * cyan line: where you go if you press now (until your usual release);
    //     green line: if you let go / don't press. A red X where one dies.
    //   * yellow line: the pathfinder - a live A* search ~1 s ahead (a press /
    //     release choice every tick in wave, every 2 in ship, 4 elsewhere) for
    //     inputs that get through; cyan / pink ticks on it = where to press /
    //     let go. Orange = no way through found, shows how far it got.
    //   * after a click: what the buffer tried - the click as made (red if it
    //     dies), the delays it tested and rejected (faint red), the one it
    //     picked (bright) - and a line of text saying why.
    // ------------------------------------------------------------------------
    struct PlanIn { float x = 0.f, y = 0.f; bool push = true; };
    AStarRun m_planA;                           // the pathfinder's search in progress (A*)
    bool  m_planActive = false;
    std::vector<acecore::Point2> m_aheadPath;     // last finished route
    std::vector<PlanIn> m_aheadInputs;
    bool  m_aheadDies = false;                   // every version died before the end
    Ref<CCLabelBMFont> m_rayLabel;

    struct Think {
        bool valid = false;
        double at = 0.0;
        std::string label;
        std::vector<acecore::Point2> onTime;
        bool onTimeDies = false;
        std::vector<std::vector<acecore::Point2>> rejected;
        std::vector<acecore::Point2> chosen;
    };
    Think m_think;
    bool  m_thinkRec = false;                   // recording the current click's tests
    std::vector<std::pair<int, std::vector<acecore::Point2>>> m_thinkTests; // (delay, path), verdict-ok paths flagged by sign

    bool ensureOverlay(PlayLayer* pl) {
        if (!pl || !pl->m_objectLayer) return false;
        if (!m_overlay.data()) {
            m_overlay = CCDrawNode::create();
            if (!m_overlay.data()) return false;
        }
        if (!m_overlay->getParent()) pl->m_objectLayer->addChild(m_overlay.data(), 9999);
        return true;
    }
    void clearRays() {
        if (m_overlayShown && m_overlay.data()) m_overlay->clear();
        if (m_rayLabel.data()) m_rayLabel->setVisible(false);
        m_overlayShown = false;
    }
    // ---- Hitboxes (2.3, Tools): your box (spikes / saws kill on it) and your
    // inner box (blocks: hit one with it and you die), like GD's debug view.
    Ref<CCDrawNode> m_assist;
    void releaseAssist() {
        if (m_assist.data() && m_assist->getParent()) m_assist->removeFromParent();
        m_assist.swap(nullptr);
    }
    void assistTick(PlayLayer* pl, PlayerObject* p) {
        Config const& c = cfg();
        bool const want = c.hitboxes && pl && pl->m_objectLayer && p && !p->m_isDead && !m_simulating;
        if (!want) {
            if (m_assist.data()) m_assist->clear();
            return;
        }
        if (!m_assist.data()) {
            m_assist = CCDrawNode::create();
            if (!m_assist.data()) return;
        }
        if (m_assist->getParent() != pl->m_objectLayer) {
            if (m_assist->getParent()) m_assist->removeFromParent();
            pl->m_objectLayer->addChild(m_assist.data(), 9998);
        }
        m_assist->clear();
        auto box = [&](CCRect const& r, ccColor4F col, float w) {
            CCPoint const a{r.getMinX(), r.getMinY()}, b{r.getMaxX(), r.getMinY()}, d{r.getMaxX(), r.getMaxY()}, e{r.getMinX(), r.getMaxY()};
            m_assist->drawSegment(a, b, w, col);
            m_assist->drawSegment(b, d, w, col);
            m_assist->drawSegment(d, e, w, col);
            m_assist->drawSegment(e, a, w, col);
        };
        {
            for (PlayerObject* q : {pl->m_player1, pl->m_gameState.m_isDualMode ? pl->m_player2 : nullptr}) {
                if (!q || q->m_isDead) continue;
                box(q->getObjectRect(), ccColor4F{1.f, 0.3f, 0.3f, 0.95f}, 0.45f);            // outer: spikes, saws, orbs
                box(q->getObjectRect(0.3f, 0.3f), ccColor4F{0.3f, 0.65f, 1.f, 0.95f}, 0.45f); // inner: blocks
            }
        }
    }
    void releaseOverlay() {
        releaseAssist();
        if (m_overlay.data() && m_overlay->getParent()) m_overlay->removeFromParent();
        m_overlay.swap(nullptr);
        if (m_rayLabel.data() && m_rayLabel->getParent()) m_rayLabel->removeFromParent();
        m_rayLabel.swap(nullptr);
        m_overlayShown = false;
        m_planActive = false;
        m_planA = AStarRun{};
        m_aheadPath.clear();
        m_aheadInputs.clear();
        m_think = Think{};
    }

    void rayLine(std::vector<acecore::Point2> const& path, CCPoint from, float width, ccColor4F col, size_t maxPts = 400) {
        if (path.empty()) return;
        size_t const step = path.size() > maxPts ? (path.size() + maxPts - 1) / maxPts : 1;
        CCPoint prev = from;
        for (size_t i = 0; i < path.size(); i += step) {
            CCPoint const cur{path[i].x, path[i].y};
            m_overlay->drawSegment(prev, cur, width, col);
            prev = cur;
        }
        CCPoint const last{path.back().x, path.back().y};
        if (prev.x != last.x || prev.y != last.y) m_overlay->drawSegment(prev, last, width, col);
    }
    void rayCross(CCPoint at, float r, ccColor4F col) {
        m_overlay->drawSegment({at.x - r, at.y - r}, {at.x + r, at.y + r}, 0.8f, col);
        m_overlay->drawSegment({at.x - r, at.y + r}, {at.x + r, at.y - r}, 0.8f, col);
    }

    // ---- the pathfinder (A*, see AStarRun) ------------------------------------------
    static constexpr int PLAN_HORIZON_BASE = 360; // (2.3) ~1.5 s ahead at 240 TPS (was 1 s)
    static constexpr int PLAN_MAX_STATES = 4000;  // then it shows the furthest it got and starts over
    static constexpr int GHOST_BASE = 360;        // (2.3) Show Rays' two ghosts: 1.5 s ahead
    struct Ghost { std::vector<acecore::Point2> path; bool dies = false, show = false; };
    std::array<Ghost, 2> m_ghosts{};
    double m_ghostAt = -1.0;
    void planReset(PlayLayer* pl) {
        PlayerObject* p = pl->m_player1;
        m_planA = AStarRun{};
        m_planA.dt = getValidDt();
        m_planA.s = BASE_DT / m_planA.dt;
        m_planA.dir = m_p1Dir < 0.f ? -1.f : 1.f;
        m_planA.speed = std::max(0.05f, m_p1Speed * m_planA.dt);
        m_planA.goalX = p->getPositionX() + m_planA.dir * m_planA.speed * static_cast<float>(scaleTicks(PLAN_HORIZON_BASE, m_planA.s));
        ANode root;
        root.snap = snapFrom(p);
        root.useSnap = true;
        root.held = m_gameHolding;
        root.modeCode = modeCode(p);
        root.x = p->getPositionX();
        root.y = p->getPositionY();
        root.vy = p->m_yVelocity;
        astarPush(m_planA, std::move(root));
        m_planActive = true;
    }
    void planPublish() {
        bool const found = m_planA.goal >= 0;
        int const end = found ? m_planA.goal : m_planA.deepest;
        m_planActive = false;
        if (end < 0) return;
        m_aheadPath.clear();
        m_aheadInputs.clear();
        std::vector<int> const chain = astarChain(m_planA, end);
        for (size_t i = 0; i < chain.size(); i++) {
            ANode const& n = m_planA.nodes[static_cast<size_t>(chain[i])];
            m_aheadPath.push_back(acecore::Point2{n.x, n.y, 0.f});
            if (i > 0 && n.toggled) {
                ANode const& par = m_planA.nodes[static_cast<size_t>(chain[i - 1])];
                m_aheadInputs.push_back(PlanIn{par.x, par.y, n.inputPush});
            }
        }
        m_aheadDies = !found;
        m_planA = AStarRun{}; // free it
    }
    // One slice of the search (about `budgetUs` of CPU), between physics steps.
    void planSlice(PlayLayer* pl, int budgetUs) {
        PlayerObject* p = pl ? pl->m_player1 : nullptr;
        if (!p || p->m_isDead || p->m_isPlatformer || pl->m_gameState.m_isDualMode || m_simulating) {
            m_planActive = false;
            m_planA = AStarRun{};
            return;
        }
        if (!m_planActive) planReset(pl);
        if (astarRun(pl, m_planA, budgetUs, PLAN_MAX_STATES)) planPublish();
    }

    // ---- what the buffer thought about the last click ------------------------------
    void thinkCapture(bool push, Decision dec, SimResult const& r0, std::vector<acecore::Point2> const& path0,
                      int bestDelay, int windowTicks, std::string const& modeName) {
        m_thinkRec = false;
        Think t;
        t.valid = true;
        t.at = m_gameTime;
        t.onTime = path0;
        t.onTimeDies = r0.end == SimEnd::Died;
        char const* act = push ? "PRESS" : "RELEASE";
        std::string const why = r0.end == SimEnd::Died ? fmt::format("on time dies in {}t ({})", r0.ticks, deathWhat(r0))
                                                        : std::string("on time is safe");
        switch (dec) {
            case Decision::Buffered:
                t.label = fmt::format("{} held +{}t: {} - fires on the first safe tick [{}]", act, bestDelay, why, modeName);
                break;
            case Decision::PassSafe:
                t.label = fmt::format("{}: {} - went in as made [{}]", act, why, modeName);
                break;
            case Decision::PassNoFix:
                t.label = fmt::format("{}: {} - nothing in your {}t window saves it, went in as made [{}]", act, why,
                                      windowTicks, modeName);
                break;
            case Decision::PassEscapable:
                t.label = fmt::format("{}: {} - you can still save it yourself, went in as made [{}]", act, why, modeName);
                break;
            case Decision::PassTruncated:
                t.label = fmt::format("{}: can't see past {} - went in as made [{}]", act, describeTrunc(r0.trunc), modeName);
                break;
            default:
                t.label = fmt::format("{}: went in as made [{}]", act, modeName);
                break;
        }
        for (auto& [d, path] : m_thinkTests) {
            if (d == bestDelay && dec == Decision::Buffered) t.chosen = path;
            else if (t.rejected.size() < 10) t.rejected.push_back(path);
        }
        m_thinkTests.clear();
        m_think = std::move(t);
    }

    // ---- drawing (a few times a second) ------------------------------------------------
    void updateRays(PlayLayer* pl) {
        Config const& c = cfg();
        if (!c.rayView || !m_active || !pl || !pl->m_player1) { clearRays(); return; }
        if (!ensureOverlay(pl)) return;
        PlayerObject* p1 = pl->m_player1;
        CCPoint const me = p1->getPosition();
        m_overlay->clear();

        // your hitbox
        {
            CCRect const& r = p1->getObjectRect();
            CCPoint const a{r.getMinX(), r.getMinY()}, b{r.getMaxX(), r.getMinY()}, cc{r.getMaxX(), r.getMaxY()},
                d{r.getMinX(), r.getMaxY()};
            ccColor4F const w{1.f, 1.f, 1.f, 0.9f};
            m_overlay->drawSegment(a, b, 0.5f, w);
            m_overlay->drawSegment(b, cc, 0.5f, w);
            m_overlay->drawSegment(cc, d, 0.5f, w);
            m_overlay->drawSegment(d, a, 0.5f, w);
        }

        // the pathfinder's route (2.3: not while autoplay plays - its own route is drawn)
        if (m_aheadPath.size() > 1 && !m_autoOn) {
            // (only what's still ahead of you - the search started a moment ago)
            float const dir = m_p1Dir < 0.f ? -1.f : 1.f;
            size_t first = 0;
            while (first < m_aheadPath.size() && (m_aheadPath[first].x - me.x) * dir < -2.f) first++;
            std::vector<acecore::Point2> const ahead(m_aheadPath.begin() + static_cast<std::ptrdiff_t>(first), m_aheadPath.end());
            ccColor4F const yel = m_aheadDies ? ccColor4F{1.f, 0.6f, 0.2f, 0.8f} : ccColor4F{1.f, 0.9f, 0.25f, 0.85f};
            if (ahead.size() > 1) rayLine(ahead, CCPoint{ahead[0].x, ahead[0].y}, 0.7f, yel);
            for (auto const& in : m_aheadInputs) {
                if ((in.x - me.x) * dir < -2.f) continue;
                ccColor4F const col = in.push ? ccColor4F{0.3f, 0.9f, 1.f, 1.f} : ccColor4F{1.f, 0.45f, 0.75f, 1.f};
                m_overlay->drawSegment({in.x, in.y - 7.f}, {in.x, in.y + 7.f}, 0.8f, col);
            }
        }

        // (2.3) the two ghosts: "press now" / "let go now" (or "keep not
        // pressing") as hidden players, 1.5 s ahead - well past the screen -
        // redone ~15x a second. Where a ghost would die its last stretch turns
        // red with an X. (They were as long as one click's lookahead: a
        // quarter to two thirds of a second.)
        if (m_autoOn) {
            for (auto& g : m_ghosts) g.show = false; // autoplay: its route instead
        } else if ((m_tick & 15) == 0 || m_ghostAt < 0.0 || m_gameTime - m_ghostAt > 30.0) {
            m_ghostAt = m_gameTime;
            int const budget = std::max(clickBudgetUs(c) / 2, 1500);
            for (int k = 0; k < 2; k++) {
                Ghost& g = m_ghosts[static_cast<size_t>(k)];
                g.path.clear();
                g.dies = false;
                g.show = false;
                bool const press = k == 0;
                Ctx x;
                if (press && m_physicalHolding) continue; // already holding: no new press to show
                bool const neverPress = !press && !m_physicalHolding; // not holding: "don't press" = the press, never made
                if (!makeCtx(pl, neverPress ? true : press, c, x)) continue;
                int const ghostTicks = std::clamp(scaleTicks(GHOST_BASE, x.s), MIN_HORIZON, MAX_SIM_STEPS);
                x.horizon = ghostTicks;
                m_hasDeadline = budget > 0;
                if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budget);
                m_recordPath = true;
                SimResult const r = neverPress ? runSim(x, ghostTicks, FollowUp{}, 1) : runSim(x, 0); // (never: a press after the end)
                m_recordPath = false;
                m_hasDeadline = false;
                g.path = m_path; // out of time: as far as it got
                g.dies = r.end == SimEnd::Died;
                g.show = g.path.size() > 1;
            }
        }
        for (int k = 0; k < 2; k++) {
            Ghost const& g = m_ghosts[static_cast<size_t>(k)];
            if (!g.show) continue;
            ccColor4F const col = k == 0 ? ccColor4F{0.3f, 0.9f, 1.f, 0.9f} : ccColor4F{0.3f, 1.f, 0.45f, 0.9f};
            // only what's still ahead of you
            float const dir = m_p1Dir < 0.f ? -1.f : 1.f;
            size_t first = 0;
            while (first + 1 < g.path.size() && (g.path[first].x - me.x) * dir < -2.f) first++;
            std::vector<acecore::Point2> ahead(g.path.begin() + static_cast<std::ptrdiff_t>(first), g.path.end());
            if (ahead.size() < 2) continue;
            if (!g.dies) {
                rayLine(ahead, me, 0.55f, col, 800);
                continue;
            }
            size_t const red = std::min<size_t>(ahead.size() - 1, std::max<size_t>(8, ahead.size() / 4));
            std::vector<acecore::Point2> const safe(ahead.begin(), ahead.end() - static_cast<std::ptrdiff_t>(red));
            std::vector<acecore::Point2> const doom(ahead.end() - static_cast<std::ptrdiff_t>(red) - 1, ahead.end());
            if (safe.size() > 1) rayLine(safe, me, 0.55f, col, 800);
            rayLine(doom, CCPoint{doom[0].x, doom[0].y}, 0.7f, ccColor4F{1.f, 0.2f, 0.2f, 0.95f}, 800);
            rayCross({ahead.back().x, ahead.back().y}, 4.f, {1.f, 0.15f, 0.15f, 1.f});
        }

        // (2.3) autoplay: where it died before (red X, bigger the more times)
        // and where moving blocks / spikes will be when you get to them
        // (orange outlines)
        if (m_autoOn) {
            for (auto const& d : m_autoDeaths) {
                if (std::fabs(d.x - me.x) > 900.f) continue;
                rayCross({d.x, d.y}, 5.f + 2.f * static_cast<float>(std::min(d.times, 5)), ccColor4F{1.f, 0.15f, 0.15f, 0.9f});
            }
            int drawn = 0;
            float const dir = m_mvDir;
            for (int i : m_mvNear) {
                if (drawn >= 60) break;
                Mover const& m = m_mv[static_cast<size_t>(i)];
                if (m.live.off && m.tl.empty()) continue;
                // where it is when you get there: your x = its x then (twice: it moves meanwhile)
                float key = static_cast<float>(m.live.x);
                MvState st = mvAt(m, key);
                key = static_cast<float>(st.x);
                st = mvAt(m, key);
                if ((st.x - me.x) * dir < -40.f || (st.x - me.x) * dir > 700.f) continue;
                if (st.off) continue;
                bool const same = std::fabs(st.x - m.live.x) < 1.0 && std::fabs(st.y - m.live.y) < 1.0 && m.live.off == st.off;
                if (same) continue; // it'll be where it is: nothing to show
                CCRect const& r = m.obj->getObjectRect(); // its box now, moved to where it will be
                float const ox = static_cast<float>(st.x - m.live.x), oy = static_cast<float>(st.y - m.live.y);
                CCPoint const a{r.getMinX() + ox, r.getMinY() + oy}, b{r.getMaxX() + ox, r.getMinY() + oy},
                    c2{r.getMaxX() + ox, r.getMaxY() + oy}, d2{r.getMinX() + ox, r.getMaxY() + oy};
                ccColor4F const col{1.f, 0.6f, 0.15f, 0.75f};
                m_overlay->drawSegment(a, b, 0.5f, col);
                m_overlay->drawSegment(b, c2, 0.5f, col);
                m_overlay->drawSegment(c2, d2, 0.5f, col);
                m_overlay->drawSegment(d2, a, 0.5f, col);
                drawn++;
            }
        }
        // (2.3) your kept clicks (Your Clicks): bright cyan lines where
        // autoplay makes them again - taller than the route's own input ticks
        if (m_autoOn) {
            for (auto const& c : m_clicks) {
                if (std::fabs(c.x - me.x) > 900.f) continue;
                float const y = c.y != 0.f ? c.y : me.y;
                m_overlay->drawSegment({c.x, y - 16.f}, {c.x, y + 16.f}, 1.4f, ccColor4F{0.2f, 1.f, 1.f, 1.f});
            }
        }
        // (2.3) autoplay's route ahead of you (violet), inputs as ticks
        if (m_autoOn && m_rtOn && m_rt.size() > 1) {
            std::vector<acecore::Point2> route;
            int const from = std::max(m_rtBase, m_rtNow);
            for (int t = from; t <= rtEnd(); t += 2) route.push_back(acecore::Point2{rtAt(t).x, rtAt(t).y, 0.f});
            if (route.size() > 1) rayLine(route, CCPoint{route[0].x, route[0].y}, 0.8f, ccColor4F{0.75f, 0.45f, 1.f, 0.9f}, 900);
            for (auto const& [t, push] : m_rtEvents) {
                if (t < from || t > rtEnd()) continue;
                SpTick const& q = rtAt(t);
                ccColor4F const col = push ? ccColor4F{0.3f, 0.9f, 1.f, 1.f} : ccColor4F{1.f, 0.45f, 0.75f, 1.f};
                m_overlay->drawSegment({q.x, q.y - 6.f}, {q.x, q.y + 6.f}, 0.8f, col);
            }
        }

        // what the buffer thought about your last click (1.5 s)
        bool const fresh = m_think.valid && (m_gameTime - m_think.at) < 90.0;
        if (fresh) {
            for (auto const& path : m_think.rejected) rayLine(path, me, 0.35f, ccColor4F{1.f, 0.3f, 0.3f, 0.35f}, 120);
            if (!m_think.onTime.empty())
                rayLine(m_think.onTime, CCPoint{m_think.onTime[0].x, m_think.onTime[0].y}, 0.5f,
                        m_think.onTimeDies ? ccColor4F{1.f, 0.2f, 0.2f, 0.85f} : ccColor4F{0.3f, 1.f, 0.45f, 0.85f});
            if (!m_think.chosen.empty())
                rayLine(m_think.chosen, CCPoint{m_think.chosen[0].x, m_think.chosen[0].y}, 0.9f, ccColor4F{1.f, 1.f, 1.f, 0.95f});
        }
        if (!m_rayLabel.data()) {
            m_rayLabel = CCLabelBMFont::create("", "chatFont.fnt");
            if (m_rayLabel.data()) m_rayLabel->setAnchorPoint({0.f, 0.f});
        }
        if (m_rayLabel.data()) {
            if (m_rayLabel->getParent() != pl->m_objectLayer) {
                if (m_rayLabel->getParent()) m_rayLabel->removeFromParent();
                pl->m_objectLayer->addChild(m_rayLabel.data(), 10000);
            }
            // (2.3) autoplay: what it's doing (the pathfinder's label was left
            // over from Show Rays and said "looking ahead..." the whole time)
            std::string const text = m_autoOn ? autoStatusText(pl)
                                   : fresh ? m_think.label
                                           : (m_aheadPath.size() > 1 ? (m_aheadDies ? "pathfinder: every path dies ahead"
                                                                                  : "pathfinder: way through found (yellow)")
                                                                    : std::string("looking ahead..."));
            m_rayLabel->setString(text.c_str());
            m_rayLabel->setScale(0.45f);
            m_rayLabel->setColor(m_autoOn ? (learning() ? ccColor3B{255, 170, 80} : m_rtStuck ? ccColor3B{255, 110, 110}
                                                                                       : ccColor3B{190, 150, 255})
                                 : fresh ? ccColor3B{255, 255, 255} : ccColor3B{255, 230, 120});
            m_rayLabel->setPosition({me.x - 20.f, me.y + 26.f});
            m_rayLabel->setVisible(true);
        }
        m_overlay->setVisible(true);
        m_overlayShown = true;
    }

public:
    // Leaving the level: drop the scan.
    void releaseScan() {
        if (scanning()) m_realUsedOrbs = m_scanSavedOrbs;
        m_beam.clear();
        m_beamHist.clear();
        if (m_scanState == ScanState::Running) m_scanState = ScanState::Off;
        m_scanMsg.clear();
    }

    static float pctAt(PlayLayer* pl, float x) {
        float const len = pl ? pl->m_levelLength : 0.f;
        return len > 1.f ? std::clamp(100.f * x / len, 0.f, 100.f) : 0.f;
    }

    // Snap (2.1): your input matches a scanned one (same direction and
    // gamemode, same height on the path). 1 = inside its window (fine as it
    // is), 2 = early by no more than your stage window (hold until targetX),
    // 0 = not on the scanned route.
    // The scanned line's height at x (false: not scanned there / a gap).

    // For the window (Tools tab).
    SavedStats levelStats() const { return m_lvlStats; }
    SavedStats lifeStats() { loadLifeStats(); return m_lifeStats; }
    std::string levelName() const { return m_lvlName; }

    // Everything you'd otherwise screenshot, in one text file (and returned).
    std::string buildReport() {
        Config const& c = cfg();
        std::string out;
        auto line = [&](std::string const& t) { out += t; out += '\n'; };
        line("Aceware report");
        line(fmt::format("mod {}  geode {}  gd {}", Mod::get()->getVersion().toVString(),
                         Loader::get()->getVersion().toVString(), Loader::get()->getGameVersion()));
        line(fmt::format("system: {}  device: {}", systemInfo().full, systemInfo().device));
        line(fmt::format("level: {} ({})  in level: {}", m_lvlName, m_lvlKey, PlayLayer::get() ? "yes" : "no"));
        line(fmt::format("stage: {}  orb helper: {}  show rays: {}  learn: {}  detail log: {}",
                         STAGES[std::clamp(c.stage, 0, STAGE_COUNT - 1)].name, c.orbHelper, c.rayView,
                         c.learnSpots, c.hudVerbose));
        line("");
        line("== Stats ==");
        m_forceDetails = true;
        auto rows = statsRows();
        m_forceDetails = false;
        for (auto const& r : rows) {
            std::string t = r.first;
            if (!t.empty() && t[0] == '\x01') { line(""); line("-- " + t.substr(1) + " --"); continue; }
            for (auto& ch : t) if (ch == '\t') ch = ':';
            line(t);
        }
        line("");
        // (2.3) the Dev tab too: prediction checks, input timing, portals, autoplay
        line("== Dev ==");
        for (auto const& r : devRows()) {
            std::string t = r.first;
            if (!t.empty() && t[0] == '\x01') { line(""); line("-- " + t.substr(1) + " --"); continue; }
            for (auto& ch : t) if (ch == '\t') ch = ':';
            line(t);
        }
        line("");
        line("== Log (newest first) ==");
        for (auto const& r : historyRows(true)) {
            std::string t = r.first;
            if (!t.empty() && t[0] == '\x02') t = t.substr(1);
            for (auto& ch : t) if (ch == '\x1f') ch = ' ';
            line(t);
        }
        line("");
        line(fmt::format("== Scanned route ({} inputs, scanned: {}) ==", m_route.size(), [&] {
            std::string t;
            for (auto const& r : m_scanned) t += fmt::format("{}{:.0f}..{:.0f}", t.empty() ? "" : ", ", r.first, r.second);
            return t.empty() ? std::string("nothing") : t;
        }()));
        for (size_t i = 0; i < m_route.size() && i < 400; i++) {
            auto const& r = m_route[i];
            line(fmt::format("  {:.1f} {:.1f} {} {}", r.x, r.y, r.push ? "press" : "release", r.mode));
        }
        if (m_route.size() > 400) line(fmt::format("  ... {} more", m_route.size() - 400));
        line("");
        line("== Trace (every click and log line, oldest first) ==");
        for (auto const& t : m_trace) line(t);
        return out;
    }
    // Export Scan (2.2, Tools): this level's scan as a text file - where to
    // press / release (in % and position), what it couldn't solve and why.
    std::string buildScanExport() {
        PlayLayer* pl = PlayLayer::get();
        std::string out;
        auto line = [&](std::string const& t) { out += t; out += '\n'; };
        line(fmt::format("Aceware - level export  ({} {})", m_lvlName, m_lvlKey));
        line(fmt::format("mod {}  gd {}", Mod::get()->getVersion().toVString(), Loader::get()->getGameVersion()));
        line(fmt::format("status: {}", scanStatus()));
        std::string ranges;
        for (auto const& r : m_scanned)
            ranges += fmt::format("{}{:.0f}%-{:.0f}%", ranges.empty() ? "" : ", ", pctAt(pl, r.first), pctAt(pl, r.second));
        line(fmt::format("scanned: {}", ranges.empty() ? std::string("nothing") : ranges));
        line(fmt::format("last scan: tried harder {}x, used your run {}x, went through {} unsolved spot(s)", m_scanBackups,
                         m_scanJumps, m_scanSkips));
        line("");
        line("== Spots it couldn't solve (it went through them ignoring deaths) ==");
        if (m_scanSkipRanges.empty()) line("  none");
        for (auto const& r : m_scanSkipRanges)
            line(fmt::format("  {:.1f}% - {:.1f}%  (x {:.0f} - {:.0f})", pctAt(pl, r.first), pctAt(pl, r.second), r.first, r.second));
        if (!m_scanLastWhat.empty()) line(fmt::format("  last thing that killed every path: {}", m_scanLastWhat));
        line("");
        line(fmt::format("level height range used: {:.0f} .. {:.0f}  (objects GD skipped, added: {}; held inside a portal's floor / ceiling: {}x)",
                         m_levelBottom, m_levelTop, m_simExtraObjs, m_bandClamps));
        line("");
        line("== The scan's path (every ~1%) ==");
        line("      %         x        y");
        {
            float lastPct = -10.f;
            for (auto const& q : m_routePath) {
                float const pc = pctAt(pl, q.x);
                if (pc < lastPct + 1.f) continue;
                lastPct = pc;
                line(fmt::format("  {:6.2f}  {:8.1f} {:8.1f}", pc, q.x, q.y));
            }
            if (m_routePath.empty()) line("  (none saved)");
        }
        line("");
        line("== What you stood on / your gamemode bands (from your own play this session) ==");
        {
            int shown = 0;
            for (auto const& t : m_groundLines) {
                line("  " + t);
                if (++shown >= 120) break;
            }
            if (!shown) line("  (play the level a bit first, then export)");
        }
        line("");
        line(fmt::format("== Where to press / release ({} inputs) ==", m_route.size()));
        line("      %         x        y   input    gamemode");
        for (auto const& r : m_route)
            line(fmt::format("  {:6.2f}  {:8.1f} {:8.1f}   {:<7}  {}", pctAt(pl, r.x), r.x, r.y, r.push ? "press" : "release", r.mode));
        return out;
    }
    std::filesystem::path saveScanExport(std::string const& text) {
        std::error_code ec;
        std::filesystem::path const dir = Mod::get()->getSaveDir() / "reports";
        std::filesystem::create_directories(dir, ec);
        long long const secs = std::chrono::duration_cast<std::chrono::seconds>(
                                   std::chrono::system_clock::now().time_since_epoch()).count();
        std::filesystem::path const file = dir / fmt::format("scan-{}-{}.txt", m_lvlKey.empty() ? "menu" : m_lvlKey, secs);
        std::ofstream o(file, std::ios::binary | std::ios::trunc);
        o << text;
        return file;
    }
    bool hasLevel() const { return !m_lvlKey.empty() && PlayLayer::get(); }
    std::filesystem::path saveReport(std::string const& text) {
        std::error_code ec;
        std::filesystem::path const dir = Mod::get()->getSaveDir() / "reports";
        std::filesystem::create_directories(dir, ec);
        auto const now = std::chrono::system_clock::now().time_since_epoch();
        long long const secs = std::chrono::duration_cast<std::chrono::seconds>(now).count();
        std::filesystem::path const file = dir / fmt::format("report-{}-{}.txt", m_lvlKey.empty() ? "menu" : m_lvlKey, secs);
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << text;
        return file;
    }
private:

    // Small tag bottom-left while the buffer is active (fair play / recordings).
    void updateBadge(PlayLayer* pl, Config const& c) {
        bool const show = c.showBadge && m_active;
        if (!show) {
            if (m_badge.data()) m_badge->setVisible(false);
            return;
        }
        if (!m_badge.data() || !m_badge->getParent()) {
            auto* l = CCLabelBMFont::create("", "chatFont.fnt");
            if (!l) return;
            l->setAnchorPoint({0.f, 0.f});
            l->setScale(0.45f);
            l->setOpacity(150);
            l->setPosition({4.f, 3.f});
            pl->addChild(l, 2000);
            m_badge = l;
            m_badgeText.clear();
        }
        std::string text = c.custom ? "Aceware: CUSTOM" : c.extreme ? "Aceware: EXTREME" : "Aceware";
        if (c.practiceOnly) text += " (practice)";
        if (text != m_badgeText) {
            m_badge->setString(text.c_str());
            m_badgeText = text;
        }
        m_badge->setVisible(true);
    }

    void releaseBadge() {
        if (m_badge.data() && m_badge->getParent()) m_badge->removeFromParent();
        m_badge.swap(nullptr);
        m_badgeText.clear();
    }

    // ------------------------------------------------------------------------
    //  Sim players (created lazily, P2 only when dual is actually used)
    // ------------------------------------------------------------------------
    PlayerObject* createSimPlayer(PlayLayer* pl) {
        PlayerObject* p = PlayerObject::create(1, 1, pl, pl, true);
        if (!p) return nullptr;
        p->setPosition({0, 105});
        p->setVisible(false);
        pl->m_objectLayer->addChild(p);
        return p;
    }

    void ensureSimPlayers(PlayLayer* pl) {
        if (!m_simPlayer1.data()) m_simPlayer1 = createSimPlayer(pl);
        if (!m_simPlayer2.data() && pl->m_gameState.m_isDualMode) m_simPlayer2 = createSimPlayer(pl);
    }

    void releaseSimPlayers() {
        if (m_simPlayer1.data() && m_simPlayer1->getParent()) m_simPlayer1->removeFromParent();
        if (m_simPlayer2.data() && m_simPlayer2->getParent()) m_simPlayer2->removeFromParent();
        m_simPlayer1.swap(nullptr);
        m_simPlayer2.swap(nullptr);
    }

    static void clearCollisionDicts(PlayerObject* self) {
        self->m_collisionLogTop->removeAllObjects();
        self->m_collisionLogBottom->removeAllObjects();
        self->m_collisionLogLeft->removeAllObjects();
        self->m_collisionLogRight->removeAllObjects();
    }
    // (2.3) GD's collision reset at the END of a step (Click Between Frames
    // does it there too). The drift detail showed what it does that the sim
    // didn't: the object you stood on / hit with your head this step is kept
    // as "last step's" (m_unk50C / m_unk510 - yours had the floor's id, the
    // route's -1) before this step's are cleared. GD uses last step's ground
    // object to keep you on a floor across steps: without it the hidden
    // player left edges a tick early ("m_stateOnGround -2/-3") and fell
    // through thin floors it should have landed on.
    static void gdResetCollisionLog(PlayerObject* self) {
        clearCollisionDicts(self);
        self->m_unk50C = self->m_lastCollisionBottom;
        self->m_unk510 = self->m_lastCollisionTop;
        self->m_lastCollisionLeft = -1;
        self->m_lastCollisionRight = -1;
        self->m_lastCollisionBottom = -1;
        self->m_lastCollisionTop = -1;
    }


    // Same attribute + hitbox synchronisation Show Trajectory uses, plus the
    // movement state and the BUTTON state, so every test starts from exactly
    // what the real player has right now.
    void syncSimPlayer(PlayerObject* sim, PlayerObject* real) {
        sim->setVisible(false);
        sim->copyAttributes(real);
        // Button FIRST (1.23.1). Previous tests left the sim in whatever state
        // they ended in: release so a later press is a clean new press, then
        // hold if the game holds. pushButton is a new press - in UFO a flap -
        // so it must come before speed / position are copied; it used to come
        // after, and every test while you held in UFO started with a flap you
        // never made (one-tick check: UFO only 72% exact, y-speed off by ~1).
        sim->releaseButton(PlayerButton::Jump);
        if (m_gameHolding) sim->pushButton(PlayerButton::Jump);
        sim->setPosition(real->getPosition());
        sim->setRotation(real->getRotation()); // (2.3) GD's checkpoints keep it; the sim kept the last test's
        sim->setScale(real->getScale());
        sim->setContentSize(real->getContentSize());
        sim->m_width = real->m_width;
        sim->m_height = real->m_height;
        sim->m_gravityMod = real->m_gravityMod;
        sim->m_isOnGround = real->m_isOnGround;
        sim->m_isUpsideDown = real->m_isUpsideDown;
        sim->m_vehicleSize = real->m_vehicleSize;
        sim->m_playerSpeed = real->m_playerSpeed;
        sim->m_yVelocity = real->m_yVelocity;
        sim->m_gravity = real->m_gravity;
        sim->m_speedMultiplier = real->m_speedMultiplier;
        // Ground / landing / portal state copyAttributes leaves behind (1.17.1).
        // Without it the sim player kept the last test's values: wave tests
        // "died" a tick after letting go, at a height it can't reach in a tick,
        // so good wave releases were held back (the wave went up).
        sim->m_position = real->m_position;
        sim->m_lastPortalPos = real->m_lastPortalPos;
        sim->m_lastGroundedPos = real->m_lastGroundedPos;
        sim->m_lastActivatedPortal = real->m_lastActivatedPortal;
        sim->m_isOnGround2 = real->m_isOnGround2;
        sim->m_lastLandTime = real->m_lastLandTime;
        sim->m_stateOnGround = real->m_stateOnGround;
        sim->m_isGoingLeft = real->m_isGoingLeft;
        sim->m_isSideways = real->m_isSideways;
        sim->m_isDashing = real->m_isDashing;
        // Orbs the real player already used this contact must not fire again.
        sim->m_touchedRings.clear();
        for (int id : real->m_touchedRings) sim->m_touchedRings.insert(id);
        // GD's own press buffer, as the real player has it (after the button).
        sim->m_jumpBuffered = real->m_jumpBuffered;
        // Everything else pushButton / earlier tests may have changed that the
        // physics reads (1.23.2). The one-tick check showed UFO still only ~67%
        // exact with the y-speed off: pushButton leaves a "press waiting" state
        // (m_stateJumpBuffered etc.) that the real player had already used, so
        // the sim flapped on its first tick. All plain values, no pointers.
        sim->m_stateJumpBuffered = real->m_stateJumpBuffered;
        sim->m_wasJumpBuffered = real->m_wasJumpBuffered;
        sim->m_stateRingJump = real->m_stateRingJump;
        sim->m_stateRingJump2 = real->m_stateRingJump2;
        sim->m_wasRobotJump = real->m_wasRobotJump;
        sim->m_fixRobotJump = real->m_fixRobotJump;
        sim->m_touchedRing = real->m_touchedRing;
        sim->m_touchedCustomRing = real->m_touchedCustomRing;
        sim->m_touchedGravityPortal = real->m_touchedGravityPortal;
        sim->m_touchedPad = real->m_touchedPad;
        sim->m_fallSpeed = real->m_fallSpeed;
        sim->m_isAccelerating = real->m_isAccelerating;
        sim->m_accelerationOrSpeed = real->m_accelerationOrSpeed;
        sim->m_isOnSlope = real->m_isOnSlope;
        sim->m_wasOnSlope = real->m_wasOnSlope;
        sim->m_slopeVelocity = real->m_slopeVelocity;
        sim->m_maybeUpsideDownSlope = real->m_maybeUpsideDownSlope;
        sim->m_slopeRotation = real->m_slopeRotation;
        sim->m_currentSlopeYVelocity = real->m_currentSlopeYVelocity;
        sim->m_isCurrentSlopeTop = real->m_isCurrentSlopeTop;
        sim->m_unk3d0 = real->m_unk3d0;
        sim->m_blackOrbRelated = real->m_blackOrbRelated;
        sim->m_unk3e0 = real->m_unk3e0;
        sim->m_unk3e1 = real->m_unk3e1;
        sim->m_collidedTopMinY = real->m_collidedTopMinY;
        sim->m_collidedBottomMaxY = real->m_collidedBottomMaxY;
        sim->m_collidedLeftMaxX = real->m_collidedLeftMaxX;
        sim->m_collidedRightMinX = real->m_collidedRightMinX;
        sim->m_stateFlipGravity = real->m_stateFlipGravity;
        sim->m_stateForce = real->m_stateForce;
        sim->m_stateForceVector = real->m_stateForceVector;
        sim->m_affectedByForces = real->m_affectedByForces;
        sim->m_lastMovedTime = real->m_lastMovedTime;
        sim->m_playerSpeedAC = real->m_playerSpeedAC;
        sim->m_jumpRelatedAC2 = real->m_jumpRelatedAC2;
        sim->m_lastJumpTime = real->m_lastJumpTime;
        sim->m_lastFlipTime = real->m_lastFlipTime;
        sim->m_lastSpiderFlipTime = real->m_lastSpiderFlipTime;
        sim->m_snapDistance = real->m_snapDistance;
        sim->m_totalTime = real->m_totalTime;
        // (2.3) which object it touched on each side, this step and last step
        // (GD's end-of-step collision reset carries them; see gdResetCollisionLog)
        sim->m_lastCollisionBottom = real->m_lastCollisionBottom;
        sim->m_lastCollisionTop = real->m_lastCollisionTop;
        sim->m_lastCollisionLeft = real->m_lastCollisionLeft;
        sim->m_lastCollisionRight = real->m_lastCollisionRight;
        sim->m_unk50C = real->m_unk50C;
        sim->m_unk510 = real->m_unk510;
        // (2.1 copied ~45 more fields here - jump / slope / boost state and what
        // the player touches. Reverted: mini cube predictions changed with it.
        // The level scan's snapshots still carry them - see PlayerSnap.)
    }
    void setHeld(PlayerObject* p1, PlayerObject* p2, bool held) {
        if (held && !m_simHeld) m_simPressTick = m_simTick; // a new press inside this test
        m_simHeld = held;
        if (held) {
            p1->pushButton(PlayerButton::Jump);
            if (p2) p2->pushButton(PlayerButton::Jump);
        } else {
            p1->releaseButton(PlayerButton::Jump);
            if (p2) p2->releaseButton(PlayerButton::Jump);
        }
    }

    float getValidDt() const {
        if (m_frameDt > 0.001f && m_frameDt < 2.0f) return m_frameDt;
        return BASE_DT;
    }

    bool budgetExpired() const {
        return m_hasDeadline && Clock::now() > m_deadline;
    }
    void clearBuffer() {
        m_queue.clear();
        m_live = false;
        m_planTrust = false;
        m_orbHelpHold = false;
        m_orbHelpRing = nullptr;
    }

    // Still on the planned path? Checked on tick boundaries only (a CBF sub-step
    // lands between two predicted positions).
    bool planDrifted(PlayLayer* pl, double now, float dt) const {
        PlayerObject* p = pl ? pl->m_player1 : nullptr;
        if (!p || dt <= 0.f) return true;
        double const steps = (now - m_planGrid) / static_cast<double>(dt);
        int const j = static_cast<int>(std::lround(steps));
        if (std::fabs(steps - static_cast<double>(j)) > 0.2) return false; // mid-tick: next boundary decides
        int const idx = j - 1; // after j steps (the first is the rest of the click's tick)
        if (idx < 0 || idx >= static_cast<int>(m_planPath.size())) return false;
        float const dx = p->getPositionX() - m_planPath[static_cast<size_t>(idx)].x;
        float const dy = p->getPositionY() - m_planPath[static_cast<size_t>(idx)].y;
        return std::fabs(dx) > PLAN_DRIFT_PX || std::fabs(dy) > PLAN_DRIFT_PX;
    }

    float simDt() const { return getValidDt() * (m_warp > 0.f ? m_warp : 1.f); }

    // Fraction of the current tick already done (0 on a boundary). Sims start
    // with the rest of this tick, and held inputs are scheduled on the real
    // tick grid, so a click made mid-tick (CBF) lines up with what was tested.
    float tickPhase() const { return (m_phase > 0.02f && m_phase < 0.98f) ? m_phase : 0.f; }

    // Per-tick budget for the live re-check of a held click: a fraction of the
    // frame time (it runs every tick), never more than a click's budget.
    int liveBudgetUs(Config const& c) const {
        int const click = clickBudgetUs(c);
        if (!m_haveFrame) return click;
        int const share = std::max(LIVE_BUDGET_MIN_US, static_cast<int>(m_frameUs * LIVE_BUDGET_FRAC));
        return click > 0 ? std::min(click, share) : share;
    }

    // CPU budget for this click in microseconds (0 = unlimited).
    int clickBudgetUs(Config const& c) const {
        int b = c.budgetUs;
        if (c.extreme && b > 0 && !c.autoBudget) b = std::max(b, EXTREME_MIN_BUDGET_US);
        if (c.autoBudget && m_haveFrame) {
            float const frac = c.extreme ? AUTO_BUDGET_FRAC_EXTREME : AUTO_BUDGET_FRAC;
            int a = static_cast<int>(m_frameUs * frac);
            a = std::clamp(a, AUTO_BUDGET_MIN_US, AUTO_BUDGET_MAX_US);
            b = (b > 0) ? std::min(b, a) : a;
        }
        return b;
    }

    static bool holdingJump(PlayerObject* p) {
        if (!p) return false;
        auto it = p->m_holdingButtons.find(static_cast<int>(PlayerButton::Jump));
        return it != p->m_holdingButtons.end() && it->second;
    }

    // ------------------------------------------------------------------------
    //  Input timing (2.3): does an input the mod sends reach the player on the
    //  tick it's sent - the tick every prediction assumes? Right before an
    //  input goes out, one tick is predicted with it and one without; if they
    //  differ clearly in y speed, the next ticks show which one you really did
    //  (0 = on time, 1 = a tick late, ...). Dev tab "Input timing"; Log when
    //  it's late. Autoplay sends its inputs that much earlier.
    // ------------------------------------------------------------------------
    bool   m_lagOn = false;
    long long m_lagTick = 0;
    double m_lagV1 = 0.0, m_lagV0 = 0.0;
    std::vector<int> m_lagSamples;
    int    m_fireLag = 0;
    bool   m_lagLogged = false;
    void lagArm(PlayLayer* pl, bool push) {
        if (m_lagOn || m_simulating || !pl || !pl->m_player1 || pl->m_player1->m_isDead || !m_simPlayer1.data()) return;
        PlayerObject* p = pl->m_player1;
        if (p->m_isPlatformer || tickPhase() > 0.f) return;
        float const sdt = simDt();
        float const sc = BASE_DT / std::max(0.001f, sdt);
        bool const rec = m_recordPath;
        m_recordPath = false;
        m_hasDeadline = false;
        simulate(pl, p, nullptr, false, push, 0, 1, true, sdt, sc); // with it
        double const v1 = m_simPlayer1->m_yVelocity;
        simulate(pl, p, nullptr, false, push, 1, 0, true, sdt, sc); // without (one tick as it is)
        double const v0 = m_simPlayer1->m_yVelocity;
        m_recordPath = rec;
        if (std::fabs(v1 - v0) < 1.0) return; // not a clean check
        m_lagOn = true;
        m_lagTick = m_tick;
        m_lagV1 = v1;
        m_lagV0 = v0;
    }
    void lagPoll(PlayerObject* p) {
        if (!m_lagOn || !p) return;
        long long const L = m_tick - 1 - m_lagTick; // ticks played since the one it was sent on
        if (L < 0) return;
        if (p->m_isDead || L > 3) { m_lagOn = false; return; }
        double const tol = std::max(0.3, 0.04 * std::fabs(m_lagV1));
        double const v = p->m_yVelocity;
        if (std::fabs(v - m_lagV1) < tol) {
            m_lagOn = false;
            m_lagSamples.push_back(static_cast<int>(L));
            if (m_lagSamples.size() > 15) m_lagSamples.erase(m_lagSamples.begin());
            if (m_lagSamples.size() >= 3) {
                std::vector<int> v2 = m_lagSamples;
                std::nth_element(v2.begin(), v2.begin() + static_cast<std::ptrdiff_t>(v2.size() / 2), v2.end());
                int const med = std::clamp(v2[v2.size() / 2], 0, 3);
                if (med != m_fireLag || !m_lagLogged) {
                    m_lagLogged = true;
                    m_fireLag = med;
                    pushLog(fmt::format("input timing: inputs the mod sends reach the player {} tick{} after they're sent "
                                        "({} checks){}", med, med == 1 ? "" : "s", m_lagSamples.size(),
                                        med > 0 ? " - every buffered input lands that late; autoplay sends them earlier" : ""),
                            med > 0 ? COL_ORANGE : COL_GREEN, false);
                    trace(fmt::format("input timing: {} tick(s) late", med));
                }
            }
        } else if (L == 0 && std::fabs(v - m_lagV0) >= tol) {
            m_lagOn = false; // neither: something else changed your speed - not a clean check
        }
    }

    void fireButton(PlayLayer* pl, bool push, bool directOnly = false) {
        lagArm(pl, push); // (2.3) is it on time?
        bool const checking = driftStartInput(pl, push, true); // (2.3) and the 100 ms after it, checked
        bool const dualSingle = pl->m_gameState.m_isDualMode && (!pl->m_level || !pl->m_level->m_twoPlayerMode);
        m_gameHolding = push;
        m_osInput = true;
        m_dfInput = true;
        rememberInput(pl, push);
        // (2.3) autoplay: straight to the player - an input mod (Click Between
        // Frames) taking it through its own queue would re-time it and split
        // the step, and autoplay counts steps
        if (!m_directMode && !directOnly) {
            m_firingBuffered = true;
            pl->handleButton(push, 1, true);
            if (dualSingle) pl->handleButton(push, 1, false);
            m_firingBuffered = false;
        }

        // The input must reach the player on THIS tick - that is the tick the
        // prediction chose. Input mods (Click Between Frames) hook the same
        // function and can hold back or re-time inputs that don't come from
        // them. If the player didn't get it, apply it directly, now.
        auto apply = [&](PlayerObject* p) {
            if (!p || holdingJump(p) == push) return false;
            if (push) p->pushButton(PlayerButton::Jump);
            else      p->releaseButton(PlayerButton::Jump);
            return true;
        };
        bool direct = apply(pl->m_player1);
        if (dualSingle) direct = apply(pl->m_player2) || direct;
        if (direct && !directOnly) {
            m_directFires++;
            if (!m_directMode) {
                // From now on apply buffered inputs straight to the player, so a
                // late copy from that mod can't arrive as a second press.
                m_directMode = true;
                pushLog("note: another input mod held back a buffered input - applying them directly from now on",
                        COL_YELLOW, false);
            }
        }
        if (checking) m_dfInput = false; // (this input is the check's own)
    }

    // Make the game's hold state match the real button so nothing can stay stuck.
    // Nothing pending: make the game match the button. Only ever RELEASES on
    // its own - a stale "held" (release lost in the pause menu) used to make it
    // press for you.
    void reconcile(PlayLayer* pl) {
        if (m_gameHolding && !m_physicalHolding) fireButton(pl, false);
    }


    // ------------------------------------------------------------------------
    //  Safety nets (1.18.1): no input is ever lost or held forever.
    // ------------------------------------------------------------------------
    // Fire everything still waiting, in order, right now.
    // Diagnostics only: every 8 ticks, predict ONE tick from the real state with
    // the current button state, and compare with what the game did. Skipped if
    // an input happened in that tick or the step wasn't a full tick (CBF).
    void oneStepCheck(PlayLayer* pl, PlayerObject* p) {
        float const nominal = getValidDt();
        if (m_osPending) {
            m_osPending = false;
            if (!m_osInput && std::fabs(m_lastStepD - nominal) < 0.02f * nominal && !p->m_isDead) {
                float const dx = p->getPositionX() - m_osX;
                float const dy = p->getPositionY() - m_osY;
                double const dv = p->m_yVelocity - m_osVy;
                OneStep& o = m_oneStep[m_osMode];
                o.n++;
                float const off = std::hypot(dx, dy);
                if (off < 0.01f && std::fabs(dv) < 0.01) o.exact++;
                o.sdx += std::clamp(dx, -20.f, 20.f);
                o.sdy += std::clamp(dy, -20.f, 20.f);
                o.sdv += std::clamp(dv, -20.0, 20.0);
                o.worst = std::max(o.worst, off);
                if (off > 0.5f && !o.logged && o.n >= 3) {
                    o.logged = true;
                    pushLog(fmt::format("one-tick check [{}]: prediction differs after ONE tick (x {:+.2f}, y {:+.2f}, "
                                        "y-speed {:+.2f}) - your state isn't fully copied", m_osMode, dx, dy, dv),
                            COL_YELLOW, false);
                }
            }
        }
        if ((m_tick & 7) != 0 || m_queue.pending() || p->m_isDead || m_simulating || tickPhase() > 0.f) return;
        PlayerObject* p2 = (pl->m_gameState.m_isDualMode && pl->m_player2) ? pl->m_player2 : nullptr;
        bool const simP2 = p2 && !(pl->m_level && pl->m_level->m_twoPlayerMode);
        bool const rec = m_recordPath;
        m_recordPath = false;
        float const sdt = simDt();
        // delay 1 > horizon 0: one step with the button exactly as the game has it
        simulate(pl, p, p2, simP2, m_gameHolding, 1, 0, true, sdt, BASE_DT / std::max(0.001f, sdt));
        m_recordPath = rec;
        PlayerObject* s1 = m_simPlayer1.data();
        if (!s1) return;
        m_osX = s1->getPositionX();
        m_osY = s1->getPositionY();
        m_osVy = s1->m_yVelocity;
        ModeInfo mi;
        mi.add(p);
        m_osMode = mi.name();
        m_osInput = false;
        m_osPending = true;
    }


    // ------------------------------------------------------------------------
    //  Prediction check (2.3, Dev tab; setting "Prediction Check", on by
    //  default): every 32 ticks the next 100 ms are predicted with the button
    //  as it is, and the real player is followed tick by tick against it. An
    //  input in between and that check doesn't count. Per gamemode ("+portal"
    //  when the prediction went through a portal itself): how often it stays
    //  within half a pixel, the average / worst miss, where it first went off,
    //  and deaths it got wrong. The first clear miss per gamemode goes in the
    //  Log with where it went off - what to fix.
    // ------------------------------------------------------------------------
    struct DriftStat {
        long long n = 0, close = 0, falseDeath = 0, missedDeath = 0, offN = 0, offTickSum = 0;
        double errSum = 0.0;
        float worst = 0.f;
        bool logged = false;
    };
    std::map<std::string, DriftStat> m_drift;
    std::vector<acecore::Point2> m_dfPath;
    std::string m_dfKey, m_dfStart;
    bool  m_dfActive = false, m_dfInput = false, m_dfSimDied = false, m_dfSkip = false;
    int   m_dfAt = 0, m_dfK = 0, m_dfFirstOff = -1, m_dfSettle = 0;
    float m_dfWorst = 0.f, m_dfOffDx = 0.f, m_dfOffDy = 0.f;

    long long portalsInSims() const {
        return m_simSpeedSwitches + m_simGravFlips + m_simModeSwitches + m_simSizeSwitches + m_simMirrors;
    }
    void driftFinish(bool falseDeath, bool missedDeath) {
        m_dfActive = false;
        DriftStat& d = m_drift[m_dfKey];
        d.n++;
        if (!falseDeath && !missedDeath && m_dfWorst < 0.5f) d.close++;
        if (falseDeath) d.falseDeath++;
        if (missedDeath) d.missedDeath++;
        d.errSum += std::min(m_dfWorst, 50.f);
        d.worst = std::max(d.worst, m_dfWorst);
        if (m_dfFirstOff > 0) {
            d.offN++;
            d.offTickSum += m_dfFirstOff;
        }
        if (!d.logged && d.n >= 3 && (falseDeath || missedDeath || m_dfWorst > 2.f)) {
            d.logged = true;
            std::string const what = falseDeath    ? std::string("it predicted a death that didn't happen")
                                   : missedDeath   ? std::string("you died where it predicted you'd live")
                                                   : fmt::format("{:.1f}px off within {}t", m_dfWorst, m_dfK);
            std::string const where = (m_dfFirstOff > 0
                ? fmt::format(" (first off at tick {}: x {:+.2f}, y {:+.2f})", m_dfFirstOff, m_dfOffDx, m_dfOffDy)
                : std::string()) + " | start: " + m_dfStart;
            pushLog(fmt::format("prediction check [{}]: {}{}", m_dfKey, what, where), COL_YELLOW, false);
            trace(fmt::format("prediction check [{}]: {}{}", m_dfKey, what, where));
        }
    }
    // (2.3) The same check for an input: right when it goes in (yours as
    // made, or one the mod sends), the next 100 ms are predicted WITH it and
    // followed like above - "ship release", "robot release (sent)", ... in the
    // Dev tab. `nextTick`: it reaches the player on the next tick, not this one
    // (yours arrive between ticks; the mod's go in on the tick).
    bool driftStartInput(PlayLayer* pl, bool push, bool sent) {
        if (!cfg().driftCheck || m_simulating || !pl || !pl->m_player1 || !m_simPlayer1.data()) return false;
        PlayerObject* p = pl->m_player1;
        if (p->m_isDead || p->m_isPlatformer || tickPhase() > 0.f || m_dfSettle > 0) return false;
        PlayerObject* p2 = (pl->m_gameState.m_isDualMode && pl->m_player2) ? pl->m_player2 : nullptr;
        bool const simP2 = p2 && !(pl->m_level && pl->m_level->m_twoPlayerMode);
        float const sdt = simDt();
        float const sc = BASE_DT / std::max(0.001f, sdt);
        int const K = std::clamp(scaleTicks(ACCURACY_CHECK_BASE, sc), 4, MAX_SIM_STEPS);
        Config const& c = cfg();
        m_orbPredict = c.orbPredict;
        m_padPredict = c.padPredict;
        m_predictPortals = c.predictPortals;
        bool const g = m_gameHolding;
        m_gameHolding = !push; // the button as it was right before
        long long const extraBefore = m_simExtraObjs;
        bool const rec = m_recordPath;
        m_recordPath = true;
        m_hasDeadline = false;
        SimResult const r = simulate(pl, p, p2, simP2, push, 0, K, true, sdt, sc);
        m_recordPath = rec;
        m_gameHolding = g;
        if (r.end == SimEnd::Truncated || r.end == SimEnd::OutOfBudget) { m_dfActive = false; return false; }
        m_dfPath = m_path;
        m_dfSimDied = r.end == SimEnd::Died;
        ModeInfo mi;
        mi.add(p);
        m_dfKey = mi.name() + (push ? " press" : " release") + (sent ? " (sent)" : "");
        m_dfStart = fmt::format("x {:.0f} y {:.2f}, y speed {:.3f}, on ground {}, objects added {}", p->getPositionX(),
                                p->getPositionY(), p->m_yVelocity, p->m_isOnGround ? "yes" : "no", m_simExtraObjs - extraBefore);
        m_dfActive = true;
        m_dfInput = false;
        m_dfSkip = !sent; // yours: GD takes it in at the next tick
        m_dfAt = 0;
        m_dfK = K;
        m_dfFirstOff = -1;
        m_dfWorst = 0.f;
        return true;
    }
    void driftCheck(PlayLayer* pl, PlayerObject* p) {
        float const nominal = getValidDt();
        if (m_dfActive) {
            if (m_dfSkip) { m_dfSkip = false; return; }
            bool const fullStep = std::fabs(m_lastStepD - nominal) < 0.02f * nominal;
            if (m_dfInput || !fullStep) { m_dfActive = false; return; } // an input / a short step: doesn't count
            if (p->m_isDead) {
                // right if the prediction died here too (a tick either way)
                bool const agreed = m_dfSimDied && std::abs(static_cast<int>(m_dfPath.size()) - m_dfAt) <= 1;
                driftFinish(false, !agreed);
                return;
            }
            size_t const k = static_cast<size_t>(m_dfAt);
            if (k >= m_dfPath.size()) {
                driftFinish(m_dfSimDied, false); // it died there and you didn't - or the check is done
                return;
            }
            float const dx = p->getPositionX() - m_dfPath[k].x;
            float const dy = p->getPositionY() - m_dfPath[k].y;
            float const off = std::hypot(dx, dy);
            if (off > 0.5f && m_dfFirstOff < 0) {
                m_dfFirstOff = m_dfAt + 1;
                m_dfOffDx = dx;
                m_dfOffDy = dy;
            }
            m_dfWorst = std::max(m_dfWorst, off);
            m_dfAt++;
            if (m_dfAt >= m_dfK) driftFinish(false, false);
            return;
        }
        if (m_dfSettle > 0) { m_dfSettle--; return; } // just respawned: the game may still move you
        if ((m_tick & 31) != 0 || m_queue.pending() || m_live || p->m_isDead || m_simulating || tickPhase() > 0.f) return;
        PlayerObject* p2 = (pl->m_gameState.m_isDualMode && pl->m_player2) ? pl->m_player2 : nullptr;
        bool const simP2 = p2 && !(pl->m_level && pl->m_level->m_twoPlayerMode);
        float const sdt = simDt();
        float const sc = BASE_DT / std::max(0.001f, sdt);
        int const K = std::clamp(scaleTicks(ACCURACY_CHECK_BASE, sc), 4, MAX_SIM_STEPS);
        Config const& c = cfg();
        m_orbPredict = c.orbPredict;
        m_padPredict = c.padPredict;
        m_predictPortals = c.predictPortals;
        long long const before = portalsInSims();
        long long const extraBefore = m_simExtraObjs;
        bool const rec = m_recordPath;
        m_recordPath = true;
        m_hasDeadline = false;
        // delay K > horizon 0: K ticks with the button exactly as the game has it
        SimResult const r = simulate(pl, p, p2, simP2, m_gameHolding, K, 0, true, sdt, sc);
        m_recordPath = rec;
        if (r.end == SimEnd::Truncated || r.end == SimEnd::OutOfBudget) return; // blind past something: no check
        m_dfPath = m_path;
        m_dfSimDied = r.end == SimEnd::Died;
        // (and how many blocks / spikes GD didn't hand over the prediction took
        // from the mod's own list - a moved / hidden block there would explain a miss)
        m_dfStart = fmt::format("x {:.0f} y {:.2f} (GD y {:.2f}), y speed {:.3f}, on ground {}, held {}, objects added {}",
                                p->getPositionX(), p->getPositionY(), p->m_position.y, p->m_yVelocity,
                                p->m_isOnGround ? "yes" : "no", m_gameHolding ? "yes" : "no", m_simExtraObjs - extraBefore);
        ModeInfo mi;
        mi.add(p);
        m_dfKey = mi.name() + (portalsInSims() != before ? " +portal" : "");
        m_dfActive = true;
        m_dfInput = false;
        m_dfAt = 0;
        m_dfK = K;
        m_dfFirstOff = -1;
        m_dfWorst = 0.f;
    }

    // ------------------------------------------------------------------------
    //  Autoplay (2.3, Tools tab - a test). The mod plays the level itself with
    //  the same hidden-player physics the buffer uses: it plans 0.5 s ahead
    //  (the button as it is when that lives, else the escape search finds
    //  where to press / let go), follows the plan tick by tick and plans again
    //  every quarter second. Every tick it checks you against the plan:
    //    * you part from it -> the prediction is wrong right there ("drift",
    //      Log + Dev tab: gamemode, how far, how many ticks in),
    //    * you die -> the Log says on what, and whether the plan thought you'd
    //      live (a prediction bug - what to fix) or it found no way through
    //      (a search limit, or something the hidden player can't do).
    //  Your clicks are ignored while it plays. Nothing is saved as progress in
    //  a level it played (no %, no completion, no new best) until you leave it.
    // ------------------------------------------------------------------------
    struct AutoStat { long long plans = 0, drifts = 0, deaths = 0, simBugs = 0; float worst = 0.f; };
    struct AutoPlan {
        std::vector<int> ch;                // ticks (from the plan's start) with an input
        std::vector<uint8_t> held;          // the button on each tick
        std::vector<acecore::Point2> path;   // where you should be after each tick
        int  k = 0;                         // ticks played since it was made
        bool valid = false, ok = false, drifted = false;
        std::string mode;
    };
    static constexpr int   AUTO_HORIZON_BASE = 120; // 0.5 s ahead (240 TPS)
    static constexpr float AUTO_DRIFT_PX = 1.f;     // further off the plan than this = a prediction miss (logged)
    static constexpr float AUTO_REPLAN_PX = 3.f;    // ...and this far off, it plans again from where you are
    std::map<std::string, AutoStat> m_autoStats;
    // (2.3) where autoplay died this session (this level): the overlay marks
    // them, and a route search near one looks further back for its input
    // (died there before = the fix comes earlier)
    // (2.3) for the death record: the last input autoplay made, the last
    // portal / orb / speed / gravity / size change, and the kinds of deaths
    long long m_autoLastInTick = -1, m_transTick = -1;
    bool m_autoLastInPush = false;
    float m_autoLastInX = 0.f;
    std::string m_transWhat;
    int m_transMode = -1, m_transOrbs = 0;
    bool m_transUp = false;
    float m_transSpeed = 0.f, m_transSize = 0.f;
    std::map<std::string, int> m_deathKinds;
    void transTick(PlayerObject* p) {
        int const mc = modeCode(p);
        int const orbs = static_cast<int>(m_realUsedOrbs.size());
        char const* what = nullptr;
        if (m_transMode >= 0) {
            if (mc != m_transMode) what = "a gamemode portal";
            else if (p->m_isUpsideDown != m_transUp) what = "a gravity change";
            else if (std::fabs(p->m_playerSpeed - m_transSpeed) > 0.01f) what = "a speed change";
            else if (std::fabs(p->m_vehicleSize - m_transSize) > 0.01f) what = "a size portal";
            else if (orbs > m_transOrbs) what = "an orb";
        }
        if (what) {
            m_transTick = m_tick;
            m_transWhat = what;
        }
        m_transMode = mc;
        m_transUp = p->m_isUpsideDown;
        m_transSpeed = p->m_playerSpeed;
        m_transSize = p->m_vehicleSize;
        m_transOrbs = orbs;
    }
    struct AutoDeath { float x = 0.f, y = 0.f; int times = 1; };
    std::vector<AutoDeath> m_autoDeaths;
    int autoDeathsNear(float x) const {
        int n = 0;
        for (auto const& d : m_autoDeaths) if (std::fabs(d.x - x) < 90.f) n = std::max(n, d.times);
        return n;
    }
    std::deque<std::string> m_autoNotes;
    AutoPlan m_ap;
    bool m_autoOn = false, m_autoUsed = false;
    long long m_autoRuns = 0, m_autoPlans = 0;
    int m_autoDriftLogs = 0;
    bool m_autoFallbackLogged = false;
    long long m_autoLastDriftPlan = -1000;

    void autoNote(std::string msg, bool important) {
        trace("autoplay: " + msg);
        pushLog("AUTOPLAY " + msg, important ? COL_ORANGE : COL_YELLOW, false);
        m_autoNotes.push_back(std::move(msg));
        while (m_autoNotes.size() > 30) m_autoNotes.pop_front();
    }
    int autoHorizon() const {
        float const sc = BASE_DT / std::max(0.001f, simDt());
        return std::clamp(scaleTicks(AUTO_HORIZON_BASE, sc), MIN_HORIZON, MAX_SIM_STEPS - 1);
    }
    // One test from the real state: the button flips on the ticks in `ch`
    // (from now), H ticks in all.
    // The button on each of the next H ticks for a plan: from the button as it
    // is, every tick in `ch` is an input - in cube / ball / spider / UFO /
    // swing a tap (press, let go a tick later: holding there re-jumps on every
    // landing), in ship / wave / robot the button flips and stays.
    std::vector<uint8_t> autoSchedule(std::vector<int> const& ch, int H, bool tap) const {
        std::vector<uint8_t> held(static_cast<size_t>(std::max(1, H)), (tap ? false : m_gameHolding) ? 1 : 0);
        bool cur = tap ? false : m_gameHolding;
        size_t k = 0;
        for (int t = 0; t < H; t++) {
            bool tapNow = false;
            while (k < ch.size() && ch[k] == t) {
                if (tap) tapNow = true;
                else cur = !cur;
                k++;
            }
            held[static_cast<size_t>(t)] = tap ? (tapNow ? 1 : 0) : (cur ? 1 : 0);
        }
        return held;
    }
    bool autoTapMode(PlayLayer* pl) const { return pl && pl->m_player1 && spTapMode(modeCode(pl->m_player1)); }
    // One test from the real state with that button schedule.
    SimResult autoRun(PlayLayer* pl, std::vector<int> const& ch, int H, bool record) {
        bool const isDual = pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr;
        bool const twoPlayer = pl->m_level && pl->m_level->m_twoPlayerMode;
        PlayerObject* p2 = isDual ? pl->m_player2 : nullptr;
        bool const simP2 = isDual && !twoPlayer;
        float const sdt = simDt();
        float const sc = BASE_DT / std::max(0.001f, sdt);
        std::vector<uint8_t> const held = autoSchedule(ch, H, autoTapMode(pl));
        // the first change of the button is "the input", the rest follow-ups
        bool prev = m_gameHolding;
        int delay = -1;
        bool push = false;
        FollowUp f;
        for (int t = 0; t < H; t++) {
            bool const h = held[static_cast<size_t>(t)] != 0;
            if (h == prev) continue;
            prev = h;
            if (delay < 0) {
                delay = t;
                push = h;
            } else if (!f.add(t, h)) {
                break; // more inputs than a test takes: the rest of it as is
            }
        }
        int horizon = 1;
        if (delay < 0) {
            delay = H; // no input: one past the end
            push = !m_gameHolding;
        } else {
            horizon = std::max(1, H - delay);
        }
        bool const rec = m_recordPath;
        m_recordPath = record;
        m_hasDeadline = false;
        SimResult const r = simulate(pl, pl->m_player1, p2, simP2, push, delay, horizon, true, sdt, sc, f);
        m_recordPath = rec;
        m_autoRuns++;
        return r;
    }
    void autoPlan(PlayLayer* pl) {
        int const H = autoHorizon();
        float const sc = BASE_DT / std::max(0.001f, simDt());
        bool const tap = autoTapMode(pl);
        Config const& c = cfg();
        m_orbPredict = c.orbPredict;
        m_padPredict = c.padPredict;
        m_predictPortals = c.predictPortals;
        auto lives = [](SimResult const& r) { return r.end == SimEnd::Survived || r.end == SimEnd::Truncated; };
        std::vector<int> plan;
        bool ok = false;
        SimResult const keep = autoRun(pl, {}, H, false);
        if (lives(keep)) {
            ok = true;
        } else if (keep.end == SimEnd::Died) {
            acecore::EscapeParams ep;
            ep.first = 0;
            ep.space = std::max(tap ? 2 : 1, scaleTicks(2, sc));
            ep.lead = 1;
            ep.grace = 0;
            ep.maxChanges = tap ? 3 : 6; // (taps are two inputs each)
            ep.maxRuns = 40;
            auto run = [&](std::vector<int> const& ch) -> acecore::EscapeTry {
                SimResult const r = autoRun(pl, ch, H, false);
                if (lives(r)) return acecore::EscapeTry{acecore::EscapeEnd::Survived, 0};
                if (r.end == SimEnd::OutOfBudget) return acecore::EscapeTry{acecore::EscapeEnd::Budget, 0};
                return acecore::EscapeTry{acecore::EscapeEnd::Died, std::max(0, r.deathStep)};
            };
            acecore::EscapeResult const er = acecore::findEscape(ep, std::max(1, keep.deathStep), run);
            if (er.escape == acecore::Escape::Yes) {
                plan = er.changes;
                ok = true;
            } else {
                plan = er.best; // as far as it gets
            }
        }
        autoRun(pl, plan, H, true); // the plan's path, tick by tick
        ModeInfo mi;
        mi.add(pl->m_player1);
        m_ap = AutoPlan{};
        m_ap.held = autoSchedule(plan, H, tap);
        m_ap.ch = std::move(plan);
        m_ap.path = m_path;
        m_ap.valid = true;
        m_ap.ok = ok;
        m_ap.mode = mi.name();
        m_autoStats[m_ap.mode].plans++;
        m_autoPlans++;
        if (!m_autoFallbackLogged) {
            m_autoFallbackLogged = true;
            autoNote(fmt::format("the route doesn't reach here (x {:.0f}) - playing a quick 0.5 s plan meanwhile",
                                 pl->m_player1->getPositionX()), false);
        }
    }
    // ---- the route (2.3): planned continuously, ahead to the end of the level ----
    // Like the level scan's sprint, but while autoplay plays: from where you
    // are, the hidden player runs on with the button as it is; where it would
    // die, an input is tried one step earlier at a time (latest first) from
    // just before the death back up to a second; nothing saves it -> the last
    // input is taken back and one earlier than it is looked for (up to 8
    // times). Every tick gets a slice of CPU, so the route is found well
    // before you get there - up to 10 s ahead, or to the end. Autoplay plays
    // the route's button tick by tick and checks you against its states.
    static constexpr int RT_CHUNK_BASE  = 60;   // ticks per forward run
    static constexpr int RT_BACK_BASE   = 240;  // inputs tried up to 1 s before a death
    static constexpr int RT_MARGIN_BASE = 48;   // a try has to live this long past the old death
    static constexpr int RT_AHEAD_BASE  = 2400; // the route reaches at most 10 s ahead of you
    static constexpr int RT_SLICE_US    = 700;  // planning time per tick
    static constexpr int AP_FRAME_US       = 2000; // (2.3) ... and all of a frame's ticks together
    static constexpr int AP_FRAME_BURST_US = 5000; // ...when the route is about to run out
    long long m_apFrameSpent = 0, m_apFrameSkips = 0;
    static constexpr int RT_FAR_US      = 250;  // ...when the route is already 3 s ahead
    static constexpr int RT_BURST_US    = 4000; // ...and when it's less than 0.5 s ahead
    static constexpr int RT_UNDO_MAX    = 64;   // (2.3) inputs taken back while fighting one spot (was 8)
    static constexpr float RT_PROGRESS_PX = 30.f; // ...until the route is this far past it
    static constexpr int RT_VERIFY_BASE = 480;  // (2.3) the next 2 s of the route are checked again...
    static constexpr int RT_VERIFY_EVERY = 12;  // ...every 12 ticks
    static constexpr int RT_VERIFY_US   = 400;  // ...with this much time per tick
    // (2.3) a stretch that now dies, or ends this far from the route's state,
    // is stale. It was 0.5 px: replaying from a saved state is a little off
    // by itself (sub-pixel to ~3 px), so far ahead the route was cut and
    // planned again all the time ("199 stale") and never got ahead of you -
    // the next jump was planned while you were already at it, and its input
    // kept moving. The level changing (triggers) shows as a death or a big
    // miss; a small miss is logged (field by field) for fixing, not acted on.
    static constexpr float RT_VERIFY_PX = 4.f;
    long long m_rtReplayMiss = 0; // stretches that replayed a little off (< RT_VERIFY_PX)
    int m_rtReplayLogs = 0;
    std::deque<SpTick>  m_rt;      // m_rt[i]: the state at the start of route tick m_rtBase + i
    std::deque<uint8_t> m_rtHeld;  // the button during route tick m_rtBase + i
    std::map<int, bool> m_rtEvents;
    std::vector<std::pair<int, GameObject*>> m_rtOrbs; // orbs the route uses, by the tick of the stretch that uses them
    int   m_rtBase = 0, m_rtLastEvent = -1, m_rtNow = 0, m_rtUndo = 0, m_rtHi = 1 << 30;
    bool  m_rtOn = false, m_rtStuck = false, m_rtDone = false, m_rtDoneLogged = false;
    float m_rtStuckX = 0.f, m_rtEndX = 1e9f, m_rtLastX = 0.f, m_rtLastY = 0.f;
    std::string m_rtStuckWhy;
    long long m_rtRuns = 0, m_rtStarts = 0;
    int   m_autoSettle = 0;
    bool  m_rtSim = false; // the current test is the route's
    long long m_rtStuckAt = 0;
    float m_stepDtNow = 0.f, m_rtPhase = 0.f; // (2.3) autoplay counts whole steps (CBF splits some)
    bool  m_rtDriftSeen = false;
    int   m_rtStuckLogs = 0;
    long long m_rtPortalWidens = 0; // (2.3) searches that looked back past a portal
    float m_rtFrontX = 0.f;         // (2.3) furthest the route got (take-backs reset past it)
    int   m_rtVerAt = 0;            // (2.3) the next route tick to check again
    long long m_rtVerTick = -1, m_rtVerRuns = 0, m_rtVerFixes = 0, m_rtUnstuck = 0;
    std::string m_rtVerLast;
    int m_rtVerLogs = 0;
    int m_rtVerLastFix = -1000000; // route tick of the last stale stretch
    bool m_rtVerOff = false;       // a replayed stretch didn't match twice in a row at one spot: off until the next route


    int rtEnd() const { return m_rtBase + static_cast<int>(m_rt.size()) - 1; }
    SpTick const& rtAt(int t) const { return m_rt[static_cast<size_t>(t - m_rtBase)]; }
    bool rtHeldAt(int t) const {
        if (m_rtHeld.empty()) return false;
        if (t < m_rtBase) return m_rtHeld.front() != 0;
        size_t const i = static_cast<size_t>(t - m_rtBase);
        return i < m_rtHeld.size() ? m_rtHeld[i] != 0 : m_rtHeld.back() != 0;
    }
    // (2.3) one route = one step length for all its tests (set when it starts),
    // so a stretch replays exactly as it was planned
    float m_rtDt = 0.f;
    float rtDt() const { return m_rtDt > 0.001f ? m_rtDt : simDt(); }
    float rtS() const { return BASE_DT / std::max(0.001f, rtDt()); }

    void rtStart(PlayLayer* pl, PlayerObject* p) {
        if (m_rtAOn) { // a new route: the A* belonged to the old one
            m_rtAOn = false;
            m_rtA = AStarRun{};
        }
        m_rtDt = simDt();
        m_rt.clear();
        m_rtHeld.clear();
        m_rtEvents.clear();
        m_rtOrbs.clear();
        SpTick t;
        t.s1 = snapFrom(p);
        bool const dual = pl->m_gameState.m_isDualMode && pl->m_player2 && !(pl->m_level && pl->m_level->m_twoPlayerMode);
        t.dual = dual;
        if (dual) {
            t.s2 = snapFrom(pl->m_player2);
            t.y2 = pl->m_player2->getPositionY();
        }
        t.x = p->getPositionX();
        t.y = p->getPositionY();
        t.vy = p->m_yVelocity;
        t.modeCode = modeCode(p);
        t.mini = p->m_vehicleSize > 0.f && p->m_vehicleSize < 0.99f;
        bool const tapMode = spTapMode(t.modeCode);
        m_rs = RtSearch{};
        m_rtDriftSeen = false;
        m_rtPhase = 0.f;
        float const t0x = t.x;
        m_rt.push_back(std::move(t));
        // tap modes (cube, ball, ...) start let go: a held button re-jumps on every landing
        m_rtHeld.push_back((!tapMode && m_gameHolding) ? 1 : 0);
        m_rtBase = 0;
        m_rtLastEvent = -1;
        m_rtNow = 0;
        m_rtUndo = 0;
        m_rtFrontX = t0x;
        m_rtVerAt = 0;
        m_rtVerOff = false;
        m_rtVerLastFix = -1000000;
        m_rtHi = 1 << 30;
        m_rtStuck = m_rtDone = false;
        m_rtStuckWhy.clear();
        m_rtEndX = pl->m_levelLength > p->getPositionX() + 30.f ? pl->m_levelLength : 1e9f; // (no length known: no end)
        m_rtOn = true;
        m_rtStarts++;
    }
    // One run from route tick t (like the sprint's): `push` from its start,
    // or `keep` = carry on as is. (2.3) `hold` > 0: the button flips back
    // after that many ticks - a short click (or a short let-go); 1 = a tap.
    SimResult rtRun(PlayLayer* pl, int t, bool push, int hold, int ticks, std::vector<SpTick>& trail, bool keep) {
        bool const before = keep ? rtHeldAt(t) : (t > m_rtBase ? rtHeldAt(t - 1) : rtHeldAt(t));
        return snapRun(pl, rtAt(t), before, push, hold, ticks, trail, keep, t, rtDt());
    }
    // One run of the hidden player from a saved state (the route's / the
    // audit's): the button was `before` until now; `push` from the start
    // (`hold` > 0: flips back after that many ticks), or `keep` = carry on.
    SimResult snapRun(PlayLayer* pl, SpTick const& from, bool before, bool push, int hold, int ticks, std::vector<SpTick>& trail,
                      bool keep, int orbBefore, float sdtIn) {
        trail.clear();
        bool const g = m_gameHolding;
        float const ph = m_phase;
        bool const rec = m_recordPath;
        std::vector<GameObject*> const usedOrbs = m_realUsedOrbs;
        m_gameHolding = before;
        m_phase = 0.f;
        m_recordPath = false;
        m_hasDeadline = false;
        // (2.3) only orbs used before this run: one this stretch uses itself
        // is still there for it (re-checks replay that stretch)
        for (auto const& [ot, o] : m_rtOrbs)
            if (ot < orbBefore) m_realUsedOrbs.push_back(o);
        m_simSnap = &from.s1;
        m_simSnap2 = from.dual ? &from.s2 : nullptr;
        m_scanSim = true; // the scan's rules: portals, floor / ceiling, out of the level = dead
        m_rtSim = true;   // ...but empty air isn't (a start position in the open)
        m_simTrail = &trail;
        float const sdt = sdtIn;
        float const sc = BASE_DT / std::max(0.001f, sdt);
        FollowUp f;
        if (hold > 0) f.add(hold, !push);
        Config const& c = cfg();
        m_orbPredict = c.orbPredict;
        m_padPredict = c.padPredict;
        m_predictPortals = c.predictPortals;
        SimResult const r = keep ? simulate(pl, pl->m_player1, nullptr, false, push, ticks, 0, true, sdt, sc)
                                 : simulate(pl, pl->m_player1, nullptr, false, push, 0, ticks, true, sdt, sc, f);
        m_simTrail = nullptr;
        m_scanSim = false;
        m_rtSim = false;
        m_simSnap = nullptr;
        m_simSnap2 = nullptr;
        m_gameHolding = g;
        m_phase = ph;
        m_recordPath = rec;
        m_realUsedOrbs = usedOrbs;
        m_rtRuns++;
        m_autoRuns++;
        return r;
    }
    void rtAppend(int t, bool held0, bool heldRest, std::vector<SpTick>& trail, GameObject* orb, int orbStep = 0) {
        size_t const keep = static_cast<size_t>(t - m_rtBase + 1);
        if (m_rt.size() > keep) m_rt.resize(keep);
        m_rtHeld.resize(keep);
        for (auto& st : trail) {
            m_rt.push_back(std::move(st));
            m_rtHeld.push_back(heldRest ? 1 : 0);
        }
        m_rtHeld[keep - 1] = held0 ? 1 : 0;
        m_rtEvents.erase(m_rtEvents.upper_bound(t - 1), m_rtEvents.end());
        std::erase_if(m_rtOrbs, [&](auto const& e) { return e.first >= t; });
        if (orb) m_rtOrbs.emplace_back(t + std::max(0, orbStep), orb); // the tick it's used on
    }
    // (2.3) an input at t that flips back after `hold` ticks (0 = stays)
    void rtAppendHold(int t, bool push, int hold, std::vector<SpTick>& trail, GameObject* orb, int orbStep = 0) {
        rtAppend(t, push, hold > 0 ? !push : push, trail, orb, orbStep);
        if (hold > 1) {
            for (int i = 1; i < hold; i++) {
                size_t const k = static_cast<size_t>(t + i - m_rtBase);
                if (k < m_rtHeld.size()) m_rtHeld[k] = push ? 1 : 0;
            }
        }
    }
    // The route back to tick t (an input taken back there).
    void rtTruncate(int t) {
        size_t const keep = static_cast<size_t>(std::max(1, t - m_rtBase + 1));
        if (m_rt.size() > keep) m_rt.resize(keep);
        bool const before = t > m_rtBase ? rtHeldAt(t - 1) : rtHeldAt(t);
        m_rtHeld.resize(keep);
        m_rtHeld[keep - 1] = before ? 1 : 0;
        m_rtEvents.erase(m_rtEvents.lower_bound(t), m_rtEvents.end());
        m_rtLastEvent = m_rtEvents.empty() ? -1 : m_rtEvents.rbegin()->first;
        std::erase_if(m_rtOrbs, [&](auto const& e) { return e.first >= t; }); // (2.3) orbs after it are free again
    }
    // A route search in progress (it carries on next tick when the time is up
    // - running out of time is never "nothing saves it").
    struct RtSearch {
        bool on = false;
        int death = 0, jj = 0, lo = 0, step = 1;
        std::string why;
        int bestJ = -1;
        float bestClr = -1.f;
        bool bestTap = false, bestPush = false;
        GameObject* bestOrb = nullptr;
        int bestOrbStep = 0;
        std::vector<SpTick> bestTrail;
        int tries = 0;
        std::map<std::string, int> reasons; // what the tries died on (stuck: the Log says)
        std::map<std::string, int> killAt;  // (2.3) ... and which object killed them, where
        // (2.3) every timing is tried two ways: the button flipped and left
        // (phase 0), and - nothing safe that way, in ship / wave / robot -
        // short clicks / short let-gos of every length (phase 1): a quick tap
        // in the ship, a short robot jump, a wave zig-zag
        int phase = 0, ki = 0, hi = 0, bestK = 0;
        bool widened = false; // (2.3) looked back past a portal too
        struct Safe { int t, k; float c; };
        std::vector<Safe> safe; // the run of safe inputs found
        std::map<int, std::string> tried; // (2.3 debug) phase 0: what each tick's input did
        // (2.3) a tap that lives but changes nothing you can see (a ceiling
        // right above you: no jump) - only used when nothing else does
        int fbJ = -1, fbK = 0, fbOrbStep = 0, useless = 0;
        // (2.3) Retry Deaths: the input that got furthest before dying (tried
        // for real where it died before and nothing lives in the predictions)
        int farJ = -1, farK = 0, farReach = -1;
        bool farPush = false;
        std::vector<SpTick> farTrail;
        // (2.3) every input that died, and how close it came (Retry Deaths
        // tries them for real, nearest miss first)
        struct Cand { int t = 0, k = 0; bool push = false; float pen = -1.f; int reach = 0; };
        std::vector<Cand> cands;
        bool fbPush = false;
        GameObject* fbOrb = nullptr;
        std::vector<SpTick> fbTrail;
    };
    long long m_rtUseless = 0, m_rtUselessUsed = 0, m_rtProgress = 0;
    // (2.3) A* rescue: every way through from a few seconds back
    AStarRun m_rtA;
    bool m_rtAOn = false;
    int m_rtATs = 0;
    float m_rtASpotX = 0.f;
    struct AstarTry { float x = 0.f; int n = 0; };
    std::vector<AstarTry> m_rtATries;
    long long m_rtAStarts = 0, m_rtAFound = 0, m_rtASpliced = 0;
    static constexpr int RT_ASTAR_MAX = 6000; // states expanded at most per search
    bool rtAstarStart(PlayLayer* pl) {
        float const spot = rtAt(rtEnd()).x;
        AstarTry* tr = nullptr;
        for (auto& t : m_rtATries) if (std::fabs(t.x - spot) < 60.f) { tr = &t; break; }
        if (!tr) {
            if (m_rtATries.size() >= 300) return false;
            m_rtATries.push_back(AstarTry{spot, 0});
            tr = &m_rtATries.back();
        }
        if (tr->n >= 2) return false; // tried twice from here (2.5 s and 4.5 s back)
        float const sc = rtS();
        int const back = scaleTicks(tr->n == 0 ? 600 : 1080, sc);
        int const ts = std::max({m_rtNow + scaleTicks(48, sc), m_rtBase + 1, rtEnd() - back});
        if (ts >= rtEnd() - scaleTicks(24, sc)) return false; // no room left before it
        tr->n++;
        SpTick const& q = rtAt(ts);
        ANode root;
        root.snap = q.s1;
        root.useSnap = true;
        root.held = rtHeldAt(ts - 1);
        root.modeCode = q.modeCode;
        root.modeIdx = q.modeIdx;
        root.x = q.x;
        root.y = q.y;
        root.vy = q.vy;
        root.mini = q.mini;
        root.tick = 0;
        if (q.dual) return false; // (dual: not here yet)
        m_rtA = AStarRun{};
        m_rtA.dt = rtDt();
        m_rtA.s = sc;
        m_rtA.dir = m_p1Dir < 0.f ? -1.f : 1.f;
        m_rtA.speed = std::max(0.05f, m_mvStep);
        m_rtA.goalX = spot + 120.f * m_rtA.dir; // 4 blocks past where it dies
        astarPush(m_rtA, std::move(root));
        m_rtAOn = true;
        m_rtATs = ts;
        m_rtASpotX = spot;
        m_rtAStarts++;
        autoNote(fmt::format("no input near x {:.0f} gets past it - trying every way through from x {:.0f} ({:.1f} s back)",
                             spot, q.x, static_cast<float>(rtEnd() - ts) * rtDt() / 60.f), false);
        return true;
    }
    void rtAstarStep(PlayLayer* pl, int budgetUs) {
        if (m_rtATs <= m_rtNow + 1 || m_rtATs > rtEnd()) { // you got there first
            m_rtAOn = false;
            m_rtA = AStarRun{};
            return;
        }
        // the route's rules (empty air isn't death) and the orbs used before it
        std::vector<GameObject*> const used = m_realUsedOrbs;
        for (auto const& [ot, o] : m_rtOrbs) if (ot < m_rtATs) m_realUsedOrbs.push_back(o);
        Config const& c = cfg();
        m_orbPredict = c.orbPredict;
        m_padPredict = c.padPredict;
        m_predictPortals = c.predictPortals;
        m_astarRoute = true;
        bool const done = astarRun(pl, m_rtA, std::max(200, budgetUs), RT_ASTAR_MAX);
        m_astarRoute = false;
        m_realUsedOrbs = used;
        if (!done) return; // carries on next tick
        m_rtAOn = false;
        int end = m_rtA.goal;
        if (end < 0 && m_rtA.deepest >= 0 && (m_rtA.nodes[static_cast<size_t>(end = m_rtA.deepest)].x - m_rtASpotX) * m_rtA.dir < 30.f)
            end = -1; // nothing got past the spot
        if (end < 0) {
            autoNote(fmt::format("tried every way from x {:.0f}: none gets past x {:.0f} ({} states)", rtAt(m_rtATs).x, m_rtASpotX,
                                 m_rtA.expansions), false);
            m_rtA = AStarRun{};
            return;
        }
        m_rtAFound++;
        // put that way into the route: each step played from the route's
        // own state (the same tests as every other input)
        std::vector<int> const chain = astarChain(m_rtA, end);
        int t = m_rtATs;
        int inputs = 0;
        bool whole = true;
        for (size_t i = 1; i < chain.size(); i++) {
            ANode const& par = m_rtA.nodes[static_cast<size_t>(chain[i - 1])];
            ANode const& n = m_rtA.nodes[static_cast<size_t>(chain[i])];
            int const k = n.tick - par.tick;
            if (k <= 0) { whole = false; break; }
            std::vector<SpTick> tr;
            if (n.toggled) {
                bool const tap = n.inputPush && !n.held;
                bool const push = tap ? true : n.held;
                int const hold = tap ? 1 : 0;
                SimResult const r = rtRun(pl, t, push, hold, k, tr, false);
                if (r.end != SimEnd::Survived || tr.empty()) { whole = false; break; }
                rtAppendHold(t, push, hold, tr, m_simOrbObj, m_simOrbStep);
                m_rtEvents[t] = push;
                if (hold > 1) m_rtEvents[t + hold] = !push;
                inputs++;
            } else {
                bool const h = t > m_rtBase ? rtHeldAt(t - 1) : rtHeldAt(t);
                SimResult const r = rtRun(pl, t, h, 0, k, tr, true);
                if (r.end != SimEnd::Survived || tr.empty()) { whole = false; break; }
                rtAppend(t, h, h, tr, m_simOrbObj, m_simOrbStep);
            }
            t = rtEnd();
        }
        m_rtLastEvent = m_rtEvents.empty() ? -1 : m_rtEvents.rbegin()->first;
        m_rtHi = 1 << 30;
        m_rs = RtSearch{};
        m_rtASpliced++;
        autoNote(fmt::format("found a way past x {:.0f}: {} inputs from x {:.0f} ({} states searched){}", m_rtASpotX, inputs,
                             rtAt(m_rtATs).x, m_rtA.expansions, whole ? "" : " - part of it (the rest didn't replay the same)"), false);
        m_rtA = AStarRun{};
    }
    bool m_astarRoute = false; // astarMove: the route's rules
    struct UndoBan { float x = 0.f; bool push = false; };
    std::vector<UndoBan> m_rtUndoBans; // (2.3) inputs taken back while fighting a spot (not tried again there)
    bool rtUndoBanned(float x, bool push) const {
        float const tol = std::max(0.05f, m_mvStep * 0.5f);
        for (auto const& b : m_rtUndoBans) if (b.push == push && std::fabs(b.x - x) < tol) return true;
        return false;
    }
    int m_rtProgressLogs = 0;
    static std::string killSpot(SimResult const& r) {
        if (r.killType == KILL_OUT_OF_LEVEL) return "out of the level";
        if (r.killType == KILL_OFF_PATH) return "off the path";
        std::string const what = r.killType >= 0 ? objectName(static_cast<GameObjectType>(r.killType)) : "a block edge";
        return fmt::format("{} ({:.0f},{:.0f})", what, r.killX, r.killY);
    }
    // (2.3) Does this input change your path in the next ~0.1 s (vs the route
    // without it)? Taps that don't - pressed under a ceiling, where GD doesn't
    // jump - were piling up as a row of presses that do nothing.
    bool rtInputChanges(int jj, std::vector<SpTick> const& trail) const {
        int const n = std::min<int>(static_cast<int>(trail.size()), std::max(6, scaleTicks(24, rtS())));
        for (int i = 0; i < n; i++) {
            int const t = jj + 1 + i;
            if (t > rtEnd()) return true; // the route without it ends (dies) here
            SpTick const& a = trail[static_cast<size_t>(i)];
            SpTick const& b = rtAt(t);
            if (std::fabs(a.y - b.y) > 1.f || std::fabs(a.x - b.x) > 1.f || a.modeCode != b.modeCode) return true;
        }
        return false;
    }
    static std::vector<int> const& rtHoldLens(float sc) {
        static std::vector<int> v;
        static float vs = -1.f;
        if (vs != sc) {
            vs = sc;
            v.clear();
            for (int b : {1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 18, 22, 27, 33, 40, 50, 62, 78, 96}) {
                int const k = std::max(1, scaleTicks(b, sc));
                if (v.empty() || k > v.back()) v.push_back(k);
            }
        }
        return v;
    }
    // the most room from hazards (within 1px of the best), of those the middle timing
    static int rtPickSafe(std::vector<RtSearch::Safe> const& safe) {
        if (safe.empty()) return -1;
        float best = -1.f;
        for (auto const& e : safe) best = std::max(best, e.c);
        std::vector<int> top;
        for (int i = 0; i < static_cast<int>(safe.size()); i++)
            if (safe[static_cast<size_t>(i)].c >= best - 1.f) top.push_back(i);
        std::sort(top.begin(), top.end(), [&](int a, int b) { return safe[static_cast<size_t>(a)].t < safe[static_cast<size_t>(b)].t; });
        return top[top.size() / 2];
    }
    RtSearch m_rs;
    // (2.3 debug) stuck: the level scan's input in that stretch, tried from the route's own state there
    std::string rtScanCompare(PlayLayer* pl, int lo, int hi, int margin) {
        if (m_route.empty() || hi < lo) return "no scanned input there";
        float const x0 = rtAt(lo).x, x1 = rtAt(hi).x;
        for (auto const& rp : m_route) {
            if (!rp.push || rp.x < std::min(x0, x1) - 1.f || rp.x > std::max(x0, x1) + 1.f) continue;
            // the route's tick closest to it
            int best = lo;
            float bd = 1e9f;
            for (int t = lo; t <= hi; t++) {
                float const d = std::fabs(rtAt(t).x - rp.x);
                if (d < bd) { bd = d; best = t; }
            }
            std::vector<SpTick> tr;
            bool const heldBefore = best > m_rtBase ? rtHeldAt(best - 1) : rtHeldAt(best);
            bool const tap = spTapMode(rtAt(best).modeCode) && !heldBefore;
            SimResult const r = rtRun(pl, best, true, tap ? 1 : 0, m_rs.death - best + margin, tr, false);
            SpTick const& st = rtAt(best);
            return fmt::format("the scan presses at x {:.1f} y {:.1f}; from the route there (x {:.1f} y {:.1f}, y speed {:.2f}, {}) "
                               "that press {}", rp.x, rp.y, st.x, st.y, st.vy, st.s1.m_isOnGround ? "on the ground" : "in the air",
                               r.end == SimEnd::Survived ? std::string("lives - the search missed it")
                                                         : r.end == SimEnd::Died ? "dies: " + deathWhat(r) : std::string(simEndName(r.end)));
        }
        return "no scanned input there";
    }
    // (2.3) The route is planned up to 10 s ahead - with the level as it was
    // then. Triggers move, show and hide things meanwhile, and far parts were
    // planned from objects GD hadn't switched on yet; that's why a spot it
    // died at from the start was beaten from a checkpoint right before it.
    // So the next 2 s are checked again and again with the level as it is
    // now: each stretch between button changes is replayed from its own
    // start; one that dies or ends somewhere else -> the route is cut there
    // and planned again from that point (with the time left before you
    // get there).
    void rtVerify(PlayLayer* pl, int budgetUs) {
        if (!m_rtOn || m_rt.empty() || m_rtVerOff) return;
        auto const t0 = Clock::now();
        auto used = [&]() { return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count(); };
        float const sc = rtS();
        int const horizon = scaleTicks(RT_VERIFY_BASE, sc);
        int const chunk = std::max(4, scaleTicks(RT_CHUNK_BASE, sc));
        if (m_rtVerAt <= m_rtNow || m_rtVerAt >= std::min(rtEnd(), m_rtNow + horizon)) {
            // a new pass: from now, every RT_VERIFY_EVERY ticks
            if (m_rtVerTick >= 0 && m_tick - m_rtVerTick < RT_VERIFY_EVERY && m_rtVerAt > m_rtNow) return;
            m_rtVerTick = m_tick;
            m_rtVerAt = m_rtNow + 1;
        }
        std::vector<SpTick> trail;
        int const stop = std::min(rtEnd(), m_rtNow + horizon);
        while (m_rtVerAt < stop && used() < budgetUs) {
            int const s0 = m_rtVerAt;
            bool const held = rtHeldAt(s0);
            bool const edge = s0 > m_rtBase && rtHeldAt(s0 - 1) != held;
            int n = s0 + 1;
            while (n < stop && n - s0 < chunk && rtHeldAt(n) == held) n++;
            int const len = n - s0;
            SimResult const r = edge ? rtRun(pl, s0, held, 0, len, trail, false) : rtRun(pl, s0, held, 0, len, trail, true);
            m_rtVerRuns++;
            bool bad = r.end == SimEnd::Died;
            float off = 0.f;
            if (!bad && r.end == SimEnd::Survived && static_cast<int>(trail.size()) >= len) {
                SpTick const& got = trail[static_cast<size_t>(len - 1)];
                SpTick const& want = rtAt(n);
                off = std::hypot(got.x - want.x, got.y - want.y);
                bad = off > RT_VERIFY_PX;
                if (!bad && off > 0.05f) {
                    m_rtReplayMiss++;
                    if (m_rtReplayLogs < 20) {
                        m_rtReplayLogs++;
                        autoNote(fmt::format("replay check: the stretch from x {:.0f} ({} ticks, {}) replayed from its saved state "
                                             "ends {:.2f}px from the route; replay/route at its end: {}",
                                             rtAt(s0).x, len, edge ? "starts with an input" : "button as it was", off,
                                             snapDiff(got.s1, want.s1)), false);
                    }
                }
            }
            if (bad && !(r.end == SimEnd::Died) && std::abs(s0 - m_rtVerLastFix) <= 4) {
                // the same spot again right after planning it again: replaying
                // from a saved state doesn't match there (not the level) -
                // don't loop on it
                m_rtVerOff = true;
                autoNote(fmt::format("route re-check off for this route: replaying from x {:.0f} ends {:.2f}px off even after "
                                     "planning it again (a saved state that doesn't replay exactly)", rtAt(s0).x, off), false);
                return;
            }
            if (bad) {
                m_rtVerLastFix = s0;
                m_rtVerFixes++;
                m_rtVerLast = fmt::format("at x {:.0f} ({:.1f} s ahead): {}", rtAt(s0).x, (s0 - m_rtNow) * simDt() / 60.f,
                                          r.end == SimEnd::Died ? "now dies - " + deathWhat(r) : fmt::format("now ends {:.1f}px away", off));
                if (m_rtVerLogs < 40) {
                    m_rtVerLogs++;
                    autoNote("route re-checked with the level as it is now: " + m_rtVerLast + " - planning again from there", false);
                }
                // cut there (that stretch's own input too) and plan again from it
                rtTruncate(s0);
                m_rs = RtSearch{};
                m_rtHi = 1 << 30;
                m_rtStuck = m_rtDone = false;
                m_rtUndo = 0;
                m_rtFrontX = rtAt(rtEnd()).x;
                m_rtVerAt = 0;
                return;
            }
            m_rtVerAt = n;
        }
    }
    // (2.3) the level scan's press in [lo, hi], tried from the route's state
    // there: lives -> its tick / hold (and the run in `trail`)
    long long m_rtScanRescues = 0;
    bool rtScanRescue(PlayLayer* pl, int lo, int hi, int margin, int& at, int& hold, std::vector<SpTick>& trail) {
        if (m_route.empty() || hi < lo) return false;
        float const x0 = rtAt(lo).x, x1 = rtAt(hi).x;
        for (auto const& rp : m_route) {
            if (!rp.push || rp.x < std::min(x0, x1) - 1.f || rp.x > std::max(x0, x1) + 1.f) continue;
            int best = lo;
            float bd = 1e9f;
            for (int t = lo; t <= hi; t++) {
                float const d = std::fabs(rtAt(t).x - rp.x);
                if (d < bd) { bd = d; best = t; }
            }
            // the tick nearest the scan's press, and its neighbours
            for (int t : {best, best - 1, best + 1, best - 2, best + 2}) {
                if (t < lo || t > hi) continue;
                bool const heldBefore = t > m_rtBase ? rtHeldAt(t - 1) : rtHeldAt(t);
                if (heldBefore) continue;
                int const h = spTapMode(rtAt(t).modeCode) ? 1 : 0;
                SimResult const r = rtRun(pl, t, true, h, m_rs.death - t + margin, trail, false);
                if (r.end == SimEnd::Survived) {
                    at = t;
                    hold = h;
                    // (2.3 debug) why didn't the search take it? what it got for
                    // this tick, and whether the same test gives the same twice
                    std::vector<SpTick> again;
                    SimResult const r2 = rtRun(pl, t, true, h, m_rs.death - t + margin, again, false);
                    auto it = m_rs.tried.find(t);
                    autoNote(fmt::format("scan press used at x {:.1f} (tick {}, hold {}): the search had {} | the same test again: {}",
                                         rtAt(t).x, t, h, it != m_rs.tried.end() ? it->second : std::string("not tried it"),
                                         r2.end == SimEnd::Survived ? "lives again" : "DIES now (" + deathWhat(r2) + ") - not repeatable"), false);
                    return true;
                }
            }
        }
        return false;
    }
    // (2.3 debug) what's in the level around a stuck spot (the mod's list of
    // blocks / spikes / slopes): kind, id, box, rotation, and GD's switches -
    // d = off (outside GD's area), d2 = its second off flag, g = group off,
    // h = hidden, n = no touch
    std::string geometryAt(float x, float y) const {
        std::string out = fmt::format("level around x {:.0f} y {:.0f}:", x, y);
        int n = 0;
        auto it = std::lower_bound(m_rayObjs.begin(), m_rayObjs.end(), x - 150.f - m_rayMaxW,
                                   [](RayObj const& a, float v) { return a.x0 < v; });
        for (; it != m_rayObjs.end() && it->x0 <= x + 90.f; ++it) {
            GameObject* o = it->obj;
            if (!o || it->x1 < x - 150.f) continue;
            CCRect const& r = o->getObjectRect();
            if (r.getMaxY() < y - 160.f || r.getMinY() > y + 160.f) continue;
            if (++n > 30) { out += " ..."; break; }
            std::string fl;
            if (o->m_isDisabled) fl += "d";
            if (o->m_isDisabled2) fl += "D";
            if (o->m_isGroupDisabled) fl += "g";
            if (!o->isVisible()) fl += "h";
            if (o->m_isNoTouch) fl += "n";
            out += fmt::format(" [{} {} ({:.0f},{:.0f} {:.0f}x{:.0f}){}{}]", objectName(o->m_objectType), o->m_objectID,
                               r.getMinX(), r.getMinY(), r.size.width, r.size.height,
                               std::fabs(o->getRotation()) > 0.5f ? fmt::format(" r{:.0f}", o->getRotation()) : std::string(),
                               fl.empty() ? std::string() : " " + fl);
        }
        if (n == 0) out += " nothing in the mod's list";
        return out;
    }
    void rtSlice(PlayLayer* pl, int budgetUs) {
        if (!m_rtOn || m_rtDone || m_rtStuck) return;
        // (2.3) a search of every way through is running: carry it on
        if (m_rtAOn) {
            rtAstarStep(pl, budgetUs);
            return;
        }
        auto const t0 = Clock::now();
        float const sc = rtS();
        int const chunk = std::max(4, scaleTicks(RT_CHUNK_BASE, sc));
        int const back = std::max(8, scaleTicks(RT_BACK_BASE, sc));
        int const margin = std::max(4, scaleTicks(RT_MARGIN_BASE, sc));
        int const ahead = scaleTicks(RT_AHEAD_BASE, sc);
        std::vector<SpTick> trail;
        auto used = [&]() { return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count(); };
        while (used() < budgetUs) {
            if (!m_rs.on) {
                int const T = rtEnd();
                if (T - m_rtNow > ahead) return; // far enough ahead for now
                SpTick const& cur = rtAt(T);
                if (cur.x >= m_rtEndX - 1.f) {
                    m_rtDone = true;
                    if (!m_rtDoneLogged) {
                        m_rtDoneLogged = true;
                        autoNote(fmt::format("route found to the end of the level ({} inputs, {} tests)", m_rtEvents.size(), m_rtRuns), false);
                    }
                    return;
                }
                bool const held = rtHeldAt(T);
                SimResult const r = rtRun(pl, T, held, 0, chunk, trail, true);
                if (r.end == SimEnd::Survived) {
                    rtAppend(T, held, held, trail, m_simOrbObj, m_simOrbStep);
                    // (2.3) the take-backs count until the route gets past
                    // the spot it was fighting (it used to reset on every
                    // stretch it lived through, then give up after 8)
                    if (rtAt(rtEnd()).x > m_rtFrontX + RT_PROGRESS_PX) {
                        m_rtFrontX = rtAt(rtEnd()).x;
                        m_rtUndo = 0;
                        // (2.3) inputs taken back behind the front: free again
                        std::erase_if(m_rtUndoBans, [&](UndoBan const& b) { return b.x < m_rtFrontX - 2000.f; });
                    }
                    continue;
                }
                if (r.end != SimEnd::Died) {
                    // something the hidden player can't play (a teleport, ...): the
                    // route ends here for now
                    rtAppend(T, held, held, trail, nullptr);
                    m_rtStuck = true;
                    m_rtStuckAt = m_tick;
                    m_rtStuckX = rtAt(rtEnd()).x;
                    m_rtStuckWhy = r.end == SimEnd::Truncated ? "can't see past " + describeTrunc(r.trunc) : "out of time";
                    return;
                }
                // it dies: keep what it lived through, then look for an input
                // before the death (latest first)
                m_rs = RtSearch{};
                m_rs.on = true;
                m_rs.death = T + static_cast<int>(trail.size()) + 1;
                m_rs.why = deathWhat(r);
                rtAppend(T, held, held, trail, nullptr);
                m_rs.step = 1; // every tick: a press window can be one tick wide
                // (2.3) autoplay died here before: the input that saves it is
                // earlier than the usual stretch - look three times as far back
                int const backN = autoDeathsNear(rtAt(rtEnd()).x) > 0 ? back * 3 : back;
                m_rs.lo = std::max({m_rtLastEvent + 1, m_rtBase, m_rs.death - backN});
                m_rs.jj = std::min({m_rs.death - 1, rtEnd(), m_rtHi});
                m_rs.hi = m_rs.jj;
            }
            // the search, a few tries per slice
            int const lo = std::max(m_rs.lo, m_rtNow + 1); // never in the past (you moved on meanwhile)
            int const window = std::max(2, scaleTicks(SP_WINDOW_BASE, sc));
            auto const& lens = rtHoldLens(sc);
            for (;;) {
                if (m_rs.jj < lo) {
                    // phase 0 found nothing safe: short clicks next
                    if (m_rs.phase == 0 && m_rs.safe.empty()) {
                        m_rs.phase = 1;
                        m_rs.ki = 0;
                        m_rs.jj = std::min(m_rs.hi, rtEnd());
                        if (m_rs.jj >= lo) continue;
                    }
                    break;
                }
                if (used() >= budgetUs) return; // carry on next tick
                // (2.3 crash fix) never past the route's end (you can get there
                // while it searches: "lo" follows you)
                if (m_rs.jj > rtEnd()) m_rs.jj = rtEnd();
                if (m_rs.jj < lo || m_rs.jj < m_rtBase) {
                    if (m_rs.phase == 0 && m_rs.safe.empty() && lo <= rtEnd()) { m_rs.jj = lo - 1; continue; }
                    break;
                }
                int const jj = m_rs.jj;
                bool const heldBefore = jj > m_rtBase ? rtHeldAt(jj - 1) : rtHeldAt(jj);
                bool const tapM = spTapMode(rtAt(jj).modeCode);
                int hold = 0;
                if (m_rs.phase == 0) {
                    hold = tapM && !heldBefore ? 1 : 0;
                    m_rs.jj -= m_rs.step;
                } else {
                    // tap modes: held for a while - it hits an orb on the way
                    // (GD fires an orb you hold into) or jumps again on landing
                    if (tapM && heldBefore) { m_rs.jj -= m_rs.step; continue; }
                    hold = lens[static_cast<size_t>(m_rs.ki)];
                    if (tapM && hold <= 1) { // the tap was phase 0's
                        if (++m_rs.ki >= static_cast<int>(lens.size())) { m_rs.ki = 0; m_rs.jj -= m_rs.step; }
                        continue;
                    }
                    if (++m_rs.ki >= static_cast<int>(lens.size())) { m_rs.ki = 0; m_rs.jj -= m_rs.step; }
                    if (hold >= m_rs.death - jj + margin) { m_rs.ki = 0; m_rs.jj -= m_rs.step; continue; } // = phase 0's
                }
                bool const push = (tapM && !heldBefore) ? true : !heldBefore;
                if (!m_rtUndoBans.empty() && rtUndoBanned(rtAt(jj).x, push)) {
                    // (2.3) taken back at this spot already (it led nowhere)
                    if (m_rs.phase == 0 && m_rs.tried.size() < 400) m_rs.tried[jj] = "taken back here before - led nowhere";
                    continue;
                }
                if (!m_apBans.empty() && apBanned(rtAt(jj).x, push)) {
                    // (2.3) Retry Deaths: this input died for real here - not again
                    if (m_rs.phase == 0 && m_rs.tried.size() < 400) m_rs.tried[jj] = "died for real before - not used again";
                    continue;
                }
                SimResult const rj = rtRun(pl, jj, push, hold, m_rs.death - jj + margin, trail, false);
                bool ok = rj.end == SimEnd::Survived;
                if (rj.end == SimEnd::Died && (m_rs.killAt.size() < 8 || m_rs.killAt.count(killSpot(rj))))
                    m_rs.killAt[killSpot(rj)]++;
                if (rj.end == SimEnd::Died && m_rs.cands.size() < 4000)
                    m_rs.cands.push_back(RtSearch::Cand{jj, hold, push, rj.killPen, jj + rj.deathStep});
                if (rj.end == SimEnd::Died && jj + rj.deathStep > m_rs.farReach) {
                    m_rs.farReach = jj + rj.deathStep;
                    m_rs.farJ = jj;
                    m_rs.farK = hold;
                    m_rs.farPush = push;
                    m_rs.farTrail = trail;
                }
                bool useless = false;
                if (ok && tapM && push && !rtInputChanges(jj, trail)) {
                    // (2.3) lives, but the tap does nothing you'd see: kept aside
                    ok = false;
                    useless = true;
                    m_rs.useless++;
                    m_rtUseless++;
                    if (m_rs.fbJ < 0) {
                        m_rs.fbJ = jj;
                        m_rs.fbK = hold;
                        m_rs.fbPush = push;
                        m_rs.fbTrail = trail;
                        m_rs.fbOrb = m_simOrbObj;
                        m_rs.fbOrbStep = m_simOrbStep;
                    }
                }
                if (m_rs.phase == 0 && m_rs.tried.size() < 400)
                    m_rs.tried[jj] = fmt::format("{} {} hold {}: {}", push ? "press" : "release", heldBefore ? "(was held)" : "", hold,
                                                 useless ? std::string("lived, changes nothing") : ok ? std::string("lived")
                                                 : rj.end == SimEnd::Died ? deathWhat(rj) : std::string(simEndName(rj.end)));
                if (!ok && !useless) {
                    m_rs.tries++;
                    std::string const why = rj.end == SimEnd::Died ? deathWhat(rj) : std::string(simEndName(rj.end));
                    if (m_rs.reasons.count(why) || m_rs.reasons.size() < 6) m_rs.reasons[why]++;
                }
                float const clr = ok ? m_scanClr : -1.f;
                if (ok && clr > m_rs.bestClr) {
                    m_rs.bestClr = clr;
                    m_rs.bestJ = jj;
                    m_rs.bestK = hold;
                    m_rs.bestPush = push;
                    m_rs.bestTrail = trail;
                    m_rs.bestOrb = m_simOrbObj;
                    m_rs.bestOrbStep = m_simOrbStep;
                }
                if (ok && clr >= SP_SAFE_CLR) m_rs.safe.push_back({jj, hold, clr});
                // a window of safe timings back from the latest one, then the
                // roomiest (rtPickSafe) - late when only late lives, else the middle
                if (!m_rs.safe.empty() && m_rs.safe.front().t - jj >= window) break;
            }
            m_rs.on = false;
            // (2.3) only inputs still ahead of you (the search can take a few ticks)
            std::erase_if(m_rs.safe, [&](RtSearch::Safe const& e) { return e.t <= m_rtNow || e.t > rtEnd(); });
            if (m_rs.bestJ >= 0 && (m_rs.bestJ <= m_rtNow || m_rs.bestJ > rtEnd())) m_rs.bestJ = -1;
            if (int const pi = rtPickSafe(m_rs.safe); pi >= 0) {
                auto const e = m_rs.safe[static_cast<size_t>(pi)];
                if (e.t != m_rs.bestJ || e.k != m_rs.bestK) {
                    bool const heldBefore = e.t > m_rtBase ? rtHeldAt(e.t - 1) : rtHeldAt(e.t);
                    bool const push = (spTapMode(rtAt(e.t).modeCode) && !heldBefore) ? true : !heldBefore;
                    if (rtRun(pl, e.t, push, e.k, m_rs.death - e.t + margin, trail, false).end == SimEnd::Survived) {
                        m_rs.bestJ = e.t;
                        m_rs.bestK = e.k;
                        m_rs.bestPush = push;
                        m_rs.bestTrail = trail;
                        m_rs.bestOrb = m_simOrbObj;
                        m_rs.bestOrbStep = m_simOrbStep;
                    }
                }
            }
            if (m_rs.bestJ >= 0) {
                int const k = m_rs.bestK;
                rtAppendHold(m_rs.bestJ, m_rs.bestPush, k, m_rs.bestTrail, m_rs.bestOrb, m_rs.bestOrbStep);
                m_rtEvents[m_rs.bestJ] = m_rs.bestPush;
                // a short click's other half is an input too (only taps' isn't:
                // the next one may come right after)
                if (k > 1) m_rtEvents[m_rs.bestJ + k] = !m_rs.bestPush;
                m_rtLastEvent = m_rs.bestJ + (k > 1 ? k : 0);
                m_rtHi = 1 << 30;
                m_rs = RtSearch{};
                continue;
            }
            // (2.3) the level scan pressed somewhere here: if that press lives
            // from the route's own state, use it (the search can miss one)
            {
                int const lo3 = std::max(m_rs.lo, m_rtNow + 1), hi3 = std::min(m_rs.death - 1, rtEnd());
                int at = -1, hold = 0;
                if (rtScanRescue(pl, lo3, hi3, margin, at, hold, trail)) {
                    bool const push = true;
                    rtAppendHold(at, push, hold, trail, m_simOrbObj, m_simOrbStep);
                    m_rtEvents[at] = push;
                    if (hold > 1) m_rtEvents[at + hold] = !push;
                    m_rtLastEvent = at + (hold > 1 ? hold : 0);
                    m_rtHi = 1 << 30;
                    m_rtScanRescues++;
                    m_rs = RtSearch{};
                    continue;
                }
            }
            // (2.3) Nothing lives with ONE input - but some got past the spot
            // and died later (on the next obstacle: they needed a second
            // input). A try only counted if it lived on with no other input,
            // so a jump that clears these spikes and then needs another jump
            // for what comes next was thrown away, and the search ended up
            // with the last-moment presses ("stuck", every try "died"). Now
            // the one that gets furthest past the spot is kept and the route
            // plans the next input from there (taken back like any other
            // input if that leads nowhere).
            if (m_rs.farJ > m_rtNow && m_rs.farJ <= rtEnd() && m_rs.farReach >= m_rs.death + std::max(3, scaleTicks(12, sc))) {
                int const k = m_rs.farK;
                float const fx = rtAt(m_rs.farJ).x;
                rtAppendHold(m_rs.farJ, m_rs.farPush, k, m_rs.farTrail, nullptr, 0);
                m_rtEvents[m_rs.farJ] = m_rs.farPush;
                if (k > 1) m_rtEvents[m_rs.farJ + k] = !m_rs.farPush;
                m_rtLastEvent = m_rs.farJ + (k > 1 ? k : 0);
                m_rtHi = 1 << 30;
                m_rtProgress++;
                if (m_rtProgressLogs < 20) {
                    m_rtProgressLogs++;
                    autoNote(fmt::format("no single input lives through x {:.0f}, but the {} at x {:.1f}{} gets past it (to x {:.0f}) - "
                                         "planning the next input from there", m_rs.death - 1 <= rtEnd() ? rtAt(std::min(m_rs.death - 1, rtEnd())).x : fx,
                                         m_rs.farPush ? "press" : "release", fx, k > 0 ? fmt::format(" held {}t", k) : std::string(),
                                         rtAt(rtEnd()).x), false);
                }
                m_rs = RtSearch{};
                continue;
            }
            // (2.3) nothing in reach saves it, but a portal was taken a while
            // before (gamemode / gravity / size / speed): which portal you go
            // into decides what comes after - look for inputs before it too
            // (another portal, or into this one differently)
            if (!m_rs.widened) {
                int const floor = std::max({m_rtLastEvent + 1, m_rtBase, m_rtNow + 1, m_rs.death - 4 * back});
                int portal = -1;
                for (int t = std::min(m_rs.lo, rtEnd()) - 1; t >= floor; t--) {
                    SpTick const& a = rtAt(t);
                    SpTick const& b = rtAt(t + 1);
                    if (a.modeCode != b.modeCode || a.s1.m_isUpsideDown != b.s1.m_isUpsideDown ||
                        std::fabs(a.s1.m_vehicleSize - b.s1.m_vehicleSize) > 0.01f ||
                        std::fabs(a.s1.m_playerSpeed - b.s1.m_playerSpeed) > 0.01f) {
                        portal = t;
                        break;
                    }
                }
                if (portal >= 0) {
                    m_rs.widened = true;
                    m_rs.on = true;
                    m_rs.phase = 0;
                    m_rs.ki = 0;
                    m_rs.safe.clear();
                    m_rs.hi = m_rs.lo - 1;
                    m_rs.jj = m_rs.lo - 1;
                    m_rs.lo = std::max(floor, portal - back);
                    m_rtPortalWidens++;
                    continue;
                }
            }
            // nothing in reach saves it: take the last input back and look for
            // one before it
            // (2.3) ... and look AFTER it too: the search used to be capped
            // just before the input it took back, so it could only ever try
            // earlier and earlier ("always looking further back") - when the
            // fix was the same jump a few ticks later (off a platform's edge:
            // later = further). Now the whole stretch from the input before
            // it to the death is searched again, without the one taken back.
            // (after a few inputs taken back without luck: every way through, A*)
            if (m_rtUndo >= 4 && rtAstarStart(pl)) return;
            if (m_rtLastEvent > m_rtNow + 1 && m_rtUndo < RT_UNDO_MAX) {
                int const e = m_rtLastEvent;
                m_rtUndo++;
                auto const ev = m_rtEvents.find(e);
                if (e >= m_rtBase && e <= rtEnd() && ev != m_rtEvents.end() && m_rtUndoBans.size() < 256)
                    m_rtUndoBans.push_back(UndoBan{rtAt(e).x, ev->second});
                rtTruncate(e);
                m_rtHi = 1 << 30;
                continue;
            }
            // (2.3) nothing in reach gets past - try EVERY way through from
            // a few seconds back (A*: press or not at each step, both kept
            // and the best ones followed, the high road and the low road),
            // not just one input changed near the death
            if (rtAstarStart(pl)) return;
            // (2.3) last resort: a tap that lived but changed nothing visible
            if (m_rs.fbJ > m_rtNow && m_rs.fbJ <= rtEnd()) {
                int const k = m_rs.fbK;
                rtAppendHold(m_rs.fbJ, m_rs.fbPush, k, m_rs.fbTrail, m_rs.fbOrb, m_rs.fbOrbStep);
                m_rtEvents[m_rs.fbJ] = m_rs.fbPush;
                if (k > 1) m_rtEvents[m_rs.fbJ + k] = !m_rs.fbPush;
                m_rtLastEvent = m_rs.fbJ + (k > 1 ? k : 0);
                m_rtHi = 1 << 30;
                m_rtUselessUsed++;
                m_rs = RtSearch{};
                continue;
            }
            // (2.3) Retry Deaths: it died here before and nothing lives in its
            // predictions - they can be wrong. Every input it tested here
            // goes on a list, nearest miss first (how deep into the spike /
            // block it ended up), and each retry plays the next one for real:
            // earlier / later, press / release, short / long - never the same
            // twice. One that gets past is kept for the next attempts.
            // (then the route plans on from it - past where it dies in the
            // predictions, the real game decides)
            if (cfg().apRetry && !deathsOff() && autoDeathsNear(rtAt(rtEnd()).x) > 0 && apTrialCommit(pl, margin)) {
                m_rs = RtSearch{};
                m_rtHi = 1 << 30;
                continue;
            }
            m_rtStuck = true;
            m_rtStuckX = rtAt(rtEnd()).x;
            m_rtStuckWhy = m_rs.why;
            m_rtStuckAt = m_tick;
            if (m_rtStuckLogs < 40) {
                m_rtStuckLogs++;
                std::string died;
                for (auto const& [why, n] : m_rs.reasons) died += fmt::format("{}{} x{}", died.empty() ? "" : ", ", why, n);
                int const lo2 = std::max(m_rs.lo, m_rtNow + 1), hi2 = std::min(m_rs.death - 1, rtEnd());
                std::string at;
                for (auto const& [where, n] : m_rs.killAt) at += fmt::format("{}{} x{}", at.empty() ? "" : ", ", where, n);
                autoNote(fmt::format("route stuck at x {:.0f}: {} - none of {} inputs (x {:.0f} .. {:.0f}) saves it; they died: {} | "
                                     "killed by: {} | furthest got to tick +{} | {}",
                                     m_rtStuckX, m_rs.why, m_rs.tries, lo2 <= hi2 ? rtAt(lo2).x : 0.f,
                                     lo2 <= hi2 ? rtAt(hi2).x : 0.f, died.empty() ? "-" : died, at.empty() ? "-" : at,
                                     m_rs.farReach - m_rs.death, rtScanCompare(pl, lo2, hi2, margin)), true);
                if (m_rtStuckLogs <= 12) autoNote(geometryAt(m_rtStuckX, rtAt(rtEnd()).y), false);
            }
            return;
        }
    }

    // (2.3 debug) A drift taken apart: the first route tick where you and the
    // route's state part (more than 0.01 px), every field that differs there
    // and the tick before, and the button both had - what the hidden player
    // gets wrong, by name.
    struct RealHist { int t = -1; long long start = -1; PlayerSnap s; bool held = false; };
    std::array<RealHist, 512> m_rtReal;
    std::string rtDissect() {
        int const from = std::max(m_rtBase + 1, m_rtNow - 500);
        for (int k = from; k <= std::min(m_rtNow, rtEnd()); k++) {
            RealHist const& h = m_rtReal[static_cast<size_t>(k) & 511];
            if (h.t != k || h.start != m_rtStarts) continue;
            SpTick const& q = rtAt(k);
            if (std::hypot(h.s.pos.x - q.s1.pos.x, h.s.pos.y - q.s1.pos.y) <= 0.01f) continue;
            RealHist const& hp = m_rtReal[static_cast<size_t>(k - 1) & 511];
            bool const prevOk = hp.t == k - 1 && hp.start == m_rtStarts;
            return fmt::format("drift detail: first apart at route tick {} (x {:.1f}); you/route there: {} | the tick before: {} | "
                               "button you/route the tick before: {}/{}", k, h.s.pos.x, snapDiff(h.s, q.s1),
                               prevOk ? snapDiff(hp.s, rtAt(k - 1).s1) : std::string("not recorded"),
                               prevOk ? (hp.held ? "held" : "up") : "?", rtHeldAt(k - 1) ? "held" : "up");
        }
        return "drift detail: no tick recorded apart (it happened before this route's history)";
    }
    // (2.3 debug) You stand on something the route fell past: replay the
    // route's last few ticks before it, tick by tick - how many objects GD
    // handed the hidden player, how many the mod added, whether GD's object
    // check ran, whether the floor you stand on was among them, and what GD's
    // collision made of it.
    struct DiagStep { float x = 0.f, y = 0.f; double vy = 0.0; bool ground = false, cco = false; int passed = 0, added = 0;
                      double cbmy = 0.0; bool gPassed = false, gAdded = false; };
    std::vector<DiagStep>* m_diag = nullptr;
    GameObject* m_diagObj = nullptr;
    std::string rtLandingDiag(PlayLayer* pl, PlayerObject* p) {
        int const k = m_rtNow;
        int const t0 = std::max(m_rtBase, k - 6);
        if (k - t0 < 1) return "landing diag: no ticks to replay";
        GameObject* g = p->m_lastGroundObject;
        std::vector<DiagStep> steps;
        std::vector<SpTick> trail;
        m_diag = &steps;
        m_diagObj = g;
        rtRun(pl, t0, rtHeldAt(t0), 0, k - t0, trail, true);
        m_diag = nullptr;
        m_diagObj = nullptr;
        std::string out = fmt::format("landing diag: you stand on {}; the route's last {} ticks replayed:",
                                      g ? fmt::format("{} id {} top {:.1f}{}{}", objectName(g->m_objectType), g->m_objectID,
                                                      g->getObjectRect().getMaxY(), g->m_isDisabled ? " d" : "", g->m_isDisabled2 ? " D" : "")
                                        : std::string("GD's ground"), k - t0);
        for (auto const& d : steps)
            out += fmt::format(" | y {:.2f} vy {:.2f} {} GD gave {} mod added {} check {} floor {} bottomMaxY {:.1f}", d.y, d.vy,
                               d.ground ? "ground" : "air", d.passed, d.added, d.cco ? "ran" : "SKIPPED",
                               d.gPassed ? "from GD" : d.gAdded ? "from the mod" : "NOT THERE", d.cbmy);
        return out;
    }
    void autoStep(PlayLayer* pl, PlayerObject* p) {
        if (p->m_isDead || p->m_isPlatformer) return;
        // Pieces of a step (an input mod splits some steps): everything is
        // decided at the start of a step; the route moves on once the step
        // is whole - it counts whole steps.
        float const nominalDt = getValidDt() * (m_warp > 0.f ? m_warp : 1.f);
        float const part = nominalDt > 0.f ? std::clamp(m_stepDtNow / nominalDt, 0.f, 1.f) : 1.f;
        if (m_rtPhase > 0.001f) {
            m_rtPhase += part;
            if (m_rtPhase >= 0.98f) {
                m_rtPhase = 0.f; // that piece finished the step
                m_rtNow++;
            }
            return;
        }
        m_gameHolding = holdingJump(p); // what the player really has (a respawn lets go)
        if (!m_autoUsed) {
            m_autoUsed = true;
            autoNote("is playing this level - nothing here is saved as progress until you leave it", false);
        }
        float const px = p->getPositionX(), py = p->getPositionY();
        // (2.3) the run's inputs (autoplay's and yours), from where it started;
        // what got it further than ever is remembered
        if (!m_apRunValid) {
            m_apRunValid = true;
            m_apRunStartX = px;
        }
        memTick(px);
        // (2.3) Remember: a stretch it got through before - the same inputs
        // again, no planning (it plans again where the memory ends)
        if (memPlay(pl, px)) {
            if (part < 0.98f) m_rtPhase = part;
            else m_rtNow++;
            return;
        }
        if (cfg().apOverride) clicksTick(pl, p); // (2.3) your good clicks: kept, and made again where they were
        // just respawned / moved by the game (start position, teleport): start
        // the route again from here - not a prediction miss
        bool const jumped = std::fabs(px - m_rtLastX) > 60.f || std::fabs(py - m_rtLastY) > 60.f;
        m_rtLastX = px;
        m_rtLastY = py;
        // stuck: try again every 30 ticks from where you are (a level's start
        // can look deadly for a moment - triggers that move / hide things
        // haven't run yet)
        bool retry = m_rtOn && m_rtStuck && (m_tick - m_rtStuckAt) >= 30;
        // (2.3) stuck further ahead: keep the route and fight that spot again
        // (the level there may have changed - see rtVerify); only stuck right
        // in front of you starts over from where you are
        if (retry && rtEnd() - m_rtNow > scaleTicks(120, rtS())) {
            retry = false;
            m_rtStuck = false;
            m_rtStuckAt = m_tick;
            m_rs = RtSearch{};
            m_rtUndo = 0;
            m_rtHi = 1 << 30;
            m_rtUnstuck++;
        }
        if (m_autoSettle > 0 || jumped || !m_rtOn || retry) {
            if (m_autoSettle > 0) m_autoSettle--;
            rtStart(pl, p);
            m_ap = AutoPlan{};
        }
        // (2.3 debug) your real state at the start of every route tick, to
        // take a drift apart field by field (below)
        {
            RealHist& h = m_rtReal[static_cast<size_t>(m_rtNow) & 511];
            h.t = m_rtNow;
            h.start = m_rtStarts;
            h.s = snapFrom(p);
        }
        // 1. where the route said you'd be now
        if (m_rtNow >= m_rtBase && m_rtNow <= rtEnd() && m_rtNow > 0) {
            SpTick const& q = rtAt(m_rtNow);
            float const dx = px - q.x, dy = py - q.y;
            float const off = std::hypot(dx, dy);
            if (off > AUTO_DRIFT_PX && !m_rtDriftSeen) {
                m_rtDriftSeen = true; // once per route
                ModeInfo mi;
                mi.add(p);
                AutoStat& st = m_autoStats[mi.name()];
                st.drifts++;
                st.worst = std::max(st.worst, off);
                if (m_autoDriftLogs < 40) {
                    m_autoDriftLogs++;
                    // what you stand on, and whether the route thought it stood
                    std::string ground = "in the air";
                    if (GameObject* g = p->m_isOnGround ? p->m_lastGroundObject : nullptr) {
                        CCRect const& gr = g->getObjectRect();
                        CCRect const& prr = p->getObjectRect();
                        ground = fmt::format("on {} id {} (top {:.2f}, your feet {:.2f})", objectName(g->m_objectType),
                                             g->m_objectID, gr.getMaxY(), prr.getMinY());
                    } else if (p->m_isOnGround) {
                        ground = "on GD's ground";
                    }
                    autoNote(fmt::format("drift [{}]: {}t into the route you're x {:+.2f}, y {:+.2f} off it (at x {:.0f} y {:.0f}, "
                                         "y speed {:.2f} vs the route's {:.2f}; you {}, the route {}){}", mi.name(), m_rtNow, dx, dy,
                                         px, py, p->m_yVelocity, q.vy, ground, q.s1.m_isOnGround ? "on the ground" : "in the air",
                                         off > AUTO_REPLAN_PX ? " - planning again from here" : ""), false);
                    autoNote(rtDissect(), false);
                    if (p->m_isOnGround && !q.s1.m_isOnGround) autoNote(rtLandingDiag(pl, p), false);
                }
            }
            if (off > AUTO_REPLAN_PX) rtStart(pl, p);
        }
        // 2. plan on: a burst when the route is short, else a slice
        int const coverNeed = std::max(8, scaleTicks(AUTO_HORIZON_BASE, rtS()));
        int const aheadNow = rtEnd() - m_rtNow;
        // (2.3) planning time per FRAME, not per tick: when the frame rate
        // dips, a frame holds several ticks and each got its full budget -
        // more time per frame, a lower frame rate, more ticks per frame (FPS
        // drops during autoplay). Now all of a frame's ticks share one
        // budget (more only when the route is about to run out).
        {
            int const cap = aheadNow < coverNeed ? AP_FRAME_BURST_US : AP_FRAME_US;
            int const left = cap - static_cast<int>(m_apFrameSpent);
            if (left > 100) {
                auto const t0 = Clock::now();
                rtVerify(pl, std::min(RT_VERIFY_US, left / 3));
                int const used = static_cast<int>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count());
                int const want = aheadNow < coverNeed ? RT_BURST_US : aheadNow > 6 * coverNeed ? RT_FAR_US : RT_SLICE_US;
                if (left - used > 50) rtSlice(pl, std::min(want, left - used));
                m_apFrameSpent += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
            } else {
                m_apFrameSkips++;
            }
        }
        // 4. this tick's button: the route's - or, past its end, a quick plan
        bool want = m_gameHolding;
        if (m_rtNow <= rtEnd() && !(m_rtStuck && m_rtNow >= rtEnd())) {
            int const at = std::min(rtEnd(), m_rtNow + m_fireLag); // sent early by the measured delay
            want = rtHeldAt(at);
            m_ap = AutoPlan{};
        } else {
            if (!m_ap.valid || m_ap.k >= autoHorizon() / 2 || static_cast<size_t>(m_ap.k) >= m_ap.held.size()) autoPlan(pl);
            size_t const at = static_cast<size_t>(m_ap.k + m_fireLag); // sent early by the measured delay
            if (at < m_ap.held.size()) want = m_ap.held[at] != 0;
            m_ap.k++;
        }
        // (2.3) Your Clicks: your press (or a kept one) is held as long as it was
        if (m_userHeld || m_tick < m_forceUntil) want = true;
        if (want != m_gameHolding) {
            fireButton(pl, want, true);
            if (m_apRunValid) m_apRun.push_back(ApInput{px, want}); // (2.3) the run's inputs (memory)
            m_autoLastInTick = m_tick;
            m_autoLastInPush = want;
            m_autoLastInX = px;
        }
        m_rtReal[static_cast<size_t>(m_rtNow) & 511].held = want; // (debug) the button this tick
        if (part < 0.98f) m_rtPhase = part; // only a piece: the rest of the step comes next
        else m_rtNow++;
        // memory: the route behind you
        while (m_rt.size() > 2 && m_rtBase < m_rtNow - 2) {
            m_rt.pop_front();
            m_rtHeld.pop_front();
            m_rtBase++;
        }
    }

    // ---- the learning run (2.3) ------------------------------------------------
    //  Autoplay's first run of a level (this session) plays it with deaths
    //  off, start to end: the moving-object tracker records where every block
    //  and spike is when you're at each x. Then the level restarts and
    //  autoplay plays for real, planning against that timeline - things that
    //  move in later are in the plan before they move. Real attempts keep
    //  adding to it. Setting: Autoplay Learning Run.
    int m_learn = 0;          // 0 = not yet, 1 = running, 2 = done (this level, this session)
    bool m_learnReset = false; // restart the level next frame (the run is over)
    float m_learnBestX = 0.f, m_learnFromX = 0.f;
    int m_learnStill = 0;
    long long m_learnHits = 0;
    std::string m_learnWhy;
    float learnPct(PlayLayer* pl) const {
        if (!pl || !pl->m_player1) return 0.f;
        float const len = pl->m_levelLength - m_learnFromX;
        return len > 1.f ? std::clamp((pl->m_player1->getPositionX() - m_learnFromX) / len * 100.f, 0.f, 100.f) : 0.f;
    }
    // (2.3) the label above you while autoplay plays
    std::string autoStatusText(PlayLayer* pl) {
        std::string mv;
        if (!m_mvNear.empty()) mv = fmt::format(" - {} moving objects tracked", m_mvNear.size());
        if (learning()) return fmt::format("autoplay: learning run {:.0f}% (deaths off){}", learnPct(pl), mv);
        if (m_memPlaying) return fmt::format("autoplay: playing what it remembers (got to x {:.0f} before)", m_memEnd);
        if (m_userClickTick >= 0 && m_tick - m_userClickTick < 120)
            return fmt::format("autoplay: your click at x {:.0f} - planning around it", m_userClickX);
        if (cfg().apNoclip) mv += " - NOCLIP";
        if (apPracticeActive(pl)) {
            mv += fmt::format(" - practice, {} checkpoints", pl && pl->m_checkpointArray ? pl->m_checkpointArray->count() : 0u);
            if (m_apRetries > 0) mv += fmt::format(", retry {} at x {:.0f}", m_apRetries, m_apRetryX);
        }
        if (!m_rtOn || m_rt.empty()) return "autoplay: planning...";
        if (m_rtDone) return "autoplay: route found to the end" + mv;
        if (m_rtStuck) {
            int const n = autoDeathsNear(m_rtStuckX);
            return fmt::format("autoplay: stuck at x {:.0f}{}", m_rtStuckX,
                               n > 0 ? fmt::format(" (died here {}x - looks further back)", n) : std::string(" - trying again"));
        }
        float const aheadS = static_cast<float>(std::max(0, rtEnd() - m_rtNow)) * simDt() / 60.f;
        if (m_rs.on) return fmt::format("autoplay: route {:.1f} s ahead, searching a way past x {:.0f}{}", aheadS,
                                        m_rs.death <= rtEnd() + 1 && m_rs.death - 1 >= m_rtBase ? rtAt(std::min(m_rs.death - 1, rtEnd())).x
                                                                                              : rtAt(rtEnd()).x, mv);
        return fmt::format("autoplay: route {:.1f} s ahead{}", aheadS, mv);
    }
    void learnFinish(std::string const& why) {
        if (m_learn != 1) return;
        m_learn = 2;
        m_learnWhy = why;
        m_learnReset = true;
        size_t timed = 0;
        for (auto const& m : m_mv) if (!m.cur.empty() || !m.tl.empty()) timed++;
        autoNote(fmt::format("learning run done ({}): {} blocks / spikes moved or switched on the way ({} recorded), {} touches "
                             "ignored - restarting, now it plays for real", why, m_mv.size(), timed, m_learnHits), false);
    }
    void learnTick(PlayLayer* pl, PlayerObject* p) {
        if (!pl || !p || p->m_isDead || p->m_isPlatformer || m_learnReset) return;
        if (m_learn == 0) {
            bool const saved = cfg().apReplay && memCovers(p->getPositionX());
            if (!cfg().apLearn || cfg().apNoclip || saved) {
                m_learn = 2; // (noclip records the whole level anyway; a remembered stretch needs no plan)
                m_learnWhy = !cfg().apLearn ? "Learning Run is off (Tools)" : cfg().apNoclip ? "Noclip is on" : "it remembers this level";
                return;
            }
            m_learn = 1;
            m_learnFromX = m_learnBestX = p->getPositionX();
            m_learnStill = 0;
            m_learnHits = 0;
            m_learnWhy.clear();
            autoNote("learning run: playing the level once with deaths off, to see how its blocks and spikes move - "
                     "then it restarts and plays for real", false);
            return;
        }
        if (m_learn != 1) return;
        if (!cfg().apLearn) { // switched off in the Tools tab: it plays for real from here
            m_learn = 2;
            m_learnWhy = "switched off (Tools)";
            autoNote("learning run switched off - deaths count again from here", false);
            return;
        }
        float const x = p->getPositionX(), y = p->getPositionY();
        float const dir = m_p1Dir < 0.f ? -1.f : 1.f;
        if ((x - m_learnBestX) * dir > 1.f) {
            m_learnBestX = x;
            m_learnStill = 0;
        } else {
            m_learnStill++;
        }
        int const tps = static_cast<int>(60.f / std::max(getValidDt(), 0.01f));
        if (pl->m_levelLength > m_learnFromX + 30.f && x >= pl->m_levelLength - 5.f) learnFinish("reached the end");
        else if (y < m_levelBottom - 600.f || y > m_levelTop + 600.f) learnFinish("left the level at x " + fmt::format("{:.0f}", x));
        else if (m_learnStill > tps * 4) learnFinish("stopped moving at x " + fmt::format("{:.0f}", x));
    }

public:
    // destroyPlayer on a real player during the learning run (or with Noclip
    // on): not a death
    bool learnNoclip(PlayLayer* pl, PlayerObject* p) {
        if (!m_autoOn || m_simulating || !pl || !p || (p != pl->m_player1 && p != pl->m_player2)) return false;
        if (m_learn == 1) {
            m_learnHits++;
            return true;
        }
        if (cfg().apNoclip) {
            m_noclipHits++;
            return true;
        }
        return false;
    }
    long long m_noclipHits = 0;
    bool deathsOff() { return m_autoOn && (m_learn == 1 || cfg().apNoclip); }

    // ---- Retry Deaths + the run's inputs (2.3) --------------------------------
    //  Retry Deaths (Tools): autoplay plays in practice mode; you put the
    //  checkpoints down. A death respawns at your last one and that jump is
    //  played again with another input: the input it died with (the last one
    //  since the checkpoint) is never used there again, and where its
    //  predictions say nothing lives it works through a list of inputs for
    //  real (SpotTrial) until one gets past.
    //  The run's real inputs are kept (back to the checkpoint on every death)
    //  - what Remember (below) is made of.
    struct ApInput { float x = 0.f; bool push = false; };
    std::vector<ApInput> m_apRun;   // this run's real inputs (your x at the tick, button)
    float m_apRunStartX = 0.f;
    bool m_apRunValid = false;      // started at the attempt's start (not part-way)
    std::vector<ApInput> m_apBans;  // inputs that died for real: not used again there
    bool m_apPracticeOurs = false;
    float m_apRespawnX = -1e9f;     // where this attempt started (practice: your checkpoint)
    int m_apRetries = 0, m_apRetryTotal = 0;
    float m_apRetryX = -1e9f;
    // ---- Remember (2.3) --------------------------------------------------------
    //  Everything autoplay (and you) pressed on the way, from where the run
    //  started: whatever got it further than ever before is remembered -
    //  every input it lived at least a second past - and saved for the
    //  level (Aceware's save folder, routes/<level>-autoplay.txt). The next
    //  attempts play those same inputs through that stretch, no planning
    //  (it was stuck re-planning spots it had already got past), and plan
    //  again only where the memory ends. Practice runs stitch together: back
    //  to your checkpoint on a death, then on. If a remembered stretch dies
    //  (the level or the physics didn't play the same), the memory is cut
    //  back to a bit before that spot. Toggle: Remember (Tools).
    std::vector<ApInput> m_mem;     // remembered inputs (by x)
    float m_memStart = 0.f, m_memEnd = -1e9f; // from where, and up to where they got it
    bool m_memPlaying = false, m_memDirty = false, m_memSkip = false;
    size_t m_memAt = 0;
    long long m_memSavedTick = -1;
    int m_memGrows = 0, m_memCuts = 0;
    std::filesystem::path apFile() const {
        if (m_lvlKey.empty()) return {};
        return Mod::get()->getSaveDir() / "routes" / (m_lvlKey + "-autoplay.txt");
    }
    void memLoad() {
        m_mem.clear();
        m_memStart = 0.f;
        m_memEnd = -1e9f;
        m_memPlaying = m_memDirty = m_memSkip = false;
        m_memGrows = m_memCuts = 0;
        auto const f = apFile();
        if (f.empty()) return;
        std::ifstream in(f);
        std::string tag;
        if (!(in >> tag >> m_memStart) || tag != "start") return;
        std::string tag2;
        if (!(in >> tag2 >> m_memEnd) || tag2 != "end") { m_memEnd = -1e9f; return; } // (an older saved run: not a memory)
        ApInput a;
        int pu = 0;
        while (in >> a.x >> pu) {
            a.push = pu != 0;
            m_mem.push_back(a);
        }
        trace(fmt::format("autoplay remembers this level from x {:.0f} to x {:.0f} ({} inputs)", m_memStart, m_memEnd, m_mem.size()));
    }
    void memSave() {
        m_memDirty = false;
        m_memSavedTick = m_tick;
        auto const f = apFile();
        if (f.empty()) return;
        std::error_code ec;
        std::filesystem::create_directories(f.parent_path(), ec);
        std::ofstream out(f, std::ios::trunc);
        if (!out) return;
        out << fmt::format("start {:.4f}\nend {:.4f}\n", m_memStart, m_memEnd);
        for (auto const& a : m_mem) out << fmt::format("{:.4f} {}\n", a.x, a.push ? 1 : 0);
    }
    bool memValid() const { return m_memEnd > -1e8f; }
    bool memCovers(float x) const { return memValid() && x >= m_memStart - 1.f && x < m_memEnd; }
    // every tick: inputs it lived a second past, further than ever = remembered
    void memTick(float px) {
        if (!m_apRunValid || deathsOff()) return; // (deaths off: getting further proves nothing)
        int const tps = static_cast<int>(60.f / std::max(getValidDt(), 0.01f));
        float const proven = px - m_mvStep * static_cast<float>(tps);
        if (proven <= m_memEnd || proven <= m_apRunStartX) return;
        std::vector<ApInput> next;
        float start = m_apRunStartX;
        // this run started inside the memory (a checkpoint, a start pos): the memory before it stays
        if (memValid() && m_apRunStartX >= m_memStart - 1.f && m_apRunStartX <= m_memEnd + 1.f) {
            start = m_memStart;
            for (auto const& a : m_mem) if (a.x < m_apRunStartX - 0.01f) next.push_back(a);
        }
        for (auto const& a : m_apRun) if (a.x <= proven) next.push_back(a);
        m_mem.swap(next);
        m_memStart = start;
        m_memEnd = proven;
        m_memDirty = true;
        m_memGrows++;
        if (m_memSavedTick < 0 || m_tick - m_memSavedTick > tps * 2) memSave();
    }
    // reached the level's end: all of it is remembered
    void memReachedEnd(PlayLayer* pl) {
        if (!m_apRunValid || !pl || !pl->m_player1) return;
        float const px = pl->m_player1->getPositionX() + 1000.f;
        if (memValid() && m_apRunStartX >= m_memStart - 1.f && m_apRunStartX <= m_memEnd + 1.f) {
            std::vector<ApInput> next;
            for (auto const& a : m_mem) if (a.x < m_apRunStartX - 0.01f) next.push_back(a);
            for (auto const& a : m_apRun) next.push_back(a);
            m_mem.swap(next);
        } else {
            m_mem = m_apRun;
            m_memStart = m_apRunStartX;
        }
        m_memEnd = px;
        memSave();
        autoNote(fmt::format("beat the level: all {} inputs are remembered (Remember plays them)", m_mem.size()), false);
    }
    // a remembered stretch: play it (true = this tick's button is done)
    bool memPlay(PlayLayer* pl, float px) {
        bool const on = cfg().apReplay && !m_memSkip && !deathsOff() && memCovers(px) && memCovers(m_apRunStartX);
        if (!on) {
            if (m_memPlaying) {
                m_memPlaying = false;
                m_rtOn = false; // plans again from here (where the memory ends)
            }
            return false;
        }
        float const dir = m_p1Dir < 0.f ? -1.f : 1.f;
        if (!m_memPlaying) {
            m_memPlaying = true;
            m_memAt = 0;
            while (m_memAt < m_mem.size() && (m_mem[m_memAt].x - px) * dir < -0.5f * m_mvStep) m_memAt++; // already behind you
        }
        bool want = m_gameHolding;
        while (m_memAt < m_mem.size() && (m_mem[m_memAt].x - px) * dir <= 0.01f) {
            want = m_mem[m_memAt].push;
            m_memAt++;
        }
        if (want != m_gameHolding) {
            fireButton(pl, want, true);
            m_apRun.push_back(ApInput{px, want});
            m_autoLastInTick = m_tick;
            m_autoLastInPush = want;
            m_autoLastInX = px;
        }
        m_rtOn = false;
        return true;
    }
    // a death while playing the memory: it didn't play the same - cut it back
    void memOnDeath(float dx) {
        if (!m_memPlaying) return;
        m_memPlaying = false;
        int const tps = static_cast<int>(60.f / std::max(getValidDt(), 0.01f));
        float const cut = std::max(m_memStart, dx - 1.5f * m_mvStep * static_cast<float>(tps));
        std::erase_if(m_mem, [&](ApInput const& a) { return a.x > cut; });
        m_memEnd = cut;
        m_memCuts++;
        memSave();
        autoNote(fmt::format("a remembered stretch died at x {:.0f} (it didn't play the same) - remembering up to x {:.0f}, planning "
                             "from there", dx, cut), false);
    }
    bool apBanned(float x, bool push) const {
        float const tol = std::max(0.05f, m_mvStep * 0.5f);
        for (auto const& b : m_apBans) if (b.push == push && std::fabs(b.x - x) < tol) return true;
        return false;
    }
    bool apPracticeActive(PlayLayer* pl) const { return pl && pl->m_isPracticeMode && m_apPracticeOurs; }
    // (2.3) Your Clicks: autoplay keeps playing; a click of yours goes into
    // the game as well (held as long as you hold it) and autoplay plans on
    // around it at once. A click you live a second past is a good one: kept
    // (saved to disk for the level) and made again at that x on every later
    // attempt; one that dies is forgotten.
    // a click: where (your x / y), how long held; kept ones also how far an
    // attempt got with it (proven) and how often it died since
    struct UserClick {
        float x = 0.f; int hold = 0; long long tick = 0; bool released = false, replay = false; float y = 0.f;
        float best = -1e9f;   // pending: the furthest any attempt had got when you made it
        float proven = 0.f;   // kept: how far an attempt got with it
        int fails = 0;        // kept: replayed and died before getting that far
    };
    std::vector<UserClick> m_clicks;   // good ones (by x)
    std::vector<UserClick> m_pending;  // made this attempt, not settled yet
    bool m_userHeld = false;           // you hold the button right now
    long long m_forceUntil = -1;       // a replayed click of yours: held until this tick
    long long m_userClickTick = -1;
    float m_userClickX = 0.f;
    float m_apBestX = -1e9f;           // the furthest autoplay got on this level (this session)
    int m_clicksMade = 0, m_clicksKept = 0, m_clicksDropped = 0, m_clicksReplayed = 0, m_clicksRemoved = 0;
    size_t m_clickAt = 0;              // the next good click ahead this attempt
    static constexpr int CLICK_FAILS_MAX = 3; // a kept click that dies this often before its proven x is forgotten
    std::filesystem::path clicksFile() const {
        if (m_lvlKey.empty()) return {};
        return Mod::get()->getSaveDir() / "routes" / (m_lvlKey + "-clicks.txt");
    }
    void clicksLoad() {
        m_clicks.clear();
        auto const f = clicksFile();
        if (f.empty()) return;
        std::ifstream in(f);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream ls(line);
            UserClick c;
            if (!(ls >> c.x >> c.hold)) continue;
            if (!(ls >> c.y)) c.y = 0.f;            // (older files: no height - drawn at yours)
            if (!(ls >> c.proven)) c.proven = c.x;  // (older files: nothing proven)
            m_clicks.push_back(c);
        }
        std::sort(m_clicks.begin(), m_clicks.end(), [](UserClick const& a, UserClick const& b) { return a.x < b.x; });
    }
    void clicksSave() {
        auto const f = clicksFile();
        if (f.empty()) return;
        std::error_code ec;
        std::filesystem::create_directories(f.parent_path(), ec);
        std::ofstream out(f, std::ios::trunc);
        for (auto const& c : m_clicks) out << fmt::format("{:.4f} {} {:.2f} {:.1f}\n", c.x, c.hold, c.y, c.proven);
    }
    // Remove one of your kept clicks (like removing a practice checkpoint):
    // the last one you passed - or, none behind you, the next one ahead.
    bool removeClick(PlayLayer* pl) {
        if (m_clicks.empty() || !pl || !pl->m_player1) return false;
        float const px = pl->m_player1->getPositionX();
        int pick = -1;
        for (int i = 0; i < static_cast<int>(m_clicks.size()); i++)
            if (m_clicks[static_cast<size_t>(i)].x <= px + 1.f) pick = i;
        if (pick < 0) pick = 0;
        float const x = m_clicks[static_cast<size_t>(pick)].x;
        m_clicks.erase(m_clicks.begin() + pick);
        if (m_clickAt > static_cast<size_t>(pick)) m_clickAt--;
        m_clicksRemoved++;
        clicksSave();
        autoNote(fmt::format("removed your kept click at x {:.1f} ({} left)", x, m_clicks.size()), false);
        return true;
    }
    // every tick (autoplay on): good clicks get kept, kept ones get made again
    void clicksTick(PlayLayer* pl, PlayerObject* p) {
        int const tps = static_cast<int>(60.f / std::max(getValidDt(), 0.01f));
        float const px = p->getPositionX();
        float const dir = m_p1Dir < 0.f ? -1.f : 1.f;
        float const perSec = m_mvStep * static_cast<float>(tps);
        // A click of yours is a good one only if it gets you somewhere new:
        // past the furthest any attempt had got when you made it (or, if
        // you made it beyond that, a second past it) - not just alive a
        // second later. Clicks that don't lead anywhere aren't kept (or drawn).
        for (auto it = m_pending.begin(); it != m_pending.end();) {
            if (!it->released) { ++it; continue; }
            float const need = std::max(it->best, it->x + perSec * dir) + 30.f * dir;
            if ((px - need) * dir >= 0.f) {
                if (!it->replay) {
                    // replaces kept clicks for the same jump (within ~1.5 blocks)
                    std::erase_if(m_clicks, [&](UserClick const& c) { return std::fabs(c.x - it->x) < 45.f; });
                    UserClick k = *it;
                    k.proven = px;
                    k.fails = 0;
                    m_clicks.push_back(k);
                    std::sort(m_clicks.begin(), m_clicks.end(), [](UserClick const& a, UserClick const& b) { return a.x < b.x; });
                    m_clickAt = 0;
                    m_clicksKept++;
                    clicksSave();
                    autoNote(fmt::format("your click at x {:.1f} (held {}t) got you further (to x {:.0f}) - kept: autoplay makes it "
                                         "there every attempt", it->x, it->hold, px), false);
                } else {
                    // a kept one that got as far again: proven further
                    for (auto& c : m_clicks) {
                        if (std::fabs(c.x - it->x) < 0.5f) {
                            c.proven = std::max(c.proven, px);
                            c.fails = 0;
                        }
                    }
                }
                it = m_pending.erase(it);
            } else {
                ++it;
            }
        }
        // a kept click here: made again (as long as you held it), and the plan goes on around it
        while (m_clickAt < m_clicks.size() && (m_clicks[m_clickAt].x - px) * dir < -m_mvStep) m_clickAt++;
        if (m_clickAt < m_clicks.size() && (m_clicks[m_clickAt].x - px) * dir <= 0.01f && !m_userHeld) {
            UserClick const c = m_clicks[m_clickAt++];
            fireButton(pl, true, true);
            if (m_apRunValid) m_apRun.push_back(ApInput{px, true});
            m_forceUntil = m_tick + std::max(1, c.hold);
            UserClick pc = c;
            pc.tick = m_tick;
            pc.released = true;
            pc.replay = true;
            pc.best = c.proven; // settled when it gets as far as it did before
            m_pending.push_back(pc);
            m_autoLastInTick = m_tick;
            m_autoLastInPush = true;
            m_autoLastInX = px;
            m_clicksReplayed++;
            m_rtOn = false; // the route plans on from here, with it
        }
    }
    // a death: your new clicks since the last settle weren't good ones (not
    // kept); a kept one that died before getting as far as it did before
    // counts a fail - forgotten after CLICK_FAILS_MAX
    void clicksOnDeath(float dx) {
        for (auto const& pc : m_pending) {
            if (!pc.replay) continue;
            for (auto it = m_clicks.begin(); it != m_clicks.end(); ++it) {
                if (std::fabs(it->x - pc.x) >= 0.5f) continue;
                if (dx < it->proven - 1.f && ++it->fails >= CLICK_FAILS_MAX) {
                    autoNote(fmt::format("your kept click at x {:.1f} died {} times before x {:.0f} - forgotten", it->x, it->fails,
                                         it->proven), false);
                    m_clicks.erase(it);
                    m_clicksDropped++;
                }
                clicksSave();
                break;
            }
        }
        m_pending.clear();
        m_userHeld = false;
        m_forceUntil = -1;
    }
public:
    // your click while autoplay plays: true = it goes into the game too
    bool overrideInput(PlayLayer* pl, bool down) {
        if (!cfg().apOverride || !pl || !pl->m_player1 || pl->m_player1->m_isDead) return false;
        float const px = pl->m_player1->getPositionX();
        if (down) {
            if (m_userHeld) return false;
            m_userHeld = true;
            UserClick c;
            c.x = px;
            c.tick = m_tick;
            c.y = pl->m_player1->getPositionY();
            c.best = m_apBestX;
            m_pending.push_back(c);
            m_clicksMade++;
            m_userClickTick = m_tick;
            m_userClickX = px;
        } else {
            // only the release of YOUR press (GD lets go of the button itself
            // on a respawn - that isn't you)
            if (!m_userHeld) return false;
            m_userHeld = false;
            for (auto it = m_pending.rbegin(); it != m_pending.rend(); ++it) {
                if (!it->replay && !it->released) {
                    it->released = true;
                    it->hold = static_cast<int>(std::max<long long>(1, m_tick - it->tick));
                    break;
                }
            }
        }
        m_gameHolding = down;
        if (m_apRunValid) m_apRun.push_back(ApInput{px, down});
        m_autoLastInTick = m_tick;
        m_autoLastInPush = down;
        m_autoLastInX = px;
        m_rtOn = false; // autoplay plans on from here, around your click
        return true;
    }
public:
    int m_apAttemptTicks = 0; // real ticks into this attempt
    // Retry Deaths' list of inputs to try for real at one death spot
    struct SpotTrial {
        float x = 0.f;              // where it dies
        struct C { float x = 0.f; bool push = false; int hold = 0; float pen = -1.f; };
        std::vector<C> list;        // nearest miss first
        size_t next = 0;            // the next one to try
        int tried = 0;
        int solved = -1;            // the one that got past (used again)
        bool used = false;          // this attempt plays list[usedIdx]
        size_t usedIdx = 0;
    };
    std::vector<SpotTrial> m_trials;
    int m_trialCommits = 0;
    SpotTrial* trialAt(float x) {
        for (auto& t : m_trials) if (std::fabs(t.x - x) < 60.f) return &t;
        return nullptr;
    }
    // The route is stuck at a spot it died at before: put the next input of
    // that spot's list into the route (true = done).
    bool apTrialCommit(PlayLayer* pl, int margin) {
        float const dx = rtAt(rtEnd()).x;
        SpotTrial* st = trialAt(dx);
        if (!st) {
            if (m_trials.size() >= 200) return false;
            m_trials.push_back(SpotTrial{});
            st = &m_trials.back();
            st->x = dx;
        }
        // (re)build the list from this search's tests: nearest miss first
        if (st->list.empty() || (st->next >= st->list.size() && st->solved < 0)) {
            auto cands = m_rs.cands;
            std::sort(cands.begin(), cands.end(), [](RtSearch::Cand const& a, RtSearch::Cand const& b) {
                float const pa = a.pen < 0.f ? 1e6f : a.pen, pb = b.pen < 0.f ? 1e6f : b.pen;
                if (std::fabs(pa - pb) > 0.25f) return pa < pb;
                return a.reach > b.reach;
            });
            // spread out first: at most one per 6 ticks of timing (per press /
            // release and hold), so the first tries cover early, late, short
            // and long - then the ones in between
            std::vector<SpotTrial::C> list;
            for (float const spacing : {6.f, 0.5f}) {
                float const tol = std::max(0.05f, m_mvStep * spacing);
                for (auto const& c : cands) {
                    if (c.t < m_rtBase || c.t > rtEnd()) continue;
                    float const x = rtAt(c.t).x;
                    if (apBanned(x, c.push)) continue;
                    bool dup = false;
                    for (auto const& e : list) {
                        if (e.push == c.push && e.hold == c.k && std::fabs(e.x - x) < tol) { dup = true; break; }
                    }
                    if (!dup) list.push_back(SpotTrial::C{x, c.push, c.k, c.pen});
                    if (list.size() >= 600) break;
                }
            }
            if (list.empty()) return false;
            st->list = std::move(list);
            st->next = 0;
            st->used = false;
        }
        if (st->used) return false; // this attempt has its try there already (it's in the route)
        size_t idx = 0;
        if (st->solved >= 0 && static_cast<size_t>(st->solved) < st->list.size()) idx = static_cast<size_t>(st->solved);
        else {
            while (st->next < st->list.size() && apBanned(st->list[st->next].x, st->list[st->next].push)) st->next++;
            if (st->next >= st->list.size()) return false;
            idx = st->next;
        }
        SpotTrial::C const c = st->list[idx];
        // the route's tick at that x (the same x grid after a respawn)
        int best = -1;
        float bd = 1e9f;
        for (int t = std::max(m_rtBase, m_rtNow + 1); t <= rtEnd(); t++) {
            float const d = std::fabs(rtAt(t).x - c.x);
            if (d < bd) { bd = d; best = t; }
        }
        if (best < 0 || bd > std::max(0.1f, m_mvStep * 0.6f)) return false; // that moment has passed
        std::vector<SpTick> trail;
        bool const heldBefore = best > m_rtBase ? rtHeldAt(best - 1) : rtHeldAt(best);
        if (c.push == heldBefore) return false; // (that input isn't a change from here)
        rtRun(pl, best, c.push, c.hold, std::max(8, scaleTicks(120, rtS())) + margin, trail, false);
        rtAppendHold(best, c.push, c.hold, trail, m_simOrbObj, m_simOrbStep);
        m_rtEvents[best] = c.push;
        if (c.hold > 1) m_rtEvents[best + c.hold] = !c.push;
        m_rtLastEvent = best + (c.hold > 1 ? c.hold : 0);
        if (!st->used || st->usedIdx != idx) {
            st->used = true;
            st->usedIdx = idx;
            m_trialCommits++;
            autoNote(fmt::format("retry at x {:.0f}: trying {} {} at x {:.1f}{} for real ({} of {} on the list{})", st->x,
                                 st->solved >= 0 ? "the input that got past:" : "", c.push ? "press" : "release", c.x,
                                 c.hold > 0 ? fmt::format(" held {}t", c.hold) : std::string(), idx + 1, st->list.size(),
                                 c.pen >= 0.f ? fmt::format(", predicted {:.1f}px into it", c.pen) : std::string()), false);
        }
        return true;
    }
    // a real death (autoplay): the jump it died on gets another input next time
    void apOnDeath(PlayLayer* pl, float dx) {
        for (auto& t : m_trials) {
            if (!t.used) continue;
            if (std::fabs(t.x - dx) < 60.f) { // its input died too: the next one
                t.next = std::max(t.next, t.usedIdx + 1);
                t.tried++;
                if (t.solved == static_cast<int>(t.usedIdx)) t.solved = -1;
            } else if (dx > t.x + 60.f && t.solved < 0) {
                t.solved = static_cast<int>(t.usedIdx); // got past it: keep that one
                autoNote(fmt::format("retry at x {:.0f}: that input got past it - kept", t.x), false);
            }
            t.used = false;
        }
        if (!cfg().apRetry || !apPracticeActive(pl)) return;
        float const cpX = m_apRespawnX;
        // the last input since the checkpoint: banned there (and, after a few
        // deaths, the one before it too - the mistake can be a jump earlier)
        int banned = 0;
        std::string what;
        for (auto it = m_apRun.rbegin(); it != m_apRun.rend() && banned < (m_apRetries >= 3 ? 2 : 1); ++it) {
            if (it->x < cpX - 0.01f) break;
            if (!apBanned(it->x, it->push)) {
                m_apBans.push_back(*it);
                what += fmt::format("{}{} at x {:.1f}", what.empty() ? "" : ", ", it->push ? "press" : "release", it->x);
            }
            banned++;
        }
        if (std::fabs(dx - m_apRetryX) < 60.f) m_apRetries++;
        else {
            m_apRetries = 1;
            m_apRetryX = dx;
        }
        m_apRetryTotal++;
        {
            autoNote(fmt::format("retry {} at x {:.0f}: {} - trying another way from the checkpoint at x {:.0f}", m_apRetries, dx,
                                 what.empty() ? "no input to rule out (it tries what got furthest)" : "won't use " + what + " again",
                                 cpX > -1e8f ? cpX : m_apRunStartX), false);
        }
    }
    // respawned: the run's inputs go back to where it respawned
    void apOnRespawn(PlayLayer* pl) {
        for (auto& t : m_trials) t.used = false;
        m_memPlaying = false; // (2.3) picked up again from where you respawn
        m_memSkip = false;
        if (m_memDirty) memSave();
        // (practice: you respawn at your checkpoint - where you are now)
        float const rx = pl && pl->m_player1 ? pl->m_player1->getPositionX() : 0.f;
        bool const atCheckpoint = apPracticeActive(pl) && pl->m_checkpointArray && pl->m_checkpointArray->count() > 0;
        m_apRespawnX = rx;
        if (atCheckpoint) {
            std::erase_if(m_apRun, [&](ApInput const& a) { return a.x >= rx - 0.01f; });
            std::erase_if(m_apBans, [&](ApInput const& b) { return b.x < rx - 1.f; }); // ruled out behind it: free again
            return;
        }
        // a fresh attempt from the start
        m_apRun.clear();
        m_apRunValid = false;
    }
    // the end of the level during the learning run: no end animation, a restart
    bool learnEnd() {
        if (m_learnReset) return true;
        if (m_learn != 1 || !m_autoOn) return false;
        learnFinish("reached the end");
        return true;
    }
    bool learning() const { return m_learn == 1 && m_autoOn; }
    // start of a frame (outside GD's step): the restart after the learning run
    void preFrame(PlayLayer* pl) {
        if (!pl) return;
        if (m_learnReset) {
            m_learnReset = false;
            pl->resetLevel();
            return;
        }
        // (2.3) Retry Deaths: practice mode (you put the checkpoints down),
        // switched between steps
        PlayerObject* p = pl->m_player1;
        bool const want = m_autoOn && cfg().apRetry && !deathsOff() && !pl->m_isPlatformer;
        if (!want) {
            if (m_apPracticeOurs && pl->m_isPracticeMode && !m_autoOn) {
                m_apPracticeOurs = false;
                pl->togglePracticeMode(false); // (GD restarts the level)
            } else if (m_apPracticeOurs && m_autoOn) {
                m_apPracticeOurs = false; // stays in practice (switching out restarts the level)
            }
            return;
        }
        if (!p || p->m_isDead) return;
        if (!pl->m_isPracticeMode) {
            pl->togglePracticeMode(true);
            m_apPracticeOurs = true;
            autoNote("Retry Deaths: practice mode on - put your checkpoints down; every death retries that jump with another input", false);
            return;
        }
        m_apPracticeOurs = true; // (you were in practice already)
    }
    bool autoplayOn() const { return m_autoOn; }
    bool autoUsed() const { return m_autoUsed; }
    void setAutoplay(bool on) {
        if (on == m_autoOn) return;
        m_autoOn = on;
        m_ap = AutoPlan{};
        clearBuffer();
        if (!on) {
            if (PlayLayer* pl = PlayLayer::get(); pl && m_gameHolding) fireButton(pl, false);
            m_physicalHolding = false;
        }
        pushLog(on ? "AUTOPLAY on: the mod plays, your clicks are ignored (Tools tab to stop)" : "AUTOPLAY off",
                COL_CYAN, false);
    }
    // destroyPlayer on a real player (before GD handles it)
    void onAutoDeath(PlayerObject* p, GameObject* obj) {
        if (!m_autoOn || !p) return;
        ModeInfo mi;
        mi.add(p);
        std::string const mode = mi.name();
        AutoStat& st = m_autoStats[mode];
        st.deaths++;
        std::string const killer = obj ? fmt::format("{} (id {}) at ({:.0f}, {:.0f})", objectName(obj->m_objectType),
                                                     obj->m_objectID, obj->getPositionX(), obj->getPositionY())
                                       : std::string("the ground / a wall");
        int const k = m_rtNow - 1; // the tick it died on
        std::string verdict;
        bool const predBug = m_rtOn && k >= m_rtBase && k + 1 <= rtEnd() && !(m_rtStuck && k >= rtEnd() - 1);
        // (2.3) the death record: everything about the moment, and what kind
        // of death it was
        {
            PlayLayer* dpl = PlayLayer::get();
            int const tps = static_cast<int>(60.f / std::max(getValidDt(), 0.01f));
            long long const sinceIn = m_autoLastInTick >= 0 ? m_tick - m_autoLastInTick : -1;
            long long const sinceTrans = m_transTick >= 0 ? m_tick - m_transTick : -1;
            std::string kind, why;
            if (predBug) {
                kind = "physics mismatch";
                why = "the route said you'd live here - the game did something the prediction didn't";
            } else if (sinceTrans >= 0 && sinceTrans <= tps * 4 / 10) {
                kind = "transition";
                why = fmt::format("{} {} ticks before - the plan through it was off", m_transWhat, sinceTrans);
            } else if (m_rtStuck) {
                kind = "pathfinding";
                why = "no route through was found here (every input it tested died)";
            } else if (!m_rtOn || k > rtEnd()) {
                kind = "pathfinding";
                why = "the route didn't reach this far yet";
            } else if (sinceIn >= 0 && sinceIn <= tps / 2 && (!m_autoLastInPush || (p->m_isShip || p->m_isDart || p->m_isBird))) {
                kind = "release";
                why = m_autoLastInPush ? "held too long" : "let go at the wrong moment";
            } else if (sinceIn >= 0 && sinceIn <= tps / 2) {
                kind = "timing";
                why = "the right input on the wrong tick";
            } else {
                kind = "pathfinding";
                why = "no input for a while before it";
            }
            m_deathKinds[kind]++;
            std::string routeThere = "no route there";
            if (m_rtOn && k >= m_rtBase && k <= rtEnd()) {
                SpTick const& q = rtAt(k);
                routeThere = fmt::format("route there x {:.1f} y {:.1f} (you {:.1f}px off)", q.x, q.y,
                                         std::hypot(p->getPositionX() - q.x, p->getPositionY() - q.y));
            }
            autoNote(fmt::format("death record [{}]: tick {} x {:.1f} y {:.1f} | speed x {:.2f}/tick y {:.2f} | {} {}{} | button {}, last "
                                 "input {} | queue {} | {} | killed by {} | {}: {}",
                                 kind, m_rtNow, p->getPositionX(), p->getPositionY(), m_mvStep, p->m_yVelocity, mode,
                                 p->m_isUpsideDown ? "gravity up" : "gravity down", dpl && dpl->m_isPracticeMode ? ", practice" : "",
                                 holdingJump(p) ? "held" : "up",
                                 sinceIn >= 0 ? fmt::format("{} {}t ago at x {:.1f}", m_autoLastInPush ? "press" : "release", sinceIn, m_autoLastInX)
                                              : std::string("none this attempt"),
                                 m_queue.pending() ? "has a waiting input" : "empty", routeThere, killer, kind, why), false);
        }
        if (predBug) {
            st.simBugs++;
            int const kk = std::min(k + 1, rtEnd());
            SpTick const& q = rtAt(kk);
            verdict = fmt::format("PREDICTION BUG: the route said you'd live here (it went {:.0f} px further) - you were "
                                  "x {:+.2f}, y {:+.2f} off it", rtAt(rtEnd()).x - p->getPositionX(),
                                  p->getPositionX() - q.x, p->getPositionY() - q.y);
        } else if (m_rtStuck) {
            verdict = fmt::format("the route got stuck at x {:.0f} ({})", m_rtStuckX, m_rtStuckWhy);
        } else {
            verdict = "the route didn't reach this far yet (planning was too slow)";
        }
        autoNote(fmt::format("died [{}] at x {:.0f} y {:.0f} on {}: {}", mode, p->getPositionX(), p->getPositionY(),
                             killer, verdict), true);
        {
            bool found = false;
            for (auto& d : m_autoDeaths) {
                if (std::fabs(d.x - p->getPositionX()) < 30.f && std::fabs(d.y - p->getPositionY()) < 60.f) {
                    d.times++;
                    found = true;
                    break;
                }
            }
            if (!found && m_autoDeaths.size() < 400) m_autoDeaths.push_back(AutoDeath{p->getPositionX(), p->getPositionY(), 1});
        }
        clicksOnDeath(p->getPositionX()); // (2.3) your clicks since the last good one weren't good ones
        if (m_memPlaying) memOnDeath(p->getPositionX()); // (2.3) the memory didn't play the same: cut back
        else apOnDeath(PlayLayer::get(), p->getPositionX());
        m_ap = AutoPlan{};
        m_rtOn = false;
    }
private:
public:
    // Any input reached the game (raw or buffered): the one-tick check for this
    // step doesn't count.
    void noteInputForCheck() { m_osInput = true; m_dfInput = true; }
    void setStepDt(float dt) { m_stepDtNow = dt; }
    // (2.3) your input went in as made: check the 100 ms after it
    void noteRawInput(PlayLayer* pl, bool push) { driftStartInput(pl, push, false); }
private:

    void flushQueue(PlayLayer* pl) {
        int guard = 0;
        while (m_queue.pending() && guard++ < 64) {
            auto due = m_queue.poll(1e18, TIME_EPS);
            if (!due) break;
            fireButton(pl, *due);
        }
        m_queue.clear();
        m_live = false;
    }

    // Is the physics-step hook (which fires held inputs) running right now?
    bool stepsRunning() const {
        if (m_lastStepWall.time_since_epoch().count() == 0) return false;
        return Clock::now() - m_lastStepWall < std::chrono::milliseconds(STEP_STALL_MS);
    }

public:
    // Once per rendered frame: a held input waiting far longer than any window
    // allows (wall clock), or held while the step hook isn't running at all
    // (another mod replaced it, a huge hitch), is fired now.
    void watchdog(PlayLayer* pl) {
        if (!pl || pl->m_isPaused || !m_queue.pending()) {
            m_pendingSinceWall = Clock::time_point{};
            return;
        }
        auto const now = Clock::now();
        if (m_pendingSinceWall.time_since_epoch().count() == 0) m_pendingSinceWall = now;
        bool const stuck = now - m_pendingSinceWall > std::chrono::milliseconds(WATCHDOG_MS);
        if (stuck || !stepsRunning()) {
            m_watchdogFires++;
            if (m_watchdogFires == 1) {
                pushLog(stuck ? "note: a held input waited too long - released it (watchdog)"
                              : "note: physics hook not running - inputs go straight through",
                        COL_YELLOW, false);
            }
            flushQueue(pl);
            m_pendingSinceWall = Clock::time_point{};
        }
    }

    // Pausing: the release usually happens in the pause menu, which never reaches
    // us. Drop what was waiting and assume the button is up.
    void onPause(PlayLayer* pl) {
        if (m_queue.pending()) flushQueue(pl);
        clearBuffer();
        m_physicalHolding = false;
        m_pendingSinceWall = Clock::time_point{};
    }
private:

    // ------------------------------------------------------------------------
    //  Prediction cut-off: should this unmodelled object stop the lookahead?
    //
    //  Uses the object's real hitbox and the direction the sim player travels:
    //    * pads / portals / speed changers fire on CONTACT. They only matter if
    //      the player had not already touched them when the input arrived, and
    //      only once the player is about to touch them. Anything behind the
    //      player (or already being touched) is history.
    //    * orbs fire on a CLICK while overlapped, so they matter whenever the
    //      player is about to overlap one or is overlapping one right now.
    //    * an object the player cannot reach vertically never matters.
    // ------------------------------------------------------------------------
    // True the first time `obj` is applied in the current test.
    bool markUsed(GameObject* obj) {
        if (std::find(m_simUsed.begin(), m_simUsed.end(), obj) != m_simUsed.end()) return false;
        m_simUsed.push_back(obj);
        return true;
    }

    bool padStrength(GameObjectType t, float& out) const {
        int const i = static_cast<int>(t);
        if (i < 0 || i >= 64) return false;
        PadLearn const& p = m_pads[i];
        if (!p.seen || p.unstable) return false;
        out = p.bump;
        return true;
    }

    void checkStateChange(PlayerObject* player, GameObject* obj, SimTrack const& tr) {
        CCPoint pos = obj->getPosition();
        float const px = player->getPositionX();
        float const py = player->getPositionY();

        // Cheap reject before asking the object for its hitbox.
        if (std::fabs(pos.x - px) > 240.f || std::fabs(pos.y - py) > 240.f) return;

        float hw = 0.f, hh = 0.f;
        float cx = pos.x, cy = pos.y;
        CCRect const& r = obj->getObjectRect();
        if (r.size.width > 1.f && r.size.height > 1.f && r.size.width < 260.f && r.size.height < 520.f) {
            hw = r.size.width * 0.5f;
            hh = r.size.height * 0.5f;
            cx = r.origin.x + hw;
            cy = r.origin.y + hh;
        } else {
            Extent const e = fallbackExtent(obj->m_objectType);
            hw = e.hw;
            hh = e.hh;
        }

        float const size = std::clamp(player->m_vehicleSize, 0.3f, 1.3f);
        float const ph   = 15.f * size; // player half-size (mini is smaller)
        float const pad  = 6.f;

        // Vertical: can the player's hitbox ever overlap this object at this height?
        float const dy = std::fabs(cy - py) - hh - ph;
        if (dy > pad) return;

        // Horizontal, along the direction of travel.
        float const ahead    = (cx - px) * tr.dir;
        float const gap      = ahead - hw - ph;                       // > 0: not touching yet
        float const gapStart = (cx - tr.startX) * tr.dir - hw - ph;   // same, when the input arrived
        float const reach    = tr.stepPx + pad;                       // about one tick of travel
        bool const overlapping = gap <= 0.f && ahead >= -(hw + ph) && dy <= 0.f;
        // Orbs: GD's activation area is a bit more generous than this box test.
        // A click "touching" an orb within the pad counts as hitting it (it used
        // to stop the prediction instead: "raw: pink orb (touching)").
        bool const orbOverlap = gap <= pad && ahead >= -(hw + ph) - pad && dy <= pad;
        GameObjectType const type = obj->m_objectType;

        // Orb hit by the click being tested: let the sim player use it (only the
        // first one, and only shortly after the input lands, since we can't know
        // if a long hold would still be held by then). Otherwise the normal
        // cut-off rules below apply.
        // "Pressed" = still holding, or a press GD hasn't used up yet (its own
        // jump buffer), so a quick tap just before the orb still counts.
        // UFO / swing only use an orb on a fresh click (a press within the last
        // ~3 ticks) - holding from an earlier
        // click doesn't fire it. The sim used to treat a held UFO press as
        // hitting every orb on the way, so every "fire now" died and held
        // clicks never fired. Other modes keep hold-to-hit-orb.
        bool const clickOnly = player->m_isBird || player->m_isSwing;
        bool const freshPress = m_simHeld && m_simPressTick >= 0 && m_simTick - m_simPressTick <= m_orbClick;
        bool const pressed = clickOnly ? freshPress : (m_simHeld || player->m_jumpBuffered);
        // The orb this test already used: nothing more can happen with it. It
        // used to stop the prediction on the very next tick, so EVERY click on
        // an orb went "raw: ... orb (touching)".
        // (1.18.6 also skipped orbs in GD's "touched rings" list - but GD puts an
        // orb there as soon as you overlap it, so the orb you were clicking got
        // ignored and every click said "too late". Removed in 1.18.8.)
        // Orb contact by hitbox (1.24.8): GD only reports orb contact while the
        // button is down, so a path without a press never "saw" the orb coming
        // and orb aim never started.
        if (isModelledOrb(type) && player == m_simPlayer1.data() && orbOverlap && !realUsedOrb(obj) &&
            obj != m_simOrbObj) {
            if (auto* ring = typeinfo_cast<RingObject*>(obj)) {
                // the next orb after the one this test used (orb chains)
                if (m_simOrbUsed && m_simNextRingTick < 0) m_simNextRingTick = m_simTick;
                if (!m_simFirstRing) {
                    m_simFirstRing = ring;
                    m_simFirstRingTick = m_simTick;
                    m_simFirstRingLast = m_simTick;
                } else if (ring == m_simFirstRing && m_simTick > m_simFirstRingLast && m_simTick <= m_simFirstRingLast + 1) {
                    m_simFirstRingLast = m_simTick;
                }
            }
        }
        if (isOrb(type) && obj == m_simOrbObj) return;
        // GD reports orb contact exactly (see simRingTouched / applyGdRing): no
        // guessing from boxes, and a modelled orb never cuts the prediction.
        if (m_gdRings && m_orbPredict && isModelledOrb(type)) return;
        if (m_orbPredict && isModelledOrb(type) && !m_simOrbUsed && pressed && orbOverlap) {
            int const since = m_simTick - m_simDelay;
            if (since >= 0 && since <= m_orbIntent) {
                if (auto* ring = typeinfo_cast<RingObject*>(obj)) {
                    m_objRestore.emplace_back(obj, obj->m_isActivated);
                    m_simOrbUsed = true;
                    m_simOrbObj = obj;
                    simRingJump(player, ring);
                    return; // modelled: no cut-off
                }
            }
        }

        // Level scan (2.1): gamemode portals switch the hidden player with its
        // own toggles, and it remembers the portal (where GD keeps it per
        // player). Normal predictions still stop at them.
        if (m_scanSim) {
            if (type == GameObjectType::DualPortal || type == GameObjectType::SoloPortal) {
                if (overlapping && player == m_simPlayer1.data() && markUsed(obj)) {
                    m_objRestore.emplace_back(obj, obj->m_isActivated);
                    if (!m_scanTwoPlayer) m_simDualReq = type == GameObjectType::DualPortal ? 1 : 2;
                }
                return; // modelled
            }
            int const pm = portalMode(type);
            if (pm >= 0) {
                if (overlapping && markUsed(obj)) {
                    m_objRestore.emplace_back(obj, obj->m_isActivated);
                    // What GD does on a real portal first (switchToFlyMode etc.):
                    // the player's own "switched to mode" step (speed / velocity
                    // changes on entry), then the mode toggle. Only on an actual
                    // change - the portal is touched again on the next steps.
                    if (modeCode(player) != pm) player->switchedToMode(type);
                    setSimMode(player, pm);
                    player->m_lastPortalPos = obj->getPosition();
                    player->m_lastActivatedPortal = obj;
                    m_scanModeSwitches++;
                }
                return;
            }
        }

        // (2.3) Normal predictions go through gamemode portals too ("Predict
        // Through Portals", on by default), like the scan: GD's own "switched
        // to mode" step, the mode toggle, and the portal remembered (that's
        // where GD centres the new floor / ceiling - the sim keeps itself in
        // that band from here, see bandClamp). They used to stop right before
        // every gamemode portal ("can't see past"), so a click near one went in
        // raw or was judged on a cut-short path.
        if (!m_scanSim && m_predictPortals && gapStart > 0.f) {
            int const pm = portalMode(type);
            if (pm >= 0) {
                if (overlapping && markUsed(obj)) {
                    m_objRestore.emplace_back(obj, obj->m_isActivated);
                    if (modeCode(player) != pm) {
                        player->switchedToMode(type);
                        setSimMode(player, pm);
                        m_simModeSwitches++;
                    }
                    player->m_lastPortalPos = obj->getPosition();
                    player->m_lastActivatedPortal = obj;
                    m_simBandOwn = true;
                }
                return;
            }
        }
        // (2.3) Mirror portals only flip the picture - you still move the same
        // way in the level - so they no longer stop a prediction.
        if (type == GameObjectType::InverseMirrorPortal || type == GameObjectType::NormalMirrorPortal) {
            if (overlapping && markUsed(obj)) m_simMirrors++;
            return;
        }

        // Size portals (1.25.1): applied to the hidden sim player only, by
        // setting its size directly - no GD portal code, nothing on the real
        // portal object changes (1.20.0 called GD's toggles and broke real
        // portals). The orbs in Amethyst's UFO part sit right at mini / normal
        // size portals, so every orb click there went "raw: size portal".
        if (type == GameObjectType::MiniSizePortal || type == GameObjectType::RegularSizePortal) {
            if (overlapping && markUsed(obj)) {
                float const want = (type == GameObjectType::MiniSizePortal) ? MINI_SIZE : 1.f;
                if (std::fabs(player->m_vehicleSize - want) > 0.01f) {
                    player->m_vehicleSize = want;
                    player->setScale(want);
                    m_simSizeSwitches++;
                }
            }
            return; // modelled: never cuts the prediction
        }

        // Pads and speed / gravity portals fire on contact, no click involved,
        // so the sim can apply them itself. Anything the player was already
        // touching when the input arrived has been applied by the game.
        if ((m_padPredict || m_scanSim) && gapStart > 0.f) {
            if (isModelledPad(type)) {
                float bump = 0.f;
                if (padStrength(type, bump)) {
                    if (overlapping && markUsed(obj)) {
                        m_objRestore.emplace_back(obj, obj->m_isActivated);
                        player->bumpPlayer(bump, static_cast<int>(type), true, obj);
                    }
                    return; // modelled (now or on contact)
                }
                // strength not seen yet -> cut-off, like before
            } else if (type == GameObjectType::Modifier) {
                float speed = 0.f;
                if (speedForPortal(obj->m_objectID, speed)) {
                    if (overlapping && markUsed(obj)) {
                        // (2.3) the way a speed portal does it: GD's own speed
                        // change for the player (updateTimeMod, no effects), then
                        // the speed itself (like ReSpeed does). It used to set the
                        // speed only - GD keeps more than that per speed.
                        float const before = player->m_playerSpeed;
                        player->updateTimeMod(speed, true);
                        player->m_playerSpeed = speed;
                        // the per-tick travel the cut-off checks use
                        int const ti = player == m_simPlayer1.data() ? 0 : 1;
                        if (before > 0.01f) m_track[ti].stepPx *= speed / before;
                        m_simSpeedSwitches++;
                    }
                    return;
                }
                // level scan: a trigger (not a speed portal) - it can't run
                // triggers anyway, so it doesn't stop at them
                if (m_scanSim) return;
            } else if (isGravityPortal(type)) {
                if (overlapping && markUsed(obj)) {
                    bool const want = type == GameObjectType::InverseGravityPortal ? true
                                    : type == GameObjectType::NormalGravityPortal  ? false
                                    : !player->m_isUpsideDown;
                    if (want != player->m_isUpsideDown) {
                        player->flipGravity(want, true);
                        m_simGravFlips++;
                        if (m_scanSim && m_scanPortalTraces < 40) {
                            m_scanPortalTraces++;
                            trace(fmt::format("scan: gravity {} at ({:.1f},{:.1f})", want ? "up" : "down",
                                              player->getPositionX(), player->getPositionY()));
                        }
                    }
                }
                return;
            }
        }

        bool relevant = false;
        if (isOrb(obj->m_objectType)) {
            relevant = gap <= reach && ahead >= -(hw + ph) - pad;
        } else {
            relevant = gapStart > 0.f && gap <= reach;
        }
        if (!relevant) return;

        m_simTrunc.valid = true;
        m_simTrunc.type  = obj->m_objectType;
        m_simTrunc.gap   = gap;
        m_simTrunc.tick  = m_simTick;
        m_simTrunc.obj   = obj;
    }

    // ------------------------------------------------------------------------
    //  Core lookahead: apply `push` after `delay` ticks and see what happens.
    // ------------------------------------------------------------------------
    SimResult simulate(PlayLayer* pl, PlayerObject* realP1, PlayerObject* realP2, bool simP2,
                       bool push, int delay, int horizon, bool noLanding, float dt, float s,
                       FollowUp const& follow = FollowUp{}) {
        SimResult out;
        PlayerObject* s1 = m_simPlayer1.data();
        PlayerObject* s2 = (simP2 && realP2) ? m_simPlayer2.data() : nullptr;
        // (2.2) the level scan's dual: player 2 from its own saved state
        bool const scanP2 = m_scanSim && m_simSnap2 && m_simPlayer2.data();
        if (scanP2) s2 = m_simPlayer2.data();
        m_simDualReq = 0;
        m_simP2Live = false;
        m_simBandOwn = false; // (2.3) no portal of its own yet: GD's floor / ceiling is right
        if (!s1 || !realP1) {
            out.ticks = horizon;
            return out;
        }

        auto const simT0 = Clock::now();
        m_simulating = true;
        m_simDead = false;
        m_simTrunc = TruncInfo{};
        m_simDelay = delay;
        m_simTick = 0;
        m_simOrbUsed = false;
        m_simOrbObj = nullptr;
        m_simPressTick = -1; // a hold from before the test isn't a fresh click
        m_simFirstRing = nullptr;
        m_simFirstRingTick = -1;
        m_simFirstRingLast = -1;
        m_simNextRingTick = -1;
        m_simOrbStep = -1;
        orbModelLoad();
        m_prevRing[0] = touchingOrb(realP1);
        m_prevRing[1] = (simP2 && realP2) ? touchingOrb(realP2) : nullptr;
        m_simKillType = -1;
        m_simKillPen = -1.f;
        m_simKillPos = realP1 ? realP1->getPosition() : CCPoint{};
        m_simKillDy = 0.f;
        m_simStartY = realP1 ? realP1->getPositionY() : 0.f;
        m_simNow = m_gameTime;
        m_simDtCur = dt;
        m_objRestore.clear();
        m_simUsed.clear();
        m_scanClr = 1e9f;
        if (m_recordPath) m_path.clear();

        // (2.3) the hidden player in YOUR gamemode first (a test that went
        // through a gamemode portal, or a scan, leaves it in another one) -
        // before the copy, so the mode switch can't change what's copied
        setSimMode(s1, modeCode(realP1));
        if (s2) setSimMode(s2, modeCode(realP2 ? realP2 : realP1));
        syncSimPlayer(s1, realP1);
        if (s2) syncSimPlayer(s2, realP2 ? realP2 : realP1);
        if (m_simSnap) {
            setSimMode(s1, m_simSnap->mode); // the scan may be past a gamemode portal
            applySnap(s1, *m_simSnap); // level scan: a saved state on top of the real player's
            m_simStartY = s1->getPositionY();
            // (2.3) the orb YOU are touching now says nothing about a saved
            // state somewhere else (it made a test depend on when it ran)
            m_prevRing[0] = m_prevRing[1] = nullptr;
        }
        if (scanP2) {
            setSimMode(s2, m_simSnap2->mode);
            applySnap(s2, *m_simSnap2);
        }
        m_simHeld = m_gameHolding; // until the action lands, the button keeps the game's current state
        clearCollisionDicts(s1); // (2.3) a test's own collision log starts empty
        if (s2) clearCollisionDicts(s2);

        m_track[0] = SimTrack{s1->getPositionX(), 1.f, 0.f};
        m_track[1] = SimTrack{s2 ? s2->getPositionX() : 0.f, 1.f, 0.f};

        int airborne = 0;
        int landed = 0;
        int const airMin = scaleTicks(15, s);
        // Long enough that a spike right after landing is still seen.
        int const landMin = scaleTicks(40, s);
        int const total = delay + horizon;
        int const clrCommit = scaleTicks(ESCAPE_COMMIT_BASE, s);
        int const clrGrace  = scaleTicks(LAND_GRACE_BASE, s);

        float const fullDt = dt;
        float const phase0 = tickPhase();
        for (int i = 0; i < total; i++) {
            m_simTick = i;
            m_simStepsTotal++;
            // step 0 = the rest of the current tick (mid-tick after a CBF click)
            dt = (i == 0 && phase0 > 0.f) ? fullDt * (1.f - phase0) : fullDt;
            if (i == delay) {
                if (!(push && orbPress(s1, s2))) setHeld(s1, s2, push); // (orb first, no flap)
            }
            // Optional follow-up inputs (used to ask "could the player still save this?").
            for (int k = 0; k < FollowUp::N; k++) {
                if (follow.at[k] == i) setHeld(s1, s2, follow.held[k]);
            }

            float const prevY1 = s1->getPositionY();
            // (2.3) GD's order in a physics step: move, collide, and only then
            // turn (updateRotation runs at the very end of the step - Click
            // Between Frames relies on the same order). The sim turned the
            // player before its collision check.
            // (2.3) where the player was at the start of the step: GD's step sets
            // it before moving the player, and its collision check uses it (did
            // you come from above a block -> land on it). The hidden player
            // kept a stale one, so on the first tick of every test it could
            // fall into thin blocks it should have stood on - Every End's 6 px
            // floor slabs: autoplay's route fell through the floor every 60
            // ticks (each new run), and the one-tick check was 1 px off.
            s1->m_lastPosition = s1->getPosition();
            s1->update(dt);
            if (s2) {
                s2->m_lastPosition = s2->getPosition();
                s2->update(dt);
            }
            if (m_simDead) { out.end = SimEnd::Died; break; }

            // After the first tick we know which way each sim player travels and how far per tick.
            if (i == 0) {
                // (a partial first step covers less ground: scale to a full tick)
                float const full = (phase0 > 0.f) ? 1.f / (1.f - phase0) : 1.f;
                float const dx1 = s1->getPositionX() - m_track[0].startX;
                m_track[0].dir = dx1 >= 0.f ? 1.f : -1.f;
                m_track[0].stepPx = std::fabs(dx1) * full;
                if (s2) {
                    float const dx2 = s2->getPositionX() - m_track[1].startX;
                    m_track[1].dir = dx2 >= 0.f ? 1.f : -1.f;
                    m_track[1].stepPx = std::fabs(dx2) * full;
                }
            }

            m_tickClr = 1e9f;
            m_tickRing[0] = m_tickRing[1] = nullptr;
            // (2.2: back on - a crash test had switched this off, which left the
            // buffer blind. The scan crash it was chasing is fixed in applySnap.)
            m_ccoInCheck = false;
            m_inCheck = s1;
            pl->checkCollisions(s1, dt, false);
            m_inCheck = nullptr;
            noteCollisionPath(s1);
            extraCollisions(pl, s1, dt);
            restoreUndisabled();
            mvRestore(); // (2.3) the moving objects back where they are
            // (2.2) the portal's floor / ceiling (2.3: in a normal prediction
            // too, once it went through a gamemode portal of its own)
            if ((m_scanSim || m_simBandOwn) && !m_simDead) bandClamp(s1, prevY1);
            if (m_scanSim && !m_rtSim && !m_simDead && !m_scanNoclip && !nearLevel(s1)) {
                // (2.2) nothing of the level near it (no block / spike / slope
                // within OFF_PATH_PX): walking under the level, flying off it -
                // not inputs that get you through
                m_simDead = true;
                m_simKillType = KILL_OFF_PATH;
                m_simKillPos = s1->getPosition();
                m_simKillDy = s1->getPositionY() - m_simStartY;
            }
            if (m_scanSim && !m_simDead && !m_scanNoclip &&
                (s1->getPositionY() > m_levelTop || s1->getPositionY() < m_levelBottom)) {
                m_simDead = true; // (2.2) flew out of the level: not a way through
                m_simKillType = KILL_OUT_OF_LEVEL;
                m_simKillPos = s1->getPosition();
                m_simKillDy = s1->getPositionY() - m_simStartY;
            }
            if (m_simDead) { out.end = SimEnd::Died; break; }
            m_scanClr = std::min(m_scanClr, m_tickClr);
            m_prevRing[0] = m_tickRing[0]; // what it touches now = "already touching" next step
            applyGdRing(s1, 0);
            // (2.2) level scan through a dual portal: player 2 appears where
            // player 1 is, gravity flipped, same gamemode and button - like GD.
            // A solo portal takes it away again.
            if (m_simDualReq == 1 && !s2 && m_simPlayer2.data()) {
                s2 = m_simPlayer2.data();
                PlayerSnap const now = snapFrom(s1);
                syncSimPlayer(s2, realP1);
                setSimMode(s2, now.mode);
                applySnap(s2, now);
                s2->flipGravity(!s1->m_isUpsideDown, true);
                if (m_simHeld) s2->pushButton(PlayerButton::Jump);
                else s2->releaseButton(PlayerButton::Jump);
                m_track[1] = SimTrack{s2->getPositionX(), m_track[0].dir, m_track[0].stepPx};
            } else if (m_simDualReq == 2) {
                s2 = nullptr;
            }
            m_simDualReq = 0;
            if ((m_scanSim || m_simBandOwn) && s2 && !m_simDead) bandClamp(s2, s2->getPositionY());

            if (s2) {
                m_ccoInCheck = false;
                m_inCheck = s2;
                pl->checkCollisions(s2, dt, false);
                m_inCheck = nullptr;
                noteCollisionPath(s2);
                extraCollisions(pl, s2, dt);
                restoreUndisabled();
                mvRestore();
                if (m_simDead) { out.end = SimEnd::Died; break; }
                m_prevRing[1] = m_tickRing[1];
                applyGdRing(s2, 1);
            }
            s1->updateRotation(dt); // (2.3) end of the step, like GD
            if (s2) s2->updateRotation(dt);
            // (2.3) What GD does to every player at the end of a step outside
            // PlayerObject::update, found by the drift detail (your state vs the
            // route's, field by field): the player's clock moves on (it stood
            // still in every prediction - 738 ticks into a route its m_totalTime
            // was where the route began, and GD times landings, jump buffering
            // and gamemode / flip grace with it), its stored position follows
            // it (m_position was the route's start for the whole route) and,
            // on the ground, the last grounded position too.
            if (m_diag) {
                DiagStep d;
                d.x = s1->getPositionX();
                d.y = s1->getPositionY();
                d.vy = s1->m_yVelocity;
                d.ground = s1->m_isOnGround;
                d.cco = m_ccoInCheck;
                d.passed = m_passPlayer == s1 ? static_cast<int>(m_passed.size()) : 0;
                d.added = m_passPlayer == s1 ? static_cast<int>(m_passAdded.size()) : 0;
                d.cbmy = s1->m_collidedBottomMaxY;
                if (m_diagObj && m_passPlayer == s1) {
                    d.gPassed = std::find(m_passed.begin(), m_passed.end(), m_diagObj) != m_passed.end();
                    d.gAdded = std::find(m_passAdded.begin(), m_passAdded.end(), m_diagObj) != m_passAdded.end();
                }
                m_diag->push_back(d);
            }
            for (PlayerObject* sp : {s1, s2}) {
                if (!sp) continue;
                sp->m_totalTime += static_cast<double>(dt) / 60.0;
                sp->m_position = sp->getPosition();
                if (sp->m_isOnGround) sp->m_lastGroundedPos = sp->getPosition();
                // (2.3) also from the drift detail: yours was your position
                // every tick (a stored position, despite its name), the route's
                // stale; and GD's step time
                sp->m_shipRotation = sp->getPosition();
                sp->m_unk838 = static_cast<float>(dt / 60.0);
                gdResetCollisionLog(sp);
            }
            if (m_simTrail) { // (2.2) the sprint keeps every tick
                SpTick t;
                t.s1 = snapFrom(s1);
                t.dual = s2 != nullptr;
                if (s2) { t.s2 = snapFrom(s2); t.y2 = s2->getPositionY(); }
                t.x = s1->getPositionX();
                t.y = s1->getPositionY();
                t.vy = s1->m_yVelocity;
                t.modeCode = modeCode(s1);
                t.modeIdx = scanModeIndex(s1);
                t.mini = s1->m_vehicleSize > 0.f && s1->m_vehicleSize < 0.99f;
                m_simTrail->push_back(std::move(t));
            }
            // Orb aim: alive a few ticks past the next orb = that orb is your next
            // click's job; the rest of the path doesn't matter (CPU).
            if (m_simChainStop > 0 && m_simNextRingTick >= 0 && i >= m_simNextRingTick + m_simChainStop) {
                out.ticks = horizon;
                out.end = SimEnd::Survived;
                break;
            }

            if (i >= delay) {
                out.ticks++;
                if (!noLanding) {
                    bool grounded = s1->m_isOnGround && (!s2 || s2->m_isOnGround);
                    if (!grounded) {
                        airborne++;
                        landed = 0;
                    } else if (airborne >= airMin) {
                        if (out.landTick < 0) out.landTick = out.ticks;
                        // Landed and alive past the grace: every verdict treats a
                        // later death as the next jump's job, so stop here (was
                        // 40 more ticks of collision checks per test).
                        ++landed;
                        if (out.ticks > out.landTick + clrGrace || landed >= landMin) {
                            out.ticks = horizon;
                            out.end = SimEnd::Survived;
                            break;
                        }
                    }
                }
                // Clearance over the part of the path this click is judged on.
                bool const judged = noLanding ? (out.ticks <= clrCommit)
                                              : (out.landTick < 0 || out.ticks <= out.landTick + clrGrace);
                if (judged) out.clear = std::min(out.clear, m_tickClr);
            }

            if (m_recordPath) m_path.push_back(acecore::Point2{s1->getPositionX(), s1->getPositionY(), s1->getRotation()});

            if (m_simTrunc.valid && !m_scanNoclip) {
                out.end = SimEnd::Truncated;
                out.trunc = m_simTrunc;
                break;
            }

            if ((i & 3) == 3 && budgetExpired()) { // every 4 steps: heavy levels can't overrun far
                out.end = SimEnd::OutOfBudget;
                break;
            }
        }

        // Put back anything an orb activation changed on real level objects.
        // Newest first, so the oldest snapshot (before the test touched it) wins.
        for (auto it = m_objRestore.rbegin(); it != m_objRestore.rend(); ++it) it->first->m_isActivated = it->second;
        m_objRestore.clear();
        restoreUndisabled(); // (2.3) safety: never leave a level object switched on
        mvRestore();         // (2.3) ... or moved

        m_simP2Live = s2 != nullptr;
        out.killType = m_simKillType;
        out.killDy = m_simKillDy;
        out.killPen = m_simKillPen;
        out.killX = m_simKillPos.x;
        out.killY = m_simKillPos.y;
        out.orbUsed = m_simOrbUsed;
        out.firstRing = m_simFirstRing;
        out.firstRingTick = m_simFirstRingTick;
        out.firstRingLast = m_simFirstRingLast;
        out.nextRingTick = m_simNextRingTick;
        if (out.end == SimEnd::Died) out.deathStep = m_simTick; // step index incl. the delay
        m_simUsTotal += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - simT0).count();
        m_simulating = false;
        return out;
    }

    // ------------------------------------------------------------------------
    //  Diagnostics
    // ------------------------------------------------------------------------
    static float ticksToMs(long long ticks, float dt) {
        return static_cast<float>(ticks) * dt * (1000.f / 60.f);
    }

    float levelMs() const {
        return std::chrono::duration<float, std::milli>(Clock::now() - m_levelStart).count();
    }

    // "hits spike at +40px, via orb" - what a predicted death runs into.
    static std::string deathWhat(SimResult const& r) {
        if (r.killType == KILL_OUT_OF_LEVEL) return fmt::format("flew out of the level ({:+.0f}px)", r.killDy);
        if (r.killType == KILL_OFF_PATH) return fmt::format("left the level's path (nothing near, {:+.0f}px)", r.killDy);
        std::string what = r.killType >= 0 ? objectName(static_cast<GameObjectType>(r.killType)) : "a block edge";
        return fmt::format("hits {} at {:+.0f}px{}", what, r.killDy, r.orbUsed ? ", after using an orb" : "");
    }

    // Decide whether (and how) a click shows up in the on-screen log.
    // One click as a Log line: tag + short text ("BUFFER  press +6t (25ms) [ufo]").
    static bool panelLine(ClickNote const& n, Config const& c, float dt, std::string& tag, std::string& text,
                          ccColor3B& color) {
        char const* act = n.push ? "press" : "release";
        std::string const mode = n.modeName.empty() ? std::string() : fmt::format(" [{}]", n.modeName);
        color = COL_WHITE;
        switch (n.d) {
            case Decision::Buffered:
                if (n.rescue) {
                    tag = "HOLD";
                    text = fmt::format("{}, checking every tick (up to +{}t){}", act, n.windowTicks, mode);
                    return true;
                }
                if (n.orbHelp) {
                    tag = "ORB";
                    text = fmt::format("press waits for the orb (+{}t){}", n.delay, mode);
                    return true;
                }
                if (n.stream && !c.hudVerbose) return false; // spam: one line per stream is enough
                tag = "BUFFER";
                text = fmt::format("{} +{}t ({:.1f}ms){}{}", act, n.delay, ticksToMs(n.delay, dt),
                                   n.stream ? ", spam kept steady" : "", mode);
                return true;
            case Decision::Queued:
                return false; // inputs behind a held one: no line (2.1)
            case Decision::PassTruncated:
                tag = "RAW";
                text = fmt::format("{}, {}", act, describeTrunc(n.r0.trunc));
                return true;
            case Decision::PassNoFix:
                // Shown only with "Log: show safe clicks": the click went through
                // as made, nothing for you to act on.
                if (!c.hudVerbose) return false;
                tag = "RAW";
                if (n.needed == 0) {
                    text = fmt::format("{}, too late: dies in {}t ({}){}", act, n.r0.ticks, deathWhat(n.r0), mode);
                } else if (n.needed > 0) {
                    text = fmt::format("{}, needed +{}t (window {}t){}", act, n.needed, n.windowTicks, mode);
                } else {
                    text = fmt::format("{}, no fix in {}t (dies in {}t, {}){}", act, n.windowTicks, n.r0.ticks,
                                       deathWhat(n.r0), mode);
                }
                color = COL_GREY;
                return true;
            case Decision::PassSpam:
                tag = "RAW";
                text = fmt::format("{}, spam guard", act);
                return true;
            case Decision::PassBudget:
                tag = "RAW";
                text = fmt::format("{}, out of CPU time", act);
                return true;
            case Decision::PassEscapable:
            case Decision::PassSafe:
                return false; // on time, nothing changed: no line (2.1)
            case Decision::PassRelease:
                return false; // releases that do nothing: never worth a line (1.25.2)
            case Decision::PassModeOff:
                if (!c.hudVerbose) return false;
                tag = "RAW";
                text = fmt::format("{}, {} is off (Modes tab)", act, n.modeName.empty() ? "this mode" : n.modeName);
                color = COL_GREY;
                return true;
            case Decision::PassNoWindow:
                if (!c.hudVerbose) return false;
                tag = "RAW";
                text = fmt::format("{}, window under 1 tick", act);
                color = COL_GREY;
                return true;
            default:
                return false;
        }
    }

    // Record one click decision (stats always, text only if a log is enabled).
    // ---- Learned margin (2.3) ------------------------------------------------
    //  Per input type (cube, ship press, wave release, ...): when a click the
    //  buffer moved dies anyway within half a second (and you made no other
    //  input since), the predictions there were a little off for you - your
    //  setup, frame rate, input mod - so next time it fires 1 tick deeper
    //  into the safe window (up to 3). Each moved click that lives eases it
    //  back by a quarter tick. Saved; on with Learn From Attempts.
    float m_lmargin[W_COUNT] = {};
    struct BufWatch { bool on = false; int g = 0; double at = 0.0; long long inputs = 0; };
    BufWatch m_bufWatch;
    int m_lmUps = 0, m_lmDowns = 0;
    void lmarginLoad() {
        std::string const t = Mod::get()->getSavedValue<std::string>("learned-margin", "");
        std::istringstream in(t);
        for (int i = 0; i < W_COUNT; i++) {
            float v = 0.f;
            char comma = 0;
            if (!(in >> v)) break;
            m_lmargin[i] = std::clamp(v, 0.f, 3.f);
            in >> comma;
        }
    }
    void lmarginSave() {
        std::string t;
        for (int i = 0; i < W_COUNT; i++) t += fmt::format("{}{:.2f}", i ? "," : "", m_lmargin[i]);
        Mod::get()->setSavedValue<std::string>("learned-margin", t);
    }
    void lmarginTick() {
        if (!m_bufWatch.on || m_gameTime - m_bufWatch.at <= 30.0) return;
        m_bufWatch.on = false; // lived half a second: it was fine
        float& m = m_lmargin[m_bufWatch.g];
        if (m > 0.f) {
            m = std::max(0.f, m - 0.25f);
            m_lmDowns++;
            lmarginSave();
        }
    }
    void lmarginOnDeath() {
        if (!m_bufWatch.on) return;
        m_bufWatch.on = false;
        if (!cfg().learnedMargin || m_gameTime - m_bufWatch.at > 30.0 || m_inputsSeen != m_bufWatch.inputs) return;
        float& m = m_lmargin[m_bufWatch.g];
        if (m >= 3.f) return;
        m = std::min(3.f, m + 1.f);
        m_lmUps++;
        lmarginSave();
        pushLog(fmt::format("a moved click died right after - next time it fires {:.0f} tick{} deeper into the safe window ({})",
                            m, m >= 1.5f ? "s" : "", WINDOW_KEYS[m_bufWatch.g] + 7), COL_CYAN, false);
    }
    void note(Config const& c, ModeInfo const& mode, float dt, ClickNote const& n) {
        if (n.d == Decision::Buffered && !n.stream && !m_autoOn)
            m_bufWatch = BufWatch{true, static_cast<int>(primaryWin(mode, n.push)), m_gameTime, m_inputsSeen};
        m_devNote = n;
        m_devNoteMode = mode.name();
        m_devNoteAt = m_gameTime;
        m_devNoteHave = true;
        m_stats.clicks++;
        m_stats.byDecision[static_cast<int>(n.d)]++;
        if (n.us > 0) {
            m_stats.simClicks++;
            m_stats.sumUs += n.us;
            m_stats.maxUs = std::max(m_stats.maxUs, n.us);
        }
        // The click a death gets blamed on: an irrelevant release (cube) or an
        // input queued behind a held one must not hide the press that mattered.
        if (n.d != Decision::PassRelease && n.d != Decision::Queued) {
            m_last.decision = n.d;
            m_last.f0End = n.r0.end;
            m_last.f0Ticks = n.r0.ticks;
            m_last.delay = n.delay;
            m_last.horizon = n.horizon;
            m_last.time = m_gameTime;
            m_last.trunc = n.r0.trunc;
        }

        // (no early return: the menu's Log tab keeps a history even with all logs off)

        std::string const modeName = mode.name();
        m_lastMode = modeName;
        int const tps = static_cast<int>(std::lround(60.0f / dt));

        // Extra column: what cut the prediction (if anything) and whether extreme was on.
        std::string extra;
        if (n.d == Decision::PassTruncated) extra = describeTrunc(n.r0.trunc);
        if (c.extreme) extra += extra.empty() ? (c.custom ? "custom" : "extreme") : (c.custom ? " [custom]" : " [extreme]");

        if (c.debug) {
            log::debug("[Aceware] {} {} {} tps={} win={}t h={} sim={}us evals={} f0={}/{} delay={}{} {}",
                       modeName, n.push ? "press" : "release", decisionName(n.d), tps, n.windowTicks,
                       n.horizon, n.us, n.evals, simEndName(n.r0.end), n.r0.ticks, n.delay,
                       n.orb ? " orb" : "", extra);
        }
        {
            // Report trace (2.1): every decision, shown or not.
            PlayLayer* tpl = PlayLayer::get();
            PlayerObject* tp = tpl ? tpl->m_player1 : nullptr;
            trace(fmt::format("click {} {} {} delay={} win={}t h={} f0={}/{} us={} at=({:.1f},{:.1f}){}{}",
                              modeName, n.push ? "press" : "release", decisionName(n.d), n.delay, n.windowTicks,
                              n.horizon, simEndName(n.r0.end), n.r0.ticks, n.us, tp ? tp->getPositionX() : 0.f,
                              tp ? tp->getPositionY() : 0.f, extra.empty() ? "" : " ", extra));
            if (n.d != Decision::PassRelease && n.d != Decision::Queued) { m_lvlStats.clicks++; m_lifeStats.clicks++; }
            if (n.d == Decision::Buffered) { m_lvlStats.buffered++; m_lifeStats.buffered++; }
        }
        if (c.csv) {
            m_csv.row(fmt::format("CLICK,{:.1f},{},{},{},{},{},{},{},{},{},{},{}",
                                  levelMs(), modeName, n.push ? "press" : "release", tps, n.windowTicks,
                                  n.horizon, n.us, simEndName(n.r0.end), n.r0.ticks, decisionName(n.d),
                                  n.delay, extra));
        }
        {
            // One line, built only if it will be shown (1.26.1: every click used to
            // build it twice and keep hidden "safe click" lines too).
            std::string tag, text;
            ccColor3B color = COL_WHITE;
            ClickNote tagged = n;
            tagged.modeName = modeName;
            if (panelLine(tagged, c, dt, tag, text, color)) pushTagged(std::move(tag), std::move(text), color);
        }
    }

    // One log line into the history shown by the window's Log tab.
    void pushLog(std::string text, ccColor3B color, bool minor) {
        // Tag from how the line starts (lines that don't say: NOTE / CHECK).
        auto starts = [&](char const* p) { return text.rfind(p, 0) == 0; };
        std::string tag = "NOTE";
        if (starts("DEATH: ")) { tag = "DEATH"; text = text.substr(7); }
        else if (starts("PRESS fired ") || starts("REL fired ")) {
            tag = "FIRED";
            text = (starts("PRESS") ? "press " : "release ") + text.substr(text.find("fired ") + 6);
        }
        else if (starts("note: ") || starts("NOTE: ")) text = text.substr(6);
        else if (starts("prediction off") || starts("one-tick") || starts("orb ") || starts("learned")) tag = "CHECK";
        else if (starts("PRESS ") || starts("REL ")) {
            tag = "HOLD";
            text = (starts("PRESS") ? "press" : "release") + text.substr(text.find(' '));
        }
        pushTagged(std::move(tag), std::move(text), color, minor);
    }

    void pushTagged(std::string tag, std::string text, ccColor3B color, bool minor = false) {
        trace(fmt::format("log {} {}", tag, text));
        m_logVersion++;
        float const t = levelMs() / 1000.f;
        if (LogConsole::on()) LogConsole::write(fmt::format("{:8.2f}s  {:<6}  {}", t, tag, text), logTagColor(tag));
        if (!m_history.empty() && m_history.back().text == text && m_history.back().tag == tag) {
            m_history.back().repeat++;
            m_history.back().t = t;
        } else {
            m_history.push_back(HistEntry{std::move(tag), std::move(text), color, t, 1, minor});
        }
        while (m_history.size() > MAX_HISTORY) m_history.pop_front();
    }

    void notePredictionMiss(float err, float dx = 0.f, float dy = 0.f) {
        Config const& c = cfg();
        if (c.debug) log::debug("[Aceware] prediction off by {:.1f}px at the 100ms check", err);
        (void)c;
        pushLog(fmt::format("prediction off by {:.1f}px [{}] (sideways {:+.1f}, up/down {:+.1f}; you vs predicted)",
                            err, m_trackMode, dx, dy), COL_YELLOW, false);
    }

    DeathReason classifyDeath(long long age) const {
        if (m_last.decision == Decision::None || age < 0) return DeathReason::NoRecentClick;
        if (age > static_cast<long long>(m_last.horizon) + m_last.delay + 8) {
            return DeathReason::NoRecentClick;
        }
        switch (m_last.decision) {
            case Decision::PassSafe:
                return age <= m_last.horizon ? DeathReason::SimMispredict : DeathReason::LaterObstacle;
            case Decision::PassTruncated: return DeathReason::Truncated;
            case Decision::PassSpam:      return DeathReason::SpamBypass;
            case Decision::PassNoFix:     return DeathReason::WindowTooSmall;
            case Decision::PassBudget:    return DeathReason::OutOfBudget;
            case Decision::Buffered:      return DeathReason::BufferedStillDied;
            case Decision::PassEscapable: return DeathReason::EscapeNotTaken;
            default:                      return DeathReason::Other;
        }
    }

    void reportDeath() {
        if (m_lastDeathTick == m_tick) return;
        m_lastDeathTick = m_tick;

        float const dt = getValidDt();
        long long const age = (m_last.time >= 0.0)
            ? static_cast<long long>(std::llround((m_gameTime - m_last.time) / dt)) : -1;
        DeathReason const why = classifyDeath(age);
        m_stats.deaths++;
        m_stats.byDeath[static_cast<int>(why)]++;

        float const ageMs = (age >= 0) ? ticksToMs(age, dt) : -1.f;
        // For "no fix" / "mispredict" the interesting number is predicted vs real death time.
        if (m_logDebug) {
            log::debug("[Aceware] DEATH reason={} lastClick={} ({:.1f}ms ago) sim_f0={}/{} real_age={}t",
                       deathName(why), decisionName(m_last.decision), ageMs,
                       simEndName(m_last.f0End), m_last.f0Ticks, age);
        }
        if (m_logCsv) {
            m_csv.row(fmt::format("DEATH,{:.1f},{},,{},,{},,{},{},{},{},{:.1f}",
                                  levelMs(), m_lastMode, static_cast<int>(std::lround(60.0f / dt)),
                                  m_last.horizon, simEndName(m_last.f0End), m_last.f0Ticks,
                                  deathName(why), age, ageMs));
            m_csv.flush();
        }
        {
            std::string text;
            if (why == DeathReason::WindowTooSmall && m_last.needed > 0) {
                text = fmt::format("DEATH: click was {}t ({:.0f}ms) too early, window {}t", m_last.needed,
                                   ticksToMs(m_last.needed, dt), m_last.window);
            } else if (why == DeathReason::WindowTooSmall && m_last.needed == 0) {
                text = fmt::format("DEATH: click came too late (no delay up to {}t helps){}", m_last.probe,
                                   m_lastMode.empty() ? std::string() : fmt::format(" [{}]", m_lastMode));
            } else if (why == DeathReason::BufferedStillDied) {
                std::string const tag = m_lastMode.empty() ? std::string() : fmt::format(" [{}]", m_lastMode);
                if (m_last.liveTicks < 0 && m_queue.pending()) {
                    // still held when you died: the live check never found it safe
                    text = fmt::format("DEATH: held click hadn't fired yet ({}t after it, planned +{}t){} | live: {}",
                                       age, m_last.delay, tag, refuseSummary());
                } else if (m_last.liveTicks >= 0) {
                    text = fmt::format("DEATH: buffered, fired +{}t ({}), planned +{}t, still died{}", m_last.liveTicks,
                                       fireReasonText(m_last.liveReason), m_last.delay, tag);
                } else {
                    text = fmt::format("DEATH: buffered +{}t still died (prediction was off){}", m_last.delay, tag);
                }
            } else if (why == DeathReason::Truncated && m_last.trunc.valid) {
                text = fmt::format("DEATH: {} cut prediction", objectName(m_last.trunc.type));
            } else if (ageMs >= 0.f && why != DeathReason::NoRecentClick) {
                text = fmt::format("DEATH: {} ({:.0f}ms after click)", deathText(why), ageMs);
            } else {
                text = fmt::format("DEATH: {}", deathText(why));
            }
            pushLog(std::move(text), COL_WHITE, false); // the red DEATH tag says it
        }
    }

    std::string summary() const {
        auto const& st = m_stats;
        auto d = [&](Decision x) { return st.byDecision[static_cast<int>(x)]; };
        auto w = [&](DeathReason x) { return st.byDeath[static_cast<int>(x)]; };
        long long avg = st.simClicks > 0 ? st.sumUs / st.simClicks : 0;
        return fmt::format(
            "clicks={} buffered={} safe={} escape={} truncated={} spam={} nofix={} budget={} | sim avg={}us max={}us scale={:.2f} | "
            "accuracy={:.1f}% of {} (err {:.1f}px, died {}, dropped {}) | "
            "deaths={} [mispredict={} later={} truncated={} spam={} window={} budget={} buffered={} escape={} none={}]",
            st.clicks, d(Decision::Buffered), d(Decision::PassSafe), d(Decision::PassEscapable), d(Decision::PassTruncated),
            d(Decision::PassSpam), d(Decision::PassNoFix), d(Decision::PassBudget),
            avg, st.maxUs, m_horizonScale,
            m_tracker.stats().percent(), m_tracker.stats().checks, m_tracker.stats().avgErr(),
            m_tracker.stats().deaths, m_tracker.stats().dropped,
            st.deaths, w(DeathReason::SimMispredict), w(DeathReason::LaterObstacle),
            w(DeathReason::Truncated), w(DeathReason::SpamBypass), w(DeathReason::WindowTooSmall),
            w(DeathReason::OutOfBudget), w(DeathReason::BufferedStillDied), w(DeathReason::EscapeNotTaken),
            w(DeathReason::NoRecentClick));
    }

    // ------------------------------------------------------------------------
    //  Top-right panel content
    // ------------------------------------------------------------------------
    std::string headerText(Config const& c) const {
        if (!c.enabled) return "ACEWARE  OFF (raw input)";
        if (!m_active)  return "ACEWARE  OFF (practice only)";
        if (c.custom)   return "ACEWARE  CUSTOM";
        if (c.extreme)  return "ACEWARE  EXTREME (BUGGY)";
        // the stage (2.2: it used to just say ON)
        std::string st = STAGES[std::clamp(c.stage, 0, STAGE_COUNT - 1)].setting;
        for (auto& ch : st) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return "ACEWARE  " + st;
    }

    ccColor3B headerColor(Config const& c) const {
        if (!c.enabled || !m_active) return COL_RED;
        if (c.custom)   return COL_CYAN;
        if (c.extreme)  return COL_ORANGE;
        return COL_GREEN;
    }

    std::string statusText(PlayLayer* pl, Config const& c) const {
        PlayerObject* p1 = pl->m_player1;
        if (!p1) return "-";
        if (p1->m_isPlatformer) return "platformer: buffer not used";
        bool const dual = pl->m_gameState.m_isDualMode && pl->m_player2;
        ModeInfo mode;
        mode.add(p1);
        if (dual) mode.add(pl->m_player2);
        float const dt = getValidDt();
        int const tps = static_cast<int>(std::lround(60.0f / dt));
        int const wt = mode.windowTicks(effectiveCfg(c), true, dt);
        std::string s = fmt::format("{}{} | {}tps", mode.name(), dual ? " dual" : "", tps);
        if (m_haveFrame) s += fmt::format(" {}fps", static_cast<int>(std::lround(1e6f / m_frameUs)));
        if (m_cbfLoaded) s += " CBF";
        s += fmt::format(" | {:.2f}f = {}t", static_cast<float>(wt) * dt, wt);
        int const budget = clickBudgetUs(c);
        if (budget > 0) s += fmt::format(" | cpu {:.1f}ms", budget / 1000.0);
        if (m_horizonScale < 0.99f) s += fmt::format(" x{:.2f}", m_horizonScale);
        return s;
    }

    std::string footerText() const {
        auto const& st = m_stats;
        std::string out = fmt::format("clicks {}  buffered {}  deaths {}", st.clicks,
                                      st.byDecision[static_cast<int>(Decision::Buffered)], st.deaths);
        auto const& a = m_tracker.stats();
        if (a.checks > 0) out += fmt::format("  |  pred {:.1f}% ({})", a.percent(), a.checks);
        return out;
    }

    // Badge, overlay fade-out, and making sure the window exists when it should.
    // (The window refreshes its own contents.)
    void refreshHud(PlayLayer* pl, bool) {
        if (!pl) return;
        Config const& c = cfg();
        updateBadge(pl, c);
        if (c.hud) windowEnsure();
    }

public:
    // ------------------------------------------------------------------------
    //  Lifecycle (no heavy work at level load: sim players are made lazily)
    // ------------------------------------------------------------------------
    void init(PlayLayer* pl) {
        releaseSimPlayers(); // stale ones from a previous level, if any
        m_simulating = false;
        m_firingBuffered = false;
        m_horizonScale = 1.0f;
        m_hasDeadline = false;
        m_stats = Stats{};
        m_planFires = m_planDrifts = m_planTicksSaved = 0;
        m_planTrust = false;
        m_liveUsSum = m_liveChecks = m_liveUsMax = 0;
        m_simStepsTotal = m_simUsTotal = 0;
        m_escapeRuns = m_escapeYes = m_escapeNo = m_escapeUnknown = 0;
        m_cheapClicks = 0;
        m_inputsSeen = m_hookMissing = 0;
        m_followOnTime = m_followSpacing = 0;
        m_rayPicks = m_rayConfirmed = m_rayRejected = 0;
        m_simExtraObjs = 0;
        m_oneStep.clear();
        m_osPending = false;
        m_drift.clear();
        m_dfActive = false;
        m_autoStats.clear();
        m_autoNotes.clear();
        m_ap = AutoPlan{};
        m_autoUsed = false; // a new level: its progress counts again (until autoplay plays it)
        m_learn = 0;        // (2.3) a new level: a new learning run
        m_learnReset = false;
        m_learnWhy.clear();
        m_autoDeaths.clear();
        m_autoRuns = m_autoPlans = 0;
        m_autoDriftLogs = 0;
        m_autoFallbackLogged = false;
        m_lagOn = false;
        m_lagSamples.clear();
        m_fireLag = 0;
        m_lagLogged = false;
        m_rtOn = false;
        m_rtDoneLogged = false;
        m_rtRuns = m_rtStarts = 0;
        m_rtStuckLogs = 0;
        m_rtProgressLogs = 0;
        m_rtProgress = 0;
        m_simSpeedSwitches = m_simGravFlips = m_simModeSwitches = m_simMirrors = 0;
        m_lastLiveAt = -1e9;
        m_last = ClickRecord{};
        m_tick = 0;
        m_lastDeathTick = -1;
        m_spots.clear();   // a new level: load what earlier sessions learned on it
        loadSpots(pl);
        releaseScan();
        loadLevelExtras(pl);
        m_outcome.clear();
        m_spotFires = m_spotBlocks = m_spotSaves = 0;
        m_p1LastT = -1.0;
        m_levelStart = Clock::now();
        m_frameDt = BASE_DT;
        m_dtIdx = 0;
        for (float& v : m_dtRing) v = 0.f;
        m_warp = 1.0f;
        m_gameTime = 0.0;
        m_haveFrame = false;
        m_lastFrameAt = Clock::time_point{};
        m_cbfLoaded = Loader::get()->isModLoaded("syzzi.click_between_frames");
        releaseOverlay();
        releaseBadge();
        m_history.clear();
        m_directFires = 0;
        m_directMode = false; // a new level re-tries the normal input path
        for (auto& r : m_holdLen) r.clear();
        for (auto& r : m_gapLen) r.clear();
        m_lastPressAt = m_lastReleaseAt = -1.0;
        m_accByMode.clear();
        m_tracker = acecore::PredictionTracker{};
        invalidateCfg();
        loadPersisted(cfg());
        lmarginLoad(); // (2.3) learned margins
        refreshFlags();
        onReset();
    }

    void quit() {
        if ((m_logDebug || m_logCsv) && m_stats.clicks > 0) {
            std::string text = summary();
            log::info("[Aceware] session: {}", text);
            if (m_logCsv) {
                m_csv.row(fmt::format("SUMMARY,{:.1f},,,,,,,,,,,\"{}\"", levelMs(), text));
            }
        }
        m_csv.close();
        m_outcome.clear(); // leaving: the last input is neither proven nor blamed
        releaseScanView();
        saveAnchors();
        saveSpots();
        m_spotFile.clear();
        saveLevelStats();
        releaseScan();
        savePersisted();
        releaseSimPlayers();
        releaseOverlay();
        releaseBadge();
        releaseRayIndex();
        m_simulating = false;
        m_firingBuffered = false;
        onReset();
    }

    // Level restart / respawn: drop anything pending so the new attempt is clean.
    // ------------------------------------------------------------------------
    //  Orb press check (1.24.3)
    // ------------------------------------------------------------------------
    // "1,2.5,3" -> numbers (saved values only; no sscanf - MSVC flags it).
    template <class... T>
    static bool parseNums(std::string const& v, char sep, T&... out) {
        std::vector<std::string> parts;
        size_t pos = 0;
        while (true) {
            size_t const e = v.find(sep, pos);
            parts.push_back(v.substr(pos, e == std::string::npos ? std::string::npos : e - pos));
            if (e == std::string::npos) break;
            pos = e + 1;
        }
        if (parts.size() != sizeof...(T)) return false;
        size_t i = 0;
        bool ok = true;
        auto one = [&](auto& x) {
            char const* b = parts[i].c_str();
            char* end = nullptr;
            double const d = std::strtod(b, &end);
            if (end == b) ok = false;
            x = static_cast<std::remove_reference_t<decltype(x)>>(d);
            i++;
        };
        (one(out), ...);
        return ok;
    }

    void orbModelLoad() {
        if (m_orbModelLoaded) return;
        m_orbModelLoaded = true;
        orbTableLoad();
    }
    // orb type | gamemode | mini | upside down (before the orb)
    static int orbKey(RingObject* r, PlayerObject* p) {
        int m = 0;
        if (p->m_isBird) m = 1;
        else if (p->m_isSwing) m = 2;
        else if (p->m_isBall) m = 3;
        else if (p->m_isRobot) m = 4;
        else if (p->m_isSpider) m = 5;
        else if (p->m_isShip) m = 6;
        else if (p->m_isDart) m = 7;
        return static_cast<int>(r->m_objectType) * 64 + m * 4 + (p->m_vehicleSize < 0.99f ? 2 : 0) +
               (p->m_isUpsideDown ? 1 : 0);
    }
    static std::string orbKeyName(int key) {
        static char const* modes[8] = {"cube", "ufo", "swing", "ball", "robot", "spider", "ship", "wave"};
        int const type = key / 64, m = (key / 4) % 16;
        return fmt::format("{} ({}{}{})", objectName(static_cast<GameObjectType>(type)), modes[std::clamp(m, 0, 7)],
                           (key & 2) ? ", mini" : "", (key & 1) ? ", upside down" : "");
    }
    void orbTableLoad() {
        std::string const v = Mod::get()->getSavedValue<std::string>("orb-table-v1", "");
        size_t pos = 0;
        while (pos < v.size()) {
            size_t const end = v.find(';', pos);
            std::string const item = v.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            int key = 0, n = 0, flips = 0;
            double sum = 0.0;
            if (parseNums(item, ':', key, sum, n, flips) && n > 0) {
                m_orbTable[key] = OrbFact{sum, n, flips};
            }
            if (end == std::string::npos) break;
            pos = end + 1;
        }
    }
    void orbTableSave() {
        if (!m_orbTableDirty) return;
        m_orbTableDirty = false;
        std::string v;
        for (auto const& [k, f] : m_orbTable) v += fmt::format("{}:{:.4f}:{}:{};", k, f.vySum, f.n, f.flips);
        Mod::get()->setSavedValue<std::string>("orb-table-v1", v);
    }
    // Every orb a sim player uses goes through here: GD's own argument, the
    // learned y-speed correction, and a record for the orb check.
    void simRingJump(PlayerObject* p, RingObject* r) {
        int const key = orbKey(r, p);
        bool const udBefore = p->m_isUpsideDown;
        // No GD ringJump for sim players (1.26.2): it drew the orb's circle effect
        // where the prediction was (a ghost "random orb" next to the real one),
        // and it never moved the sim player anyway (measured in 1.24.5: y-speed
        // stayed a plain flap). The learned orb below is what the game does.
        // Learned from the game: set exactly what this orb really does.
        // Exact entry, else the same orb at the other size, else upside down
        // (speed mirrored) - better than the sim's own orb, which does nothing.
        OrbFact const* fp = nullptr;
        double sign = 1.0;
        for (int alt : {0, 2, 1, 3}) {
            auto it = m_orbTable.find(key ^ alt);
            if (it != m_orbTable.end() && it->second.n > 0) {
                fp = &it->second;
                sign = (alt & 1) ? -1.0 : 1.0;
                break;
            }
        }
        // (2.3) GD's own orb rules (orbModel). The learned table had wrong
        // entries (yellow orb for a mini cube -11.88, blue orb for a cube "no
        // flip" -7.40...): a learned value is only used when it agrees with
        // the rules, else the rules are.
        double mvy = 0.0;
        bool mflip = false;
        bool const model = orbModel(r->m_objectType, p, udBefore, mvy, mflip);
        bool useLearned = fp != nullptr;
        if (fp && model) {
            OrbFact const& f = *fp;
            double const lvy = sign * f.vySum / f.n;
            bool const lflip = f.flips * 2 > f.n;
            if (lflip != mflip || std::fabs(lvy - mvy) > 0.15 * std::fabs(mvy) + 0.3) {
                useLearned = false;
                m_orbLearnedBad++;
            }
        }
        if (useLearned) {
            OrbFact const& f = *fp;
            bool const flip = f.flips * 2 > f.n;
            bool const want = flip ? !udBefore : udBefore;
            if (p->m_isUpsideDown != want) p->flipGravity(want, true);
            p->m_yVelocity = sign * f.vySum / f.n;
            p->m_isOnGround = false;
            m_orbTableUses++;
        } else if (model) {
            bool const want = mflip ? !udBefore : udBefore;
            if (p->m_isUpsideDown != want) p->flipGravity(want, true);
            if (!p->m_isDart) p->m_yVelocity = mvy; // (the wave doesn't keep a y speed)
            p->m_isOnGround = false;
            m_orbModelUses++;
        }
        if (p == m_simPlayer1.data() && m_simOrbStep < 0) {
            m_simOrbStep = m_simTick;
        }
    }
    // (2.3) What an orb does, from GD's PlayerObject::ringJump (as ported in
    // open GD physics remakes, checked against this mod's own learned values
    // where they agree): the y speed is GD's jump speed for the current speed
    // (yStart) times the orb's factor for the gamemode, x0.8 mini, x0.7 ball /
    // spider, x0.6 swing; + is up on screen. Blue / green flip gravity first.
    long long m_orbModelUses = 0, m_orbLearnedBad = 0;
    static bool orbModel(GameObjectType t, PlayerObject* p, bool udBefore, double& vy, bool& flip) {
        if (!isModelledOrb(t)) return false;
        float const sp = p->m_playerSpeed;
        double const yStart = sp < 0.8f ? 10.620032 : sp < 1.0f ? 11.180032 : sp < 1.2f ? 11.420032 : 11.230032;
        bool const mini = p->m_vehicleSize < 0.99f;
        bool const ship = p->m_isShip, ufo = p->m_isBird, ball = p->m_isBall, robot = p->m_isRobot;
        bool const spider = p->m_isSpider, swing = p->m_isSwing, wave = p->m_isDart;
        flip = t == GameObjectType::GravityRing || t == GameObjectType::GreenRing;
        bool const udAfter = flip ? !udBefore : udBefore;
        double const up = udAfter ? -1.0 : 1.0; // "up" for the gravity after the orb, on screen
        double keep = (ball || spider) ? 0.7 : swing ? 0.6 : 1.0;
        double const miniMod = mini ? 0.8 : 1.0;
        double f = 1.0;
        switch (t) {
            case GameObjectType::YellowJumpRing: f = robot ? 0.9 : 1.0; break;
            case GameObjectType::PinkJumpRing: f = ship ? 0.37 : ufo ? 0.42 : ball ? 0.77 : 0.72; break;
            case GameObjectType::RedJumpRing:
                f = ship ? (mini ? 1.4 : 1.0) : ufo ? (mini ? 1.36 : 1.02) : ball ? 1.34 : robot ? 1.28 : spider ? 1.34 : 1.38;
                break;
            case GameObjectType::GravityRing: // blue: flip, then a small push toward the new floor
                vy = -up * 0.4 * yStart * miniMod * keep;
                return true;
            case GameObjectType::GreenRing: f = ship ? 0.7 : 1.0; break;
            case GameObjectType::DropRing: { // black: straight down (toward your gravity), not scaled by size
                double const b = spider ? 16.5 : (ship || wave || swing) ? 14.0 : ufo ? 14.0 * 0.8 : 15.0;
                vy = -up * b;
                return true;
            }
            default: return false;
        }
        vy = up * yStart * f * miniMod * keep;
        return true;
    }
    void noteRealOrb(PlayerObject* p, RingObject* r, bool arg, double before, double after, int key, bool flipped) {
        orbModelLoad();
        if (r && !realUsedOrb(r)) m_realUsedOrbs.push_back(r);
        // Learn what this orb really does (every real orb use counts).
        if (r && isModelledOrb(r->m_objectType) && std::isfinite(after)) {
            OrbFact& f = m_orbTable[key];
            bool const fresh = f.n == 0;
            double const avg = fresh ? after : f.vySum / f.n;
            if (!fresh && std::fabs(after - avg) > std::max(1.0, std::fabs(avg) * 0.25)) {
                // Very different from before (another orb setting / speed): start over.
                f = OrbFact{};
            }
            if (f.n < 50) {
                f.vySum += after;
                f.n++;
                if (flipped) f.flips++;
            }
            m_orbTableDirty = true;
            if (fresh && cfg().hudVerbose) pushLog(fmt::format("learned: {} gives y-speed {:.2f}{}", orbKeyName(key), after,
                                           flipped ? " and flips gravity" : ""), COL_GREEN, false);
        }
        (void)arg; // (the sim no longer calls ringJump: 1.26.2)
        (void)p;
        (void)before;
    }
    void onReset() {
        mvEndAttempt(); // (2.3) what this attempt saw the level do goes into the timeline
        apOnRespawn(PlayLayer::get()); // (2.3) the run's inputs back to where it respawns
        m_autoLastInTick = m_transTick = -1;
        m_transMode = -1;
        m_pending.clear(); // (2.3) your clicks: a fresh attempt
        m_userHeld = false;
        m_forceUntil = -1;
        m_clickAt = 0;
        m_userClickTick = -1;
        m_apAttemptTicks = 0;
        commitAnchors(m_attemptLastX, false);
        if (!scanning()) releaseScanView();
        m_deathReported = false;
        m_osPending = false; // a check from the last attempt vs. the new spawn isn't a check
        m_dfActive = false;
        m_dfSettle = 4;
        m_ap = AutoPlan{};
        m_rtOn = false;
        m_autoSettle = 8; // the game may still move you onto the start, and its start triggers run
        m_routeOff = 0;
        m_attemptPending = true;
        clearBuffer();
        m_lastPressAt = m_lastReleaseAt = -1.0; // a respawn breaks the rhythm
        m_tracker.invalidate();
        m_realUsedOrbs.clear();
        orbTableSave();
        m_physicalHolding = false;
        m_gameHolding = false;
        m_lastInputTime = -1e9;
        m_streamLast = -1e9;
        m_streamShift = 0.0;
        m_streak = 0;
        m_pressAtN = m_pressAtI = 0; // a fresh attempt: the click rate starts over
        m_outcome.clear(); // respawned: the last input is settled (death already counted)
        m_p1LastT = -1.0;
        m_liveWaited = false;
    }

    bool isSimulating() const { return m_simulating; }
    // Real player hit a pad: remember the strength GD used for that pad type.
    void learnPad(int type, float bump) {
        if (type < 0 || type >= 64) return;
        PadLearn& p = m_pads[type];
        if (!p.seen) {
            p.seen = true;
            p.bump = bump;
            m_padsDirty = true;
        } else if (std::fabs(p.bump - bump) > 1e-3f && !p.unstable) {
            p.unstable = true;
            m_padsDirty = true;
        }
    }

    // Player and orb hitboxes overlap right now (with a little slack: GD's orb
    // area is a bit more generous than the box).
    static bool overlapsNow(PlayerObject* p, GameObject* o) {
        if (!p || !o) return false;
        CCRect const& b = o->getObjectRect();
        if (b.size.width < 1.f || b.size.height < 1.f || b.size.width > 260.f || b.size.height > 520.f)
            return true; // no usable box: trust GD's contact
        // the player's box from its position (its cached rect can be a step old)
        float const ph = 15.f * std::clamp(p->m_vehicleSize, 0.3f, 1.3f);
        float const px = p->getPositionX(), py = p->getPositionY();
        float const pad = 4.f;
        return px - ph - pad <= b.origin.x + b.size.width && b.origin.x - pad <= px + ph &&
               py - ph - pad <= b.origin.y + b.size.height && b.origin.y - pad <= py + ph;
    }

    bool simOrbAvailable(RingObject* r) const {
        if (!r || realUsedOrb(r) || static_cast<GameObject*>(r) == m_simOrbObj) return false;
        // (2.3) not m_isDisabled: that's GD's "off screen" - the route plans
        // ahead and has to be able to use orbs out there too
        return r->isVisible() && !r->m_isGroupDisabled;
    }

    void snapshotObj(GameObject* o) {
        for (auto const& e : m_objRestore) if (e.first == o) return;
        m_objRestore.emplace_back(o, o->m_isActivated);
    }

    bool realUsedOrb(GameObject* o) const {
        return std::find(m_realUsedOrbs.begin(), m_realUsedOrbs.end(), o) != m_realUsedOrbs.end();
    }

    // GD lists this orb as touched by the player right now.
    static bool touchingOrbObj(PlayerObject* p, RingObject* r) {
        if (!p || !r || !p->m_touchingRings) return false;
        unsigned const n = p->m_touchingRings->count();
        for (unsigned i = 0; i < n; i++) {
            if (p->m_touchingRings->objectAtIndex(i) == r) return true;
        }
        return false;
    }

    // The modelled orb a (real) player is touching right now, if any.
    static RingObject* touchingOrb(PlayerObject* p) {
        if (!p || !p->m_touchingRings) return nullptr;
        unsigned const n = p->m_touchingRings->count();
        for (unsigned i = 0; i < n; i++) {
            auto* r = typeinfo_cast<RingObject*>(p->m_touchingRings->objectAtIndex(i));
            if (r && isModelledOrb(r->m_objectType)) return r;
        }
        return nullptr;
    }

    // UFO / swing press while already touching an orb: GD uses the orb for that
    // press on the same tick, and there's no flap (1.24.1). The sim used to
    // flap first and use the orb a tick later, starting the orb arc late and
    // high - "no fix ... hits object at +22px, after using an orb".
    bool orbPress(PlayerObject* s1, PlayerObject* s2) {
        if (!m_orbPredict || m_simOrbUsed) return false;
        if (!(s1->m_isBird || s1->m_isSwing)) return false;
        RingObject* r = m_prevRing[0];
        if (!r || static_cast<GameObject*>(r) == m_simOrbObj) return false;
        if (realUsedOrb(r)) return false;     // you already used it this attempt
        if (!overlapsNow(s1, r)) return false; // a stale "touching" entry
        m_objRestore.emplace_back(r, r->m_isActivated);
        m_simOrbUsed = true;
        m_simOrbObj = r;
        simRingJump(s1, r);
        if (s2 && m_prevRing[1] && m_prevRing[1] != r) simRingJump(s2, m_prevRing[1]);
        m_simHeld = true;          // the button is down, but the press went into the orb
        m_simPressTick = m_simTick;
        return true;
    }

    // GD's collision says a sim player overlaps this orb (exact hitbox).
    void simRingTouched(PlayerObject* player, RingObject* ring) {
        int const idx = (player == m_simPlayer2.data()) ? 1 : 0;
        m_gdRings = true;
        if (idx == 0 && !m_simFirstRing && !realUsedOrb(ring) && isModelledOrb(ring->m_objectType)) {
            m_simFirstRing = ring;
            m_simFirstRingTick = m_simTick;
            m_simFirstRingLast = m_simTick;
        } else if (idx == 0 && ring == m_simFirstRing && m_simTick == m_simFirstRingLast + 1) {
            m_simFirstRingLast = m_simTick;
        }
        if (!m_tickRing[idx] && static_cast<GameObject*>(ring) != m_simOrbObj) m_tickRing[idx] = ring;
    }

    // After a sim player's collision step: use the orb GD reported, under the
    // same rules as before (pressed - a fresh click in UFO / swing - within the
    // click's intent window, one orb per test).
    void applyGdRing(PlayerObject* player, int idx) {
        RingObject* ring = m_tickRing[idx];
        m_tickRing[idx] = nullptr;
        if (!ring || m_simOrbUsed || !m_orbPredict || !isModelledOrb(ring->m_objectType)) return;
        bool const clickOnly = player->m_isBird || player->m_isSwing;
        bool const freshPress = m_simHeld && m_simPressTick >= 0 && m_simTick - m_simPressTick <= m_orbClick;
        bool const pressed = clickOnly ? freshPress : (m_simHeld || player->m_jumpBuffered);
        int const since = m_simTick - m_simDelay;
        if (!pressed || since < 0 || since > m_orbIntent) return;
        m_objRestore.emplace_back(ring, ring->m_isActivated);
        m_simOrbUsed = true;
        m_simOrbObj = ring;
        simRingJump(player, ring);
    }

    bool isSimPlayer(PlayerObject* p) const {
        return p && (p == m_simPlayer1.data() || p == m_simPlayer2.data());
    }
    bool isFiringBuffered() const { return m_firingBuffered; }

    // True if `player` is one of our sim players (swallow the death).
    // True if `player` is one of our sim players (their "death" is swallowed).
    bool checkSimulationDeath(PlayerObject* player, GameObject* killer = nullptr) {
        if (m_simulating && (player == m_simPlayer1.data() || player == m_simPlayer2.data())) {
            if (m_scanNoclip) return true; // scan "going through" a spot: deaths don't count
            // (2.3) GD's anticheat spike: an invisible spike GD puts where you
            // start, to catch noclip - it never kills anyone (GD ignores it in
            // destroyPlayer). The hidden player "died" on it at every level's
            // start: false deaths, and autoplay's route stuck at x 0.
            if (killer) {
                PlayLayer* pl = PlayLayer::get();
                if (pl && killer == pl->m_anticheatSpike) {
                    m_simAnticheat++;
                    return true;
                }
            }
            if (!m_simDead) {
                // (its collision place - GD's drawn position can be stale off screen)
                m_simKillPos = killer ? CCPoint{static_cast<float>(killer->m_positionX), static_cast<float>(killer->m_positionY)}
                                      : player->getPosition();
                m_simKillType = killer ? static_cast<int>(killer->m_objectType) : -1;
                m_simKillDy = player->getPositionY() - m_simStartY;
                m_simKillPen = -1.f;
                if (killer) {
                    CCRect const& a = player->getObjectRect();
                    CCRect const& b = killer->getObjectRect();
                    float const ox = std::min(a.getMaxX(), b.getMaxX()) - std::max(a.getMinX(), b.getMinX());
                    float const oy = std::min(a.getMaxY(), b.getMaxY()) - std::max(a.getMinY(), b.getMinY());
                    m_simKillPen = std::max(0.f, std::min(ox, oy));
                }
            }
            m_simDead = true;
            return true;
        }
        return false;
    }

    // The real player really died (called once per death; see the hook and
    // onPlayerUpdate). GD can call destroyPlayer every tick while touching a
    // hazard (practice mode, noclip-style mods), which is not a death.
    void onRealDeath() {
        if (m_deathReported) return;
        m_deathReported = true;
        lmarginOnDeath(); // (2.3) a moved click that died right after: aim deeper next time
        if (PlayLayer* dpl = PlayLayer::get()) {
            m_lvlStats.deaths++;
            m_lifeStats.deaths++;
            if (!dpl->m_isPracticeMode) m_lvlStats.best = std::max(m_lvlStats.best, dpl->getCurrentPercent());
            if (PlayerObject* dp = dpl->m_player1) {
                trace(fmt::format("death at ({:.1f},{:.1f}) {:.1f}%", dp->getPositionX(), dp->getPositionY(),
                                  dpl->getCurrentPercent()));
            }
            saveLevelStats();
        }
        if (cfg().learnSpots) {
            finishOutcome(m_outcome.death(m_gameTime, m_outcomeWindow));
            saveSpots(); // a death is a natural pause: write what was learned
        }
        if (m_tracker.active()) {
            AccCount& a = m_accByMode[m_trackMode];
            a.checks++;
            a.deaths++;
        }
        m_tracker.died(); // a pending check predicted you'd be alive
        reportDeath();
        onReset();
        m_deathReported = true; // onReset clears it; stay reported until respawn
    }

    // Called from the collision hook while simulating: only solid/hazard-type
    // objects are collided with; state-changing objects may end the lookahead
    // (see checkStateChange for exactly when).
    gd::vector<GameObject*>& filterForSim(PlayerObject* player, gd::vector<GameObject*>* vec, int count) {
        m_scratch.clear();
        if (!vec) return m_scratch;
        int const n = std::min<int>(count, static_cast<int>(vec->size()));
        SimTrack const& tr = m_track[(player == m_simPlayer2.data()) ? 1 : 0];
        // (2.2) what GD handed this sim player this tick (so the extras below
        // never repeat one)
        if (m_passTick != m_simTick || m_passPlayer != player || m_passStep != m_simStepsTotal) {
            m_passTick = m_simTick;
            m_passPlayer = player;
            m_passStep = m_simStepsTotal;
            m_passed.clear(); // (a flat list: a few dozen objects, cheaper than a hash set every tick)
            m_passAdded.clear();
            m_passExtra = false;
        }
        auto take = [&](GameObject* obj) {
            switch (classify(obj->m_objectType)) {
                case ObjClass::Collide:
                    // (2.3) GD's object check skips anything GD has switched
                    // off - and GD switches off what's outside the area around
                    // the camera. So far ahead the hidden player went straight
                    // through blocks and spikes: no landing (Every End's 6 px
                    // slabs: "you on block 1220, the route in the air") and
                    // every route there "died on a spike below". For the
                    // hidden player's step only, those blocks / spikes / slopes
                    // count as switched on (put back right after the step).
                    // A group a trigger turned off stays off.
                    if (obj->m_isDisabled && !obj->m_isGroupDisabled) {
                        obj->m_isDisabled = false;
                        m_undisabled.push_back(obj);
                        m_simUndisabledN++;
                    }
                    if (obj->m_isDisabled2 && !obj->m_isGroupDisabled) {
                        obj->m_isDisabled2 = false;
                        m_undisabled2.push_back(obj);
                        m_simUndisabledN2++;
                    }
                    // Hazards: how close the path comes (clearance; current engine)
                    if (m_simModern && isHazard(obj->m_objectType)) hazardClearance(player, obj);
                    m_scratch.push_back(obj);
                    break;
                case ObjClass::StateChange:
                    // Orbs the sim can use go to GD's own collision as well, which
                    // reports the exact tick the player overlaps them (its real
                    // orb hitbox) through playerTouchedRing (1.23.0).
                    if (m_orbPredict && isModelledOrb(obj->m_objectType) && obj != m_simOrbObj) {
                        // GD marks an orb activated as soon as a player TOUCHES it.
                        // Remember its state before GD sees it, so the test leaves
                        // it exactly as it was (1.25.0: it used to be "restored"
                        // to activated, and every later test ignored that orb).
                        snapshotObj(obj);
                        m_scratch.push_back(obj);
                    }
                    if (!m_simTrunc.valid) checkStateChange(player, obj, tr);
                    break;
                default:
                    break;
            }
        };
        // (2.3) moving blocks / spikes: where they will be at this step, first
        // (GD's own list has them where they are now - skipped below)
        if (mvPassFirst(player)) {
            mvApply(player, [&](GameObject* o) {
                m_passAdded.push_back(o);
                take(o);
            });
        }
        bool const anyMv = !m_mvIdx.empty();
        for (int i = 0; i < n; i++) {
            GameObject* obj = (*vec)[i];
            if (!obj) continue;
            if (anyMv && isMover(obj)) continue; // (2.3) done above, at its place then
            // already added below on an earlier call this tick: don't check it twice
            if (!m_passAdded.empty() && std::find(m_passAdded.begin(), m_passAdded.end(), obj) != m_passAdded.end()) continue;
            m_passed.push_back(obj);
            take(obj);
        }
        // (2.2) Blocks / spikes / slopes right around the hidden player that GD
        // didn't hand over: GD only gives a player the objects of the level
        // sections it has loaded for collisions, and the hidden player was
        // missing some - invisible floors it fell straight through in every
        // prediction (and the scan / pathfinder) while you stood on them. Every
        // solid, hazard and slope near it now comes from the mod's own list of
        // the level, once per tick.
        if (m_inCheck == player) m_ccoInCheck = true;
        if (!m_passExtra && !m_rayObjs.empty()) {
            m_passExtra = true;
            CCRect const& pr = player->getObjectRect();
            float const lo = pr.getMinX() - SIM_EXTRA_PAD, hi = pr.getMaxX() + SIM_EXTRA_PAD;
            float const ylo = pr.getMinY() - SIM_EXTRA_PAD, yhi = pr.getMaxY() + SIM_EXTRA_PAD;
            auto it = std::lower_bound(m_rayObjs.begin(), m_rayObjs.end(), lo - m_rayMaxW,
                                       [](RayObj const& a, float v) { return a.x0 < v; });
            for (; it != m_rayObjs.end() && it->x0 <= hi; ++it) {
                GameObject* o = it->obj;
                // (2.3) m_isDisabled: GD switches off what's outside the area
                // it's running (off screen) - it's still part of the level. Only
                // a group toggled off is gone. (Skipping m_isDisabled left the
                // hidden player blind off screen: no floor, no spikes.)
                if (it->x1 < lo || o->m_isGroupDisabled) continue;
                if (it->mv >= 0 && m_mv[static_cast<size_t>(it->mv)].ahead) continue; // (2.3: a mover: mvApply's)
                if (o->m_isDisabled) m_simOffscreenUsed++;
                CCRect const& r = o->getObjectRect();
                if (r.getMaxX() < lo || r.getMinX() > hi || r.getMaxY() < ylo || r.getMinY() > yhi) continue;
                if (std::find(m_passed.begin(), m_passed.end(), o) != m_passed.end()) continue; // GD gave it already
                m_simExtraObjs++;
                m_passAdded.push_back(o);
                take(o);
            }
        }
        return m_scratch;
    }
    // (2.3) GD is about to finish a hidden player's collision step
    // (postCollision: landing, ground, what it touched). If GD gave it no
    // objects at all this tick - nothing of the level is loaded out there,
    // GD only loads the sections around the camera - the mod's own list goes
    // in NOW, inside GD's step, so landing on a block works the way it does
    // for you. It used to go in after GD's step had finished: off screen the
    // hidden player sank into blocks instead of standing on them (Every End's
    // floor slabs - autoplay's route fell through the floor, and everything
    // past the screen looked deadly).
    void beforeSimPostCollision(PlayerObject* p, float dt) {
        if (!m_simulating || !isSimPlayer(p)) return;
        if (m_inCheck == p) m_postInCheck++;
        else m_postOutside++;
        PlayLayer* pl = PlayLayer::get();
        if (!pl) return;
        // (2.3) GD handed this hidden player NO objects this step (it only
        // loads the level's sections near the camera): GD's object check never
        // ran, so blocks / spikes from the mod's list never got GD's block
        // collision - they were only "checked" after the step, too late to
        // land on. Here (inside GD's step, before it settles the landing) the
        // nearby blocks get GD's own block collision and spikes kill.
        if (m_inCheck == p && !m_ccoInCheck) {
            m_ccoSkipped++;
            resolveOwnObjects(pl, p, dt);
            return;
        }
        m_simInlineExtra++;
        extraCollisions(pl, p, dt);
    }
    long long m_simInlineExtra = 0;
    PlayerObject* m_inCheck = nullptr; // the hidden player inside GD's checkCollisions right now
    bool m_ccoInCheck = false;         // GD's object check ran for it during this checkCollisions
    long long m_postInCheck = 0, m_postOutside = 0, m_ccoSkipped = 0, m_ownSolid = 0, m_ownHazard = 0;
    long long m_pathSteps = 0, m_pathCco = 0;
    void noteCollisionPath(PlayerObject*) {
        m_pathSteps++;
        if (m_ccoInCheck) m_pathCco++;
    }
    void resolveOwnObjects(PlayLayer* pl, PlayerObject* p, float dt) {
        if (m_rayObjs.empty()) return;
        // (2.3) moving blocks / spikes, where they will be then
        if (mvPassFirst(p)) {
            mvApply(p, [&](GameObject* o) {
                if (m_simDead) return;
                CCRect const& r = o->getObjectRect();
                if (o->m_objectType == GameObjectType::Solid) {
                    p->collidedWithObject(dt, o, r, false);
                    m_ownSolid++;
                } else if (isHazard(o->m_objectType) && p->getObjectRect().intersectsRect(r)) {
                    m_ownHazard++;
                    pl->destroyPlayer(p, o);
                }
            });
        }
        CCRect const& pr = p->getObjectRect();
        float const lo = pr.getMinX() - SIM_EXTRA_PAD, hi = pr.getMaxX() + SIM_EXTRA_PAD;
        float const ylo = pr.getMinY() - SIM_EXTRA_PAD, yhi = pr.getMaxY() + SIM_EXTRA_PAD;
        auto it = std::lower_bound(m_rayObjs.begin(), m_rayObjs.end(), lo - m_rayMaxW,
                                   [](RayObj const& a, float v) { return a.x0 < v; });
        for (; it != m_rayObjs.end() && it->x0 <= hi && !m_simDead; ++it) {
            GameObject* o = it->obj;
            if (it->x1 < lo || o->m_isGroupDisabled) continue;
            if (it->mv >= 0 && m_mv[static_cast<size_t>(it->mv)].ahead) continue;
            CCRect const& r = o->getObjectRect();
            if (r.getMaxX() < lo || r.getMinX() > hi || r.getMaxY() < ylo || r.getMinY() > yhi) continue;
            if (o->m_objectType == GameObjectType::Solid) {
                p->collidedWithObject(dt, o, r, false);
                m_ownSolid++;
            } else if (isHazard(o->m_objectType)) {
                if (p->getObjectRect().intersectsRect(r)) {
                    m_ownHazard++;
                    pl->destroyPlayer(p, o); // (a hidden player: only marks the test dead)
                }
            }
        }
    }
    std::vector<GameObject*> m_undisabled; // (2.3) switched on for this hidden-player step only
    long long m_simUndisabledN = 0;
    std::vector<GameObject*> m_undisabled2;
    long long m_simUndisabledN2 = 0;
    void restoreUndisabled() {
        for (GameObject* o : m_undisabled) o->m_isDisabled = true;
        m_undisabled.clear();
        for (GameObject* o : m_undisabled2) o->m_isDisabled2 = true;
        m_undisabled2.clear();
    }
    // GD skipped its object check for this hidden player this tick (nothing in
    // its loaded sections): run it with just the mod's own nearby objects.
    // (2.3: normally from inside GD's step, see beforeSimPostCollision; this
    // call after it is only the fallback.)
    void extraCollisions(PlayLayer* pl, PlayerObject* p, float dt) {
        if (m_rayObjs.empty() || (m_passPlayer == p && m_passStep == m_simStepsTotal && m_passExtra)) return;
        static gd::vector<GameObject*> none;
        none.clear();
        pl->collisionCheckObjects(p, &none, 0, dt);
    }
    std::vector<GameObject*> m_passed;    // objects GD handed over this tick (this sim player)
    std::vector<GameObject*> m_passAdded; // ... and the ones added from the mod's list
    int  m_passTick = -1;
    long long m_passStep = -1;
    PlayerObject* m_passPlayer = nullptr;
    bool m_passExtra = false;
    long long m_simExtraObjs = 0; // objects added that GD's collision hadn't handed over

    static bool isHazard(GameObjectType t) {
        return t == GameObjectType::Hazard || t == GameObjectType::AnimatedHazard;
    }

    static acecore::Box toBox(CCRect const& r) {
        return acecore::Box{r.origin.x, r.origin.y, r.size.width, r.size.height};
    }

    // One hazard in one sim step: how close the player's box comes to it.
    void hazardClearance(PlayerObject* player, GameObject* obj) {
        float const gap = acecore::boxGap(toBox(player->getObjectRect()), toBox(obj->getObjectRect()));
        if (gap >= 0.f) m_tickClr = std::min(m_tickClr, gap);
    }

    // ---- spot memory helpers ---------------------------------------------------
    static uint32_t spotKey(std::string const& modeName, bool push, bool upsideDown) {
        uint32_t h = 2166136261u; // FNV-1a
        for (char ch : modeName) { h ^= static_cast<uint8_t>(ch); h *= 16777619u; }
        h ^= (push ? 1u : 0u) | (upsideDown ? 2u : 0u);
        h *= 16777619u;
        return h;
    }

    // Match tolerance: x about half a tick of travel.
    acecore::SpotTol spotTol() const {
        acecore::SpotTol t;
        t.x = std::clamp(0.5f * m_p1Speed * getValidDt(), 0.3f, 4.f);
        return t;
    }

    acecore::Spot spotNow(PlayLayer* pl, bool push, std::string const& modeName) const {
        PlayerObject* p = pl->m_player1;
        acecore::Spot s;
        s.x = p->getPositionX();
        s.y = p->getPositionY();
        s.vy = static_cast<float>(p->m_yVelocity);
        s.key = spotKey(modeName, push, p->m_isUpsideDown);
        s.anyKey = spotKey("any", push, p->m_isUpsideDown); // same press, any gamemode
        return s;
    }

    void finishOutcome(std::optional<acecore::OutcomeTracker::Done> const& d) {
        if (!d) return;
        m_spots.add(d->spot, d->good, spotTol());
        m_spotsDirty = true;
    }

    // ---- session memory (this level, this session only) -------------------------
    // Nothing is read from or written to disk: the memory starts empty when a level
    // opens and is gone when you leave it. (Older builds kept a spots/ file per level.)
    void loadSpots(PlayLayer* pl) {
        m_spotFile.clear();
        m_spotsLoaded = 0;
        m_spotsDirty = false;
        m_spots.clear();
        m_spotLevelName = (pl && pl->m_level) ? pl->m_level->m_levelName : std::string();
    }

    void saveSpots() {
        m_spotsDirty = false; // session-only: deliberately not persisted
    }

public:
    // Stage tab "Forget this level": wipe what was learned here (and its file).
    void forgetSpots() {
        m_spots.clear();
        m_outcome.clear();
        m_spotsLoaded = 0;
        m_spotsDirty = false;
        if (!m_spotFile.empty()) {
            std::error_code ec;
            std::filesystem::remove(m_spotFile, ec);
        }
        Notification::create("Aceware - forgot this level", NotificationIcon::None, 1.5f)->show();
    }
    bool inLevelWithSpots() const { return !m_spotFile.empty(); }
    // Forget Scan (2.2, Tools): this level's scanned route, its guide path,
    // what's marked scanned and your saved runs - in memory and on disk.
    bool forgetScan() {
        if (m_routeFile.empty()) return false;
        releaseScan(); // stops a running scan too
        releaseScanView();
        m_route.clear();
        m_routePath.clear();
        m_scanned.clear();
        m_anchors.clear();
        m_attemptAnchors.clear();
        m_anchorLastX = -1e9f;
        m_anchorsDirty = false;
        m_scanDeathPts.clear();
        m_scanSkipRanges.clear();
        m_scanMsg.clear();
        std::error_code ec;
        auto const dir = m_routeFile.parent_path();
        std::filesystem::remove(m_routeFile, ec);
        std::filesystem::remove(dir / (m_lvlKey + "-path.txt"), ec);
        std::filesystem::remove(dir / (m_lvlKey + "-done.txt"), ec);
        std::filesystem::remove(anchorsFile(), ec);
        if (auto const f = apFile(); !f.empty()) std::filesystem::remove(f, ec); // (2.3) what autoplay remembers too
        m_mem.clear();
        m_memEnd = -1e9f;
        m_memPlaying = false;
        if (auto const f = clicksFile(); !f.empty()) std::filesystem::remove(f, ec); // (2.3) your kept clicks too
        m_clicks.clear();
        trace("scan forgotten for this level");
        return true;
    }

    // An input reached the game (raw or buffered): it is now the one that gets
    // the credit or the blame.
    void rememberInput(PlayLayer* pl, bool push) {
        if (!cfg().learnSpots || !pl || !pl->m_player1 || pl->m_player1->m_isDead) return;
        ModeInfo mode;
        mode.add(pl->m_player1);
        // Every gamemode records now: the memory is per level and per session, so it
        // cannot go stale the way a spots file from an older route used to.
        // Presses only. A death in ship / wave gets blamed on whatever input came
        // last, which is often a release that wasn't the problem; learned
        // "bad releases" then held your good releases back.
        if (!push) return;
        // A cube-type press in the air doesn't jump (until you land): it isn't a
        // spot to repeat. (Orb clicks in the air are left out too, on purpose.)
        bool const cubeOnly = mode.cubeLike && !(mode.ship || mode.wave || mode.ufo || mode.swing || mode.robot);
        if (push && cubeOnly && !pl->m_player1->m_isOnGround) return;
        finishOutcome(m_outcome.input(spotNow(pl, push, mode.name()), m_gameTime));
        m_outcomeLand = !mode.noLanding();
        m_outcomeWindow = m_outcomeLand ? SPOT_WINDOW_LAND_MAX : SPOT_WINDOW_HOLD;
        m_outcomeAir = false;
        m_outcomeLandAt = -1.0;
    }

    // Every real P1 step: speed for the tolerances, and settle the pending input.
    void spotStep(PlayerObject* p) {
        float const x = p->getPositionX();
        if (m_p1LastT >= 0.0) {
            double const gap = m_gameTime - m_p1LastT;
            if (gap > 0.05 && gap < 3.0) {
                float const v = static_cast<float>((x - m_p1LastX) / gap);
                if (std::fabs(v) < 200.f) {
                    m_p1Speed = std::fabs(v);
                    if (std::fabs(v) > 0.1f) m_p1Dir = v >= 0.f ? 1.f : -1.f;
                }
                m_p1LastX = x;
                m_p1LastT = m_gameTime;
            }
        } else {
            m_p1LastX = x;
            m_p1LastT = m_gameTime;
        }
        if (!m_outcome.pending()) return;
        if (m_outcomeLand) {
            if (!p->m_isOnGround) { m_outcomeAir = true; m_outcomeLandAt = -1.0; }
            else if (m_outcomeAir && m_outcomeLandAt < 0.0) m_outcomeLandAt = m_gameTime;
            if (m_outcomeLandAt >= 0.0 && m_gameTime - m_outcomeLandAt > SPOT_LAND_GRACE) {
                finishOutcome(m_outcome.poll(m_gameTime, -1.0)); // landed and stayed alive: good
                return;
            }
        }
        finishOutcome(m_outcome.poll(m_gameTime, m_outcomeWindow));
    }

    // How much room this gamemode's paths need (px), from its measured error.
    float clearTarget(std::string const& mode) const {
        auto it = m_accByMode.find(mode);
        if (it == m_accByMode.end()) return CLEAR_DEFAULT_PX;
        return acecore::clearanceTarget(it->second.checks, it->second.errSum, CLEAR_MIN_CHECKS,
                                       CLEAR_DEFAULT_PX, CLEAR_MIN_PX, CLEAR_MAX_PX, CLEAR_ERR_SCALE);
    }

    // Called after every real P1 update (full tick or CBF sub-step) with its length.
    void setFrameDelta(PlayLayer* pl, float dt) {
        float const warp = pl->m_gameState.m_timeWarp;
        if (warp > 0.01f && warp < 10.f) m_warp = warp;
        float const d = (warp > 0.f) ? dt / warp : dt;
        if (!(d > 0.f && d < 2.0f)) return;
        m_lastStepD = d;
        m_gameTime += d; // every step counts, however short
        // Where we are inside the current physics tick (1.23.3). Click Between
        // Frames splits a tick at your click; the pieces add up to one tick.
        {
            float const nom = getValidDt();
            m_stepAccum += d;
            if (m_stepAccum >= nom * 0.98f) m_stepAccum -= nom;
            if (m_stepAccum < nom * 0.02f || m_stepAccum < 0.f) m_stepAccum = 0.f;
            m_phase = std::clamp(m_stepAccum / nom, 0.f, 1.f);
        }
        if (d < 0.0005f) return; // too short to say anything about the nominal tick
        m_dtRing[m_dtIdx] = d;
        m_dtIdx = (m_dtIdx + 1) % DT_RING;
        // Click-Between-Frames can insert shorter sub-steps; the nominal tick is the longest recent one.
        float mx = 0.f;
        for (float v : m_dtRing) mx = std::max(mx, v);
        m_frameDt = mx;
    }

    // Called once per rendered frame: smoothed wall-clock frame time for the auto budget.
    // Once per rendered frame of a running level: counts frames without a physics
    // step (the real "hook isn't running" signal).
    void noteFrame(PlayLayer* pl) {
        if (!pl || pl->m_isPaused || !pl->m_player1 || pl->m_player1->m_isDead) { m_framesSinceStep = 0; return; }
        if (m_framesSinceStep < 1000000) m_framesSinceStep++;
    }

    void onFrame() {
        auto const now = Clock::now();
        if (m_haveFrame || m_lastFrameAt.time_since_epoch().count() != 0) {
            float const us = std::chrono::duration<float, std::micro>(now - m_lastFrameAt).count();
            // Ignore pauses, loading hitches and the like.
            if (us > 500.f && us < 100000.f) {
                m_frameUs = m_haveFrame ? (m_frameUs * 0.95f + us * 0.05f) : us;
                m_haveFrame = true;
            }
        }
        m_lastFrameAt = now;
        m_apFrameSpent = 0; // (2.3) autoplay's planning time: per frame
    }

    // ------------------------------------------------------------------------
    //  Hotkeys
    // ------------------------------------------------------------------------
    // Set the stage (0 = off, 1-4, 5 = extreme). Written into the setting, so
    // the settings screen, F6 and the window always agree.
    // notify: the "Aceware - Stage 3 ..." banner - only for the F6 key
    // (2.1); the window shows the stage itself.
    void setStage(int stage, bool notify = false) {
        stage = std::clamp(stage, 0, STAGE_COUNT - 1);
        Mod::get()->setSettingValue<std::string>("stage", STAGES[stage].setting);
        // (2.2) Custom Min / Max are Custom's own: any other stage puts them back
        if (stage != STAGE_CUSTOM) {
            Mod::get()->setSavedValue<double>("min-ticks", 1.0);
            Mod::get()->setSavedValue<double>("max-ticks", 1.0); // (2.3: Custom Max starts at 1 too)
        }
        invalidateCfg();
        refreshFlags();
        clearBuffer();
        if (notify) Notification::create(fmt::format("Aceware - {}", STAGES[stage].name), NotificationIcon::None, 1.2f)->show();
        log::info("[Aceware] {}", STAGES[stage].name);
        refreshHud(PlayLayer::get(), true);
    }

    int currentStage() { return cfg().stage; }

    // Cycle key (F6): Stage 1 -> 2 -> 3 -> 4 -> Extreme -> Custom -> Off -> Stage 1.
    // Reads the stage directly, so it can never lose track of where it is.
    void cyclePreset() {
        int const cur = Config::load().stage;
        int const next = (cur == STAGE_COUNT - 1) ? 0 : cur + 1;
        setStage(next, true);
    }

    // ------------------------------------------------------------------------
    //  Data for the window (BufferWindow)
    // ------------------------------------------------------------------------
    using Row = std::pair<std::string, ccColor3B>;

    // Newest first. Safe passes / irrelevant releases only if `showMinor`.
    std::vector<Row> historyRows(bool showMinor) const {
        std::vector<Row> rows;
        rows.reserve(m_history.size() + 1);
        for (auto it = m_history.rbegin(); it != m_history.rend(); ++it) {
            if (it->minor && !showMinor) continue;
            // '\x02' time '\x1f' tag '\x1f' text: the window draws three columns.
            std::string line = fmt::format("\x02{:.1f}s\x1f{}\x1f{}", it->t, it->tag, it->text);
            if (it->repeat > 1) line += fmt::format("  x{}", it->repeat);
            rows.emplace_back(std::move(line), it->color);
        }
        return rows; // empty: the window shows what will appear here
    }

    void clearHistory() { m_history.clear(); m_logVersion++; }
    // (2.3) the Log is empty: why (for the empty-Log text)
    int historyHidden() const {
        int n = 0;
        for (auto const& h : m_history) if (h.minor) n++;
        return n;
    }
    long long clicksSeen() const { return m_inputsSeen; }

    // ---- Dev tab (2.2): what's going on right now, for working on the mod ----
    ClickNote   m_devNote;
    std::string m_devNoteMode;
    double      m_devNoteAt = 0.0;
    bool        m_devNoteHave = false;

    std::vector<Row> devRows() {
        std::vector<Row> rows;
        auto head = [&](std::string const& t) {
            if (!rows.empty()) rows.emplace_back("", COL_WHITE);
            rows.emplace_back("\x01" + t, COL_WHITE);
        };
        auto kv = [&](std::string const& k, std::string const& v, ccColor3B col = COL_WHITE) { rows.emplace_back(k + "\t" + v, col); };
        float const dt = getValidDt();
        PlayLayer* pl = PlayLayer::get();
        PlayerObject* p = pl ? pl->m_player1 : nullptr;
        head("Player");
        if (p) {
            ModeInfo mi;
            mi.add(p);
            kv("Mode", fmt::format("{}{}{}", mi.name(), p->m_isUpsideDown ? ", upside down" : "",
                                   pl->m_gameState.m_isDualMode ? ", dual" : ""));
            kv("Position", fmt::format("x {:.1f}  y {:.1f}", p->getPositionX(), p->getPositionY()));
            kv("Y speed", fmt::format("{:.3f}", p->m_yVelocity));
            kv("On ground", p->m_isOnGround ? "yes" : "no");
            kv("Holding (game / button)", fmt::format("{} / {}", m_gameHolding ? "yes" : "no", m_physicalHolding ? "yes" : "no"));
            kv("Speed / size", fmt::format("{:.2f} / {:.2f}", p->m_playerSpeed, p->m_vehicleSize));
            kv("Dead", p->m_isDead ? "yes" : "no", p->m_isDead ? COL_ORANGE : COL_WHITE);
        } else {
            kv("Level", "not in a level");
        }
        kv("TPS / FPS", fmt::format("{} / {}", static_cast<int>(std::lround(60.0f / dt)),
                                    static_cast<int>(std::lround(1e6f / std::max(1.f, m_frameUs)))));
        head("Last input");
        if (m_devNoteHave) {
            ClickNote const& n = m_devNote;
            kv("Input", fmt::format("{} [{}], {:.2f}s ago", n.push ? "press" : "release", m_devNoteMode,
                                    std::max(0.0, (m_gameTime - m_devNoteAt) / 60.0)));
            kv("Decision", decisionName(n.d), n.d == Decision::Buffered ? COL_GREEN : COL_WHITE);
            kv("Delay / window", fmt::format("{}t / {}t ({:.2f} / {:.2f} frames)", n.delay, n.windowTicks,
                                             n.delay * dt, n.windowTicks * dt));
            kv("Click as made", n.r0.end == SimEnd::Died
                                    ? fmt::format("dies in {}t ({})", n.r0.ticks, deathWhat(n.r0))
                                    : fmt::format("{} ({}t looked at)", simEndName(n.r0.end), n.r0.ticks),
               n.r0.end == SimEnd::Died ? COL_ORANGE : COL_WHITE);
            kv("Search", fmt::format("{} delays tested, {} us, lookahead {}t", n.evals, n.us, n.horizon));
            std::string flags;
            auto f = [&](bool b, char const* t) { if (b) flags += flags.empty() ? t : std::string(", ") + t; };
            f(n.orb, "orb"); f(n.stream, "wave stream"); f(n.rescue, "rescue"); f(n.portal, "portal");
            f(n.orbHelp, "orb helper");
            if (!flags.empty()) kv("Flags", flags);
        } else {
            kv("Input", "none yet");
        }
        head("Queue");
        kv("Waiting inputs", fmt::format("{} (shift {:.2f} frames)", m_queue.size(), m_queue.shift()));
        kv("Live check", m_live ? "on" : "off");
        head("Your rhythm");
        for (auto [w, name] : {std::pair{W_SHIP, "Ship"}, std::pair{W_WAVE, "Wave"}}) {
            kv(fmt::format("{} hold / gap", name),
               fmt::format("{:.1f}f / {:.1f}f ({} / {} samples)", rhythm(m_holdLen[w], DEFAULT_TAP_FRAMES),
                           rhythm(m_gapLen[w], DEFAULT_GAP_FRAMES), m_holdLen[w].count(), m_gapLen[w].count()));
        }
        kv("Ship limits", fmt::format("held up to {}t", SHIP_MAX_DELAY_BASE));
        {
            std::string lm;
            for (int i = 0; i < W_COUNT; i++)
                if (m_lmargin[i] > 0.01f) lm += fmt::format("{}{} +{:.2f}t", lm.empty() ? "" : ", ", WINDOW_KEYS[i] + 7, m_lmargin[i]);
            kv("Learned margin", fmt::format("{} ({} deeper after a moved click died, {} eased after it lived)",
                                             lm.empty() ? std::string("none (moved clicks have lived)") : lm, m_lmUps, m_lmDowns));
        }
        // (2.3) the escape search: "could you still save it yourself?"
        kv("Escape search", fmt::format("{} way out / {} none / {} undecided ({} tests)", m_escapeYes, m_escapeNo,
                                        m_escapeUnknown, m_escapeRuns));
        // (2.3) the 100 ms prediction check (setting "Prediction Check")
        head("Prediction (100 ms)");
        if (m_drift.empty()) kv("Checks", cfg().driftCheck ? "none yet (play a little)" : "off (setting: Prediction Check)");
        for (auto const& [key, d] : m_drift) {
            if (d.n == 0) continue;
            double const pct = 100.0 * static_cast<double>(d.close) / static_cast<double>(d.n);
            std::string extra;
            if (d.offN > 0) extra += fmt::format(", off from ~t{:.0f}", static_cast<double>(d.offTickSum) / static_cast<double>(d.offN));
            if (d.falseDeath) extra += fmt::format(", {} false deaths", d.falseDeath);
            if (d.missedDeath) extra += fmt::format(", {} missed deaths", d.missedDeath);
            kv(key, fmt::format("{:.0f}% exact of {}, avg {:.2f}px, worst {:.1f}px{}", pct, d.n,
                                d.errSum / static_cast<double>(d.n), d.worst, extra),
               pct >= 99.0 ? COL_GREEN : pct >= 90.0 ? COL_YELLOW : COL_ORANGE);
        }
        head("Input timing");
        if (m_lagSamples.empty()) kv("Sent inputs", "no clean check yet");
        else {
            std::string v;
            for (int x : m_lagSamples) v += (v.empty() ? "" : " ") + std::to_string(x);
            kv("Sent inputs", fmt::format("reach the player {} tick(s) late (checks: {})", m_fireLag, v),
               m_fireLag == 0 ? COL_GREEN : COL_ORANGE);
        }
        head("Portals in predictions");
        kv("Speed / gravity", fmt::format("{} / {} (speed portals: GD's own speed change)", m_simSpeedSwitches, m_simGravFlips));
        kv("GD's anticheat spike", fmt::format("{} touches ignored (it never kills)", m_simAnticheat));
        kv("Off-screen collisions", fmt::format("{} hidden-player steps with the mod's own objects, inside GD's step", m_simInlineExtra));
        kv("Off-screen objects", fmt::format("{} blocks / spikes GD had switched off (outside its area) used", m_simOffscreenUsed));
        kv("Off-screen collisions on", fmt::format("{} / {} times a switched-off (flag 1 / 2) block / spike was counted for a hidden-player step",
                                                   m_simUndisabledN, m_simUndisabledN2), COL_GREY);
        kv("Step audit: between steps", fmt::format("{} steps; GD changed outside the hidden player's step (end of step -> next step): {}",
                                                    m_audOutsideN, auditTop(m_audOutside, 16)), COL_GREY);
        kv("Step audit: one tick from your state", fmt::format("{:.0f}% exact of {}; fields off (you / hidden): {}",
                                                               m_audSimN ? 100.0 * m_audSimExact / m_audSimN : 0.0, m_audSimN,
                                                               auditTop(m_audSim, 16)), COL_GREY);
        kv("Step audit: one run vs saved states", fmt::format("{:.0f}% the same of {} (12 ticks in one run vs 12 one-tick runs); "
                                                              "fields off (one run / restarted): {}",
                                                              m_audRestartN ? 100.0 * m_audRestartExact / m_audRestartN : 0.0,
                                                              m_audRestartN, auditTop(m_audRestart, 16)), COL_GREY);
        kv("Collision path", fmt::format("{} hidden steps: GD's object check ran in {} ({:.0f}%), skipped in {} (no objects handed: "
                                         "the mod's blocks / spikes resolved itself, {} block / {} spike hits); finish step in GD's check {} / outside {}",
                                         m_pathSteps, m_pathCco, m_pathSteps ? 100.0 * m_pathCco / m_pathSteps : 0.0, m_ccoSkipped,
                                         m_ownSolid, m_ownHazard, m_postInCheck, m_postOutside), COL_GREY);
        kv("Gamemode / size / mirror", fmt::format("{} / {} / {}{}", m_simModeSwitches, m_simSizeSwitches, m_simMirrors,
                                                   cfg().predictPortals ? "" : " (gamemodes off: setting)"));
        if (p) {
            float lo = 0.f, hi = 0.f;
            if (bandOf(p, lo, hi, pl->m_gameState.m_portalY)) {
                CCRect const& r = p->getObjectRect();
                kv("Floor / ceiling", fmt::format("{:.0f} .. {:.0f} (you {:.0f} .. {:.0f}, portal y {:.0f})", lo, hi, r.getMinY(),
                                                  r.getMaxY(), p->m_lastPortalPos.y));
            }
        }
        head("Autoplay");
        kv("Status", m_autoOn ? fmt::format("playing - {} plans, {} tests", m_autoPlans, m_autoRuns)
                              : std::string("off (Tools tab)"), m_autoOn ? COL_GREEN : COL_WHITE);
        if (m_autoUsed) kv("Progress", "not saved on this level (autoplay played it)", COL_ORANGE);
        kv("Autoplay options", fmt::format("Learning Run {}, Noclip {}{}, Retry Deaths {}, Remember {}, Moving Objects {}; "
                                           "{} inputs kept because they got past a spot (the next input planned from there); planning "
                                           "skipped on {} ticks (that frame's time was used up)",
                                           cfg().apLearn ? "on" : "off",
                                           cfg().apNoclip ? "on" : "off",
                                           m_noclipHits ? fmt::format(" ({} touches ignored)", m_noclipHits) : std::string(),
                                           cfg().apRetry ? "on" : "off", cfg().apReplay ? "on" : "off", cfg().apMovers ? "on" : "off",
                                           m_rtProgress, m_apFrameSkips));
        kv("Every-way search", fmt::format("{} started (no input near a spot got past it), {} found a way, {} put into the route",
                                           m_rtAStarts, m_rtAFound, m_rtASpliced));
        kv("Retry Deaths", fmt::format("{}; {} of your checkpoints; {} retries ({} in a row at x {:.0f}), {} inputs ruled out",
                                       apPracticeActive(pl) ? "practice mode" : "not in use",
                                       pl && pl->m_checkpointArray ? pl->m_checkpointArray->count() : 0u,
                                       m_apRetryTotal, m_apRetries, m_apRetryX > -1e8f ? m_apRetryX : 0.f, m_apBans.size()));
        {
            std::string dk;
            for (auto const& [k2, n] : m_deathKinds) dk += fmt::format("{}{} {}", dk.empty() ? "" : ", ", n, k2);
            int sol = 0, tri = 0;
            for (auto const& t : m_trials) { if (t.solved >= 0) sol++; tri += t.tried; }
            kv("Death types", dk.empty() ? std::string("no deaths yet") : dk);
            kv("Your Clicks", fmt::format("{} made, {} got you further and were kept; {} kept for this level (saved to {}), {} made "
                                          "again by autoplay, {} forgotten (died {}x), {} removed by you",
                                          m_clicksMade, m_clicksKept, m_clicks.size(),
                                          clicksFile().empty() ? std::string("nowhere - no level key") : clicksFile().filename().string(),
                                          m_clicksReplayed, m_clicksDropped, CLICK_FAILS_MAX, m_clicksRemoved));
            kv("Retry spots", fmt::format("{} spots with a list of inputs to try, {} tried for real, {} got past", m_trials.size(), tri, sol));
        }
        kv("Remember", !memValid() ? std::string("nothing yet (inputs it lives a second past, further than ever, are remembered)")
                                   : fmt::format("x {:.0f} .. {:.0f}, {} inputs{}; grew {} times, cut back {} times (a stretch died); this run "
                                                 "{} inputs from x {:.0f}", m_memStart, m_memEnd, m_mem.size(),
                                                 m_memPlaying ? " - playing it" : "", m_memGrows, m_memCuts, m_apRun.size(), m_apRunStartX));
        kv("Learning run", m_learn == 1 ? fmt::format("running - deaths off, {:.0f}% of the level, {} touches ignored", learnPct(pl), m_learnHits)
                         : m_learn == 2 ? fmt::format("done{}", m_learnWhy.empty() ? std::string() : " (" + m_learnWhy + ")")
                                        : std::string("not yet (autoplay's first run of a level)"),
           m_learn == 1 ? COL_ORANGE : COL_WHITE);
        {
            long long nAhead = 0, timed = 0, moving = 0;
            for (auto const& m : m_mv) {
                if (m.ahead) nAhead++;
                if (!m.tl.empty()) timed++;
                if (m.vx != 0.0 || m.vy != 0.0 || m.vrot != 0.f) moving++;
            }
            kv("Moving objects", fmt::format("{} blocks / spikes moved or switched ({} ahead now, {} moving now, {} switches seen); "
                                             "timeline for {} from {} attempt(s), {} samples; predictions: {} placed, {} from the timeline, "
                                             "{} from their motion; hitbox followed {} / {}{}; taps that changed nothing: {} skipped, {} used",
                                             m_mv.size(), nAhead, moving, m_mvToggled, timed, m_mvAttempts, m_mvSamples, m_mvApplies,
                                             m_mvFromTl, m_mvFromMotion, m_mvRectOk, m_mvRectOk + m_mvRectBad,
                                             m_mvSetPos ? " (moved the drawn way too)" : "", m_rtUseless, m_rtUselessUsed),
               m_mvRectBad > m_mvRectOk ? COL_ORANGE : COL_WHITE);
        }

        if (m_autoOn && m_rtOn && !m_rt.empty()) {
            // ticks are simDt() 60fps-frames long
            float const aheadS = static_cast<float>(std::max(0, rtEnd() - m_rtNow)) * simDt() / 60.f;
            kv("Route check", fmt::format("{} stretches re-checked with the level as it is now, {} were stale and planned again, "
                                          "{} replayed a little off{}; {} stuck spots fought again", m_rtVerRuns, m_rtVerFixes, m_rtReplayMiss,
                                          m_rtVerLast.empty() ? std::string() : " (last " + m_rtVerLast + ")", m_rtUnstuck),
               m_rtVerFixes ? COL_ORANGE : COL_GREEN);
            kv("Route", fmt::format("{:.1f} s ahead (to x {:.0f}), {} inputs, {} restarts, {} tests, {} looked back past a portal, {} from the level scan{}", aheadS,
                                    rtAt(rtEnd()).x, m_rtEvents.size(), m_rtStarts, m_rtRuns, m_rtPortalWidens, m_rtScanRescues,
                                    m_rtDone ? " - to the end" : m_rtStuck ? fmt::format(" - stuck at x {:.0f}: {}", m_rtStuckX, m_rtStuckWhy) : std::string()),
               m_rtStuck ? COL_ORANGE : COL_GREEN);
        }
        for (auto const& [mode, a] : m_autoStats) {
            kv(mode, fmt::format("{} plans, {} drifts (worst {:.1f}px), {} deaths ({} the plan said were safe)", a.plans,
                                 a.drifts, a.worst, a.deaths, a.simBugs),
               a.simBugs > 0 || a.drifts > 0 ? COL_ORANGE : COL_GREEN);
        }
        for (auto const& note : m_autoNotes) rows.emplace_back(note, COL_GREY);
        head("Prediction (one tick)");
        if (m_oneStep.empty()) kv("Checks", "none yet (turn on Log: More Detail)");
        for (auto const& [mode, o] : m_oneStep) {
            if (o.n == 0) continue;
            double const pct = 100.0 * o.exact / o.n;
            kv(mode, fmt::format("{:.0f}% of {}, worst {:.2f}px", pct, o.n, o.worst),
               pct >= 99.0 ? COL_GREEN : pct >= 90.0 ? COL_YELLOW : COL_ORANGE);
        }
        head("Scan");
        kv("Status", scanStatus());
        kv("Versions / history", fmt::format("{} / {}", m_beam.size(), m_beamHist.size()));
        kv("Backups / runs / real / skips", fmt::format("{} / {} / {} / {}", m_scanBackups, m_scanJumps, m_scanReal, m_scanSkips));
        kv("Saved run states", std::to_string(m_anchors.size()));
        kv("Route inputs", std::to_string(m_route.size()));
        return rows;
    }
    // Dev tab "Copy All": the tab plus the newest trace lines.
    std::string devCopyText() {
        std::string out = fmt::format("Aceware dev info - {} - {}\n", Mod::get()->getVersion().toVString(), systemInfo().full);
        for (auto const& r : devRows()) {
            std::string t = r.first;
            if (!t.empty() && t[0] == '\x01') { out += "\n== " + t.substr(1) + " ==\n"; continue; }
            for (auto& ch : t) if (ch == '\t') ch = ':';
            out += t + "\n";
        }
        out += "\n== Trace (newest 80) ==\n";
        size_t const from = m_trace.size() > 80 ? m_trace.size() - 80 : 0;
        for (size_t i = from; i < m_trace.size(); i++) out += m_trace[i] + "\n";
        return out;
    }
    // Console just opened: what's in the Log so far, oldest first.
    void dumpLogToConsole() {
        for (auto const& h : m_history) {
            std::string const rep = h.repeat > 1 ? fmt::format(" (x{})", h.repeat) : std::string();
            LogConsole::write(fmt::format("{:8.2f}s  {:<6}  {}{}", h.t, h.tag, h.text, rep), logTagColor(h.tag));
        }
    }
    // Bumped on every log change: the window only rebuilds the Log when it moved.
    unsigned long long logVersion() const { return m_logVersion; }
    unsigned long long m_logVersion = 0;

    // One-line status for the window footer.
    std::string statusLine() {
        Config const& c = cfg();
        PlayLayer* pl = PlayLayer::get();
        std::string out = pl ? statusText(pl, c) : headerText(c);
        auto const& a = m_tracker.stats();
        if (a.checks > 0) out += fmt::format(" | pred {:.1f}%", a.percent());
        return out;
    }

    std::string headerLine() { return headerText(cfg()); }
    ccColor3B headerLineColor() { return headerColor(cfg()); }

    // Stats tab (1.25.8): short sections of "label<TAB>value" rows - the window
    // draws the label on the left (muted) and the value right-aligned in the
    // row's colour. Rows without a tab are section titles / notes. Everything
    // that's only for debugging is listed under DETAILS, shown with "Log: show
    // safe clicks" on.
    std::vector<Row> statsRows() {
        Config const& c = cfg();
        Config const e = effectiveCfg(c);
        auto const& st = m_stats;
        auto d = [&](Decision x) { return st.byDecision[static_cast<int>(x)]; };
        std::vector<Row> rows;
        // '\x01' first = a section title: the window draws it bold (bigFont) and white.
        auto head = [&](std::string const& t) {
            if (!rows.empty() && !rows.back().first.empty()) rows.emplace_back("", COL_GREY); // a gap between sections
            rows.emplace_back("\x01" + t, COL_WHITE);
        };
        auto kv = [&](std::string const& label, std::string const& value, ccColor3B col = COL_WHITE) {
            // grey rows read as white (2.1); green / yellow / orange still mean something
            if (col.r == COL_GREY.r && col.g == COL_GREY.g && col.b == COL_GREY.b) col = COL_WHITE;
            rows.emplace_back(label + "\t" + value, col);
        };
        auto note = [&](std::string const& t, ccColor3B col = COL_GREY) { rows.emplace_back("  " + t, col); };
        auto pctCol = [](double p) { return p >= 95.0 ? COL_GREEN : p >= 80.0 ? COL_YELLOW : COL_ORANGE; };

        PlayLayer* pl = PlayLayer::get();

        // ---- this level (its name)
        {
            std::string lvl;
            if (pl && pl->m_level) lvl = std::string(pl->m_level->m_levelName);
            if (lvl.empty()) lvl = m_spotLevelName;
            head(lvl.empty() ? std::string("This level") : lvl);
        }
        int const raw = d(Decision::PassTruncated) + d(Decision::PassNoFix) + d(Decision::PassSpam) + d(Decision::PassBudget);
        kv("Clicks", fmt::format("{}", st.clicks));
        kv("Moved to the right tick", fmt::format("{}", d(Decision::Buffered)), d(Decision::Buffered) ? COL_GREEN : COL_WHITE);
        kv("Already on time", fmt::format("{}", d(Decision::PassSafe) + d(Decision::PassEscapable)));
        kv("Went in as you made them", fmt::format("{}", raw));
        kv("Deaths", fmt::format("{}", st.deaths), st.deaths ? COL_WHITE : COL_GREY);
        {
            int best = -1, bestN = 0;
            for (int i = 0; i < static_cast<int>(DeathReason::Count); i++) {
                if (st.byDeath[i] > bestN) { bestN = st.byDeath[i]; best = i; }
            }
            if (best >= 0) kv("Most deaths", fmt::format("{} ({})", deathText(static_cast<DeathReason>(best)), bestN), COL_GREY);
        }
        if (m_hookMissing) {
            note(fmt::format("{} clicks passed raw: the physics hook isn't running", m_hookMissing), COL_ORANGE);
        } else if (m_inputsSeen == 0 && pl) {
            note("No clicks reached the buffer yet. If you've been clicking, another mod takes them first.");
        }

        // ---- prediction
        head("Prediction");
        auto const& a = m_tracker.stats();
        if (a.checks > 0) {
            kv("Right 100ms after a click", fmt::format("{:.0f}% of {}", a.percent(), a.checks), pctCol(a.percent()));
        } else {
            kv("Right 100ms after a click", c.accuracy ? "no checks yet" : "off", COL_GREY);
        }
        {
            long long n = 0, ex = 0;
            for (auto const& [name, o] : m_oneStep) { n += o.n; ex += o.exact; }
            if (n > 0) {
                double const p = 100.0 * static_cast<double>(ex) / static_cast<double>(n);
                kv("Exact after one tick", fmt::format("{:.0f}% of {}", p, n), pctCol(p));
            }
        }
        for (auto const& [name, ac] : m_accByMode) {
            if (ac.checks == 0) continue;
            double const pct = 100.0 * ac.hits / ac.checks;
            kv("  " + name, fmt::format("{:.0f}%, {:.1f}px off", pct, ac.errSum / ac.checks), pctCol(pct));
        }

        // ---- rhythm
        head("Your rhythm (60fps frames)");
        {
            bool any = false;
            for (int w : {W_CUBE, W_SHIP, W_WAVE}) {
                if (m_holdLen[w].count() == 0 && m_gapLen[w].count() == 0) continue;
                any = true;
                bool const learning = m_holdLen[w].count() < RHYTHM_MIN_SAMPLES;
                kv(fmt::format("  {}", winLabel(w)),
                   fmt::format("hold {:.1f}f, gap {:.1f}f{}", rhythm(m_holdLen[w], DEFAULT_TAP_FRAMES),
                               rhythm(m_gapLen[w], DEFAULT_GAP_FRAMES), learning ? " (learning)" : ""),
                   learning ? COL_GREY : COL_WHITE);
            }
            if (!any) note("learning from your clicks...");
        }

        // ---- windows
        head("Windows (how early a click can be)");
        for (int w = 0; w < W_COUNT; w++) {
            bool const autoOn = c.autoWindow && m_autoW[w] > 0.f;
            // What clicks really get: Extreme widens every window to at least 20
            // frames (12 for ship / wave) - this showed the base 8f / 5f (1.25.9).
            float f = e.window[w];
            if (c.extreme) {
                bool const holdW = w == W_SHIP || w == W_SHIP_REL || w == W_WAVE || w == W_WAVE_REL;
                f = std::max(f, static_cast<float>(holdW ? c.extremeHoldCap : c.extremeWindow) * BASE_DT);
                // (releases: the same window as presses since 2.2)
            }
            kv(fmt::format("  {}", winLabel(w)), fmt::format("{:g}f{}", std::round(f * 100.f) / 100.f, autoOn ? " (auto)" : ""),
               autoOn ? COL_GREEN : COL_WHITE);
        }

        // ---- performance
        head("Performance");
        long long const avg = st.simClicks > 0 ? st.sumUs / st.simClicks : 0;
        kv("Prediction per click", fmt::format("avg {:.1f}ms, max {:.1f}ms", avg / 1000.0, st.maxUs / 1000.0));
        if (m_haveFrame) {
            float const frameMs = m_frameUs / 1000.f;
            float const worst = static_cast<float>(std::max<long long>(st.maxUs, m_liveUsMax)) / 1000.f;
            kv("Frame", fmt::format("{:.1f}ms ({} fps){}", frameMs, static_cast<int>(std::lround(1e6f / m_frameUs)),
                                     m_cbfLoaded ? ", CBF" : ""));
            kv("Worst click cost", fmt::format("{:.0f}% of a frame", frameMs > 0.f ? 100.f * worst / frameMs : 0.f),
               worst > frameMs * 0.5f ? COL_ORANGE : COL_GREEN);
        }
        if (m_simStepsTotal > 0) {
            double const perStep = static_cast<double>(m_simUsTotal) / static_cast<double>(m_simStepsTotal);
            kv("Level weight", perStep < 3.0 ? "light" : perStep < 8.0 ? "medium" : "heavy",
               perStep < 8.0 ? COL_WHITE : COL_ORANGE);
        }
        if (m_planFires + m_planDrifts > 0) {
            kv("Held clicks fired on plan", fmt::format("{} ({} re-checked)", m_planFires, m_planDrifts),
               m_planDrifts > m_planFires ? COL_YELLOW : COL_GREEN);
        }
        if (!m_route.empty()) {
            kv("Scanned route", fmt::format("{} inputs", m_route.size()));
        }
        if (m_simExtraObjs > 0) {
            kv("Objects GD skipped", fmt::format("{} blocks / spikes added to predictions (invisible floors etc.)", m_simExtraObjs));
        }
        if (m_rayPicks > 0) {
            kv("Wave rays", fmt::format("{} picks: {} confirmed, {} rejected (full search)", m_rayPicks, m_rayConfirmed,
                                        m_rayRejected));
        }
        if (m_followOnTime + m_followSpacing > 0) {
            kv("Wave / ship follow-ups", fmt::format("{} on time, {} kept spacing", m_followOnTime, m_followSpacing));
        }
        if (m_cheapClicks > 0) kv("Clicks finished with quick checks", fmt::format("{}", m_cheapClicks), COL_YELLOW);
        if (m_watchdogFires > 0) kv("Inputs released by the watchdog", fmt::format("{}", m_watchdogFires), COL_ORANGE);
        if (m_directFires > 0 || m_directMode) kv("Applied directly (another input mod)", fmt::format("{}", m_directFires), COL_YELLOW);

        // ---- learned spots (mini cube)
        if (c.learnSpots && m_spots.size() > 0) {
            head("Learn from attempts (mini cube)");
            kv("Spots on this level", fmt::format("{} good, {} bad", m_spots.goodCount(), m_spots.badCount()));
        }

        // ---- what the game runs on (2.1)
        {
            SystemInfo const& si = systemInfo();
            head("System");
            kv("Running on", si.full);
            if (!si.device.empty()) kv("Device", si.device);
            kv("Geometry Dash", Loader::get()->getGameVersion());
            kv("Geode", Loader::get()->getVersion().toVString());
            kv("Aceware", Mod::get()->getVersion().toVString());
        }

        // ---- details (debugging), only with "Log: show safe clicks"
        if (!c.hudVerbose && !m_forceDetails) {
            rows.emplace_back("", COL_GREY);
            note("More detail: turn on \"Log: more detail\" (Stage tab).", COL_DIM);
            return rows;
        }
        rows.emplace_back("", COL_GREY);
        head("Details");
        for (auto const& [k, f] : m_orbTable) {
            if (f.n == 0) continue;
            kv("Orb learned: " + orbKeyName(k), fmt::format("{:.2f}{}", f.vySum / f.n, f.flips * 2 > f.n ? ", flips" : ""), COL_GREY);
        }
        kv("Raw: orb / portal cut", fmt::format("{}", d(Decision::PassTruncated)), COL_GREY);
        kv("Raw: no fix in the window", fmt::format("{}", d(Decision::PassNoFix)), COL_GREY);
        kv("Raw: spam guard / CPU", fmt::format("{} / {}", d(Decision::PassSpam), d(Decision::PassBudget)), COL_GREY);
        for (int i = 0; i < static_cast<int>(DeathReason::Count); i++) {
            if (st.byDeath[i] == 0) continue;
            kv(std::string("Death: ") + deathText(static_cast<DeathReason>(i)), fmt::format("{}", st.byDeath[i]), COL_GREY);
        }
        if (a.checks > 0) kv("Accuracy: died first / skipped", fmt::format("{} / {}", a.deaths, a.dropped), COL_GREY);
        for (auto const& [name, ac] : m_accByMode) {
            if (ac.checks == 0) continue;
            kv("Off by (" + name + ")", fmt::format("x {:+.1f}, y {:+.1f}px, room {:.1f}px", ac.dxSum / ac.checks,
                                                    ac.dySum / ac.checks, clearTarget(name)), COL_GREY);
        }
        for (auto const& [name, o] : m_oneStep) {
            if (o.n == 0) continue;
            double const pct = 100.0 * o.exact / o.n;
            kv("One tick (" + name + ")", fmt::format("{:.0f}% of {}, worst {:.1f}px", pct, o.n, o.worst), pctCol(pct));
        }
        kv("Orb contact from the game", m_gdRings ? "yes" : "not seen yet", COL_GREY);
        kv("Orb rules", fmt::format("{} orbs in predictions from GD's rules, {} learned values that disagreed with them skipped",
                                    m_orbModelUses, m_orbLearnedBad), COL_GREY);
        kv("Size portals in predictions", fmt::format("{}", m_simSizeSwitches), COL_GREY);
        kv("Near misses avoided", fmt::format("{}", m_cleanWaits), COL_GREY);
        if (m_liveChecks > 0) {
            kv("Held-click re-checks", fmt::format("{}, avg {:.1f}ms", m_liveChecks, (m_liveUsSum / m_liveChecks) / 1000.0), COL_GREY);
        }
        if (c.learnSpots && !m_spotFile.empty()) {
            kv("Spots file", fmt::format("spots/{}", m_spotFile.filename().string()), COL_GREY);
        }
        for (int t = 0; t < 64; t++) {
            if (!m_pads[t].seen) continue;
            kv(fmt::format("Pad: {}", objectName(static_cast<GameObjectType>(t))),
               m_pads[t].unstable ? "varies (not predicted)" : "learned", m_pads[t].unstable ? COL_ORANGE : COL_GREY);
        }
        return rows;
    }

    // Clear learned timing now (menu button).
    void resetCalibration() {
        auto* mod = Mod::get();
        for (int i = 0; i < W_COUNT; i++) {
            m_calib[i].clear();
            m_autoW[i] = 0.f;
            mod->setSavedValue<std::string>(std::string("calib-") + WINDOW_KEYS[i], "");
        }
        m_calibDirty = false;
        Notification::create("Aceware - calibration cleared", NotificationIcon::None, 1.0f)->show();
    }

    // A setting was changed from the menu.
    void settingsChanged() {
        invalidateCfg();
        refreshFlags();
        if (!cfg().learnSpots) m_spots.clear(); // switching it off forgets this level
        clearBuffer();
        if (PlayLayer* pl = PlayLayer::get()) refreshHud(pl, true);
    }

    // ------------------------------------------------------------------------
    //  Prediction context: everything one test of "press / release now (or
    //  after d ticks)" needs. Built at click time and again every tick by the
    //  live re-planner, so both use exactly the same rules.
    // ------------------------------------------------------------------------
    struct Ctx {
        PlayLayer*    pl = nullptr;
        PlayerObject* p1 = nullptr;
        PlayerObject* p2 = nullptr;
        bool  simP2 = false;
        bool  push = true;
        ModeInfo mode;
        float dt = BASE_DT, sdt = BASE_DT, s = 1.f;
        int   horizon = 0;
        bool  noLanding = false;
        bool  tapModel = false;
        int   tapTicks = 0;
        bool  canEscape = false;
        bool  landRule = false;
        int   window = 0;   // stage window for this input, ticks
        bool  modern = false; // current engine (mini cube) vs 1.10.2 behaviour
        int   waveCommit = 0; // wave (2.1): a death within this many ticks is the input's fault
    };

    // ---- Wave rays (2.2) ------------------------------------------------------
    //  See acecore::waveRay: a wave input is two straight lines, so the delay
    //  to try is found by casting them against the level's hitboxes instead
    //  of running the physics for every candidate. The pick is then checked
    //  once with the real prediction - the rays only save the search.
    // (2.3) where a block / spike is, for the moving-object tracker (below)
    struct MvState { double x = 0.0, y = 0.0; float rot = 0.f; bool off = false; };
    struct RayObj {
        float x0 = 0.f, x1 = 0.f; // left / right edge when the level loaded
        GameObject* obj = nullptr;
        bool slope = false;       // (2.2) slopes: in the list for the sim, not for the rays
        MvState base;             // (2.3) where it was when the level loaded
        int mv = -1;              // (2.3) it moves: its entry in m_mv
    };
    std::vector<RayObj> m_rayObjs;               // solids + hazards, by left edge
    std::vector<float> m_speedXs;                // (2.2) where the speed portals are
    float m_rayMaxW = 0.f;                       // widest one (query slack)
    PlayLayer* m_rayLayer = nullptr;             // level the index belongs to
    std::vector<acecore::RayBox> m_rayScratch;
    long long m_rayPicks = 0, m_rayConfirmed = 0, m_rayRejected = 0;

    static bool waveOnly(ModeInfo const& m) {
        return m.wave && !(m.ship || m.ufo || m.swing || m.robot || m.cubeLike);
    }
    void releaseRayIndex() {
        m_speedXs.clear();
        m_rayObjs.clear();
        m_rayObjs.shrink_to_fit();
        mvClear(); // (2.3) the moving objects belong to the level too
        m_rayLayer = nullptr;
        m_rayMaxW = 0.f;
        m_levelBottom = -1e9f;
        m_levelTop = 1e9f;
    }
    // Once per level (first wave input): every solid block and hazard. The
    // boxes themselves are read live when used, so things that moved
    // up / down are where they are now.
    void buildRayIndex(PlayLayer* pl) {
        releaseRayIndex();
        m_rayLayer = pl;
        if (!pl || !pl->m_objects) return;
        for (auto* o : CCArrayExt<GameObject*>(pl->m_objects)) {
            if (!o) continue;
            GameObjectType const t = o->m_objectType;
            bool const solid = t == GameObjectType::Solid;
            bool const slope = t == GameObjectType::Slope;
            float spd = 0.f;
            if (t == GameObjectType::Modifier && speedForPortal(o->m_objectID, spd)) { m_speedXs.push_back(o->getPositionX()); continue; }
            if (!solid && !slope && !isHazard(t)) continue;
            if (o->m_isNoTouch || (solid && o->m_isPassable)) continue;
            CCRect const& r = o->getObjectRect();
            if (r.size.width <= 0.f || r.size.height <= 0.f) continue;
            m_rayObjs.push_back(RayObj{r.getMinX(), r.getMaxX(), o, slope, mvRead(o), -1});
            m_rayMaxW = std::max(m_rayMaxW, std::min(r.size.width, 3000.f));
        }
        std::sort(m_rayObjs.begin(), m_rayObjs.end(), [](RayObj const& a, RayObj const& b) { return a.x0 < b.x0; });
        // (2.2) the level's height range: everything placed in it (any object),
        // plus a margin. A scan / pathfinder version outside it has flown out of
        // the level (the hidden player isn't held inside the floor / ceiling GD
        // puts at a ship / wave / UFO portal) - it can't be a way through.
        float lo = 1e9f, hi = -1e9f;
        for (auto* o : CCArrayExt<GameObject*>(pl->m_objects)) {
            if (!o) continue;
            CCRect const& r = o->getObjectRect();
            if (r.size.height <= 0.f || r.size.height > 3000.f) continue; // giant backgrounds don't count
            lo = std::min(lo, r.getMinY());
            hi = std::max(hi, r.getMaxY());
        }
        m_levelBottom = lo < hi ? lo - LEVEL_MARGIN : -1e9f;
        m_levelTop = lo < hi ? hi + LEVEL_MARGIN : 1e9f;
    }
    float m_levelBottom = -1e9f, m_levelTop = 1e9f;

    // ---- Moving objects (2.3) -------------------------------------------------
    //  GD moves, turns and switches blocks / spikes on and off with triggers.
    //  Every prediction used to see the level frozen the way it is right now:
    //  a block that slides into the way later wasn't in the plan, and one that
    //  slides out of the way still blocked it. Every End: "route re-checked
    //  with the level as it is now: now dies" at the same x every attempt, the
    //  route falling past block 1220 that you then land on, and "none of 1502
    //  inputs saves it" - all inputs tested against blocks that weren't going
    //  to be there.
    //  Now every block / spike / slope that leaves its place is tracked from
    //  what really happens:
    //   - where it is and how it moves right now (each tick);
    //   - a timeline: where it was when you were at each x. Recorded on every
    //     attempt and on the learning run (autoplay's first run of a level,
    //     deaths off), so the next attempt knows where things will be before
    //     they move - by your x, which is the level's clock (the triggers that
    //     move things go off at an x, and you cross the level at a set speed).
    //  For each hidden-player step the ones near it go where they will be at
    //  that moment - the timeline if it has that x and matched this attempt so
    //  far, else its motion right now carried on - and back right after.
    //  Only the objects' collision position changes (GD's m_positionX / Y,
    //  which its hitboxes use); nothing drawn moves. Kept for this session only.
    struct MvSample { float px = 0.f, x = 0.f, y = 0.f, rot = 0.f; bool off = false; };
    struct Mover {
        GameObject* obj = nullptr;
        int ray = -1;                 // its m_rayObjs entry
        float hw = 0.f;               // half its width
        MvState live, last;           // now / the tick before
        double vx = 0.0, vy = 0.0;    // its motion right now, per px you travel (x is the clock)
        float vrot = 0.f;
        int still = 0;                // ticks since it last moved
        bool lastValid = false;
        bool trust = true;            // the timeline matches what it does this attempt
        std::vector<MvSample> tl;     // earlier attempts, by your x
        std::vector<MvSample> cur;    // this attempt so far
        float tx0 = 1e9f, tx1 = -1e9f; // where the timeline has it (x range)
        float ex0 = 1e9f, ex1 = -1e9f; // where it can be for a prediction (x range)
        bool ahead = false;           // in m_mvNear this tick (not "near": a Windows macro)
    };
    struct MvUndo { GameObject* o = nullptr; double x = 0.0, y = 0.0; float rot = 0.f; bool off = false, rotSet = false; };
    static constexpr float MV_AHEAD = 3600.f;      // the stretch ahead it watches (and records)
    static constexpr float MV_BEHIND = 400.f;
    static constexpr float MV_KEY_PX = 40.f;       // a sample at least this often while it sits still
    static constexpr float MV_TRUST_PX = 6.f;      // the timeline is off from what it does now by more: not used
    static constexpr float MV_EXTRAP_TICKS = 120.f; // its motion right now carried on for at most this long (240 TPS ticks; ~0.5 s)
    static constexpr size_t MV_SAMPLES_MAX = 4000000;
    std::vector<Mover> m_mv;
    std::unordered_map<GameObject*, int> m_mvIdx;
    std::vector<int> m_mvNear;    // movers that can be near the stretch ahead (this tick)
    std::vector<MvUndo> m_mvUndo; // moved for the current hidden-player step
    float m_mvPx = 0.f, m_mvPrevPx = 0.f, m_mvStep = 1.f, m_mvDir = 1.f;
    bool m_mvHavePx = false, m_mvRec = true, m_mvSetPos = false, m_mvOn = true;
    long long m_mvPassStep = -1;
    PlayerObject* m_mvPassPlayer = nullptr;
    // the first collision call of this hidden player's step (the movers go in once per step)
    bool mvPassFirst(PlayerObject* p) {
        if (m_mvPassStep == m_simStepsTotal && m_mvPassPlayer == p) return false;
        m_mvPassStep = m_simStepsTotal;
        m_mvPassPlayer = p;
        return true;
    }
    size_t m_mvSamples = 0;
    long long m_mvApplies = 0, m_mvFromTl = 0, m_mvFromMotion = 0, m_mvRectOk = 0, m_mvRectBad = 0, m_mvToggled = 0;
    int m_mvAttempts = 0;

    static MvState mvRead(GameObject* o) {
        // (a round hitbox - a saw - doesn't care how it's turned: spinning isn't moving)
        return MvState{o->m_positionX, o->m_positionY, o->m_objectRadius > 0.f ? 0.f : o->getRotation(), o->m_isGroupDisabled};
    }
    static bool mvSame(MvState const& a, MvState const& b) {
        return std::fabs(a.x - b.x) < 0.02 && std::fabs(a.y - b.y) < 0.02 && std::fabs(a.rot - b.rot) < 0.05f && a.off == b.off;
    }
    static bool mvSameS(MvSample const& a, MvState const& b) {
        return std::fabs(a.x - b.x) < 0.02 && std::fabs(a.y - b.y) < 0.02 && std::fabs(a.rot - b.rot) < 0.05f && a.off == b.off;
    }
    static MvSample mvSample(float px, MvState const& s) {
        return MvSample{px, static_cast<float>(s.x), static_cast<float>(s.y), s.rot, s.off};
    }
    void mvClear() {
        m_mv.clear();
        m_mv.shrink_to_fit();
        m_mvIdx.clear();
        m_mvNear.clear();
        m_mvUndo.clear();
        m_mvHavePx = false;
        m_mvRec = true;
        m_mvSamples = 0;
        m_mvApplies = m_mvFromTl = m_mvFromMotion = m_mvRectOk = m_mvRectBad = m_mvToggled = 0;
        m_mvAttempts = 0;
    }
    // The timeline at your x = px (linear between samples; past its end: its last place).
    static MvSample mvInterp(std::vector<MvSample> const& tl, float px) {
        auto it = std::lower_bound(tl.begin(), tl.end(), px, [](MvSample const& s, float v) { return s.px < v; });
        if (it == tl.begin()) return tl.front();
        if (it == tl.end()) return tl.back();
        MvSample const& b = *it;
        MvSample const& a = *(it - 1);
        float const span = b.px - a.px;
        if (span <= 1e-4f || a.off != b.off) return px - a.px < span * 0.5f ? a : b;
        float const f = (px - a.px) / span;
        return MvSample{px, a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.rot + (b.rot - a.rot) * f, a.off};
    }
    // Where mover m is when you're at x = key (key: your x at the start of that step).
    MvState mvAt(Mover const& m, float key, bool* fromTl = nullptr) const {
        if (fromTl) *fromTl = false;
        float const ahead = (key - m_mvPx) * m_mvDir;
        if (ahead < -0.01f && !m.cur.empty() && m_mvDir > 0.f && key >= m.cur.front().px) {
            MvSample const s = mvInterp(m.cur, key); // a replay of a moment ago: where it was then
            return MvState{s.x, s.y, s.rot, s.off};
        }
        if (ahead <= 0.01f) return m.live;
        if (m.trust && !m.tl.empty() && m_mvDir > 0.f && key >= m.tl.front().px - 0.01f &&
            key <= m.tl.back().px + 2.f * MV_KEY_PX) {
            if (fromTl) *fromTl = true;
            MvSample const s = mvInterp(m.tl, key);
            return MvState{s.x, s.y, s.rot, s.off};
        }
        if (m.vx == 0.0 && m.vy == 0.0 && m.vrot == 0.f) return m.live;
        double const t = std::min<double>(ahead, m_mvCap * m_mvStep); // carried on for at most ~0.5 s
        MvState s = m.live;
        s.x += m.vx * t;
        s.y += m.vy * t;
        s.rot += m.vrot * static_cast<float>(t);
        return s;
    }
    // (MV_EXTRAP_TICKS is in 240 TPS ticks; at another tick rate the cap is the same time)
    float m_mvCap = MV_EXTRAP_TICKS;

    // Every real tick (before the step): who moves, how, and the timeline.
    void moverTick(PlayerObject* p) {
        if (m_rayObjs.empty() || !p || p->m_isDead || m_simulating) return;
        float const px = p->getPositionX();
        float const dir = m_p1Dir < 0.f ? -1.f : 1.f;
        float dpx = 0.f; // how far you went since the last call (a piece of a step with an input mod: less)
        if (m_mvHavePx) {
            float const d = (px - m_mvPx) * dir;
            dpx = d;
            // went back or was moved (a teleport): this attempt's x isn't a clock any more
            if (d < -1.f || d > 60.f || dir != m_mvDir) m_mvRec = false;
        }
        // one whole step (your speed - pieces of a step don't count)
        if (m_p1Speed > 0.01f) m_mvStep = std::max(0.05f, m_p1Speed * getValidDt());
        m_mvPrevPx = m_mvHavePx ? m_mvPx : px;
        m_mvOn = cfg().apMovers;
        m_mvCap = MV_EXTRAP_TICKS * 0.25f / std::max(getValidDt(), 0.01f);
        m_mvPx = px;
        m_mvDir = dir;
        m_mvHavePx = true;
        // (as far ahead as autoplay's route can reach: ~11 s)
        float const reach = std::max(MV_AHEAD, m_mvStep * (60.f / std::max(getValidDt(), 0.01f)) * 11.f);
        float const lo = px - (dir < 0.f ? reach : MV_BEHIND), hi = px + (dir < 0.f ? MV_BEHIND : reach);
        // new ones: anything in the stretch that left its place since the level loaded
        {
            auto it = std::lower_bound(m_rayObjs.begin(), m_rayObjs.end(), lo - m_rayMaxW,
                                       [](RayObj const& a, float v) { return a.x0 < v; });
            for (; it != m_rayObjs.end() && it->x0 <= hi; ++it) {
                if (it->mv >= 0 || it->x1 < lo) continue;
                GameObject* o = it->obj;
                MvState const now = mvRead(o);
                if (mvSame(now, it->base)) continue;
                Mover m;
                m.obj = o;
                m.ray = static_cast<int>(it - m_rayObjs.begin());
                m.hw = std::max(1.f, (it->x1 - it->x0) * 0.5f);
                m.last = it->base; // it sat there until now
                m.lastValid = true;
                if (m_mvRec && m_mvSamples < MV_SAMPLES_MAX) {
                    m.cur.push_back(mvSample(m_mvPrevPx, it->base));
                    m_mvSamples++;
                }
                it->mv = static_cast<int>(m_mv.size());
                m_mvIdx[o] = it->mv;
                m_mv.push_back(std::move(m));
            }
        }
        // all of them: motion, timeline, and where they can be
        m_mvNear.clear();
        for (int i = 0; i < static_cast<int>(m_mv.size()); i++) {
            Mover& m = m_mv[static_cast<size_t>(i)];
            MvState const now = mvRead(m.obj);
            MvState const prev = m.last;
            if (m.lastValid) {
                double const dx = now.x - prev.x, dy = now.y - prev.y;
                float const dr = now.rot - prev.rot;
                bool const moved = std::fabs(dx) > 1e-4 || std::fabs(dy) > 1e-4 || std::fabs(dr) > 1e-3f;
                if (moved && dpx > 0.005f && std::fabs(dx) < 120.0 && std::fabs(dy) < 120.0 && std::fabs(dr) < 90.f) {
                    m.vx = dx / dpx;
                    m.vy = dy / dpx;
                    m.vrot = dr / dpx;
                    m.still = 0;
                } else if (moved && dpx <= 0.005f && std::fabs(dx) < 120.0 && std::fabs(dy) < 120.0) {
                    m.still = 0; // moved while you didn't (a pause): keep its motion
                } else if (moved) { // a jump (respawn, a trigger that snaps it): no motion to carry on
                    m.vx = m.vy = 0.0;
                    m.vrot = 0.f;
                } else if (dpx > 0.005f && ++m.still >= 3) {
                    m.vx = m.vy = 0.0;
                    m.vrot = 0.f;
                }
                if (now.off != prev.off) m_mvToggled++;
            }
            m.last = now;
            m.live = now;
            m.lastValid = true;
            bool const inWin = now.x + m.hw >= lo && now.x - m.hw <= hi;
            if (m_mvRec && inWin && m_mvSamples < MV_SAMPLES_MAX) {
                auto& c = m.cur;
                auto push = [&](MvSample s) {
                    // three in a line (a steady move): the middle one says nothing
                    if (c.size() >= 2) {
                        MvSample const& a = c[c.size() - 2];
                        MvSample const& b = c.back();
                        float const span = s.px - a.px;
                        if (span > 1e-4f && a.off == b.off && b.off == s.off) {
                            float const f = (b.px - a.px) / span;
                            if (std::fabs(a.x + (s.x - a.x) * f - b.x) < 0.03f && std::fabs(a.y + (s.y - a.y) * f - b.y) < 0.03f &&
                                std::fabs(a.rot + (s.rot - a.rot) * f - b.rot) < 0.05f && span < MV_KEY_PX) {
                                c.back() = s;
                                return;
                            }
                        }
                    }
                    c.push_back(s);
                    m_mvSamples++;
                };
                if (c.empty() || (px - c.back().px) * dir < -0.01f) {
                    if (c.empty()) push(mvSample(px, now));
                } else if (!mvSameS(c.back(), now)) {
                    // it starts moving: where it sat, up to the tick before
                    if (mvSameS(c.back(), prev) && (m_mvPrevPx - c.back().px) * dir > 0.01f) push(mvSample(m_mvPrevPx, prev));
                    push(mvSample(px, now));
                } else if ((px - c.back().px) * dir >= MV_KEY_PX) {
                    push(mvSample(px, now));
                }
            }
            // does the timeline match what it does this attempt?
            m.trust = true;
            if (!m.tl.empty() && px >= m.tl.front().px && px <= m.tl.back().px + 2.f * MV_KEY_PX) {
                MvSample const t = mvInterp(m.tl, px);
                m.trust = std::fabs(t.x - now.x) < MV_TRUST_PX && std::fabs(t.y - now.y) < MV_TRUST_PX && t.off == now.off;
            }
            float const ext = static_cast<float>(m.vx) * m_mvCap * m_mvStep;
            float e0 = static_cast<float>(now.x) + std::min(0.f, ext) - m.hw;
            float e1 = static_cast<float>(now.x) + std::max(0.f, ext) + m.hw;
            if (m.vrot != 0.f) { e0 -= m.hw; e1 += m.hw; } // turning: its corners reach further
            if (m.trust && !m.tl.empty()) {
                e0 = std::min(e0, m.tx0 - m.hw * 2.f);
                e1 = std::max(e1, m.tx1 + m.hw * 2.f);
            }
            m.ex0 = e0;
            m.ex1 = e1;
            m.ahead = e1 >= lo && e0 <= hi;
            if (m.ahead) m_mvNear.push_back(i);
        }
    }
    // An attempt ended: what it recorded goes into the timeline (replacing
    // what earlier attempts had for the same stretch).
    void mvEndAttempt() {
        bool any = false;
        size_t total = 0;
        for (auto& m : m_mv) {
            if (m.cur.size() >= 1) {
                any = true;
                float const a = m.cur.front().px, b = m.cur.back().px;
                std::vector<MvSample> merged;
                merged.reserve(m.tl.size() + m.cur.size());
                for (auto const& s : m.tl) if (s.px < a - 0.01f) merged.push_back(s);
                for (auto const& s : m.cur) merged.push_back(s);
                for (auto const& s : m.tl) if (s.px > b + 0.01f) merged.push_back(s);
                m.tl.swap(merged);
                m.tx0 = 1e9f;
                m.tx1 = -1e9f;
                for (auto const& s : m.tl) {
                    m.tx0 = std::min(m.tx0, s.x);
                    m.tx1 = std::max(m.tx1, s.x);
                }
            }
            m.cur.clear();
            m.lastValid = false;
            m.vx = m.vy = 0.0;
            m.vrot = 0.f;
            m.still = 0;
            m.trust = true;
            m.ahead = false;
            total += m.tl.size();
        }
        if (any) m_mvAttempts++;
        m_mvSamples = total;
        m_mvRec = true;
        m_mvHavePx = false;
        m_mvNear.clear();
    }
    // A hidden player's step: the movers that can be near it go where they
    // will be then (put back by mvRestore after the step). Returns the ones
    // near enough to collide with, for filterForSim.
    template <class Take>
    void mvApply(PlayerObject* player, Take&& take) {
        if (m_mvNear.empty()) return;
        if (!m_mvOn) { // Moving Objects off (Tools): where GD has them now
            for (int i : m_mvNear) {
                GameObject* o = m_mv[static_cast<size_t>(i)].obj;
                if (o->m_isGroupDisabled) continue;
                CCRect const& pr = player->getObjectRect();
                CCRect const& r = o->getObjectRect();
                if (r.getMaxX() < pr.getMinX() - SIM_EXTRA_PAD || r.getMinX() > pr.getMaxX() + SIM_EXTRA_PAD ||
                    r.getMaxY() < pr.getMinY() - SIM_EXTRA_PAD || r.getMinY() > pr.getMaxY() + SIM_EXTRA_PAD) continue;
                take(o);
            }
            return;
        }
        CCRect const& pr = player->getObjectRect();
        float const lo = pr.getMinX() - SIM_EXTRA_PAD, hi = pr.getMaxX() + SIM_EXTRA_PAD;
        float const ylo = pr.getMinY() - SIM_EXTRA_PAD, yhi = pr.getMaxY() + SIM_EXTRA_PAD;
        // the objects are where they were when this step began (you, after it: one step on)
        float const key = player->getPositionX() - m_mvStep * m_mvDir;
        for (int i : m_mvNear) {
            Mover& m = m_mv[static_cast<size_t>(i)];
            if (m.ex1 < lo || m.ex0 > hi) continue;
            GameObject* o = m.obj;
            bool tl = false;
            MvState const s = mvAt(m, key, &tl);
            if (tl) m_mvFromTl++;
            else if (m.vx != 0.0 || m.vy != 0.0 || m.vrot != 0.f) m_mvFromMotion++;
            MvUndo u;
            u.o = o;
            u.x = o->m_positionX;
            u.y = o->m_positionY;
            u.rot = o->getRotation();
            u.off = o->m_isGroupDisabled;
            bool const check = (m_mvRectOk + m_mvRectBad) < 4000 && std::fabs(s.x - u.x) > 2.0;
            float const midBefore = check ? o->getObjectRect().getMidX() : 0.f;
            if (m_mvSetPos) o->setPosition(CCPoint{static_cast<float>(s.x), static_cast<float>(s.y)});
            o->m_positionX = s.x;
            o->m_positionY = s.y;
            if (o->m_objectRadius <= 0.f && std::fabs(s.rot - u.rot) > 0.05f) {
                o->setRotation(s.rot);
                u.rotSet = true;
            }
            o->m_isGroupDisabled = s.off;
            o->m_isObjectRectDirty = true;
            o->m_isOrientedBoxDirty = true;
            m_mvUndo.push_back(u);
            m_mvApplies++;
            if (s.off) continue;
            CCRect const& r = o->getObjectRect();
            if (check) {
                // (debug) GD's hitbox followed the move?
                if (std::fabs((r.getMidX() - midBefore) - static_cast<float>(s.x - u.x)) < 0.5f) m_mvRectOk++;
                else m_mvRectBad++;
                // GD's hitbox didn't follow its stored position: move it the drawn way too
                if (!m_mvSetPos && m_mvRectBad >= 20 && m_mvRectBad > m_mvRectOk * 4) m_mvSetPos = true;
            }
            if (r.getMaxX() < lo || r.getMinX() > hi || r.getMaxY() < ylo || r.getMinY() > yhi) continue;
            take(o);
        }
    }
    void mvRestore() {
        for (auto it = m_mvUndo.rbegin(); it != m_mvUndo.rend(); ++it) {
            GameObject* o = it->o;
            if (m_mvSetPos) o->setPosition(CCPoint{static_cast<float>(it->x), static_cast<float>(it->y)});
            o->m_positionX = it->x;
            o->m_positionY = it->y;
            if (it->rotSet) o->setRotation(it->rot);
            o->m_isGroupDisabled = it->off;
            o->m_isObjectRectDirty = true;
            o->m_isOrientedBoxDirty = true;
        }
        m_mvUndo.clear();
    }
    // a mover mvApply places this step (one outside the stretch it watches stays where GD has it)
    bool isMover(GameObject* o) const {
        if (m_mvIdx.empty()) return false;
        auto it = m_mvIdx.find(o);
        return it != m_mvIdx.end() && m_mv[static_cast<size_t>(it->second)].ahead;
    }

    // Hitboxes inside [lo, hi] x [ylo, yhi], as they are right now.
    void rayBoxes(float lo, float hi, float ylo, float yhi, std::vector<acecore::RayBox>& out, bool* slopeNear = nullptr) const {
        out.clear();
        if (slopeNear) *slopeNear = false;
        auto it = std::lower_bound(m_rayObjs.begin(), m_rayObjs.end(), lo - m_rayMaxW,
                                   [](RayObj const& a, float v) { return a.x0 < v; });
        for (; it != m_rayObjs.end() && it->x0 <= hi + m_rayMaxW; ++it) {
            GameObject* o = it->obj;
            if (o->m_isGroupDisabled) continue; // (2.3: off screen counts - see filterForSim)
            CCRect const& r = o->getObjectRect();
            if (r.getMaxX() < lo || r.getMinX() > hi || r.getMaxY() < ylo || r.getMinY() > yhi) continue;
            if (it->slope) { // (2.2) a slope isn't a box: say it's there
                if (slopeNear) *slopeNear = true;
                continue;
            }
            acecore::RayBox b;
            b.x = r.origin.x;
            b.y = r.origin.y;
            b.w = r.size.width;
            b.h = r.size.height;
            b.solid = o->m_objectType == GameObjectType::Solid;
            if (!b.solid && o->m_objectRadius > 0.f) b.r = std::min(r.size.width, r.size.height) * 0.5f; // saws
            out.push_back(b);
            if (out.size() >= 800) break;
        }
    }
    // Your wave right now as two rays: where you are, your hitbox, speed and
    // slope, which way you're going and which way this input sends you.
    bool waveRayInput(PlayLayer* pl, ModeInfo const& mode, bool push, float dt, acecore::WaveRayIn& in) const {
        PlayerObject* p = pl ? pl->m_player1 : nullptr;
        if (!p || p->m_isDead || p->m_isSideways || p->m_isPlatformer || !waveOnly(mode)) return false;
        if (pl->m_gameState.m_isDualMode || m_p1Speed < 0.01f || dt <= 0.f) return false;
        CCRect const& r = p->getObjectRect();
        in.x = r.getMidX();
        in.y = r.getMidY();
        in.hw = r.size.width * 0.5f;
        in.hh = r.size.height * 0.5f;
        in.vx = m_p1Speed * dt * (m_p1Dir < 0.f ? -1.f : 1.f);
        in.vy = std::fabs(in.vx) * (mode.miniWave ? 2.f : 1.f);
        bool const flip = p->m_isUpsideDown;
        in.dirNow = (m_gameHolding != flip) ? 1 : -1;
        in.dirNew = (push != flip) ? 1 : -1;
        return true;
    }
    // The delay the rays pick for this input (-1: none in [minD, maxD],
    // -2: rays can't be used here). `nowHit`: where it hits as made.
    int wavePick(PlayLayer* pl, Ctx const& x, int minD, int maxD, acecore::WaveRayHit* nowHit = nullptr) {
        acecore::WaveRayIn in;
        if (!waveRayInput(pl, x.mode, x.push, x.dt, in)) return -2;
        if (m_rayLayer != pl) buildRayIndex(pl);
        int const n = std::max(0, maxD) + std::max(1, x.waveCommit) + 2;
        float const reachX = std::fabs(in.vx) * static_cast<float>(n) + in.hw + 2.f;
        float const reachY = in.vy * static_cast<float>(n) + in.hh + 2.f;
        bool slope = false;
        rayBoxes(in.x - reachX, in.x + reachX, in.y - reachY, in.y + reachY, m_rayScratch, &slope);
        // (2.2) the rays are straight lines at one speed: with a slope (the
        // wave slides along it in GD) or a speed portal in reach, the full
        // simulation decides instead
        if (slope) return -2;
        for (float sx : m_speedXs) if (sx >= in.x - reachX && sx <= in.x + reachX) return -2;
        return acecore::waveRayDelay(in, minD, maxD, std::max(1, x.waveCommit), m_rayScratch, nowHit);
    }
    // Wave / ship: how long an input answers for the path - until your next
    // input at your own rhythm plus a short grace (wave), or 100 ms (ship).
    int holdCommit(ModeInfo const& m, bool push, float dt, float s) const {
        if (m.wave && !(m.ship || m.ufo || m.swing || m.robot || m.cubeLike)) {
            WinIdx const g = primaryWin(m, true);
            float const nextFrames = push ? rhythm(m_holdLen[g], DEFAULT_TAP_FRAMES)
                                          : rhythm(m_gapLen[g], DEFAULT_GAP_FRAMES);
            int const next = std::max(1, static_cast<int>(std::lround(nextFrames / dt)));
            return std::clamp(next + scaleTicks(WAVE_GRACE_BASE, s), scaleTicks(6, s), scaleTicks(ESCAPE_COMMIT_BASE, s));
        }
        return scaleTicks(ESCAPE_COMMIT_BASE, s);
    }

    // Wave / ship (2.1): an input that arrives while the one before it is held
    // (or after it fired late) - e.g. your release behind a buffered press. It
    // can go in on time (as you made it) or keep your spacing (moved by the
    // same delay). Spacing used to be automatic: a +18t press moved its release
    // 18t too, and in a tight mini wave / ship gap that was the death. Both are
    // tested from the real state now; on time wins when it's safe (less lag),
    // else spacing, else whichever survives longer. Returns the time to use.
    double pickFollowUpTime(PlayLayer* pl, Config const& c, bool push, double minGap) {
        double const spacing = m_gameTime + m_queue.shift();
        bool const headPending = m_queue.pending();
        if (headPending && m_queue.size() != 1) return spacing; // a longer chain: keep it simple
        double const prevAt = headPending ? m_queue.nextAt() : m_queue.lastAt();
        double const onTime = std::max(m_gameTime, prevAt + minGap);
        Ctx x;
        bool const headPush = headPending ? m_queue.nextPush() : push;
        if (!makeCtx(pl, headPush, c, x)) return spacing;
        if (!(x.mode.wave || x.mode.ship) || x.modern) return spacing;
        float const dt = x.dt;
        if (spacing - onTime < 0.5 * static_cast<double>(dt)) return spacing; // the same tick anyway
        double const gridStart = m_gameTime - static_cast<double>(tickPhase()) * dt;
        auto ticksAt = [&](double t) {
            return std::max(0, static_cast<int>(std::ceil((t - gridStart) / static_cast<double>(dt) - 1e-3)));
        };
        int const commit = holdCommit(x.mode, push, dt, x.s);
        int const budget = clickBudgetUs(c);
        m_hasDeadline = budget > 0;
        if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budget);
        bool const rec = m_recordPath;
        m_recordPath = false;
        constexpr int SAFE = 1 << 30;
        // ticks it survives after the follow-up (SAFE: through its commit time; -1: unknown)
        auto test = [&](double at) -> int {
            SimResult r;
            int follow = 0;
            if (headPending) {
                int const dh = ticksAt(prevAt);
                follow = std::max(dh + 1, ticksAt(at));
                FollowUp f;
                f.at[0] = follow;
                f.held[0] = push;
                Ctx y = x;
                y.horizon = std::clamp(follow + commit + 1 - dh, MIN_HORIZON, MAX_SIM_STEPS);
                r = runSim(y, dh, f);
            } else {
                follow = ticksAt(at);
                Ctx y;
                if (!makeCtx(pl, push, c, y)) return -1;
                y.horizon = std::clamp(commit + 1, MIN_HORIZON, MAX_SIM_STEPS);
                r = runSim(y, follow);
            }
            if (r.end == SimEnd::OutOfBudget) return -1;
            if (r.end != SimEnd::Died) return SAFE;
            return std::max(0, r.deathStep - follow);
        };
        // (2.2) Your spacing first: the follow-up keeps the held input's shift,
        // so the hold / gap is exactly what you made. "On time when safe" (2.1)
        // shortened a delayed release's gap every time - in spam that added up
        // and shot you up. On time only when keeping the spacing dies and on
        // time doesn't (or lives longer) - the 2.1 case: a +18t mini-wave
        // press whose release, moved 18t too, killed you.
        double pick = spacing;
        int const b = test(spacing);
        if (b != SAFE) {
            int const a = test(onTime);
            if (a == SAFE || (a >= 0 && b >= 0 && a > b)) pick = onTime;
        }
        m_recordPath = rec;
        m_hasDeadline = false;
        if (pick == onTime) m_followOnTime++;
        else m_followSpacing++;
        return pick;
    }

    bool makeCtx(PlayLayer* pl, bool push, Config const& c, Ctx& x) {
        x = Ctx{};
        x.pl = pl;
        bool const isDual = pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr;
        bool const twoPlayer = pl->m_level && pl->m_level->m_twoPlayerMode;
        x.p1 = pl->m_player1;
        x.p2 = isDual ? pl->m_player2 : nullptr;
        x.simP2 = isDual && !twoPlayer;
        if (!x.p1 || x.p1->m_isDead || x.p1->m_isPlatformer || (x.p2 && x.p2->m_isPlatformer)) return false;
        x.push = push;
        x.mode.add(x.p1);
        x.mode.add(x.p2);
        x.dt  = getValidDt();
        x.sdt = simDt();
        x.s   = BASE_DT / x.dt;
        x.horizon = std::clamp(static_cast<int>(std::lround(x.mode.baseHorizon() * x.s * m_horizonScale)),
                               MIN_HORIZON, MAX_SIM_STEPS);
        x.noLanding = x.mode.noLanding();
        x.window = x.mode.windowTicks(effectiveCfg(c), push, x.dt);
        if (c.custom) x.window = std::min(x.window, std::max(1, scaleTicks(c.maxTicks, x.s))); // Custom Max (2.2)

        m_orbPredict = c.orbPredict;
        m_padPredict = c.padPredict;
        m_predictPortals = c.predictPortals;
        m_orbIntent  = scaleTicks(ORB_INTENT_BASE, x.s);
        m_orbClick   = scaleTicks(ORB_CLICK_BASE, x.s);

        // Cube / ball / spider press: in 2.2 holding re-jumps on landing, so a sim
        // that never lets go predicts bunny-hops you don't do. Let go after your
        // usual tap length instead (measured, default ~67 ms).
        bool const onlyLanding = x.mode.cubeLike &&
                                 !(x.mode.ship || x.mode.wave || x.mode.ufo || x.mode.swing || x.mode.robot);
        x.tapModel = c.realTaps && push && onlyLanding;
        x.tapTicks = std::max(scaleTicks(2, x.s), static_cast<int>(std::lround(
                         rhythm(m_holdLen[W_CUBE], DEFAULT_TAP_FRAMES) / x.dt)));

        // Later inputs change the path in these modes ("could you still save it").
        x.canEscape = x.mode.wave || x.mode.ship || x.mode.robot || x.mode.ufo || x.mode.swing;
        // Cube / ball / spider / robot: a click is judged on reaching the ground
        // again; a death after that landing is your next jump's job.
        x.landRule = !x.noLanding;
        x.modern = modernEngine(x.mode);

        // Wave (2.1): a wave input only answers for the stretch until your next
        // input (your own rhythm) plus a short grace - after that, where you go
        // is the next input's job (same rule savableLater uses). So the
        // lookahead only needs to reach a bit past that: 2x the commit time,
        // at least 26 base ticks (the accuracy check reads 24). Was 65 base
        // ticks (50 mini) every test - most of the work, and the frame-time
        // spikes while spamming.
        if (x.mode.wave && !(x.mode.ship || x.mode.ufo || x.mode.swing || x.mode.robot || x.mode.cubeLike)) {
            x.waveCommit = holdCommit(x.mode, push, x.dt, x.s);
            int const want = std::max({2 * x.waveCommit, x.waveCommit + scaleTicks(8, x.s), scaleTicks(26, x.s)});
            x.horizon = std::clamp(want, MIN_HORIZON, x.horizon);
        }
        return true;
    }

    long long   m_simSizeSwitches = 0;   // size portals passed inside predictions
    long long   m_simAnticheat = 0;      // (2.3) touches of GD's anticheat spike ignored
    long long   m_simOffscreenUsed = 0;  // (2.3) objects GD had switched off (off screen) the predictions used
    // (2.3) portals the predictions went through (Dev tab)
    long long   m_simSpeedSwitches = 0, m_simGravFlips = 0, m_simModeSwitches = 0, m_simMirrors = 0;
    bool        m_simBandOwn = false;    // this test went through a gamemode portal: it keeps its own band
    bool        m_predictPortals = true;
    int         m_simChainStop = 0;      // > 0: a test ends (alive) this many ticks after reaching the next orb (unused since 1.25.9)

    SimResult runSim(Ctx const& x, int delay, FollowUp const& f = FollowUp{}, int h = -1) {
        int const hz = (h > 0) ? std::min(h, x.horizon) : x.horizon;
        m_simModern = x.modern;                         // near-miss clearance: current engine only
        if (x.tapModel && f.at[0] < 0) {
            FollowUp t;
            t.at[0] = delay + x.tapTicks;
            t.held[0] = false;
            return simulate(x.pl, x.p1, x.p2, x.simP2, x.push, delay, hz, x.noLanding, x.sdt, x.s, t);
        }
        return simulate(x.pl, x.p1, x.p2, x.simP2, x.push, delay, hz, x.noLanding, x.sdt, x.s, f);
    }

    // "Could the player still save this themselves?" for a test `r` that died.
    //  * cube / ball / spider / robot: died after landing again -> yes (next jump).
    //  * ship / wave / robot / UFO / swing (2.3): a real search for a way out
    //    with follow-up inputs a person can make (acecore::findEscape): the
    //    first no sooner than your own rhythm, a few ticks apart, at most 3,
    //    and the first one still has to work a little late. `base`: inputs
    //    already known (your queued release), played before the search's own.
    //    It used to try 1-3 guessed moments with one follow-up each (plus one
    //    "cycle"), and called every ship crash past 100 ms avoidable unchecked:
    //    clicks you could save looked deadly and were held back (late wave /
    //    ship releases), and some that nothing saves went in raw.
    bool savableLater(Ctx const& x, int delay, SimResult const& r, int samples, bool& budgetHit,
                      FollowUp const& base = FollowUp{}) {
        if (r.end != SimEnd::Died) return false;
        if (x.landRule && r.landTick >= 0 && r.ticks > r.landTick + scaleTicks(LAND_GRACE_BASE, x.s)) return true;
        if (!x.canEscape) return false;
        int const deadTicks = r.ticks; // ticks it lived after the click went in
        int commit = scaleTicks(ESCAPE_COMMIT_BASE, x.s);
        int react  = scaleTicks(ESCAPE_FOLLOW_BASE, x.s); // earliest follow-up we assume
        // Ship / wave: your next input comes at YOUR rhythm (median hold after a
        // press, gap after a release), often well under 50 ms.
        if (x.mode.wave || x.mode.ship) {
            WinIdx const g = primaryWin(x.mode, true);
            float const nextFrames = x.push ? rhythm(m_holdLen[g], DEFAULT_TAP_FRAMES)
                                            : rhythm(m_gapLen[g], DEFAULT_GAP_FRAMES);
            int const next = std::max(1, static_cast<int>(std::lround(nextFrames / x.dt)));
            react  = std::clamp(next, scaleTicks(2, x.s), react);
            commit = std::clamp(next + scaleTicks(WAVE_GRACE_BASE, x.s), scaleTicks(6, x.s), commit);
        }
        // Cheap mode (out of time on a heavy level): no follow-up tests, assume a
        // death after the commit time is yours to steer out of.
        if (samples < 0) return deadTicks > commit;

        // UFO / swing press: a follow-up is another tap (let go, press a tick later)
        bool const tapFollow = x.push && (x.mode.ufo || x.mode.swing) && !(x.mode.ship || x.mode.wave || x.mode.robot);
        int const tapGap = scaleTicks(1, x.s);
        // the known inputs, as ticks after the click
        int const nBase = base.count();
        int lastBase = -1000000;
        bool heldAfterBase = x.push;
        for (int k = 0; k < nBase; k++) {
            lastBase = std::max(lastBase, base.at[k] - delay);
            heldAfterBase = base.held[k];
        }
        int const space = std::max(scaleTicks(2, x.s), tapFollow ? tapGap + scaleTicks(2, x.s) : react / 2);

        acecore::EscapeParams ep;
        ep.first = std::max(react, nBase > 0 ? lastBase + space : react);
        ep.space = space;
        ep.lead = 1;
        ep.grace = scaleTicks(WAVE_GRACE_BASE, x.s);
        ep.maxChanges = 3;
        ep.maxRuns = samples >= 3 ? 14 : samples == 2 ? 8 : 4; // (the delay search tries many delays: 4 each)
        auto run = [&](std::vector<int> const& changes) -> acecore::EscapeTry {
            FollowUp f = base;
            bool held = heldAfterBase;
            for (int t : changes) {
                int const at = delay + t;
                if (tapFollow) {
                    if (!f.add(at, false) || !f.add(at + tapGap, true)) return acecore::EscapeTry{acecore::EscapeEnd::Died, t};
                } else {
                    held = !held;
                    if (!f.add(at, held)) return acecore::EscapeTry{acecore::EscapeEnd::Died, t};
                }
            }
            SimResult const re = runSim(x, delay, f);
            m_escapeRuns++;
            if (re.end == SimEnd::OutOfBudget) return acecore::EscapeTry{acecore::EscapeEnd::Budget, 0};
            if (re.end == SimEnd::Survived || re.end == SimEnd::Truncated) return acecore::EscapeTry{acecore::EscapeEnd::Survived, 0};
            return acecore::EscapeTry{acecore::EscapeEnd::Died, re.ticks};
        };
        acecore::EscapeResult const er = acecore::findEscape(ep, deadTicks, run);
        switch (er.escape) {
            case acecore::Escape::Yes:
                m_escapeYes++;
                return true;
            case acecore::Escape::No:
                m_escapeNo++;
                return false;
            case acecore::Escape::Unknown:
            default:
                if (budgetExpired()) { budgetHit = true; return false; } // out of CPU time
                m_escapeUnknown++;
                return deadTicks > commit; // out of tries: the old rule of thumb
        }
    }
    long long m_escapeRuns = 0, m_escapeYes = 0, m_escapeNo = 0, m_escapeUnknown = 0;

    // Verdict for a test `rd` of the input delayed by d. `r0Ticks`: how far the
    // undelayed input got (a cut-short path must get clearly further to count).
    acecore::Verdict verdictFor(Ctx const& x, int d, SimResult const& rd, int r0Ticks, bool& budgetHit, int samples = 2,
                               FollowUp const& base = FollowUp{}) {
        switch (rd.end) {
            case SimEnd::OutOfBudget: return acecore::Verdict::Budget;
            case SimEnd::Survived:    return acecore::Verdict::Good;
            case SimEnd::Truncated:
                // Wave release (2.1): the path survived up to an orb / portal we
                // can't see past - the release itself didn't kill you, so let it
                // go. Holding it there kept the wave rising until the window ran
                // out (late / "ignored" releases near portals and orbs).
                if (!x.push && x.mode.wave && !x.mode.ship) return acecore::Verdict::Good;
                // Safe until an orb / portal we can't see past, and clearly further than frame 0.
                return rd.ticks > r0Ticks + scaleTicks(6, x.s) ? acecore::Verdict::Partial : acecore::Verdict::Bad;
            case SimEnd::Died:
            default: {
                bool const esc = savableLater(x, d, rd, samples, budgetHit, base);
                if (budgetHit) return acecore::Verdict::Budget;
                return esc ? acecore::Verdict::Good : acecore::Verdict::Bad;
            }
        }
    }

    // ------------------------------------------------------------------------
    //  Click rate (presses per second), for the Raw Wave Spam threshold.
    //  Game time is in 60fps frames, so 60 units = one second.
    // ------------------------------------------------------------------------
    void notePressRate(double now) {
        m_pressAt[m_pressAtI] = now;
        m_pressAtI = (m_pressAtI + 1) % PRESS_RATE_N;
        if (m_pressAtN < PRESS_RATE_N) m_pressAtN++;
    }

    // Presses per second over the last second of presses; 0 while there are too
    // few to judge (the first few taps of a burst are still corrected).
    double pressCps() const {
        int const n = std::min(m_pressAtN, PRESS_RATE_N);
        if (n < 2) return 0.0;
        int used = 0;
        double oldest = 0.0, newest = 0.0;
        for (int k = 0; k < n; k++) {
            double const t = m_pressAt[(m_pressAtI - 1 - k + PRESS_RATE_N * 2) % PRESS_RATE_N];
            if (m_gameTime - t > 60.0) break; // only the last second
            if (used == 0) newest = t;
            oldest = t;
            used++;
        }
        if (used < 4) return 0.0;          // need a real burst before going raw
        double const span = newest - oldest;
        if (span < 12.0) return 0.0;       // under 0.2 s of data: not enough yet
        return static_cast<double>(used - 1) * 60.0 / span;
    }

    // ------------------------------------------------------------------------
    //  Input interception (called from GJBaseGameLayer::handleButton)
    //  returns true  -> let the game handle the input now
    //          false -> swallow it (it is buffered and will be re-fired)
    // ------------------------------------------------------------------------
    bool interceptInput(PlayLayer* pl, bool push, bool isPlayer1) {
        bool const isDual = pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr;
        bool const isTwoPlayerControl = pl->m_level && pl->m_level->m_twoPlayerMode;
        // Which inputs are "yours"? GD (and keybind / remap mods) can deliver a
        // jump with either player flag, depending on the key. So:
        //   * two-player levels: only player-1 inputs are buffered; player 2 is
        //     another person and stays raw;
        //   * one-player dual: a player-2 event right after a matching player-1
        //     event is GD's mirror of it and follows it; otherwise it's yours;
        //   * everything else: any flag is yours (mouse, space, W, up, custom keys).
        if (!isPlayer1) {
            if (isTwoPlayerControl) return true;
            bool const mirror = isDual && m_lastPrimaryTick == m_tick && m_lastPrimaryPush == push;
            if (mirror) return !m_queue.active(); // held with P1's input, else straight through
        }
        m_lastPrimaryTick = m_tick;
        m_lastPrimaryPush = push;

        // Same state twice (key auto-repeat, or a second jump key while one is
        // held): GD sees it as it would without the mod, but it is never a new
        // click for the buffer, and never jumps ahead of a held input.
        if (push == m_physicalHolding) {
            // Swallow only while an input is actually waiting to fire (so it
            // can't jump ahead of it). Otherwise let it through: the "held"
            // state may be stale (a release made in the pause menu never
            // reaches us), and swallowing then ate the player's next real press.
            if (m_queue.pending()) return false;
            m_gameHolding = push;
            return true;
        }

        // Always record the true physical state first.
        m_physicalHolding = push;
        m_inputsSeen++;
        if (push) notePressRate(m_gameTime); // for the Raw Wave Spam threshold

        // A pending accuracy check assumed no further input: drop it (a release
        // only counts where releasing changes the path).
        {
            PlayerObject* rp = pl->m_player1;
            bool const relMatters = rp && (rp->m_isDart || rp->m_isShip || rp->m_isRobot);
            if (push || relMatters) {
                m_tracker.invalidate();
            }
        }

        Config const& c = cfg();
        m_active   = allowedHere(c, pl);
        m_enabled  = m_active;
        m_logDebug = c.debug;
        m_logCsv   = c.csv;
        if (!m_enabled) {
            clearBuffer();
            m_gameHolding = push;
            return true;
        }
        // Dead (death animation, respawn): straight through, nothing to check.
        // (2.1: before the hook check - clicks while dead looked like a
        // missing physics hook.)
        PlayerObject* p1 = pl->m_player1;
        PlayerObject* p2 = isDual ? pl->m_player2 : nullptr;
        if (!p1 || p1->m_isDead) {
            clearBuffer();
            m_gameHolding = push;
            return true;
        }

        // The hook that fires held inputs isn't running (another mod replaced
        // it, or the game is stalled): never hold an input we couldn't re-send.
        // Only reported when frames keep running without physics steps while
        // you're alive (2.1) - not for the moment after a respawn / unpause.
        if (!stepsRunning()) {
            if (m_queue.pending()) flushQueue(pl);
            clearBuffer();
            m_gameHolding = push;
            if (m_framesSinceStep >= STALL_FRAMES && m_hookMissing++ == 0) {
                pushLog("NOTE: the physics hook isn't running, so the buffer can't hold inputs - they go straight "
                        "through. Another mod (TPS / physics bypass, speedhack) is probably replacing it.",
                        COL_ORANGE, false);
            }
            return true;
        }

        // Platformer inputs depend on held directions the sim does not model.
        if (p1->m_isPlatformer || (p2 && p2->m_isPlatformer)) {
            clearBuffer();
            m_gameHolding = push;
            return true;
        }

        ModeInfo mode;
        mode.add(p1);
        mode.add(p2);

        // Wave / ship stream: inputs closer together than STREAM_GAP_FRAMES are
        // one stream (spam). A pause ends it and drops the steady shift.
        // (2.2: ship too - its spam drifted up the same way.)
        bool inStream = false;
        if (mode.holdBased()) {
            double const since = m_gameTime - m_streamLast;
            inStream = since >= 0.0 && since < STREAM_GAP_FRAMES; // (2.3: never across a respawn's clock reset)
            m_streamLast = m_gameTime;
        }
        if (!inStream) m_streamShift = 0.0;

        // Rhythm: hold length (press -> release) and gap (release -> press), per mode.
        {
            WinIdx const g = primaryWin(mode, true);
            if (push) {
                if (m_lastReleaseAt >= 0.0) {
                    double const gap = m_gameTime - m_lastReleaseAt;
                    if (gap > 0.0 && gap < 60.0) m_gapLen[g].add(static_cast<float>(gap));
                }
                m_lastPressAt = m_gameTime;
            } else {
                if (m_lastPressAt >= 0.0) {
                    double const hold = m_gameTime - m_lastPressAt;
                    if (hold > 0.0 && hold < 60.0) m_holdLen[g].add(static_cast<float>(hold));
                }
                m_lastReleaseAt = m_gameTime;
            }
        }

        float const dt  = getValidDt(); // nominal tick, 60fps-frame units (time warp removed)
        float const s = BASE_DT / dt;   // real ticks per 240-TPS base tick

        int horizon = static_cast<int>(std::lround(mode.baseHorizon() * s * m_horizonScale));
        horizon = std::clamp(horizon, MIN_HORIZON, MAX_SIM_STEPS);

        // Custom Max (2.2, Stage tab, Custom stage only): a hard cap - the
        // window, held clicks, the orb helper and snap to scan never go past it.
        int const maxCap = c.custom ? std::max(1, scaleTicks(c.maxTicks, s)) : (1 << 20);
        int const windowTicks = std::min(mode.windowTicks(effectiveCfg(c), push, dt), maxCap);
        auto capT = [&](int t) { return std::min(t, maxCap); };

        ClickNote base;
        base.push = push;
        base.windowTicks = windowTicks;
        base.horizon = horizon;

        auto pass = [&](Decision d) {
            m_gameHolding = push;
            if (d != Decision::PassRelease && d != Decision::PassModeOff) rememberInput(pl, push);
            ClickNote n = base;
            n.d = d;
            note(c, mode, dt, n);
            return true;
        };
        // 1a. (2.3) Raw Wave Spam (Tools): in a wave stream every input goes in
        //      exactly as you made it - nothing held, nothing judged. One still
        //      held back from before the stream goes in first (order kept).
        //      Only above 7 clicks/s: at a normal wave rhythm the clicks are still
        //      corrected, which is the point of the mod. (1.3.6)
        if (c.rawWaveSpam && mode.wave && inStream && pressCps() >= RAW_SPAM_MIN_CPS) {
            if (m_queue.active()) flushQueue(pl);
            m_streamShift = 0.0;
            return pass(Decision::PassSpam);
        }
        // 1. An input is already held back: queue this one behind it so taps are never eaten.
        if (m_queue.active()) {
            // Keep order and spacing: replay it after the held input with the same shift.
            // Wave / ship (2.1): on time instead, when that's the safe one.
            double const minGap = static_cast<double>(dt) * 0.999;
            bool placed = false;
            // (2.2: releases no longer snap to the scan unchecked - they keep
            // your spacing from the held press, like any follow-up.)
            if (!placed && push != m_queue.lastPush() && mode.holdBased() && !modernEngine(mode)) {
                placed = m_queue.pushAt(push, pickFollowUpTime(pl, c, push, minGap), m_gameTime, minGap);
            }
            if (!placed) m_queue.push(push, m_gameTime, minGap);
            // e.g. the release of a press that already fired: it gets the live check too
            if (!m_live && m_queue.pending()) {
                m_live = true;
                m_liveState.needInit = true;
            }
            m_lastInputTime = m_gameTime;
            ClickNote n = base;
            n.d = Decision::Queued;
            n.delay = m_queue.pending()
                ? std::max(0, static_cast<int>(std::ceil((m_queue.nextAt() - m_gameTime) / dt - 1e-3)))
                : static_cast<int>(std::lround(m_queue.shift() / dt));
            note(c, mode, dt, n);
            return false;
        }

        // 1b. Gamemode switched off in the Modes tab: raw input.
        {
            std::string const id = modeId(p1);
            if (!modeOn(id) || (p2 && !modeOn(modeId(p2)))) return pass(Decision::PassModeOff);
        }

        // 2. Releases only matter in hold-based modes (and don't count as spam elsewhere).
        if (!push && !mode.releaseMatters()) {
            return pass(Decision::PassRelease);
        }

        // 2a. Orb helper (2.1, Tools tab): a press made a little before an orb
        //     waits for the orb and goes in the tick you touch it - for orb
        //     double-clicks and UFO / swing orbs that need a fresh click. Only
        //     when not pressing until then is safe, and the orb press is too.
        if (push && c.orbHelper && !mode.wave && !touchingOrb(p1)) {
            Ctx ox;
            if (makeCtx(pl, true, c, ox)) {
                int const look = capT(scaleTicks(ORB_HELP_BASE, s));
                int const budget = clickBudgetUs(c);
                m_hasDeadline = budget > 0;
                if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budget);
                bool const rec = m_recordPath;
                m_recordPath = false;
                int k = -1;
                RingObject* ring = nullptr;
                // (2.2) Never take over a good click: a press that survives as
                // made goes in as made - unless it's a cube / ball / spider press
                // in the air, which does nothing until an orb anyway. (On by
                // default since 2.2, it held normal jumps near orbs for the orb.)
                bool const airPress = mode.cubeLike && !(mode.ship || mode.ufo || mode.swing || mode.robot) &&
                                      !p1->m_isOnGround;
                bool helpOk = airPress;
                if (!helpOk) {
                    SimResult const now = runSim(ox, 0);
                    bool bh0 = false;
                    acecore::Verdict const v0 = verdictFor(ox, 0, now, -1000000, bh0);
                    helpOk = now.end != SimEnd::OutOfBudget && v0 != acecore::Verdict::Good;
                }
                SimResult idle;
                idle.end = SimEnd::OutOfBudget; // "not looked" unless the helper may act
                if (helpOk) idle = runSim(ox, look + 1, FollowUp{}, 1); // no press within `look`
                if (idle.end != SimEnd::OutOfBudget && idle.firstRing && idle.firstRingTick >= 1 &&
                    idle.firstRingTick <= look &&
                    !(idle.end == SimEnd::Died && idle.deathStep >= 0 && idle.deathStep <= idle.firstRingTick)) {
                    k = idle.firstRingTick;
                    ring = idle.firstRing;
                    SimResult const atOrb = runSim(ox, k);
                    bool bh = false;
                    acecore::Verdict const v = verdictFor(ox, k, atOrb, -1000000, bh);
                    if (!(v == acecore::Verdict::Good || v == acecore::Verdict::Partial)) k = -1;
                } else if (idle.end == SimEnd::Truncated && idle.trunc.obj && isOrb(idle.trunc.type) &&
                           !isModelledOrb(idle.trunc.type) && idle.trunc.tick >= 0) {
                    // (2.2) Dash / spider / teleport / custom orbs: the prediction
                    // can't play them (it stops right before one), so an early
                    // click went through raw and missed the orb. Now: if doing
                    // nothing gets you to the orb alive, the press waits and goes
                    // in the tick you touch it - what you clicked for.
                    float const step = std::max(0.01f, m_p1Speed * ox.dt);
                    int const extra = idle.trunc.gap > 0.f ? static_cast<int>(std::ceil(idle.trunc.gap / step)) : 0;
                    int const kk = idle.trunc.tick + std::max(1, extra);
                    auto* r = typeinfo_cast<RingObject*>(idle.trunc.obj);
                    if (r && kk >= 1 && kk <= look && !realUsedOrb(r)) {
                        k = kk;
                        ring = r;
                    }
                }
                m_recordPath = rec;
                m_hasDeadline = false;
                if (k > 0 && ring) {
                    double const gridStart = m_gameTime - static_cast<double>(tickPhase()) * dt;
                    auto at = [&](double ticks) { return gridStart + ticks * dt; };
                    m_planTrust = false;
                    m_liveSpotHold = false;
                    m_liveRescue = false;
                    m_orbHelpHold = true;
                    m_orbHelpRing = ring;
                    m_queue.start(true, m_gameTime, std::max(0.0, at(k) - m_gameTime));
                    m_live = true;
                    m_liveState.startClick(m_gameTime, dt, at(k), at(capT(k + scaleTicks(2, s))) + TIME_EPS, at(k) + TIME_EPS);
                    m_liveArrive = m_gameTime;
                    m_livePlanTicks = k;
                    m_liveWaited = false;
                    m_liveWaitWhy = 0;
                    for (int& v : m_refuse) v = 0;
                    m_refuseHave = false;
                    m_last.liveTicks = -1;
                    m_last.liveReason = acecore::FireReason::None;
                    ClickNote n = base;
                    n.d = Decision::Buffered;
                    n.delay = k;
                    n.orbHelp = true;
                    note(c, mode, dt, n);
                    return false;
                }
            }
        }

        // 2b. Wave spam: keep ONE steady shift for the whole stream. Jitter came
        //     from every input being judged on its own - one delayed 3 ticks,
        //     the next raw, the next 2 - so your holds came out uneven. Now an
        //     input in a stream that already has a shift gets exactly the same
        //     shift, on a fixed schedule (no live re-timing), until you pause.
        if (inStream && m_streamShift > TIME_EPS) {
            Ctx sx;
            if (makeCtx(pl, push, c, sx)) {
                // Keep the stream's shift exactly (it only resets after a pause):
                // shrinking it 1 tick per tap shortened a gap by a tick each time.
                int const S = std::min(maxCap, std::max(0, static_cast<int>(std::lround(m_streamShift / dt))));
                int const budgetUs0 = std::min(clickBudgetUs(c) > 0 ? clickBudgetUs(c) : STREAM_BUDGET_US, STREAM_BUDGET_US);
                m_hasDeadline = budgetUs0 > 0;
                if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budgetUs0);
                m_recordPath = false;
                auto okAt = [&](int d) {
                    SimResult const r = runSim(sx, d);
                    bool bh = false;
                    acecore::Verdict const v = verdictFor(sx, d, r, -1000000, bh);
                    return v == acecore::Verdict::Good || v == acecore::Verdict::Partial;
                };
                // (2.3) On time first, presses and releases alike: an input in
                // a stream goes in when you made it whenever that's safe, and
                // only keeps the stream's shift when on time dies. Keeping the
                // shift for every input (2.2's "balanced" rule) made every
                // press of a spam late, and on a fixed tick schedule - which
                // also threw away Click Between Frames' in-between-ticks timing,
                // so the same spam came out different each time. (The drift
                // the balanced rule paid back only built up while most inputs
                // were shifted; with on time first few are.)
                int chosen = -1;
                if (okAt(0)) chosen = 0;
                else if (S > 0 && okAt(S)) chosen = S;
                m_hasDeadline = false;
                if (chosen == 0) {
                    m_streamShift = 0.0;
                    return pass(Decision::PassSafe); // stream wound down to no shift
                }
                if (chosen > 0) {
                    m_streamShift = static_cast<double>(chosen) * dt;
                    double const gridStart = m_gameTime - static_cast<double>(tickPhase()) * dt;
                    m_queue.start(push, m_gameTime, std::max(0.0, gridStart + static_cast<double>(chosen) * dt - m_gameTime));
                    m_live = false; // fixed schedule: the stream keeps its rhythm
                    ClickNote n = base;
                    n.d = Decision::Buffered;
                    n.delay = chosen;
                    n.stream = true;
                    note(c, mode, dt, n);
                    return false;
                }
                // neither keeps you alive: judge this one normally below
            }
        }

        // 3. Streak-based spam guard (real-time interval, off in Extreme).
        if (mode.spamGuarded() && !c.extreme) {
            double const interval = m_gameTime - m_lastInputTime;
            if (interval < static_cast<double>(c.spamFrames)) m_streak++;
            else m_streak = 0;
            m_lastInputTime = m_gameTime;
            if (m_streak >= c.spamStreak) return pass(Decision::PassSpam);
        } else {
            m_streak = 0;
            m_lastInputTime = m_gameTime;
        }

        // 4. Window shorter than one physics tick (e.g. 0.5f at 60 TPS): nothing to do.
        if (windowTicks <= 0) return pass(Decision::PassNoWindow);
        int maxDelay = windowTicks;
        // Wave / ship (2.1): never hold an input longer than most of the gesture
        // it starts (your usual hold / gap) - see HOLD_DELAY_FRAC.
        if (mode.holdBased() && !modernEngine(mode)) {
            WinIdx const g = primaryWin(mode, true);
            float const opp = push ? rhythm(m_holdLen[g], DEFAULT_TAP_FRAMES) : rhythm(m_gapLen[g], DEFAULT_GAP_FRAMES);
            int const cap = std::max(scaleTicks(HOLD_DELAY_MIN_BASE, s),
                                     static_cast<int>(std::lround(HOLD_DELAY_FRAC * opp / dt)));
            maxDelay = std::min(maxDelay, cap);
        }
        // Ship, light touch (2.2): small corrections only, on every stage.
        if (mode.ship && !mode.wave) maxDelay = std::min(maxDelay, scaleTicks(SHIP_MAX_DELAY_BASE, s));
        // (2.3) Ship taps stay taps: a release is held at most a third of how
        // long you held the button. Up to 8 ticks on a 2-3 frame tap nearly
        // doubled it - the ship rose more than you tapped for (straight fly).
        if (mode.ship && !mode.wave && !push && m_lastPressAt >= 0.0) {
            int const heldTicks = static_cast<int>((m_gameTime - m_lastPressAt) / dt);
            maxDelay = std::min(maxDelay, std::max(1, heldTicks / 3));
        }
        // Robot release (2.2): letting go ends the jump's boost - a late
        // release is a higher jump. Small corrections only, like the ship.
        if (mode.robot && !push) maxDelay = std::min(maxDelay, scaleTicks(SHIP_MAX_DELAY_BASE, s));
        // Mini wave (2.2): twice as steep, so a tick of delay moves you twice as
        // far - half the hold for the same distance (never under 2 ticks).
        if (mode.miniWave && !mode.ship) maxDelay = std::max(std::min(maxDelay, scaleTicks(2, s)), maxDelay / 2);
        // Aceware: any release is only ever nudged. Two frames on Extreme is
        // already a lot for a let-go, and ship/robot have their own tighter cap.
        if (!push && c.extreme) maxDelay = std::min(maxDelay, scaleTicks(8, s));
        // Custom Min (2.2, Custom stage only): a click that has to move moves
        // at least this far (never past the window / Custom Max).
        int const minDelay = c.custom ? std::clamp(scaleTicks(c.minTicks, s), 1, std::max(1, maxDelay)) : 1;

        // CPU budget: the manual cap, further capped to a share of the frame time
        // when Auto is on (extreme gets a bigger share since it searches more).
        int budgetUs = clickBudgetUs(c);
        // (2.3) in a wave / ship stream (spam): a tight budget, no 6 ms floor
        if (inStream) budgetUs = std::min(budgetUs > 0 ? budgetUs : STREAM_BUDGET_US, STREAM_BUDGET_US);
        m_lastBudgetUs = budgetUs;

        auto const t0 = Clock::now();
        m_hasDeadline = budgetUs > 0;
        if (m_hasDeadline)
            m_deadline = t0 + std::chrono::microseconds(inStream ? budgetUs : std::max(budgetUs, R0_BUDGET_MIN_US));

        bool const holdBased = mode.holdBased();
        m_recordPath = c.accuracy;
        // Show Rays (2.2): keep every test's path for "what the buffer thought"
        m_thinkTests.clear();
        m_thinkRec = c.rayView && !inStream; // (2.3) not while spamming (CPU)
        if (m_thinkRec) m_recordPath = true;

        Ctx x;
        if (!makeCtx(pl, push, c, x)) return pass(Decision::PassNoWindow);
        base.horizon = x.horizon; // wave: the shorter lookahead (2.1)
        bool budgetHit = false;
        auto sim = [&](int delay, FollowUp const& f = FollowUp{}, int h = -1) { return runSim(x, delay, f, h); };
        auto escapable = [&](int delay, SimResult const& r, int samples) { return savableLater(x, delay, r, samples, budgetHit); };

        // 5. Frame 0 (immediate input).
        SimResult r0 = sim(0);
        std::vector<acecore::Point2> path0;
        if (m_recordPath) path0 = m_path;

        bool discard = (r0.end == SimEnd::OutOfBudget); // result not trustworthy -> raw input
        // The rest (escape check, search) gets a fresh budget of its own, so a
        // slow first test doesn't leave it nothing.
        m_hasDeadline = budgetUs > 0;
        if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budgetUs);
        budgetHit = discard;
        bool escape0 = false;
        bool liveFallback = false; // search ran out of CPU time on a deadly click: re-plan live
        // Out of time on a deadly click: instead of giving up (raw), finish with
        // cheap checks (no follow-up tests) under a hard cap. The original
        // Predictive Buffer never gave up at all - that's why it kept buffering
        // on Grief / Slaughterhouse where this one went "raw: CPU budget".
        bool cheap = false;
        auto enterCheap = [&]() {
            cheap = true;
            budgetHit = false;
            m_cheapClicks++;
            m_hasDeadline = true;
            m_deadline = t0 + std::chrono::microseconds(inStream ? STREAM_HARD_CAP_US : CLICK_HARD_CAP_US);
        };
        int bestDelay = 0;
        int evals = 0;
        if (r0.end == SimEnd::Died) {
            escape0 = escapable(0, r0, 3);
            if (budgetHit) { // ran out asking "could you save it": decide it cheaply instead
                enterCheap();
                escape0 = savableLater(x, 0, r0, -1, budgetHit);
            }
            // Ran out of time asking "could you save it yourself?": the click is
            // deadly as made, so hold it and let the live check (one cheap test
            // per tick) decide, instead of letting a deadly click through.
            // (Ship / wave: raw, as before 1.12.1 - holding a wave press back
            // isn't "safe", it just keeps you flying the other way.)
            if (budgetHit) { if (x.modern) liveFallback = true; else discard = true; }
        }

        // How good is the click if delayed by d ticks? (shared by the search and the probe)
        auto eval = [&](int d) -> acecore::Verdict {
            SimResult rd = sim(d);
            if (m_thinkRec && m_thinkTests.size() < 24) m_thinkTests.emplace_back(d, m_path);
            if (rd.end == SimEnd::OutOfBudget && !cheap) {
                enterCheap();
                rd = sim(d);
            }
            // one "could you still save it" sample per candidate (the first
            // test gets three): the search tries many delays.
            acecore::Verdict v = verdictFor(x, d, rd, r0.ticks, budgetHit, cheap ? -1 : 1);
            if (v == acecore::Verdict::Budget && !cheap && rd.end != SimEnd::OutOfBudget) {
                enterCheap();
                v = verdictFor(x, d, rd, r0.ticks, budgetHit, -1);
            }
            return v;
        };

        // 6. Frame 0 is doomed: find the delay to use.
        int earliest = 0;
        int planTick = 0; // > 0: follow the plan (see below)
        // 6a. Wave rays (2.2): cast the two straight lines your wave can take
        //     and let them pick the delay; one real prediction confirms it
        //     (then up to 3 more find the earliest that still holds). Instead
        //     of the full search's dozen-plus predictions per deadly click -
        //     the cost that made spam hit the CPU limit. If the real
        //     prediction disagrees, the normal search runs as before.
        bool rayDone = false;
        if (r0.end == SimEnd::Died && !escape0 && !discard && waveOnly(mode) && maxDelay > 0) {
            int const pick = wavePick(pl, x, minDelay, maxDelay);
            if (pick > 0) {
                m_rayPicks++;
                if (eval(pick) == acecore::Verdict::Good) {
                    int d = pick;
                    evals++;
                    for (int k = 0; k < 3 && d - 1 >= minDelay; k++) {
                        evals++;
                        if (eval(d - 1) != acecore::Verdict::Good) break;
                        d--;
                    }
                    bestDelay = d;
                    earliest = d;
                    planTick = d; // wave: fire on the first safe tick (same as the search's plan)
                    rayDone = true;
                    m_rayConfirmed++;
                } else {
                    evals++;
                    m_rayRejected++;
                }
            }
        }
        if (r0.end == SimEnd::Died && !escape0 && !discard && !rayDone) {
            acecore::SearchParams sp;
            sp.maxDelay = maxDelay;
            sp.minDelay = minDelay;
            // Every tick near the click (narrow safe gaps live there), then a
            // stride over the rest of the window.
            sp.dense    = scaleTicks(DENSE_SCAN_BASE, s);
            sp.stride   = std::max(1, (std::max(0, maxDelay - sp.dense) + MAX_CANDIDATES - 1) / MAX_CANDIDATES);
            sp.guard    = c.cbfGuard;
            sp.margin   = (c.safetyTicks > 0 && !mode.wave) ? scaleTicks(c.safetyTicks, s) : 0;
            // (2.3) learned margin: deeper into the safe window where moved clicks died anyway
            if (c.learnSpots && !mode.wave) {
                int const lm = static_cast<int>(std::lround(m_lmargin[primaryWin(mode, push)]));
                if (lm > 0) sp.margin = std::max(sp.margin, scaleTicks(lm, s));
            }
            sp.aim      = 0.f; // always the earliest safe tick
            acecore::SearchResult sr = acecore::searchDelay(sp, eval);
            evals = sr.evals;
            if (sr.budget) budgetHit = true;
            if (sr.delay == 0 && sr.budget) {
                // ran out before finding anything: cube-type -> decide live; ship / wave -> raw
                if (x.modern) liveFallback = true; else discard = true;
            }
            bestDelay = sr.delay;
            earliest = sr.earliest;
            // Follow the plan (1.26.1, not mini cube - it keeps its per-tick
            // checks: near-miss waits, learned spots): fire on the tick the
            // per-tick re-check would have fired on - the first safe tick with a
            // safe tick after it (wave: the first safe tick) - instead of
            // re-predicting every tick to find it again.
            if (!x.modern && sr.full && !sr.budget && sr.guarded > 0) {
                planTick = mode.wave ? sr.earliest : sr.guarded;
                bestDelay = planTick;
            }
        }

        // 7. CPU time used. (The lookahead is never shortened to save time: a
        //    short lookahead can't see the next obstacle and calls deadly clicks
        //    "safe". Out of time = this one click goes raw, nothing else.)
        long long const usedUs = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
        // 7b. Nothing in the window saves this click: look further, only to tell
        //     you how early it was (death replay, auto windows). Never applied.
        int needed = -1;   // -1 not probed, 0 nothing found, >0 ticks it needed
        int probeMax = 0;
        // Quick search found nothing: in cube / UFO / robot / ball / spider /
        // swing the click is held and checked EVERY tick instead (the search
        // only checks every tick for the first 12, then every few ticks, so a
        // narrow safe moment far from an early click was stepped over and the
        // click was called "too late"). Ship / wave: holding a press isn't safe.
        // (Not UFO / swing either: in Amethyst's UFO part the prediction was wrong
        // about both pressing and not pressing, so holding the click there made
        // things worse than letting it through. 1.19.2)
        // 1.22.2: REMOVED at the player's request (held clicks felt wrong).
        // Kept as a constant so the code around it stays simple.
        bool const rescueOk = false;
        if (r0.end == SimEnd::Died && !escape0 && !discard && !liveFallback && bestDelay == 0 && !rescueOk &&
            (c.hud || c.debug || c.csv || c.autoWindow)) {
            probeMax = std::min(MAX_SIM_STEPS / 2, std::max(maxDelay * 2, scaleTicks(PROBE_MIN_BASE, s)));
            if (probeMax > maxDelay) {
                bool const savedBudgetHit = budgetHit;
                m_hasDeadline = budgetUs > 0;
                // Log-only ("click was Nt too early"): kept cheap - 1/8 of the
                // budget, 8 coarse candidates, no dense scan.
                if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(std::max(300, budgetUs / 8));
                acecore::SearchParams pp;
                pp.minDelay = maxDelay + 1;
                pp.maxDelay = probeMax;
                pp.stride   = std::max(1, (probeMax - maxDelay + 7) / 8);
                pp.dense    = 0;
                pp.guard    = false;
                acecore::SearchResult pr = acecore::searchDelay(pp, eval);
                needed = pr.earliest;
                budgetHit = savedBudgetHit;
            }
        }
        m_hasDeadline = false;

        // 8. Decide.
        Decision dec;
        if (discard)                          dec = Decision::PassBudget;
        else if (liveFallback && r0.end == SimEnd::Died) dec = Decision::Buffered; // plan +1, the live check decides
        else if (r0.end == SimEnd::Truncated) dec = Decision::PassTruncated;
        else if (r0.end == SimEnd::Died && escape0) dec = Decision::PassEscapable;
        else if (r0.end == SimEnd::Died)      dec = (bestDelay > 0) ? Decision::Buffered : Decision::PassNoFix;
        else                                  dec = Decision::PassSafe;

        // 8b. Learned spots: what really happened on earlier attempts beats the
        //     prediction. A click on a spot that killed you before is held and
        //     re-planned live; a "no fix" click with a spot ahead that worked
        //     before is held for it.
        int spotNote = 0;
        // Session memory (Assists): the two "wait for a remembered spot" paths hold
        // the input, which only preserves your line in modes where holding doesn't
        // change the trajectory - so ship / wave skip them and keep the plain
        // fire-on-the-spot behaviour from liveStep.
        if (c.learnSpots && push && maxDelay > 0 && m_spots.size() > 0 && !discard && !mode.holdBased()) {
            acecore::Spot const here = spotNow(pl, push, mode.name());
            acecore::SpotTol const tol = spotTol();
            if ((dec == Decision::PassSafe || dec == Decision::PassEscapable) &&
                m_spots.at(here.x, here.y, here.vy, here.key, tol) == acecore::SpotHit::Bad) {
                dec = Decision::Buffered;
                bestDelay = std::max(1, bestDelay);
                spotNote = 1;
            } else if (dec == Decision::PassNoFix || dec == Decision::PassTruncated) {
                // An early click the prediction can't line up (blind past a
                // portal / orb, or no safe moment found), with a spot ahead where
                // a press WORKED on an earlier attempt - in any gamemode, e.g. an
                // orb right after a UFO portal: hold it for that spot.
                // Short holds only (1.23.1): holding a press changes your path (in
                // UFO it's a flap that doesn't happen), so after a long hold you
                // reach the spot lower than on the attempt that worked.
                int const holdCap = std::min(maxDelay, scaleTicks(SPOT_HOLD_MAX_BASE, s));
                float const reach = m_p1Speed * dt * static_cast<float>(holdCap);
                float d = m_spots.goodAhead(here.x, here.y, here.key, m_p1Dir, reach, tol, here.anyKey);
                if (d > 0.f && m_p1Speed > 0.01f) {
                    // ...and only if not pressing until then doesn't kill you first.
                    int const k = std::clamp(static_cast<int>(std::lround(d / (m_p1Speed * dt))), 1, holdCap);
                    m_hasDeadline = budgetUs > 0;
                    if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budgetUs);
                    bool const rec = m_recordPath;
                    m_recordPath = false;
                    SimResult const wait = sim(k);
                    m_recordPath = rec;
                    m_hasDeadline = false;
                    if (wait.end == SimEnd::Died && wait.deathStep >= 0 && wait.deathStep < k) d = -1.f;
                }
                if (d > 0.f && m_p1Speed > 0.01f) {
                    dec = Decision::Buffered;
                    bestDelay = std::clamp(static_cast<int>(std::lround(d / (m_p1Speed * dt))), 1, maxDelay);
                    spotNote = 2;
                    m_spotTargetX = here.x + d * m_p1Dir;
                    m_spotTargetDir = m_p1Dir;
                }
            }
        }

        // 8b-pick (Aceware): the search gives the earliest safe tick, but the
        // earliest is not always the best. Look a few ticks ahead - three
        // frames for a press, two ticks for a release - keep only ticks the
        // simulation calls Good, and take the one with the most room around it.
        // Equal choices are broken at random, so a run doesn't lock into one
        // rhythm and the same jump doesn't always get the same tick.
        if (dec == Decision::Buffered && !discard && bestDelay >= 0 && maxDelay > 0) {
            int const frameTicks = std::max(1, static_cast<int>(std::lround(BASE_DT / dt)));
            // More ghosts, not fewer: every tick across the first frame after
            // the earliest safe one, then every second tick out to three frames.
            // Releases stay within a single tick of your own timing.
            std::vector<int> cands;
            if (push) {
                int const denseEnd = std::min(maxDelay, bestDelay + frameTicks);
                for (int d = bestDelay; d <= denseEnd; d++) cands.push_back(d);
                for (int d = denseEnd + 2; d <= bestDelay + 3 * frameTicks && d <= maxDelay; d += 2) cands.push_back(d);
            } else {
                for (int d = bestDelay; d <= bestDelay + 1 && d <= maxDelay; d++) cands.push_back(d);
            }
            struct Pick { int d; float clear; };
            Pick best{bestDelay, -1e9f};
            std::vector<Pick> tied;
            for (int d : cands) {
                m_hasDeadline = budgetUs > 0;
                if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budgetUs);
                bool bh = false;
                SimResult const r = sim(d);
                acecore::Verdict const v = verdictFor(x, d, r, -1000000, bh);
                m_hasDeadline = false;
                if (v != acecore::Verdict::Good) continue;
                float const cl = (r.clear < 1e8f) ? r.clear : 1e7f; // no hazard near = best
                if (cl > best.clear + 0.5f) {
                    best = Pick{d, cl};
                    tied.clear();
                    tied.push_back(best);
                } else if (std::fabs(cl - best.clear) <= 0.5f) {
                    tied.push_back(Pick{d, cl});
                }
            }
            if (!tied.empty()) bestDelay = tied[static_cast<size_t>(std::rand()) % tied.size()].d;
        }

        // 8b1 / 8b2 (orb aim, portal hold) removed in 1.25.9: they held or moved
        // clicks and made the game feel laggy. Orbs are still predicted exactly
        // (learned orb speeds, size portals); nothing holds a click for one.

        // 8c. Rescue: hold it and check every tick for the first safe moment.
        bool rescue = false;
        if (dec == Decision::PassNoFix && rescueOk) {
            dec = Decision::Buffered;
            bestDelay = maxDelay; // no plan: "couldn't check" falls back to the window end
            rescue = true;
        }


        ClickNote n = base;
        n.d = dec;
        n.us = usedUs;
        n.r0 = r0;
        n.delay = bestDelay;
        n.evals = evals;
        n.needed = needed;
        n.rescue = rescue;
        note(c, mode, dt, n);
        if (m_thinkRec) thinkCapture(push, dec, r0, path0, bestDelay, windowTicks, mode.name());
        m_last.needed = needed;
        m_last.probe = probeMax;
        m_last.window = windowTicks;

        // Auto windows: how much later did this click need to be?
        WinIdx const widx = primaryWin(mode, push);
        if (dec == Decision::Buffered && earliest > 0) {
            recordSave(c, widx, static_cast<float>(earliest) * dt);
        } else if (dec == Decision::PassNoFix) {
            recordMiss(c, widx, needed > 0 ? static_cast<float>(needed) * dt : -1.f);
        }

        // Accuracy: compare this prediction with reality 100 ms after the click lands.
        int const checkSteps = scaleTicks(ACCURACY_CHECK_BASE, s);
        if (c.accuracy && dec == Decision::PassSafe && r0.end == SimEnd::Survived && !path0.empty()) {
            m_tracker.start(m_gameTime, dt, path0, checkSteps - 1, ACCURACY_TOL_PX);
            m_trackMode = mode.name();
        }


        if (spotNote == 1) pushLog(fmt::format("{} held: this spot killed you on an earlier attempt", push ? "PRESS" : "REL"), COL_CYAN, false);
        if (spotNote == 2) pushLog(fmt::format("{} held for a spot that worked on an earlier attempt (+{}t)", push ? "PRESS" : "REL", bestDelay), COL_CYAN, false);
        if (spotNote) m_spotSaves++;

        if (dec == Decision::Buffered) {
            // Times on the real tick grid: the current tick started `grid` ago,
            // and delay d means "at the start of the d-th tick from now" - what
            // the sim tested. (Before 1.23.3 a mid-tick click fired up to a
            // whole tick later than planned with Click Between Frames.)
            double const gridStart = m_gameTime - static_cast<double>(tickPhase()) * dt;
            auto at = [&](double ticks) { return gridStart + ticks * dt; };
            // Follow the plan: one prediction now, recording where you'll be on
            // every tick until it fires (and after) - replaces a full prediction
            // per tick while it's held.
            m_planTrust = false;
            if (planTick > 0 && planTick == bestDelay && !liveFallback && !rescue && spotNote == 0) {
                bool const rec = m_recordPath;
                m_recordPath = true;
                m_hasDeadline = false;
                SimResult const pr = sim(planTick);
                m_recordPath = rec;
                if (pr.end != SimEnd::OutOfBudget && static_cast<int>(m_path.size()) >= planTick) {
                    m_planPath = m_path;
                    m_planGrid = gridStart;
                    m_planTrust = true;
                }
            }
            m_queue.start(push, m_gameTime, std::max(0.0, at(bestDelay) - m_gameTime));
            m_live = true;
            // If the live check keeps refusing until the click-time plan, fire ON
            // the plan: the search found it safe, and in a narrow gap even one
            // tick later runs you into the obstacle.
            // No plan to fall back on (out of CPU / memory hold): give up after a
            // few ticks instead of holding it for the whole window.
            // Custom Max (2.2): never past it, whatever holds the click.
            auto capD = [&](int t) { return std::min(t, maxCap); };
            double latest = at(capD(std::max(1, scaleTicks(LIVE_GIVEUP_BASE, s)))) + TIME_EPS;
            if (!liveFallback && spotNote != 1 && bestDelay > 0) {
                latest = at(bestDelay) + TIME_EPS; // exactly on the plan
            }
            // Held for a learned spot: fire on the spot, or 2 ticks after its
            // estimated arrival at the latest (the estimate uses your speed).
            m_liveSpotHold = (spotNote == 2);
            m_orbHelpHold = false;
            if (spotNote == 2) latest = at(capD(bestDelay + scaleTicks(2, s))) + TIME_EPS;
            if (rescue) {
                // No plan to fall back on: the whole window is checked tick by
                // tick. But if doing nothing kills you inside the window, fire
                // 2 ticks before that at the latest - a held click must never
                // just sit there while you die.
                latest = -1.0;
                m_hasDeadline = budgetUs > 0;
                if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budgetUs);
                bool const rec = m_recordPath;
                m_recordPath = false;
                SimResult const idle = sim(maxDelay); // press only at the window end = idle until then
                m_recordPath = rec;
                m_hasDeadline = false;
                if (idle.end == SimEnd::Died && idle.deathStep >= 0 && idle.deathStep < maxDelay) {
                    int const lastChance = std::max(1, idle.deathStep - scaleTicks(2, s));
                    latest = at(lastChance) + TIME_EPS;
                }
            }
            m_liveRescue = rescue; // first safe tick, no margin

            m_liveState.startClick(m_gameTime, dt, at(bestDelay), at(std::min(maxDelay + 1, maxCap)) + TIME_EPS, latest);
            m_liveArrive = m_gameTime;
            m_livePlanTicks = bestDelay;
            m_liveWaited = false;
            m_liveWaitWhy = 0;
            for (int& v : m_refuse) v = 0;
            m_refuseHave = false;
            m_last.liveTicks = -1;
            m_last.liveReason = acecore::FireReason::None;
            if (mode.holdBased()) m_streamShift = static_cast<double>(bestDelay) * dt; // updated when it really fires
            return false;
        }

        if (mode.holdBased()) m_streamShift = 0.0; // went in raw: the stream has no shift now
        m_gameHolding = push;
        return true;
    }

    // ------------------------------------------------------------------------
    //  Live re-planning step (before a physics step, while an input is held)
    // ------------------------------------------------------------------------
    void liveStep(PlayLayer* pl) {
        double const now = m_gameTime;
        float const dt = getValidDt();
        Config const& c = cfg();
        bool const wasFirst = m_liveState.first && !m_liveState.needInit;

        std::vector<acecore::Point2> path;
        std::string modeName;
        float simScale = 1.f;
        bool waveMode = false;

        // "Is firing it now safe?" - one test from the real current state.
        //  * The click you made (head = first) gets everything: learned spots,
        //    near-miss waits, waiting for a spot that worked before.
        //  * Inputs queued behind it (usually your release) only get the plain
        //    safety check, so your hold lengths / rhythm stay yours.
        auto check = [&](bool push) -> acecore::LiveVerdict {
            // Following the plan: on the predicted path, just wait for the plan
            // tick (it fires there through m_liveState.latest) - no prediction.
            // Presses and releases (2.1 - a held wave release was re-predicted
            // every tick: the per-tick cost while you spam).
            if (m_planTrust && m_liveState.first) {
                if (!planDrifted(pl, now, dt)) {
                    m_planTicksSaved++;
                    return acecore::LiveVerdict::Unsafe; // "not yet"; the plan tick fires it
                }
                m_planTrust = false; // something the prediction didn't know: check for real
                m_planDrifts++;
                if (c.hudVerbose) pushLog("held click: you left the predicted path - checking every tick", COL_GREY, false);
            }
            Ctx x;
            if (!makeCtx(pl, push, c, x)) return acecore::LiveVerdict::OnSchedule;
            modeName = x.mode.name();
            simScale = x.s;
            waveMode = x.mode.wave;
            if (!push && !x.mode.releaseMatters()) return acecore::LiveVerdict::OnSchedule;
            bool const head = m_liveState.first;
            // Orb helper (2.1): fire the moment you touch the orb (or on the
            // predicted tick through m_liveState.latest).
            if (head && push && m_orbHelpHold) {
                if (m_orbHelpRing && overlapsNow(pl->m_player1, m_orbHelpRing)) return acecore::LiveVerdict::Proven;
                return acecore::LiveVerdict::Unsafe;
            }
            // Held for a learned spot: fire exactly when you reach it (no
            // prediction - it may be blind there), else keep waiting.
            if (head && push && m_liveSpotHold) {
                // Reached (or just passed) the spot's position: fire now. By
                // position, not an exact-tick match, so a check that lands a
                // fraction of a tick off (CBF sub-steps) can't skip it.
                float const px = pl->m_player1->getPositionX();
                float const tolX = spotTol().x;
                if ((px - m_spotTargetX) * m_spotTargetDir >= -tolX) {
                    m_spotFires++;
                    return acecore::LiveVerdict::Proven;
                }
                return acecore::LiveVerdict::Unsafe;
            }
            PlayerObject* const p1 = pl->m_player1;
            // Cube-type press in the air does nothing until you land: never
            // "proven" there (a learned air spot must not eat the click).
            bool const airPress = push && x.mode.cubeLike && !(x.mode.ship || x.mode.wave || x.mode.ufo ||
                                                              x.mode.swing || x.mode.robot) && !p1->m_isOnGround;

            // Learned spots first: firing on a spot that worked before repeats it;
            // a spot that killed you before is skipped whatever the sim says.
            acecore::Spot spotHere;
            acecore::SpotTol spotT;
            // Session memory: presses only, every gamemode (firing on a remembered
            // spot doesn't move the input, so it keeps the line in ship / wave too).
            bool const useSpots = head && push && c.learnSpots && m_spots.size() > 0;
            if (useSpots) {
                spotHere = spotNow(pl, push, modeName);
                spotT = spotTol();
                acecore::SpotHit const h = m_spots.at(spotHere.x, spotHere.y, spotHere.vy, spotHere.key, spotT);
                if (h == acecore::SpotHit::Good && !airPress) { m_spotFires++; return acecore::LiveVerdict::Proven; }
                if (h == acecore::SpotHit::Bad)  { m_spotBlocks++; m_refuse[RF_SPOT]++; return acecore::LiveVerdict::Unsafe; }
            }

            // The next input you already made (queued, exact) or, while you still
            // hold, at least how long you've held so far. Ticks after this one fires.
            int knownGap = -1;
            {
                double const opp = m_queue.nextOppositeAt();
                if (opp >= 0.0) knownGap = std::max(1, static_cast<int>(std::lround((opp - m_queue.nextAt()) / dt)));
            }
            if (x.tapModel) {
                // Cube: holding through a landing re-jumps in 2.2, so use your real
                // hold, not the typical tap.
                int hold = knownGap;
                if (hold < 0 && m_physicalHolding) hold = static_cast<int>(std::lround((now - m_liveArrive) / dt));
                if (hold > 0) x.tapTicks = std::max(x.tapTicks, hold);
            }
            // Ship / wave / robot: your release (or next press) is part of the path.
            bool const useGap = x.modern && knownGap > 0 && !x.tapModel && x.mode.releaseMatters();
            auto gapAt = [&](int d) {
                FollowUp f;
                if (useGap) {
                    f.at[0] = d + knownGap;
                    f.held[0] = !push;
                }
                return f;
            };
            auto simAt = [&](int d) { return runSim(x, d, gapAt(d)); };
            auto goodPath = [&](SimResult const& rk) {
                return rk.end == SimEnd::Survived ||
                       (x.landRule && rk.end == SimEnd::Died && rk.landTick >= 0 &&
                        rk.ticks > rk.landTick + scaleTicks(LAND_GRACE_BASE, x.s));
            };

            // A re-check runs every tick while a click is held: smaller share of
            // the frame than a click gets.
            int const budget = liveBudgetUs(c);
            m_hasDeadline = budget > 0;
            if (m_hasDeadline) m_deadline = Clock::now() + std::chrono::microseconds(budget);
            m_recordPath = c.accuracy;
            SimResult r = simAt(0);
            if (m_recordPath) path = m_path;
            bool budgetHit = false;
            acecore::Verdict const v = verdictFor(x, 0, r, -1000000, budgetHit, 2, gapAt(0));
            auto probe = [&](int d) {
                bool const rec = m_recordPath;
                m_recordPath = false; // keep the path of the "now" test
                SimResult rk = simAt(d);
                m_recordPath = rec;
                return rk;
            };

            // Safe now, but a spot that worked before is coming within the window:
            // wait for it (repeat what you know works).
            if (head && v == acecore::Verdict::Good && useSpots && !airPress && !x.mode.holdBased() &&
                m_p1Speed > 0.01f && m_liveState.deadline >= 0.0) {
                float const reach = m_p1Speed * static_cast<float>(m_liveState.deadline - now - 2.0 * dt);
                float const ahead = reach > 0.f
                    ? m_spots.goodAhead(spotHere.x, spotHere.y, spotHere.key, m_p1Dir, reach, spotT) : -1.f;
                if (ahead > 0.f) {
                    // Only if waiting for it is safe too (it may belong to a later
                    // jump: waiting must not skip the obstacle in front of you).
                    int const k = std::max(1, static_cast<int>(std::lround(ahead / (m_p1Speed * dt))));
                    if (goodPath(probe(k))) {
                        m_hasDeadline = false;
                        m_liveWaitWhy = 2;
                        m_refuse[RF_AHEAD]++;
                        return acecore::LiveVerdict::Unsafe; // not yet
                    }
                }
            }
            // Safe, but only just (a near miss the real game may not repeat)?
            // If a clearly cleaner moment comes within a few steps, wait for it.
            // Presses only: release timing is your rhythm (wave / ship corridors are
            // always "close", so waiting there just shifts your release).
            if (x.modern && head && push && v == acecore::Verdict::Good && !budgetHit) {
                float const target = clearTarget(modeName);
                int const look = scaleTicks(CLEAN_LOOK_BASE, x.s);
                double const deadline = m_liveState.deadline;
                int const k = acecore::cleanerAhead(r.clear, target, CLEAN_GAIN_PX, look, [&](int d) {
                    acecore::CleanProbe cp;
                    // leave room for the safe-in-a-row rule before the window ends
                    if (deadline >= 0.0 && now + static_cast<double>(d + 2) * dt >= deadline) { cp.stop = true; return cp; }
                    SimResult const rk = probe(d);
                    if (rk.end == SimEnd::OutOfBudget) { cp.stop = true; return cp; }
                    cp.good = goodPath(rk);
                    cp.clear = rk.clear;
                    return cp;
                });
                if (k > 0) {
                    m_hasDeadline = false;
                    m_cleanWaits++;
                    m_liveWaited = true;
                    m_liveWaitWhy = 1;
                    m_refuse[RF_CLEAN]++;
                    return acecore::LiveVerdict::Unsafe; // not yet: a cleaner moment is coming
                }
            }
            m_hasDeadline = false;
            if (head && v == acecore::Verdict::Bad) {
                if (r.end == SimEnd::Truncated) m_refuse[RF_CUT]++;
                else m_refuse[RF_DIED]++;
                if (!m_refuseHave && r.end == SimEnd::Died) { m_refuseFirst = r; m_refuseHave = true; }
            }
            switch (v) {
                case acecore::Verdict::Good:    return acecore::LiveVerdict::Safe;
                case acecore::Verdict::Partial: return acecore::LiveVerdict::SafeCut;
                case acecore::Verdict::Budget:  return acecore::LiveVerdict::Unknown;
                default:                       return acecore::LiveVerdict::Unsafe;
            }
        };
        // The click itself keeps a one-tick margin off the deadly edge (not in wave).
        // (Rescue: the first safe tick, no margin - the safe moment can be 1 tick wide.)
        // Aceware: a release needs ONE safe tick. Asking for two in a row made
        // narrow release windows impossible to satisfy, so the button stayed
        // down until the window ran out - and you died holding it.
        auto need = [&](bool first) {
            if (!m_queue.nextPush()) return 1; // releases: one proven tick is enough
            return (first && !waveMode && !m_liveRescue && c.safetyTicks > 0) ? 2 : 1;
        };
        // A queued input (your release, or a press made while one was held)
        // fires exactly on its scheduled time: same spacing from the held
        // input as you made it, no re-check. Re-checking held wave releases up
        // to the whole window and made them late.
        // (2.2) Releases (and presses) queued behind a held input get the same
        // treatment as a click: from their own time, the first safe tick
        // inside their window. They used to fire on schedule unchecked, so a
        // release made a little early while your press was held just died.
        auto windowTime = [&](bool push) -> double {
            PlayerObject* p1 = pl->m_player1;
            if (!p1) return 0.0;
            ModeInfo mi;
            mi.add(p1);
            if (!push && !mi.releaseMatters()) return 0.0; // a cube release does nothing
            // Ship (2.2): your tap stays exactly as long as you made it. In a
            // straight fly (quick taps to stay level) a release that waited for
            // a "safe" tick made that tap a longer hold, and the ship rose by
            // itself. A queued ship input fires on its schedule: the held
            // input's shift, your hold / gap unchanged.
            if (mi.ship && !mi.wave) return 0.0;
            // Robot (2.2): the same - its release sets the jump's height, so a
            // queued one keeps exactly the hold you made.
            if (mi.robot && !push) return 0.0;
            int wt = mi.windowTicks(effectiveCfg(c), push, dt);
            if (c.custom) wt = std::min(wt, std::max(1, scaleTicks(c.maxTicks, BASE_DT / dt)));
            return static_cast<double>(std::max(0, wt)) * dt;
        };

        auto fired = acecore::liveStep(m_queue, m_liveState, now, TIME_EPS, check, need, windowTime);
        if (fired) {
            fireButton(pl, *fired);
            // (2.2) which hold it was, for the log: an orb wait used to say
            // "spot that worked before" (the learned-spot text)
            char const* holdWhy = !wasFirst ? nullptr : m_orbHelpHold ? "touched the orb" : nullptr;
            if (wasFirst) { m_orbHelpHold = false; m_orbHelpRing = nullptr; }
            if (wasFirst) {
                int const firedTicks = static_cast<int>(std::lround((now - m_liveArrive) / dt));
                m_last.liveTicks = firedTicks;
                if (m_streamShift > TIME_EPS || modeName.find("wave") != std::string::npos ||
                    modeName.find("ship") != std::string::npos) {
                    m_streamShift = std::max(0.0, now - m_liveArrive); // the shift it really got
                }
                m_last.liveReason = m_liveState.reason;
                // A queued ship / robot release has no window at all (windowTime()
                // returns 0 for it): it goes in on its own scheduled tick by design.
                // That is not "the window ended with nothing safe found".
                ModeInfo relMode;
                relMode.add(pl->m_player1);
                bool const onScheduleRelease = !*fired && ((relMode.ship && !relMode.wave) || relMode.robot);
                bool const late = m_liveState.reason == acecore::FireReason::Deadline && !onScheduleRelease;
                // Followed the plan to its tick: the normal case now, not a warning.
                bool const followed = m_planTrust && m_liveState.reason == acecore::FireReason::Plan;
                if (followed) m_planFires++;
                m_planTrust = false;
                // Always shown: when the held click really went in, and why.
                bool const onPlan = !followed && m_liveState.reason == acecore::FireReason::Plan;
                std::string const why = (holdWhy && m_liveState.reason == acecore::FireReason::Proven) ? holdWhy
                                        : onScheduleRelease ? "on schedule"
                                                            : fireReasonText(m_liveState.reason);
                pushLog(fmt::format("{} fired +{}t ({}){}{}", *fired ? "PRESS" : "REL", firedTicks, why,
                                    m_liveWaited ? ", waited for a cleaner path" : "",
                                    ((late || onPlan) && c.hudVerbose) ? " | live: " + refuseSummary() : std::string()),
                        (late || onPlan) ? COL_ORANGE : COL_GREEN, false);
                // Accuracy: that path is exactly what should happen now.
                if (c.accuracy && !path.empty()) {
                    m_tracker.start(now, dt, std::move(path), scaleTicks(ACCURACY_CHECK_BASE, simScale) - 1, ACCURACY_TOL_PX);
                    m_trackMode = modeName;
                }
            }
        }
        if (!m_queue.pending()) m_live = false;
    }

    // "said no 14x: died 12 (dies 9t after firing, landed -), memory 2"
    std::string refuseSummary() const {
        static char const* names[RF_COUNT] = {"died", "orb/portal", "memory", "near miss", "learned spot ahead"};
        int total = 0;
        std::string parts;
        for (int i = 0; i < RF_COUNT; i++) {
            if (!m_refuse[i]) continue;
            total += m_refuse[i];
            if (!parts.empty()) parts += ", ";
            parts += fmt::format("{} {}", names[i], m_refuse[i]);
            if (i == RF_DIED && m_refuseHave) {
                SimResult const& f = m_refuseFirst;
                std::string what = f.killType >= 0 ? objectName(static_cast<GameObjectType>(f.killType)) : "a block edge / squish (no object)";
                parts += fmt::format(" (dies {}t after firing, {}, hit {} at {:+.0f}px height)", f.ticks,
                                     f.landTick >= 0 ? fmt::format("landed at {}t", f.landTick) : std::string("never landed"),
                                     what, f.killDy);
            }
        }
        if (total == 0) return "no refusals";
        return fmt::format("said no {}x: {}", total, parts);
    }

    static char const* fireReasonText(acecore::FireReason r) {
        switch (r) {
            case acecore::FireReason::Safe:     return "safe";
            case acecore::FireReason::SafeCut:  return "safe up to an orb / portal";
            case acecore::FireReason::Schedule: return "on plan, couldn't check";
            case acecore::FireReason::Proven:   return "spot that worked before";
            case acecore::FireReason::Deadline: return "window ended, nothing safe found";
            case acecore::FireReason::Plan:     return "on plan";
            default:                           return "?";
        }
    }

    // ------------------------------------------------------------------------
    //  Runs before every P1 physics step (full tick or CBF sub-step)
    // ------------------------------------------------------------------------
    // ------------------------------------------------------------------------
    //  Step audit (2.3, Dev tab): learns from the REAL game what the hidden
    //  player gets wrong, field by field, while you play (autoplay or not).
    //   1. Between steps: what GD changes on you after a step's last part
    //      (updateRotation) and before the next step - things the hidden
    //      player's own step never does, so the prediction must do them.
    //   2. One tick from a saved state: every 4 ticks a hidden player starts
    //      from your exact state (the route's / scan's way) and runs one tick
    //      with your button; next tick every field that came out different.
    // ------------------------------------------------------------------------
    struct AuditField { long long n = 0; std::string example; };
    std::map<std::string, AuditField> m_audOutside, m_audSim, m_audRestart;
    long long m_audOutsideN = 0, m_audSimN = 0, m_audSimExact = 0, m_audRestartN = 0, m_audRestartExact = 0;
    PlayerSnap m_audEnd, m_audSimResult;
    bool m_audEndValid = false, m_audPending = false, m_audHeld = false;
    double m_audT0 = 0.0;
    static void auditAdd(std::map<std::string, AuditField>& into, DiffList const& d) {
        for (auto const& [name, v] : d) {
            AuditField& f = into[name];
            if (f.n++ == 0) f.example = v;
        }
    }
    void noteStepEnd(PlayerObject* p) {
        if (m_simulating || !p || p->m_isDead) return;
        m_audEnd = snapFrom(p);
        m_audEndValid = true;
    }
    void stepAudit(PlayLayer* pl, PlayerObject* p) {
        if (m_simulating || p->m_isDead || p->m_isPlatformer || pl->m_isPaused) { m_audEndValid = m_audPending = false; return; }
        PlayerSnap const now = snapFrom(p);
        if (m_audEndValid) {
            m_audOutsideN++;
            auditAdd(m_audOutside, snapDiffList(m_audEnd, now));
            m_audEndValid = false;
        }
        float const nominal = getValidDt();
        if (m_audPending) {
            m_audPending = false;
            // a whole step, the button as it was, no input in between
            if (m_gameHolding == m_audHeld && std::fabs(m_lastStepD - nominal) < 0.02f * nominal &&
                m_gameTime - m_audT0 < 1.5 * nominal) {
                DiffList const d = snapDiffList(now, m_audSimResult);
                m_audSimN++;
                if (d.empty()) m_audSimExact++;
                auditAdd(m_audSim, d);
            }
        }
        if ((m_tick & 3) != 0 || m_queue.pending() || tickPhase() > 0.f || !m_simPlayer1.data()) return;
        SpTick from;
        from.s1 = now;
        from.x = now.pos.x;
        from.y = now.pos.y;
        from.vy = p->m_yVelocity;
        from.modeCode = modeCode(p);
        std::vector<SpTick> trail;
        // 3. (every 64 ticks) 12 ticks in one run vs 12 runs of one tick, each
        //    from the last one's saved state: whatever comes out different is
        //    state the saved state doesn't carry (the route plans in long
        //    runs and replays from saved states)
        if ((m_tick & 63) == 0) {
            constexpr int N = 12;
            snapRun(pl, from, m_gameHolding, m_gameHolding, 0, N, trail, true, -(1 << 30), simDt());
            if (static_cast<int>(trail.size()) >= N) {
                PlayerSnap const one = trail[N - 1].s1;
                SpTick st = from;
                bool ok = true;
                for (int i = 0; i < N && ok; i++) {
                    snapRun(pl, st, m_gameHolding, m_gameHolding, 0, 1, trail, true, -(1 << 30), simDt());
                    if (trail.empty()) { ok = false; break; }
                    st = trail.front();
                }
                if (ok) {
                    DiffList const d = snapDiffList(one, st.s1);
                    m_audRestartN++;
                    if (d.empty()) m_audRestartExact++;
                    auditAdd(m_audRestart, d);
                }
            }
        }
        snapRun(pl, from, m_gameHolding, m_gameHolding, 0, 1, trail, true, -(1 << 30), simDt());
        if (trail.empty()) return;
        m_audSimResult = trail.front().s1;
        m_audHeld = m_gameHolding;
        m_audT0 = m_gameTime;
        m_audPending = true;
    }
    static std::string auditTop(std::map<std::string, AuditField> const& m, int maxN) {
        std::vector<std::pair<std::string, AuditField const*>> v;
        for (auto const& [k, f] : m) v.emplace_back(k, &f);
        std::sort(v.begin(), v.end(), [](auto const& a, auto const& b) { return a.second->n > b.second->n; });
        std::string out;
        int i = 0;
        for (auto const& [k, f] : v) {
            if (i++ >= maxN) break;
            out += fmt::format("{}{} x{} ({})", out.empty() ? "" : "; ", k, f->n, f->example);
        }
        return out.empty() ? std::string("nothing") : out;
    }

    void onPlayerUpdate(PlayLayer* pl, PlayerObject* player) {
        if (player != pl->m_player1) return;
        stepAudit(pl, player); // (2.3) before anything else touches you this tick
        m_tick++;
        m_framesSinceStep = 0;
        lagPoll(player); // (2.3) the last sent input: on time?
        // Report trace (2.1): where the REAL player switches gamemode and what
        // GD keeps for it (portal position, level portal height) - to check the
        // scan's portal handling against the real game.
        {
            int const mc = modeCode(player);
            // (2.2) band check: how far down / up you really went in this
            // gamemode, against the band the scan uses (for the report)
            {
                CCRect const& r = player->getObjectRect();
                m_realBandMin = std::min(m_realBandMin, r.getMinY());
                m_realBandMax = std::max(m_realBandMax, r.getMaxY());
            }
            // (2.2) ground check: what you really stand on, and whether the
            // hidden player could see it (for the report) - a few lines a level
            if (player->m_isOnGround && m_groundTraces < 60) {
                GameObject* g = player->m_lastGroundObject;
                if (g != m_groundSeen) {
                    m_groundSeen = g;
                    m_groundTraces++;
                    CCRect const& pr = player->getObjectRect();
                    if (!g) {
                        float lo = 0.f, hi = 0.f;
                        bool const band = bandOf(player, lo, hi, pl->m_gameState.m_portalY);
                        groundLog(fmt::format("ground check: standing at y {:.1f} (feet {:.1f}) on no object - GD ground / portal floor{}",
                                          player->getPositionY(), pr.getMinY(),
                                          band ? fmt::format(" (scan band {:.0f} .. {:.0f})", lo, hi) : std::string(" (no band)")));
                    } else {
                        bool inIndex = false;
                        for (auto const& ro : m_rayObjs) if (ro.obj == g) { inIndex = true; break; }
                        CCRect const& r = g->getObjectRect();
                        groundLog(fmt::format("ground check: standing on {} id {} at ({:.0f},{:.0f}) {:.0f}x{:.0f} type {} "
                                          "noTouch {} passable {} groupOff {} off {} visible {} - in the mod's list: {}",
                                          objectName(g->m_objectType), g->m_objectID, r.getMidX(), r.getMidY(), r.size.width,
                                          r.size.height, static_cast<int>(g->m_objectType), g->m_isNoTouch, g->m_isPassable,
                                          g->m_isGroupDisabled, g->m_isDisabled, g->isVisible(), inIndex ? "yes" : "NO"));
                    }
                }
            }
            if (mc != m_lastRealMode) {
                if (m_lastRealMode >= 0 && bandHeight(m_lastRealMode) > 0.f) {
                    float const h = bandHeight(m_lastRealMode);
                    float const lo = std::max(GROUND_Y, m_realBandY - h * 0.5f);
                    groundLog(fmt::format("band check: mode {} you went {:.1f} .. {:.1f}, scan band {:.1f} .. {:.1f} "
                                      "(portal y {:.1f}, portalY {:.1f}, mode height {:.1f})",
                                      m_lastRealMode, m_realBandMin, m_realBandMax, lo, lo + h, m_realBandY,
                                      pl->m_gameState.m_portalY, pl->m_gameState.m_lastModeHeight));
                }
                m_realBandMin = 1e9f;
                m_realBandMax = -1e9f;
                m_realBandY = player->m_lastPortalPos.y;
                if (m_lastRealMode >= 0) {
                    trace(fmt::format("real mode {} -> {} at ({:.1f},{:.1f}) lastPortalPos=({:.1f},{:.1f}) portalY={:.1f}",
                                      m_lastRealMode, mc, player->getPositionX(), player->getPositionY(),
                                      player->m_lastPortalPos.x, player->m_lastPortalPos.y, pl->m_gameState.m_portalY));
                }
                m_lastRealMode = mc;
            }
        }
        if (m_attemptPending && !player->m_isDead) {
            // First step of an attempt (2.1): count it, and the click guide
            // starts recording from here.
            m_attemptPending = false;
            m_lvlStats.attempts++;
            m_lifeStats.attempts++;
            trace(fmt::format("attempt start at x={:.1f}{}", player->getPositionX(), pl->m_isPracticeMode ? " (practice)" : ""));
            // Wave rays (2.2): index the level's hitboxes now (once per level),
            // not on your first wave click - a big level takes a moment.
            if (m_rayLayer != pl) buildRayIndex(pl);
        }
        m_lastStepWall = Clock::now();

        // Dead (and not reported through the hook): report once. Alive again
        // after a death without a level reset (some respawn paths): re-arm.
        if (player->m_isDead) {
            if (!m_deathReported) onRealDeath();
            return;
        }
        if (m_deathReported) m_deathReported = false;
        moverTick(player); // (2.3) moving blocks / spikes: before anything predicts this tick
        if (m_autoOn && (player->getPositionX() - m_apBestX) * (m_p1Dir < 0.f ? -1.f : 1.f) > 0.f) m_apBestX = player->getPositionX();
        transTick(player); // (2.3) portals / orbs / speed / gravity, for the death record
        lmarginTick();     // (2.3) a moved click lived half a second: ease its learned margin
        assistTick(pl, player); // (2.3) Hitboxes
        m_apAttemptTicks++;
        if (m_autoOn) learnTick(pl, player);
        routeCheck(pl, player);
        recordAnchor(pl, player);

        // Housekeeping a few times per frame, not every step.
        if ((m_tick & 7) == 0) {
            refreshFlags();
            refreshHud(pl, false);
        }
        if (cfg().rayView) {
            // Show Rays (2.2): the pathfinder gets ~1 ms every 4 steps, the
            // drawing is redone every 8 (~30x a second)
            // (paused while one of your inputs is waiting to go in: nothing may
            // touch the game between your click and its tick)
            // (2.3: not while autoplay plays - its route is drawn instead)
            if ((m_tick & 3) == 0 && !m_queue.pending() && !m_live && !m_autoOn) planSlice(pl, 1000);
            if ((m_tick & 7) == 0) updateRays(pl);
        } else if ((m_tick & 15) == 0) {
            clearRays();
            if (m_planActive) { m_planActive = false; m_planA = AStarRun{}; }
        }

        // (2.3) Autoplay: the mod plays (the buffer stays out of it)
        if (m_autoOn) {
            ensureSimPlayers(pl);
            if (cfg().driftCheck && !deathsOff()) driftCheck(pl, player); // (deaths off: its "you'd die" can't come true)
            autoStep(pl, player);
            return;
        }

        if (!m_enabled) {
            clearBuffer();
            reconcile(pl);
            return;
        }

        ensureSimPlayers(pl);
        // Diagnostics only (1.26.0): a prediction every 8 ticks. It reads 100%
        // in every mode now; only with "Log: show safe clicks" on.
        if (cfg().hudVerbose) oneStepCheck(pl, player);
        if (cfg().driftCheck) driftCheck(pl, player); // (2.3) 100 ms prediction check
        spotStep(player);
        // Accuracy: has a prediction reached its checkpoint?
        if (m_tracker.active() && pl->m_player1) {
            if (auto r = m_tracker.poll(m_gameTime, pl->m_player1->getPositionX(), pl->m_player1->getPositionY())) {
                AccCount& a = m_accByMode[m_trackMode];
                a.checks++;
                a.errSum += std::min(r->err, 20.f); // one wild miss shouldn't decide the target alone
                a.dxSum += std::clamp(r->dx, -20.f, 20.f);
                a.dySum += std::clamp(r->dy, -20.f, 20.f);
                if (r->hit) a.hits++;
                else notePredictionMiss(r->err, r->dx, r->dy);
            }
        }

        if (m_queue.pending() && m_live) {
            // Once per physics tick: CBF adds short extra steps, and re-checking on
            // each of them doubled the work in a frame (chop). A held click whose
            // plan / window end comes due still goes in on time (checked first).
            double const sinceCheck = m_gameTime - m_lastLiveAt;
            bool const due = (m_liveState.latest >= 0.0 && m_gameTime + TIME_EPS >= m_liveState.latest) ||
                             (m_liveState.deadline >= 0.0 && m_gameTime + TIME_EPS >= m_liveState.deadline) ||
                             m_liveState.needInit;
            if (due || sinceCheck >= 0.6 * static_cast<double>(getValidDt())) {
                m_lastLiveAt = m_gameTime;
                auto const t0 = Clock::now();
                liveStep(pl);
                long long const us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
                m_liveUsSum += us;
                m_liveChecks++;
                m_liveUsMax = std::max(m_liveUsMax, us);
            }
        } else if (m_queue.pending()) {
            // At most one queued input per physics step, right before the step runs.
            if (auto due = m_queue.poll(m_gameTime, TIME_EPS)) {
                fireButton(pl, *due);
            }
        } else {
            reconcile(pl);
        }
    }
};

TrajectoryBufferSim s_sim;

// ----------------------------------------------------------------------------
//  9. The window: draggable, live, tabs Log / Stats / Settings.
//
//    * Drag it by the title bar; its position is remembered.
//    * Shown / hidden with the window key (Left Alt by default).
//      Contents refresh on their own a few times a second.
//    * While a level is actually running it is display-only: it never takes
//      your clicks (no swallowed jumps). Interact with it while paused, or
//      outside a level. The mouse wheel scrolls it any time.
// ----------------------------------------------------------------------------
std::string windowKeyName();

// Footer hint (2.1): how to show / hide the window - the Window Key you set,
// or the pause-menu button on phones (no keyboard).
std::string toggleHint() {
#ifdef GEODE_IS_MOBILE
    return "Pause menu button to toggle";
#else
    return windowKeyName() + " to toggle";
#endif
}

// "how to bring the window back", for the hide notification and the empty Log.
std::string showHowTo() {
#ifdef GEODE_IS_MOBILE
    return "the Aceware button in the pause menu";
#else
    return windowKeyName();
#endif
}

// Window key name for on-screen hints ("Left Alt"), from the Window Key
// setting. cocos key codes are Windows virtual-key codes.
// Any key you bound (2.2): its name, with Ctrl / Shift / Alt if you added them.
std::string keyNameFor(Keybind const& b, char const* fallback);
[[maybe_unused]] std::string windowKeyName() {
    auto const binds = Mod::get()->getSettingValue<std::vector<Keybind>>("window-key-v3");
    if (binds.empty()) return "the Window Key";
    return keyNameFor(binds.front(), "the Window Key");
}
// The Stage Cycle Key you set ("F6").
[[maybe_unused]] std::string cycleKeyName() {
    auto const binds = Mod::get()->getSettingValue<std::vector<Keybind>>("cycle-key");
    if (binds.empty()) return "";
    return keyNameFor(binds.front(), "");
}
// (2.3) Geode's names can have symbols the window's font can't draw (Mac's
// key symbols came out as nothing: "  to toggle"). Plain text only, else the fallback.
static std::string plainKeyName(std::string const& t, char const* fallback) {
    std::string out;
    for (unsigned char ch : t) if (ch >= 32 && ch < 127) out += static_cast<char>(ch);
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    while (!out.empty() && out.back() == ' ') out.pop_back();
    bool any = false;
    for (char ch : out) any = any || std::isalnum(static_cast<unsigned char>(ch));
    return any ? out : std::string(fallback);
}
std::string keyNameFor(Keybind const& b, char const* fallback) {
    // with modifiers, Geode's own name says it all ("Ctrl+Shift+X")
    if (b.modifiers != KeyboardModifier::None) return plainKeyName(b.toString(), fallback);
    int const vk = static_cast<int>(b.key);
    switch (vk) {
        case 0xA4: return "Left Alt";
        case 0xA5: return "Right Alt";
        case 0x12: return "Alt";
        case 0xA0: return "Left Shift";
        case 0xA1: return "Right Shift";
        case 0x10: return "Shift";
        case 0xA2: return "Left Ctrl";
        case 0xA3: return "Right Ctrl";
        case 0x11: return "Ctrl";
        case 0x09: return "Tab";
        case 0x2D: return "Insert";
        case 0x2E: return "Delete";
        case 0x24: return "Home";
        case 0x23: return "End";
        case 0x21: return "Page Up";
        case 0x22: return "Page Down";
        case 0xC0: return "`";
        default: break;
    }
    if (vk >= 0x70 && vk <= 0x87) return fmt::format("F{}", vk - 0x70 + 1);
    if ((vk >= 0x30 && vk <= 0x39) || (vk >= 0x41 && vk <= 0x5A)) return std::string(1, static_cast<char>(vk));
    // anything else: Geode's name for it
    return plainKeyName(b.toString(), fallback);
}

// Window size per platform (Window Size 1.0). (2.3) Phones were 1.5x - too big
// with the taller window (half the screen); each has its own now.
#if defined(GEODE_IS_IOS)
constexpr float PLATFORM_WINDOW_SCALE = 1.1f;
#elif defined(GEODE_IS_ANDROID)
constexpr float PLATFORM_WINDOW_SCALE = 1.1f;
#else
constexpr float PLATFORM_WINDOW_SCALE = 1.0f;
#endif
// (2.3) Never taller than this share of the screen (any Window Size), so it
// always fits on a small / wide phone.
// Never wider than this share either: a window wider than the screen can only
// be dragged up and down (iPad).
#ifdef GEODE_IS_MOBILE
constexpr float WINDOW_MAX_SCREEN_H = 0.5f;
constexpr float WINDOW_MAX_SCREEN_W = 0.7f;
#else
constexpr float WINDOW_MAX_SCREEN_H = 0.9f;
constexpr float WINDOW_MAX_SCREEN_W = 0.9f;
#endif
// The badge is the manifest version, short form (v1.1 for 1.1.0), so bumping
// mod.json is all a release needs.
inline std::string titleBadge() {
    std::string v = Mod::get()->getVersion().toVString();
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v.erase(v.begin()); // "v1.1.0" -> "1.1.0"
    size_t const p = v.rfind('.');
    if (p != std::string::npos) v = v.substr(0, p);
    return "v" + v;
}

// ----------------------------------------------------------------------------
//  Window text (2.2): one smooth bold font for the whole window - Poppins
//  Bold, bundled with the mod (resources/fonts, SIL Open Font License, see
//  OFL.txt there). The window's code keeps GD-font sizes: a label made "as
//  bigFont" or "as chatFont" is scaled to come out the size that GD font
//  would have been (measured once). If the bundled font isn't there, it falls
//  back to GD's own font.
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//  Online users (2.2): how many people have Aceware and how many are
//  playing right now, shown in the window header (green dot = online, grey =
//  offline). Each game sends the Aceware server an anonymous random ID
//  (made on first start, nothing else about you) every 2 minutes; the server
//  answers with the totals. Off with the "Show Online Users" setting.
//  ONLINE_URL (src/OnlineURL.hpp) is your Cloudflare Worker (see server/README.md); empty = off.
// ----------------------------------------------------------------------------
// The URL lives in its own file (src/OnlineURL.hpp) so a new main.cpp never wipes it.
#include "OnlineURL.hpp"
constexpr float ONLINE_PING_SECS = 90.f;  // (2.3) "I'm here", every 1.5 minutes (was 2; the server's "online" is 3)
constexpr float ONLINE_STATS_SECS = 15.f; // (2.3) the totals, every 15 s - only while you're looking at the window
// (Cloudflare's free plan: 100 000 requests a day. A game left open pings
// ~960 times a day; the extra totals only while the window is up and you're
// not playing.)
struct OnlineStats { long long total = -1, online = -1; };
OnlineStats s_online;

std::string onlineId() {
    auto* mod = Mod::get();
    std::string id = mod->getSavedValue<std::string>("anon-id", "");
    bool ok = id.size() == 32;
    for (char ch : id) ok = ok && std::isxdigit(static_cast<unsigned char>(ch));
    if (!ok) {
        std::random_device rd;
        std::mt19937_64 rng((static_cast<uint64_t>(rd()) << 32) ^ rd());
        id.clear();
        for (int i = 0; i < 32; i++) id += "0123456789abcdef"[rng() & 15];
        mod->setSavedValue<std::string>("anon-id", id);
    }
    return id;
}

bool onlineEnabled() {
    return ONLINE_URL[0] != '\0' && Mod::get()->getSettingValue<bool>("online-count");
}

void sendOnlinePing() {
    if (!onlineEnabled()) { s_online = OnlineStats{}; return; }
    web::WebRequest req;
    req.bodyJSON(matjson::makeObject({{"id", onlineId()},
                                      {"version", Mod::get()->getVersion().toVString()},
                                      {"platform", std::string(GEODE_PLATFORM_SHORT_IDENTIFIER)}}));
    req.timeout(std::chrono::seconds(10));
    async::spawn(req.post(std::string(ONLINE_URL) + "/ping"), [](web::WebResponse res) {
        if (!res.ok()) return;
        auto j = res.json();
        if (!j) return;
        auto const total = (*j)["total"].asInt();
        auto const online = (*j)["online"].asInt();
        if (total && online) {
            s_online.total = static_cast<long long>(*total);
            s_online.online = static_cast<long long>(*online);
        }
    });
}

// (2.3) Just the totals (no "I'm here"): cheap, so the numbers follow other
// players coming and going between your own pings. Every version of the mod
// (Aceware,) talks to the same server, so they all count.
void fetchOnlineStats() {
    if (!onlineEnabled()) { s_online = OnlineStats{}; return; }
    web::WebRequest req;
    req.timeout(std::chrono::seconds(10));
    async::spawn(req.get(std::string(ONLINE_URL) + "/stats"), [](web::WebResponse res) {
        if (!res.ok()) return;
        auto j = res.json();
        if (!j) return;
        auto const total = (*j)["total"].asInt();
        auto const online = (*j)["online"].asInt();
        if (total && online) {
            s_online.total = static_cast<long long>(*total);
            s_online.online = static_cast<long long>(*online);
        }
    });
}
// (2.3) "I'm gone": when the game closes (or a phone puts it in the
// background) the server is told right away, so you show as offline at once
// instead of 3 minutes later. Waits up to 2 s (the game is closing - a
// request sent the normal way would never get out). If the game carries on
// after all, the next ping (2 s later) puts you back online.
bool s_onlineBye = false;
void sendOnlineBye() {
    if (!onlineEnabled() || s_onlineBye) return;
    s_onlineBye = true;
    web::WebRequest req;
    req.bodyJSON(matjson::makeObject({{"id", onlineId()}}));
    req.timeout(std::chrono::seconds(2));
    (void)req.postSync(std::string(ONLINE_URL) + "/bye");
}
bool s_onlineNow = false;    // set: fetch the totals right away (the window just opened)

// ----------------------------------------------------------------------------
//  Updates (2.3): on the main menu, once a session, the newest release of the
//  mod's GitHub repo is looked up (GitHub's public API, no account). Newer
//  than this one -> a popup (and an "Update" tile on the Stage tab): Update
//  downloads that release's .geode over this one and asks you to restart.
//  Nothing is downloaded unless you press it. Off: setting "Check For Updates".
// ----------------------------------------------------------------------------
//  Updates (1.4.0): on the main menu, once a session, the newest release of this
//  mod's GitHub repo is looked up (GitHub's public API, no account). If it is
//  newer, its .geode is downloaded and written over this one, and the game asks
//  you to restart. Off with the "Auto Update" setting - nothing else here talks
//  to the network.
// ----------------------------------------------------------------------------
constexpr char const* UPDATE_REPO = "fuzzyace/Aceware";
struct UpdateState {
    bool checked = false, available = false, popupPending = false, downloading = false, done = false, failed = false;
    bool restartPending = false; // written, waiting for the restart prompt
    std::string version, url;
};
UpdateState s_update;
// (2.3) The update box inside the window (defined with it): shows / redraws
// it for the update's current state. false = the window isn't up (or you're
// playing): nothing was shown.
bool updatePopup(bool open);
// After an update is written: ask for the restart that actually loads it. If there
// is no scene to put a popup on (still booting) or a level is running, the flag
// stays set and the main menu asks when it appears.
void showRestartPrompt() {
    if (!s_update.done || !s_update.restartPending) return;
    if (PlayLayer::get()) return; // never over gameplay
    if (!CCDirector::get() || !CCDirector::get()->getRunningScene()) return;
    s_update.restartPending = false;
    geode::createQuickPopup("Aceware updated",
                            fmt::format("Aceware {} is installed.\nRestart the game to use it?", s_update.version),
                            "Later", "Restart", [](FLAlertLayer*, bool restart) {
                                if (restart) geode::utils::game::restart(true);
                            });
}
void startUpdate() {
    if (s_update.url.empty() || s_update.downloading || s_update.done) return;
    s_update.downloading = true;
    s_update.failed = false;
    if (!updatePopup(false)) {
        Notification::create(fmt::format("Aceware: downloading {}...", s_update.version), NotificationIcon::Loading, 2.f)->show();
    }
    web::WebRequest req;
    req.userAgent("Aceware-mod");
    req.followRedirects(true);
    req.timeout(std::chrono::seconds(60));
    async::spawn(req.get(s_update.url), [](web::WebResponse res) {
        // Worker thread: only the download and the file write happen here; every
        // cocos / notification call is marshalled back to the main thread.
        bool const ok = res.ok() && res.data().size() >= 1024;
        bool wrote = false;
        auto const path = Mod::get()->getPackagePath();
        if (ok) {
            auto const w = geode::utils::file::writeBinarySafe(path, res.data());
            wrote = static_cast<bool>(w);
            if (!wrote) log::error("[Aceware] update: couldn't write {}", path.string());
        }
        Loader::get()->queueInMainThread([ok, wrote, path] {
            s_update.downloading = false;
            if (!ok) {
                s_update.failed = true;
                if (!updatePopup(false)) {
                    Notification::create("Aceware: the update didn't download - try again later", NotificationIcon::Error, 2.5f)->show();
                }
                return;
            }
            if (!wrote) {
                s_update.failed = true;
                if (!updatePopup(false)) Notification::create("Aceware: couldn't save the update", NotificationIcon::Error, 2.5f)->show();
                return;
            }
            s_update.done = true;
            s_update.restartPending = true;
            log::info("[Aceware] updated to {} ({})", s_update.version, path.string());
            updatePopup(true);   // refresh the window's box if it is open
            showRestartPrompt(); // Restart / Later; falls back to a notification
            if (s_update.restartPending) {
                Notification::create(fmt::format("Aceware {} is installed - restart the game to use it", s_update.version),
                                     NotificationIcon::Success, 3.f)->show();
            }
        });
    });
}
void checkForUpdate() {
    if (s_update.checked) return;
    s_update.checked = true;
    if (!Mod::get()->getSettingValue<bool>("check-updates")) return;
    web::WebRequest req;
    req.userAgent("Aceware-mod");
    req.header("Accept", "application/vnd.github+json");
    req.timeout(std::chrono::seconds(10));
    async::spawn(req.get(fmt::format("https://api.github.com/repos/{}/releases/latest", UPDATE_REPO)), [](web::WebResponse res) {
        if (!res.ok()) return;
        auto j = res.json();
        if (!j) return;
        std::string const tagStr = (*j)["tag_name"].asString().unwrapOr("");
        if (tagStr.empty()) return;
        // tags are written "v1.4.0"; VersionInfo wants the bare semver
        std::string tag = tagStr;
        if (tag.size() > 1 && (tag[0] == 'v' || tag[0] == 'V')) tag.erase(tag.begin());
        auto v = VersionInfo::parse(tag);
        if (!v || !(*v > Mod::get()->getVersion())) return;
        for (auto const& a : (*j)["assets"]) {
            std::string const name = a["name"].asString().unwrapOr("");
            std::string const url = a["browser_download_url"].asString().unwrapOr("");
            if (!url.empty() && name.size() > 6 && name.ends_with(".geode")) {
                log::info("[Aceware] update available: {} ({})", tagStr, name);
                // Main thread: touch the state, then install it straight away.
                Loader::get()->queueInMainThread([tagStr, url] {
                    s_update.available = true;
                    s_update.version = tagStr;
                    s_update.url = url;
                    startUpdate();
                });
                return;
            }
        }
    });
}
bool s_onlineWanted = false; // the window is up (and you're not playing): keep the totals fresh

// Sends the ping on start and every ONLINE_PING_SECS, and fetches the totals
// every ONLINE_STATS_SECS (lives on Geode's overlay).
class OnlineCounter : public CCNode {
    float m_wait = 3.f; // first ping a few seconds after start
    float m_statsWait = ONLINE_STATS_SECS;
public:
    static OnlineCounter* create() {
        auto* p = new OnlineCounter();
        if (p->init()) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    bool init() override {
        if (!CCNode::init()) return false;
        scheduleUpdate();
        return true;
    }
    void update(float dt) override {
        // (2.3) an update was found: the window tells you, on the main menu
        if (s_update.popupPending) {
            auto* scene = CCDirector::get()->getRunningScene();
            if (scene && scene->getChildByType<MenuLayer>(0) && updatePopup(true)) s_update.popupPending = false;
        }
        if (s_onlineBye) { // still running after a "bye" (back from the background): say hi again
            s_onlineBye = false;
            m_wait = std::min(m_wait, 2.f);
        }
        m_wait -= dt;
        m_statsWait -= dt;
        if (m_wait <= 0.f) {
            m_wait = ONLINE_PING_SECS;
            m_statsWait = ONLINE_STATS_SECS; // (the ping answers with the totals too)
            s_onlineNow = false;
            sendOnlinePing();
            return;
        }
        if ((m_statsWait <= 0.f && s_onlineWanted) || s_onlineNow) {
            m_statsWait = ONLINE_STATS_SECS;
            s_onlineNow = false;
            fetchOnlineStats();
        }
    }
};

// ----------------------------------------------------------------------------
//  Smooth shapes (2.2): GD draws shapes without anti-aliasing, so circles and
//  round corners came out jagged. These small textures are made once by the
//  mod with soft 1-px edges (white, tinted when used): a circle, a rounded
//  box, and a rounded box with a gentle top-to-bottom shade and a thin light
//  top edge (buttons). Remade after a graphics reload.
// ----------------------------------------------------------------------------
namespace aa {
constexpr int TEX = 64;          // texture size, px
constexpr float CORNER = 16.f;   // corner radius in the rounded textures, px
inline CCTexture2D* s_circle = nullptr;
inline CCTexture2D* s_round = nullptr;
inline CCTexture2D* s_button = nullptr;

inline CCTexture2D* make(bool circle, bool shaded) {
    std::vector<uint8_t> px(static_cast<size_t>(TEX * TEX * 4));
    float const half = TEX * 0.5f;
    for (int y = 0; y < TEX; y++) {
        for (int x = 0; x < TEX; x++) {
            float const fx = static_cast<float>(x) + 0.5f, fy = static_cast<float>(y) + 0.5f;
            float sd; // signed distance to the edge, px (negative inside)
            if (circle) {
                sd = std::hypot(fx - half, fy - half) - (half - 1.f);
            } else {
                float const r = CORNER;
                float const qx = std::fabs(fx - half) - (half - 1.f - r), qy = std::fabs(fy - half) - (half - 1.f - r);
                sd = std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - r;
            }
            float const a = std::clamp(0.5f - sd, 0.f, 1.f);
            float shade = 1.f;
            if (shaded) {
                float const t = fy / static_cast<float>(TEX);         // 0 top .. 1 bottom (image rows run top-down)
                shade = 0.97f - 0.17f * t;                           // lighter at the top
                if (fy < 3.5f + 0.f && sd < -0.5f) shade = 1.f;      // thin light top edge
            }
            uint8_t const v = static_cast<uint8_t>(std::lround(255.f * shade));
            size_t const i = static_cast<size_t>((y * TEX + x) * 4);
            px[i] = v; px[i + 1] = v; px[i + 2] = v;
            px[i + 3] = static_cast<uint8_t>(std::lround(255.f * a));
        }
    }
    auto* t = new CCTexture2D();
    if (!t->initWithData(px.data(), kCCTexture2DPixelFormat_RGBA8888, TEX, TEX, CCSize(TEX, TEX))) {
        t->release();
        return nullptr;
    }
    t->setAntiAliasTexParameters();
    return t; // kept (one reference) until reset()
}
inline CCTexture2D* circle() { if (!s_circle) s_circle = make(true, false); return s_circle; }
inline CCTexture2D* round() { if (!s_round) s_round = make(false, false); return s_round; }
inline CCTexture2D* button() { if (!s_button) s_button = make(false, true); return s_button; }
inline void reset() {
    for (CCTexture2D** t : {&s_circle, &s_round, &s_button}) {
        if (*t) (*t)->release();
        *t = nullptr;
    }
}
// The textures' size in points (cocos counts textures in pixels / content scale).
inline float pts(float px) { return px / std::max(0.01f, CC_CONTENT_SCALE_FACTOR()); }
// A smooth filled circle, `d` across, centred on its position.
inline CCSprite* circleSprite(float d, ccColor3B col, GLubyte op = 255) {
    CCTexture2D* t = circle();
    if (!t) return nullptr;
    auto* s = CCSprite::createWithTexture(t);
    s->setScale(d / pts(static_cast<float>(TEX)));
    s->setColor(col);
    s->setOpacity(op);
    return s;
}
// A smooth rounded box `w` x `h` with corner radius `r`, anchored bottom-left.
inline CCNode* roundBox(float w, float h, float r, ccColor3B col, GLubyte op = 255, bool shaded = false) {
    CCTexture2D* t = shaded ? button() : round();
    if (!t) return nullptr;
    r = std::clamp(r, 0.5f, std::min(w, h) * 0.5f);
    float const tp = pts(static_cast<float>(TEX)), cp = pts(CORNER);
    float const k = r / cp; // the texture's corners are cp points: scale them to r
    auto* frame = CCSpriteFrame::createWithTexture(t, CCRect(0.f, 0.f, tp, tp));
    auto* s = extension::CCScale9Sprite::createWithSpriteFrame(frame, CCRect(cp, cp, tp - 2.f * cp, tp - 2.f * cp));
    if (!s) return nullptr;
    s->setContentSize({w / k, h / k});
    s->setScale(k);
    s->setAnchorPoint({0.f, 0.f});
    s->setColor(col);
    s->setOpacity(op);
    return s;
}
} // namespace aa

class AceLabel : public CCLabelBMFont {
public:
    float m_k = 1.f; // drawn scale = asked scale * m_k

    static char const* fontFile() { return "aceFont.fnt"_spr; }
    static bool fontReady() {
        static int ok = -1;
        if (ok < 0) {
            auto* fu = CCFileUtils::sharedFileUtils();
            auto const path = fu->fullPathForFilename(fontFile(), false);
            ok = fu->isFileExist(path) ? 1 : 0;
        }
        return ok == 1;
    }
    static float factor(bool big) {
        static float kBig = -1.f, kChat = -1.f;
        if (kBig < 0.f) {
            kBig = kChat = 1.f;
            if (fontReady()) {
                auto h = [](char const* font) {
                    auto* l = CCLabelBMFont::create("AgH", font);
                    return l ? l->getContentSize().height : 0.f;
                };
                float const mine = h(fontFile()), big = h("bigFont.fnt"), chat = h("chatFont.fnt");
                if (mine > 0.f && big > 0.f) kBig = big / mine * 1.0f;
                if (mine > 0.f && chat > 0.f) kChat = chat / mine * 1.18f; // a bit bigger: easier to read
            }
        }
        return big ? kBig : kChat;
    }
    // `gdFont`: "bigFont.fnt" or "chatFont.fnt" - which GD font's sizes the caller uses
    static AceLabel* make(char const* text, char const* gdFont) {
        bool const big = std::strcmp(gdFont, "bigFont.fnt") == 0;
        bool const mine = fontReady();
        auto* l = new AceLabel();
        if (!l->initWithString(text, mine ? fontFile() : gdFont, -1.f, kCCTextAlignmentLeft, CCPoint{0.f, 0.f})) {
            return nullptr; // (cocos frees a label whose init fails)
        }
        l->autorelease();
        l->m_k = mine ? factor(big) : 1.f;
        l->setScale(1.f);
        return l;
    }
    void setScale(float s) override { CCLabelBMFont::setScale(s * m_k); }
};

#include "AceTheme.hpp"

class BufferWindow : public CCLayer {
public:
    enum Tab { TAB_LOG, TAB_STATS, TAB_SETTINGS, TAB_MODES, TAB_TOOLS, TAB_DEV, TAB_COUNT };
    static bool isList(Tab t) { return t == TAB_LOG || t == TAB_STATS || t == TAB_DEV; }
    // Dev mode (2.2): hidden; clicking the version badge 5 times turns it on /
    // off. It shows the Stats and Dev tabs.
    static bool devOn() { return Mod::get()->getSavedValue<bool>("dev-tab", false); }
    // Aceware: Log, Stage, Modes, Assists. Stats and Dev stay hidden.
    static bool tabShown(int t) { return t == TAB_LOG || t == TAB_SETTINGS || t == TAB_MODES || t == TAB_TOOLS ||
                                     t == TAB_STATS; }
    // a tab's row in the sidebar (hidden tabs take no room)
    static int tabSlot(int t) {
        int slot = 0;
        for (int i = 0; i < t; i++) if (tabShown(i)) slot++;
        return slot;
    }
    static int tabSlotsShown() {
        int n = 0;
        for (int i = 0; i < TAB_COUNT; i++) if (tabShown(i)) n++;
        return n;
    }

    static BufferWindow* instance() { return s_instance; }

    // Create the window if it should be shown and isn't. It lives on Geode's
    // overlay (the layer notifications use), so it stays across scenes: menus,
    // level select, levels, pause.
    static void ensure() {
        CCNode* host = OverlayManager::get();
        if (!host) return;
        if (s_instance && s_instance->getParent() == host) {
            // Window Size / Opacity changed in the settings: rebuild it.
            bool const stale = std::fabs(Config::load().hudScale - s_builtScale) > 1e-3f ||
                               std::fabs(windowOpacity() - s_builtAlpha) > 1e-3f;
            if (!stale) {
                // (2.3) hidden for the mod's settings: it comes back when they close
                if (s_instance->m_settingsPopup && s_instance->m_settingsPopup->getParent()) return;
                if (!s_instance->isVisible()) {
                    s_instance->setVisible(true);
                    s_instance->popIn();
                }
                return;
            }
        }
        // (2.3) settings still open (Window Size / Opacity changed in them): the
        // new window waits hidden for them to close, like the old one did
        Ref<CCNode> popup = s_instance && s_instance->m_settingsPopup && s_instance->m_settingsPopup->getParent()
                                ? s_instance->m_settingsPopup : Ref<CCNode>(nullptr);
        if (s_instance) s_instance->removeFromParent(); // from an older version / scene
        auto* w = new BufferWindow();
        if (w->init()) {
            w->autorelease();
            if (popup) {
                w->m_settingsPopup = popup;
                w->setVisible(false);
            }
            host->addChild(w, 100);
        } else {
            delete w;
        }
    }

    // Is this key the Window Key? Compared by key only (Shift held or not),
    // and left / right / generic Shift all count for a Shift binding.
    static bool isWindowKey(enumKeyCodes key) {
        auto const binds = Mod::get()->getSettingValue<std::vector<Keybind>>("window-key-v3");
        auto shiftLike = [](enumKeyCodes k) { return k == KEY_Shift || k == KEY_LeftShift || k == KEY_RightShift; };
        for (Keybind const& b : binds) {
            if (b.key == key) return true;
            if (b.key == KEY_LeftMenu && key == KEY_Alt) return true;
            if (b.key == KEY_LeftShift && key == KEY_Shift) return true;
            if (b.key == KEY_Shift && shiftLike(key)) return true;
        }
        return false;
    }

    // The game is rebuilding its graphics (fullscreen <-> borderless /
    // windowed, a graphics setting, Android coming back from the background):
    // everything drawn the window's way (its shapes, the switches, the sky)
    // was made for the old graphics and comes out as big coloured blocks.
    // Throw the window away now; it's built again fresh right after (2.2).
    static void dropForReload() {
        aa::reset(); // (2.2) the smooth-shape textures belong to the old graphics
        aw::reset();
        if (!s_instance) return;
        s_instance->removeFromParent(); // (its destructor clears s_instance)
        s_instance = nullptr;
    }
    static void rebuildSoon() {
        dropForReload();
        if (Mod::get()->getSavedValue<bool>("window-shown", true)) {
            Loader::get()->queueInMainThread([] { BufferWindow::ensure(); });
        }
    }

    // Window key: flip "window shown".
    static void toggle() {
        auto* mod = Mod::get();
        // (2.3) The mod's settings are open (from the cog): the window waits
        // for them to close and comes back where it was. The key used to pull
        // it up over the settings, and the next press hid it / brought it back
        // somewhere else.
        if (s_instance && s_instance->m_settingsPopup && s_instance->m_settingsPopup->getParent()) return;
        // Shown in settings but hidden on screen (e.g. after "All settings"): just bring it back.
        if (mod->getSavedValue<bool>("window-shown", true) && s_instance && !s_instance->isVisible()) {
            ensure();
            return;
        }
        bool const on = !mod->getSavedValue<bool>("window-shown", true);
        mod->setSavedValue<bool>("window-shown", on);
        s_sim.settingsChanged();
        if (on) ensure();
        else if (s_instance) s_instance->popOut();
    }

protected:
    static inline BufferWindow* s_instance = nullptr;

    // Layout (window-local units; the whole window is scaled by "Window Size").
    // 2.2: a sidebar on the left (title + the tabs), the page on the right.
    // W is the PAGE width - every page is laid out in page coordinates.
    static constexpr float SIDE_W  = 104.f; // (2.2 Aceware: room for course demo and a big ACEWARE)
    static constexpr bool DEMO_RUN_SHOWN = false; // (2.3) course demo hidden: GUI customization goes there
    static constexpr float W       = 320.f;
    static constexpr float FULL_W  = SIDE_W + W;
    static constexpr float H       = 214.f; // (2.2 Aceware: taller, roomier pages)
    static constexpr float TITLE_H = 18.f;
    static constexpr float TABS_H  = 0.f;   // (the tabs are in the sidebar)
    static constexpr float FOOT_H  = 14.f;
    static constexpr float LINE_H  = 11.f;
    static constexpr float PAD     = 6.f;
    static constexpr float SCROLL_W = 12.f;  // right strip of list tabs: arrows + scrollbar
    static constexpr float LOG_TIME_W = 24.f; // log: time column (right-aligned to this x)
    static constexpr float LOG_TEXT_X = 62.f; // log: where the text starts (after the tag)
    static constexpr int   PRIO_WINDOW = -510; // window body: swallows touches inside it
    static constexpr int   PRIO_MENUS  = -520; // its buttons: ahead of the body

    // Palette (2.2): dark glass with a purple hue - near-black panels, soft
    // violet edges, one bright violet accent.
    // (2.2 overhaul: flat charcoal panels with a purple tint, one muted violet accent)
    static inline const ccColor3B K_FRAME = {15, 16, 20};
    static inline ccColor3B C_FRAME = K_FRAME;
    static inline const ccColor3B K_EDGE = {42, 46, 54};         // outline around the window / cards
    static inline ccColor3B C_EDGE = K_EDGE;
    static inline const ccColor4B K_GRAD_TOP = {23, 25, 31, 242};   // page: charcoal at the top...
    static inline ccColor4B C_GRAD_TOP = K_GRAD_TOP;
    static inline const ccColor4B K_GRAD_BOT = {16, 17, 22, 242};   // ...a little darker at the bottom
    static inline ccColor4B C_GRAD_BOT = K_GRAD_BOT;
    static inline const ccColor3B K_SIDE = {18, 20, 25};         // sidebar
    static inline ccColor3B C_SIDE = K_SIDE;
    static inline const ccColor3B K_SIDE_SEL = {34, 39, 47};        // selected tab in the sidebar
    static inline ccColor3B C_SIDE_SEL = K_SIDE_SEL;
    static inline const ccColor3B K_CARD = {23, 25, 31};
    static inline ccColor3B C_CARD = K_CARD;
    // (2.2) card outlines: a violet glow at rest, brighter under the mouse,
    // flaring when a tab opens; grey for a mode that's off
    static inline const ccColor3B K_CARD_REST = {56, 63, 74};
    static inline ccColor3B C_CARD_REST = K_CARD_REST;
    static inline const ccColor3B K_CARD_HOV = {95, 211, 160};
    static inline ccColor3B C_CARD_HOV = K_CARD_HOV;
    static inline const ccColor3B K_CARD_LIT = {150, 240, 200};
    static inline ccColor3B C_CARD_LIT = K_CARD_LIT;
    static inline const ccColor3B K_CARD_OFF = {36, 40, 47};
    static inline ccColor3B C_CARD_OFF = K_CARD_OFF;
    static inline const ccColor3B K_GLOW = {95, 211, 160};
    static inline ccColor3B C_GLOW = K_GLOW;
    static inline const ccColor3B K_EXTREME      = {38, 132, 101};  // the Extreme button: hot pink (2.3: follows the accent)
    static inline ccColor3B C_EXTREME = K_EXTREME;
    static inline const ccColor3B K_EXTREME_TEXT = {150, 235, 200};
    static inline ccColor3B C_EXTREME_TEXT = K_EXTREME_TEXT;
    static inline const ccColor3B K_ROW_LINE = {34, 38, 45};        // thin line between rows
    static inline ccColor3B C_ROW_LINE = K_ROW_LINE;
    static inline const ccColor3B K_BTN = {32, 36, 42};         // button box
    static inline ccColor3B C_BTN = K_BTN;
    static inline const ccColor3B K_BTN_SEL = {42, 120, 92};       // selected stage / main buttons (2.2: muted)
    static inline ccColor3B C_BTN_SEL = K_BTN_SEL;
    static inline const ccColor3B K_BTN_HOT = {186, 146, 72};       // unsaved Apply
    static inline ccColor3B C_BTN_HOT = K_BTN_HOT;
    static inline const ccColor3B K_SW_OFF = {48, 53, 62};         // switch track, off
    static inline ccColor3B C_SW_OFF = K_SW_OFF;
    static inline const ccColor3B K_PILL = {26, 29, 35};
    static inline ccColor3B C_PILL = K_PILL;
    static inline const ccColor3B K_LAV = {230, 234, 240};      // card titles (2.2: near white, like the reference)
    static inline ccColor3B C_LAV = K_LAV;
    static inline const ccColor3B K_MUTED = {150, 158, 170};  // secondary text (brighter in 2.2)
    static inline ccColor3B C_MUTED = K_MUTED;
    static inline const ccColor3B K_DIM = {108, 116, 130};
    static inline ccColor3B C_DIM = K_DIM;

    Tab   m_tab = TAB_LOG;
    bool  m_shownOnce = false;
    int   m_offset = 0;            // first visible row
    CCNode*        m_body = nullptr;     // the full window (everything below is in here)
    CCNode*        m_tabSlots = nullptr; // the four tab boxes (static)
    CCNode*        m_tabHi = nullptr;    // the selected tab's highlight (slides)
    bool           m_tabHiPlaced = false;
    float          m_alpha = 1.f;        // Window Opacity setting (panels only, text stays solid)
    static inline float s_builtScale = -1.f, s_builtAlpha = -1.f;
    int   m_rowCount = 0;
    bool  m_dragging = false;
    bool  m_scrollDrag = false;
    bool  m_resizing = false;
    float m_dragStartScale = 1.f;
    float m_dragAccum = 0.f;
    // animation state (Aceware)
    float m_sweepT = 0.f;      // seconds until the accent line sweeps again
    float m_scrollFade = 0.f;  // scrollbar fade timer
    float m_animT = 0.f;       // animation clock
    Ref<CCLabelBMFont> m_badgeLab;   // the version under the wordmark (breathes)
    std::vector<CCNode*> m_grip;     // the resize strokes (pulse)
    float m_refreshAt = 0.f;       // seconds until the next content refresh
    bool  m_interactive = true;

    CCLabelBMFont* m_header = nullptr;
    CCLayerGradient* m_accent = nullptr;   // line under the title
    CCDrawNode*    m_headerPill = nullptr;
    CCNode*        m_headerFx = nullptr;   // (2.2) Extreme's glow on the stage pill
    CCNode*        m_stripes = nullptr;
    CCLayerColor*  m_scrollTrack = nullptr;
    CCLayerColor*  m_scrollThumb = nullptr;
    CCLabelBMFont* m_footer = nullptr;
    CCLabelBMFont* m_platform = nullptr; // footer, right: "Windows 11" etc. (2.1)
    CCNode* m_page = nullptr;      // the page (right of the sidebar); pages use its coordinates
    aw::Starfield* m_pageSky = nullptr; // (2.2) stars + nebulae behind the page
    aw::Starfield* m_sideSky = nullptr; // and a few in the sidebar
    CCSprite* m_accentSweep = nullptr;  // the light that runs along the header line
    CCNode*   m_runBox = nullptr;       // course demo: its card
    aw::CourseView* m_run = nullptr;    // course demo itself
    CCRect    m_runRect;
    bool      m_tapLogo = false;        // a click on ACEWARE calls the robot
    Clock::time_point m_lastHeaderTap{}; // (2.3) double-click the header: back to the top right
    float     m_tapMoved = 0.f;
    float   m_badgeW = 20.f;
    int     m_badgeTaps = 0;
    float   m_badgeTapAge = 0.f;
    CCLabelBMFont* m_pageTitle = nullptr;
    CCMenu* m_titleMenu = nullptr; // close button
    CCMenu* m_tabMenu = nullptr;
    CCMenu* m_themeMenu = nullptr; // (2.3) the Accent / Background dots
    struct ThemeHov { CCNode* glow; int row; std::string name; ccColor3B col; };
    std::vector<ThemeHov> m_themeHov;
    CCLabelBMFont* m_themeLab[2] = {nullptr, nullptr};
    int m_themeSlots = -1;         // tabs shown when it was built (Dev mode changes the room)
    CCMenu* m_listMenu = nullptr;  // scroll arrows / list buttons
    CCMenu* m_settingsMenu = nullptr;
    CCNode* m_settingsLabels = nullptr;
    CCNode* m_cards = nullptr;     // card panels behind the Stage / Modes pages
    std::vector<CCLabelBMFont*> m_rows;
    std::vector<CCLabelBMFont*> m_rowsR; // right-aligned values ("label<TAB>value" rows)
    std::vector<CCLabelBMFont*> m_rowsH; // bold section titles (rows starting with '\x01') / log tags
    std::vector<CCLabelBMFont*> m_rowsT; // log time column
    std::vector<std::pair<std::string, ccColor3B>> m_data;

    ~BufferWindow() override {
        if (s_instance == this) s_instance = nullptr;
    }

    static float contentTop()    { return H - TITLE_H - TABS_H - 2.f; }
    static float contentBottom() { return FOOT_H + 2.f; }
    static int   visibleRows()   { return static_cast<int>((contentTop() - contentBottom()) / LINE_H); }
    static float trackTop()      { return contentTop() - 12.f; }
    static float trackBottom()   { return contentBottom() + 12.f; }

    // (2.3) GUI colours: a card in the sidebar with two rows of 6 dots -
    // Accent (glow, buttons, outlines, tinted text) and Background (the dark
    // panels). Click a dot: saved, and the window is built again in it.
    void buildThemeBox(float cx, float cy, float cw, float ch) {
        if (ch < 22.f) return; // no room
        bool const compact = ch < 46.f; // (Dev tabs on: two rows of dots, no titles - a click says which)
        auto op = [&](float a) { return static_cast<GLubyte>(std::clamp(a * m_alpha, 0.f, 255.f)); };
        if (CCNode* g = aw::glowBox(cx, cy, cw, ch, 5.5f, C_GLOW, op(60.f))) m_body->addChild(g, 1);
        CCNode* e = aa::roundBox(cw, ch, 7.f, C_CARD_REST, op(190.f));
        if (!e) e = panel(cw, ch, C_CARD_REST, op(220.f), 5.5f);
        e->setPosition({cx, cy});
        m_body->addChild(e, 1);
        if (CCNode* in = aa::roundBox(cw - 1.6f, ch - 1.6f, 6.2f, C_CARD, op(240.f))) {
            in->setPosition({cx + 0.8f, cy + 0.8f});
            m_body->addChild(in, 1);
        }
        m_themeMenu = makeMenu(m_body);
        int const curA = std::clamp(static_cast<int>(Mod::get()->getSavedValue<int>("theme-accent", 0)), 0, 5);
        int const curB = std::clamp(static_cast<int>(Mod::get()->getSavedValue<int>("theme-back", 0)), 0, 5);
        float const rowH = ch / 2.f;
        float const d = std::min(compact ? std::max(5.f, rowH - 5.f) : 9.f, (cw - 12.f) / 6.f - 3.f); // dot size
        float const step = (cw - 12.f) / 6.f;
        for (int row = 0; row < 2; row++) {
            bool const accent = row == 0;
            float const top = cy + ch - static_cast<float>(row) * rowH;
            m_themeLab[row] = nullptr;
            if (!compact) {
                auto* lab = bodyLabel(accent ? "Accent" : "Background", 0.2f, C_MUTED);
                lab->setAnchorPoint({0.f, 0.5f});
                lab->setPosition({cx + 6.f, top - 6.5f});
                m_body->addChild(lab, 3);
                m_themeLab[row] = lab;
            }
            // (2.3) Presets (top right): a whole look - accent + background -
            // built in or saved by you ("+" saves the current one)
            if (accent && !compact) buildPresetButtons(cx, cw, top - 6.5f, curA, curB);
            int const cur = accent ? curA : curB;
            for (int i = 0; i < 6; i++) {
                aw::ThemeDot const& td = accent ? aw::THEME_ACCENTS[i] : aw::THEME_BACKS[i];
                float const box = d + 4.f;
                auto* node = CCNode::create();
                node->setContentSize({box, box});
                // (2.3) under the mouse: it glows in its own colour (and the
                // row's title says its name, in that colour)
                // (the dark background colours a little lighter, so they read)
                auto lift = [&](GLubyte v) { return static_cast<GLubyte>(v + (255 - v) * (accent ? 0.f : 0.45f)); };
                ccColor3B const shine = {lift(td.dot.r), lift(td.dot.g), lift(td.dot.b)};
                if (CCSprite* glow = aw::sprite(aw::soft(), d * 2.8f, shine, 0)) {
                    aw::additive(glow);
                    glow->setTag(HOVER_TAG);
                    glow->setPosition({box / 2.f, box / 2.f});
                    node->addChild(glow, -1);
                    m_themeHov.push_back({glow, row, std::string(td.name), shine});
                }
                // the chosen one: a white ring; the rest: a faint one
                if (auto* ring = aa::circleSprite(d + 2.6f, i == cur ? ccColor3B{255, 255, 255} : C_EDGE, i == cur ? 255 : 200)) {
                    ring->setPosition({box / 2.f, box / 2.f});
                    node->addChild(ring);
                }
                if (auto* gap = aa::circleSprite(d + 0.9f, C_CARD)) {
                    gap->setPosition({box / 2.f, box / 2.f});
                    node->addChild(gap);
                }
                if (auto* fill = aa::circleSprite(d, td.dot)) { // the colour itself (not themed)
                    fill->setPosition({box / 2.f, box / 2.f});
                    node->addChild(fill);
                }
                char const* key = accent ? "theme-accent" : "theme-back";
                std::string const name = td.name;
                auto* item = CCMenuItemExt::createSpriteExtra(node, [key, i, cur, name, accent](CCMenuItemSpriteExtra*) {
                    if (i == cur) return;
                    Mod::get()->setSavedValue<int>(key, i);
                    Notification::create(fmt::format("{}: {}", accent ? "Accent" : "Background", name), NotificationIcon::None, 1.f)->show();
                    // built again in the new colours (after this click is done)
                    Loader::get()->queueInMainThread([] { BufferWindow::rebuildSoon(); });
                });
                item->setSizeMult(1.15f);
                item->setPosition({cx + 6.f + step * (static_cast<float>(i) + 0.5f), compact ? top - rowH * 0.5f : top - rowH * 0.64f});
                m_themeMenu->addChild(item);
            }
        }
    }
    // (2.3) Theme presets: built-in looks, plus up to 6 of yours (saved value
    // "theme-presets": "accent,background;..."). "Presets" applies the next
    // one; "+" saves the current look as one of yours.
    struct ThemePreset { std::string name; int a, b; };
    static std::vector<ThemePreset> themePresets() {
        std::vector<ThemePreset> out = {{"Aceware", 0, 0},    {"Royal", 1, 1},   {"Deep Sea", 2, 2}, {"Toxic", 3, 3},
                                        {"Inferno", 4, 4}, {"Sakura", 5, 0},  {"Stealth", 2, 5},  {"Sunset", 5, 4}};
        std::string const mine = Mod::get()->getSavedValue<std::string>("theme-presets", "");
        std::istringstream in(mine);
        std::string part;
        int n = 0;
        while (std::getline(in, part, ';')) {
            int a = 0, b = 0;
            if (std::sscanf(part.c_str(), "%d,%d", &a, &b) == 2) out.push_back({fmt::format("My Look {}", ++n), std::clamp(a, 0, 5), std::clamp(b, 0, 5)});
        }
        return out;
    }
    void buildPresetButtons(float cx, float cw, float y, int curA, int curB) {
        float const bw = 30.f, pw = 9.f;
        boxButton(m_themeMenu, "Presets", {cx + cw - 6.f - pw - 3.f - bw / 2.f, y}, bw, 9.f, C_BTN, aw::themed(ccColor3B{255, 255, 255}), 0.17f,
                  [curA, curB] {
            auto const all = themePresets();
            size_t next = 0;
            for (size_t i = 0; i < all.size(); i++) {
                if (all[i].a == curA && all[i].b == curB) { next = (i + 1) % all.size(); break; }
            }
            Mod::get()->setSavedValue<int>("theme-accent", all[next].a);
            Mod::get()->setSavedValue<int>("theme-back", all[next].b);
            Notification::create(fmt::format("Preset: {} ({} / {})", all[next].name, next + 1, all.size()), NotificationIcon::None, 1.f)->show();
            Loader::get()->queueInMainThread([] { BufferWindow::rebuildSoon(); });
        });
        boxButton(m_themeMenu, "+", {cx + cw - 6.f - pw / 2.f, y}, pw, 9.f, C_BTN, aw::themed(ccColor3B{255, 255, 255}), 0.2f, [curA, curB] {
            std::string mine = Mod::get()->getSavedValue<std::string>("theme-presets", "");
            std::vector<std::string> parts;
            std::istringstream in(mine);
            std::string part;
            while (std::getline(in, part, ';')) if (!part.empty()) parts.push_back(part);
            std::string const now = fmt::format("{},{}", curA, curB);
            if (std::find(parts.begin(), parts.end(), now) != parts.end()) {
                Notification::create("This look is saved already", NotificationIcon::None, 1.f)->show();
                return;
            }
            parts.push_back(now);
            while (parts.size() > 6) parts.erase(parts.begin()); // the oldest goes
            std::string out;
            for (auto const& p2 : parts) out += (out.empty() ? "" : ";") + p2;
            Mod::get()->setSavedValue<std::string>("theme-presets", out);
            Notification::create(fmt::format("Saved as My Look {} (Presets cycles through it)", parts.size()), NotificationIcon::None, 1.2f)->show();
        });
    }
    // (2.3) GUI colours (Sidebar: Accent / Background dots): the window's
    // colours through the theme, before anything is built
    static void applyPalette() {
        // Aceware: presets are gone, so any accent/background saved by an older
        // build is ignored and the fixed palette below is what gets used.
        Mod::get()->setSavedValue<int>("theme-accent", 0);
        Mod::get()->setSavedValue<int>("theme-back", 0);
        aw::setTheme(0, 0);
        C_FRAME = aw::themed(K_FRAME);
        C_EDGE = aw::themed(K_EDGE);
        C_GRAD_TOP = aw::themed(K_GRAD_TOP);
        C_GRAD_BOT = aw::themed(K_GRAD_BOT);
        C_SIDE = aw::themed(K_SIDE);
        C_SIDE_SEL = aw::themed(K_SIDE_SEL);
        C_CARD = aw::themed(K_CARD);
        C_CARD_REST = aw::themed(K_CARD_REST);
        C_CARD_HOV = aw::themed(K_CARD_HOV);
        C_CARD_LIT = aw::themed(K_CARD_LIT);
        C_CARD_OFF = aw::themed(K_CARD_OFF);
        C_GLOW = aw::themed(K_GLOW);
        C_ROW_LINE = aw::themed(K_ROW_LINE);
        C_BTN = aw::themed(K_BTN);
        C_BTN_SEL = aw::themed(K_BTN_SEL);
        C_SW_OFF = aw::themed(K_SW_OFF);
        C_PILL = aw::themed(K_PILL);
        C_LAV = aw::themed(K_LAV);
        C_MUTED = aw::themed(K_MUTED);
        C_DIM = aw::themed(K_DIM);
        C_ROW = aw::themed(K_ROW);
        C_ROW_ON = aw::themed(K_ROW_ON);
        C_HOVER = aw::themed(K_HOVER);
        C_EXTREME = aw::themed(K_EXTREME);
        C_EXTREME_TEXT = aw::themed(K_EXTREME_TEXT);
        C_BTN_HOT = aw::themed(K_BTN_HOT);
    }
    bool init() override {
        if (!CCLayer::init()) return false;
        applyPalette();
        s_instance = this;
        setContentSize({FULL_W, H});
        ignoreAnchorPointForPosition(false);
        setAnchorPoint({0.f, 0.f});

        Config const c = Config::load();
        // Window Size 1.0 = half the original size. Phones get PLATFORM_WINDOW_SCALE
        // on top (2.1; 2.3: 1.1 on iOS / Android, was 1.5), capped to the screen.
        float scale = 0.5f * PLATFORM_WINDOW_SCALE * std::clamp(c.hudScale, 0.5f, 2.0f);
        auto const screen = CCDirector::get()->getWinSize();
        if (screen.height > 1.f) scale = std::min(scale, screen.height * WINDOW_MAX_SCREEN_H / H); // (2.3) fits the screen
        if (screen.width > 1.f) scale = std::min(scale, screen.width * WINDOW_MAX_SCREEN_W / FULL_W);
        setScale(scale);
        m_alpha = windowOpacity();
        s_builtScale = c.hudScale;
        s_builtAlpha = m_alpha;
        auto op = [&](float a) { return static_cast<GLubyte>(std::clamp(a * m_alpha, 0.f, 255.f)); };

        // Everything lives in m_body (centred anchor, so it can pop in from the
        // middle); the in-game pill is a sibling.
        m_body = CCNode::create();
        m_body->setContentSize({FULL_W, H});
        m_body->setAnchorPoint({0.5f, 0.5f});
        m_body->setPosition({FULL_W / 2.f, H / 2.f});
        this->addChild(m_body, 0);

        // Frame: a soft violet edge around a near-black glass panel.
        auto* edge = panel(FULL_W, H, C_EDGE, op(150.f), 10.f);
        m_body->addChild(edge, 0);
        auto* frame = panel(FULL_W - 2.f, H - 2.f, C_FRAME, op(245.f), 9.f);
        frame->setPosition({1.f, 1.f});
        m_body->addChild(frame, 0);

        // Sidebar panel, with a few slow stars of its own.
        auto* side = panel(SIDE_W - 4.f, H - 8.f, C_SIDE, op(235.f), 7.f);
        side->setPosition({4.f, 4.f});
        m_body->addChild(side, 0);
        m_sideSky = aw::Starfield::create(SIDE_W - 10.f, H - 14.f, 16, 9001u, m_alpha, 0.9f, false);
        if (m_sideSky) {
            m_sideSky->setPosition({7.f, 7.f});
            m_body->addChild(m_sideSky, 0);
        }

        // The page (right of the sidebar): its own node, so every page keeps
        // its own coordinates (0..W).
        m_page = CCNode::create();
        m_page->setContentSize({W, H});
        m_page->setPosition({SIDE_W, 0.f});
        m_body->addChild(m_page, 1);

        // Page background: dusk purple fading to black.
        ccColor4B gTop = C_GRAD_TOP, gBot = C_GRAD_BOT;
        gTop.a = op(gTop.a);
        gBot.a = op(gBot.a);
        // (rounded corners, 2.2 - a gradient layer is a plain box)
        auto* body = CCDrawNode::create();
        roundedGradient(body, W - 8.f, H - TITLE_H - FOOT_H - 3.f, 7.f, gTop, gBot);
        body->setPosition({3.f, FOOT_H + 1.f});
        m_page->addChild(body, 0);

        // (2.2) deep space behind the page: slow parallax stars that
        // twinkle, and soft nebulae that breathe. Kept faint - text first.
        {
            float const bw = W - 8.f, bh = H - TITLE_H - FOOT_H - 3.f;
            CCNode* clip = aw::clipped(bw, bh, 7.f);
            clip->setPosition({3.f, FOOT_H + 1.f});
            m_page->addChild(clip, 0);
            m_pageSky = aw::Starfield::create(bw, bh, 46, 12345u, m_alpha, 3.f, true);
            if (m_pageSky) clip->addChild(m_pageSky);
        }

        // Accent line under the page header: violet into pink. A light runs
        // along it when you open a tab.
        m_accent = CCLayerGradient::create(aw::themed(ccColor4B{95, 211, 160, 210}), aw::themed(ccColor4B{48, 120, 98, 130}), CCPoint{1.f, 0.f});
        m_accent->setContentSize({W - 14.f, 1.f});
        m_accent->setPosition({6.f, H - TITLE_H});
        m_page->addChild(m_accent, 1);
        m_accentSweep = aw::sprite(aw::soft(), 30.f, aw::themed(ccColor3B{185, 240, 215}), 0);
        if (m_accentSweep) {
            aw::additive(m_accentSweep);
            m_accentSweep->setScaleY(m_accentSweep->getScaleX() * 0.18f);
            m_accentSweep->setPosition({6.f, H - TITLE_H + 0.5f});
            m_page->addChild(m_accentSweep, 2);
        }

        // ---- sidebar header (2.2), centred: ACEWARE in neon (each letter
        // lights up under the mouse; the O is a ring galaxy), "ACEWARE"
        // under it, then the version - glowing too (5 quick clicks: Dev tab) -
        // and the author on one line.
        float const sideMid = (SIDE_W + 4.f) / 2.f; // the sidebar panel's middle
        // ACEWARE and V2.2 side by side, the author under them, all centred
        std::string const badge = titleBadge();
        auto* v2 = AceLabel::make(badge.c_str(), "bigFont.fnt");
        v2->setScale(0.28f);
        m_badgeW = v2->getContentSize().width * v2->getScale();
        constexpr float VER_GAP = 4.f;
        float const wordY = H - 20.f; // (the author's line moved to the Log)
        buildWordmark(sideMid, wordY + 4.f, 0.f); // centred on its own, no version beside it
        m_verX = sideMid - m_badgeW / 2.f;   // version centred UNDER the wordmark
        m_verY = wordY - 14.f;
        // (Aceware) no bloom behind the version
        // (2.2) ACEWARE sits in the middle between the panel's top and the line:
        // measured from the letters as drawn (glyphs, the galaxy O), the line
        // goes as far under them as the panel's top is above them; the tabs
        // follow the line.
        float const lineY = std::clamp(wordLineY(), H - 58.f, H - 42.f);
        s_tabTop = lineY - 8.f;
        auto* sideLine = CCLayerColor::create(aw::themed(ccColor4B{120, 130, 145, 32}), SIDE_W - 20.f, 1.f);
        sideLine->setPosition({10.f, lineY});
        m_body->addChild(sideLine, 2);

        // ---- course demo (2.2): your icons, one gamemode at a time, through GD
        // obstacles; a portal at the end of each section changes the mode.
        // Click the icon and it explodes (and tries again).
        // (2.3) Hidden for now - that spot gets GUI customization (colours
        // etc.) next. The code stays (DEMO_RUN_SHOWN, AceTheme.hpp's CourseView).
        if (DEMO_RUN_SHOWN) {
            float const cx = 8.f, cy = 27.f, cw = SIDE_W - 16.f;
            float const ch = std::min(62.f, sideItemY(3) - SIDE_ITEM_H / 2.f - 6.f - cy); // under the four tabs
            m_runRect = CCRect(cx, cy, cw, ch);
            m_runBox = CCNode::create();
            m_body->addChild(m_runBox, 1);
            if (CCNode* g = aw::glowBox(cx, cy, cw, ch, 5.5f, C_GLOW, op(60.f))) m_runBox->addChild(g);
            CCNode* e = aa::roundBox(cw, ch, 7.f, C_CARD_REST, op(190.f));
            if (!e) e = panel(cw, ch, C_CARD_REST, op(220.f), 5.5f);
            e->setPosition({cx, cy});
            m_runBox->addChild(e);
            m_run = aw::CourseView::create(cw - 2.f, ch - 2.f, m_alpha);
            if (m_run) {
                m_run->setPosition({cx + 1.f, cy + 1.f});
                m_runBox->addChild(m_run);
            }
        }
        // (2.3) GUI colours, where course demo was: Accent and Background, 6 dots each
        // (under the last tab shown: smaller when the Dev tabs are on)
        if (!DEMO_RUN_SHOWN) {
            float const cx = 8.f, cy = 27.f, cw = SIDE_W - 16.f;
            float const ch = std::min(62.f, sideItemY(tabSlotsShown() - 1) - SIDE_ITEM_H / 2.f - 6.f - cy);
            m_themeSlots = tabSlotsShown();
            // (Aceware) colour presets removed: one fixed palette, see K_* constants
        }

        // Tabs: a highlight that slides to the selected one, items over it.
        m_tabSlots = CCNode::create();
        m_body->addChild(m_tabSlots, 1);
        m_tabHi = aa::roundBox(SIDE_W - 16.f, SIDE_ITEM_H, 5.f, C_SIDE_SEL, 255, true);
        if (!m_tabHi) m_tabHi = panel(SIDE_W - 16.f, SIDE_ITEM_H, C_SIDE_SEL, 255, 5.f);
        m_body->addChild(m_tabHi, 2);

        // System, bottom of the sidebar ("Windows 11" etc.).
        auto* sideLine2 = CCLayerColor::create(aw::themed(ccColor4B{120, 130, 145, 30}), SIDE_W - 20.f, 1.f);
        sideLine2->setPosition({10.f, 21.f});
        m_body->addChild(sideLine2, 2);
        std::string const sys = (systemInfo().side.empty() || systemInfo().brief.rfind("Wine", 0) == 0)
                                    ? systemInfo().brief : systemInfo().side;
        m_platform = AceLabel::make(sys.c_str(), "chatFont.fnt");
        m_platform->setScale(0.42f);
        limitNodeWidth(m_platform, SIDE_W - 14.f, 0.42f, 0.2f);
        m_platform->setColor(C_MUTED);
        m_platform->setPosition({(SIDE_W + 2.f) / 2.f, 12.5f});
        m_body->addChild(m_platform, 2);

        // ---- page header: page name on the left, status pill + close on the right ----
        m_pageTitle = AceLabel::make("", "bigFont.fnt");
        m_pageTitle->setScale(0.4f);
        m_pageTitle->setColor(aw::themed(ccColor3B{255, 255, 255}));
        m_pageTitle->setAnchorPoint({0.f, 0.5f});
        m_pageTitle->setPosition({PAD + 2.f, H - TITLE_H / 2.f});
        m_page->addChild(m_pageTitle, 2);

        m_headerPill = panel(60.f, 12.f, C_PILL, 240);
        m_headerPill->setAnchorPoint({1.f, 0.5f});
        m_headerPill->setPosition({W - 21.f, H - TITLE_H / 2.f});
        m_page->addChild(m_headerPill, 1);
        // Online users (2.2): "(green) 12 online  (grey) 328 offline", left of the stage pill
        m_onlineDots = CCNode::create();
        m_page->addChild(m_onlineDots, 2);
        m_onlineOn = bodyLabel("", 0.26f);
        m_onlineOn->setAnchorPoint({0.f, 0.5f});
        m_page->addChild(m_onlineOn, 2);
        m_onlineOff = bodyLabel("", 0.26f, C_MUTED);
        m_onlineOff->setAnchorPoint({0.f, 0.5f});
        m_page->addChild(m_onlineOff, 2);
        m_header = bodyLabel("", 0.28f);
        m_header->setAnchorPoint({1.f, 0.5f});
        m_header->setPosition({W - 26.f, H - TITLE_H / 2.f});
        m_page->addChild(m_header, 2);

        // Card panels (Stage / Modes) sit under the page content.
        m_cards = CCNode::create();
        m_page->addChild(m_cards, 1);

        // Zebra stripes behind the list rows.
        m_stripes = CCNode::create();
        m_page->addChild(m_stripes, 1);
        for (int i = 1; i < visibleRows(); i += 2) {
            auto* st = CCLayerColor::create(aw::themed(ccColor4B{255, 255, 255, 7}), W - 2.f * PAD - SCROLL_W + 2.f, LINE_H);
            st->setPosition({PAD - 2.f, contentTop() - static_cast<float>(i + 1) * LINE_H + 1.f});
            m_stripes->addChild(st);
        }

        // Scrollbar: a faint track and a thumb sized to how much of the list is visible.
        m_scrollTrack = CCLayerColor::create(aw::themed(ccColor4B{255, 255, 255, 18}), 2.f, trackTop() - trackBottom());
        m_scrollTrack->setPosition({W - PAD - SCROLL_W / 2.f - 1.f, trackBottom()});
        m_page->addChild(m_scrollTrack, 1);
        m_scrollThumb = CCLayerColor::create(aw::themed(ccColor4B{95, 211, 160, 200}), 2.f, 10.f);
        m_scrollThumb->setPosition({W - PAD - SCROLL_W / 2.f - 1.f, trackBottom()});
        m_page->addChild(m_scrollThumb, 2);

        // Footer: a faint line, then how to show / hide the window.
        auto* footLine = CCLayerColor::create(aw::themed(ccColor4B{120, 130, 145, 34}), W - 14.f, 1.f);
        footLine->setPosition({6.f, FOOT_H});
        m_page->addChild(footLine, 1);
        m_footer = bodyLabel("", 0.23f, C_MUTED);
        m_footer->setAnchorPoint({0.f, 0.5f});
        m_footer->setPosition({PAD + 2.f, FOOT_H / 2.f});
        m_page->addChild(m_footer, 2);

        m_titleMenu = makeMenu(m_page);
        // (2.2) settings: a round button with a cog, bottom right (right
        // of the online count). Opens the mod's settings; the cog turns while
        // the mouse is on it.
        {
            float const d = 11.5f;
            auto* node = CCNode::create();
            node->setContentSize({d, d});
            if (auto* ring = aa::circleSprite(d, aw::themed(ccColor3B{52, 58, 68}))) { ring->setPosition({d / 2.f, d / 2.f}); node->addChild(ring); }
            if (auto* fill = aa::circleSprite(d - 1.3f, C_BTN)) { fill->setPosition({d / 2.f, d / 2.f}); node->addChild(fill); }
            if (auto* hov = aa::circleSprite(d, C_HOVER, 0)) {
                hov->setTag(HOVER_TAG);
                hov->setPosition({d / 2.f, d / 2.f});
                node->addChild(hov);
                m_gearHover = hov;
            }
            m_gear = aw::sprite(aw::gear(), 7.6f, aw::themed(ccColor3B{208, 214, 222}));
            if (m_gear) {
                m_gear->setPosition({d / 2.f, d / 2.f});
                node->addChild(m_gear);
            }
            auto* item = CCMenuItemExt::createSpriteExtra(node, [this](CCMenuItemSpriteExtra*) { openSettings(); });
            item->setSizeMult(1.1f);
            // The far right of the footer belongs to the resize grip: the cog sits
            // one slot further in, so the corner handle can't draw over it.
            item->setPosition({W - PAD - d / 2.f - 6.f, FOOT_H / 2.f + 0.5f});
            m_titleMenu->addChild(item);
        }
        // Close: a white X in a small box.
        boxButton(m_titleMenu, "X", {W - 11.f, H - TITLE_H / 2.f}, 12.f, 12.f, C_BTN, aw::themed(ccColor3B{255, 255, 255}), 0.3f, [] {
            BufferWindow::toggle();
            Notification::create(fmt::format("Aceware hidden - {} shows it again", showHowTo()),
                                 NotificationIcon::None, 2.0f)->show();
        });

        m_tabMenu = makeMenu(m_body);
        m_listMenu = makeMenu(m_page);
        m_settingsMenu = makeMenu(m_page);
        m_settingsLabels = CCNode::create();
        m_page->addChild(m_settingsLabels, 3);

        // Row label pool.
        for (int i = 0; i < visibleRows(); i++) {
            auto* l = AceLabel::make("", "chatFont.fnt");
            l->setAnchorPoint({0.f, 1.f});
            l->setScale(0.5f);
            l->setPosition({PAD + 1.f, contentTop() - static_cast<float>(i) * LINE_H});
            m_page->addChild(l, 2);
            m_rows.push_back(l);
            auto* r = AceLabel::make("", "chatFont.fnt");
            r->setAnchorPoint({1.f, 1.f});
            r->setScale(0.5f);
            r->setPosition({W - PAD - SCROLL_W - 2.f, contentTop() - static_cast<float>(i) * LINE_H});
            m_page->addChild(r, 2);
            m_rowsR.push_back(r);
            auto* h = AceLabel::make("", "bigFont.fnt");
            h->setAnchorPoint({0.f, 0.5f});
            h->setScale(0.3f);
            h->setColor(aw::themed(ccColor3B{255, 255, 255}));
            h->setPosition({PAD + 1.f, contentTop() - static_cast<float>(i) * LINE_H - LINE_H / 2.f});
            m_page->addChild(h, 2);
            m_rowsH.push_back(h);
            auto* tl = AceLabel::make("", "chatFont.fnt");
            tl->setAnchorPoint({1.f, 1.f});
            tl->setScale(0.42f);
            tl->setColor(C_DIM);
            tl->setPosition({PAD + LOG_TIME_W, contentTop() - static_cast<float>(i) * LINE_H - 0.5f});
            m_page->addChild(tl, 2);
            m_rowsT.push_back(tl);
        }

        // resize grip: a 45° corner mark, drawn as segments (a 9-slice this small
        // distorts). Hidden unless you are dragging the corner: the settings cog
        // lives next to it, and a mark at rest reads as a stray line.
        if (auto* grip = CCDrawNode::create()) {
            ccColor4F gc = ccc4FFromccc3B(C_DIM);
            gc.a = 0.67f;
            for (int i = 0; i < 3; i++) {
                float const len = 8.f - static_cast<float>(i) * 2.5f;
                CCPoint const a{FULL_W - 9.f, 3.f + static_cast<float>(i) * 3.5f};
                CCPoint const b{a.x + len * 0.7071f, a.y + len * 0.7071f};
                grip->drawSegment(a, b, 0.45f, gc);
            }
            grip->setVisible(false);
            this->addChild(grip, 6);
            m_grip.push_back(grip);
        }

        // (1.4.4) a small cat, drawn from circles so it costs nothing and can't be
        // missing. Hidden until it feels like a walk along the footer.

        restorePosition();
        setTouchEnabled(true);
        // (mouse wheel: BufMouse hook -> takeScroll, so it's handled before anything behind the window)
        scheduleUpdate();
        int lastTab = std::clamp(static_cast<int>(Mod::get()->getSavedValue<int>("last-tab", 0)), 0, TAB_COUNT - 1);
        if (!tabShown(lastTab)) lastTab = TAB_LOG;
        showTab(static_cast<Tab>(lastTab));
        popIn();
        if (LogConsole::available() && Mod::get()->getSavedValue<bool>("log-console", false)) LogConsole::open();
        return true;
    }

    static float windowOpacity() {
        return static_cast<float>(std::clamp(Mod::get()->getSettingValue<double>("window-opacity"), 0.3, 1.0));
    }

    // Open (2.2): grows in from 86% with a small overshoot while rising 8
    // units into place. Close: shrinks away, then hides.
    // (2.3) The mod's settings: the window steps aside while they're open and
    // comes back by itself when they close (on phones there was no key to
    // bring it back - only the pause menu button).
    Ref<CCNode> m_settingsPopup;
    void openSettings() {
        setVisible(false);
        m_settingsPopup = openSettingsPopup(Mod::get(), false);
    }
    void popIn() {
        s_onlineNow = true; // fresh online numbers when the window opens
        if (!m_body) return;
        m_body->stopAllActions();
        CCPoint const home{FULL_W / 2.f, H / 2.f};
        m_body->setScale(0.86f);
        m_body->setPosition({home.x, home.y - 8.f});
        m_body->runAction(CCEaseBackOut::create(CCScaleTo::create(0.24f, 1.f)));
        m_body->runAction(CCEaseOut::create(CCMoveTo::create(0.2f, home), 2.5f));
        if (auto* pr = typeinfo_cast<CCRGBAProtocol*>(m_page)) {
            pr->setOpacity(0);
            m_page->runAction(CCSequence::create(CCDelayTime::create(0.04f), CCFadeTo::create(0.2f, 255), nullptr));
        }
        signOn();
    }
    void popOut() {
        if (!m_body || !isVisible()) return;
        m_body->stopAllActions();
        m_body->runAction(CCSequence::create(CCEaseIn::create(CCScaleTo::create(0.13f, 0.84f), 2.f),
                                             CCCallFunc::create(this, callfunc_selector(BufferWindow::finishClose)), nullptr));
        m_body->runAction(CCEaseIn::create(CCMoveTo::create(0.13f, CCPoint{FULL_W / 2.f, H / 2.f - 8.f}), 2.f));
    }
    void finishClose() {
        setVisible(false);
        if (!m_body) return;
        m_body->setScale(1.f);
        m_body->setPosition({FULL_W / 2.f, H / 2.f});
    }

    // Rounded box, drawn as an exact polygon (1.25.8). The 9-slice texture
    // came out square on small boxes in the game, so every button looked like a
    // block; a drawn shape has the same round corners at any size.
    static void drawRound(CCDrawNode* dn, float w, float h, ccColor3B col, GLubyte opacity, float radius = -1.f) {
        dn->clear();
        float r = radius >= 0.f ? radius : std::min(4.f, std::min(w, h) * 0.34f);
        r = std::clamp(r, 0.f, std::min(w, h) / 2.f);
        constexpr int SEG = 6; // points per quarter circle
        std::vector<CCPoint> pts;
        pts.reserve(4 * (SEG + 1));
        struct Corner { float cx, cy, a0; };
        Corner const cs[4] = {{w - r, h - r, 0.f}, {r, h - r, 90.f}, {r, r, 180.f}, {w - r, r, 270.f}};
        auto same = [](CCPoint const& a, CCPoint const& b) { return std::fabs(a.x - b.x) < 0.01f && std::fabs(a.y - b.y) < 0.01f; };
        for (auto const& c : cs) {
            for (int j = 0; j <= SEG; j++) {
                float const ang = (c.a0 + 90.f * static_cast<float>(j) / SEG) * 3.14159265f / 180.f;
                CCPoint const q{c.cx + r * std::cos(ang), c.cy + r * std::sin(ang)};
                // a fully round end (a pill) repeats points where the corners meet;
                // GD's polygon fill turns a zero-length edge into a spike (2.2)
                if (!pts.empty() && same(pts.back(), q)) continue;
                pts.push_back(q);
            }
        }
        while (pts.size() > 3 && same(pts.front(), pts.back())) pts.pop_back();
        // CCDrawNode blends premultiplied (GL_ONE, GL_ONE_MINUS_SRC_ALPHA).
        float const al = opacity / 255.f;
        ccColor4F const fill{col.r / 255.f * al, col.g / 255.f * al, col.b / 255.f * al, al};
        dn->drawPolygon(pts.data(), static_cast<unsigned int>(pts.size()), fill, 0.f, ccColor4F{0.f, 0.f, 0.f, 0.f});
        dn->setContentSize({w, h});
    }
    static CCDrawNode* panel(float w, float h, ccColor3B col, GLubyte opacity, float radius = -1.f) {
        auto* dn = CCDrawNode::create();
        dn->setAnchorPoint({0.f, 0.f});
        drawRound(dn, w, h, col, opacity, radius);
        return dn;
    }

    // A card on a page: rounded panel + small caps title, optional hint on the right.
    // (2.2) Its outline glows: a violet light runs in when the tab opens,
    // settles to a soft glow, and brightens while the mouse is over the card.
    struct CardFx {
        CCNode* edge = nullptr;
        CCNode* glow = nullptr;
        CCRect  rect;
        bool    lit = false;
        ccColor3B rest = C_CARD_REST;      // its outline's colour at rest
    };
    std::vector<CardFx> m_cardFx;
    int m_cardHover = -1;
    static constexpr GLubyte GLOW_REST = 42, GLOW_HOVER = 105, GLOW_PEAK = 170;
    GLubyte glowOp(GLubyte v) const { return static_cast<GLubyte>(std::clamp(static_cast<float>(v) * m_alpha, 0.f, 255.f)); }
    static void tint(CCNode* n, ccColor3B c, float t) {
        if (!n) return;
        n->stopActionByTag(77);
        auto* a = CCTintTo::create(t, c.r, c.g, c.b);
        a->setTag(77);
        n->runAction(a);
    }
    static void fadeNode(CCNode* n, GLubyte to, float t, float delay = 0.f) {
        if (!n) return;
        n->stopActionByTag(78);
        CCAction* a = delay > 0.f ? static_cast<CCAction*>(CCSequence::create(CCDelayTime::create(delay), CCFadeTo::create(t, to), nullptr))
                                  : static_cast<CCAction*>(CCFadeTo::create(t, to));
        a->setTag(78);
        n->runAction(a);
    }
    int card(float x, float y, float w, float h, char const* titleText, std::string const& hint = {}) {
        // (2.2) smooth corners (textures - GD's drawn shapes are jagged)
        GLubyte const eo = static_cast<GLubyte>(std::clamp(200.f * m_alpha, 0.f, 255.f));
        GLubyte const po = static_cast<GLubyte>(std::clamp(235.f * m_alpha, 0.f, 255.f));
        CardFx fx;
        fx.rect = CCRect(x, y, w, h);
        fx.glow = aw::glowBox(x, y, w, h, 6.f, C_GLOW, glowOp(GLOW_REST));
        if (fx.glow) m_cards->addChild(fx.glow);
        CCNode* e = aa::roundBox(w, h, 6.f, C_CARD_REST, eo);
        if (!e) e = panel(w, h, C_CARD_REST, eo, 6.f);
        e->setPosition({x, y});
        m_cards->addChild(e);
        fx.edge = e;
        CCNode* p = aa::roundBox(w - 1.2f, h - 1.2f, 5.4f, C_CARD, po);
        if (!p) p = panel(w - 1.2f, h - 1.2f, C_CARD, po, 5.4f);
        p->setPosition({x + 0.6f, y + 0.6f});
        m_cards->addChild(p);
        if (titleText && *titleText) {
            auto* t = AceLabel::make(titleText, "bigFont.fnt");
            t->setScale(0.28f);
            t->setColor(C_LAV);
            t->setAnchorPoint({0.f, 0.5f});
            t->setPosition({x + 6.f, y + h - 7.f});
            m_settingsLabels->addChild(t);
        }
        if (!hint.empty()) {
            auto* hl = AceLabel::make(hint.c_str(), "chatFont.fnt");
            hl->setScale(0.4f);
            hl->setColor(aw::themed(ccColor3B{255, 255, 255}));
            hl->setAnchorPoint({1.f, 0.5f});
            hl->setPosition({x + w - 6.f, y + h - 7.f});
            m_settingsLabels->addChild(hl);
        }
        m_cardFx.push_back(fx);
        return static_cast<int>(m_cardFx.size()) - 1;
    }
    // The tab just opened: the outlines light up one after another, then settle.
    void cardIntro() {
        float delay = 0.f;
        for (auto& fx : m_cardFx) {
            if (fx.edge) {
                if (auto* r = typeinfo_cast<CCRGBAProtocol*>(fx.edge)) r->setColor(fx.rest);
                fx.edge->stopActionByTag(77);
                auto* a = CCSequence::create(CCDelayTime::create(delay), CCTintTo::create(0.12f, C_CARD_LIT.r, C_CARD_LIT.g, C_CARD_LIT.b),
                                             CCTintTo::create(0.65f, fx.rest.r, fx.rest.g, fx.rest.b), nullptr);
                a->setTag(77);
                fx.edge->runAction(a);
            }
            if (fx.glow) {
                if (auto* r = typeinfo_cast<CCRGBAProtocol*>(fx.glow)) r->setOpacity(0);
                fx.glow->stopActionByTag(78);
                auto* a = CCSequence::create(CCDelayTime::create(delay), CCFadeTo::create(0.14f, glowOp(GLOW_PEAK)),
                                             CCFadeTo::create(0.7f, glowOp(GLOW_REST)), nullptr);
                a->setTag(78);
                fx.glow->runAction(a);
            }
            fx.lit = false;
            delay += 0.05f;
        }
        m_cardHover = -1;
    }
    // the card under the mouse brightens
    void hoverCards(CCPoint mouseWorld, bool allowed) {
        int hit = -1;
        if (allowed && m_cards && m_cards->isVisible()) {
            CCPoint const p = m_cards->convertToNodeSpace(mouseWorld);
            for (size_t i = 0; i < m_cardFx.size(); i++) {
                if (m_cardFx[i].rect.containsPoint(p)) { hit = static_cast<int>(i); break; }
            }
        }
        if (hit == m_cardHover) return;
        if (m_cardHover >= 0 && m_cardHover < static_cast<int>(m_cardFx.size())) {
            auto& o = m_cardFx[static_cast<size_t>(m_cardHover)];
            tint(o.edge, o.rest, 0.3f);
            fadeNode(o.glow, glowOp(GLOW_REST), 0.3f);
        }
        m_cardHover = hit;
        if (hit >= 0) {
            auto& n = m_cardFx[static_cast<size_t>(hit)];
            tint(n.edge, C_CARD_HOV, 0.14f);
            fadeNode(n.glow, glowOp(GLOW_HOVER), 0.14f);
        }
    }

    CCMenu* makeMenu(CCNode* parent) {
        auto* m = CCMenu::create();
        m->ignoreAnchorPointForPosition(false);
        m->setAnchorPoint({0.f, 0.f});
        m->setPosition({0.f, 0.f});
        m->setContentSize({parent == m_page ? W : FULL_W, H});
        m->setTouchPriority(PRIO_MENUS);
        parent->addChild(m, 4);
        return m;
    }

    // ---- position ------------------------------------------------------------
    void restorePosition() {
        auto const win = CCDirector::sharedDirector()->getWinSize();
        float const sw = FULL_W * getScale(), sh = H * getScale();
        auto* mod = Mod::get();
        float const x = static_cast<float>(mod->getSavedValue<double>("window-x", win.width - sw - 6.0));
        float const y = static_cast<float>(mod->getSavedValue<double>("window-y", win.height - sh - 6.0));
        setPosition(clampPos({x, y}));
    }

    CCPoint clampPos(CCPoint p) {
        auto const win = CCDirector::sharedDirector()->getWinSize();
        float const sw = FULL_W * getScale(), sh = H * getScale();
        p.x = std::clamp(p.x, 0.f, std::max(0.f, win.width - sw));
        p.y = std::clamp(p.y, 0.f, std::max(0.f, win.height - sh));
        return p;
    }

    void savePosition() {
        auto* mod = Mod::get();
        mod->setSavedValue<double>("window-x", getPositionX());
        mod->setSavedValue<double>("window-y", getPositionY());
    }

    // ---- buttons: a rounded box, the WHOLE box is clickable ---------------------
    // Text (2.2): headings (title, page name, card titles) stay in GD's bold
    // font; labels and buttons use the clean one. Sizes are given in bigFont
    // terms - chatFont is about half as big, so it's drawn at 2x.
    static constexpr float BODY_K = 1.f; // (sizes are bigFont sizes; AceLabel converts)
    static CCLabelBMFont* bodyLabel(char const* text, float bigScale, ccColor3B col = {255, 255, 255}) {
        auto* l = AceLabel::make(text, "bigFont.fnt");
        l->setScale(bigScale);
        l->setColor(col);
        return l;
    }
    static float textWidth(char const* text, float scale) {
        auto* l = AceLabel::make(text, "bigFont.fnt");
        l->setScale(scale);
        return l->getContentSize().width * l->getScale();
    }
    // Geode's limitNodeWidth, aware of AceLabel's size conversion (shadows it in the window).
    static void limitNodeWidth(CCNode* n, float width, float def, float minScale) {
        if (!n) return;
        float k = 1.f;
        if (auto* l = typeinfo_cast<AceLabel*>(n)) k = l->m_k;
        float const cw = n->getContentSize().width * k;
        float sc = def;
        if (cw > 0.f && def * cw > width) sc = width / cw;
        if (minScale != 0.f && sc < minScale) sc = minScale;
        n->setScale(sc);
    }

    CCMenuItemSpriteExtra* boxButton(CCMenu* menu, std::string const& text, CCPoint center, float w, float h,
                                     ccColor3B fill, ccColor3B textCol, float textScale, std::function<void()> fn,
                                     GLubyte opacity = 235) {
        auto* node = CCNode::create();
        node->setContentSize({w, h});
        // (2.2) smooth rounded box with a gentle shade (lighter top edge)
        float const br = std::min(4.5f, h * 0.38f);
        CCNode* bg = aa::roundBox(w, h, br, fill, opacity, true);
        node->addChild(bg ? bg : panel(w, h, fill, opacity));
        // a soft violet glow that fades in while the mouse is over it (see updateHover)
        if (CCNode* hov = aa::roundBox(w, h, br, C_HOVER, 0)) {
            hov->setTag(HOVER_TAG);
            node->addChild(hov);
        }
        float const ts = textScale * 1.12f;
        auto* l = bodyLabel(text.c_str(), ts, textCol);
        limitNodeWidth(l, w - 8.f, ts, ts * 0.5f);
        l->setPosition({w / 2.f, h / 2.f - 0.4f});
        node->addChild(l);
        auto* item = CCMenuItemExt::createSpriteExtra(node, [fn = std::move(fn)](CCMenuItemSpriteExtra*) { fn(); });
        item->setSizeMult(1.06f);
        item->setPosition(center);
        menu->addChild(item);
        return item;
    }

    // (2.3) Extreme's look on another button: a glowing edge in `glow` and a light
    // sweeping across it (no embers).
    void shine(CCMenuItemSpriteExtra* item, float w, float h, ccColor3B glow, bool on = true) {
        if (!item) return;
        if (CCNode* img = item->getNormalImage()) {
            if (auto* fx = aw::ExtremeFx::create(w, h, std::min(4.5f, h * 0.38f), on, glow, false)) img->addChild(fx, 1);
        }
    }

    // A box sized to its text.
    CCMenuItemSpriteExtra* boxButtonFit(CCMenu* menu, char const* text, CCPoint center, ccColor3B textCol,
                                        std::function<void()> fn, float textScale = 0.24f, float h = 12.f,
                                        ccColor3B fill = C_BTN) {
        float const w = textWidth(text, textScale * 1.12f) + 16.f;
        return boxButton(menu, text, center, w, h, fill, textCol, textScale, std::move(fn));
    }

    // ---- tabs (2.2): a list in the sidebar, the highlight slides to the selected one ----
    static constexpr float SIDE_ITEM_H = 16.f;
    static constexpr float SIDE_ITEM_GAP = 3.f;
    static inline float s_tabTop = H - 49.f; // (2.2) the first tab's middle: set from where the line under ACEWARE is
    static float sideItemY(int i) { return s_tabTop - static_cast<float>(i) * (SIDE_ITEM_H + SIDE_ITEM_GAP); }
    static char const* tabName(int i) {
        static char const* names[TAB_COUNT] = {"Log", "Stats", "Stage", "Modes", "Assists", "Dev"};
        return names[std::clamp(i, 0, TAB_COUNT - 1)];
    }

    void buildTabs() {
        m_tabMenu->removeAllChildren();
        // the version, plain: no dev switch, no glow
        {
            float const bh = 10.f, by = m_verY, bw = m_badgeW + 6.f;
            auto* node = CCNode::create();
            node->setContentSize({bw, bh});
            std::string const badge = titleBadge();
            auto* v2 = AceLabel::make(badge.c_str(), "bigFont.fnt");
            v2->setScale(0.28f);
            v2->setColor(C_MUTED);
            v2->setPosition({bw / 2.f, bh / 2.f - 0.3f});
            node->addChild(v2);
            m_badgeLab = v2; // breathes in update()
            node->setPosition({m_verX + m_badgeW / 2.f, by});
            m_tabMenu->addChild(node);
        }
        float const iw = SIDE_W - 16.f;
        for (int i = 0; i < TAB_COUNT; i++) {
            if (!tabShown(i)) continue;
            bool const sel = i == static_cast<int>(m_tab);
            // the whole row clicks: a dot, then the name, left-aligned
            auto* node = CCNode::create();
            node->setContentSize({iw, SIDE_ITEM_H});
            // (2.2) a soft glow under the mouse, and a smooth dot
            if (CCNode* hov = aa::roundBox(iw, SIDE_ITEM_H, 5.f, C_HOVER, 0)) { hov->setTag(HOVER_TAG); node->addChild(hov); }
            ccColor3B const dc = sel ? aw::themed(ccColor3B{95, 211, 160}) : C_DIM;
            if (auto* dot = aa::circleSprite(4.f, dc)) {
                dot->setPosition({8.f, SIDE_ITEM_H / 2.f - 0.3f});
                node->addChild(dot);
            }
            auto* l = bodyLabel(tabName(i), 0.34f, sel ? aw::themed(ccColor3B{255, 255, 255}) : C_MUTED);
            l->setAnchorPoint({0.f, 0.5f});
            l->setPosition({15.f, SIDE_ITEM_H / 2.f});
            node->addChild(l);
            auto* item = CCMenuItemExt::createSpriteExtra(node, [this, i](CCMenuItemSpriteExtra*) { showTab(static_cast<Tab>(i)); });
            item->setSizeMult(1.03f);
            item->setPosition({8.f + iw / 2.f, sideItemY(tabSlot(i))});
            m_tabMenu->addChild(item);
        }
        if (m_tabHi) {
            CCPoint const to{8.f, sideItemY(tabSlot(static_cast<int>(m_tab))) - SIDE_ITEM_H / 2.f};
            m_tabHi->stopAllActions();
            if (!m_tabHiPlaced) {
                m_tabHi->setPosition(to);
                m_tabHiPlaced = true;
            } else {
                m_tabHi->runAction(CCEaseOut::create(CCMoveTo::create(0.12f, to), 2.5f));
            }
        }
        // (1.4.5) a small, extremely ill-advised button, always in the sidebar
        if (auto* item = boxButton(m_tabMenu, "DO NOT PRESS!!!", {8.f + iw / 2.f, 44.f}, iw, 13.f,
                                   ccColor3B{134, 26, 30}, aw::themed(ccColor3B{255, 230, 230}), 0.2f, [] {
                                       Notification::create("meow", NotificationIcon::Error, 2.2f)->show();
                                   })) {
            item->stopAllActions();
            item->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseInOut::create(CCScaleTo::create(0.55f, 1.08f), 1.7f),
                CCEaseInOut::create(CCScaleTo::create(0.55f, 1.f), 1.7f), nullptr)));
        }
    }

    void showTab(Tab t) {
        bool const changed = t != m_tab || !m_shownOnce;
        m_shownOnce = true;
        m_tab = t;
        m_offset = 0;
        m_forceRefresh = true;
        Mod::get()->setSavedValue<int>("last-tab", static_cast<int>(t));
        buildTabs();
        if (m_pageTitle) m_pageTitle->setString(tabName(static_cast<int>(t)));
        bool const list = isList(t);
        for (auto* l : m_rows) l->setVisible(list);
        for (auto* l : m_rowsR) l->setVisible(list);
        for (auto* l : m_rowsH) l->setVisible(list);
        for (auto* l : m_rowsT) l->setVisible(list);
        if (m_stripes) m_stripes->setVisible(list);
        if (m_scrollTrack) m_scrollTrack->setVisible(false); // updateScrollbar shows them when the list scrolls
        if (m_scrollThumb) m_scrollThumb->setVisible(false);
        m_listMenu->setVisible(list);
        m_settingsMenu->setVisible(!list);
        m_settingsLabels->setVisible(!list);
        m_cards->setVisible(!list);
        m_scanLabel = nullptr; // its label goes with m_settingsLabels
        m_listMenu->removeAllChildren();
        m_footBtns.clear();
        m_settingsMenu->removeAllChildren();
        m_settingsLabels->removeAllChildren();
        m_cards->removeAllChildren();
        m_cardFx.clear();
        m_cardHover = -1;
        m_modeTiles.clear();
        m_modesSummary = nullptr;
        if (list) buildListButtons();
        else if (t == TAB_MODES) buildModes();
        else if (t == TAB_TOOLS) buildTools();
        else buildSettings();
        // a new page slides in a little (only when you switch tabs - not when
        // the same page is rebuilt after a click)
        for (CCNode* n : {static_cast<CCNode*>(m_settingsMenu), static_cast<CCNode*>(m_settingsLabels),
                          static_cast<CCNode*>(m_cards)}) {
            n->stopAllActions();
            n->setPosition({changed ? 7.f : 0.f, 0.f});
            if (changed) n->runAction(CCEaseOut::create(CCMoveTo::create(0.16f, CCPoint{0.f, 0.f}), 2.5f));
        }
        if (changed) {
            if (m_pageTitle) {
                float const base = m_pageTitle->getScale();
                m_pageTitle->stopAllActions();
                m_pageTitle->setScale(base * 1.14f);
                m_pageTitle->runAction(CCEaseBackOut::create(CCScaleTo::create(0.22f, base)));
            }
            // list tabs: the rows rise in, one after another
            if (isList(m_tab)) {
                for (size_t i = 0; i < m_rows.size(); i++) {
                    for (CCLabelBMFont* row : {m_rows[i], m_rowsR[i], m_rowsH[i], m_rowsT[i]}) {
                        if (!row) continue;
                        row->stopAllActions();
                        row->setOpacity(0);
                        row->runAction(CCSequence::create(CCDelayTime::create(0.012f * static_cast<float>(i)),
                                                          CCFadeTo::create(0.14f, 255), nullptr));
                    }
                }
            }
            cardIntro(); // (2.2) the outlines light up
            if (m_accentSweep) {
                m_accentSweep->stopAllActions();
                m_accentSweep->setPosition({6.f, H - TITLE_H + 0.5f});
                m_accentSweep->setOpacity(0);
                m_accentSweep->runAction(CCEaseInOut::create(CCMoveTo::create(0.6f, CCPoint{W - 8.f, H - TITLE_H + 0.5f}), 2.f));
                m_accentSweep->runAction(CCSequence::create(CCFadeTo::create(0.12f, 255), CCDelayTime::create(0.3f),
                                                            CCFadeTo::create(0.18f, 0), nullptr));
            }
        }
        refresh();
    }

    void footBtn(CCMenuItemSpriteExtra* item) {
        if (item) m_footBtns.push_back({item, item->getContentSize().width});
    }
    void buildListButtons() {
        float const x = W - PAD - SCROLL_W / 2.f;
        boxButton(m_listMenu, "^", {x, contentTop() - 5.f}, 10.f, 10.f, C_BTN, C_MUTED, 0.26f,
                  [this] { scrollBy(-visibleRows() + 1); });
        boxButton(m_listMenu, "v", {x, contentBottom() + 5.f}, 10.f, 10.f, C_BTN, C_MUTED, 0.26f,
                  [this] { scrollBy(visibleRows() - 1); });
        if (m_tab == TAB_DEV) {
            footBtn(boxButtonFit(m_listMenu, "Copy All", {W - PAD - 24.f, FOOT_H / 2.f}, aw::themed(ccColor3B{255, 255, 255}), [] {
                bool const ok = geode::utils::clipboard::write(s_sim.devCopyText());
                Notification::create(ok ? "Dev info copied - paste it to Claude" : "Couldn't copy", NotificationIcon::None, 1.5f)->show();
            }, 0.22f, 10.f, C_BTN_SEL));
        }
        if (m_tab == TAB_LOG) {
            // In the footer (laid out by layoutFooter: Console, then Clear).
            // The Log in a command-prompt window instead (2.2, Windows).
            if (LogConsole::available()) {
                footBtn(boxButtonFit(m_listMenu, LogConsole::on() ? "Console: On" : "Console", {W - PAD - 62.f, FOOT_H / 2.f},
                             aw::themed(ccColor3B{255, 255, 255}), [this] {
                    bool const on = !LogConsole::on();
                    if (on) {
                        if (!LogConsole::open()) {
                            Notification::create("Couldn't open a console window", NotificationIcon::None, 1.5f)->show();
                            return;
                        }
                        s_sim.dumpLogToConsole();
                    } else {
                        LogConsole::close();
                    }
                    Mod::get()->setSavedValue<bool>("log-console", on);
                    showTab(TAB_LOG);
                }, 0.22f, 10.f, LogConsole::on() ? C_BTN_SEL : C_BTN));
            }
            footBtn(boxButtonFit(m_listMenu, "Clear", {W - PAD - 15.f, FOOT_H / 2.f}, COL_ORANGE, [this] {
                s_sim.clearHistory();
                m_forceRefresh = true;
                refresh();
            }, 0.22f, 10.f));
        }
    }

    // ---- list content -----------------------------------------------------------
    void scrollBy(int rows) {
        m_scrollFade = 1.4f; // the scrollbar lights up for a moment
        int const maxOff = std::max(0, m_rowCount - visibleRows());
        m_offset = std::clamp(m_offset + rows, 0, maxOff);
        drawRows();
    }

    // Only what changed is rebuilt (1.26.0): text labels are rebuilt glyph by
    // glyph, and doing all of them 4x a second while you play cost frame time.
    std::string m_shownHeader, m_shownFooter;
    Tab m_shownFooterTab = TAB_COUNT;
    unsigned long long m_shownLogVersion = ~0ull;
    float m_statsAge = 0.f;
    bool  m_forceRefresh = true;

    void refresh() {
        bool const force = m_forceRefresh;
        m_forceRefresh = false;
        refreshOnline(force);
        std::string head = s_sim.headerLine();
        if (head.rfind("ACEWARE  ", 0) == 0) head = head.substr(9); // the title already says it
        if (force || head != m_shownHeader) {
            m_shownHeader = head;
            m_header->setString(head.c_str());
            m_header->setColor(s_sim.currentStage() == STAGE_EXTREME ? C_EXTREME_TEXT : aw::themed(ccColor3B{255, 255, 255}));
            m_header->setScale(0.28f);
            limitNodeWidth(m_header, W - 120.f, 0.28f, 0.15f);
            if (m_headerPill) {
                bool const extreme = s_sim.currentStage() == STAGE_EXTREME;
                float const hw = m_header->getContentSize().width * m_header->getScale() + 12.f;
                drawRound(m_headerPill, hw, 13.f, extreme ? C_EXTREME : C_PILL, 240);
                m_header->setAnchorPoint({0.5f, 0.5f});
                m_header->setPosition({W - 21.f - hw / 2.f, H - TITLE_H / 2.f - 0.4f});
                if (extreme) m_header->setColor(aw::themed(ccColor3B{255, 255, 255}));
                // (2.2) Extreme: the same glow, sweep and embers as its button
                if (m_headerFx) { m_headerFx->removeFromParent(); m_headerFx = nullptr; }
                if (extreme) {
                    m_headerFx = aw::ExtremeFx::create(hw, 13.f, 6.5f, true, aw::themed(ccColor3B{95, 211, 160}));
                    if (m_headerFx) {
                        m_headerFx->setPosition({W - 21.f - hw, H - TITLE_H / 2.f - 6.5f});
                        m_page->addChild(m_headerFx, 2);
                    }
                }
            }
        }
        std::string const foot = toggleHint(); // was the status line ("ACEWARE  EXTREME" ...) before 2.1
        if (force || foot != m_shownFooter || m_shownFooterTab != m_tab) {
            m_shownFooterTab = m_tab;
            m_shownFooter = foot;
            std::string up = foot;
            for (auto& ch : up) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            m_footer->setString(up.c_str());
            m_footer->setScale(0.23f);
            // room for "clear" / "console" on the Log tab
            // (2.2) up to the footer's buttons / the online pill
            limitNodeWidth(m_footer, std::max(30.f, m_footTextRight - 6.f - (PAD + 2.f)), 0.23f, 0.12f);
        }
        if (m_tab == TAB_TOOLS && m_scanLabel) {
            std::string const st = s_sim.scanStatus();
            if (st != m_scanShown) {
                m_scanShown = st;
                std::string up = st;
                for (auto& ch : up) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                m_scanLabel->setString(up.c_str());
                m_scanLabel->setScale(0.42f);
                limitNodeWidth(m_scanLabel, W - PAD - 6.f - (PAD + 88.f), 0.42f, 0.25f);
            }
        }
        if (!isList(m_tab)) return;
        if (m_tab == TAB_LOG) {
            if (!force && s_sim.logVersion() == m_shownLogVersion) return; // nothing new
            m_shownLogVersion = s_sim.logVersion();
        } else {
            // Stats: about once a second while a level runs, 4x a second otherwise (Dev: always 4x).
            if (!force && m_tab != TAB_DEV && levelRunning() && m_statsAge < 1.f) return;
            m_statsAge = 0.f;
        }
        Config const c = Config::load();
        m_data = (m_tab == TAB_LOG) ? s_sim.historyRows(c.hudVerbose) : (m_tab == TAB_DEV) ? s_sim.devRows() : s_sim.statsRows();
        if (m_tab == TAB_LOG && LogConsole::on()) {
            m_data.clear();
            m_data.emplace_back("The Log is in the console window now.", COL_WHITE);
            m_data.emplace_back("Press console (bottom right) to bring it back here.", C_MUTED);
        } else if (m_tab == TAB_LOG && m_data.empty()) {
            // Empty log: say WHY it's empty (2.3), what will show up here, and how to hide the window.
            int const hidden = s_sim.historyHidden();
            if (!c.enabled) {
                m_data.emplace_back("Nothing logged: Stage is Off - Aceware isn't buffering.", COL_ORANGE);
                m_data.emplace_back("Pick a stage (Stage tab) and your clicks show up here.", COL_WHITE);
            } else if (hidden > 0) {
                m_data.emplace_back(fmt::format("{} click{} so far - all went in as you made them (nothing to fix).", hidden,
                                                hidden == 1 ? "" : "s"), COL_WHITE);
                m_data.emplace_back("The Log only lists clicks Aceware moved. To see every click: Stage tab > Log: More Detail.", COL_WHITE);
            } else if (levelRunning() && s_sim.clicksSeen() == 0) {
                m_data.emplace_back("Nothing logged yet - no clicks reached Aceware on this level.", COL_WHITE);
                m_data.emplace_back("If you are clicking: this gamemode may be off (Modes tab), or another mod takes the input first.",
                                    C_MUTED);
            } else {
                m_data.emplace_back("Nothing logged yet.", COL_WHITE);
                m_data.emplace_back("Buffered clicks, when they fired, and deaths show up here while you play.", COL_WHITE);
            }
            m_data.emplace_back(fmt::format("Show / hide this window: {}. Drag it by the title bar.", showHowTo()), COL_WHITE);
            m_data.emplace_back("", COL_WHITE);
            m_data.emplace_back("Assists tab: orb helper, rays, hitboxes. F7 turns buffering off.", C_MUTED);
        }
        m_rowCount = static_cast<int>(m_data.size());
        scrollBy(0);
    }

    void drawRows() {
        float const full = W - 2.f * PAD - SCROLL_W - 3.f;
        for (size_t i = 0; i < m_rows.size(); i++) {
            size_t const idx = static_cast<size_t>(m_offset) + i;
            auto* l = m_rows[i];
            auto* r = m_rowsR[i];
            auto* h = m_rowsH[i];
            auto* tl = m_rowsT[i];
            float const rowY = contentTop() - static_cast<float>(i) * LINE_H;
            h->setString("");
            tl->setString("");
            l->setPosition({PAD + 1.f, rowY});
            h->setPosition({PAD + 1.f, rowY - LINE_H / 2.f});
            if (idx < m_data.size()) {
                std::string const& text = m_data[idx].first;
                size_t const tab = text.find('\t');
                if (!text.empty() && text[0] == '\x02') {
                    // log line: time | TAG | text
                    size_t const a = text.find('\x1f');
                    size_t const b = (a == std::string::npos) ? std::string::npos : text.find('\x1f', a + 1);
                    if (b != std::string::npos) {
                        std::string const tag = text.substr(a + 1, b - a - 1);
                        tl->setString(text.substr(1, a - 1).c_str());
                        tl->setScale(0.42f);
                        h->setString(tag.c_str());
                        h->setColor(logTagColor(tag));
                        h->setScale(0.2f);
                        h->setPosition({PAD + LOG_TIME_W + 5.f, rowY - LINE_H / 2.f});
                        l->setString(text.substr(b + 1).c_str());
                        l->setColor(m_data[idx].second);
                        l->setScale(0.5f);
                        l->setPosition({PAD + LOG_TEXT_X, rowY});
                        limitNodeWidth(l, full - LOG_TEXT_X + 1.f, 0.5f, 0.25f);
                    } else {
                        l->setString(text.substr(1).c_str());
                    }
                    r->setString("");
                } else if (!text.empty() && text[0] == '\x01') {
                    // section title: like the card titles (2.2) - lavender, bold, uppercase
                    h->setColor(C_LAV);
                    l->setString("");
                    r->setString("");
                    std::string up = text.substr(1);
                    for (auto& ch : up) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    h->setString(up.c_str());
                    h->setScale(0.32f);
                    limitNodeWidth(h, full, 0.32f, 0.15f);
                } else if (tab == std::string::npos) {
                    // plain row (log lines, section titles, notes)
                    l->setString(text.c_str());
                    l->setColor(m_data[idx].second);
                    l->setScale(0.5f);
                    limitNodeWidth(l, full, 0.5f, 0.25f);
                    r->setString("");
                } else {
                    // label (muted) on the left, value (row colour) on the right
                    r->setString(text.substr(tab + 1).c_str());
                    r->setColor(m_data[idx].second);
                    r->setScale(0.5f);
                    limitNodeWidth(r, full * 0.55f, 0.5f, 0.25f);
                    float const valueW = r->getContentSize().width * r->getScale();
                    l->setString(text.substr(0, tab).c_str());
                    l->setColor(aw::themed(ccColor3B{255, 255, 255})); // (titles are the coloured ones - 2.2)
                    l->setScale(0.5f);
                    limitNodeWidth(l, std::max(40.f, full - valueW - 8.f), 0.5f, 0.25f);
                }
            } else {
                l->setString("");
                r->setString("");
            }
        }
        updateScrollbar();
    }

    // Thumb size = visible share of the list; position = scroll offset.
    void updateScrollbar() {
        if (!m_scrollThumb || !m_scrollTrack) return;
        int const vis = visibleRows();
        bool const scrollable = m_rowCount > vis;
        m_scrollTrack->setVisible(isList(m_tab) && scrollable);
        m_scrollThumb->setVisible(isList(m_tab) && scrollable);
        if (!scrollable) return;
        float const trackH = trackTop() - trackBottom();
        // (track spans between the two arrow boxes)
        float const thumbH = std::max(8.f, trackH * static_cast<float>(vis) / static_cast<float>(m_rowCount));
        int const maxOff = std::max(1, m_rowCount - vis);
        float const t = static_cast<float>(m_offset) / static_cast<float>(maxOff); // 0 = top
        m_scrollThumb->setContentSize({2.f, thumbH});
        m_scrollThumb->setPosition({W - PAD - SCROLL_W / 2.f - 1.f, trackTop() - thumbH - t * (trackH - thumbH)});
    }

    // ---- stage page ------------------------------------------------------------
    // What each stage means, in plain words.
    // Every stage's window as cube / ship / wave frames, in one line.
    static std::string stageFramesLine() {
        std::string out = "frames c/s/w   ";
        for (int i = 0; i < STAGE_COUNT; i++) {
            float cu = STAGES[i].w[W_CUBE], sh = STAGES[i].w[W_SHIP], wa = STAGES[i].w[W_WAVE];
            if (i == STAGE_EXTREME) { cu = 20.f; sh = 2.f; wa = 12.f; }
            if (i == STAGE_CUSTOM) { CustomFrames const f = CustomFrames::load(); cu = f.cube; sh = f.ship; wa = f.wave; }
            char const* tag = i == 0 ? "off" : i == STAGE_EXTREME ? "ex" : i == STAGE_CUSTOM ? "cu"
                            : (i == 1 ? "1" : i == 2 ? "2" : i == 3 ? "3" : "4");
            out += fmt::format("{} {}/{}/{}   ", tag, cu, sh, wa);
        }
        return out;
    }

    static std::string stageBlurb(int st) {
        switch (st) {
            case 0:  return "Raw input. The buffer does nothing.";
            case 1:  return "Saves clicks up to 1 frame early (1f @60fps).";
            case 2:  return "Saves clicks up to 2 frames early.";
            case 3:  return "3 frames early (2 for ship).";
            case 4:  return "5 frames early (4 for wave, 2 for ship).";
            case 5:  return "20 frames early (12 for wave, 2 for ship).";
            case 6: {
                CustomFrames const f = CustomFrames::load();
                return fmt::format("Your frames: cube {:g}, ship {:g}, wave {:g}.", f.cube, f.ship, f.wave);
            }
            default: return "";
        }
    }

    // Custom frames (saved on every +/- since 2.2).
    CustomFrames m_customEdit = CustomFrames::load();

    // the switch's size (the rows draw one at their right end)
    static constexpr float SW_W = 21.f, SW_H = 11.f;

    // (2.2) A settings row like the reference: a rounded pill with the
    // name (and a short grey description under it), the switch at its right
    // end. The WHOLE row is the button: click anywhere to switch it, it glows
    // violet under the mouse, and an "on" row has a faint violet tint.
    static inline const ccColor3B K_ROW = {32, 29, 43};
    static inline ccColor3B C_ROW = K_ROW;
    static inline const ccColor3B K_ROW_ON = {28, 52, 44};
    static inline ccColor3B C_ROW_ON = K_ROW_ON;
    void rowToggle(float x, float y, float w, char const* title, char const* desc, char const* key, bool def) {
        float const h = desc ? 17.f : 13.f;
        bool const on = Mod::get()->getSavedValue<bool>(key, def);
        std::string const k = key;
        auto* node = CCNode::create();
        node->setContentSize({w, h});
        CCNode* bg = aa::roundBox(w, h, 4.f, on ? C_ROW_ON : C_ROW, 255);
        bool const smoothBg = bg != nullptr; // a CCScale9Sprite, or the old drawn box
        if (!bg) bg = panel(w, h, on ? C_ROW_ON : C_ROW, 255, 4.f);
        node->addChild(bg);
        if (CCNode* hov = aa::roundBox(w, h, 4.f, C_HOVER, 0)) {
            hov->setTag(HOVER_TAG);
            node->addChild(hov);
        }
        float const textW = w - SW_W - 12.f;
        auto* l = bodyLabel(title, 0.3f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setPosition({5.f, desc ? h / 2.f + 3.2f : h / 2.f});
        limitNodeWidth(l, textW, 0.3f, 0.17f);
        node->addChild(l);
        if (desc) {
            auto* d = bodyLabel(desc, 0.19f, C_DIM);
            d->setAnchorPoint({0.f, 0.5f});
            d->setPosition({5.f, h / 2.f - 3.6f});
            limitNodeWidth(d, textW, 0.19f, 0.11f);
            node->addChild(d);
        }
        // the switch (drawn, not its own button: the row is the button)
        CCNode* track = aa::roundBox(SW_W, SW_H, SW_H / 2.f, on ? C_BTN_SEL : C_SW_OFF, 255, true);
        bool const smooth = track != nullptr;
        if (!track) track = panel(SW_W, SW_H, on ? C_BTN_SEL : C_SW_OFF, 255, SW_H / 2.f);
        track->setPosition({w - SW_W - 4.f, (h - SW_H) / 2.f});
        node->addChild(track);
        float const r = SW_H / 2.f - 1.5f;
        auto* knob = CCNode::create();
        if (auto* sh = aa::circleSprite(2.f * r + 1.2f, aw::themed(ccColor3B{0, 0, 0}), 70)) { sh->setPosition({0.f, -0.6f}); knob->addChild(sh); }
        if (auto* kk = aa::circleSprite(2.f * r, aw::themed(ccColor3B{246, 244, 252}))) knob->addChild(kk);
        float const offX = w - SW_W - 4.f + SW_H / 2.f, onX = w - 4.f - SW_H / 2.f;
        knob->setPosition({on ? onX : offX, h / 2.f});
        node->addChild(knob);
        auto* item = CCMenuItemExt::createSpriteExtra(node, [k, def, w, h, bg, smoothBg, track, smooth, knob, offX, onX](CCMenuItemSpriteExtra*) {
            auto* mod = Mod::get();
            bool const now = !mod->getSavedValue<bool>(k, def);
            mod->setSavedValue<bool>(k, now);
            s_sim.settingsChanged();
            if (smooth) static_cast<CCNodeRGBA*>(track)->setColor(now ? C_BTN_SEL : C_SW_OFF);
            else drawRound(static_cast<CCDrawNode*>(track), SW_W, SW_H, now ? C_BTN_SEL : C_SW_OFF, 255, SW_H / 2.f);
            if (smoothBg) static_cast<CCNodeRGBA*>(bg)->setColor(now ? C_ROW_ON : C_ROW);
            else drawRound(static_cast<CCDrawNode*>(bg), w, h, now ? C_ROW_ON : C_ROW, 255, 4.f);
            knob->stopAllActions();
            knob->runAction(CCEaseOut::create(CCMoveTo::create(0.13f, CCPoint{now ? onX : offX, h / 2.f}), 2.5f));
            // squash as it flips, then spring back
            knob->runAction(CCSequence::create(CCScaleTo::create(0.05f, 1.22f, 1.22f),
                                               CCEaseBackOut::create(CCScaleTo::create(0.14f, 1.f, 1.f)), nullptr));
        });
        item->setSizeMult(1.015f);
        item->setPosition({x + w / 2.f, y});
        m_settingsMenu->addChild(item);
    }

    // A value row: a pill with the name on the left and [-] value [+] on the right.
    void stepperRow(float x, float y, float w, char const* label, std::string const& value, std::function<void()> minus,
                    std::function<void()> plus) {
        float const h = 13.f;
        if (CCNode* bg = aa::roundBox(w, h, 4.f, C_ROW, 255)) {
            bg->setPosition({x, y - h / 2.f});
            m_cards->addChild(bg);
        }
        auto* l = bodyLabel(label, 0.28f);
        l->setAnchorPoint({0.f, 0.5f});
        l->setPosition({x + 5.f, y});
        limitNodeWidth(l, w - 56.f, 0.28f, 0.16f);
        m_settingsLabels->addChild(l);
        boxButton(m_settingsMenu, "-", {x + w - 44.f, y}, 11.f, 10.f, C_BTN, C_LAV, 0.28f, std::move(minus));
        auto* num = bodyLabel(value.c_str(), 0.29f);
        num->setPosition({x + w - 27.5f, y});
        limitNodeWidth(num, 20.f, 0.29f, 0.18f);
        m_settingsLabels->addChild(num);
        boxButton(m_settingsMenu, "+", {x + w - 11.f, y}, 11.f, 10.f, C_BTN, C_LAV, 0.28f, std::move(plus));
    }

    // A small grey caption inside a card.
    void caption(float x, float y, char const* text, float maxW) {
        auto* c = bodyLabel(text, 0.21f, C_DIM);
        c->setAnchorPoint({0.f, 0.5f});
        c->setPosition({x, y});
        limitNodeWidth(c, maxW, 0.21f, 0.12f);
        m_settingsLabels->addChild(c);
    }



    //  Stage tab (2.2), like the reference:
    //    Stage (top, full width): the stage buttons, its name and what it does
    //    Options (left): rows       |  Custom (right): frames + Custom Min / Max
    //    Quick (left): tiles        |
    void buildSettings() {
        int const cur = s_sim.currentStage();
        ccColor3B const white{255, 255, 255};
        float const lx = PAD, lw = 154.f, rx = PAD + lw + 6.f, rw = W - PAD - rx;

        // -- Stage (146..192)
        std::string cycleHint;
#ifndef GEODE_IS_MOBILE
        if (std::string const k = cycleKeyName(); !k.empty()) cycleHint = k + " cycles stages";
#endif
        card(lx, 146.f, W - 2.f * PAD, 46.f, "Stage", cycleHint);
        char const* labels[] = {"Off", "1", "2", "3", "4", "Extreme (buggy)", "Custom"};
        constexpr float BTN_H = 13.f, BTN_GAP = 4.f, BTN_TEXT = 0.27f;
        std::vector<float> widths;
        float total = 0.f;
        for (int i = 0; i < STAGE_COUNT; i++) {
            widths.push_back(std::max(20.f, textWidth(labels[i], BTN_TEXT) + 14.f));
            total += widths.back();
        }
        total += BTN_GAP * static_cast<float>(STAGE_COUNT - 1);
        float x = std::max(lx + 4.f, (W - total) / 2.f);
        for (int i = 0; i < STAGE_COUNT; i++) {
            bool const sel = (i == cur);
            bool const extreme = i == STAGE_EXTREME;
            // (2.2) Extreme, the top stage, looks it: hot pink, a glowing
            // edge, a light sweeping across, embers rising while it's on
            ccColor3B const fill = sel ? (extreme ? C_EXTREME : C_BTN_SEL) : C_BTN;
            ccColor3B const text = sel ? white : (extreme ? C_EXTREME_TEXT : C_MUTED);
            auto* item = boxButton(m_settingsMenu, labels[i], {x + widths[i] / 2.f, 171.f}, widths[i], BTN_H, fill, text, BTN_TEXT,
                                   [this, i] {
                                       s_sim.setStage(i);
                                       showTab(TAB_SETTINGS);
                                   }, sel ? 255 : 230);
            if (extreme && item) {
                if (CCNode* img = item->getNormalImage()) {
                    if (auto* fx = aw::ExtremeFx::create(widths[i], BTN_H, std::min(4.5f, BTN_H * 0.38f), sel,
                                                         aw::themed(ccColor3B{95, 211, 160}))) img->addChild(fx, 1);
                }
            }
            if (i == STAGE_CUSTOM) shine(item, widths[i], BTN_H, C_GLOW, sel); // (2.3)
            if (sel && item) {
                // the chosen stage gives a small pop every time the page opens
                CCNode* img = item->getNormalImage();
                if (img) {
                    float const base = img->getScale();
                    img->stopAllActions();
                    img->setScale(base * 1.12f);
                    img->runAction(CCEaseBackOut::create(CCScaleTo::create(0.24f, base)));
                }
            }
            x += widths[i] + BTN_GAP;
        }
        auto* name = bodyLabel(STAGES[cur].name, 0.3f, cur == STAGE_EXTREME ? C_EXTREME_TEXT : white);
        limitNodeWidth(name, W - 2.f * PAD - 12.f, 0.3f, 0.18f);
        name->setPosition({W / 2.f, 159.5f});
        m_settingsLabels->addChild(name);
        auto* blurb = bodyLabel(stageFramesLine().c_str(), 0.19f, C_DIM);
        limitNodeWidth(blurb, W - 2.f * PAD - 12.f, 0.19f, 0.11f);
        blurb->setPosition({W / 2.f, 152.5f});
        m_settingsLabels->addChild(blurb);

        // -- Options (left, 84..142)
        card(lx, 84.f, lw, 58.f, "Options");
        rowToggle(lx + 4.f, 119.f, lw - 8.f, "Log: More Detail", "Every click's decision in the Log", "log-safe-clicks", false);

        // -- Level (left, 18..80): one button
        card(lx, 18.f, lw, 62.f, "Level");
        if (auto* forget = boxButton(m_settingsMenu, "Forget Level",
                                     {lx + 4.f + (lw - 12.f) / 2.f, 43.f}, lw - 12.f, 34.f, C_BTN, white, 0.24f,
                                     [] {
                                         s_sim.clearHistory();
                                         Notification::create("Aceware: log cleared", NotificationIcon::None, 1.2f)->show();
                                     })) {
            shine(forget, lw - 12.f, 34.f, C_GLOW);
        }

        // -- Custom (right, 18..142): frames, then Custom Min / Max (ticks)
        card(rx, 18.f, rw, 124.f, "Custom", "frames @60fps");
        auto frames = [&](char const* label, float& value, float y) {
            float* v = &value;
            auto apply = [this]() {
                m_customEdit.save();
                s_sim.setStage(STAGE_CUSTOM); // reloads the config with the new frames
                showTab(TAB_SETTINGS);
            };
            stepperRow(rx + 4.f, y, rw - 8.f, label, fmt::format("{:g}", value),
                       [v, apply] { *v = std::max(CUSTOM_MIN_FRAMES, *v - 0.5f); apply(); },
                       [v, apply] { *v = std::min(CUSTOM_MAX_FRAMES, *v + 0.5f); apply(); });
        };
        frames("Cube", m_customEdit.cube, 118.f);
        frames("Ship", m_customEdit.ship, 102.f);
        frames("Wave", m_customEdit.wave, 86.f);
        caption(rx + 6.f, 72.f, "TICKS (4 = 1 frame) - changing one switches to Custom", rw - 12.f);
        auto* mod = Mod::get();
        int const maxV = std::clamp(static_cast<int>(std::lround(mod->getSavedValue<double>("max-ticks", 1.0))), 1, MAX_TICKS_LIMIT);
        int const minV = std::clamp(static_cast<int>(std::lround(mod->getSavedValue<double>("min-ticks", 1.0))), 1, maxV);
        auto setTicks = [this](bool isMax, int delta) {
            auto* m = Mod::get();
            int mx = std::clamp(static_cast<int>(std::lround(m->getSavedValue<double>("max-ticks", 1.0))), 1, MAX_TICKS_LIMIT);
            int mn = std::clamp(static_cast<int>(std::lround(m->getSavedValue<double>("min-ticks", 1.0))), 1, mx);
            if (isMax) mx = std::clamp(mx + delta, 1, MAX_TICKS_LIMIT);
            else mn = std::clamp(mn + delta, 1, MAX_TICKS_LIMIT);
            if (mn > mx) { if (isMax) mn = mx; else mx = mn; } // Min never above Max
            m->setSavedValue<double>("max-ticks", mx);
            m->setSavedValue<double>("min-ticks", mn);
            s_sim.setStage(STAGE_CUSTOM); // (2.2) they only apply to Custom: switch to it
            showTab(TAB_SETTINGS);
        };
        stepperRow(rx + 4.f, 57.f, rw - 8.f, "Custom Min", fmt::format("{}t", minV),
                   [setTicks] { setTicks(false, -1); }, [setTicks] { setTicks(false, +1); });
        stepperRow(rx + 4.f, 41.f, rw - 8.f, "Custom Max", maxV >= MAX_TICKS_LIMIT ? std::string("Off") : fmt::format("{}t", maxV),
                   [setTicks] { setTicks(true, -1); }, [setTicks] { setTicks(true, +1); });
        caption(rx + 6.f, 27.f, "Max all the way up = Off (no cap)", rw - 12.f);
    }

    // ---- Modes tab (2.2) ------------------------------------------------------
    //  A tile per gamemode with YOUR icon for it. Normal / Mini under it, each
    //  on its own; click the icon to switch both. A mode that's off: its
    //  clicks go through raw (no buffering). Changes show in place.
    struct ModeTile {
        std::string id;
        SimplePlayer* icon = nullptr;
        float iconScale = 0.f;
        CCSprite* halo = nullptr;
        CCSprite* dot = nullptr;
        CCLabelBMFont* name = nullptr;
        CCNode* seg[2] = {nullptr, nullptr};
        CCLabelBMFont* segLabel[2] = {nullptr, nullptr};
        int fx = -1;
        bool lit = false;
    };
    std::vector<ModeTile> m_modeTiles;
    CCLabelBMFont* m_modesSummary = nullptr;
    static IconType modeIcon(int i) {
        static IconType const t[] = {IconType::Cube, IconType::Ship, IconType::Ball, IconType::Ufo,
                                     IconType::Wave, IconType::Robot, IconType::Spider, IconType::Swing};
        return t[std::clamp(i, 0, 7)];
    }
    static float modeIconScale(int i) {
        static float const s[] = {0.6f, 0.52f, 0.58f, 0.52f, 0.58f, 0.46f, 0.44f, 0.56f};
        return s[std::clamp(i, 0, 7)];
    }
    void buildModes() {
        m_modeTiles.clear();
        m_modeTiles.resize(MODE_TOGGLE_COUNT);
        constexpr int COLS = 4;
        constexpr float GAP = 6.f, TH = 72.f;
        float const tw = (W - 2.f * PAD - GAP * (COLS - 1)) / COLS;
        float const top = 192.f;
        for (int i = 0; i < MODE_TOGGLE_COUNT; i++) {
            int const col = i % COLS, row = i / COLS;
            float const x = PAD + static_cast<float>(col) * (tw + GAP);
            float const y = top - TH - static_cast<float>(row) * (TH + GAP);
            ModeTile& t = m_modeTiles[static_cast<size_t>(i)];
            t.id = MODE_TOGGLES[i].id;
            t.fx = card(x, y, tw, TH, nullptr);
            // your icon, with a soft violet light behind it when the mode is on;
            // the icon area is a button: both on / both off
            CCPoint const ic{x + tw / 2.f, y + TH - 26.f};
            {
                float const bw = tw - 10.f, bh = 30.f;
                CCPoint const c{bw / 2.f, bh / 2.f};
                auto* node = CCNode::create();
                node->setContentSize({bw, bh});
                // under the mouse: a round violet light behind the icon (no box)
                if (CCSprite* hov = aw::sprite(aw::soft(), 46.f, aw::themed(ccColor3B{95, 211, 160}), 0)) {
                    aw::additive(hov);
                    hov->setPosition(c);
                    hov->setTag(HOVER_TAG);
                    node->addChild(hov, 0);
                }
                t.halo = aw::sprite(aw::soft(), 40.f, aw::themed(ccColor3B{52, 170, 130}), 0);
                if (t.halo) {
                    aw::additive(t.halo);
                    t.halo->setPosition(c);
                    node->addChild(t.halo, 1);
                }
                if (auto* shadow = aw::sprite(aw::soft(), 22.f, aw::themed(ccColor3B{0, 0, 0}), 110)) {
                    shadow->setScaleY(shadow->getScaleX() * 0.22f);
                    shadow->setPosition({c.x, c.y - 11.5f});
                    node->addChild(shadow, 1);
                }
                t.iconScale = modeIconScale(i);
                t.icon = aw::playerIcon(modeIcon(i), t.iconScale);
                if (t.icon) {
                    t.icon->setPosition(c);
                    node->addChild(t.icon, 2);
                }
                auto* item = CCMenuItemExt::createSpriteExtra(node, [this, i](CCMenuItemSpriteExtra*) {
                    auto& tile = m_modeTiles[static_cast<size_t>(i)];
                    bool const any = modeOn(tile.id) || modeOn("mini-" + tile.id);
                    setMode(i, 0, !any);
                    setMode(i, 1, !any);
                    applyTile(i, true);
                });
                item->setSizeMult(1.04f);
                item->setPosition(ic);
                m_settingsMenu->addChild(item);
            }
            // status dot, top right
            t.dot = aa::circleSprite(4.f, aw::themed(ccColor3B{95, 211, 160}));
            if (t.dot) {
                t.dot->setPosition({x + tw - 7.5f, y + TH - 7.5f});
                m_settingsLabels->addChild(t.dot, 1);
            }
            t.name = bodyLabel(MODE_TOGGLES[i].label, 0.31f);
            t.name->setPosition({x + tw / 2.f, y + 28.f});
            m_settingsLabels->addChild(t.name, 1);
            // Normal | Mini: a dark pill, each half its own switch
            float const sw = tw - 12.f, sh = 12.f, sy = y + 11.f;
            if (CCNode* bg = aa::roundBox(sw, sh, sh / 2.f, C_ROW, 255)) {
                bg->setPosition({x + 6.f, sy - sh / 2.f});
                m_settingsLabels->addChild(bg, 0);
            }
            for (int half = 0; half < 2; half++) {
                float const hw = sw / 2.f - 1.4f, hh = sh - 2.4f;
                auto* node = CCNode::create();
                node->setContentSize({hw, hh});
                CCNode* fill = aa::roundBox(hw, hh, hh / 2.f, C_BTN_SEL, 255, true);
                if (fill) node->addChild(fill);
                t.seg[half] = fill;
                if (CCNode* hov = aa::roundBox(hw, hh, hh / 2.f, C_HOVER, 0)) { hov->setTag(HOVER_TAG); node->addChild(hov); }
                auto* l = bodyLabel(half ? "Mini" : "Normal", 0.21f);
                l->setPosition({hw / 2.f, hh / 2.f - 0.2f});
                node->addChild(l);
                t.segLabel[half] = l;
                auto* item = CCMenuItemExt::createSpriteExtra(node, [this, i, half](CCMenuItemSpriteExtra*) {
                    auto& tile = m_modeTiles[static_cast<size_t>(i)];
                    setMode(i, half, !modeOn(half ? "mini-" + tile.id : tile.id));
                    applyTile(i, true);
                });
                item->setSizeMult(1.04f);
                item->setPosition({x + 6.f + 1.2f + hw / 2.f + static_cast<float>(half) * (sw / 2.f + 0.2f), sy});
                m_settingsMenu->addChild(item);
            }
        }
        for (int i = 0; i < MODE_TOGGLE_COUNT; i++) applyTile(i, false);

        // bottom bar: all on / all off, and how many are on
        auto setAll = [this](bool on) {
            for (int i = 0; i < MODE_TOGGLE_COUNT; i++) {
                setMode(i, 0, on);
                setMode(i, 1, on);
                applyTile(i, true);
            }
        };
        float const by = 28.f;
        boxButton(m_settingsMenu, "All On", {PAD + 24.f, by}, 44.f, 13.f, C_BTN, COL_GREEN, 0.24f, [setAll] { setAll(true); });
        boxButton(m_settingsMenu, "All Off", {PAD + 74.f, by}, 44.f, 13.f, C_BTN, COL_ORANGE, 0.24f, [setAll] { setAll(false); });
        m_modesSummary = bodyLabel("", 0.24f, C_MUTED);
        m_modesSummary->setAnchorPoint({1.f, 0.5f});
        m_modesSummary->setPosition({W - PAD - 4.f, by});
        m_settingsLabels->addChild(m_modesSummary);
        updateModesSummary();
    }
    void setMode(int i, int mini, bool on) {
        std::string const id = mini ? fmt::format("mini-{}", MODE_TOGGLES[i].id) : std::string(MODE_TOGGLES[i].id);
        Mod::get()->setSavedValue<bool>("mode-on-" + id, on);
        s_sim.settingsChanged();
    }
    void updateModesSummary() {
        if (!m_modesSummary) return;
        int on = 0;
        for (auto const& m : MODE_TOGGLES) on += (modeOn(m.id) ? 1 : 0) + (modeOn(fmt::format("mini-{}", m.id)) ? 1 : 0);
        m_modesSummary->setString(fmt::format("{} / {} buffered  -  off = clicks go through raw", on, 2 * MODE_TOGGLE_COUNT).c_str());
        m_modesSummary->setScale(0.24f);
        limitNodeWidth(m_modesSummary, W - 2.f * PAD - 110.f, 0.24f, 0.14f);
    }
    // Show a tile's state: lit icon + violet halo when on, grey when off.
    void applyTile(int i, bool animate) {
        if (i < 0 || i >= static_cast<int>(m_modeTiles.size())) return;
        ModeTile& t = m_modeTiles[static_cast<size_t>(i)];
        bool const n = modeOn(t.id), m = modeOn("mini-" + t.id);
        int const count = (n ? 1 : 0) + (m ? 1 : 0);
        for (int half = 0; half < 2; half++) {
            bool const on = half ? m : n;
            if (auto* r = typeinfo_cast<CCRGBAProtocol*>(t.seg[half])) {
                r->setColor(C_BTN_SEL);
                if (!animate) r->setOpacity(on ? 255 : 0);
            }
            if (animate && t.seg[half]) fadeNode(t.seg[half], on ? 255 : 0, 0.14f);
            if (t.segLabel[half]) t.segLabel[half]->setColor(on ? aw::themed(ccColor3B{255, 255, 255}) : C_DIM);
        }
        if (t.name) t.name->setColor(count ? aw::themed(ccColor3B{255, 255, 255}) : C_DIM);
        if (t.dot) t.dot->setColor(count == 2 ? aw::themed(ccColor3B{96, 240, 136}) : count == 1 ? aw::themed(ccColor3B{255, 196, 84}) : aw::themed(ccColor3B{116, 112, 134}));
        GLubyte const haloOp = static_cast<GLubyte>(count == 2 ? 120 : count == 1 ? 70 : 0);
        if (t.halo) {
            if (animate) fadeNode(t.halo, haloOp, 0.25f);
            else t.halo->setOpacity(haloOp);
        }
        if (t.icon) {
            auto* gm = GameManager::sharedState();
            bool const lit = count > 0;
            bool const wasLit = t.lit;
            t.lit = lit;
            if (lit) {
                t.icon->setColor(aw::playerColor());
                t.icon->setSecondColor(aw::playerColor(true));
                if (gm && gm->getPlayerGlow()) t.icon->setGlowOutline(gm->colorForIdx(gm->getPlayerGlowColor()));
                t.icon->setOpacity(255);
            } else {
                t.icon->setColor(aw::themed(ccColor3B{96, 92, 112}));
                t.icon->setSecondColor(aw::themed(ccColor3B{70, 66, 86}));
                if (gm && gm->getPlayerGlow()) t.icon->disableGlowOutline();
                t.icon->setOpacity(150);
            }
            if (animate && lit && !wasLit) {
                t.icon->stopAllActions();
                t.icon->setScale(t.iconScale);
                t.icon->runAction(CCSequence::create(CCScaleTo::create(0.08f, t.iconScale * 1.18f),
                                                     CCEaseBackOut::create(CCScaleTo::create(0.22f, t.iconScale)), nullptr));
            }
        }
        // the tile's outline: violet when the mode is on
        if (t.fx >= 0 && t.fx < static_cast<int>(m_cardFx.size())) {
            CardFx& fx = m_cardFx[static_cast<size_t>(t.fx)];
            fx.rest = count ? C_CARD_REST : C_CARD_OFF;
            if (m_cardHover == t.fx) {
                // (under the mouse: stays bright until it leaves)
            } else if (animate) {
                tint(fx.edge, fx.rest, 0.25f);
            } else if (auto* r = typeinfo_cast<CCRGBAProtocol*>(fx.edge)) {
                r->setColor(fx.rest);
            }
        }
        updateModesSummary();
    }

    // ---- Tools tab (2.1) ----------------------------------------------------------------
    //  ASSISTS     Orb helper, Click guide (toggles + one line each)
    //  YOUR STATS  this level / all levels, saved between sessions
    //  REPORT      export everything to a file (and the clipboard) to send
    void toolsLine(float x, float y, std::string const& text, ccColor3B col, float maxW, float scale = 0.42f) {
        auto* l = AceLabel::make(text.c_str(), "chatFont.fnt");
        l->setScale(scale);
        l->setColor(col);
        l->setAnchorPoint({0.f, 0.5f});
        limitNodeWidth(l, maxW, scale, scale * 0.6f);
        l->setPosition({x, y});
        m_settingsLabels->addChild(l);
    }
    CCLabelBMFont* m_scanLabel = nullptr; // Tools tab: scan status (refreshed while shown)
    std::string    m_scanShown;

    //  Tools tab (2.2): Assists rows + Your Stats on the left, Level Scan on the right.
    void buildTools() {
        ccColor3B const white{255, 255, 255};
        float const lx = PAD, lw = 154.f, rx = PAD + lw + 6.f, rw = W - PAD - rx;

        // -- Assists (left, 72..192)
        card(lx, 72.f, lw, 120.f, "Assists");
        rowToggle(lx + 4.f, 170.f, lw - 8.f, "Orb Helper", "An early click waits for the orb, goes in on touch",
                  "orb-helper", true);
        rowToggle(lx + 4.f, 151.f, lw - 8.f, "Raw Wave Spam", "Above 7 clicks/s in wave: goes in exactly as you click",
                  "raw-wave-spam", false);
        rowToggle(lx + 4.f, 132.f, lw - 8.f, "Hitboxes", "Your box (spikes) and inner box (blocks)",
                  "show-hitboxes", false);
        rowToggle(lx + 4.f, 113.f, lw - 8.f, "Show Rays", "Your hitbox, press / release paths, the pathfinder",
                  "show-rays", false);
        rowToggle(lx + 4.f, 94.f, lw - 8.f, "Session Memory", "Repeats the clicks that worked, this level only",
                  "session-memory", true);


        // -- Your Stats (left, 18..66)
        card(lx, 18.f, lw, 48.f, "Your Stats");
        auto const lv = s_sim.levelStats();
        auto const life = s_sim.lifeStats();
        std::string const name = s_sim.levelName();
        toolsLine(lx + 6.f, 50.f,
                  name.empty() ? std::string("This level: play one to start counting")
                               : fmt::format("{}: {} attempts, {} deaths, best {:.0f}%, {} buffered", name, lv.attempts,
                                             lv.deaths, lv.best, lv.buffered),
                  white, lw - 12.f, 0.36f);
        toolsLine(lx + 6.f, 41.f,
                  fmt::format("All levels: {} attempts, {} deaths, {} completions, {} buffered", life.attempts,
                              life.deaths, life.completions, life.buffered),
                  C_MUTED, lw - 12.f, 0.36f);
        shine(boxButton(m_settingsMenu, "Export Report", {lx + lw / 2.f, 28.f}, lw - 12.f, 14.f, C_BTN_SEL, white, 0.27f, [] {
            std::string const text = s_sim.buildReport();
            auto const file = s_sim.saveReport(text);
            bool const copied = geode::utils::clipboard::write(text);
#ifdef GEODE_IS_DESKTOP
            (void)geode::utils::file::openFolder(file.parent_path());
#endif
            Notification::create(fmt::format("Report saved{} - {}", copied ? " and copied" : "", file.filename().string()),
                                 NotificationIcon::None, 2.5f)->show();
        }), lw - 12.f, 14.f, C_GLOW);

        // -- Keys (right, 100..192)
        card(rx, 100.f, rw, 92.f, "Keys");
        toolsLine(rx + 6.f, 176.f, "Right Shift  -  this window", white, rw - 12.f, 0.34f);
        toolsLine(rx + 6.f, 164.f, "F6  -  cycle stage", C_MUTED, rw - 12.f, 0.34f);
        toolsLine(rx + 6.f, 152.f, "F7  -  buffer on / off", C_MUTED, rw - 12.f, 0.34f);
        toolsLine(rx + 6.f, 140.f, "Drag corner  -  resize", C_MUTED, rw - 12.f, 0.34f);
        toolsLine(rx + 6.f, 128.f, "Drag title bar  -  move", C_MUTED, rw - 12.f, 0.34f);
        caption(rx + 6.f, 112.f, "Assists above apply the moment you flip them", rw - 12.f);

        // -- Quick (right, 18..94)
        card(rx, 18.f, rw, 76.f, "Quick");
        shine(boxButton(m_settingsMenu, "Copy Report", {rx + rw / 2.f, 62.f}, rw - 12.f, 18.f, C_BTN_SEL, white, 0.28f, [] {
            std::string const text = s_sim.buildReport();
            bool const ok = geode::utils::clipboard::write(text);
            Notification::create(ok ? "Aceware: report copied" : "Aceware: copy failed", NotificationIcon::None, 1.6f)->show();
        }), rw - 12.f, 18.f, C_GLOW);
        boxButton(m_settingsMenu, "Hide Window", {rx + rw / 2.f, 38.f}, rw - 12.f, 18.f, C_BTN, white, 0.28f, [] {
            BufferWindow::toggle();
        });
        caption(rx + 6.f, 24.f, "F7 stops buffering without losing your stage", rw - 12.f);
    }


    // ---- ACEWARE title (2.2): a neon sign. Each letter is a bright tube
    // with a tight glow and a wide bloom round it (the letter itself, blurred -
    // real glow, not a bigger copy). Idle it hums softly; under the mouse a
    // letter flickers on like a neon tube and burns bright. The O is a ring
    // galaxy that slowly turns. When the window opens the sign flickers on.
    struct WordLetter {
        Ref<CCLabelBMFont> letter;          // the tube (hidden for the O)
        Ref<CCSprite> tight, wide;          // glow layers (additive)
        Ref<CCSprite> ring;                 // the O: the galaxy
        float scale = 0.f, lit = 0.f, flicker = 0.f, ringScale = 0.f;
        bool  over = false;
    };
    std::vector<WordLetter> m_word;
    float m_wordT = 0.f;
    Ref<CCSprite> m_verTight, m_verWide; // the version's glow
    float m_verLit = 0.f;
    float m_wordLeft = 0.f, m_wordRight = 0.f; // where ACEWARE is (a click on it calls the robot)
    float m_verX = 10.f, m_verY = 0.f;   // where the version is (left, middle)
    // Where the line under ACEWARE goes so ACEWARE is centred between it and the
    // sidebar panel's top (H - 4): the letters' drawn top / bottom.
    float wordLineY() {
        float top = -1e9f, bot = 1e9f;
        for (auto const& n : m_word) {
            if (n.ring) {
                CCRect const r = n.ring->boundingBox();
                top = std::max(top, r.getMaxY());
                bot = std::min(bot, r.getMinY());
                continue;
            }
            CCLabelBMFont* l = n.letter.data();
            if (!l || !l->getChildren()) continue;
            for (auto* g : CCArrayExt<CCNode*>(l->getChildren())) {
                CCRect const gb = g->boundingBox();
                CCPoint const a = m_body->convertToNodeSpace(l->convertToWorldSpace(gb.origin));
                CCPoint const b = m_body->convertToNodeSpace(l->convertToWorldSpace({gb.getMaxX(), gb.getMaxY()}));
                top = std::max({top, a.y, b.y});
                bot = std::min({bot, a.y, b.y});
            }
        }
        if (top < bot) return H - 39.f;
        float const gap = (H - 4.f) - top;
        return bot - gap;
    }
    void buildVersionGlow(float cx, float cy) {
        float const pad = 7.f;
        std::string const badge = titleBadge();
        auto* src = AceLabel::make(badge.c_str(), "bigFont.fnt");
        src->setScale(0.28f);
        float const w = src->getContentSize().width * src->getScaleX() + 2.f * pad;
        float const h = src->getContentSize().height * src->getScaleY() + 2.f * pad;
        src->setAnchorPoint({0.5f, 0.5f});
        src->setPosition({w / 2.f, h / 2.f});
        m_verTight = aw::bloom(src, w, h, 0.7f, 2.4f);
        m_verWide = aw::bloom(src, w, h, 2.4f, 2.2f);
        for (CCSprite* sp : {m_verTight.data(), m_verWide.data()}) {
            if (!sp) continue;
            sp->setColor(aw::themed(ccColor3B{95, 211, 160}));
            aw::additive(sp);
            sp->setAnchorPoint({0.5f, 0.5f});
            sp->setPosition({cx, cy});
            sp->setOpacity(0);
            m_body->addChild(sp, 3);
        }
    }

    // `x` is the middle of ACEWARE plus `extra` (the version beside it)
    void buildWordmark(float x, float y, float extra = 0.f) {
        m_word.clear();
        static char const* const word = "ACEWARE";
        ccColor3B const glowCols[4] = {aw::themed(ccColor3B{60, 170, 130}), aw::themed(ccColor3B{80, 190, 150}), aw::themed(ccColor3B{95, 211, 160}), aw::themed(ccColor3B{140, 240, 200})};
        float sc = 0.95f;
        float const gap = 0.35f;
        // fit the sidebar
        auto widthAt = [&](float s) {
            float tw = 0.f;
            for (int i = 0; word[i]; i++) {
                char const ch[2] = {word[i], 0};
                tw += textWidth(ch, s) + (word[i + 1] ? gap : 0.f);
            }
            return tw;
        };
        float const room = SIDE_W - 16.f - extra;
        if (widthAt(sc) > room) sc *= room / widthAt(sc);
        x -= (widthAt(sc) + extra) / 2.f; // (x is the middle of ACEWARE + the version)
        m_wordLeft = x;
        for (int i = 0; word[i]; i++) {
            char const ch[2] = {word[i], 0};
            auto* l = AceLabel::make(ch, "bigFont.fnt");
            l->setScale(sc);
            l->setColor(aw::themed(ccColor3B{232, 240, 236}));
            l->setAnchorPoint({0.5f, 0.5f});
            float const w = l->getContentSize().width * l->getScaleX();
            float const h = l->getContentSize().height * l->getScaleY();
            l->setPosition({x + w / 2.f, y});
            m_body->addChild(l, 5);
            WordLetter n;
            n.letter = l;
            n.scale = sc;
            if (word[i] == 'O') {
                // the galaxy, sized to the O's glyph
                float d = std::min(w, h) * 0.8f;
                CCPoint c = l->getPosition();
                if (l->getChildren() && l->getChildrenCount() > 0) {
                    if (auto* g = typeinfo_cast<CCSprite*>(l->getChildren()->objectAtIndex(0))) {
                        CCSize const gs = g->getContentSize();
                        d = std::min(gs.width, gs.height) * l->getScaleX() * 1.02f;
                        c = m_body->convertToNodeSpace(l->convertToWorldSpace(g->getPosition()));
                    }
                }
                l->setVisible(false);
                n.ring = aw::sprite(aw::galaxyRing(), d * 64.f / aw::RING_OUT);
                if (n.ring) {
                    n.ring->setPosition(c);
                    n.ringScale = n.ring->getScale();
                    m_body->addChild(n.ring, 5);
                }
                n.tight = aw::sprite(aw::ringGlow(), d * 80.f / aw::RING_OUT, glowCols[i % 4]);
                n.wide = aw::sprite(aw::soft(), d * 1.7f, glowCols[i % 4]);
                for (CCSprite* s : {n.tight.data(), n.wide.data()}) {
                    if (!s) continue;
                    aw::additive(s);
                    s->setPosition(c);
                    s->setOpacity(0);
                    m_body->addChild(s, s == n.tight.data() ? 4 : 3);
                }
            } else {
                // the glow: this letter drawn on its own and blurred, twice
                float const pad = 9.f;
                float const bw = w + 2.f * pad, bh = h + 2.f * pad;
                auto* src = AceLabel::make(ch, "bigFont.fnt");
                src->setScale(sc);
                src->setAnchorPoint({0.5f, 0.5f});
                src->setPosition({bw / 2.f, bh / 2.f});
                n.tight = aw::bloom(src, bw, bh, 0.8f, 2.4f);
                n.wide = aw::bloom(src, bw, bh, 3.f, 2.2f);
                // (no render textures on this device: soft round glows instead)
                if (!n.tight) n.tight = aw::sprite(aw::soft(), std::max(w, h) * 1.5f);
                if (!n.wide) n.wide = aw::sprite(aw::soft(), std::max(w, h) * 2.6f);
                for (CCSprite* s : {n.tight.data(), n.wide.data()}) {
                    if (!s) continue;
                    s->setColor(glowCols[i % 4]);
                    s->setBlendFunc(ccBlendFunc{GL_SRC_ALPHA, GL_ONE});
                    s->setAnchorPoint({0.5f, 0.5f});
                    s->setPosition(l->getPosition());
                    s->setOpacity(0);
                    m_body->addChild(s, s == n.tight.data() ? 4 : 3);
                }
            }
            m_word.push_back(n);
            m_wordRight = x + w;
            x += w + gap;
        }
        signOn();
    }
    // the sign flickers on, letter by letter
    void signOn() {
        for (size_t i = 0; i < m_word.size(); i++) {
            m_word[i].flicker = 0.25f + 0.09f * static_cast<float>(i);
            m_word[i].lit = 0.f;
        }
    }
    void stepWordmark(float dt) {
        m_wordT += dt;
        if (m_word.empty() || !m_body) return;
        bool live = isVisible() && !levelRunning();
        CCPoint p{-9999.f, -9999.f};
#ifdef GEODE_IS_DESKTOP
        if (live) p = m_body->convertToNodeSpace(getMousePos());
#else
        live = false;
#endif
        for (size_t i = 0; i < m_word.size(); i++) {
            WordLetter& n = m_word[i];
            CCNode* shape = n.ring ? static_cast<CCNode*>(n.ring.data()) : static_cast<CCNode*>(n.letter.data());
            if (!shape) continue;
            CCRect box = shape->boundingBox();
            if (n.letter) {
                CCRect const lb = n.letter->boundingBox(); // the letter's full cell (also for the O)
                box.origin.y = std::min(box.origin.y, lb.origin.y);
                box.size.height = std::max(box.size.height, lb.size.height);
            }
            box.origin.x -= 1.2f;
            box.size.width += 2.4f;
            bool const over = live && box.containsPoint(p);
            if (over && !n.over && n.flicker <= 0.f) n.flicker = 0.2f; // a tube striking
            n.over = over;
            n.lit += ((over ? 1.f : 0.f) - n.lit) * std::min(1.f, dt * (over ? 16.f : 4.f));
            float f = 1.f;
            if (n.flicker > 0.f) {
                n.flicker -= dt;
                float const k = std::fmod(std::max(0.f, n.flicker), 0.1f);
                f = k > 0.055f ? 0.2f : 1.f;
            }
            float const hum = 0.86f + 0.14f * std::sin(m_wordT * 2.2f + static_cast<float>(i) * 1.1f);
            float const idle = 0.36f * hum;
            float const e = (idle + (1.f - idle) * n.lit) * f;           // how bright, 0..1
            float const a = std::clamp(m_alpha, 0.5f, 1.f);
            if (n.tight) n.tight->setOpacity(static_cast<GLubyte>(std::clamp(255.f * (0.3f + 0.7f * e) * f * a, 0.f, 255.f)));
            if (n.wide) n.wide->setOpacity(static_cast<GLubyte>(std::clamp(235.f * e * a, 0.f, 255.f)));
            float const grow = 1.f + 0.07f * n.lit;
            if (n.letter && n.letter->isVisible()) {
                n.letter->setScale(n.scale * grow);
                GLubyte const v = static_cast<GLubyte>(std::clamp(214.f + 41.f * e, 0.f, 255.f));
                n.letter->setColor({v, static_cast<GLubyte>(v - 8), 255});
                n.letter->setOpacity(static_cast<GLubyte>(f < 1.f ? 150 : 255));
            }
            if (n.ring) {
                n.ring->setScale(n.ringScale * grow);
                n.ring->setRotation(n.ring->getRotation() + dt * (8.f + 80.f * n.lit));
                n.ring->setOpacity(static_cast<GLubyte>(f < 1.f ? 150 : 255));
            }
        }
        // the version hums with the sign and burns brighter under the mouse
        {
            bool const overVer = live && p.x >= m_verX - 3.f && p.x <= m_verX + m_badgeW + 3.f && std::fabs(p.y - m_verY) <= 5.f;
            m_verLit += ((overVer ? 1.f : 0.f) - m_verLit) * std::min(1.f, dt * (overVer ? 14.f : 4.f));
            float const hum = 0.85f + 0.15f * std::sin(m_wordT * 2.2f + 4.f);
            float const e = 0.4f * hum + 0.6f * m_verLit;
            float const a = std::clamp(m_alpha, 0.5f, 1.f);
            if (m_verTight) m_verTight->setOpacity(static_cast<GLubyte>(std::clamp(255.f * (0.35f + 0.65f * e) * a, 0.f, 255.f)));
            if (m_verWide) m_verWide->setOpacity(static_cast<GLubyte>(std::clamp(220.f * e * a, 0.f, 255.f)));
        }
    }

    // ---- hover (2.2): buttons light up purple under the mouse --------------------------
    static constexpr int HOVER_TAG = 7301;
    static inline const ccColor3B K_HOVER = {255, 255, 255}; // neutral lift on hover
    static inline ccColor3B C_HOVER = K_HOVER;
    Ref<CCNode> m_hovered; // kept alive: a page rebuild can remove the button under the mouse
    void updateHover() {
        CCNode* hit = nullptr;
        bool const allowed = isVisible() && m_interactive && !m_dragging;
        CCPoint const mouse = getMousePos();
        if (allowed) {
            std::vector<CCMenu*> const menus = m_updBox ? std::vector<CCMenu*>{m_updMenu}
                                                        : std::vector<CCMenu*>{m_settingsMenu, m_listMenu, m_titleMenu, m_tabMenu, m_themeMenu};
            for (CCMenu* m : menus) {
                if (!m || !m->isVisible() || !m->getChildren()) continue;
                CCPoint const p = m->convertToNodeSpace(mouse);
                // a button = menu item > its sprite node > the glow (by tag);
                // found through the children, no casts (2.2: the cast never matched)
                for (auto* item : CCArrayExt<CCNode*>(m->getChildren())) {
                    if (!item || !item->isVisible() || !item->getChildren()) continue;
                    CCNode* hov = nullptr;
                    for (auto* img : CCArrayExt<CCNode*>(item->getChildren())) {
                        if (img && (hov = img->getChildByTag(HOVER_TAG))) break;
                    }
                    if (!hov) continue;
                    if (item->boundingBox().containsPoint(p)) { hit = hov; break; }
                }
                if (hit) break;
            }
        }
        hoverCards(mouse, allowed && !m_updBox);
        if (hit == m_hovered.data()) return;
        auto fade = [](CCNode* n, GLubyte to, float t) {
            n->stopAllActions();
            n->runAction(CCFadeTo::create(t, to));
        };
        if (m_hovered.data()) fade(m_hovered.data(), 0, 0.16f);
        m_hovered = hit;
        if (hit) fade(hit, 46, 0.1f);
        // (2.3) a colour dot: its row's title shows its name, in its colour
        for (int r = 0; r < 2; r++) {
            if (!m_themeLab[r]) continue;
            m_themeLab[r]->setString(r == 0 ? "Accent" : "Background");
            m_themeLab[r]->setColor(C_MUTED);
        }
        for (auto const& th : m_themeHov) {
            if (th.glow != hit || th.row < 0 || th.row > 1 || !m_themeLab[th.row]) continue;
            m_themeLab[th.row]->setString(fmt::format("{}: {}", th.row == 0 ? "Accent" : "Background", th.name).c_str());
            m_themeLab[th.row]->setColor(th.col);
        }
    }

    // the settings cog turns while the mouse is on it, then settles on a tooth
    CCSprite* m_gear = nullptr;
    CCNode*   m_gearHover = nullptr;
    void stepGear(float dt) {
        if (!m_gear) return;
        bool const over = m_gearHover && m_hovered.data() == m_gearHover;
        float r = m_gear->getRotation();
        if (over) {
            r += 150.f * dt;
        } else {
            float const next = std::ceil(r / 45.f - 1e-3f) * 45.f;
            r = std::min(next, r + 150.f * dt);
        }
        m_gear->setRotation(std::fmod(r, 360.f));
    }

    // ---- online users (2.2) ----------------------------------------------------------
    CCNode*        m_onlineDots = nullptr; // (2.2) the pill + dots (smooth sprites)
    CCLabelBMFont* m_onlineOn = nullptr;
    CCLabelBMFont* m_onlineOff = nullptr;
    long long      m_onlineShownTotal = -2, m_onlineShownOn = -2;
    int            m_onlineTab = -1;
    // the page's own footer buttons (Log: Console / Clear, Dev: Copy All), laid
    // out right to left from the online pill; the footer text stops before them
    struct FootBtn { CCMenuItemSpriteExtra* item; float w; };
    std::vector<FootBtn> m_footBtns;
    float m_footTextRight = W - PAD;

    void refreshOnline(bool force) {
        if (!m_onlineDots || !m_onlineOn || !m_onlineOff) return;
        if (!force && s_online.total == m_onlineShownTotal && s_online.online == m_onlineShownOn && m_tab == m_onlineTab) return;
        m_onlineTab = m_tab;
        m_onlineShownTotal = s_online.total;
        m_onlineShownOn = s_online.online;
        m_onlineDots->removeAllChildren();
        bool const have = s_online.total >= 0 && s_online.online >= 0;
        m_onlineOn->setVisible(have);
        m_onlineOff->setVisible(have);
        float left = W - PAD - 22.f; // the settings button and the resize corner are at the far right
        if (have) {
            long long const off = std::max(0LL, s_online.total - s_online.online);
            m_onlineOn->setString(fmt::format("{} online", s_online.online).c_str());
            m_onlineOff->setString(fmt::format("{} offline", off).c_str());
            // (2.2) bottom right, just left of the settings button
            float const y = FOOT_H / 2.f + 0.5f;
            float const wOn = m_onlineOn->getContentSize().width * m_onlineOn->getScale();
            float const wOff = m_onlineOff->getContentSize().width * m_onlineOff->getScale();
            float const right = W - PAD - 19.f;
            float const xOff = right - wOff;
            float const xOn = xOff - 16.f - wOn;
            m_onlineOn->setPosition({xOn, y});
            m_onlineOff->setPosition({xOff, y});
            // a dark pill with a thin violet edge behind both, round dots with a soft glow
            float const px0 = xOn - 11.f, px1 = right + 5.f, ph = 11.f;
            if (auto* edge = aa::roundBox(px1 - px0, ph, ph / 2.f, aw::themed(ccColor3B{96, 74, 168}), 150)) {
                edge->setPosition({px0, y - ph / 2.f});
                m_onlineDots->addChild(edge);
            }
            if (auto* body = aa::roundBox(px1 - px0 - 1.2f, ph - 1.2f, ph / 2.f - 0.6f, aw::themed(ccColor3B{22, 20, 30}), 240)) {
                body->setPosition({px0 + 0.6f, y - ph / 2.f + 0.6f});
                m_onlineDots->addChild(body);
            }
            auto dot = [&](float x, float d, ccColor3B c, GLubyte op) {
                if (auto* sp = aa::circleSprite(d, c, op)) { sp->setPosition({x, y}); m_onlineDots->addChild(sp); }
            };
            dot(xOn - 5.f, 7.f, aw::themed(ccColor3B{80, 230, 120}), 60);   // glow
            dot(xOn - 5.f, 4.2f, aw::themed(ccColor3B{96, 240, 136}), 255); // online
            dot(xOff - 5.f, 4.2f, aw::themed(ccColor3B{140, 138, 156}), 255); // offline
            left = px0;
        }
        layoutFooter(left);
    }
    void layoutFooter(float left) {
        float x = left - 4.f;
        for (auto it = m_footBtns.rbegin(); it != m_footBtns.rend(); ++it) {
            if (!it->item) continue;
            it->item->setPositionX(x - it->w / 2.f);
            x -= it->w + 4.f;
        }
        m_footTextRight = x;
        m_shownFooter.clear(); // the footer text refits next refresh
    }

    // ---- live update -------------------------------------------------------------
    static bool levelRunning() {
        PlayLayer* pl = PlayLayer::get();
        return pl && !pl->m_isPaused;
    }

    // A vertical gradient (top colour -> bottom colour) with round corners:
    // thin horizontal bands, each inset by the corner's curve.
    static void roundedGradient(CCDrawNode* dn, float w, float h, float r, ccColor4B top, ccColor4B bot) {
        constexpr int BANDS = 48;
        r = std::clamp(r, 0.f, std::min(w, h) / 2.f);
        auto inset = [&](float y) {
            float const d = std::min(y, h - y); // distance to the nearest edge
            if (d >= r) return 0.f;
            float const dy = r - d;
            return r - std::sqrt(std::max(0.f, r * r - dy * dy));
        };
        for (int i = 0; i < BANDS; i++) {
            float const y0 = h * static_cast<float>(i) / BANDS, y1 = h * static_cast<float>(i + 1) / BANDS;
            float const t = (y0 + y1) * 0.5f / h; // 0 bottom .. 1 top
            float const a = (bot.a + (top.a - bot.a) * t) / 255.f;
            float const cr = (bot.r + (top.r - bot.r) * t) / 255.f, cg = (bot.g + (top.g - bot.g) * t) / 255.f,
                        cb = (bot.b + (top.b - bot.b) * t) / 255.f;
            float const i0 = inset(y0), i1 = inset(y1);
            CCPoint pts[4] = {{i0, y0}, {w - i0, y0}, {w - i1, y1 + 0.05f}, {i1, y1 + 0.05f}};
            dn->drawPolygon(pts, 4, ccColor4F{cr * a, cg * a, cb * a, a}, 0.f, ccColor4F{0.f, 0.f, 0.f, 0.f});
        }
    }

    // ---- the robot (2.2 easter egg) ----------------------------------------------
    // Every 10 seconds there's a 1 in 5 chance your robot drops in at the left
    // of the page and jumps across it from button to button (and along the
    // cards' tops). (2.3) It falls in from the top of the screen, and when
    // it's done it jumps off and falls to the bottom of the screen. Click
    // ACEWARE to call it; click the robot and it explodes.
    Ref<aw::RobotHop> m_robot;
    float m_robotClock = 0.f;
    aw::Rand m_fxRand{static_cast<uint32_t>(std::random_device{}())};
    // (2.3) you picked another icon / colours in the garage while the window
    // is up: the robot and the Modes tab's icons change too
    long long m_iconSig = 0;
    float m_iconCheck = 0.f;
    void checkIcons(float dt) {
        m_iconCheck -= dt;
        if (m_iconCheck > 0.f) return;
        m_iconCheck = 0.5f;
        long long const sig = aw::iconSig();
        if (m_iconSig == 0) { m_iconSig = sig; return; }
        if (sig == m_iconSig) return;
        m_iconSig = sig;
        if (m_robot) m_robot->refreshIcon();
        if (m_tab == TAB_MODES && isVisible()) showTab(TAB_MODES);
    }
    void stepRobot(float dt) {
        checkIcons(dt);
        if (m_robot) {
            if (m_robot->done() || !isVisible()) {
                m_robot->removeFromParent();
                m_robot = nullptr;
            }
            return;
        }
        if (!isVisible() || levelRunning()) { m_robotClock = 0.f; return; }
        m_robotClock += dt;
        if (m_robotClock < 10.f) return;
        m_robotClock = 0.f;
        if (m_fxRand.f() < 0.2f) launchRobot();
    }
    // what it can stand on: the page's buttons and the tops of its cards (window space)
    std::vector<aw::Perch> perches() {
        std::vector<aw::Perch> out;
        auto add = [&](CCNode* space, CCRect r, CCNode* node) {
            CCPoint const a = m_body->convertToNodeSpace(space->convertToWorldSpace(r.origin));
            CCPoint const b = m_body->convertToNodeSpace(space->convertToWorldSpace({r.getMaxX(), r.getMaxY()}));
            if (b.x - a.x < 8.f || b.y > H - TITLE_H - 2.f) return;   // (not the header)
            out.push_back({a.x, b.x, b.y, node});
        };
        for (CCMenu* m : {m_settingsMenu, m_listMenu, m_titleMenu}) {
            if (!m || !m->isVisible() || !m->getChildren()) continue;
            for (auto* item : CCArrayExt<CCNode*>(m->getChildren())) {
                if (item && item->isVisible()) add(m, item->boundingBox(), item);
            }
        }
        if (m_cards && m_cards->isVisible()) {
            for (auto const& fx : m_cardFx) add(m_cards, fx.rect, nullptr);
        }
        // and the footer line, so there's always somewhere to land
        out.push_back({SIDE_W + 6.f, FULL_W - 8.f, FOOT_H + 0.5f, nullptr});
        return out;
    }
    void launchRobot() {
        if (!m_body || !getParent() || m_robot) return;
        // (2.3) the screen's top / bottom in the window's space
        CCSize const win = CCDirector::get()->getWinSize();
        float const screenTop = m_body->convertToNodeSpace(m_body->getParent() ? CCPoint{0.f, win.height} : CCPoint{0.f, H}).y;
        float const screenBottom = m_body->convertToNodeSpace(CCPoint{0.f, 0.f}).y;
        m_robot = aw::RobotHop::create(m_body, perches(), SIDE_W + 6.f, std::max(screenTop, H), FULL_W + 24.f,
                                       std::min(screenBottom, 0.f));
        if (m_robot) m_body->addChild(m_robot, 60);
    }

    void update(float dt) override {
        s_onlineWanted = isVisible() && !levelRunning();
        // (2.3) back after the settings popup closed
        if (m_settingsPopup && !m_settingsPopup->getParent()) {
            m_settingsPopup = nullptr;
            if (!isVisible() && Mod::get()->getSavedValue<bool>("window-shown", true)) {
                setVisible(true);
                popIn();
            }
        }
        // (2.3) Dev mode on / off changes the room under the tabs: the colours
        // card is built again to fit
        if (!DEMO_RUN_SHOWN && m_themeSlots >= 0 && tabSlotsShown() != m_themeSlots) {
            m_themeSlots = tabSlotsShown();
            Loader::get()->queueInMainThread([] { BufferWindow::rebuildSoon(); });
        }
        // the sky, course demo, the sign - only while the window shows
        if (isVisible()) {
            if (m_run && m_runBox) {
                bool const room = tabSlotsShown() <= 4; // (the Dev tabs take its place)
                m_runBox->setVisible(room);
                if (room) m_run->step(dt);
            }
            if (m_pageSky) m_pageSky->step(dt);
            if (m_sideSky) m_sideSky->step(dt);
            stepWordmark(dt);
            stepGear(dt);
        }
        stepRobot(dt);
        // Display-only while the level runs: buttons off so clicks stay jumps.
        bool const interactive = !levelRunning();
        if (interactive != m_interactive) {
            m_interactive = interactive;
            for (CCMenu* m : {m_titleMenu, m_tabMenu, m_listMenu, m_settingsMenu}) m->setEnabled(interactive);
            if (m_themeMenu) m_themeMenu->setEnabled(interactive);
            if (m_updMenu) m_updMenu->setEnabled(interactive);
            if (m_updBlock) m_updBlock->setEnabled(interactive);
            m_dragging = m_scrollDrag = false;
        }
#ifdef GEODE_IS_DESKTOP
        updateHover();
#endif
        // ---- animation tick (Aceware) -------------------------------------
        m_animT += dt;
        // the version breathes under the wordmark
        if (m_badgeLab) {
            float const k = 1.f + 0.035f * std::sin(m_animT * 1.9f);
            m_badgeLab->setScale(0.28f * k);
        }
        // the accent line gets a light running along it every few seconds
        m_sweepT -= dt;
        if (m_sweepT <= 0.f && m_accentSweep && isVisible()) {
            m_sweepT = 7.5f;
            m_accentSweep->stopAllActions();
            m_accentSweep->setPosition({6.f, H - TITLE_H + 0.5f});
            m_accentSweep->setOpacity(0);
            m_accentSweep->runAction(CCSequence::create(
                CCFadeTo::create(0.12f, 210),
                CCEaseInOut::create(CCMoveTo::create(1.05f, CCPoint{W - 8.f, H - TITLE_H + 0.5f}), 2.f),
                CCFadeTo::create(0.2f, 0), nullptr));
        }
        // the selected tab breathes very slightly, so the sidebar feels alive
        if (m_tabHi) {
            auto* r = typeinfo_cast<CCRGBAProtocol*>(m_tabHi);
            if (r) r->setOpacity(static_cast<GLubyte>(236 + 14 * std::sin(m_animT * 2.4f)));
        }
        // the resize strokes only exist while you drag the corner, and pulse then
        for (size_t i = 0; i < m_grip.size(); i++) {
            CCNode* g = m_grip[i];
            if (!g) continue;
            g->setVisible(m_resizing);
            float const pulse = m_resizing ? (1.25f + 0.12f * std::sin(m_animT * 14.f)) : 1.f;
            g->setScale(pulse);
        }
        // scrollbar: bright while scrolling, then settles down
        m_scrollFade -= dt;
        if (m_scrollThumb) {
            GLubyte const want = m_scrollFade > 0.f ? 235 : 120;
            auto* r = typeinfo_cast<CCRGBAProtocol*>(m_scrollThumb);
            if (r && r->getOpacity() != want) r->setOpacity(want);
        }
        m_refreshAt -= dt;
        m_statsAge += dt;
        m_badgeTapAge += dt;
        if (m_refreshAt <= 0.f && isVisible()) {
            m_refreshAt = 0.25f;
            refresh();
        }
    }

    // ---- touches: drag by the title bar, drag-scroll the list ---------------------
    void registerWithTouchDispatcher() override {
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, PRIO_WINDOW, true);
    }

    bool insideLocal(CCPoint p) const { return p.x >= 0.f && p.y >= 0.f && p.x <= FULL_W && p.y <= H; }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!isVisible() || levelRunning()) return false;
        CCPoint const bp = m_body ? m_body->convertToNodeSpace(touch->getLocation()) : CCPoint{-1.f, -1.f};
        // the robot (it can be outside the window): it explodes
        if (m_robot && m_robot->hit(bp)) {
            m_robot->blowUp();
            return true;
        }
        CCPoint const p = convertToNodeSpace(touch->getLocation());
        if (!insideLocal(p)) return false;
        // corner grip: drag to resize, no settings menu needed. Starts past the
        // settings cog, which owns the rest of the footer's right end.
        if (p.x > FULL_W - 11.f && p.y < 16.f) {
            m_resizing = true;
            // Window Size is a multiplier (1.0 = half the original size, see init()),
            // so the drag starts from the multiplier, not from the live scale.
            m_dragStartScale = getScale() / std::max(0.01f, 0.5f * PLATFORM_WINDOW_SCALE);
            return true;
        }
        // course demo: click the icon and it explodes like in GD
        if (m_run && m_runBox && m_runBox->isVisible() && m_runRect.containsPoint(bp)) {
            if (m_run->clickAt({bp.x - m_runRect.origin.x - 1.f, bp.y - m_runRect.origin.y - 1.f})) return true;
        }
        // ACEWARE: a click (not a drag) calls the robot
        m_tapLogo = bp.x > m_wordLeft - 2.f && bp.x < m_wordRight + 2.f && bp.y > H - 33.f && bp.y < H - 10.f;
        m_tapMoved = 0.f;
        // (2.3) double-click the page header: the window goes back to its
        // starting spot (top right) - for a window dragged somewhere awkward
        if (p.y >= H - TITLE_H && p.x >= SIDE_W) {
            auto const nowT = Clock::now();
            if (nowT - m_lastHeaderTap < std::chrono::milliseconds(350)) {
                auto const win = CCDirector::sharedDirector()->getWinSize();
                setPosition(clampPos({win.width - FULL_W * getScale() - 6.f, win.height - H * getScale() - 6.f}));
                savePosition();
                m_lastHeaderTap = {};
                return true;
            }
            m_lastHeaderTap = nowT;
        }
        // drag: by the page header, or the sidebar's title block
        m_dragging = p.y >= H - TITLE_H || (p.x < SIDE_W && p.y >= H - 40.f);
        m_scrollDrag = !m_dragging && p.x >= SIDE_W && isList(m_tab) && p.y > contentBottom() && p.y < contentTop();
        m_dragAccum = 0.f;
        return true; // swallow: nothing behind the window gets this touch
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        CCPoint const d = touch->getDelta();
        if (m_resizing) {
            // Multiplier units, like the Window Size setting and init()'s scale.
            float const mult = std::clamp(m_dragStartScale + (d.x + d.y) * 0.008f, 0.7f, 1.6f);
            setScale(0.5f * PLATFORM_WINDOW_SCALE * mult);
            s_builtScale = mult; // don't let ensure() rebuild it mid-drag
            return;
        }
        m_tapMoved += std::fabs(d.x) + std::fabs(d.y);
        if (m_dragging) {
            setPosition(clampPos({getPositionX() + d.x, getPositionY() + d.y}));
        } else if (m_scrollDrag) {
            m_dragAccum += d.y / getScale();
            while (m_dragAccum >= LINE_H)  { m_dragAccum -= LINE_H; scrollBy(1); }
            while (m_dragAccum <= -LINE_H) { m_dragAccum += LINE_H; scrollBy(-1); }
        }
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (m_resizing) {
            // Save the multiplier, not the absolute scale: a drag to 1.2 rebuilds at 1.2.
            float const mult = std::clamp(getScale() / std::max(0.01f, 0.5f * PLATFORM_WINDOW_SCALE), 0.7f, 1.6f);
            Mod::get()->setSettingValue<double>("window-size", static_cast<double>(mult));
            s_builtScale = mult;
            m_resizing = false;
            return;
        }
        if (m_dragging) savePosition();
        // (Aceware) the robot easter egg is off
        m_tapLogo = false;
        m_dragging = m_scrollDrag = false;
    }

    void ccTouchCancelled(CCTouch* t, CCEvent* e) override {
        m_tapLogo = false; // (a cancelled touch never calls the robot)
        ccTouchEnded(t, e);
    }

    // ---- (2.3) the update box: Aceware's own look, over the window ----------------
    // A dimmed window with a card in the middle: what's new, Later / Update,
    // then Downloading..., then Restart. The window behind it can't be
    // clicked while it's up.
    CCNode* m_updBox = nullptr;
    CCMenu* m_updMenu = nullptr;  // its buttons (ahead of everything in the window)
    CCMenu* m_updBlock = nullptr; // the rest of the window: swallows clicks
    bool m_updShownWindow = false; // the box brought the (hidden) window up: Later hides it again

    void closeUpdateBox() {
        if (!m_updBox) return;
        CCNode* box = m_updBox;
        m_updBox = nullptr;
        m_updMenu = nullptr;
        m_updBlock = nullptr;
        m_hovered = nullptr;
        box->removeFromParent();
        if (m_updShownWindow) {
            m_updShownWindow = false;
            if (!Mod::get()->getSavedValue<bool>("window-shown", true)) popOut();
        }
    }

    void buildUpdateBox(bool popAnim) {
        if (m_updBox) {
            m_updBox->removeFromParent();
            m_updBox = nullptr;
            m_hovered = nullptr;
        }
        ccColor3B const white = aw::themed(ccColor3B{255, 255, 255});
        auto* root = CCNode::create();
        root->setContentSize({FULL_W, H});
        m_body->addChild(root, 50);
        m_updBox = root;

        // the window behind: dimmed, and it takes no clicks
        root->addChild(panel(FULL_W, H, ccColor3B{0, 0, 0}, static_cast<GLubyte>(std::clamp(150.f * m_alpha, 0.f, 255.f)), 10.f), 0);
        m_updBlock = CCMenu::create();
        m_updBlock->ignoreAnchorPointForPosition(false);
        m_updBlock->setAnchorPoint({0.f, 0.f});
        m_updBlock->setPosition({0.f, 0.f});
        m_updBlock->setContentSize({FULL_W, H});
        m_updBlock->setTouchPriority(PRIO_MENUS - 5);
        {
            auto* blocker = CCMenuItem::create();
            blocker->setContentSize({FULL_W, H});
            blocker->setAnchorPoint({0.5f, 0.5f});
            blocker->setPosition({FULL_W / 2.f, H / 2.f});
            m_updBlock->addChild(blocker);
        }
        root->addChild(m_updBlock, 1);

        // the card, a card like the pages' (glowing edge)
        float const cw = 214.f, ch = 106.f;
        float const cx = (FULL_W - cw) / 2.f, cy = (H - ch) / 2.f;
        auto* card = CCNode::create();
        card->setContentSize({FULL_W, H});
        card->setAnchorPoint({0.5f, 0.5f});
        card->ignoreAnchorPointForPosition(false);
        card->setPosition({FULL_W / 2.f, H / 2.f});
        root->addChild(card, 2);
        if (CCNode* g = aw::glowBox(cx, cy, cw, ch, 6.f, C_GLOW, glowOp(GLOW_PEAK))) card->addChild(g);
        CCNode* e = aa::roundBox(cw, ch, 6.f, C_CARD_LIT, 255);
        if (!e) e = panel(cw, ch, C_CARD_LIT, 255, 6.f);
        e->setPosition({cx, cy});
        card->addChild(e);
        CCNode* in = aa::roundBox(cw - 1.2f, ch - 1.2f, 6.6f, C_CARD, 250);
        if (!in) in = panel(cw - 1.2f, ch - 1.2f, C_CARD, 250, 5.4f);
        in->setPosition({cx + 0.6f, cy + 0.6f});
        card->addChild(in);

        // what it says
        std::string head, line1, line2;
        std::string const cur = Mod::get()->getVersion().toVString();
        if (s_update.done) {
            head = "Update installed";
            line1 = fmt::format("Aceware {} is ready.", s_update.version);
            line2 = "Restart the game to use it.";
        } else if (s_update.downloading) {
            head = "Updating...";
            line1 = fmt::format("Downloading Aceware {}", s_update.version);
            line2 = "You can close this - it keeps going.";
        } else if (s_update.failed) {
            head = "Update failed";
            line1 = "The update didn't download.";
            line2 = "Check your connection and try again.";
        } else {
            head = "Update available";
            line1 = fmt::format("Aceware {} is out (you have {}).", s_update.version, cur);
            line2 = "It downloads from Aceware's GitHub.";
        }
        auto* t = AceLabel::make(head.c_str(), "bigFont.fnt");
        t->setScale(0.4f);
        t->setColor(C_LAV);
        limitNodeWidth(t, cw - 16.f, 0.4f, 0.2f);
        t->setPosition({FULL_W / 2.f, cy + ch - 17.f});
        card->addChild(t);
        auto* l1 = bodyLabel(line1.c_str(), 0.26f, white);
        limitNodeWidth(l1, cw - 16.f, 0.26f, 0.14f);
        l1->setPosition({FULL_W / 2.f, cy + ch - 39.f});
        card->addChild(l1);
        auto* l2 = bodyLabel(line2.c_str(), 0.21f, C_MUTED);
        limitNodeWidth(l2, cw - 16.f, 0.21f, 0.12f);
        l2->setPosition({FULL_W / 2.f, cy + ch - 53.f});
        card->addChild(l2);

        // buttons
        m_updMenu = CCMenu::create();
        m_updMenu->ignoreAnchorPointForPosition(false);
        m_updMenu->setAnchorPoint({0.f, 0.f});
        m_updMenu->setPosition({0.f, 0.f});
        m_updMenu->setContentSize({FULL_W, H});
        m_updMenu->setTouchPriority(PRIO_MENUS - 10);
        card->addChild(m_updMenu, 4);
        float const bw = 86.f, bh = 22.f, by = cy + 20.f;
        bool const two = !s_update.downloading;
        float const lx = two ? FULL_W / 2.f - bw / 2.f - 4.f : FULL_W / 2.f;
        // (the box is rebuilt / removed after the click, never inside it)
        boxButton(m_updMenu, "Later", {lx, by}, bw, bh, C_BTN, white, 0.28f, [] {
            Loader::get()->queueInMainThread([] {
                if (auto* w = BufferWindow::s_instance) w->closeUpdateBox();
            });
        });
        if (two) {
            char const* act = s_update.done ? "Restart" : s_update.failed ? "Try Again" : "Update";
            auto* go = boxButton(m_updMenu, act, {FULL_W / 2.f + bw / 2.f + 4.f, by}, bw, bh, C_BTN_SEL, white, 0.28f, [] {
                if (s_update.done) {
                    geode::utils::game::restart(true);
                    return;
                }
                Loader::get()->queueInMainThread([] {
                    startUpdate();
                    updatePopup(false);
                });
            });
            shine(go, bw, bh, C_GLOW);
        }
        bool const on = m_interactive;
        m_updMenu->setEnabled(on);
        m_updBlock->setEnabled(on);

        if (popAnim) {
            card->setScale(0.86f);
            card->runAction(CCEaseBackOut::create(CCScaleTo::create(0.22f, 1.f)));
        }
    }

public:
    // Show the box (open) or redraw it if it's up (open = false). false = the
    // window can't show it now (you're playing, or it isn't there yet).
    static bool updateBox(bool open) {
        if (levelRunning()) return false;
        bool made = false;
        if (open && (!s_instance || !s_instance->getParent())) { // hidden all session: brought up for this
            ensure();
            made = true;
        }
        BufferWindow* w = s_instance;
        if (!w || !w->getParent()) return false;
        if (made) w->m_updShownWindow = !Mod::get()->getSavedValue<bool>("window-shown", true);
        if (w->m_settingsPopup && w->m_settingsPopup->getParent()) return false; // (the mod's settings are open)
        if (!open && !w->m_updBox) {
            if (w->m_tab == TAB_SETTINGS && w->isVisible()) w->showTab(TAB_SETTINGS); // the Update tile's text
            return w->isVisible();
        }
        if (!w->isVisible()) {
            w->setVisible(true);
            w->popIn();
            w->m_updShownWindow = !Mod::get()->getSavedValue<bool>("window-shown", true);
        }
        bool const fresh = !w->m_updBox;
        w->buildUpdateBox(fresh);
        if (w->m_tab == TAB_SETTINGS) w->showTab(TAB_SETTINGS);
        return true;
    }

public:
    // Mouse wheel over the window (1.25.9): the window gets it FIRST and it
    // stops there - before, a level list behind the window scrolled too.
    // Called from the CCMouseDispatcher hook; true = handled, don't pass it on.
    static bool takeScroll(float y) {
        BufferWindow* w = s_instance;
        if (!w || !w->isVisible() || !w->getParent()) return false;
        CCPoint const p = w->convertToNodeSpace(getMousePos());
        if (!w->insideLocal(p)) return false;
        if (isList(w->m_tab) && y != 0.f) w->scrollBy(y > 0.f ? 3 : -3);
        return true; // over the window: nothing behind it scrolls
    }
};

void windowEnsure() { BufferWindow::ensure(); }
bool updatePopup(bool open) { return BufferWindow::updateBox(open); }

} // namespace

// Level scan runner (2.1): one slice of the scan per frame while it runs. On
// Geode's overlay so it keeps going while the level is paused.
class ScanRunner : public CCNode {
public:
    static ScanRunner* create() {
        auto* p = new ScanRunner();
        if (p->init()) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    bool init() override {
        if (!CCNode::init()) return false;
        scheduleUpdate();
        return true;
    }
    void update(float) override { s_sim.scanSlice(); }
};

#ifdef GEODE_IS_WINDOWS
// Window Key on Windows: read the key itself every frame. Input mods (Click
// Between Frames) take over the keyboard and Shift on its own never reaches
// GD's key handling, so events can't be relied on. Only while GD is the
// focused window, never while typing in a text box.
class WindowKeyPoller : public CCNode {
    bool m_wasDown = false;

    static bool gdFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    // cocos key codes are Windows virtual-key codes for normal keys.
    static bool bindDown() {
        auto const binds = Mod::get()->getSettingValue<std::vector<Keybind>>("window-key-v3");
        for (Keybind const& b : binds) {
            int const vk = static_cast<int>(b.key);
            if (vk <= 0 || vk >= 0xFF) continue;
            if (GetAsyncKeyState(vk) & 0x8000) return true;
        }
        return false;
    }

public:
    static WindowKeyPoller* create() {
        auto* p = new WindowKeyPoller();
        if (p->init()) { p->autorelease(); return p; }
        delete p;
        return nullptr;
    }
    bool init() override {
        if (!CCNode::init()) return false;
        scheduleUpdate();
        return true;
    }
    void update(float) override {
        bool const down = gdFocused() && bindDown();
        if (down && !m_wasDown && !CCIMEDispatcher::sharedDispatcher()->hasDelegate()) BufferWindow::toggle();
        m_wasDown = down;
    }
};
#elif defined(GEODE_IS_MACOS)
// Window Key (macOS): from GD's key handling, in every scene. (Android has no
// keyboard: the pause menu's Aceware button shows / hides the window.)
static bool isRemoveClickKey(enumKeyCodes key) {
    auto const binds = Mod::get()->getSettingValue<std::vector<Keybind>>("remove-click-key");
    for (Keybind const& b : binds) if (b.key == key) return true;
    return false;
}
class $modify(BufKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double time) {
        if (down && !repeat && !CCIMEDispatcher::sharedDispatcher()->hasDelegate() && BufferWindow::isWindowKey(key)) {
            BufferWindow::toggle();
        }
        // F7 (Aceware): buffer on / off in one key. Off remembers the stage you
        // were on, so pressing it again puts you straight back.
        if (down && !repeat && key == KEY_F7 && !CCIMEDispatcher::sharedDispatcher()->hasDelegate()) {
            auto* mod = Mod::get();
            int const cur = s_sim.currentStage();
            if (cur != 0) {
                mod->setSavedValue<double>("aceware-prev-stage", cur);
                s_sim.setStage(0);
                Notification::create("Aceware: buffer OFF", NotificationIcon::None, 1.2f)->show();
            } else {
                int const prev = std::clamp(static_cast<int>(std::lround(mod->getSavedValue<double>("aceware-prev-stage", 2.0))),
                                            1, STAGE_COUNT - 1);
                s_sim.setStage(prev);
                Notification::create(fmt::format("Aceware: buffer ON - {}", STAGES[prev].name),
                                     NotificationIcon::None, 1.2f)->show();
            }
        }
        // (2.3) Remove Click Key: one of your kept autoplay clicks goes
        if (down && !repeat && !CCIMEDispatcher::sharedDispatcher()->hasDelegate() && s_sim.autoplayOn() && isRemoveClickKey(key)) {
            if (PlayLayer* pl = PlayLayer::get()) {
                bool const ok = s_sim.removeClick(pl);
                Notification::create(ok ? "Aceware - removed your last kept click" : "Aceware - no kept clicks on this level",
                                     NotificationIcon::None, 1.2f)->show();
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, time);
    }
};
#endif

#ifdef GEODE_IS_DESKTOP
// Mouse wheel: the window first when the cursor is over it (1.25.9). Desktop
// only - phones have no mouse (the list scrolls by dragging there).
class $modify(BufMouse, CCMouseDispatcher) {
    bool dispatchScrollMSG(float y, float x) {
        if (BufferWindow::takeScroll(y)) return true;
        return CCMouseDispatcher::dispatchScrollMSG(y, x);
    }
};
#endif

// Pause menu button that shows / hides the window (1.26.2), on every platform
// (the only way on phones). GD's green circle base with the mod's icon
// (resources/pauseButtonIcon.png, icon only), sized to match the others.
class $modify(BufPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        // "Pause Menu Button" setting (2.2): turned off = no button.
        if (!Mod::get()->getSettingValue<bool>("pause-button")) return;
        // GD's own green circle button (same base, rings, outline and shading as
        // the gear next to it) with the mod's icon on top. If the icon isn't in
        // the build (resources\ folder missing), fall back to the old text
        // button so there's always a way to open the window.
        // Drawn, not textured: a mint disc with a dot. Cannot become a missing
        // texture, and it matches the window's one accent colour.
        CCNode* spr = nullptr;
        {
            auto* holder = CCNode::create();
            holder->setContentSize({40.f, 40.f});
            if (auto* disc = aa::circleSprite(40.f, {95, 211, 160})) {
                disc->setPosition({20.f, 20.f});
                holder->addChild(disc);
                if (auto* inner = aa::circleSprite(19.f, {20, 22, 28})) {
                    inner->setPosition({20.f, 20.f});
                    holder->addChild(inner);
                }
                if (auto* dot = aa::circleSprite(9.f, {95, 211, 160})) {
                    dot->setPosition({20.f, 20.f});
                    holder->addChild(dot);
                }
                spr = holder;
            }
        }
        if (!spr) {
            spr = ButtonSprite::create("Aceware", "bigFont.fnt", "GJ_button_04.png", 0.6f);
            if (!spr) return;
        }
        auto* menu = this->getChildByID("right-button-menu");
        // same size as the buttons already in that menu (their sprites vary by GD version)
        float target = 0.f;
        if (menu) {
            for (auto* child : CCArrayExt<CCNode*>(menu->getChildren())) {
                float const w = child->getScaledContentSize().width;
                if (w > target) target = w;
            }
        }
        if (target <= 0.f) target = 38.f;
        float const w = spr->getContentSize().width;
        if (w > 0.f) spr->setScale(target / w);
        auto* btn = CCMenuItemExt::createSpriteExtra(spr, [](CCMenuItemSpriteExtra*) { BufferWindow::toggle(); });
        btn->setID("aceware-button"_spr);
        if (menu) {
            menu->addChild(btn);
            menu->updateLayout();
            // (Aceware) the button breathes, so it reads as alive in the pause menu
            {
                float const base = spr->getScale();
                spr->stopAllActions();
                spr->runAction(CCRepeatForever::create(CCSequence::create(
                    CCEaseInOut::create(CCScaleTo::create(1.15f, base * 1.07f), 1.7f),
                    CCEaseInOut::create(CCScaleTo::create(1.15f, base), 1.7f), nullptr)));
            }
            log::info("[Aceware] pause button added to right-button-menu");
        } else {
            log::info("[Aceware] no right-button-menu in the pause menu - button placed bottom-left");
            // no such menu (another mod moved things): our own, bottom-left
            auto* m = CCMenu::create();
            m->addChild(btn);
            m->setPosition({35.f, 35.f});
            this->addChild(m, 10);
        }
    }
};

// ----------------------------------------------------------------------------
//  8. Geode hooks
// ----------------------------------------------------------------------------

$on_mod(Loaded) {
    // (Aceware) no legacy migration: this mod id starts clean.
    // Stage cycle: 1 -> 2 -> 3 -> 4 -> Extreme -> Off
    listenForKeybindSettingPresses("cycle-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) s_sim.cyclePreset();
        return false; // don't swallow the key
    });
    // Window show / hide (default Left Shift): a key poller on Geode's overlay
    // (Windows) / the keyboard hook (elsewhere), so it works in every scene.
    // 1.12.3: the Window Key setting got a new id (old installs kept F10 from an
    // early version), and the window is switched back on once so it shows up.
    if (!Mod::get()->getSavedValue<bool>("migrated-1123", false)) {
        Mod::get()->setSavedValue<bool>("migrated-1123", true);
        Mod::get()->setSavedValue<bool>("window-shown", true);
    }
#ifdef GEODE_IS_WINDOWS
    queueInMainThread([] {
        if (auto* p = WindowKeyPoller::create()) OverlayManager::get()->addChild(p);
    });
#endif
    queueInMainThread([] {
        if (auto* p = OnlineCounter::create()) OverlayManager::get()->addChild(p);
    });
    log::info("[Aceware] loaded");
}


// Graphics reload (fullscreen <-> borderless etc.): rebuild the window, or it
// draws as big coloured blocks. GD shows the menu again after it, which brings
// the window back (2.2).
class $modify(BufGameManager, GameManager) {
    void reloadAll(bool switchingModes, bool toFullscreen, bool borderless, bool fix, bool unused) {
        BufferWindow::dropForReload();
        GameManager::reloadAll(switchingModes, toFullscreen, borderless, fix, unused);
    }
};

#ifdef GEODE_IS_ANDROID
// Android drops the game's graphics when it goes to the background: rebuild
// the window when it comes back.
class $modify(BufAppDelegate, AppDelegate) {
    void applicationWillEnterForeground() {
        AppDelegate::applicationWillEnterForeground();
        BufferWindow::rebuildSoon();
    }
};
#endif

// (2.3) Online: say goodbye when the game closes / goes to the background
// (GD saves the game at both).
class $modify(AceOnlineDelegate, AppDelegate) {
    void trySaveGame(bool force) {
        AppDelegate::trySaveGame(force);
        sendOnlineBye();
    }
    void applicationDidEnterBackground() {
        AppDelegate::applicationDidEnterBackground();
        sendOnlineBye();
    }
};

// Show the window in the main menu too (if it's switched on).
class $modify(BufMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        checkForUpdate();    // on opening the game: once a session
        showRestartPrompt(); // an update installed before the menu was up
        if (Mod::get()->getSavedValue<bool>("window-shown", true)) {
            Loader::get()->queueInMainThread([] { windowEnsure(); });
        }
        return true;
    }
};

class $modify(BufPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("PlayLayer::destroyPlayer", -1000000);
        (void)self.setHookPriority("PlayLayer::playEndAnimationToPos", -1000000);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        s_sim.init(this);
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        s_sim.onReset();
    }

    void pauseGame(bool p0) {
        s_sim.onPause(this);
        PlayLayer::pauseGame(p0);
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
        if (s_sim.checkSimulationDeath(player, gameObject)) return;
        if (s_sim.learnNoclip(this, player)) return; // (2.3) autoplay's learning run: deaths off
        bool const wasDead = player && player->m_isDead;
        PlayLayer::destroyPlayer(player, gameObject);
        // (2.3) autoplay: only a real death (a respawn's grace calls this for
        // nothing a dozen times)
        if (player && !wasDead && player->m_isDead && (player == m_player1 || player == m_player2)) {
            s_sim.onAutoDeath(player, gameObject);
        }
        // Only a real death counts: noclip-style mods and repeated calls while
        // already dead leave nothing to report.
        if (player && !wasDead && player->m_isDead &&
            (player == m_player1 || player == m_player2)) {
            s_sim.onRealDeath();
        }
    }

    void playEndAnimationToPos(cocos2d::CCPoint p0) {
        if (s_sim.isSimulating()) return;
        if (s_sim.learnEnd()) return; // (2.3) the learning run reached the end: restart instead
        s_sim.onLevelEnd(this);
        PlayLayer::playEndAnimationToPos(p0);
    }

    void onQuit() {
        PlayLayer::onQuit();
        s_sim.quit();
    }

    // (2.3) Autoplay played this level: nothing is saved as progress - the
    // completion goes through as a test run (GD saves nothing then), no new
    // best popup.
    void levelComplete() {
        if (s_sim.learnEnd()) return; // (2.3) not a completion: the learning run
        if (!s_sim.autoUsed()) {
            PlayLayer::levelComplete();
            return;
        }
        bool const old = m_isTestMode;
        m_isTestMode = true;
        PlayLayer::levelComplete();
        m_isTestMode = old;
    }
    void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {
        if (s_sim.autoUsed()) return;
        PlayLayer::showNewBest(newReward, orbs, diamonds, demonKey, noRetry, noTitle);
    }
};

// (2.3) ...and no percentage is saved for it either.
class $modify(AceAutoLevel, GJGameLevel) {
    void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
        if (s_sim.autoUsed()) return;
        GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
    }
};

class $modify(BufHardStreak, HardStreak) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("HardStreak::addPoint", -1000000);
    }

    void addPoint(cocos2d::CCPoint p0) {
        if (s_sim.isSimulating()) return;
        HardStreak::addPoint(p0);
    }
};

class $modify(BufEffectGameObject, EffectGameObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("EffectGameObject::triggerObject", -1000000);
    }

    void triggerObject(GJBaseGameLayer* p0, int p1, const gd::vector<int>* p2) {
        if (s_sim.isSimulating()) return;
        EffectGameObject::triggerObject(p0, p1, p2);
    }
};
// Orb side effects (effect circle, spawn triggers) must not run when a sim
// player uses an orb during a test.
// NOT hooked (2.1): RingObject::triggerActivated. On iOS and Apple Silicon
// Macs it is only 12 bytes long - smaller than an arm64 hook jump - so hooking
// it overwrote the start of powerOnObject right after it and crashed the game
// the moment you touched an orb. Its side effects in a test are covered
// anyway: spawn triggers (EffectGameObject::triggerObject) and the circle
// (spawnCircle) are blocked, and orb "activated" flags are put back after
// every test.
class $modify(BufRingObject, RingObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("RingObject::spawnCircle", -1000000);
        (void)self.setHookPriority("RingObject::powerOnObject", -1000000);
    }
    void spawnCircle() {
        if (s_sim.isSimulating()) return;
        RingObject::spawnCircle();
    }
    void powerOnObject(int state) {
        if (s_sim.isSimulating()) return;
        RingObject::powerOnObject(state);
    }
};

class $modify(BufGameObject, GameObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GameObject::playShineEffect", -1000000);
    }

    void playShineEffect() {
        if (s_sim.isSimulating()) return;
        GameObject::playShineEffect();
    }
};

class $modify(BufPlayerObject, PlayerObject) {
    // (2.3) off-screen collisions for the hidden player, inside GD's step
    void postCollision(float dt, bool betweenSteps) {
        s_sim.beforeSimPostCollision(this, dt);
        PlayerObject::postCollision(dt, betweenSteps);
    }
    // (2.3) step audit: your state at the end of each step (its last part)
    void updateRotation(float dt) {
        PlayerObject::updateRotation(dt);
        if (s_sim.isSimulating()) return;
        if (PlayLayer* pl = PlayLayer::get(); pl && this == pl->m_player1) s_sim.noteStepEnd(this);
    }
    static void onModify(auto& self) {
        (void)self.setHookPriority("PlayerObject::playSpiderDashEffect", -1000000);
        (void)self.setHookPriority("PlayerObject::incrementJumps", -1000000);
        (void)self.setHookPriority("PlayerObject::spawnCircle", -1000000);
        (void)self.setHookPriority("PlayerObject::spawnScaleCircle", -1000000);
        (void)self.setHookPriority("PlayerObject::spawnPortalCircle", -1000000);
        (void)self.setHookPriority("PlayerObject::ringJump", -1000000);
        (void)self.setHookPriority("PlayerObject::bumpPlayer", -1000000);
    }

    void playSpiderDashEffect(cocos2d::CCPoint from, cocos2d::CCPoint to) {
        if (s_sim.isSimulating()) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }

    void incrementJumps() {
        if (s_sim.isSimulating()) return;
        PlayerObject::incrementJumps();
    }

    // Circle effects a hidden sim player could draw (orb / portal / size): never
    // during a test (1.26.2 - a ghost circle showed up next to orbs).
    void spawnCircle() {
        if (s_sim.isSimulating()) return;
        PlayerObject::spawnCircle();
    }
    void spawnScaleCircle() {
        if (s_sim.isSimulating()) return;
        PlayerObject::spawnScaleCircle();
    }
    void spawnPortalCircle(cocos2d::ccColor3B color, float startRadius) {
        if (s_sim.isSimulating()) return;
        PlayerObject::spawnPortalCircle(color, startRadius);
    }

    void update(float dt) {
        if (s_sim.isSimulating()) {
            PlayerObject::update(dt);
            return;
        }
        PlayLayer* pl = PlayLayer::get();
        if (!pl) {
            PlayerObject::update(dt);
            return;
        }
        s_sim.setStepDt(dt); // (2.3) a whole step, or a piece of one (Click Between Frames)
        s_sim.onPlayerUpdate(pl, this);
        PlayerObject::update(dt);
        if (this == pl->m_player1) s_sim.setFrameDelta(pl, dt);
    }

    // Pads: during a test only sim players may be bumped; on a real hit, learn
    // the strength GD passes so the sim can apply the same pad later.
    void bumpPlayer(float bumpMod, int objectType, bool noEffects, GameObject* object) {
        if (s_sim.isSimulating()) {
            if (!s_sim.isSimPlayer(this)) return;
        } else {
            PlayLayer* pl = PlayLayer::get();
            if (pl && (this == pl->m_player1 || this == pl->m_player2)) s_sim.learnPad(objectType, bumpMod);
        }
        PlayerObject::bumpPlayer(bumpMod, objectType, noEffects, object);
    }

    void ringJump(RingObject* p0, bool p1) {
        // During a test only our sim players may use orbs (that's the orb prediction).
        if (s_sim.isSimulating() && !s_sim.isSimPlayer(this)) return;
        if (s_sim.isSimulating()) {
            PlayerObject::ringJump(p0, p1);
            return;
        }
        double const before = m_yVelocity;
        int const key = p0 ? TrajectoryBufferSim::orbKey(p0, this) : -1;
        bool const ud = m_isUpsideDown;
        PlayerObject::ringJump(p0, p1);
        s_sim.noteRealOrb(this, p0, p1, before, m_yVelocity, key, m_isUpsideDown != ud);
    }
};

class $modify(BufBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GJBaseGameLayer::canBeActivatedByPlayer", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::handleButton", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::collisionCheckObjects", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::playerTouchedRing", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::flipGravity", -1000000);
    }

    // Declared on GJBaseGameLayer (PlayLayer only inherits it).
    void flipGravity(PlayerObject* p0, bool p1, bool p2) {
        if (s_sim.isSimulating()) {
            // A sim player using a blue / green orb or gravity pad: flip only that
            // player, with no level effects. Nothing else may flip during a test.
            if (s_sim.isSimPlayer(p0)) p0->flipGravity(p1, true);
            return;
        }
        GJBaseGameLayer::flipGravity(p0, p1, p2);
    }

    // Once per rendered frame: feeds the frame-time meter used by the auto CPU budget.
    void update(float dt) {
        PlayLayer* pl = PlayLayer::get();
        bool const mine = pl && static_cast<GJBaseGameLayer*>(pl) == this;
        if (mine) {
            s_sim.preFrame(pl); // (2.3) the restart after autoplay's learning run
            s_sim.onFrame();
            s_sim.noteFrame(pl);
        }
        GJBaseGameLayer::update(dt);
        if (mine) s_sim.watchdog(pl);
    }

    bool canBeActivatedByPlayer(PlayerObject* p0, EffectGameObject* p1) {
        if (s_sim.isSimulating()) {
            // Orbs may be "touched" by sim players (exact orb contact); nothing
            // else may be activated during a test.
            if (s_sim.isSimPlayer(p0)) {
                if (auto* ring = typeinfo_cast<RingObject*>(p1)) {
                    if (GJBaseGameLayer::canBeActivatedByPlayer(p0, p1)) return true;
                    // GD's per-player "already activated" flags are set by an
                    // earlier test touching the orb and never put back, so only the
                    // FIRST test of a click could use an orb (1.25.0). The sim
                    // decides from what really happened instead: an orb you didn't
                    // use this attempt, and not the one this test already used.
                    return s_sim.simOrbAvailable(ring);
                }
            }
            return false;
        }
        return GJBaseGameLayer::canBeActivatedByPlayer(p0, p1);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (button == 1 && !s_sim.isSimulating() && !s_sim.isFiringBuffered()) {
            PlayLayer* pl = PlayLayer::get();
            if (pl && static_cast<GJBaseGameLayer*>(pl) == this) {
                if (s_sim.autoplayOn()) {
                    // (2.3) autoplay plays: your clicks are ignored - unless
                    // Your Clicks is on: then you take over (straight to the
                    // game) and autoplay learns your inputs for that stretch
                    if (!s_sim.overrideInput(pl, down)) return;
                    GJBaseGameLayer::handleButton(down, button, isPlayer1);
                    return;
                }
                if (!s_sim.interceptInput(pl, down, isPlayer1)) return;
                s_sim.noteInputForCheck();
                s_sim.noteRawInput(pl, down);
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* vec, int objectsCount, float dt) {
        if (s_sim.isSimulating()) {
            auto& filtered = s_sim.filterForSim(player, vec, objectsCount);
            GJBaseGameLayer::collisionCheckObjects(player, &filtered, static_cast<int>(filtered.size()), dt);
            return;
        }
        GJBaseGameLayer::collisionCheckObjects(player, vec, objectsCount, dt);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        if (s_sim.isSimulating()) {
            if (s_sim.isSimPlayer(player)) s_sim.simRingTouched(player, ring); // exact contact, nothing else
            return;
        }
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
};
