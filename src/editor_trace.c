#include "editor_trace.h"
#include <string.h>
#if defined(__arm__)
#define ED_TRACE_RAM __attribute__((section(".ewram")))
#else
#define ED_TRACE_RAM
#endif
static ED_TRACE_RAM uint16_t trace[ED_TRACE_BUCKETS];
void ed_trace_clear(void) { memset(trace, 0xff, sizeof trace); }
void ed_trace_sample(int32_t x, int32_t y, unsigned width) {
    if (width > ED_WIDTH || x < 0 || (uint32_t)x >= width*16u || y < 0 || y >= 16384) return;
    /* 0xffff marks empty; the single colliding combination is irrelevant
     * to the 2048px-tall game world but still must not wrap. */
    uint16_t packed = (uint16_t)((y << 2) | (x & 3));
    if (packed != ED_TRACE_EMPTY) trace[(uint32_t)x >> 2] = packed;
}
int ed_trace_get(unsigned bucket, int *x, int *y) {
    uint16_t v;
    if (bucket >= ED_TRACE_BUCKETS || (v=trace[bucket]) == ED_TRACE_EMPTY) return 0;
    if (x) *x=(int)(bucket*4u + (v&3));
    if (y) *y=(int)(v>>2);
    return 1;
}
