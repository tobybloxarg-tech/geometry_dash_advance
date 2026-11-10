#include "editor_core.h"
#include <string.h>
const uint16_t ed_metatile[ED_KIND_COUNT] = {
    0x00, 0x02, 0x01, 0x2a, 0x03, 0x2b, 0x2c, 0x2d, 0x2e,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x22, 0x23
};
/* Stable codes above map to the stock engine's ObjTypes, not Tiled GIDs. */
const uint16_t ed_sprite_type[ED_SPR_COUNT] = {
    0, 9, 16, 18, 160, 163, 164, 1, 2, 26, 57, 114, 14, 15, 28, 29
};
int ed_sprite_put(EditorLevel *level, unsigned x, unsigned y, unsigned kind) {
    if (x >= ED_WIDTH || y >= ED_HEIGHT || kind >= ED_SPR_COUNT) return 0;
    level->sprites[y * ED_WIDTH + x] = (uint8_t)kind; return 1;
}
int ed_sprite_get(const EditorLevel *level, unsigned x, unsigned y) {
    if (x >= ED_WIDTH || y >= ED_HEIGHT) return -1;
    return level->sprites[y * ED_WIDTH + x];
}
int ed_menu_input(int *screen, unsigned input) {
    switch (*screen) {
    case ED_SCREEN_EDIT:
        if (input & ED_INPUT_SELECT) *screen = ED_SCREEN_MENU;
        break;
    case ED_SCREEN_MENU:
        if (input & ED_INPUT_B) *screen = ED_SCREEN_EDIT;
        else if (input & ED_INPUT_A) *screen = ED_SCREEN_CONFIRM_NEW;
        else if (input & ED_INPUT_START) return ED_ACTION_EXIT;
        break;
    case ED_SCREEN_CONFIRM_NEW:
        if (input & ED_INPUT_B) *screen = ED_SCREEN_MENU;
        else if (input & ED_INPUT_A) {
            *screen = ED_SCREEN_EDIT;
            return ED_ACTION_NEW;
        }
        break;
    }
    return ED_ACTION_NONE;
}
void ed_clear(EditorLevel *level) { memset(level, 0, sizeof(*level)); }
int ed_put(EditorLevel *level, unsigned x, unsigned y, unsigned kind) {
    if (x >= ED_WIDTH || y >= ED_HEIGHT || kind >= ED_KIND_COUNT) return 0;
    level->cells[y * ED_WIDTH + x] = (uint8_t)kind; return 1;
}
int ed_get(const EditorLevel *level, unsigned x, unsigned y) {
    if (x >= ED_WIDTH || y >= ED_HEIGHT) return -1;
    return level->cells[y * ED_WIDTH + x];
}
static uint32_t checksum(const uint8_t *p, size_t n) {
    uint32_t h = 2166136261u; while (n--) { h ^= *p++; h *= 16777619u; } return h;
}
/* Version 4 packs two 4-bit stable symbols into each cell, retaining the
 * original record and bank lengths and addresses. Old readers reject v4. */
void ed_encode(const EditorLevel *level, uint8_t out[ED_SAVE_BYTES]) {
    const uint8_t header[8] = {'G','D','E','D',4,ED_WIDTH,ED_HEIGHT,0};
    uint32_t h; unsigned i;
    memcpy(out,header,8);
    for (i=0;i<ED_CELLS;i++) out[8+i] = (uint8_t)(level->cells[i] | (level->sprites[i]<<4));
    h = checksum(out, 8+ED_CELLS);
    for (i=0;i<4;i++) out[8+ED_CELLS+i]=(uint8_t)(h>>(8*i));
}
int ed_decode(EditorLevel *level, const uint8_t in[ED_SAVE_BYTES]) {
    uint32_t h, stored=0; unsigned i, version;
    if (memcmp(in,"GDED",4) || in[5]!=ED_WIDTH || in[6]!=ED_HEIGHT || in[7]) return 0;
    version=in[4]; if(version!=1 && version!=2 && version!=4) return 0;
    h=checksum(in,8+ED_CELLS);
    for (i=0;i<4;i++) stored|=(uint32_t)in[8+ED_CELLS+i]<<(8*i);
    if (h!=stored) return 0;
    for (i=0;i<ED_CELLS;i++) {
        unsigned c=in[8+i];
        if (version==1 && c>=3) return 0;
        if (version==2 && c>=ED_KIND_COUNT) return 0;
        if (version==4 && ((c & 15)>=ED_KIND_COUNT || (c>>4)>=ED_SPR_COUNT)) return 0;
    }
    for (i=0;i<ED_CELLS;i++) {
        unsigned c=in[8+i];
        level->cells[i]=(uint8_t)(version==4 ? c&15 : c);
        level->sprites[i]=(uint8_t)(version==4 ? c>>4 : 0);
    }
    return 1;
}

int ed_bank_read(const uint8_t *bank, EditorLevel *level, uint32_t *sequence) {
    unsigned i; int zero=1, ff=1;
    uint32_t stored=0, seq=0;
    /* EditorLevel is 1024 bytes. Keep the validation scratch out of the
     * limited GBA stack: this function is also called while the editor loop
     * and bank-selection frames are live. It is not called from interrupts. */
#if defined(__arm__)
    static EditorLevel temp __attribute__((section(".ewram")));
#else
    static EditorLevel temp;
#endif
    for(i=0;i<ED_BANK_BYTES;i++) { if(bank[i]) zero=0; if(bank[i]!=0xff) ff=0; }
    if(zero || ff) return ED_BANK_EMPTY;
    if(memcmp(bank,"GDM3",4)) return ED_BANK_UNKNOWN;
    for(i=0;i<4;i++) { seq |= (uint32_t)bank[4+i] << (8*i);
        stored |= (uint32_t)bank[ED_BANK_BYTES-4+i] << (8*i); }
    if(stored!=checksum(bank,ED_BANK_BYTES-4) || !ed_decode(&temp,bank+8)) return ED_BANK_UNKNOWN;
    if(level) *level=temp;
    if(sequence) *sequence=seq;
    return ED_BANK_VALID;
}
void ed_bank_encode(uint8_t *bank, const EditorLevel *level, uint32_t sequence) {
    uint32_t h; unsigned i;
    memcpy(bank,"GDM3",4);
    for(i=0;i<4;i++) bank[4+i]=(uint8_t)(sequence>>(8*i));
    ed_encode(level,bank+8);
    h=checksum(bank,ED_BANK_BYTES-4);
    for(i=0;i<4;i++) bank[ED_BANK_BYTES-4+i]=(uint8_t)(h>>(8*i));
}
/* Return the newest valid bank or -1; NEVER write a slot with unknown bytes. */
int ed_choose_bank(const uint8_t *a, const uint8_t *b, int *writable) {
    uint32_t sa=0,sb=0;
    int aa=ed_bank_read(a,0,&sa), bb=ed_bank_read(b,0,&sb);
    *writable=aa!=ED_BANK_UNKNOWN && bb!=ED_BANK_UNKNOWN;
    if(aa!=ED_BANK_VALID) return bb==ED_BANK_VALID ? 1 : -1;
    if(bb!=ED_BANK_VALID) return 0;
    return (int32_t)(sb-sa)>0 ? 1 : 0;
}
