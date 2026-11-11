#include <tonc.h>
#include "main.h"
#include "game_options.h"

/* OBJ tile 1023 is unused by gameplay; 1022 is reserved for editor trace,
 * 1021 for debug hitboxes, 998..1007 for stock digits. Bank 5 is the
 * existing number palette. The 3x5 label fits one 8x8 tile. */
static const u16 fps_label[] = {
    ATTR0_4BPP | ATTR0_SQUARE, ATTR1_SIZE_8x8,
    ATTR2_PALBANK(5), 0, 0, 0, 0, 0xffff
};
void fps_graphics_init(void) {
    static const u8 rows[8] = {0, 0xdb, 0x59, 0xdb, 0x89, 0xc9, 0, 0};
    unsigned y,x;
    /* Do not modify the baseline 1023 tile when the overlay is disabled. */
    if (!option_show_fps) return;
    for (y=0;y<8;y++) {
        u32 data=0;
        for (x=0;x<8;x++) {
            int ink = (rows[y] & (1u<<x)) != 0;
            int border = (x && (rows[y] & (1u<<(x-1)))) ||
                         (x<7 && (rows[y] & (1u<<(x+1)))) ||
                         (y && (rows[y-1] & (1u<<x))) ||
                         (y<7 && (rows[y+1] & (1u<<x)));
            /* Existing OBJ bank 5: white index 2, black index 1. */
            if (ink) data |= 2u<<(4*x);
            else if (border) data |= 1u<<(4*x);
        }
        /* A TILEBLOCK holds 512 tiles; OBJ tile 1023 is block 1, tile 511. */
        tile_mem_obj[1][511].data[y]=data;
    }
}
void fps_draw(void) {
    int fps;
    if (!option_show_fps || complete_cutscene || nextSpr > 125) return;
    fps=fps_read();
    /* Small, fixed top-left HUD, away from the centred percentage. Reserve no
     * OAM slots before gameplay objects: the HUD is dropped if OAM is full. */
    oam_metaspr(8,8,fps_label,0,0,1023,-1,PRIORITY_DONT_DISABLE_0,0,1,1);
    if (fps>=0) {
        /* Always reserve two digit cells, even for a one-digit sample. */
        oam_metaspr(24,8,numberSpr,0,0,FIRST_NUMBER_ID+(unsigned)fps/10,-1,PRIORITY_DONT_DISABLE_0,0,1,1);
        oam_metaspr(32,8,numberSpr,0,0,FIRST_NUMBER_ID+(unsigned)fps%10,-1,PRIORITY_DONT_DISABLE_0,0,1,1);
    }
}
