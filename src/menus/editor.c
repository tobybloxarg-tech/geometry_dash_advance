/* Experimental grid editor. No changes to the stock save structure. */
#include <tonc.h>
#include <string.h>
#include "main.h"
#include "level_select.h"
#include "editor.h"
#include "editor_core.h"
#include "tonc_tte.h"
#include "pusab.h"

/* Kept well above the game's SaveBlock (at offset zero) and below 32 KiB.
 * Save file should be backed up before trying this experimental build. */
#define EDITOR_SRAM_ADDR 0x6000
static EWRAM_DATA EditorLevel editor_level;
static EWRAM_DATA u8 editor_blob[ED_SAVE_BYTES];

static void editor_text(int x, int y, const char *text) {
    tte_set_pos(x,y); tte_write(text);
}
static void editor_draw(unsigned x, unsigned y, unsigned left, unsigned brush, const char *status) {
    char row[19];
    static const char symbols[] = "OHS";
    static const char *brush_names[] = {"EMPTY", "BLOCK", "SPIKE"};
    tte_erase_rect(0,0,240,160);
    editor_text(0,0,"EDITOR PROTOTYPE");
    editor_text(0,16,"L/R TOOL  A PLACE  B ERASE");
    for (unsigned r=0;r<ED_HEIGHT;r++) {
        for (unsigned c=0;c<16;c++) {
            int v=ed_get(&editor_level,left+c,r);
            row[c]=(x==left+c && y==r) ? 'X' : symbols[v];
        }
        row[16]=0;
        editor_text(0,32+(int)r*8,row);
    }
    editor_text(0,104,brush_names[brush]);
    editor_text(0,120,"START SAVE  SELECT EXIT");
    editor_text(0,136,status);
}
void editor_loop(void) {
    unsigned x=0,y=0,left=0,brush=ED_BLOCK;
    const char *status="NO PLAYTEST YET";
    /* Editor uses a separate checked SRAM record, never reset the main save. */
    sram_read(EDITOR_SRAM_ADDR,editor_blob,ED_SAVE_BYTES);
    if (!ed_decode(&editor_level,editor_blob)) ed_clear(&editor_level);
    REG_DISPCNT=DCNT_MODE0 | DCNT_BG2;
    REG_BG2HOFS=0; REG_BG2VOFS=0;
    memset32(palette_buffer,0,256);
    memcpy16(palette_buffer,menu_palette,sizeof(menu_palette)/sizeof(COLOR));
    memcpy32(pal_bg_mem,palette_buffer,256);
    tte_init_se(2,BG_CBB(0)|BG_SBB(27)|BG_REG_32x32|BG_PRIO(0),0,0,0,&pusabFont,NULL);
    tte_set_special(0x2000);
    memcpy32(&tile_mem[0][0],level_select_chr,sizeof(level_select_chr)/4);
    memset32(&se_mem[27][0],0,sizeof(SCREENBLOCK)/4);
    editor_draw(x,y,left,brush,status);
    while (1) {
        key_poll();
        if (key_hit(KEY_SELECT)) { game_state=STATE_TITLE_SCREEN; return; }
        if (key_hit(KEY_LEFT) && x>0) x--;
        if (key_hit(KEY_RIGHT) && x+1<ED_WIDTH) x++;
        if (key_hit(KEY_UP) && y>0) y--;
        if (key_hit(KEY_DOWN) && y+1<ED_HEIGHT) y++;
        if (key_hit(KEY_L)) brush=(brush+ED_KIND_COUNT-1)%ED_KIND_COUNT;
        if (key_hit(KEY_R)) brush=(brush+1)%ED_KIND_COUNT;
        if (key_hit(KEY_A)) { ed_put(&editor_level,x,y,brush); status="UNSAVED"; }
        if (key_hit(KEY_B)) { ed_put(&editor_level,x,y,ED_EMPTY); status="UNSAVED"; }
        if (key_hit(KEY_START)) {
            ed_encode(&editor_level,editor_blob);
            if (sram_write(EDITOR_SRAM_ADDR,editor_blob,ED_SAVE_BYTES)==0) {
                sram_read(EDITOR_SRAM_ADDR,editor_blob,ED_SAVE_BYTES);
                status=ed_decode(&editor_level,editor_blob) ? "SAVED" : "SAVE FAILED";
            } else status="SAVE FAILED";
        }
        if (x<left) left=x;
        if (x>=left+16) left=x-15;
        editor_draw(x,y,left,brush,status);
        VBlankIntrWait();
    }
}
