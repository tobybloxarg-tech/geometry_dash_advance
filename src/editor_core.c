#include "editor_core.h"
#include <string.h>
const uint16_t ed_metatile[ED_KIND_COUNT] = {
    0x00, 0x02, 0x01, 0x2a, 0x03, 0x2b, 0x2c, 0x2d, 0x2e,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x22, 0x23
};
/* Stable codes above map to the stock engine's ObjTypes, not Tiled GIDs. */
const uint16_t ed_sprite_type[ED_SPR_COUNT] = {
    0, 9, 16, 18, 160, 163, 164, 1, 2, 26, 57, 114, 14, 15, 28, 29, 52, 53, 54, 55, 56
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
unsigned ed_edit_buttons_step(EditorButtons *s, unsigned down, unsigned hit,
                              int in_grid, int flush) {
    const unsigned buttons = ED_INPUT_A | ED_INPUT_B;
    unsigned out = 0, other;
    down &= buttons; hit &= buttons;
    if (s->blocked) {
        if (!down) s->blocked = 0;
        return 0;
    }
    if (!in_grid) { s->pending = s->age = 0; return 0; }
    if (s->pending) {
        other = buttons ^ s->pending;
        if ((down & s->pending) && (down & hit & other)) {
            s->pending = s->age = 0; s->blocked = 1;
            return ED_BUTTON_CHORD;
        }
        if (!(down & s->pending) || ++s->age >= ED_CHORD_WINDOW_FRAMES || flush) {
            out = s->pending; s->pending = s->age = 0;
        }
    }
    if (!s->pending && hit) {
        if ((hit & down) == buttons) {
            s->blocked = 1;
            return out | ED_BUTTON_CHORD;
        }
        /* A button that was already held (or inherited from a menu) must not
         * become a chord after its single action has committed. */
        if (down & (buttons ^ hit)) out |= hit;
        else if (flush) out |= hit;
        else { s->pending = hit; s->age = 0; }
    }
    return out;
}
void ed_test_confirm_open(EditorTestConfirm *s) {
    s->yes = 0; s->armed = 0;
}
int ed_test_confirm_step(EditorTestConfirm *s, unsigned down, unsigned hit) {
    if (!s->armed) {
        if (!(down & (ED_INPUT_A | ED_INPUT_B))) s->armed = 1;
        return ED_TEST_WAIT;
    }
    /* B always wins over A; ignore navigation on an action frame. */
    if (hit & ED_INPUT_B) return ED_TEST_CANCEL;
    if (hit & ED_INPUT_A) return (down & ED_INPUT_B) || !s->yes ? ED_TEST_CANCEL : ED_TEST_START;
    if (hit & (ED_INPUT_UP | ED_INPUT_LEFT)) s->yes = 1;
    else if (hit & (ED_INPUT_DOWN | ED_INPUT_RIGHT)) s->yes = 0;
    return ED_TEST_WAIT;
}
void ed_clear(EditorLevel *level) {
    memset(level, 0, sizeof(*level));
    level->settings.speed = ED_SPEED_X1;
    level->width = ED_WIDTH;
}
int ed_settings_valid(const EditorSettings *s) {
    return s && s->mode < ED_MODE_COUNT && s->speed < ED_SPEED_COUNT &&
           s->mini < 2 && s->song < ED_SONG_COUNT;
}
static int default_settings(const EditorSettings *s) {
    return s->mode == ED_MODE_CUBE && s->speed == ED_SPEED_X1 &&
           !s->mini && s->song == ED_SONG_STEREOMA;
}
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
/* The SRAM record and bank addresses remain unchanged. Old versions use
 * a 64x8 row-major grid. V6 uses a 512-byte command area, row-major at the
 * declared width. Runs never exceed the declared grid and padding is zero.
 * 00..7f: 1..128 empty cells; 80..bf: 1..64 cells of following terrain;
 * c1..df: one sprite (low five bits), next byte terrain; e1..ef: one terrain.
 * The header holds settings and the width in 16-column units; the stream
 * is self-terminating after exactly width*8 cells. The checksum covers padding.
 * V6 is intentionally rejected by older builds rather than misread. */
static int legacy_mode(const EditorLevel *level) {
    unsigned x,y,n=0; int v4;
    if(level->width!=ED_OLD_WIDTH || !ed_settings_valid(&level->settings)) return 0;
    v4=default_settings(&level->settings);
    for(y=0;y<ED_HEIGHT;y++) for(x=0;x<ED_WIDTH;x++) {
        unsigned i=y*ED_WIDTH+x, c=level->cells[i], s=level->sprites[i];
        if(c>=ED_KIND_COUNT || s>=ED_SPR_COUNT) return 0;
        if(x>=ED_OLD_WIDTH && (c||s)) return 0;
        if(x<ED_OLD_WIDTH) { if(s) n++; if(s>=16) v4=0; }
    }
    return v4 ? 4 : n<=ED_V5_MAX_SPRITES ? 5 : 0;
}
/* Return zero on invalid or insufficient capacity, else number of bytes used
 * plus one, so an empty 0-byte result is never confused with failure. */
static unsigned encode_v6(const EditorLevel *l, uint8_t *out) {
    unsigned i=0, pos=0, total, width=l->width;
    if(width<ED_OLD_WIDTH || width>ED_WIDTH || (width%16) || !ed_settings_valid(&l->settings)) return 0;
    /* Out-of-width data is forbidden: encoding must never silently discard it. */
    for(unsigned y=0;y<ED_HEIGHT;y++) for(unsigned x=0;x<ED_WIDTH;x++) {
        unsigned k=y*ED_WIDTH+x;
        if(l->cells[k]>=ED_KIND_COUNT || l->sprites[k]>=ED_SPR_COUNT ||
           (x>=width && (l->cells[k]||l->sprites[k]))) return 0;
    }
    total=width*ED_HEIGHT;
    while(i<total) {
        unsigned k=(i/width)*ED_WIDTH+i%width, c=l->cells[k], s=l->sprites[k], run=1;
        if(s) {
            if(pos+2>ED_OLD_CELLS) return 0;
            if(out) { out[pos]=(uint8_t)(0xc0|s); out[pos+1]=(uint8_t)c; }
            pos+=2; i++; continue;
        }
        while(i+run<total && run<(c?64u:128u)) {
            unsigned q=((i+run)/width)*ED_WIDTH+(i+run)%width;
            if(l->cells[q]!=c || l->sprites[q]) break;
            run++;
        }
        if(!c) {
            if(pos+1>ED_OLD_CELLS) return 0;
            if(out) out[pos]=(uint8_t)(run-1);
            pos++;
        } else if(run==1) {
            if(pos+1>ED_OLD_CELLS) return 0;
            if(out) out[pos]=(uint8_t)(0xe0|c);
            pos++;
        } else {
            if(pos+2>ED_OLD_CELLS) return 0;
            if(out) { out[pos]=(uint8_t)(0x80|(run-1)); out[pos+1]=(uint8_t)c; }
            pos+=2;
        }
        i+=run;
    }
    return pos+1;
}
int ed_can_encode(const EditorLevel *l) {
    return l && ed_settings_valid(&l->settings) && (legacy_mode(l) || encode_v6(l,0));
}
int ed_encode(const EditorLevel *l, uint8_t out[ED_SAVE_BYTES]) {
    uint32_t h; unsigned i,x,y,n=0; int version;
    if(!l || !out || !ed_settings_valid(&l->settings)) return 0;
    version=legacy_mode(l);
    if(!version && !encode_v6(l,0)) return 0; /* Do not touch output on failure. */
    memset(out,0,ED_SAVE_BYTES); memcpy(out,"GDED",4);
    out[4]=(uint8_t)(version?version:6);
    if(version==4) {
        out[5]=ED_OLD_WIDTH; out[6]=ED_HEIGHT;
        for(y=0;y<ED_HEIGHT;y++) for(x=0;x<ED_OLD_WIDTH;x++) {
            unsigned old=y*ED_OLD_WIDTH+x, k=y*ED_WIDTH+x;
            out[8+old]=(uint8_t)(l->cells[k] | (l->sprites[k]<<4));
        }
    } else if(version==5) {
        out[5]=(uint8_t)(l->settings.mode | (l->settings.speed<<3) | (l->settings.mini<<6));
        out[6]=l->settings.song;
        for(y=0;y<ED_HEIGHT;y++) for(x=0;x<ED_OLD_WIDTH;x++) {
            unsigned old=y*ED_OLD_WIDTH+x, k=y*ED_WIDTH+x;
            out[8+old/2] |= (uint8_t)(l->cells[k] << (4*(old%2)));
            if(l->sprites[k]) {
                unsigned e=old | (l->sprites[k]<<9);
                out[8+ED_OLD_CELLS/2+n*2]=(uint8_t)e;
                out[8+ED_OLD_CELLS/2+n*2+1]=(uint8_t)(e>>8); n++;
            }
        }
        out[7]=(uint8_t)n;
    } else {
        out[5]=(uint8_t)(l->settings.mode | (l->settings.speed<<3) | (l->settings.mini<<6));
        out[6]=l->settings.song; /* width is encoded in one byte, in 16-column units */
        out[7]=(uint8_t)(l->width/16);
        encode_v6(l,out+8);
    }
    h=checksum(out,8+ED_OLD_CELLS);
    for(i=0;i<4;i++) out[8+ED_OLD_CELLS+i]=(uint8_t)(h>>(8*i));
    return 1;
}
int ed_decode(EditorLevel *level, const uint8_t in[ED_SAVE_BYTES]) {
    uint32_t h, stored=0; unsigned i, version, prev=0, width=ED_OLD_WIDTH;
    EditorSettings settings={ED_MODE_CUBE,ED_SPEED_X1,0,ED_SONG_STEREOMA};
    if(!level || !in || memcmp(in,"GDED",4)) return 0;
    version=in[4]; if(version!=1 && version!=2 && version!=4 && version!=5 && version!=6) return 0;
    if(version!=5 && version!=6 && (in[5]!=ED_OLD_WIDTH || in[6]!=ED_HEIGHT || in[7])) return 0;
    if(version==5 || version==6) {
        settings.mode=in[5]&7; settings.speed=(in[5]>>3)&7; settings.mini=(in[5]>>6)&1;
        settings.song=in[6];
        if((in[5]&128) || !ed_settings_valid(&settings)) return 0;
        if(version==5 && in[7]>ED_V5_MAX_SPRITES) return 0;
        if(version==6) { width=(unsigned)in[7]*16; if(width<ED_OLD_WIDTH || width>ED_WIDTH) return 0; }
    }
    h=checksum(in,8+ED_OLD_CELLS);
    for(i=0;i<4;i++) stored|=(uint32_t)in[8+ED_OLD_CELLS+i]<<(8*i);
    if(h!=stored) return 0;
    if(version==6) {
        /* First pass validates everything before changing caller's level. */
        unsigned pos=0, count=0, total=width*ED_HEIGHT;
        while(count<total) {
            unsigned t,run=1,c=0,s=0;
            if(pos>=ED_OLD_CELLS) return 0;
            t=in[8+pos++];
            if(t<0x80) run=t+1;
            else if(t<0xc0) { run=(t&63)+1; if(pos>=ED_OLD_CELLS) return 0; c=in[8+pos++]; if(!c||c>=ED_KIND_COUNT) return 0; }
            else if(t<0xe0) { s=t&31; if(!s||s>=ED_SPR_COUNT || pos>=ED_OLD_CELLS) return 0; c=in[8+pos++]; if(c>=ED_KIND_COUNT) return 0; }
            else if(t<0xf0) { c=t&15; if(!c || c>=ED_KIND_COUNT) return 0; }
            else return 0;
            if(run>total-count) return 0;
            count+=run;
        }
        for(;pos<ED_OLD_CELLS;pos++) if(in[8+pos]) return 0;
        ed_clear(level); level->width=(uint16_t)width; level->settings=settings;
        pos=0; count=0;
        while(count<total) {
            unsigned t=in[8+pos++], run=1,c=0,s=0;
            if(t<0x80) run=t+1;
            else if(t<0xc0) { run=(t&63)+1; c=in[8+pos++]; }
            else if(t<0xe0) { s=t&31; c=in[8+pos++]; }
            else c=t&15;
            while(run--) { unsigned k=(count/width)*ED_WIDTH+count%width; level->cells[k]=(uint8_t)c; level->sprites[k]=(uint8_t)s; count++; }
        }
        return 1;
    }
    if(version==5) {
        for(i=0;i<ED_V5_MAX_SPRITES;i++) {
            unsigned e=in[8+ED_OLD_CELLS/2+i*2] | ((unsigned)in[8+ED_OLD_CELLS/2+i*2+1]<<8);
            unsigned index=e&511, kind=(e>>9)&31;
            if(i<in[7]) { if((e&0xc000) || !kind || kind>=ED_SPR_COUNT || (i && index<=prev)) return 0; prev=index; }
            else if(e) return 0;
        }
    } else for(i=0;i<ED_OLD_CELLS;i++) {
        unsigned c=in[8+i];
        if(version==1 && c>=3) return 0;
        if(version==2 && c>=ED_KIND_COUNT) return 0;
        if(version==4 && ((c&15)>=ED_KIND_COUNT || (c>>4)>=ED_SPR_COUNT)) return 0;
    }
    ed_clear(level); level->width=ED_OLD_WIDTH; level->settings=settings;
    for(i=0;i<ED_OLD_CELLS;i++) {
        unsigned c=in[8+i], k=(i/ED_OLD_WIDTH)*ED_WIDTH+i%ED_OLD_WIDTH;
        level->cells[k]=(uint8_t)(version==5 ? (in[8+i/2]>>(4*(i%2)))&15 : version==4 ? c&15 : c);
        level->sprites[k]=(uint8_t)(version==4 ? c>>4 : 0);
    }
    if(version==5) for(i=0;i<in[7];i++) {
        unsigned e=in[8+ED_OLD_CELLS/2+i*2] | ((unsigned)in[8+ED_OLD_CELLS/2+i*2+1]<<8), old=e&511;
        level->sprites[(old/ED_OLD_WIDTH)*ED_WIDTH+old%ED_OLD_WIDTH]=(uint8_t)((e>>9)&31);
    }
    return 1;
}

/* Single-threaded editor-only scratch; never put a 16 KiB level on the GBA
 * stack. Apply to a copy, including overlapping moves, then commit atomically. */
int ed_transfer(EditorLevel *level, unsigned layer, unsigned sx, unsigned sy,
                unsigned w, unsigned h, unsigned dx, unsigned dy, int copy) {
#if defined(__arm__)
    static EditorLevel temp __attribute__((section(".ewram")));
#else
    static EditorLevel temp;
#endif
    unsigned x,y,any=0;
    const uint8_t *src;
    uint8_t *out;
    if(!level || layer>ED_LAYER_SPRITE || (copy!=0 && copy!=1) ||
       !w || !h || level->width>ED_WIDTH || level->width<ED_OLD_WIDTH ||
       sx>=level->width || dx>=level->width || sy>=ED_HEIGHT || dy>=ED_HEIGHT ||
       w>level->width-sx || w>level->width-dx || h>ED_HEIGHT-sy || h>ED_HEIGHT-dy)
        return ED_TRANSFER_INVALID;
    if(!ed_can_encode(level)) return ED_TRANSFER_INVALID;
    src=layer==ED_LAYER_TERRAIN ? level->cells : level->sprites;
    for(y=0;y<h;y++) for(x=0;x<w;x++) {
        unsigned si=(sy+y)*ED_WIDTH+sx+x, tx=dx+x, ty=dy+y;
        if(!src[si]) continue;
        any=1;
        if(src[ty*ED_WIDTH+tx] &&
           (copy || tx<sx || tx>=sx+w || ty<sy || ty>=sy+h))
            return ED_TRANSFER_OCCUPIED;
    }
    if(!any) return ED_TRANSFER_EMPTY;
    if(!copy && sx==dx && sy==dy) return ED_TRANSFER_SAME;
    temp=*level;
    out=layer==ED_LAYER_TERRAIN ? temp.cells : temp.sprites;
    if(!copy) for(y=0;y<h;y++) for(x=0;x<w;x++)
        out[(sy+y)*ED_WIDTH+sx+x]=0;
    for(y=0;y<h;y++) for(x=0;x<w;x++) {
        unsigned si=(sy+y)*ED_WIDTH+sx+x;
        if(src[si]) out[(dy+y)*ED_WIDTH+dx+x]=src[si];
    }
    if(!ed_can_encode(&temp)) return ED_TRANSFER_CAPACITY;
    *level=temp;
    return ED_TRANSFER_OK;
}

int ed_bank_read(const uint8_t *bank, EditorLevel *level, uint32_t *sequence) {
    unsigned i; int zero=1, ff=1;
    uint32_t stored=0, seq=0;
    /* EditorLevel is large. Keep the validation scratch out of the
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
int ed_bank_encode(uint8_t *bank, const EditorLevel *level, uint32_t sequence) {
    uint32_t h; unsigned i;
    if(!bank || !ed_can_encode(level)) return 0;
    memcpy(bank,"GDM3",4);
    for(i=0;i<4;i++) bank[4+i]=(uint8_t)(sequence>>(8*i));
    ed_encode(level,bank+8);
    h=checksum(bank,ED_BANK_BYTES-4);
    for(i=0;i<4;i++) bank[ED_BANK_BYTES-4+i]=(uint8_t)(h>>(8*i));
    return 1;
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
