/* mods.c: replaces nothing in the DOS build. The settings a mod for the original game makes in
   its executable, given to the port instead. A mod for DOS changes bytes of the executable
   (KarlClinckspoor/UltimaUnderworldsModManager installs such mods), but the port runs its own
   compiled code and refuses any executable but the GOG release's (mem/fardata.c), so it takes
   them like this:

     start-square  The square a new game starts on, on level 1 (ui/MAINMENU.C's player_setup
                   call, which the executable's code holds as constants). --start-square X,Y
                   on the command line, for this run, or start-square=X,Y in the settings file;
                   1 to 62 each, "off" for the game's own.
     exe-patch     Byte patches to the executable's data, applied to the copy the port reads
                   at start-up (mem/fardata.c), after its CRC check: --exe-patch FILE, for this
                   run, or exe-patch=FILE in the settings file (a relative name is in the home
                   folder). FILE has a patch a line, as offsets in the GOG executable and hex
                   bytes: "0x4CCD4 0a0b 0c0d" is the two original bytes at 0x4CCD4 and the two
                   new ones; # starts a comment. Only bytes the port copies as the game's data
                   can be patched (port_exe_data_ranges: the far data segments and DGROUP);
                   the code is the port's own C. A patch elsewhere, or whose original bytes are
                   not there, stops the start with a message.
     --features    Prints what this build supports, one line each, and exits: for the mod
                   manager, which tells from it which mods this build can play.

   Both change play. A session played with either says so in its log ("mods: ... (not as DOS)"),
   and its recording keeps them in MODS.TXT beside RECORD.OUT (the black box's session folder,
   or the home folder for --record). --replay takes them from the MODS.TXT beside the recording
   it is given, and never from the settings file or the command line, so a recording replays as
   it was played. With neither set the game is exactly as before. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "port.h"
#include "sys/enhance.h"

int port_config_get(const char *home, const char *key, char *out, size_t outsz);
extern int16_t rp_request;
extern int16_t rp_blackbox;
extern const char *rp_blackbox_name;

#define MAX_PATCHES 4096
#define MAX_PATCH_BYTES 65536

static int start_x = -1, start_y = -1;          /* -1: the game's own square */
static const char *cl_start, *cl_patch;         /* the command line's, for this run */
static long patch_at[MAX_PATCHES];
static int patch_len[MAX_PATCHES];
static unsigned char patch_old[MAX_PATCH_BYTES], patch_new[MAX_PATCH_BYTES];
static int patch_off[MAX_PATCHES];              /* where each patch's bytes start in patch_old/new */
static int npatch, nbytes;
static char patch_source[1100];
static char mods_error[700];                    /* the last refusal, for main's message box */

/* A refusal: on stderr, and kept for port_mods_error. */
static int fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(mods_error, sizeof mods_error, fmt, ap);
    va_end(ap);
    fprintf(stderr, PORT_NAME ": mods: %s\n", mods_error);
    return -1;
}

const char *port_mods_error(void) { return mods_error[0] ? mods_error : NULL; }

void port_mods_cl_start(const char *v) { cl_start = v; }
void port_mods_cl_patch(const char *v) { cl_patch = v; }

/* ui/MAINMENU.C: the start square, x (axis 0) or y (axis 1), or the game's own (dflt) */
int port_start_square(int axis, int dflt)
{
    int v = axis ? start_y : start_x;
    return v < 0 ? dflt : v;
}

int port_mods_active(void) { return start_x >= 0 || npatch > 0; }

static int parse_start(const char *v, const char *where)
{
    int x, y;
    char extra;
    while (*v == ' ') v++;
    if (!*v || !strcmp(v, "off")) { start_x = start_y = -1; return 0; }
    if (sscanf(v, "%d , %d %c", &x, &y, &extra) != 2 || x < 1 || x > 62 || y < 1 || y > 62) {
        return fail("%s: the start square must be X,Y with each 1 to 62, or off, not \"%s\"", where, v);
    }
    start_x = x;
    start_y = y;
    return 0;
}

static int hexbytes(const char *s, unsigned char *out, int max)
{
    int n = 0;
    while (*s && !isspace((unsigned char)*s)) {
        int hi, lo;
        if (!isxdigit((unsigned char)s[0]) || !isxdigit((unsigned char)s[1]) || n >= max) return -1;
        hi = isdigit((unsigned char)s[0]) ? s[0] - '0' : tolower((unsigned char)s[0]) - 'a' + 10;
        lo = isdigit((unsigned char)s[1]) ? s[1] - '0' : tolower((unsigned char)s[1]) - 'a' + 10;
        out[n++] = (unsigned char)(hi << 4 | lo);
        s += 2;
    }
    return n;
}

