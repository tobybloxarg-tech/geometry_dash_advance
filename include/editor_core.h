#ifndef GDA_EDITOR_CORE_H
#define GDA_EDITOR_CORE_H
#include <stddef.h>
#include <stdint.h>
#define ED_WIDTH 64
#define ED_HEIGHT 8
#define ED_CELLS (ED_WIDTH * ED_HEIGHT)
#define ED_SAVE_BYTES (8 + ED_CELLS + 4)
/* Stable editor symbols, not the engine's actual metatile IDs. */
enum { ED_EMPTY, ED_BLOCK, ED_SPIKE, ED_DARK, ED_CHECK, ED_SLAB_UP,
       ED_SLAB_DOWN, ED_SLAB_LEFT, ED_SLAB_RIGHT, ED_SPIKE_DOWN,
       ED_SPIKE_RIGHT, ED_SPIKE_LEFT, ED_SMALL_UP, ED_SMALL_DOWN,
       ED_MEDIUM_UP, ED_MEDIUM_DOWN, ED_KIND_COUNT };
/* Index is a stable editor symbol; result is an actual metatiles[] index. */
extern const uint16_t ed_metatile[ED_KIND_COUNT];
typedef struct { uint8_t cells[ED_CELLS]; } EditorLevel;
/* UI state machine uses abstract button bits so it can be tested off-GBA. */
enum { ED_SCREEN_EDIT, ED_SCREEN_MENU, ED_SCREEN_CONFIRM_NEW };
enum { ED_INPUT_SELECT = 1, ED_INPUT_A = 2, ED_INPUT_B = 4, ED_INPUT_START = 8 };
enum { ED_ACTION_NONE, ED_ACTION_NEW, ED_ACTION_EXIT };
int ed_menu_input(int *screen, unsigned input);
void ed_clear(EditorLevel *level);
int ed_put(EditorLevel *level, unsigned x, unsigned y, unsigned kind);
int ed_get(const EditorLevel *level, unsigned x, unsigned y);
void ed_encode(const EditorLevel *level, uint8_t out[ED_SAVE_BYTES]);
int ed_decode(EditorLevel *level, const uint8_t in[ED_SAVE_BYTES]);
/* Two independently checked banks per slot, within the 32 KiB SRAM window. */
#define ED_SLOT_COUNT 6
#define ED_BANK_BYTES (8 + ED_SAVE_BYTES + 4)
enum { ED_BANK_EMPTY, ED_BANK_VALID, ED_BANK_UNKNOWN };
int ed_bank_read(const uint8_t *bank, EditorLevel *level, uint32_t *sequence);
void ed_bank_encode(uint8_t *bank, const EditorLevel *level, uint32_t sequence);
int ed_choose_bank(const uint8_t *a, const uint8_t *b, int *writable);
#endif
