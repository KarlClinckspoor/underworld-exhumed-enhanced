/* The settings screen's table (ui/settings.h): every row of the Sound, Controls, Display and Game
   tabs; the Enhancements tab is generated from enhtab.c. Rows keep to the settings file (home
   directory) by their key, except the sound cards, which DATA\UW.CFG holds (uwcfg.c's read_uw_cfg
   and write_uw_cfg). `--settings-list` prints the table, for tools/setcheck.py. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "port.h"
#include "plat.h"
#include "sound/audio.h"
#include "sound/mt32roms.h"
#include "sound/altmusic.h"
#include "ui/settings.h"

void mouse_look_speed(int pct);             /* mousedrv.c */
void read_uw_cfg(int *music, int *speech);  /* uwcfg.c */
int write_uw_cfg(const char *spec);
extern const char *port_home;

/* a row with no tab on the web, where it cannot work: a row whose tab is -1 is on no tab, never
   drawn or reached. Window scale (the browser's window is not the port's to size); Game folder and
   MT-32 ROMs (a browser has no host folders to pick: the page gives the port its packed files);
   Fullscreen, which the page's own button does: a browser enters full screen only from the page's
   handler of a click or key, and the settings screen's input reaches the port later, from SDL's
   queue on the next frame, so the row could only fail (and SDL would make the canvas full screen
   without the page's buttons over it). */
#ifdef __EMSCRIPTEN__
#define WEB_HIDDEN(tab) (-1)
#else
#define WEB_HIDDEN(tab) (tab)
#endif

/* The rows, in each tab's order; the display rows read each other's values */
enum { R_MUSIC, R_SPEECH, R_ROMS, R_VOLUME, R_REPLACE, R_REPLACE_DIR, R_REPLACE_VOL, R_MOUSE, R_LOOK, R_FULL, R_SCALE, R_ASPECT, R_INTEGER, R_FOLDER, R_RECORD, R_START };

/* Window scale offers only the scales whose window fits the display (with 4:3 as it stands) */
static int scale_limit(void)
{
    return plat_max_scale(settings_value(R_ASPECT));
}

static void apply_display(int unused)
{
    (void)unused;
    plat_set_display(settings_value(R_FULL), settings_value(R_SCALE) + 1, settings_value(R_ASPECT), settings_value(R_INTEGER));
}

/* The window's scale, a cycle and not a slider: dragging a slider that resizes the window would
   move the row under the pointer. The row's value is the scale less one. */
static const char *const scale_names[] = { "1x", "2x", "3x", "4x", "5x", "6x", "7x", "8x", NULL };
static const char *const scale_stored[] = { "1", "2", "3", "4", "5", "6", "7", "8", NULL };

static void apply_mouse(int v) { plat_set_mouse_lock(v); }

/* The sound cards: the cycle's index is a place in the list, the file's number is the card's (the
   lists below). Either may be -1 for "no DATA\UW.CFG yet", which the screen shows as its default. */
static const char *const music_names[] = { "None", "PC speaker", "Ad Lib", "Sound Blaster", "Sound Blaster Pro", "Pro Audio Spectrum", "Roland MT-32", NULL };
static const char *const music_cards[] = { "0", "1", "2", "3", "4", "5", "6", NULL };
static const char *const speech_names[] = { "None", "Sound Blaster", "Sound Blaster Pro", "Pro Audio Spectrum", NULL };
static const char *const speech_cards[] = { "0", "1", "2", "3", NULL };

static int card_index(const char *const *cards, int card)
{
    int i;
    for (i = 0; cards[i]; i++)
        if (atoi(cards[i]) == card) return i;
    return -1;
}

static void write_cards(int music, int speech)
{
    char spec[16];
    snprintf(spec, sizeof spec, "%d,%d", music, speech);
    write_uw_cfg(spec);
    if (port_home) port_config_set(port_home, "sound-auto", "0");    /* the player has chosen: the port does not */
}

