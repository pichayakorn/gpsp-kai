/* gpSP kai 4.0 app UI (built with UI4=1). See the spec in gpsp-kai-design
 * app-ui/spec/README.md; colours, sizes and motion come from ui_tokens.h. */
#ifndef UI_H
#define UI_H

#include "ui_tokens.h"

/* Memory pool, reserved before the game buffer so the UI never competes
 * with it (on a PSP-1000 the game buffer takes all the RAM left). Allocations
 * are a stack: ui_pool_mark() before, ui_pool_release() to free back. */
void ui_pool_init(void);
void *ui_pool_alloc(u32 size);
u32 ui_pool_mark(void);
void ui_pool_release(u32 mark);
u32 ui_pool_free(void);
u32 ui_pool_size(void);

/* Input, read once per frame. Masks are PSP_CTRL_* bits; the stick also
 * moves like the d-pad. Keys held when ui_input_init() runs are ignored
 * until released. */
void ui_input_init(void);
void ui_input_update(void);
u32 ui_down(u32 mask);       /* held now */
u32 ui_pressed(u32 mask);    /* went down this frame */
u32 ui_released(u32 mask);   /* went up this frame */
u32 ui_repeat(u32 mask);     /* d-pad: on press, then after 18 f every 4 f */
u32 ui_tap(u32 mask);        /* released before a hold completed */
u32 ui_held(u32 mask);       /* a hold completed this frame (UI_F_HOLD) */
u32 ui_hold_frames(u32 key); /* frames one key has been down */
extern u32 ui_key_confirm;   /* × or ○, from the PSP's Enter setting */
extern u32 ui_key_back;

/* UI fonts (Fusion Pixel, data/font/ui): a Latin set at 10 and 12 px plus
 * the CJK set of the menu language, loaded into the pool. ui_font_init()
 * returns the bytes loaded, 0 if the Latin fonts are missing. */
#define UI_FONT_SMALL 0  /* 10 px, 14 px line */
#define UI_FONT_BODY  1  /* 12 px, 16 px line */
#define UI_TEXT_BOLD  1  /* double-struck, for Latin and Cyrillic only */
#define UI_TEXT_2X    2  /* drawn at twice the size (page titles) */
u32 ui_font_init(u32 language);
int ui_font_loaded(void);
u32 ui_font_bytes(void);
int ui_print(int x, int y, const char *utf8, u16 color, int font, int flags);
int ui_print_width(const char *utf8, int font, int flags);
int ui_print_fit(int x, int y, const char *utf8, u16 color, int font, int flags, int max_width);

/* The frame being drawn (uncached VRAM), for the drawing code. */
extern u16 *ui_fb;
extern u32 ui_fb_pitch;

/* Frame: draw into the back buffer, then ui_frame_end() shows it at the
 * next vblank. */
void ui_frame_begin(void);
void ui_frame_end(void);

/* Drawing into the 480 x 272 menu buffer (16-bit, 5 bits per channel). */
#define UI_SCREEN_W 480
#define UI_SCREEN_H 272
void ui_fill(int x, int y, int w, int h, u16 color);
void ui_ground(void);
void ui_door(int x, int y, int w, int h);
void ui_rect(int x, int y, int w, int h, int thickness, u16 color);
void ui_card(int x, int y, int w, int h, u16 color, int lifted);
int ui_text(int x, int y, char *str, u16 color);
int ui_text_width(char *str);
int ui_glyph(int x, int y, u32 key);
int ui_glyph_width(u32 key);

typedef struct
{
  u32 keys;   /* one or more keys sharing the verb */
  char *verb;
} ui_hint_t;
void ui_hint_strip(const ui_hint_t *hints, int count);

/* Easing: value at frame f (1..n) of an n-frame out-back or in-cubic move
 * from a to b. */
int ui_ease_out_back(int a, int b, int f, int n);
int ui_ease_in_cubic(int a, int b, int f, int n);

/* Step 1 benchmark screen: hold R while kai starts. */
void ui_bench(void);

#endif
