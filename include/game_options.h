#pragma once
#include <stdint.h>

/* These are deliberately session-only; never stored in the game's SRAM. */
extern uint8_t option_low_graphics;
extern uint8_t option_show_fps;
void options_menu_loop(void);

/* FPS samples completed gameplay OAM presentations, divided by the number
 * of GBA VBlanks. This is emulated-GBA throughput, not host display FPS. */
void fps_start(void);
void fps_stop(void);
void fps_vblank(int presented);
int fps_read(void); /* -1 until the first complete sample */
void fps_graphics_init(void);
void fps_draw(void);