static int music_get(void)
{
    int m, s;
    read_uw_cfg(&m, &s);
    return card_index(music_cards, m);
}

static void music_put(int index)
{
    int m, s;
    read_uw_cfg(&m, &s);
    write_cards(atoi(music_cards[index]), s >= 0 ? s : 1);
}

static int speech_get(void)
{
    int m, s;
    read_uw_cfg(&m, &s);
    return card_index(speech_cards, s);
}

static void speech_put(int index)
{
    int m, s;
    read_uw_cfg(&m, &s);
    write_cards(m >= 0 ? m : 3, atoi(speech_cards[index]));
}

static int roms_check(const char *path) { return !mt32roms_pick(path, NULL); }

/* The ROMs row shows whether the ROMs are there, not the folder: "found" when the folder the run
   started with (main.c's search, mt32roms_locate: given, the variable, the remembered folder or the
   search's) held a pair, or when a folder chosen on the screen does; else "not found". The font
   has ASCII only, so no tick. A folder is read only when the row's text changes. */
static const char *start_roms;          /* the folder the run found, or NULL */

void port_settings_roms(const char *dir)
{
    char kept[1024];
    start_roms = dir && *dir ? dir : NULL;
    /* ROMs the port found itself, with no folder in the settings file: the row holds where they are,
       so that its folder picker starts there */
    if (start_roms && port_config_get(port_home, "mt32-roms", kept, sizeof kept) != 0) settings_set_path(R_ROMS, start_roms);
}

static const char *roms_show(const char *path)
{
    static char last[1024];
    static const char *word;
    if (word && !strcmp(last, path)) return word;
    snprintf(last, sizeof last, "%s", path);
    word = start_roms || (*path && mt32roms_pick(path, NULL)) ? "found" : "not found";
    fprintf(stderr, "settings: MT-32 ROMs: %s\n", word);
    return word;
}
static int game_check(const char *path) { return port_game_dir_ok(path); }

/* Replacement music (Exhume's sound/altmusic.c; docs/MUSIC.md): the player's Ogg Vorbis files, one a
   theme, played in place of the game's music. The themes are named by their music files,
   SOUND\UWnn.XMI (nn two octal digits: themes 1 to 13, uw01 to uw15), so a file is uwnn.ogg; the
   FM cards' own arrangements, AWnn.XMI, take awnn.ogg first when there is one (SOUND.C's
   load_new_music). The folder is the Music folder row's, or by default a folder called music in
   the home folder; the row shows how many of the themes it has a file for. */
static const char *const uw1_themes[] = { "uw01", "uw02", "uw03", "uw04", "uw05", "uw06", "uw07",
                                          "uw10", "uw11", "uw12", "uw13", "uw14", "uw15" };
static const char *const uw1_fm_themes[] = { "aw01", "aw02", "aw03", "aw04", "aw05", "aw06", "aw07",
                                             "aw10", "aw11", "aw12", "aw13", "aw14", "aw15" };
#define NTHEMES ((int)(sizeof uw1_themes / sizeof uw1_themes[0]))

/* The folder the files are looked for in: the row's, or the home folder's music */
static const char *music_folder(const char *path)
{
    static char def[1100];
    if (path && *path) return path;
    snprintf(def, sizeof def, "%s/music", port_home ? port_home : ".");
    return def;
}

/* Any folder is taken (the row then says how many themes it has): the music plays from it at the
   next theme the game starts */
static int music_check(const char *path)
{
    altmusic_set_dir(music_folder(path));
    return 0;
}

/* "N of 13 themes" (a theme counts by either of its names): counted again when the folder shown
   changes, or two seconds after the last count, as the screen draws it every frame and the player
   may copy files in while it is open */
