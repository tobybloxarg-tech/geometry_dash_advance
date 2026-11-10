#include "editor_play.h"
#include <string.h>

#if defined(__arm__)
#define ED_EWRAM __attribute__((section(".ewram")))
#else
#define ED_EWRAM
#endif
static ED_EWRAM EditorLevel play_level;
static int active, error_code;
/* Five halfwords per object and one complete sentinel halfword. */
static ED_EWRAM uint16_t sprite_stream[ED_PLAY_MAX_SPRITES * ED_PLAY_RECORD_WORDS + 1];
static unsigned sprite_words;
static EditorPlayConfig pending_config = { 0, 1, 0, 0 };
static EditorPlayConfig play_config = { 0, 1, 0, 0 };

int ed_play_configure(unsigned mode, unsigned speed, unsigned mini, unsigned song) {
    if (active || mode >= ED_PLAY_MODE_COUNT || speed >= ED_PLAY_SPEED_COUNT ||
        mini > 1 || song >= ED_PLAY_SONG_COUNT) return 0;
    pending_config.mode = (uint8_t)mode;
    pending_config.speed = (uint8_t)speed;
    pending_config.mini = (uint8_t)mini;
    pending_config.song = (uint8_t)song;
    return 1;
}
EditorPlayConfig ed_play_config(void) { return active ? play_config : pending_config; }

unsigned ed_play_width(void) { return active ? play_level.width + 10 : 0; }

int ed_play_prepare(const EditorLevel *level) {
    unsigned i, x, y, count = 0, last_x = 0, n = 0;
    if (active || !level || level->width < ED_OLD_WIDTH || level->width > ED_WIDTH || level->width % 16) { error_code = ED_PLAY_INVALID; return 0; }
    /* Invalidate even after failure; never let a stale prior stream start. */
    sprite_words = 0;
    for (i = 0; i < ED_CELLS; ++i) {
        if (level->cells[i] >= ED_KIND_COUNT || level->sprites[i] >= ED_SPR_COUNT ||
            (i % ED_WIDTH >= level->width && (level->cells[i] || level->sprites[i]))) {
            error_code = ED_PLAY_INVALID; return 0;
        }
        if (level->sprites[i] && ++count > ED_PLAY_MAX_SPRITES) {
            error_code = ED_PLAY_TOO_MANY; return 0;
        }
    }
    memcpy(&play_level, level, sizeof play_level);
    play_config = pending_config;
    /* Exporter sorts by X then Y. Its Y is a Tiled tile object's bottom
     * edge, so add one cell to match the editor's top-left anchor. */
    for (x = 0; x < play_level.width; ++x) for (y = 0; y < ED_HEIGHT; ++y) {
        unsigned kind = play_level.sprites[y * ED_WIDTH + x];
        if (!kind) continue;
        sprite_stream[n++] = x * 16 - last_x;
        sprite_stream[n++] = (y + 1) * 16;
        sprite_stream[n++] = ed_sprite_type[kind];
        sprite_stream[n++] = 0x20;
        sprite_stream[n++] = 0;
        last_x = x * 16;
    }
    sprite_stream[n] = 0xffff;
    sprite_words = n;
    error_code = ED_PLAY_OK;
    active = 1;
    return 1;
}
int ed_play_error(void) { return error_code; }
uint16_t *ed_play_sprite_stream(void) { return active ? sprite_stream : 0; }
unsigned ed_play_sprite_words(void) { return active ? sprite_words : 0; }
int ed_play_record_valid(const uint16_t *record, uint32_t last_x) {
    uintptr_t p = (uintptr_t)record, base = (uintptr_t)sprite_stream;
    unsigned offset, k, found = 0;
    if (!active || p < base || p - base > sprite_words * sizeof(uint16_t) ||
        (p - base) % (ED_PLAY_RECORD_WORDS * sizeof(uint16_t))) return 0;
    offset = (unsigned)((p - base) / sizeof(uint16_t));
    if (sprite_words - offset < ED_PLAY_RECORD_WORDS) return 0;
    for (k = 1; k < ED_SPR_COUNT; ++k)
        if (record[2] == ed_sprite_type[k]) found = 1;
    return found && record[0] % 16 == 0 && last_x <= (play_level.width-1)*16 &&
           record[0] <= (play_level.width-1)*16 - last_x &&
           record[1] >= 16 && record[1] <= ED_HEIGHT*16 && record[1] % 16 == 0 &&
           record[3] == 0x20 && record[4] == 0;
}
void ed_play_end(void) { active = 0; }
int ed_play_active(void) { return active; }
uint16_t ed_play_metatile(unsigned layer, unsigned x, unsigned y) {
    unsigned kind;
    if (!active || layer != 0 || x >= play_level.width || y >= ED_HEIGHT) return 0;
    kind = play_level.cells[y * ED_WIDTH + x];
    return kind < ED_KIND_COUNT ? ed_metatile[kind] : 0;
}
