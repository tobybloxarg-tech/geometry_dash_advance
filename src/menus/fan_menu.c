#include <tonc.h>
#include <maxmod.h>
#include <string.h>
#include "main.h"
#include "level_select.h"
#include "fan_levels.h"
#include "editor_play.h"
#include "tonc_tte.h"
#include "pusab.h"

/* Build in working RAM only; ed_play_prepare takes its own private snapshot.
 * This area is never a user-editor level or SRAM bank. */
static EWRAM_DATA EditorLevel fan_work;
static unsigned selected;
static void fan_text(int x,int y,const char *text) { tte_set_pos(x,y); tte_write(text); }
static void draw_fan_menu(const char *error) {
    tte_erase_rect(0,0,240,160);
    fan_text(8,8,"NIVELES FAN 2");
    fan_text(8,24,"INSPIRADOS NO OFICIALES");
    fan_text(8,48,selected==0?"X GEOMETRICAL DOMINATOR":"  GEOMETRICAL DOMINATOR");
    fan_text(8,72,selected==1?"X DEADLOCKED":"  DEADLOCKED");
    fan_text(8,96,"VERSIONES CORTAS ORIGINALES");
    fan_text(8,112,selected ? "MUSICA BLAST PROCESSING" : "MUSICA STEREO MADNESS");
    fan_text(8,136,"A JUGAR  B TITULO");
    fan_text(8,152,error);
}
void fan_menu_loop(void) {
    const char *error="";
    REG_DISPCNT=DCNT_MODE0|DCNT_BG2;
    REG_BLDCNT=0; REG_MOSAIC=0;
    REG_WIN0H=REG_WIN0V=REG_WININ=REG_WINOUT=0;
    REG_BG2HOFS=REG_BG2VOFS=0;
    /* Title fades out using the old window limits. Full viewport is needed
     * even with window display disabled (also resets later game sessions). */
    REG_WIN0H=SCREEN_WIDTH; REG_WIN0V=SCREEN_HEIGHT;
    memset32(palette_buffer,0,256);
    memcpy16(palette_buffer,menu_palette,sizeof(menu_palette)/sizeof(COLOR));
    palette_buffer[0]=0x5000;
    palette_buffer[33]=0x7fff; palette_buffer[34]=0; palette_buffer[35]=0x5294;
    memcpy32(pal_bg_mem,palette_buffer,256);
    tte_init_se(2,BG_CBB(0)|BG_SBB(27)|BG_REG_32x32|BG_PRIO(0),0,0,0,&pusabFont,NULL);
    tte_set_special(0x2000);
    memset32(&se_mem[27][0],0,sizeof(SCREENBLOCK)/4);
    draw_fan_menu(error);
    while(1) {
        key_poll();
        if(key_hit(KEY_B)) { game_state=STATE_TITLE_SCREEN; return; }
        if(key_hit(KEY_UP)||key_hit(KEY_DOWN)) { selected^=1; error=""; draw_fan_menu(error); }
        if(key_hit(KEY_A)) {
            if(fan_level_build(&fan_work,selected) &&
               ed_play_configure(fan_work.settings.mode,fan_work.settings.speed,
                                 fan_work.settings.mini,fan_work.settings.song)) {
                editor_test_set_invincible(0);
                if(editor_play_start(&fan_work)) { fan_play_set_active(1); return; }
            }
            error="NO SE PUDO INICIAR"; draw_fan_menu(error);
        }
        VBlankIntrWait();
    }
}