static const char *music_show(const char *path)
{
    static char last[1100] = "\001", word[40];
    static time_t when;
    const char *d = music_folder(path);
    int n;
    if (!strcmp(last, d) && time(NULL) - when < 2) return word;
    snprintf(last, sizeof last, "%s", d);
    when = time(NULL);
    if (!altmusic_available()) snprintf(word, sizeof word, "not in this build");
    else if ((n = altmusic_count(d)) > 0) snprintf(word, sizeof word, "%d of %d themes", n, NTHEMES);
    else snprintf(word, sizeof word, "%s", *path ? "no themes found" : "none (home/music)");
    return word;
}

static const char *const mouse_names[] = { "Follow", "Lock", NULL };
static const char *const mouse_stored[] = { "follow", "lock", NULL };

const struct setting port_settings[] = {
    [R_MUSIC]   = { .tab = SET_TAB_SOUND, .label = "Music", .kind = SET_CYCLE, .names = music_names, .stored = music_cards,
                    .def = 3, .restart = 1, .get = music_get, .put = music_put },
    [R_SPEECH]  = { .tab = SET_TAB_SOUND, .label = "Speech", .kind = SET_CYCLE, .names = speech_names, .stored = speech_cards,
                    .def = 1, .restart = 1, .get = speech_get, .put = speech_put },
    [R_ROMS]    = { .tab = WEB_HIDDEN(SET_TAB_SOUND), .label = "MT-32 ROMs", .kind = SET_FOLDER, .key = "mt32-roms", .restart = 1,
                    .check = roms_check, .refuse = "That folder does not hold MT-32 or CM-32L ROMs",
                    .show = roms_show },
    [R_VOLUME]  = { .tab = SET_TAB_SOUND, .label = "Volume", .kind = SET_SLIDER, .key = "volume", .lo = 0, .hi = 100, .step = 10,
                    .def = 100, .apply = audio_set_volume },
    /* replacement music: off by the switch, or with no files; the volume is the files' alone */
    [R_REPLACE] = { .tab = WEB_HIDDEN(SET_TAB_SOUND), .label = "Replacement music", .kind = SET_BOOL, .key = "music-replace",
                    .def = 1, .apply = altmusic_set_enabled },
    [R_REPLACE_DIR] = { .tab = WEB_HIDDEN(SET_TAB_SOUND), .label = "Music folder", .kind = SET_FOLDER, .key = "music-dir",
                    .check = music_check, .show = music_show },
    [R_REPLACE_VOL] = { .tab = WEB_HIDDEN(SET_TAB_SOUND), .label = "Replacement volume", .kind = SET_SLIDER, .key = "music-volume",
                    .lo = 0, .hi = 100, .step = 10, .def = 70, .apply = altmusic_set_volume },
    [R_MOUSE]   = { .tab = SET_TAB_CONTROLS, .label = "Mouse", .kind = SET_CYCLE, .key = "mouse", .names = mouse_names,
                    .stored = mouse_stored, .def = 0, .apply = apply_mouse },
    [R_LOOK]    = { .tab = SET_TAB_CONTROLS, .label = "Mouse-look speed", .kind = SET_SLIDER, .key = "look-speed", .lo = 10,
                    .hi = 400, .step = 10, .def = 100, .apply = mouse_look_speed },
    [R_FULL]    = { .tab = WEB_HIDDEN(SET_TAB_DISPLAY), .label = "Fullscreen", .kind = SET_BOOL, .key = "fullscreen", .def = 0,
                    .apply = apply_display },
    [R_SCALE]   = { .tab = WEB_HIDDEN(SET_TAB_DISPLAY), .label = "Window scale", .kind = SET_CYCLE, .key = "scale", .names = scale_names,
                    .stored = scale_stored, .def = 2, .apply = apply_display, .limit = scale_limit },
    [R_ASPECT]  = { .tab = SET_TAB_DISPLAY, .label = "4:3 aspect", .kind = SET_BOOL, .key = "aspect", .def = 1,
                    .apply = apply_display },
    [R_INTEGER] = { .tab = SET_TAB_DISPLAY, .label = "Whole-number scaling", .kind = SET_BOOL, .key = "integer", .def = 1,
                    .apply = apply_display },
    [R_FOLDER]  = { .tab = WEB_HIDDEN(SET_TAB_GAME), .label = "Game folder", .kind = SET_FOLDER, .key = "data", .restart = 1,
                    .check = game_check, .refuse = "That folder does not hold the game" },
    [R_RECORD]  = { .tab = SET_TAB_GAME, .label = "Record sessions", .kind = SET_BOOL, .key = "recording", .def = 1,
                    .restart = 1 },
    [R_START]   = { .tab = SET_TAB_GAME, .label = "Show this at start", .kind = SET_BOOL, .key = "settings-at-start", .def = 1 },
};
const int port_settings_count = (int)(sizeof port_settings / sizeof port_settings[0]);

