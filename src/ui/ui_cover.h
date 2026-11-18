/* ui_cover.h - preset covers (spec section 5) */

#ifndef UI_COVER_H
#define UI_COVER_H

#ifndef UI_COVER_HOST
#include "../common.h"
#endif

/* Draw the cover art for a title into a size x size square of dst. From
 * 32 px up its bottom ui_cover_band(size) rows are the dark title band;
 * the caller writes the title on it (white, the 10 px font, centred). */
void ui_cover_art(u16 *dst, int pitch, int size, const char *title);
int ui_cover_band(int size);
u16 ui_cover_band_color(const char *title);
u32 ui_cover_seed(const char *title);

#endif
