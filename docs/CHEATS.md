# Cheats

*Ultima Underworld*'s port (UW1 only, for now) has cheats of its own, for players: changes to the game that you turn on and set while you play, on the **Cheats** screen. They are all off until you turn them on, and with them off the game plays exactly as the original. They are not the [test cheats](BUILDING.md#cheats-for-tests) that input scripts use, which a player's build refuses.

## Turning them on

Open the settings screen (F11, Cmd+, on macOS, or the gear on the web page). On its **Game** tab:

| Row | What it does |
|---|---|
| Cheats | The master switch. Off, every cheat plays as the original, and keeps its setting for when you turn it back on. Settings-file key `cheats` |
| Cheats... | Opens the Cheats screen, one tab for each kind of cheat. Esc comes back to the Game tab; F11 (Cmd+,) closes everything |

The game is paused while the settings screen is open, so a change takes effect when the screen closes. Every change is written to the settings file at once (`cheat-NAME=VALUE`), as the other settings are, and the next run starts with it.

## The cheats

### Travel

| Cheat | What it does | Settings-file key | Default |
|---|---|---|---|
| Ctrl+T map teleport | On the map, Ctrl+T leaves the map and puts you on the square under the pointer, on the level the map shows (levels 1 to 8; use the map's arrows to show another level). Turned on, it works from the next time the map opens | `cheat-map-teleport` | off |
| Ctrl+M map fill | On the map, Ctrl+M fills in the map of the level the map shows (levels 1 to 8), as if you had seen all of it, and draws it again at once. Turned on, it works from the next time the map opens | `cheat-map-fill` | off |

The point that counts is the pen's point, where a note would be written. On your own level, a square with no room for you (a wall, a crowd) moves you to the nearest square that has room, as a teleport trap does, and one with no room near it is refused. On another level, only a square you have seen is taken: that level is not loaded until you arrive, and the game kills a player it cannot place there. A refused square beeps and leaves the map open. The move itself is the game's own teleport, with its fade.

Ctrl+M fills in every square that is not solid rock, each as the 3D view marks a square it has seen in the light: its shape, water and lava shaded, and the marks of its doors, bridges and stairs (secret doors included, as seeing one marks it). Squares you have already seen are left as you saw them, and your notes stay. Your own level is filled from the level as it is now; another level from the game in progress as you last left it (or as it starts, if you have never been there). The filled map is kept as a map you drew is, in the game in progress and in the games you save. Two things to know: a filled square no longer counts as new when you first walk past it, so it gives none of the small experience the game awards for exploring; and the map's drawing takes random numbers, as it always does, so a game with a fill plays on differently from one without.

## Recordings and replays

Each session's recording (in `recordings/` in the home folder) holds every cheat change it was played with, so a recording of a session with cheats replays exactly, cheats and all:

- Each change is recorded when the game takes it: the pass of the game's main loop, and the text, `cfg NAME VALUE` for a setting or `act NAME` for a one-shot action. The master switch is `cfg cheats 1` or `cfg cheats 0`. The log says `cheat: TEXT (not as DOS)` for each.
- A session started with cheats already set in the settings file records them at its start, so its recording needs no settings file to replay.
- A change is recorded once it changes play. Turning the master switch on with every cheat at its default, or setting cheats while it is off, changes nothing yet, so nothing is recorded until a cheat takes effect, and then everything that changed is recorded at once.
- The first change recorded makes the recording **format 7** (Exhume's `runtime/replay/replay.c`). DOS cannot replay one, and ports from before format 7 refuse it rather than replay it without the cheats. A session in which no cheat took effect is recorded as before, and DOS can replay it as before.
- A replay plays the recording's cheats alone, starting from all off, whatever your settings file says. While it plays, the Cheats screen shows the rows but cannot change them.

`tools/replay.py` (Exhume's) reads format 7, and needs `EXHUME_TEST_CHEATS=1` for it only if a test's cheats are in it too. The mechanism is Exhume's: its `runtime/README.md`, "The player's live cheats".

## For tests

An input script sets a cheat as the Cheats screen does, at a time in milliseconds ([BUILDING.md](BUILDING.md#input-scripts)):

```
2000 cfg cheats 1               the master switch on
2100 cfg map-teleport 1         Ctrl+T on the map
2200 cfg map-fill 1             Ctrl+M on the map
```

The name is the cheat's (`src/port/sys/cheattab.c`), and the value is checked against its range when the script is read. These lines need no `EXHUME_TEST_CHEATS`: they do no more than a player can.

## Adding a cheat

1. A `CH_` entry in `uw1/src/port/portgame.h`, before `CH_COUNT`.
2. Its entry in `cheat_live_table` (`uw1/src/port/sys/cheattab.c`): its name, its neutral value (the game as DOS plays it) and its range; and its row in `cheat_rows`, with `.live = 1 + CH_...`, its tab, its settings-file key `cheat-NAME`, and an `.about` help line.
3. Where the game acts on it, in port-only code (`#ifndef __TURBOC__`): `cheat_live(CH_...)` gives its value, or the neutral one while the master switch is off. Read it only on the game's thread (the game's own code always is), and leave the game exactly as it was at the neutral value, random numbers drawn and all, so that every session recorded in DOS still replays.
4. This page.