static void join(const char *const *v, char *out, size_t n)
{
    size_t len = 0;
    int i;
    out[0] = 0;
    for (i = 0; v && v[i]; i++) len += (size_t)snprintf(out + len, len < n ? n - len : 0, "%s%s", i ? "|" : "", v[i]);
}

/* One line a row, tab separated: tab, kind, key (- for the sound cards), label, def, lo, hi, step,
   names, stored (the card numbers, for the cards). */
void port_settings_list(FILE *f)
{
    int i;
    char names[256], stored[256];
    for (i = 0; i < port_settings_count; i++) {
        const struct setting *s = &port_settings[i];
        join(s->names, names, sizeof names);
        join(s->stored, stored, sizeof stored);
        fprintf(f, "%d\t%d\t%s\t%s\t%d\t%d\t%d\t%d\t%s\t%s\n", s->tab, s->kind, s->key ? s->key : "-", s->label,
                s->def, s->lo, s->hi, s->step, names, stored);
    }
}

/* The run's start from the settings file, as the screen read it (a bad value is the row's
   default, as the screen shows it): the display options into the pointers given (NULL for one the
   command line gave), the volume and the mouse-look speed applied. */
void port_settings_start(int *fullscreen, int *scale, int *aspect, int *integer_scale)
{
#ifdef __EMSCRIPTEN__
    /* the row is hidden on the web (above): a fullscreen=1 kept by an earlier page is not used */
    if (fullscreen) *fullscreen = 0;
#else
    if (fullscreen) *fullscreen = settings_value(R_FULL);
#endif
    if (scale) *scale = settings_value(R_SCALE) + 1;
    if (aspect) *aspect = settings_value(R_ASPECT);
    if (integer_scale) *integer_scale = settings_value(R_INTEGER);
    audio_set_volume(settings_value(R_VOLUME));
    {
        /* replacement music: the game's theme names, the folder (the file's, else home/music), on
           or off and the volume, as the screen read them */
        char dir[1024];
        altmusic_set_themes(uw1_themes, uw1_fm_themes, NTHEMES);
        altmusic_set_dir(music_folder(port_home && port_config_get(port_home, "music-dir", dir, sizeof dir) == 0 ? dir : NULL));
        altmusic_set_enabled(settings_value(R_REPLACE));
        altmusic_set_volume(settings_value(R_REPLACE_VOL));
    }
    mouse_look_speed(settings_value(R_LOOK));
}

/* The values this run really has (the command line over the settings file), for the screen to show
   and to apply with; nothing is written to the file. */
void port_settings_seed(int fullscreen, int scale, int aspect, int integer_scale, int lock)
{
    settings_set_value(R_FULL, fullscreen != 0);
    settings_set_value(R_SCALE, (scale < 1 ? 1 : scale > 8 ? 8 : scale) - 1);
    settings_set_value(R_ASPECT, aspect != 0);
    settings_set_value(R_INTEGER, integer_scale != 0);
    settings_set_value(R_MOUSE, lock != 0);
}
