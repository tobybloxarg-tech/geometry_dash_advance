/* Multi-slot visual editor. Play uses a validated snapshot of a saved level. */
#include <tonc.h>
#include <string.h>
#include "posprintf.h"
#include "main.h"
#include "level_select.h"
#include "editor.h"
#include "editor_core.h"
#include "editor_play.h"
#include "metatiles.h"
#include "tonc_tte.h"
#include "pusab.h"
#include "editor_sprite_icons.h"

#define EDITOR_SRAM_ADDR 0x6000
#define BANK_BASE 0x6300
#define BANK_ADDR(slot,bank) (BANK_BASE + ((slot)*2+(bank))*ED_BANK_BYTES)
#define VIEW_W 14
#define VIEW_H 6
/* Compile-time overlap guard against stock SaveBlock growth. */
typedef char editor_sram_after_save[(sizeof(struct SaveBlock) <= EDITOR_SRAM_ADDR) ? 1 : -1];
static EWRAM_DATA EditorLevel editor_level;
static EWRAM_DATA u8 editor_blob[ED_SAVE_BYTES];
static EWRAM_DATA u8 bank_a[ED_BANK_BYTES], bank_b[ED_BANK_BYTES];
static EWRAM_DATA u8 bank_out[ED_BANK_BYTES], bank_verify[ED_BANK_BYTES];
typedef char editor_banks_fit[(BANK_BASE + ED_SLOT_COUNT*2*ED_BANK_BYTES <= 0x8000) ? 1 : -1];
static int slot_status[ED_SLOT_COUNT], slot_write[ED_SLOT_COUNT];
static int legacy_valid, legacy_unknown;
static int active_slot=-1, screen_list, screen_detail, list_cursor, detail_cursor;
static int dirty, legacy_copy, settings_cursor;
#define SONG_NAME(name, label) label,
static const char *song_names[] = { ED_SONG_LIST(SONG_NAME) };
#undef SONG_NAME
static const char *mode_names[] = {"CUBO", "NAVE", "BOLA", "OVNI", "ONDA"};
static const char *speed_names[] = {"0.5X", "1X", "2X", "3X", "4X"};
static int blank(const u8 *p, unsigned n) {
    int z=1,f=1; unsigned i; for(i=0;i<n;i++) { if(p[i]) z=0; if(p[i]!=0xff) f=0; }
    return z||f;
}
static int read_banks(int slot) {
    return sram_read(BANK_ADDR(slot,0),bank_a,ED_BANK_BYTES)==0 &&
           sram_read(BANK_ADDR(slot,1),bank_b,ED_BANK_BYTES)==0;
}
/* Legacy stays in its original address; saving an edit creates a separate copy. */
static void refresh_slots(void) {
    unsigned i;
    if(sram_read(EDITOR_SRAM_ADDR,editor_blob,ED_SAVE_BYTES)!=0) { legacy_valid=0; legacy_unknown=1; }
    else { legacy_valid=ed_decode(&editor_level,editor_blob); legacy_unknown=!legacy_valid && !blank(editor_blob,ED_SAVE_BYTES); }
    for(i=0;i<ED_SLOT_COUNT;i++) {
        int writable=0, chosen=-1;
        if(read_banks(i)) chosen=ed_choose_bank(bank_a,bank_b,&writable);
        slot_status[i]=chosen>=0 ? 1 : (writable ? 0 : -1);
        slot_write[i]=writable;
    }
}
static int load_slot(int slot) {
    int writable, chosen;
    if(slot<0) return sram_read(EDITOR_SRAM_ADDR,editor_blob,ED_SAVE_BYTES)==0 && ed_decode(&editor_level,editor_blob);
    if(!read_banks(slot)) return 0;
    chosen=ed_choose_bank(bank_a,bank_b,&writable);
    return chosen>=0 && ed_bank_read(chosen?bank_b:bank_a,&editor_level,0)==ED_BANK_VALID;
}
static int save_slot(int slot) {
    int writable, chosen, target; uint32_t seq=0;
    if(slot<0 || slot>=ED_SLOT_COUNT || !read_banks(slot)) return 0;
    chosen=ed_choose_bank(bank_a,bank_b,&writable);
    if(!writable) return 0;
    if(chosen>=0) ed_bank_read(chosen?bank_b:bank_a,0,&seq);
    target=(chosen==0)?1:0;
    if(!ed_bank_encode(bank_out,&editor_level,seq+1)) return 0;
    return sram_write(BANK_ADDR(slot,target),bank_out,ED_BANK_BYTES)==0 &&
           sram_read(BANK_ADDR(slot,target),bank_verify,ED_BANK_BYTES)==0 &&
           !memcmp(bank_out,bank_verify,ED_BANK_BYTES);
}

