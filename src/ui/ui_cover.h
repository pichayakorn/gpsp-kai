/* ui_cover.h - preset covers (spec section 5) */

#ifndef UI_COVER_H
#define UI_COVER_H

#ifndef UI_COVER_HOST
#include "../common.h"
#endif

/* Draw the cover art for a title into a size x size square of dst: the art
 * alone. From 32 px up the caller puts the name on a plate at the bottom
 * (spec section 5), in ui_cover_plate_color(). */
void ui_cover_art(u16 *dst, int pitch, int size, const char *title);
u16 ui_cover_plate_color(const char *title);
u32 ui_cover_seed(const char *title);

#endif
