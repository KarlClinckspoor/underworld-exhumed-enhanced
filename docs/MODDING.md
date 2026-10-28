# Mods

Some mods for the original games change bytes of the executable (`UW.EXE`, `UW2.EXE`): the
square a new game starts on, or a table in the executable's data. The ports cannot take such an
executable: they run their own compiled code, and they check that the executable is the GOG
release's before reading its data (`src/port/mem/fardata.c`). So a mod manager gives the ports
the same settings another way. Everything here is port-only and off unless set; with nothing
set, the game is exactly as before.

Other files of a mod need nothing new: a file in the port's home folder is read in place of the
game's own (the home folder is laid over the game folder, Exhume's `runtime/port/platform/files.c`),
so a mod's `DATA\LEV.ARK`, `DATA\STRINGS.PAK` or pictures go in `DATA` in a home folder of its own,
started with `--home`. Its saved games stay there too.

## Start square

The square a new game starts on, always on level 1. The original sets it in the main menu's code,
`player_setup(0x20, 2, 1)` in UW1 and `player_setup(0x13, 0x30, -1)` in UW2 (`src/ui/MAINMENU.C`);
the port asks `port_start_square` there (`src/port/sys/mods.c`).

| Where | How |
|---|---|
| Command line | `--start-square X,Y` (this run only) |
| Settings file | `start-square=X,Y` in `uw1port.cfg` / `uw2port.cfg` |

X and Y are 1 to 62; `off` (or nothing) is the game's own square. The player is put in the middle
of the square, on its floor, as the original does.

## Executable data patches

Byte patches to the executable's data, applied to the copy the port reads at start-up, after its
check of the executable:

| Where | How |
|---|---|
| Command line | `--exe-patch FILE` (this run only) |
| Settings file | `exe-patch=FILE`, a name relative to the home folder or a full path |

`FILE` has one patch a line: the offset in the GOG executable (`0x` for hex), the original bytes
and the new bytes, as hex; `#` starts a comment.

```
# two bytes of the 3D models' table
0x4CCD4 0a0b 0c0d
```

Only the bytes the port copies as the game's data can be patched: the far data segments and
DGROUP (`port_exe_data_ranges` in `src/port/mem/fardata.c`; `--features` prints the ranges). The
game's code is the port's own C, so a change to a code byte would change nothing reliably and is
refused. A patch outside those ranges, or whose original bytes are not in the executable (a
patch made for another release), stops the start with a message saying which.

## What a build supports

`--features` prints one line for each thing this build plays and exits, for a mod manager:

```
features 1 uw1port
start-square
exe-patch 0x3C7B0-0x58FC4 0x59440-0x59EF0 0x5DCC0-0x65700
data-overlay
enhance skip-intro,wrap-menu,...
```

`exe-patch` lists the patchable ranges of the executable as `start-end` (end excluded),
`data-overlay` says that files in the home folder's `DATA` and the other game folders are read in
place of the game's, and `enhance` lists this game's enhancements.

## Recordings

Both settings change play, so a session played with either says so in its log
(`mods: start square (33, 5); 2 bytes of UW.EXE to patch (not as DOS)`) and its recording keeps
them: `MODS.TXT` beside `RECORD.OUT` (in the black box's session folder, or in the home folder
for `--record`):

```
start-square 33,5
exe-patch 0x5DDC0 0000 ffff
```

`--replay` reads the `MODS.TXT` beside the recording it is given, and never the settings file or
the command line (giving `--start-square` or `--exe-patch` with `--replay` is refused), so a
recording replays as it was played. A recording made without mods has no `MODS.TXT` and replays
as before; DOS replays it as before too.
