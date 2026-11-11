#ifndef GDA_EDITOR_TRACE_H
#define GDA_EDITOR_TRACE_H
#include <stdint.h>
#include "editor_core.h"
/* One recorded player-centre position per four horizontal pixels. A bucket
 * stores the latest sample, including its 2-bit x remainder. No interpolation. */
#define ED_TRACE_BUCKETS (ED_WIDTH * 4)
#define ED_TRACE_EMPTY 0xffffu
void ed_trace_clear(void);
void ed_trace_sample(int32_t x, int32_t y, unsigned width);
int ed_trace_get(unsigned bucket, int *x, int *y);
#endif
