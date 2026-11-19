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
u32 ui_tap(u32 mask);        /* released within UI_F_HOLD_SHOW: not a hold */
u32 ui_held(u32 mask);       /* a hold completed this frame (UI_F_HOLD) */
u32 ui_hold_frames(u32 key); /* frames one key has been down */
#ifdef UI_PERF
extern u32 (*ui_input_script)(void);  /* keys from a script instead of the pad */
#endif
/* Moves index i of a list or strip of count items by the less/more keys
 * (with repeat). A fresh press wraps to the other end; a repeat stops at
 * the end, so the next fresh press wraps. */
int ui_step(int i, int count, u32 less, u32 more);
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
/* Drawing stays inside the clip rectangle; reset it after use. */
extern int ui_clip_x0, ui_clip_y0, ui_clip_x1, ui_clip_y1;
void ui_clip_set(int x, int y, int w, int h);
void ui_clip_reset(void);
void ui_ground_rect(int x, int y, int w, int h);

/* Frame: draw into the back buffer, then ui_frame_end() shows it at the
 * next vblank. */
void ui_frame_begin(void);
void ui_frame_end(void);
void ui_frame_end_now(void);

/* Drawing into the 480 x 272 menu buffer (16-bit, 5 bits per channel). */
#define UI_SCREEN_W 480
#define UI_SCREEN_H 272
void ui_fill(int x, int y, int w, int h, u16 color);
void ui_ground(void);
void ui_door(int x, int y, int w, int h);
void ui_checker(int x, int y, int w, int h, u16 color);
void ui_blit_scaled(const u16 *src, int sw, int sh, int x, int y, int w, int h);
void ui_shrink_box(const u16 *src, int sw, int sh, u16 *dst, int w, int h);
void ui_shrink_53_rows(const u16 *src, int rows, u16 *dst);  /* 240 wide to 144, 5:3 */
void ui_blit(const u16 *src, int pitch, int x, int y, int w, int h);
void ui_stretch_smooth(const u16 *src, int sw, int sh, int pitch, int x, int y, int w, int h);
void ui_layer_init(void);  /* once, after ui_pool_init() */
int ui_layer_begin(void);
void ui_layer_end(void);
void ui_layer_reset(void);
int ui_target_begin(u16 *buf, int pitch, int w, int h);
void ui_target_end(void);
int ui_layer_ready(void);
void ui_rect(int x, int y, int w, int h, int thickness, u16 color);
void ui_card(int x, int y, int w, int h, u16 color, int lifted);
int ui_text(int x, int y, char *str, u16 color);
int ui_text_width(char *str);
int ui_glyph(int x, int y, u32 key);
int ui_glyph_inverted(int x, int y, u32 key);  /* plum pill, cream marks */
int ui_glyph_width(u32 key);

typedef struct
{
  u32 keys;   /* one or more keys sharing the verb */
  char *verb;
} ui_hint_t;
void ui_hint_strip(const ui_hint_t *hints, int count);
/* Hints packed with a gap (spec: 10 px on the pause screen). Hints that
 * never change on a screen come first, the most changing one last, and a
 * hint that doesn't apply is left out. */
void ui_hint_packed(const ui_hint_t *hints, int count, int gap);

/* Easing: value at frame f (1..n) of an n-frame out-back or in-cubic move
 * from a to b. */
int ui_ease_out_back(int a, int b, int f, int n);
int ui_ease_in_cubic(int a, int b, int f, int n);
int ui_ease_out_cubic(int a, int b, int f, int n);

/* Components (ui_comp.c, spec section 2). Each draws one frame from the
 * state it is given; f arguments are frames since the change started. */
void ui_round_fill(int x, int y, int w, int h, int r, u16 color);
void ui_round_frame(int x, int y, int w, int h, int r, u16 color);
void ui_cursor_frame(int x, int y, int w, int h);
void ui_line(int x0, int y0, int x1, int y1, u16 color);
void ui_disc(int cx, int cy, int r, u16 color);
void ui_ring(int cx, int cy, int r, int t, u16 color);  /* t px thick, outer radius r */

#define UI_ROW_CHANGED  1  /* not the default: a tangerine dot in the margin */
#define UI_ROW_PER_GAME 2  /* saved for this game only: the cartridge mark */
#define UI_ROW_DISABLED 4  /* no card, mute label */
void ui_per_game_mark(int x, int y, u16 color);
void ui_card_row(int x, int y, int w, const char *label, int selected, u16 group,
 int flags, int *vx, int *vy);
void ui_row_note(int xr, int y, const char *note);
void ui_row_cycle(int xr, int y, const char *value, int selected, u16 group, int f);
/* Segment labels drawn as button discs (Confirm button row) */
#define UI_SEG_CROSS  "\x01x"
#define UI_SEG_CIRCLE "\x01o"
void ui_row_segments(int xr, int y, const char **labels, int count, int k,
 int from, int f, int selected, u16 group);