/* Legacy IDs 1,2 remain block and spike in every version. The order below is
 * visual category order, not a contiguous metatile range. */
static const u8 brushes[] = { ED_BLOCK, ED_DARK, ED_CHECK, ED_SLAB_UP,
    ED_SLAB_DOWN, ED_SLAB_LEFT, ED_SLAB_RIGHT, ED_SPIKE, ED_SPIKE_DOWN,
    ED_SPIKE_RIGHT, ED_SPIKE_LEFT, ED_SMALL_UP, ED_SMALL_DOWN,
    ED_MEDIUM_UP, ED_MEDIUM_DOWN };
#define TERRAIN_BRUSH_COUNT (sizeof(brushes)/sizeof(brushes[0]))
#define BRUSH_COUNT (TERRAIN_BRUSH_COUNT + ED_SPR_COUNT - 1)
static const char *names[] = {"VACIO","BLOQUE","PINCHO","BLOQUE OSCURO",
    "BLOQUE CUADROS","LOSA ARRIBA","LOSA ABAJO","LOSA IZQ","LOSA DER",
    "PINCHO ABAJO","PINCHO DER","PINCHO IZQ","PINCHO CHICO","CHICO INV",
    "PINCHO MEDIO","MEDIO INV"};
static const char *sprite_names[] = {"", "ORBE AMARILLO", "ORBE AZUL", "ORBE ROSA",
    "ORBE VERDE", "ORBE NEGRO", "ORBE ROJO", "PORTAL NAVE", "PORTAL CUBO",
    "PORTAL BOLA", "PORTAL OVNI", "PORTAL ONDA", "GRAVEDAD AZUL",
    "GRAVEDAD AMARILLA", "PORTAL MINI", "PORTAL GRANDE",
    "VELOCIDAD 0.5X", "VELOCIDAD 1X", "VELOCIDAD 2X",
    "VELOCIDAD 3X", "VELOCIDAD 4X"};
