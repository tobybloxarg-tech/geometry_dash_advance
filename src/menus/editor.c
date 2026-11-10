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
static EWRAM_DATA u8 editor_verify[ED_SAVE_BYTES];

static void editor_text(int x, int y, const char *text) {
    tte_set_pos(x,y); tte_write(text);
}
static void editor_draw(unsigned x, unsigned y, unsigned left, unsigned brush,
                        int screen, const char *status) {
    char row[17];
    static const char symbols[] = "OHS";
    static const char *brush_names[] = {"VACIO", "BLOQUE", "PINCHO"};
    tte_erase_rect(0,0,240,160);
    if (screen == ED_SCREEN_MENU) {
        editor_text(0,0,"MENU EDITOR");
        editor_text(0,16,"A NUEVO NIVEL");
        editor_text(0,32,"B VOLVER AL EDITOR");
        editor_text(0,48,"START SALIR AL TITULO");
        editor_text(0,80,"SALIR PIERDE LOS CAMBIOS");
        editor_text(0,96,"NO GUARDADOS");
        editor_text(0,120,"UN SOLO NIVEL GUARDADO");
        editor_text(0,136,"NIVELES NO JUGABLES");
        return;
    }
    if (screen == ED_SCREEN_CONFIRM_NEW) {
        editor_text(0,0,"NUEVO NIVEL?");
        editor_text(0,16,"A CONFIRMAR  B CANCELAR");
        editor_text(0,48,"BORRA EL NIVEL ACTUAL");
        editor_text(0,64,"Y CAMBIOS NO GUARDADOS");
        editor_text(0,96,"EL GUARDADO SIGUE IGUAL");
        editor_text(0,112,"HASTA PULSAR START");
        editor_text(0,128,"START LO REEMPLAZA");
        return;
    }
    editor_text(0,0,"EDITOR - NO JUGABLE");
    editor_text(0,16,"L/R TIPO  A PONER  B BORRAR");
    for (unsigned r=0;r<ED_HEIGHT;r++) {
        for (unsigned c=0;c<16;c++) {
            int v=ed_get(&editor_level,left+c,r);
            row[c]=(x==left+c && y==r) ? 'X' : symbols[v];
        }
        row[16]=0;
        editor_text(0,32+(int)r*8,row);
    }
    editor_text(0,104,brush_names[brush]);
    editor_text(0,120,"START GUARDA  SELECT NUEVO");
    editor_text(0,136,status);
}
void editor_loop(void) {
    unsigned x=0,y=0,left=0,brush=ED_BLOCK;
    int screen=ED_SCREEN_EDIT;
    const char *status="NO JUGABLE";
    /* Editor uses a separate checked SRAM record, never reset the main save. */
    if (sram_read(EDITOR_SRAM_ADDR,editor_blob,ED_SAVE_BYTES)!=0 ||
        !ed_decode(&editor_level,editor_blob)) ed_clear(&editor_level);
    REG_DISPCNT=DCNT_MODE0 | DCNT_BG2;
    REG_BG2HOFS=0; REG_BG2VOFS=0;
    memset32(palette_buffer,0,256);
    memcpy16(palette_buffer,menu_palette,sizeof(menu_palette)/sizeof(COLOR));
    memcpy32(pal_bg_mem,palette_buffer,256);
    tte_init_se(2,BG_CBB(0)|BG_SBB(27)|BG_REG_32x32|BG_PRIO(0),0,0,0,&pusabFont,NULL);
    tte_set_special(0x2000);
    memcpy32(&tile_mem[0][0],level_select_chr,sizeof(level_select_chr)/4);
    memset32(&se_mem[27][0],0,sizeof(SCREENBLOCK)/4);
    editor_draw(x,y,left,brush,screen,status);
    while (1) {
        unsigned input=0;
        int old_screen=screen, action;
        key_poll();
        if (key_hit(KEY_SELECT)) input|=ED_INPUT_SELECT;
        if (key_hit(KEY_A)) input|=ED_INPUT_A;
        if (key_hit(KEY_B)) input|=ED_INPUT_B;
        if (key_hit(KEY_START)) input|=ED_INPUT_START;
        action=ed_menu_input(&screen,input);
        if (action==ED_ACTION_EXIT) { game_state=STATE_TITLE_SCREEN; return; }
        if (action==ED_ACTION_NEW) {
            /* This only clears RAM. The one SRAM slot changes on START. */
            ed_clear(&editor_level);
            x=0; y=0; left=0; brush=ED_BLOCK;
            status="NUEVO SIN GUARDAR";
        }
        /* A/B/START on a modal must never also edit or save the grid. */
        if (old_screen==ED_SCREEN_EDIT && screen==ED_SCREEN_EDIT) {
            if (key_hit(KEY_LEFT) && x>0) x--;
            if (key_hit(KEY_RIGHT) && x+1<ED_WIDTH) x++;
            if (key_hit(KEY_UP) && y>0) y--;
            if (key_hit(KEY_DOWN) && y+1<ED_HEIGHT) y++;
            if (key_hit(KEY_L)) brush=(brush+ED_KIND_COUNT-1)%ED_KIND_COUNT;
            if (key_hit(KEY_R)) brush=(brush+1)%ED_KIND_COUNT;
            if (input & ED_INPUT_A) { ed_put(&editor_level,x,y,brush); status="SIN GUARDAR"; }
            if (input & ED_INPUT_B) { ed_put(&editor_level,x,y,ED_EMPTY); status="SIN GUARDAR"; }
            if (input & ED_INPUT_START) {
                ed_encode(&editor_level,editor_blob);
                if (sram_write(EDITOR_SRAM_ADDR,editor_blob,ED_SAVE_BYTES)==0 &&
                    sram_read(EDITOR_SRAM_ADDR,editor_verify,ED_SAVE_BYTES)==0 &&
                    memcmp(editor_blob,editor_verify,ED_SAVE_BYTES)==0)
                    status="GUARDADO";
                else status="ERROR AL GUARDAR";
            }
            if (x<left) left=x;
            if (x>=left+16) left=x-15;
        }
        editor_draw(x,y,left,brush,screen,status);
        VBlankIntrWait();
    }
}
