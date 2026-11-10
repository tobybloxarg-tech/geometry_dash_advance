#include "editor_core.h"
#include <string.h>
void ed_clear(EditorLevel *level) { memset(level->cells, 0, sizeof(level->cells)); }
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
void ed_encode(const EditorLevel *level, uint8_t out[ED_SAVE_BYTES]) {
    const uint8_t header[8] = {'G','D','E','D',1,ED_WIDTH,ED_HEIGHT,0};
    memcpy(out,header,8); memcpy(out+8,level->cells,ED_CELLS);
    uint32_t h = checksum(out, 8+ED_CELLS);
    for (unsigned i=0;i<4;i++) out[8+ED_CELLS+i]=(uint8_t)(h>>(8*i));
}
int ed_decode(EditorLevel *level, const uint8_t in[ED_SAVE_BYTES]) {
    const uint8_t header[8] = {'G','D','E','D',1,ED_WIDTH,ED_HEIGHT,0};
    if (memcmp(in,header,8)) return 0;
    uint32_t h=checksum(in,8+ED_CELLS), stored=0;
    for (unsigned i=0;i<4;i++) stored|=(uint32_t)in[8+ED_CELLS+i]<<(8*i);
    if (h!=stored) return 0;
    for (unsigned i=0;i<ED_CELLS;i++) if (in[8+i]>=ED_KIND_COUNT) return 0;
    memcpy(level->cells,in+8,ED_CELLS); return 1;
}
