#ifndef GDA_EDITOR_PLAY_H
#define GDA_EDITOR_PLAY_H
#include <stdint.h>
#include "editor_core.h"

/* The 64 editable columns are followed by 10 empty columns: the stock
 * completion cutscene starts 152 pixels before the nominal level end. */
#define ED_PLAY_WIDTH (ED_WIDTH + 10)
/* Conservative cap: limits live OAM pressure as well as object slots. */
#define ED_PLAY_MAX_SPRITES 16
#define ED_PLAY_RECORD_WORDS 5
enum { ED_PLAY_OK, ED_PLAY_INVALID, ED_PLAY_TOO_MANY };

/* Stable UI values: modes cube/ship/ball/ufo/wave = 0..4, speed
 * 0.5x/1x/2x/3x/4x = 0..4, mini = 0/1, song = index into ED_SONG_LIST (not a generated MOD_* value). Call configure for
 * EVERY attempt before editor_play_start; prepare snapshots these values. */
#define ED_PLAY_MODE_COUNT 5
#define ED_PLAY_SPEED_COUNT 5
#define ED_PLAY_SONG_COUNT ED_SONG_COUNT
typedef struct { uint8_t mode, speed, mini, song; } EditorPlayConfig;
int ed_play_configure(unsigned mode, unsigned speed, unsigned mini, unsigned song);
EditorPlayConfig ed_play_config(void);

/* Pure data adapter. It owns a validated snapshot, so SRAM and editor RAM
 * are never modified while playing. Out-of-range columns are always air. */
int ed_play_prepare(const EditorLevel *level);
void ed_play_end(void);
int ed_play_active(void);
int ed_play_error(void);
uint16_t *ed_play_sprite_stream(void);
unsigned ed_play_sprite_words(void);
int ed_play_record_valid(const uint16_t *record, uint32_t last_x);
uint16_t ed_play_metatile(unsigned layer, unsigned x, unsigned y);

/* Runtime entry/exit, called only by the GBA game and editor UI. */
int editor_play_start(const EditorLevel *level);
void editor_play_finish(void);
#endif