void ui_row_switch(int xr, int y, int p, int selected);
int ui_switch_pos(int on, int f);
void ui_row_blocks(int xr, int y, int n, int k, const char *text, int f,
 int selected, u16 group);
void ui_row_action(int xr, int y, int selected);

int ui_tabs(int x, const char **labels, int count, int k, int prev, int f);

int ui_toast(const char *text, u16 edge, const char *undo, int f);
int ui_toast_at(const char *text, u16 edge, const char *undo, int f, int y);
int ui_toast_up(int f);
/* The clock and battery chip, top right (pause, settings). ui_status_read()
 * reads everything once, when a screen opens; the chip then refreshes a
 * value a frame, over each second. */
void ui_status_read(void);
void ui_status_chip(void);
void ui_status_chip_at(int y);
void ui_game_toast(const char *text, u16 edge);  /* over the game, after the pause */
void ui_game_battery(int level, int on_battery);  /* once a second in a game */
/* Every frame in a game. fps < 0: Show FPS off. ff: 0 off, 1 Max, 2 or 3
 * the fixed speed. */
void ui_game_status(int fps, int ff);

typedef struct
{
  int p;      /* progress, 1/256 */
  int fired;  /* the hold completed and the key is still down */
  int done;   /* the hold completed this frame */
  int shown;  /* held past UI_F_HOLD_SHOW: its card shows until it drains */
} ui_hold_t;
int ui_hold_step(ui_hold_t *h, u32 key);
void ui_hold_bar(int x, int y, int w, int p, u16 color);

int ui_wrap_line(const char *text, int n, int max_width, int font, char *out, int out_size);
void ui_tip_line(const char *text, int k);  /* k: 1..8 frames into its fade */
void ui_help_card(const char *title, const char *body, u16 edge, int f, int closing);
int ui_error_card(const char *title, const char *body, const char *ok, const char *cancel,
 u16 edge);  /* 1: the first button, 0: closed */
void ui_dialog(const char *title, const char *body, const char **buttons,
 int button_count, int selected, u16 edge, int f, int closing);

/* Step 1 benchmark screen: hold R while kai starts. */
void ui_bench(void);
/* Step 2 component screen: hold L while kai starts. */
void ui_components(void);

/* The pause screen (the menu button in a game); returns 1 when the game's
 * state changed (a load or a restart), like menu(). */
u32 ui_pause(u16 *game_picture);

/* The Library (ui_library.c). ui_library() lets the player pick a game and
 * returns 1 with its folder and file name, and in *slot the savestate to
 * resume once it has started (Continue: 10 Last or 0-9; -1 from the
 * start), 0 without pool memory for it.
 * ui_library_switch() is the pause screen's Library: it leaves the game
 * running (the caller has saved Last), loads the one picked and returns 1,
 * or 0 when the library could not open. */
int ui_library(char *dir, int dir_size, char *name, int name_size, int *slot);
void ui_library_resume(int slot);  /* slot from ui_library(): -1 from the start */
u32 ui_library_switch(void);
u32 ui_library_switch_to(const char *rom_name, const char *state_file, int slot);
int ui_library_has_rom(const char *rom_name);
/* Cheats (ui_cheats.c), from the pause screen's Cheats tile. */
void ui_cheats(void);
extern int ui_no_bios;  /* the built-in BIOS is in use: the library's banner */
void ui_display_name(const char *file, char *out);  /* out: 256 bytes */
s32 ui_library_load(const char *dir, char *name);  /* with the Loading card */
void ui_startup_hook(int current, int total);  /* kai's start-up steps */
void ui_first_run(void);  /* the first-run cards, once (ui_first.c) */

/* Settings (from the pause screen's Settings tile), opened on a tab and row
 * (0, 0: the first). Returns 1 when the player asked to restart kai, 2 to
 * quit kai, 0 on back. The caller draws its own ground again. */
int ui_settings(int tab, int row);
void ui_settings_where(int *tab, int *row);
int ui_settings_saved_for_game(const game_config_t *cfg);
extern const u16 *ui_settings_game;  /* the game picture for Controls (240 x 160) */
extern u32 ui_start_language;  /* the language kai started with */

/* Restart kai (ui_restart.c): the note that brings the player back where
 * they were, and the relaunch itself. */
typedef struct
{
  int pending;      /* a note was read at this start */
  int tab, row;     /* Settings tab and row */
  char game[MAX_PATH];  /* the game to reopen from Last; empty: none */
} ui_restart_note_t;
extern ui_restart_note_t ui_restart_note;
void ui_restart_note_read(void);
void ui_restart_now(int tab, int row, const char *game);
void ui_restart_exec(void);  /* from quit(3) */

/* Sleep: set by the power callback, handled by the pause screen. */
extern volatile int ui_power_locked;
extern volatile int ui_woke;

#endif
