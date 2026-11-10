/* Experimental visual metatile editor. Separate, checked SRAM; not a game loader. */
#include <tonc.h>
#include <string.h>
#include "posprintf.h"
#include "main.h"
#include "level_select.h"
#include "editor.h"
#include "editor_core.h"
#include "metatiles.h"
#include "tonc_tte.h"
#include "pusab.h"

#define EDITOR_SRAM_ADDR 0x6000
#define VIEW_W 14
#define VIEW_H 6
/* Compile-time overlap guard against stock SaveBlock growth. */
typedef char editor_sram_after_save[(sizeof(struct SaveBlock) <= EDITOR_SRAM_ADDR) ? 1 : -1];
static EWRAM_DATA EditorLevel editor_level;
static EWRAM_DATA u8 editor_blob[ED_SAVE_BYTES];
static EWRAM_DATA u8 editor_verify[ED_SAVE_BYTES];
/* Legacy IDs 1,2 remain block and spike in every version. The order below is
 * visual category order, not a contiguous metatile range. */
static const u8 brushes[] = { ED_BLOCK, ED_DARK, ED_CHECK, ED_SLAB_UP,
    ED_SLAB_DOWN, ED_SLAB_LEFT, ED_SLAB_RIGHT, ED_SPIKE, ED_SPIKE_DOWN,
    ED_SPIKE_RIGHT, ED_SPIKE_LEFT, ED_SMALL_UP, ED_SMALL_DOWN,
    ED_MEDIUM_UP, ED_MEDIUM_DOWN };
#define BRUSH_COUNT (sizeof(brushes)/sizeof(brushes[0]))
static const char *names[] = {"VACIO","BLOQUE","PINCHO","BLOQUE OSCURO",
    "BLOQUE CUADROS","LOSA ARRIBA","LOSA ABAJO","LOSA IZQ","LOSA DER",
    "PINCHO ABAJO","PINCHO DER","PINCHO IZQ","PINCHO CHICO","CHICO INV",
    "PINCHO MEDIO","MEDIO INV"};
