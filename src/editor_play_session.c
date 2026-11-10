#include <tonc.h>
#include <maxmod.h>
#include "main.h"
#include "level_select.h"
#include "editor_play.h"
#include "sprite_loading.h"
#include "memory.h"

/* Keep the last stock level selection intact: no synthetic level ID ever
 * enters level_defines[] or saved_levels[]. */
static u16 previous_level_id;
#ifdef DEBUG
static u8 previous_noclip;
#endif
int editor_play_start(const EditorLevel *level) {
    if (!ed_play_prepare(level)) return 0;
    previous_level_id = loaded_level_id;
#ifdef DEBUG
    previous_noclip = noclip;
    noclip = FALSE;
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
#endif
    /* No editor stream or queued upload may survive into stock play. */
    sprite_pointer = 0;
    last_sprite_x = 0;
    loaded_object_buffer_offset = unloaded_object_buffer_offset = 0;
    for (int i = 0; i < MAX_OBJECTS; ++i) object_buffer[i].occupied = FALSE;
    ed_play_end();
}