/* One patch line ("0x4CCD4 0a0b 0c0d"); blank and # lines are skipped. */
static int parse_patch_line(char *line, const char *where, int lineno)
{
    char *p = line, *a, *b, *c;
    unsigned char o[256], w[256];
    int no, nw;
    long at;
    char *hash = strchr(p, '#');
    if (hash) *hash = 0;
    while (isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    a = strtok(p, " \t\r\n");
    b = strtok(NULL, " \t\r\n");
    c = strtok(NULL, " \t\r\n");
    if (!a || !b || !c || strtok(NULL, " \t\r\n")) goto bad;
    at = strtol(a, &p, 0);
    if (*p || at < 0) goto bad;
    no = hexbytes(b, o, sizeof o);
    nw = hexbytes(c, w, sizeof w);
    if (no <= 0 || no != nw) goto bad;
    if (npatch >= MAX_PATCHES || nbytes + no > MAX_PATCH_BYTES) {
        return fail("%s: more than %d patches or %d bytes", where, MAX_PATCHES, MAX_PATCH_BYTES);
    }
    patch_at[npatch] = at;
    patch_len[npatch] = no;
    patch_off[npatch] = nbytes;
    memcpy(patch_old + nbytes, o, no);
    memcpy(patch_new + nbytes, w, no);
    nbytes += no;
    npatch++;
    return 0;
bad:
    return fail("%s:%d: a patch is OFFSET ORIGINAL NEW, the bytes in hex and as many new as original", where, lineno);
}

static int read_patches(const char *path)
{
    char line[1200];
    int n = 0;
    FILE *f = fopen(path, "r");
    if (!f) return fail("cannot open the patch file %s", path);
    snprintf(patch_source, sizeof patch_source, "%s", path);
    while (fgets(line, sizeof line, f)) {
        n++;
        if (parse_patch_line(line, path, n)) { fclose(f); return -1; }
    }
    fclose(f);
    return 0;
}

/* A recording's MODS.TXT: "start-square X,Y" and "exe-patch OFFSET ORIGINAL NEW" lines. */
static int read_sidecar(const char *path, FILE *f)
{
    char line[1200];
    int n = 0;
    snprintf(patch_source, sizeof patch_source, "%s", path);
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        n++;
        while (isspace((unsigned char)*p)) p++;
        if (!*p || *p == '#') continue;
        if (!strncmp(p, "start-square ", 13)) {
            char *e = p + strlen(p);
            while (e > p && isspace((unsigned char)e[-1])) *--e = 0;
            if (parse_start(p + 13, path)) return -1;
        } else if (!strncmp(p, "exe-patch ", 10)) {
            if (parse_patch_line(p + 10, path, n)) return -1;
        } else {
            return fail("%s:%d: unknown line (a newer port's recording?)", path, n);
        }
    }
    return 0;
}

static void dir_of(const char *path, char *out, size_t n)
{
    const char *s = strrchr(path, '/'), *b = strrchr(path, '\\');
    if (b && (!s || b > s)) s = b;
    if (!s) snprintf(out, n, ".");
    else snprintf(out, n, "%.*s", (int)(s - path), path);
}

/* main, before the executable is read: the mods of this run. A replay's are its recording's
   (MODS.TXT beside it); otherwise the command line's, else the settings file's. */
