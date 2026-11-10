#ifndef GDA_EDITOR_CORE_H
#define GDA_EDITOR_CORE_H
#include <stddef.h>
#include <stdint.h>
#define ED_WIDTH 64
#define ED_HEIGHT 8
#define ED_CELLS (ED_WIDTH * ED_HEIGHT)
#define ED_SAVE_BYTES (8 + ED_CELLS + 4)
/* Stable editor symbols, not the engine's actual metatile IDs. */
enum { ED_EMPTY, ED_BLOCK, ED_SPIKE, ED_KIND_COUNT };
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
#endif
