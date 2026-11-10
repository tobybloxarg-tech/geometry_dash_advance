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
/* Sprite layer is independent of terrain; one sprite per 16x16 anchor cell. */
enum { ED_SPR_EMPTY, ED_SPR_YELLOW_ORB, ED_SPR_BLUE_ORB, ED_SPR_PINK_ORB,
       ED_SPR_GREEN_ORB, ED_SPR_BLACK_ORB, ED_SPR_RED_ORB,
       ED_SPR_SHIP, ED_SPR_CUBE, ED_SPR_BALL, ED_SPR_UFO, ED_SPR_WAVE,
       ED_SPR_GRAVITY_BLUE, ED_SPR_GRAVITY_YELLOW, ED_SPR_MINI, ED_SPR_BIG,
       ED_SPR_SPEED_X05, ED_SPR_SPEED_X1, ED_SPR_SPEED_X2,
       ED_SPR_SPEED_X3, ED_SPR_SPEED_X4, ED_SPR_COUNT };
extern const uint16_t ed_sprite_type[ED_SPR_COUNT];
/* Stable editor IDs: mode/speed match player.h enum values. Song is a stable
 * index, NOT a generated Maxmod ID; resolve with MOD_##name at build time. */
enum { ED_MODE_CUBE, ED_MODE_SHIP, ED_MODE_BALL, ED_MODE_UFO, ED_MODE_WAVE, ED_MODE_COUNT };
enum { ED_SPEED_X05, ED_SPEED_X1, ED_SPEED_X2, ED_SPEED_X3, ED_SPEED_X4, ED_SPEED_COUNT };
#define ED_SONG_LIST(X) \
    X(STEREOMA, "STEREO MADNESS") X(BACKOT, "BACK ON TRACK") \
    X(POLAR, "POLARGEIST") X(DRY, "DRY OUT") X(BASEAB, "BASE AFTER BASE") \
    X(CANTLG, "CANT LET GO") X(JUMPER, "JUMPER") X(TIME, "TIME MACHINE") \
    X(CYCLES, "CYCLES") X(XSTEP, "XSTEP") X(CLTRFNK, "CLUTTERFUNK") \
    X(TOE, "THEORY OF EVERYTHING") X(ELECTRO, "ELECTROMAN") \
    X(CLUBSTEP, "CLUBSTEP") X(DYNAMIX, "ELECTRODYNAMIX") \
    X(HEXAGON, "HEXAGON FORCE") X(BLASTP, "BLAST PROCESSING") \
    X(DEATHMOON, "DEATH MOON") X(PROBLEM, "PROBLEMATIC") \
    X(RTAY, "GOLDEN HAZE") X(PYRO, "PYROPHORIC") X(ROSE, "ROSE") \
    X(EVERYEND, "EVERY END") X(GROUND, "GROUND TO SPACE") \
    X(ULTIMATE, "ULTIMATE DESTRUCTION") X(SELECTPAYMENT, "SELECT PAYMENT") \
    X(CLOWNPARTY, "CLOWN PARTY") X(FINALBATTLE, "FINAL BATTLE")
#define ED_SONG_ENUM(name, label) ED_SONG_##name,
enum { ED_SONG_LIST(ED_SONG_ENUM) ED_SONG_COUNT };
#undef ED_SONG_ENUM
typedef struct { uint8_t mode, speed, mini, song; } EditorSettings;
typedef struct { uint8_t cells[ED_CELLS]; uint8_t sprites[ED_CELLS]; EditorSettings settings; } EditorLevel;
/* v5 has room for 128 sparse sprites, maximum kind 31. v4 is retained for
 * default settings and 4-bit sprites, including dense old levels. */
#define ED_V5_MAX_SPRITES 128
int ed_settings_valid(const EditorSettings *settings);
int ed_can_encode(const EditorLevel *level);
int ed_sprite_put(EditorLevel *level, unsigned x, unsigned y, unsigned kind);
int ed_sprite_get(const EditorLevel *level, unsigned x, unsigned y);
/* UI state machine uses abstract button bits so it can be tested off-GBA. */
enum { ED_SCREEN_EDIT, ED_SCREEN_MENU, ED_SCREEN_CONFIRM_NEW, ED_SCREEN_SETTINGS };
enum { ED_INPUT_SELECT = 1, ED_INPUT_A = 2, ED_INPUT_B = 4, ED_INPUT_START = 8 };
enum { ED_ACTION_NONE, ED_ACTION_NEW, ED_ACTION_EXIT };
int ed_menu_input(int *screen, unsigned input);
void ed_clear(EditorLevel *level);
int ed_put(EditorLevel *level, unsigned x, unsigned y, unsigned kind);
int ed_get(const EditorLevel *level, unsigned x, unsigned y);
int ed_encode(const EditorLevel *level, uint8_t out[ED_SAVE_BYTES]);
int ed_decode(EditorLevel *level, const uint8_t in[ED_SAVE_BYTES]);
/* Two independently checked banks per slot, within the 32 KiB SRAM window. */
#define ED_SLOT_COUNT 6
#define ED_BANK_BYTES (8 + ED_SAVE_BYTES + 4)
enum { ED_BANK_EMPTY, ED_BANK_VALID, ED_BANK_UNKNOWN };
int ed_bank_read(const uint8_t *bank, EditorLevel *level, uint32_t *sequence);
int ed_bank_encode(uint8_t *bank, const EditorLevel *level, uint32_t sequence);
int ed_choose_bank(const uint8_t *a, const uint8_t *b, int *writable);
#endif
