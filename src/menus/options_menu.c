#include <tonc.h>
#include <string.h>
#include "main.h"
#include "level_select.h"
#include "game_options.h"
#include "tonc_tte.h"
#include "pusab.h"

static unsigned selected;
static void text(int x, int y, const char *s) { tte_set_pos(x,y); tte_write(s); }
static void draw_options(void) {
    tte_erase_rect(0,0,240,160);
    text(8,8,"OPCIONES");
    text(8,32,selected==0 ? "X GRAFICOS" : "  GRAFICOS");
    text(120,32,option_low_graphics ? "BAJOS" : "NORMALES");
    text(8,56,selected==1 ? "X FPS" : "  FPS");
    text(120,56,option_show_fps ? "SI" : "NO");
    text(8,80,"BAJOS: MENOS ESTELAS Y");
    text(8,88,"EFECTOS DE ACTIVACION");
    text(8,112,"SOLO ESTA SESION");
    text(8,136,"ARR/ABA ELEGIR A CAMBIAR");
    text(8,152,"B VOLVER AL TITULO");
}
void options_menu_loop(void) {
    REG_DISPCNT=DCNT_MODE0|DCNT_BG2;
    /* Menus may leave mosaic/colour special effects enabled. */
    REG_BLDCNT=0; REG_MOSAIC=0;
    REG_BG2HOFS=REG_BG2VOFS=0;
    /* Like the fan menu, leave the gameplay window fully open. */
    REG_WIN0H=SCREEN_WIDTH; REG_WIN0V=SCREEN_HEIGHT;
    REG_WININ=REG_WINOUT=0;
    memset32(palette_buffer,0,256);
    memcpy16(palette_buffer,menu_palette,sizeof(menu_palette)/sizeof(COLOR));
    palette_buffer[0]=0x5000;
    palette_buffer[33]=0x7fff; palette_buffer[34]=0; palette_buffer[35]=0x5294;
    memcpy32(pal_bg_mem,palette_buffer,256);
    tte_init_se(2,BG_CBB(0)|BG_SBB(27)|BG_REG_32x32|BG_PRIO(0),0,0,0,&pusabFont,NULL);
    tte_set_special(0x2000);
    memset32(&se_mem[27][0],0,sizeof(SCREENBLOCK)/4);
    draw_options();
    while (1) {
        key_poll();
        if (key_hit(KEY_B)) { game_state=STATE_TITLE_SCREEN; return; }
        if (key_hit(KEY_UP) || key_hit(KEY_DOWN)) { selected ^= 1; draw_options(); }
        if (key_hit(KEY_A) || key_hit(KEY_LEFT) || key_hit(KEY_RIGHT)) {
            if (selected==0) option_low_graphics ^= 1;
            else option_show_fps ^= 1;
            draw_options();
        }
        VBlankIntrWait();
    }
}