int port_mods_init(const char *home, const char *replay)
{
    char v[1024], path[1200];
    if (replay) {
        FILE *f;
        if (cl_start || cl_patch) return fail("a replay takes its recording's mods; give no --start-square or --exe-patch");
        dir_of(replay, v, sizeof v);
        snprintf(path, sizeof path, "%s/MODS.TXT", v);
        f = fopen(path, "r");
        if (f) {
            int r = read_sidecar(path, f);
            fclose(f);
            if (r) return -1;
        }
    } else {
        if (cl_start) { if (parse_start(cl_start, "--start-square")) return -1; }
        else if (port_config_get(home, "start-square", v, sizeof v) == 0 && parse_start(v, "start-square in the settings file")) return -1;
        if (cl_patch) { if (read_patches(cl_patch)) return -1; }
        else if (port_config_get(home, "exe-patch", v, sizeof v) == 0 && v[0]) {
            if (v[0] == '/' || v[0] == '\\' || (v[0] && v[1] == ':')) snprintf(path, sizeof path, "%s", v);
            else snprintf(path, sizeof path, "%s/%s", home, v);
            if (read_patches(path)) return -1;
        }
    }
    if (port_mods_active()) {
        char sq[40] = "", pt[64] = "";
        if (start_x >= 0) snprintf(sq, sizeof sq, "start square (%d, %d)", start_x, start_y);
        if (npatch) snprintf(pt, sizeof pt, "%s%d bytes of " PORT_EXE_NAME " to patch", sq[0] ? "; " : "", nbytes);
        fprintf(stderr, PORT_NAME ": mods: %s%s (not as DOS)\n", sq, pt);
    }
    return 0;
}

/* mem/fardata.c's port_load_exe, after the CRC check and before the copies: the patches,
   each wholly in a range the port copies as data and finding its original bytes. */
int port_mods_patch_exe(unsigned char *exe, long n)
{
    long lo[8], hi[8];
    int nr = port_exe_data_ranges(lo, hi, 8), i, k;
    for (i = 0; i < npatch; i++) {
        long at = patch_at[i], end = at + patch_len[i];
        int ok = 0;
        for (k = 0; k < nr; k++) if (at >= lo[k] && end <= hi[k]) ok = 1;
        if (!ok || end > n)
            return fail("%s: the patch at 0x%lX is not in the executable's data, which is all the port can patch (--features lists the ranges)", patch_source, at);
        if (memcmp(exe + at, patch_old + patch_off[i], patch_len[i]))
            return fail("%s: the bytes at 0x%lX are not the patch's original ones (a patch for another release?)", patch_source, at);
    }
    for (i = 0; i < npatch; i++) memcpy(exe + patch_at[i], patch_new + patch_off[i], patch_len[i]);
    return 0;
}

/* main, once the recording's place is known: MODS.TXT beside it, so that its replay has the
   mods (none: a stale one in the home folder is removed, for --record). */
void port_mods_record(const char *home)
{
    char dir[1100], path[1200];
    FILE *f;
    int i, k;
    if (rp_request != 1) return;
    if (rp_blackbox && rp_blackbox_name) dir_of(rp_blackbox_name, dir, sizeof dir);
    else snprintf(dir, sizeof dir, "%s", home);
    snprintf(path, sizeof path, "%s/MODS.TXT", dir);
    if (!port_mods_active()) {
        if (!rp_blackbox) remove(path);
        return;
    }
    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, PORT_NAME ": mods: cannot write %s; the recording will replay without the mods\n", path);
        return;
    }
    fprintf(f, "# the mods this session was played with (" PORT_NAME " sys/mods.c); --replay applies them again\n");
    if (start_x >= 0) fprintf(f, "start-square %d,%d\n", start_x, start_y);
    for (i = 0; i < npatch; i++) {
        fprintf(f, "exe-patch 0x%lX ", patch_at[i]);
        for (k = 0; k < patch_len[i]; k++) fprintf(f, "%02x", patch_old[patch_off[i] + k]);
        fputc(' ', f);
        for (k = 0; k < patch_len[i]; k++) fprintf(f, "%02x", patch_new[patch_off[i] + k]);
        fputc('\n', f);
    }
    fclose(f);
}

/* --features: what this build plays, one line each ("NAME" or "NAME DETAILS"). */
void port_mods_features(FILE *f)
{
    long lo[8], hi[8];
    int nr = port_exe_data_ranges(lo, hi, 8), k;
    char b[600];
    fprintf(f, "features 1 " PORT_NAME "\n");
    fprintf(f, "start-square\n");
    fprintf(f, "exe-patch");
    for (k = 0; k < nr; k++) fprintf(f, " 0x%lX-0x%lX", lo[k], hi[k]);
    fprintf(f, "\n");
    fprintf(f, "data-overlay\n");               /* files in the home's DATA etc. are read before the game's (platform/files.c) */
    /* this game's enhancements (the kind masks leave out the other game's) */
    fprintf(f, "enhance %s\n", enhance_names(enhance_kind_mask(ENH_PRESENTATION) | enhance_kind_mask(ENH_TIMING) | enhance_kind_mask(ENH_GAMEPLAY), b, sizeof b));
}
