#include <tonc.h>
#include <maxmod.h>
#include "main.h"
#include "level_select.h"
#include "editor_play.h"
#include "sprite_loading.h"
#include "memory.h"
#include "editor_trace.h"
#include "physics_defines.h"

/* Keep the last stock level selection intact: no synthetic level ID ever
 * enters level_defines[] or saved_levels[]. */
static u16 previous_level_id;
static int test_invincible;
void editor_test_set_invincible(int enabled) { test_invincible = !!enabled; }
int editor_test_invincible(void) { return ed_play_active() && test_invincible; }
#ifdef DEBUG
static u8 previous_noclip, previous_debug_mode, previous_hitbox_display;
#endif
int editor_play_start(const EditorLevel *level) {
    if (!ed_play_prepare(level)) return 0;
    ed_trace_clear();
    previous_level_id = loaded_level_id;
#ifdef DEBUG
    previous_noclip = noclip;
    previous_debug_mode = debug_mode;
    previous_hitbox_display = hitbox_display;
    debug_mode = hitbox_display = FALSE;
    noclip = test_invincible ? TRUE : FALSE;
#endif
    loaded_level_id = 0; /* finite stock ID; data is intercepted by load_level */
    mmStop();
    REG_BLDCNT = BLD_BUILD(BLD_OBJ, BLD_BG0 | BLD_BG1 | BLD_BG2, BLD_MODE(1));
    REG_BLDALPHA = BLDA_BUILD(16, 16);
    irq_disable(II_HBLANK);
    update_flags = UPDATE_NONE;
    frame_finished = FALSE;
    clear_checkpoints();
    in_practice_mode = FALSE;
    complete_cutscene = FALSE;
    cutscene_frame = 0;
    screen_mirrored = FALSE;
    screen_mirrored_transition = FALSE;
    swap_queue = FALSE;
    game_state = STATE_PLAYING;
    return 1;
}
void editor_play_finish(void) {
    if (!ed_play_active()) return;
    loaded_level_id = previous_level_id;
#ifdef DEBUG
    noclip = previous_noclip;
    debug_mode = previous_debug_mode;
    hitbox_display = previous_hitbox_display;
#endif
    /* No editor stream or queued upload may survive into stock play. */
    sprite_pointer = 0;
    last_sprite_x = 0;
    gamemode_upload_buffer[ID_PLAYER_1] = -1;
    gamemode_upload_buffer[ID_PLAYER_2] = -1;
    loaded_object_buffer_offset = unloaded_object_buffer_offset = 0;
    for (int i = 0; i < MAX_OBJECTS; ++i) object_buffer[i].occupied = FALSE;
    ed_play_end();
}

/* Tile 1022 is otherwise unused in gameplay (1021 is debug hitbox art).
 * Bank 7 is unused by the editor's permitted objects. Only test play changes
 * them; the game reloads its sprite palette/art for the next level. */
static const u16 trace_dot[] = {
    ATTR0_4BPP | ATTR0_SQUARE, ATTR1_SIZE_8x8,
    ATTR2_PALBANK(7) | ATTR2_ID(1022), 0, 0, 0, 0, 0xffff
};
void editor_trace_graphics_init(void) {
    if (!ed_play_active()) return;
    for (int y=0; y<8; ++y) tile_mem_obj[0][1022].data[y] = y<3 ? 0x00000111 : 0;
    palette_buffer[256+7*16+1] = 0x03e0;
}
void editor_trace_draw_play(void) {
    if (!ed_play_active() || complete_cutscene) return;
    /* Recent visible path, bounded to 30 OBJ sprites and 96 total. Every
     * point represents an actual sampled position, no synthesized segments. */
    s64 scrollwide = (s64)scroll_x >> SUBPIXEL_BITS;
    s32 scrollyp = (s32)(scroll_y >> SUBPIXEL_BITS);
    if (scrollwide <= -240 || scrollwide >= ED_WIDTH*16) return;
    s32 scrollxp = (s32)scrollwide;
    unsigned first = scrollxp < 0 ? 0 : (unsigned)scrollxp / 4;
    unsigned last = first + 62, drawn = 0;
    if (last > ED_TRACE_BUCKETS) last=ED_TRACE_BUCKETS;
    for (unsigned b=first; b<last && drawn<30 && nextSpr<96; b+=2) {
        int x,y;
        if (ed_trace_get(b,&x,&y) || (b+1<last && ed_trace_get(b+1,&x,&y))) {
            x-=scrollxp; y-=scrollyp;
            if (x>=0 && x<237 && y>=0 && y<157) {
                oam_metaspr(x,y,trace_dot,0,0,0,-1,0,0,1,1);
                ++drawn;
            }
        }
    }
}