typedef char editor_sprite_name_count[(sizeof(sprite_names)/sizeof(sprite_names[0]) == ED_SPR_COUNT) ? 1 : -1];
typedef char editor_sprite_icon_count[(sizeof(editor_sprite_tiles)/sizeof(editor_sprite_tiles[0]) == (ED_SPR_COUNT-1)*32) ? 1 : -1];
static void text(int x,int y,const char *s) { tte_set_pos(x,y); tte_write(s); }
/* CBB1 holds the first 128 game tiles; 128..131 are editor grid quadrants. */
static void metatile_at(unsigned col, unsigned row, unsigned kind) {
    const SCR_ENTRY *m=metatiles[ed_metatile[kind]];
    unsigned base=row*32+col;
    if (kind==ED_EMPTY) {
        se_mem[28][base]=0x1080; se_mem[28][base+1]=0x1081;
        se_mem[28][base+32]=0x1082; se_mem[28][base+33]=0x1083;
    } else {
        se_mem[28][base]=m[0]; se_mem[28][base+1]=m[1];
        se_mem[28][base+32]=m[2]; se_mem[28][base+33]=m[3];
    }
}
/* Icons mark the object's anchor; collision artwork can extend beyond it. */
static void sprite_at(unsigned col, unsigned row, unsigned kind) {
    unsigned base=row*32+col, tile=8+(kind-1)*4;
    if(!kind) return;
    se_mem[26][base]=0x3000|tile; se_mem[26][base+1]=0x3000|(tile+1);
    se_mem[26][base+32]=0x3000|(tile+2); se_mem[26][base+33]=0x3000|(tile+3);
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
    memset32(&se_mem[26][0],0x00040004,sizeof(SCREENBLOCK)/4);
    tte_erase_rect(0,0,240,160);
    if(screen_list) {
        text(0,0,"MIS NIVELES");
        text(0,8,list_cursor==0?"X NUEVO":"  NUEVO");
        for(int i=0;i<ED_SLOT_COUNT;i++) if(slot_status[i]!=0) {
            posprintf(line,"%s NIVEL %d %s",list_cursor==i+1?"X":" ",i+1,
                      slot_status[i]<0?"BLOQUEADO":"VER");
            text(0,16+i*16,line);
        }
        if(legacy_valid || legacy_unknown) text(0,112,list_cursor==7?"X ANTERIOR VER":"  ANTERIOR VER");
        text(0,144,"A VER/NUEVO B TITULO"); text(0,152,status); return;
    }
    if(screen_detail) {
        posprintf(line,active_slot<0?"NIVEL ANTERIOR":"NIVEL %d",active_slot+1); text(0,0,line);
        text(0,24,detail_cursor==0?"X JUGAR":"  JUGAR");
        text(0,48,detail_cursor==1?"X EDITAR":"  EDITAR");
        if(active_slot<0) text(0,80,"START GUARDA COPIA");
        else if(!slot_write[active_slot]) text(0,80,"GUARDADO BLOQUEADO");
        text(0,128,"A ELEGIR B VOLVER"); text(0,144,status); return;
    }
    if (screen==ED_SCREEN_MENU) {
        text(0,0,"MENU EDITOR"); text(0,16,"B VOLVER AL EDITOR");
        text(0,32,"A AJUSTES DEL NIVEL");
        text(0,48,"START VOLVER A LISTA");
        text(0,80,"CAMBIOS SIN GUARDAR"); text(0,96,"SE PERDERAN AL SALIR");
        text(0,112,"JUGAR DESDE LA LISTA"); return;
    }
    if (screen==ED_SCREEN_SETTINGS) {
        const EditorSettings *a=&editor_level.settings;
        text(0,0,"AJUSTES DEL NIVEL");
        posprintf(line,"%s MODO %s",settings_cursor==0?"X":" ",mode_names[a->mode]); text(0,24,line);
        posprintf(line,"%s VELOCIDAD %s",settings_cursor==1?"X":" ",speed_names[a->speed]); text(0,40,line);
        posprintf(line,"%s MINI %s",settings_cursor==2?"X":" ",a->mini?"SI":"NO"); text(0,56,line);
        posprintf(line,"%s MUSICA",settings_cursor==3?"X":" "); text(0,72,line);
        text(8,88,song_names[a->song]);
        posprintf(line,"%s LARGO %d",settings_cursor==4?"X":" ",editor_level.width); text(0,104,line);
        if(settings_cursor==4 && editor_level.width<ED_WIDTH) text(0,112,"DER AMPLIAR");
        text(0,128,"ARRIBA ABAJO ELEGIR"); text(0,136,"IZQ DER CAMBIAR B VOLVER");
        text(0,152,status); return;
    }
    posprintf(line,"X%d/%d Y%d L/R TIPO",x+1,editor_level.width,y+1); text(0,0,line);
    for(unsigned r=0;r<VIEW_H;r++) for(unsigned c=0;c<VIEW_W;c++) {
        metatile_at(c*2,2+r*2,(unsigned)ed_get(&editor_level,left+c,top+r));
        sprite_at(c*2,2+r*2,(unsigned)ed_sprite_get(&editor_level,left+c,top+r));
    }
    outline((x-left)*2,2+(y-top)*2);
    text(0,112,selected<TERRAIN_BRUSH_COUNT ? names[brushes[selected]] : sprite_names[selected-TERRAIN_BRUSH_COUNT+1]);
    /* Visible category strip, up to seven actual 16x16 metatiles per page. */
    unsigned start=(selected/7)*7;
    for(unsigned i=0;i<7 && start+i<BRUSH_COUNT;i++) {
        if(start+i<TERRAIN_BRUSH_COUNT) metatile_at(i*3,16,brushes[start+i]);
        else sprite_at(i*3,16,start+i-TERRAIN_BRUSH_COUNT+1);
    }
    outline((selected-start)*3,16);
    text(0,144,"A/B EDITAR A+B AJUSTES"); text(0,152,status);
}
void editor_loop(void) {
    unsigned x=0,y=0,left=0,top=0,selected=0;
    int screen=ED_SCREEN_EDIT; const char *status="";
    EditorButtons buttons={0};
    unsigned ax=0,ay=0,aselected=0,bx=0,by=0,bselected=0;
    screen_list=1; screen_detail=0; list_cursor=detail_cursor=0; active_slot=-1; dirty=legacy_copy=0;
    refresh_slots();
    REG_DISPCNT=DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_BG2 | DCNT_BG3;
    REG_BLDCNT=0; REG_MOSAIC=0;
    REG_BG0CNT=BG_CBB(2)|BG_SBB(29)|BG_REG_32x32|BG_PRIO(0);
    REG_BG1CNT=BG_CBB(1)|BG_SBB(28)|BG_REG_32x32|BG_PRIO(2);
    REG_BG3CNT=BG_CBB(2)|BG_SBB(26)|BG_REG_32x32|BG_PRIO(1);
    REG_BG3HOFS=REG_BG3VOFS=0;
    REG_BG0HOFS=REG_BG0VOFS=REG_BG1HOFS=REG_BG1VOFS=REG_BG2HOFS=REG_BG2VOFS=0;
    memset32(palette_buffer,0,256);
    memcpy16(palette_buffer,blockPalette,16);
    memcpy16(&palette_buffer[32],&menu_palette[32],16);
    /* The game normally computes these object colors when a level loads.
     * Raw blockPalette has a black OBJ entry, so without this initialization
     * the editor renders every block outline and spike almost invisible. */
    palette_buffer[0]=0x5000;
    palette_buffer[8]=0x5863; palette_buffer[9]=0x6d4a;
    palette_buffer[10]=0x7fff; palette_buffer[11]=0x7fff;
    /* Pusab glyph indices are 1=white, 2=black, 3=gray. The menu's
     * default bank 2 has index 1 black and 3 white, inverting the glyphs. */
    palette_buffer[33]=0x7fff; palette_buffer[34]=0x0000;
    palette_buffer[35]=0x5294;
    memcpy32(pal_bg_mem,palette_buffer,256);
    tte_init_se(2,BG_CBB(0)|BG_SBB(27)|BG_REG_32x32|BG_PRIO(1),0,0,0,&pusabFont,NULL);
    tte_set_special(0x2000);
    memcpy32(&tile_mem[1][0],blockset,128*sizeof(TILE)/4);
    /* 16x16 grid cells: top/left outlines, one line per metatile. */
    for(unsigned r=0;r<8;r++) {
        tile_mem[1][128].data[r]=(r==0)?0x22222222:0x11111112;
        tile_mem[1][129].data[r]=(r==0)?0x22222222:0x11111111;
        tile_mem[1][130].data[r]=0x11111112;
        tile_mem[1][131].data[r]=0x11111111;
    }
    memset32(&tile_mem[2][0],0,5*sizeof(TILE)/4);
    /* Colour 15 in palette bank 1 is the cursor, indices reflect GBA nibble order. */
    for(unsigned r=0;r<8;r++) {
        tile_mem[2][0].data[r]=(r==0)?0xffffffff:0x0000000f;
        tile_mem[2][1].data[r]=(r==0)?0xffffffff:0xf0000000;
        tile_mem[2][2].data[r]=(r==7)?0xffffffff:0x0000000f;
        tile_mem[2][3].data[r]=(r==7)?0xffffffff:0xf0000000;
    }
    memcpy32(&tile_mem[2][8],editor_sprite_tiles,sizeof(editor_sprite_tiles)/4);
    memcpy16(&palette_buffer[48],editor_sprite_palette,16);
    /* Cursor tiles use palette 1. */
    palette_buffer[17]=0x5000; palette_buffer[18]=0x7e80; palette_buffer[31]=0x03ff;
    memcpy32(pal_bg_mem,palette_buffer,256);
    memset32(&se_mem[27][0],0,sizeof(SCREENBLOCK)/4);
    VBlankIntrWait(); draw(x,y,left,top,selected,screen,status);
    while(1) {
        int changed=0;
        unsigned down, hit, edit_buttons;
        key_poll();
        down=(key_is_down(KEY_A)?ED_INPUT_A:0)|(key_is_down(KEY_B)?ED_INPUT_B:0);
        hit=(key_hit(KEY_A)?ED_INPUT_A:0)|(key_hit(KEY_B)?ED_INPUT_B:0);
        /* SELECT wins over grid actions; START commits a pending edit before
         * saving. Never recognize a chord in list/detail/menu/settings. */
        edit_buttons=ed_edit_buttons_step(&buttons,down,hit,
            !screen_list && !screen_detail && screen==ED_SCREEN_EDIT && !key_hit(KEY_SELECT),
            key_hit(KEY_START));
        if(screen_list) {
            int old=list_cursor;
            if(key_hit(KEY_UP)) { do { list_cursor=(list_cursor+7)%8; }
                while(list_cursor && !(list_cursor==7?(legacy_valid||legacy_unknown):slot_status[list_cursor-1]!=0)); }
            if(key_hit(KEY_DOWN)) { do { list_cursor=(list_cursor+1)%8; }
                while(list_cursor && !(list_cursor==7?(legacy_valid||legacy_unknown):slot_status[list_cursor-1]!=0)); }
            if(key_hit(KEY_B)) { game_state=STATE_TITLE_SCREEN; return; }
            if(key_hit(KEY_A)) {
                if(list_cursor==0) {
                    int slot; for(slot=0;slot<ED_SLOT_COUNT;slot++) if(slot_status[slot]==0 && slot_write[slot]) break;
                    if(slot==ED_SLOT_COUNT) status="SIN ESPACIO DISPONIBLE";
                    else { active_slot=slot; ed_clear(&editor_level); dirty=1; legacy_copy=0;
                        x=y=left=top=selected=0; screen_list=0; status="START GUARDAR SELECT MENU"; }
                } else {
                    active_slot=list_cursor==7?-1:list_cursor-1;
                    if((active_slot<0 && !legacy_valid) || (active_slot>=0 && slot_status[active_slot]<0)) status="NIVEL DANADO BLOQUEADO";
                    else { screen_list=0; screen_detail=1; detail_cursor=0; status=""; }
                }
                changed=1;
            }
            if(old!=list_cursor) changed=1;
        } else if(screen_detail) {
            if(key_hit(KEY_UP)||key_hit(KEY_DOWN)) { detail_cursor^=1; changed=1; }
            if(key_hit(KEY_B)) { screen_detail=0; screen_list=1; status=""; changed=1; }
            else if(key_hit(KEY_A)) {
                if(detail_cursor==0) {
                    if(!load_slot(active_slot)) status="ERROR AL LEER NIVEL";
                    else if(!ed_play_configure(editor_level.settings.mode, editor_level.settings.speed,
                                               editor_level.settings.mini, editor_level.settings.song))
                        status="AJUSTES INVALIDOS";
                    else if(editor_play_start(&editor_level)) return;
                    else status=ed_play_error()==ED_PLAY_TOO_MANY ? "MAXIMO 16 OBJETOS" : "NIVEL INVALIDO";
                }
                else if(load_slot(active_slot)) { screen_detail=0; screen=ED_SCREEN_EDIT; dirty=0; legacy_copy=(active_slot<0);
                    x=y=left=top=selected=0; status=active_slot<0?"START GUARDA COPIA":"START GUARDAR SELECT MENU"; }
                else status="ERROR AL LEER NIVEL";
                changed=1;
            }
        } else if(screen==ED_SCREEN_SETTINGS) {
            if(key_hit(KEY_B) && !buttons.blocked) { screen=ED_SCREEN_MENU; status=""; changed=1; }
            else {
                EditorSettings before=editor_level.settings;
                unsigned width_before=editor_level.width;
                if(key_hit(KEY_UP)) { settings_cursor=(settings_cursor+4)%5; changed=1; }
                if(key_hit(KEY_DOWN)) { settings_cursor=(settings_cursor+1)%5; changed=1; }
                int delta=key_hit(KEY_LEFT)?-1:key_hit(KEY_RIGHT)?1:0;
                if(delta) {
                    if(settings_cursor==0) editor_level.settings.mode=(before.mode+ED_MODE_COUNT+delta)%ED_MODE_COUNT;
                    else if(settings_cursor==1) editor_level.settings.speed=(before.speed+ED_SPEED_COUNT+delta)%ED_SPEED_COUNT;
                    else if(settings_cursor==2) editor_level.settings.mini^=1;
                    else if(settings_cursor==3) editor_level.settings.song=(before.song+ED_SONG_COUNT+delta)%ED_SONG_COUNT;
                    else if(delta>0) editor_level.width=ED_WIDTH;
                    /* Shortening would discard cells; only the legacy width is
                     * retained automatically, never truncate an existing grid. */
                    if(!ed_can_encode(&editor_level)) { editor_level.settings=before; editor_level.width=width_before; status="SIN ESPACIO EN NIVEL"; }
                    else if(settings_cursor!=4 || editor_level.width!=width_before) { dirty=1; status="SIN GUARDAR"; }
                    changed=1;
                }
            }
        } else if(screen==ED_SCREEN_MENU) {
            if(key_hit(KEY_B)) { screen=ED_SCREEN_EDIT; changed=1; }
            else if(key_hit(KEY_A)) { screen=ED_SCREEN_SETTINGS; settings_cursor=0; status=""; changed=1; }
            else if(key_hit(KEY_START)) { refresh_slots(); screen_list=1; screen=ED_SCREEN_EDIT;
                status=dirty?"CAMBIOS DESCARTADOS":""; changed=1; }
        } else {
            if(key_hit(KEY_SELECT)) { screen=ED_SCREEN_MENU; changed=1; }
            else if(edit_buttons & ED_BUTTON_CHORD) {
                screen=ED_SCREEN_SETTINGS; settings_cursor=0; status=""; changed=1;
            } else {
                /* Hold a direction to travel the long grid; first movement is instant. */
                static unsigned move_age;
                int horiz=key_is_down(KEY_RIGHT)?1:key_is_down(KEY_LEFT)?-1:0;
                if(!horiz) move_age=0;
                else {
                    if(key_hit(KEY_LEFT)||key_hit(KEY_RIGHT)) move_age=0;
                    if(move_age==0 || (move_age>=15 && move_age%3==0)) {
                        if(horiz<0 && x>0) { x--; changed=1; }
                        if(horiz>0 && x+1<editor_level.width) { x++; changed=1; }
                    }
                    if(move_age<60000) move_age++;
                }
                if(key_hit(KEY_UP)&&y>0) { y--; changed=1; }
                if(key_hit(KEY_DOWN)&&y+1<ED_HEIGHT) { y++; changed=1; }
                if(key_hit(KEY_L)) { selected=(selected+BRUSH_COUNT-1)%BRUSH_COUNT; changed=1; }
                if(key_hit(KEY_R)) { selected=(selected+1)%BRUSH_COUNT; changed=1; }
                /* Remember the target at the press, not after the debounce
                 * delay: the cursor/brush may move during the window. */
                if(hit & ED_INPUT_A) { ax=x; ay=y; aselected=selected; }
                if(hit & ED_INPUT_B) { bx=x; by=y; bselected=selected; }
                if(edit_buttons & ED_INPUT_A) {
                    if(aselected<TERRAIN_BRUSH_COUNT) {
                        int old=ed_get(&editor_level,ax,ay);
                        ed_put(&editor_level,ax,ay,brushes[aselected]);
                        if(!ed_can_encode(&editor_level)) {
                            ed_put(&editor_level,ax,ay,(unsigned)old);
                            status="SIN ESPACIO EN NIVEL"; changed=1;
                            goto placement_done;
                        }
                    }
                    else {
                        int old=ed_sprite_get(&editor_level,ax,ay);
                        ed_sprite_put(&editor_level,ax,ay,aselected-TERRAIN_BRUSH_COUNT+1);
                        if(!ed_can_encode(&editor_level)) {
                            ed_sprite_put(&editor_level,ax,ay,(unsigned)old);
                            status="SIN ESPACIO EN NIVEL"; changed=1;
                            goto placement_done;
                        }
                    }
                    dirty=1; status="SIN GUARDAR"; changed=1;
placement_done: ;
                }
                if(edit_buttons & ED_INPUT_B) {
                    /* Removing can split a run and increase encoded size. */
                    int old=bselected<TERRAIN_BRUSH_COUNT ? ed_get(&editor_level,bx,by) : ed_sprite_get(&editor_level,bx,by);
                    if(bselected<TERRAIN_BRUSH_COUNT) ed_put(&editor_level,bx,by,ED_EMPTY);
                    else ed_sprite_put(&editor_level,bx,by,ED_SPR_EMPTY);
                    if(!ed_can_encode(&editor_level)) {
                        if(bselected<TERRAIN_BRUSH_COUNT) ed_put(&editor_level,bx,by,(unsigned)old);
                        else ed_sprite_put(&editor_level,bx,by,(unsigned)old);
                        status="SIN ESPACIO EN NIVEL"; changed=1;
                    } else { dirty=1; status="SIN GUARDAR"; changed=1; }
                }
                if(key_hit(KEY_START)) {
                    if(active_slot<0 && legacy_copy) {
                        int slot; for(slot=0;slot<ED_SLOT_COUNT;slot++) if(slot_status[slot]==0 && slot_write[slot]) break;
                        if(slot==ED_SLOT_COUNT) status="SIN ESPACIO PARA COPIA";
                        else if(save_slot(slot)) { active_slot=slot; legacy_copy=0; dirty=0; status="COPIA GUARDADA"; }
                        else status="ERROR AL GUARDAR COPIA";
                    } else if(save_slot(active_slot)) { status="GUARDADO"; dirty=0; }
                    else status="ERROR/BLOQUEADO";
                    changed=1;
                }
                if(x<left) left=x;
                if(x>=left+VIEW_W) left=x-VIEW_W+1;
                if(y<top) top=y;
                if(y>=top+VIEW_H) top=y-VIEW_H+1;
            }
        }
        VBlankIntrWait(); if(changed) draw(x,y,left,top,selected,screen,status);
    }
}
