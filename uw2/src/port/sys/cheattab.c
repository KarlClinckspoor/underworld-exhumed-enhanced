/* cheattab.c: UW2's live cheats (as UW1's), the player's own (docs/CHEATS.md), and the rows of the Cheats
   screen that sets them. Two tables, kept in step by the CH_ enum (portgame.h):

   - cheat_live_table, for Exhume's runtime (sys/cheat.h): each cheat's name (what a recording
     and an input script's "cfg" line call it), its neutral value (the game as DOS plays it) and
     its range. Entry 0 is the master switch.
   - cheat_rows, for the settings screen (ui/settings.h): how each cheat is shown and changed, on
     which tab, under which key in the settings file (cheat-NAME). A row's live field is 1 + its
     CH_ index; that is what ties the row to the cheat.

   The master switch itself is not on the Cheats screen but on the main screen's Game tab, beside
   the row that opens the Cheats screen (settab.c), so that it can be seen and turned off without
   going in.

   How a change reaches the game, and why a recording replays it exactly, is Exhume's (its
   runtime/README.md, "The player's live cheats"): the screen posts it, the game's thread takes it
   at the next pass of its main loop and records it. So nothing here touches the game: the game's
   own code reads each value with cheat_live(CH_...) where it acts on it (Ctrl+T and Ctrl+M:
   ui/AUTOMAP.C). */
#include "portgame.h"
#include "sys/cheat.h"
#include "ui/settings.h"

/* the runtime's table, in the CH_ enum's order */
const struct cheat_live cheat_live_table[CH_COUNT] = {
    [CH_ON]           = { "cheats", 0, 0, 1, NULL },
    [CH_MAP_TELEPORT] = { "map-teleport", 0, 0, 1, NULL },
    [CH_MAP_FILL]     = { "map-fill", 0, 0, 1, NULL },
};

/* The Cheats screen's tabs. Only Travel has cheats yet; the plan's other tabs (Checks, Skill odds,
   Skill crits, Character, Survival) come with their cheats, so that no tab is ever empty. */
enum { TAB_TRAVEL };
const char *const cheat_tabs[] = { "Travel" };
const int cheat_tabs_count = (int)(sizeof cheat_tabs / sizeof cheat_tabs[0]);

const struct setting cheat_rows[] = {
    { .tab = TAB_TRAVEL, .label = "Ctrl+T map teleport", .kind = SET_BOOL, .key = "cheat-map-teleport",
      .def = 0, .live = 1 + CH_MAP_TELEPORT,
      .about = "On the map, Ctrl+T leaves the map and puts you on the square under the pointer, on the level "
               "shown, in any world. On another level, only a square you have seen. Takes effect the next "
               "time the map opens" },
    { .tab = TAB_TRAVEL, .label = "Ctrl+M map fill", .kind = SET_BOOL, .key = "cheat-map-fill",
      .def = 0, .live = 1 + CH_MAP_FILL,
      .about = "On the map, Ctrl+M fills in the map of the level shown, in any world, as if you had seen all "
               "of it, doors, bridges and stairs included. Squares filled give no explorer's experience later. "
               "Takes effect the next time the map opens" },
};
const int cheat_rows_count = (int)(sizeof cheat_rows / sizeof cheat_rows[0]);
