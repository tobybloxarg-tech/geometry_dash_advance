#ifndef GDA_FAN_LEVELS_H
#define GDA_FAN_LEVELS_H
#include "editor_core.h"
/* Original short courses, not official ports and not save-backed. */
#define FAN_LEVEL_COUNT 2
int fan_level_build(EditorLevel *level, unsigned id);
int fan_play_active(void);
void fan_play_set_active(int active);
void fan_menu_loop(void);
#endif