static void text(int x,int y,const char *s) { tte_set_pos(x,y); tte_write(s); }
/* CBB1 holds first 128 real game tiles; the editor's grid tile is #127. */
static void metatile_at(unsigned col, unsigned row, unsigned kind) {
    const SCR_ENTRY *m=metatiles[ed_metatile[kind]];
    unsigned base=row*32+col;
    if (kind==ED_EMPTY) {
        se_mem[28][base]=0x107f; se_mem[28][base+1]=0x107f;
        se_mem[28][base+32]=0x107f; se_mem[28][base+33]=0x107f;
    } else {
        se_mem[28][base]=m[0]; se_mem[28][base+1]=m[1];
        se_mem[28][base+32]=m[2]; se_mem[28][base+33]=m[3];
    }
}
/* Transparent 16x16 outline overlaid independently of object artwork. */
static void outline(unsigned col,unsigned row) {
    unsigned b=row*32+col;
    se_mem[29][b]=0x1000; se_mem[29][b+1]=0x1001;
    se_mem[29][b+32]=0x1002; se_mem[29][b+33]=0x1003;
}
static void draw(unsigned x,unsigned y,unsigned left,unsigned top,unsigned selected,
                 int screen,const char *status) {
    char line[32];
    memset32(&se_mem[28][0],0,sizeof(SCREENBLOCK)/4);
    /* Tile 4 is fully transparent, so 0..3 can be the cursor quadrants. */
    memset32(&se_mem[29][0],0x00040004,sizeof(SCREENBLOCK)/4);
    tte_erase_rect(0,0,240,160);
    if (screen==ED_SCREEN_MENU) {
        text(0,0,"MENU EDITOR"); text(0,16,"A NUEVO NIVEL");
        text(0,32,"B VOLVER AL EDITOR"); text(0,48,"START SALIR AL TITULO");
        text(0,80,"SALIR PIERDE LOS CAMBIOS"); text(0,96,"NO GUARDADOS");
        text(0,120,"UN SOLO NIVEL GUARDADO"); text(0,136,"NIVELES NO JUGABLES"); return;
    }
    if (screen==ED_SCREEN_CONFIRM_NEW) {
        text(0,0,"NUEVO NIVEL?"); text(0,16,"A CONFIRMAR B CANCELAR");
        text(0,48,"BORRA EL NIVEL ACTUAL"); text(0,64,"Y CAMBIOS NO GUARDADOS");
        text(0,96,"EL GUARDADO SIGUE IGUAL"); text(0,112,"HASTA PULSAR START"); return;
    }
    posprintf(line,"EDITOR X%02d Y%d NO JUGABLE",x+1,y+1); text(0,0,line);
    for(unsigned r=0;r<VIEW_H;r++) for(unsigned c=0;c<VIEW_W;c++)
        metatile_at(c*2,2+r*2,(unsigned)ed_get(&editor_level,left+c,top+r));
    outline((x-left)*2,2+(y-top)*2);
    text(0,112,names[brushes[selected]]);
    /* Visible category strip, up to seven actual 16x16 metatiles per page. */
    unsigned start=(selected/7)*7;
    for(unsigned i=0;i<7 && start+i<BRUSH_COUNT;i++)
        metatile_at(i*3,16,brushes[start+i]);
    outline((selected-start)*3,16);
    text(0,144,"L/R TIPO A PONER B BORRAR"); text(0,152,status);
}
void editor_loop(void) {
    unsigned x=0,y=0,left=0,top=0,selected=0;
    int screen=ED_SCREEN_EDIT, locked=0; const char *status="START GUARDAR SELECT MENU";
    if (sram_read(EDITOR_SRAM_ADDR,editor_blob,ED_SAVE_BYTES)!=0) {
        ed_clear(&editor_level); locked=1; status="ERROR SRAM NO GUARDA";
    } else if (!ed_decode(&editor_level,editor_blob)) {
        /* Never overwrite an unknown/newer/corrupt nonempty record. */
        unsigned i; int allzero=1, allff=1;
        for(i=0;i<ED_SAVE_BYTES;i++) { if(editor_blob[i]) allzero=0; if(editor_blob[i]!=0xff) allff=0; }
        ed_clear(&editor_level);
        if(!allzero && !allff) { locked=1; status="SRAM DESCONOCIDA BLOQUEADA"; }
    }
    REG_DISPCNT=DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_BG2;
    REG_BLDCNT=0; REG_MOSAIC=0;
    REG_BG0CNT=BG_CBB(2)|BG_SBB(29)|BG_REG_32x32|BG_PRIO(0);
    REG_BG1CNT=BG_CBB(1)|BG_SBB(28)|BG_REG_32x32|BG_PRIO(2);
    REG_BG0HOFS=REG_BG0VOFS=REG_BG1HOFS=REG_BG1VOFS=REG_BG2HOFS=REG_BG2VOFS=0;
    memset32(palette_buffer,0,256);
    memcpy16(palette_buffer,blockPalette,16);
    memcpy16(&palette_buffer[32],&menu_palette[32],16);
    palette_buffer[0]=0x5000; palette_buffer[31]=0x7fff;
    memcpy32(pal_bg_mem,palette_buffer,256);
    tte_init_se(2,BG_CBB(0)|BG_SBB(27)|BG_REG_32x32|BG_PRIO(1),0,0,0,&pusabFont,NULL);
    tte_set_special(0x2000);
    memcpy32(&tile_mem[1][0],blockset,128*sizeof(TILE)/4);
    /* Fine blue grid: a line at the top/left of each 8x8 tile. */
    for(unsigned r=0;r<8;r++) tile_mem[1][127].data[r]=(r==0)?0x11111111:0x11111112;
    memset32(&tile_mem[2][0],0,5*sizeof(TILE)/4);
    /* Colour 15 in palette bank 1 is the cursor, indices reflect GBA nibble order. */
    for(unsigned r=0;r<8;r++) {
        tile_mem[2][0].data[r]=(r==0)?0xffffffff:0x0000000f;
        tile_mem[2][1].data[r]=(r==0)?0xffffffff:0xf0000000;
        tile_mem[2][2].data[r]=(r==7)?0xffffffff:0x0000000f;
        tile_mem[2][3].data[r]=(r==7)?0xffffffff:0xf0000000;
    }
    /* Cursor tiles use palette 1. */
    palette_buffer[17]=0x5400; palette_buffer[18]=0x4000; palette_buffer[31]=0x03ff;
    memcpy32(pal_bg_mem,palette_buffer,256);
    memset32(&se_mem[27][0],0,sizeof(SCREENBLOCK)/4);
    VBlankIntrWait(); draw(x,y,left,top,selected,screen,status);
    while(1) {
        unsigned input=0; int old=screen,action;
        unsigned old_x=x,old_y=y,old_selected=selected; const char *old_status=status;
        key_poll();
        if(key_hit(KEY_SELECT)) input|=ED_INPUT_SELECT;
        if(key_hit(KEY_A)) input|=ED_INPUT_A;
        if(key_hit(KEY_B)) input|=ED_INPUT_B;
        if(key_hit(KEY_START)) input|=ED_INPUT_START;
        action=ed_menu_input(&screen,input);
        if(action==ED_ACTION_EXIT) { game_state=STATE_TITLE_SCREEN; return; }
        if(action==ED_ACTION_NEW) { ed_clear(&editor_level); x=y=left=top=selected=0; status=locked?"SRAM BLOQUEADA":"NUEVO SIN GUARDAR"; }
        if(old==ED_SCREEN_EDIT && screen==ED_SCREEN_EDIT) {
            if(key_hit(KEY_LEFT)&&x>0) x--;
            if(key_hit(KEY_RIGHT)&&x+1<ED_WIDTH) x++;
            if(key_hit(KEY_UP)&&y>0) y--;
            if(key_hit(KEY_DOWN)&&y+1<ED_HEIGHT) y++;
            if(key_hit(KEY_L)) selected=(selected+BRUSH_COUNT-1)%BRUSH_COUNT;
            if(key_hit(KEY_R)) selected=(selected+1)%BRUSH_COUNT;
            if(input&ED_INPUT_A) { ed_put(&editor_level,x,y,brushes[selected]); status=locked?"SRAM BLOQUEADA":"SIN GUARDAR"; }
            if(input&ED_INPUT_B) { ed_put(&editor_level,x,y,ED_EMPTY); status=locked?"SRAM BLOQUEADA":"SIN GUARDAR"; }
            if(input&ED_INPUT_START) {
                if(locked) status="SRAM BLOQUEADA";
                else { ed_encode(&editor_level,editor_blob);
                    if(sram_write(EDITOR_SRAM_ADDR,editor_blob,ED_SAVE_BYTES)==0 &&
                       sram_read(EDITOR_SRAM_ADDR,editor_verify,ED_SAVE_BYTES)==0 &&
                       !memcmp(editor_blob,editor_verify,ED_SAVE_BYTES)) status="GUARDADO";
                    else status="ERROR AL GUARDAR";
                }
            }
            if(x<left) left=x;
            if(x>=left+VIEW_W) left=x-VIEW_W+1;
            if(y<top) top=y;
            if(y>=top+VIEW_H) top=y-VIEW_H+1;
        }
        VBlankIntrWait();
        if(old!=screen || old_x!=x || old_y!=y || old_selected!=selected ||
           old_status!=status || input || action)
            draw(x,y,left,top,selected,screen,status);
    }
}
