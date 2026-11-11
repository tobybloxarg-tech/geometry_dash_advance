#include "game_options.h"

uint8_t option_low_graphics;
uint8_t option_show_fps;

/* Only the VBlank handler updates ticks/frames; byte accesses are atomic on
 * ARM7. Main changes active only outside the IRQ and reads an atomic published sample.
 * 120 VBlanks = ~2.009 seconds of GBA time. No new IRQ/timer is installed. */
static volatile uint8_t active, ticks, frames;
static volatile uint16_t published; /* high byte = sequence, low = count */
static uint16_t read_sequence;
static int last_fps = -1;
void fps_stop(void) { active = 0; }
void fps_start(void) {
    active = 0;
    ticks = frames = 0;
    read_sequence = published & 0xff00u;
    last_fps = -1;
    active = option_show_fps;
}
void fps_vblank(int presented) {
    if (!active) return;
    if (presented) ++frames;
    if (++ticks == 120) {
        /* Aligned 16-bit load/store is atomic on ARM7; avoids mixing count
         * with a different sequence if an IRQ interrupts fps_read(). */
        published = (uint16_t)((((published >> 8) + 1u) << 8) | frames);
        ticks = frames = 0;
    }
}
int fps_read(void) {
    uint16_t sample = published;
    if ((sample & 0xff00u) != read_sequence) {
        unsigned n = sample & 0xffu;
        /* GBA refresh = 16777216 / (1232 * 228) = 59.7275 Hz.
         * Integer rounded estimate of n * 59.73 / 120. */
        last_fps = (n * 5973u + 6000u) / 12000u;
        read_sequence = sample & 0xff00u;
    }
    return last_fps;
}
