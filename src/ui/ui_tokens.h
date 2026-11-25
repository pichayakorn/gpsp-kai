/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Hachi
 */

/* Colours, sizes and timings from the 4.0 design, shared by every screen. */
#ifndef UI_TOKENS_H
#define UI_TOKENS_H

#define UI_COL_PLUM       COLOR16( 5,  3,  9)
#define UI_COL_PLUM_2     COLOR16( 6,  4, 11)
#define UI_COL_CARD       COLOR16(10,  7, 17)
#define UI_COL_SHADE      COLOR16( 2,  1,  4)
#define UI_COL_CREAM      COLOR16(31, 29, 24)
#define UI_COL_MUTE       COLOR16(19, 16, 22)
#define UI_COL_TANGERINE  COLOR16(31, 17,  4)
#define UI_COL_SKY        COLOR16( 9, 22, 31)
#define UI_COL_LIME       COLOR16(19, 28,  6)
#define UI_COL_PINK       COLOR16(31, 11, 19)

#define UI_FONT_BODY_PX   12
#define UI_FONT_BODY_LINE 16
#define UI_FONT_SMALL_PX  10
#define UI_FONT_SMALL_LINE 14
#define UI_TITLE_SCALE    2

#define UI_MARGIN      20
#define UI_HEADER_H    34
#define UI_ROW_H       20
#define UI_ROW_PITCH   26
#define UI_ROW_PITCH_COMPACT 22
#define UI_LIFT         2
#define UI_SHADOW       3
#define UI_SHADOW_MODAL 4
#define UI_STRIP_Y    248
#define UI_STRIP_H     24
#define UI_CHECK        8

#define UI_F_CURSOR         8
#define UI_F_SCROLL        12
#define UI_F_PAGE          14
#define UI_F_VALUE          8
#define UI_F_DIALOG_IN     12
#define UI_F_DIALOG_OUT     8
#define UI_F_TOAST_IN      12
#define UI_F_TOAST_HOLD   120
#define UI_F_TOAST_OUT      8
#define UI_F_LOADING        6
#define UI_F_TAB           12
#define UI_F_POPUP_IN      10
#define UI_F_POPUP_OUT      8
#define UI_F_PAUSE_OPEN    20
#define UI_F_PAUSE_CLOSE   10
#define UI_F_RESUME_FADE    8
#define UI_F_HOLD_SHOW     12
#define UI_F_FLASH          8
#define UI_F_HOLD          60
#define UI_F_HOLD_DRAIN    10
#define UI_F_SAVING        30
#define UI_F_CARD_RISE     10
#define UI_F_TIP_DELAY     18
#define UI_F_LETTER_POP    40
#define UI_F_RAIL_IN        8
#define UI_F_RAIL_OUT       8
#define UI_F_HELP_IN       12
#define UI_F_HELP_OUT       8
#define UI_F_LIST_SCROLL    8
#define UI_F_LIST_REST     10
#define UI_F_LIST_OPEN      8
#define UI_F_LIST_CLOSE     8
#define UI_F_FIRST_RUN     12
#define UI_MARQUEE_DELAY 40
#define UI_MARQUEE_SPEED  1
#define UI_MARQUEE_GAP   32
#define UI_F_REPEAT_DELAY 18
#define UI_F_REPEAT_EVERY  4

#ifdef __GNUC__
#define UI_TOKENS_UNUSED __attribute__((unused))
#else
#define UI_TOKENS_UNUSED
#endif
static const unsigned short UI_EASE_OUT_BACK_8[8] UI_TOKENS_UNUSED = { 119, 198, 246, 269, 274, 269, 260, 256 };
static const unsigned short UI_EASE_OUT_BACK_10[10] UI_TOKENS_UNUSED = { 98, 171, 221, 252, 269, 274, 272, 265, 259, 256 };
static const unsigned short UI_EASE_OUT_BACK_12[12] UI_TOKENS_UNUSED = { 84, 149, 198, 233, 256, 269, 274, 273, 269, 263, 258, 256 };
static const unsigned short UI_EASE_OUT_BACK_14[14] UI_TOKENS_UNUSED = { 73, 132, 179, 215, 241, 258, 269, 273, 274, 271, 266, 262, 258, 256 };
static const unsigned short UI_EASE_OUT_BACK_20[20] UI_TOKENS_UNUSED = { 53, 98, 138, 171, 198, 221, 239, 252, 262, 269, 273, 274, 274, 272, 269, 265, 262, 259, 257, 256 };
static const unsigned short UI_EASE_IN_CUBIC_8[8] UI_TOKENS_UNUSED = { 0, 4, 14, 32, 62, 108, 172, 256 };
static const unsigned short UI_EASE_IN_CUBIC_10[10] UI_TOKENS_UNUSED = { 0, 2, 7, 16, 32, 55, 88, 131, 187, 256 };
static const unsigned short UI_EASE_OUT_CUBIC_8[8] UI_TOKENS_UNUSED = { 84, 148, 194, 224, 242, 252, 256, 256 };
static const unsigned short UI_EASE_OUT_CUBIC_10[10] UI_TOKENS_UNUSED = { 69, 125, 168, 201, 224, 240, 249, 254, 256, 256 };

#endif
