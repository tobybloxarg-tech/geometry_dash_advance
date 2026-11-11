#include "fan_levels.h"

/* Hand-authored original terrain. Row 7 is immediately above the floor.
 * NES Famidash was consulted for broad mode ordering, not copied: these
 * coordinates, obstacle choices and timings are new. Robot is cube; there
 * is no fake automatic teleport. Lengths are deliberately much shorter.
 * All portals sit at ground level, so players should enter them low. */
typedef struct { unsigned short x; unsigned char y, kind; } FanItem;
static const FanItem dominator[] = {
    {12,7,ED_SPIKE},{22,7,ED_SPIKE},{32,7,ED_SPIKE},{33,7,ED_SPIKE},
    {44,7,ED_BLOCK},{54,7,ED_SPIKE},{64,7,ED_SPIKE},{65,7,ED_SPIKE},
    /* ship: ascending/descending obstacles leave broad flight passages */
    {82,7,ED_SPIKE},{94,3,ED_SPIKE_DOWN},{104,7,ED_SPIKE},{114,3,ED_SPIKE_DOWN},
    {132,7,ED_SPIKE},{142,7,ED_SPIKE},{143,7,ED_SPIKE},{154,7,ED_BLOCK},
    {174,7,ED_SPIKE},{184,3,ED_SPIKE_DOWN},{194,7,ED_SPIKE},
    {216,7,ED_SPIKE},{226,7,ED_SPIKE},{227,7,ED_SPIKE},{239,7,ED_BLOCK},
    {249,7,ED_SPIKE},{258,7,ED_SPIKE},{259,7,ED_SPIKE},{270,7,ED_SPIKE}
};
static const FanItem deadlocked[] = {
    {10,7,ED_SPIKE},{19,7,ED_SPIKE},{20,7,ED_SPIKE},{31,7,ED_SPIKE},
    {41,7,ED_BLOCK},{50,7,ED_SPIKE},{51,7,ED_SPIKE},{62,7,ED_SPIKE},
    {78,7,ED_SPIKE},{88,3,ED_SPIKE_DOWN},{98,7,ED_SPIKE},{110,3,ED_SPIKE_DOWN},
    {130,7,ED_SPIKE},{140,7,ED_SPIKE},{141,7,ED_SPIKE},{151,7,ED_BLOCK},
    {173,7,ED_SPIKE},{182,3,ED_SPIKE_DOWN},{192,7,ED_SPIKE},{201,3,ED_SPIKE_DOWN},
    {218,7,ED_SPIKE},{228,7,ED_SPIKE},{229,7,ED_SPIKE},{240,7,ED_BLOCK},
    {249,7,ED_SPIKE},{250,7,ED_SPIKE},{261,7,ED_SPIKE},{272,7,ED_SPIKE}
};
static const FanItem portals[] = {
    {72,7,ED_SPR_SHIP}, {122,7,ED_SPR_CUBE},
    {164,7,ED_SPR_WAVE}, {208,7,ED_SPR_CUBE}
};
static int active;
int fan_play_active(void) { return active; }
void fan_play_set_active(int value) { active = !!value; }
int fan_level_build(EditorLevel *level, unsigned id) {
    const FanItem *items; unsigned count, i;
    if (!level || id >= FAN_LEVEL_COUNT) return 0;
    ed_clear(level);
    level->width = 288;
    level->settings.mode = ED_MODE_CUBE;
    level->settings.speed = ED_SPEED_X1;
    level->settings.song = id ? ED_SONG_BLASTP : ED_SONG_STEREOMA;
    if (id == 0) { items=dominator; count=sizeof dominator/sizeof *dominator; }
    else { items=deadlocked; count=sizeof deadlocked/sizeof *deadlocked; }
    for (i=0;i<count;i++) ed_put(level,items[i].x,items[i].y,items[i].kind);
    for (i=0;i<sizeof portals/sizeof *portals;i++)
        ed_sprite_put(level,portals[i].x,portals[i].y,portals[i].kind);
    return 1;
}
