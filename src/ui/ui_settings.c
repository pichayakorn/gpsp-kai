/* The 4.0 Settings screen (spec section 4, "Settings"; step 4). Tabs
 * Display / Sound / Controls / Play / System; each row keeps kai's fixed
 * scope: global, or saved for this game only (the cartridge mark). A dot
 * marks a setting changed from its default; holding START resets the tab
 * (with Undo); the tip line explains the selected row once the cursor
 * rests. Rows bind straight to g_gpsp_config / g_game_config, so leaving
 * the screen applies them the way the old menu did (screen settings when
 * the game takes the LCD again, the CPU speed on resume, the rest live).
 *
 * Only the cursor, the selected row and its value move; every other row,
 * the legend and the hint strip are drawn into the ground layer, rebuilt
 * when the cursor moves, the list scrolls or the tab changes (spec section
 * 6: repeated actions move little, text is drawn once).
 *
 * Not here yet: the Controls tab (the drawn PSP and the picker), side bars
 * (#104), TV output, the Confirm button setting and Help; their rows show
 * greyed out. Texts are English placeholders until they move to the .msg
 * files. */
#include "../common.h"
#include "ui.h"

enum { K_SEG, K_SWITCH, K_BLOCKS, K_CYCLE, K_SCREEN, K_ACTION, K_OFF };

typedef struct
{
  const char *label;
  const char *tip;
  u8 kind;
  u8 per_game;
  u8 count;            /* choices (segments, blocks, cycle names) */
  const char *opts[4];
  u32 *value;          /* the config field */
  const u8 *map;       /* choice -> stored value; NULL: the same */
  int def;             /* the default stored value */
  int lo, hi;          /* K_CYCLE with numbers: the range */
  const char *fmt;     /* K_CYCLE with numbers: "%d frames" */
  const char **names;  /* K_CYCLE with names (count of them) */
} row_t;

#define TABS 5
#define MAX_ROWS 8
#define ROWS_Y 62
#define VIEW_T 60
#define VIEW_B 226
#define ROW_X  UI_MARGIN
#define ROW_W  (UI_SCREEN_W - UI_MARGIN * 2)
#define SCREEN_ROW_H 40

/* K_ACTION rows keep their action in def */
enum { ACT_RESET_ALL, ACT_RESTART };
#define LANG_TAB 4  /* System: the Language row is its first */
#define PAD_TAB 2   /* Controls */

static const u8 map_frameskip[3] = { no_frameskip, auto_frameskip, manual_frameskip };
static const u8 map_ff[2] = { FF_HOLD, FF_TOGGLE };
static const u8 map_clock[3] = { 10, 7, 9 };  /* 222, 266, 333 MHz */

static const char *language_names[12] =
{
  "Japanese", "English", "French", "Spanish", "German", "Italian",
  "Dutch", "Portuguese", "Russian", "Korean", "Chinese (traditional)",
  "Chinese (simplified)"
};

static row_t rows[TABS][MAX_ROWS] =
{
  { /* Display */
    { "Screen size", "Original keeps the GBA's pixels exact; Fit fills the height; Full fills the screen.",
      K_SCREEN, 0, 3, { "Original", "Fit", "Full" }, &g_gpsp_config.screen_mode, NULL, scaled_aspect },
    { "Smoothing", "Sharp keeps pixels crisp when the picture is scaled; Smooth softens them.",
      K_SEG, 0, 3, { "Off", "Smooth", "Sharp" }, &g_gpsp_config.screen_filter, NULL, filter_sharp_bilinear },
    { "Side bars", "What fills the space beside the picture in 3:2 sizes.",
      K_SEG, 0, 3, { "Black", "Pattern", "Game colours" }, &g_gpsp_config.side_bars, NULL, SIDE_BARS_BLACK },
    { "Frameskip", "Auto skips frames only when the game would slow down.",
      K_SEG, 1, 3, { "Off", "Auto", "Fixed" }, &g_game_config.frameskip_type, map_frameskip, auto_frameskip },
    { "Skip up to", "The most frames Auto skips in a row; Fixed skips this many.",
      K_CYCLE, 1, 0, { NULL }, &g_game_config.frameskip_value, NULL, 9, 0, 99, "%d frames" },
    { "Picture scale", "Below 100 % the picture shrinks inside its screen size.",
      K_CYCLE, 0, 0, { NULL }, &g_gpsp_config.screen_scale, NULL, 100, 50, 200, "%d%%" },
    { "TV output", NULL, K_OFF, 0, 0, { "Comes later" } },
  },
  { /* Sound */
    { "Sound", "Off mutes the game and runs a little faster.", K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.enable_audio, NULL, YES },
    { "Sound delay", "Less delay feels faster; more delay stops crackles in busy games.",
      K_BLOCKS, 1, 11, { NULL }, &g_game_config.audio_buffer_size_number, NULL, 2 },
  },
  { /* Controls: the Stick page; the Buttons page is drawn on its own */
    { "Use the stick", "The stick works like the d-pad, or does what you pick for each direction.",
      K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.enable_analog, NULL, NO },
    { "Sensitivity", "The ring shows where a push starts to count.",
      K_BLOCKS, 0, 10, { NULL }, &g_gpsp_config.analog_sensitivity_level, NULL, 4, 0, 0, "%d" },
    { "Tilt sensor", "For games made for a tilt cartridge: tilt the PSP to play.",
      K_SWITCH, 1, 2, { NULL }, &g_game_config.allocate_sensor, NULL, NO },
  },
  { /* Play */
    { "Save when leaving a game", "Continue on the Home screen picks up exactly here.",
      K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.save_on_leave, NULL, YES },
    { "Fast forward", "Hold: fast while the button is down. Toggle: one press on, one press off.",
      K_SEG, 0, 2, { "Hold", "Toggle" }, &g_gpsp_config.fast_forward_mode, map_ff, FF_TOGGLE },
    { "Fast forward speed", "Max runs as fast as the PSP can, so its speed changes with each game.",
      K_SEG, 0, 3, { "2\xC3\x97", "3\xC3\x97", "Max" }, &g_gpsp_config.ff_speed, NULL, FF_SPEED_3X },
    { "Sound while fast", "Off keeps fast forward quiet.", K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.fast_forward_sound, NULL, YES },
    { "Rapid-fire speed", "How fast a rapid-fire button presses while you hold it.", K_SEG, 1, 3, { "Slow", "Medium", "Fast" },
      &g_game_config.rapidfire_speed, NULL, RAPIDFIRE_FAST },
    { "Show FPS", "Shows the frames per second in the top-left corner.", K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.show_fps, NULL, NO },
  },
  { /* System */
    { "Language", "The language comes from your PSP until you pick one.",
      K_CYCLE, 0, 12, { NULL }, &g_gpsp_config.language, NULL, 1, 0, 0, NULL, language_names },
    { "CPU speed", "333 MHz runs every game at full speed; lower saves battery.",
      K_SEG, 1, 3, { "222", "266", "333 MHz" }, &g_game_config.clock_speed_number, map_clock, 9 },
    { "Write game saves", "Right away writes the game's save at once; On exit waits until you leave.",
      K_SEG, 1, 2, { "On exit", "Right away" }, &g_game_config.update_backup_flag, NULL, YES },
    { "Start games with", "GBA BIOS shows the GBA's start-up logo first; Game Cart skips it.", K_SEG, 0, 2, { "Game Cart", "GBA BIOS" }, &g_gpsp_config.boot_mode, NULL, GAME_CART },
    { "No-BIOS notice", "Shows a line in the library each time kai starts without a GBA BIOS.",
      K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.bios_note, NULL, YES },
    { "Confirm button", "PSP setting follows the console: \xC3\x97 on most PSPs, \xE2\x97\x8B on Japanese models.",
      K_SEG, 0, 3, { "PSP setting", UI_SEG_CROSS, UI_SEG_CIRCLE }, &g_gpsp_config.confirm_button, NULL, CONFIRM_SYSTEM },
    { "Restart kai", "Saves like Exit, starts kai again and brings you back here.",
      K_ACTION, 0, 0, { NULL }, NULL, NULL, ACT_RESTART },
    { "Reset all settings", "Puts every setting back to its default, for every game.",
      K_ACTION, 0, 0, { NULL }, NULL, NULL, ACT_RESET_ALL },
  },
};

static const char *tab_labels[TABS] = { "Display", "Sound", "Controls", "Play", "System" };

static int row_count(int tab)
{
  int n = 0;

  while((n < MAX_ROWS) && rows[tab][n].label)
    n++;
  return n;
}

static int row_height(const row_t *r)
{
  return (r->kind == K_SCREEN) ? SCREEN_ROW_H : UI_ROW_H;
}

/* Settings rows sit on a 26 px pitch (6 px between cards keeps the
 * selected card's cursor frame clear of the row above); the Stick page's
 * rows beside the stick use the compact 22 px one. */
static int row_pitch(int tab, const row_t *r)
{
  return row_height(r) + ((tab == PAD_TAB) ? UI_ROW_PITCH_COMPACT : UI_ROW_PITCH) - UI_ROW_H;
}

/* The choice shown for a stored value; -1 when it is none of them. */
static int row_index(const row_t *r)
{
  int i;

  if(!r->map)
    return (int)*r->value;
  for(i = 0; i < r->count; i++)
    if(r->map[i] == *r->value)
      return i;
  return -1;
}

static void row_set_index(row_t *r, int i)
{
  *r->value = r->map ? r->map[i] : (u32)i;
}

static int row_selectable(const row_t *r)
{
  return r->kind != K_OFF;
}

static int row_changed(const row_t *r)
{
  return r->value && ((int)*r->value != r->def);
}

/* ---- state ---------------------------------------------------------------- */

static struct
{
  int tab, tab_prev, tab_f;
  int cur, cursor_from, cursor_f, rest;
  int cursor_h_from;              /* the frame's height as it left the last row */
  int top;                        /* scroll: where the list rests */
  int top_from, top_f;            /* and where it slides from (scroll token) */
  int value_f, value_from;        /* the selected row's value motion */
  int dirty;                      /* the ground needs drawing again */
  ui_hold_t reset_hold;           /* START: reset this tab */
  ui_hold_t action_hold;          /* confirm on Reset all */
  u32 undo[TABS][MAX_ROWS];       /* the values before a reset */
  int undo_all;                   /* the undo covers every tab */
  char toast[48];
  int toast_f, toast_undo;
  u16 toast_edge;
  int restart;                    /* leave Settings and restart kai */
  /* Controls */
  int page;                       /* 0 Buttons, 1 Stick */
  int btn;                        /* the selected chip on the Buttons page */
  int dir;                        /* the selected stick chip, or -1 on the rows */
  int pick_map;                   /* the map entry the picker edits */
  int pick, pick_f, pick_out;     /* the action picker: open, its motion */
  int help, help_f, help_out;     /* the □ help card: open, its motion */
  int pcur, pcur_from, pcur_f;    /* the picker's cursor */
  u32 undo_map[MAX_GAMEPAD_CONFIG_MAP];
  u32 undo_map_default;
} s;

/* The picture of the game under the pause screen, for the Controls tab's
 * PSP (240 x 160); NULL draws an empty screen. */
const u16 *ui_settings_game;

/* The language kai started with; the Language row may differ until a
 * restart. */
u32 ui_start_language;

static int lang_pending(void)
{
  return g_gpsp_config.language != ui_start_language;
}

static int is_lang_row(int tab, int i)
{
  return (tab == LANG_TAB) && (rows[tab][i].value == &g_gpsp_config.language);
}


static int action_of(int tab, int i)
{
  return (rows[tab][i].kind == K_ACTION) ? rows[tab][i].def : -1;
}

/* The Stick page keeps its rows on the right, beside the stick. */
static int row_x(int tab)  { return (tab == PAD_TAB) ? 236 : ROW_X; }
static int row_w(int tab)  { return (tab == PAD_TAB) ? 224 : ROW_W; }
static int rows_y0(int tab) { return (tab == PAD_TAB) ? 70 : ROWS_Y; }

static int row_y(int tab, int i)
{
  int y = rows_y0(tab), j;

  for(j = 0; j < i; j++)
    y += row_pitch(tab, &rows[tab][j]);
  return y;
}

static int rows_bottom(int tab)
{
  return row_y(tab, row_count(tab));
}

/* Where the list is now: sliding to s.top with the scroll token. */
static int top_now(void)
{
  return ui_ease_out_back(s.top_from, s.top, s.top_f, UI_F_SCROLL);
}

/* Scroll by whole rows, so the first row in view always sits at the top of
 * the list and no row is ever cut there; the cursor stays a row from the
 * bottom edge (spec: Settings; the r4 mockups' settings_top). */
static void scroll_to_cursor(void)
{
  int n = row_count(s.tab), bottom = rows_bottom(s.tab), top = 0, i;

  if(bottom > VIEW_B)
  {
    int y0 = rows_y0(s.tab), cy = row_y(s.tab, s.cur);
    int want = cy + row_height(&rows[s.tab][s.cur]) + ((s.tab == PAD_TAB) ? UI_ROW_PITCH_COMPACT : UI_ROW_PITCH) - VIEW_B;

    if(want > bottom - VIEW_B)
      want = bottom - VIEW_B;
    if(want > 0)
    {
      /* the first row start at or past it, else the last one */
      top = row_y(s.tab, n - 1) - y0;
      for(i = 0; i < n; i++)
        if(row_y(s.tab, i) - y0 >= want)
        {
          top = row_y(s.tab, i) - y0;
          break;
        }
    }
  }
  if(top != s.top)
  {
    s.top_from = top_now();
    s.top = top;
    s.top_f = 0;
    s.dirty = 1;
  }
}

/* ---- drawing ---------------------------------------------------------------- */

/* Screen size as three little PSP screens with the game's area in sky, so
 * the choice shows what it does. */
static void draw_screen_choices(const row_t *r, int x, int y, int selected, u16 group)
{
  static const u8 inner_w[3] = { 15, 26, 30 }, inner_h[3] = { 10, 17, 17 };
  int k = row_index(r), j, xr = x;

  for(j = 2; j >= 0; j--)
  {
    int tw = ui_text_width((char *)r->opts[j]), bx, by = y + 11;
    int on = (j == k);

    xr -= tw + 8;
    if(on)
      ui_round_fill(xr - 4, y + 10, tw + 8, 15, 2, selected ? UI_COL_PLUM : UI_COL_LIME);
    ui_text(xr, y + 11, (char *)r->opts[j], on ? (selected ? UI_COL_CREAM : UI_COL_PLUM) :
     (selected ? UI_COL_PLUM : UI_COL_MUTE));
    xr -= 6 + 34;
    bx = xr;
    ui_round_fill(bx, by - 3, 34, 21, 2, on ? (selected ? UI_COL_PLUM : group) : UI_COL_PLUM_2);
    ui_fill(bx + 2, by - 1, 30, 17, UI_COL_PLUM);
    ui_fill(bx + 2 + (30 - inner_w[j]) / 2, by - 1 + (17 - inner_h[j]) / 2, inner_w[j], inner_h[j],
     UI_COL_SKY);
    xr -= 12;
  }
}

static void draw_row(int tab, int i, int y, int selected, int f)
{
  row_t *r = &rows[tab][i];
  u16 g = UI_COL_LIME;
  int vx, vy, flags = 0, k;
  char text[32];
  int rx = row_x(tab), rw = row_w(tab);

  if(r->kind == K_OFF)
  {
    ui_card_row(rx, y, rw, r->label, 0, g, UI_ROW_DISABLED, &vx, &vy);
    ui_row_note(vx, vy, r->opts[0]);
    return;
  }
  if(row_changed(r))
    flags |= UI_ROW_CHANGED;
  if(r->per_game)
    flags |= UI_ROW_PER_GAME;

  if(r->kind == K_SCREEN)
  {
    int lift = selected ? UI_LIFT : 0;
    if(selected)
      ui_round_fill(rx + 1, y + 1, rw, SCREEN_ROW_H, 2, UI_COL_SHADE);
    ui_round_fill(rx - lift, y - lift, rw, SCREEN_ROW_H, 2, selected ? g : UI_COL_CARD);
    if(flags & UI_ROW_CHANGED)
      ui_fill(rx - 9, y + 18, 4, 4, UI_COL_TANGERINE);
    ui_print(rx + 8 - lift, y + 12 - lift, r->label, selected ? UI_COL_PLUM : UI_COL_CREAM,
     UI_FONT_BODY, 0);
    draw_screen_choices(r, rx + rw - 8 - lift, y - lift, selected, g);
    return;
  }

  ui_card_row(rx, y, rw, r->label, selected, g, flags, &vx, &vy);
  if(is_lang_row(tab, i) && lang_pending())
  {
    /* "next start": a sky pill 10 px after the label, 3 px down, 7 px
       either side of its text, which is centred in it */
    int lift = selected ? UI_LIFT : 0;
    int px = rx + 8 - lift + ui_print_width(r->label, UI_FONT_BODY, 0) + 10;
    int pw = ui_print_width("next start", UI_FONT_SMALL, 0) + 14;
    ui_round_fill(px, y + 3 - lift, pw, 14, 2, UI_COL_SKY);
    ui_print(px + 7, y + 3 - lift, "next start", UI_COL_PLUM, UI_FONT_SMALL, 0);
  }
  k = (r->value) ? row_index(r) : 0;
  switch(r->kind)
  {
    case K_SEG:
      ui_row_segments(vx, vy, r->opts, r->count, k, selected ? s.value_from : k, f, selected, g);
      break;
    case K_SWITCH:
      ui_row_switch(vx, vy, ui_switch_pos(*r->value != 0, selected ? f : UI_F_VALUE), selected);
      break;
    case K_BLOCKS:
      if(r->fmt)
        snprintf(text, sizeof(text), r->fmt, k + 1);
      else
        snprintf(text, sizeof(text), "%s", msg[MSG_SB_2048 + k]);
      ui_row_blocks(vx, vy, r->count, k, text, selected ? f : UI_F_VALUE, selected, g);
      break;
    case K_CYCLE:
      if(r->names)
        snprintf(text, sizeof(text), "%s", r->names[(k >= 0) && (k < r->count) ? k : 0]);
      else
        snprintf(text, sizeof(text), r->fmt, (int)*r->value);
      ui_row_cycle(vx, vy, text, selected, g, selected ? f : UI_F_VALUE);
      break;
    case K_ACTION:
      ui_row_action(vx, vy, selected);  /* its hold bar is in the tip line */
      break;
  }
}

static void show_toast(const char *text, int undo, u16 edge);

/* ---- Controls: the Buttons page ------------------------------------------ */

/* The PSP drawn with each button's job (spec Settings › Controls). Chips
 * sit in three groups: left (L and the d-pad), right (R and the face
 * buttons) and below (SELECT, START). The map is the game's own: editing
 * it makes the game keep its own map (use_default_gamepad_map off). */
extern const u32 gamepad_config_map_init[MAX_GAMEPAD_CONFIG_MAP];

#define PAD_BX 128
#define PAD_BY 56
#define PAD_BW 224
#define PAD_BH 120
#define CHIP_W 112
#define CHIP_H 18
#define PAD_ACTIONS 22  /* BUTTON_ID_UP .. BUTTON_ID_NONE */

typedef struct
{
  u8 map;     /* index in gamepad_config_map */
  u32 key;    /* its glyph */
  s16 x, y;   /* the chip */
  s16 ax, ay; /* where its line meets the button */
} chip_t;

/* 0-4 left, 5-9 right, 10-11 below */
static const chip_t chips[12] =
{
  {  4, PSP_CTRL_LTRIGGER,   8,  46, PAD_BX + 26,          PAD_BY - 3 },
  {  8, PSP_CTRL_UP,         8,  72, PAD_BX + 26,          PAD_BY + 35 },
  {  7, PSP_CTRL_LEFT,       8,  98, PAD_BX + 15,          PAD_BY + 46 },
  {  9, PSP_CTRL_RIGHT,      8, 124, PAD_BX + 37,          PAD_BY + 46 },
  {  6, PSP_CTRL_DOWN,       8, 150, PAD_BX + 26,          PAD_BY + 57 },
  {  5, PSP_CTRL_RTRIGGER, 360,  46, PAD_BX + PAD_BW - 26, PAD_BY - 3 },
  {  0, PSP_CTRL_TRIANGLE, 360,  72, PAD_BX + PAD_BW - 26, PAD_BY + 33 },
  {  2, PSP_CTRL_CIRCLE,   360,  98, PAD_BX + PAD_BW - 13, PAD_BY + 46 },
  {  1, PSP_CTRL_CROSS,    360, 124, PAD_BX + PAD_BW - 26, PAD_BY + 59 },
  {  3, PSP_CTRL_SQUARE,   360, 150, PAD_BX + PAD_BW - 39, PAD_BY + 46 },
  { 10, PSP_CTRL_SELECT,   128, 188, PAD_BX + 87,          PAD_BY + 99 },
  { 11, PSP_CTRL_START,    248, 188, PAD_BX + 137,         PAD_BY + 99 },
};

static const char *action_name(u32 id)
{
  return msg[MSG_PAD_MENU_CFG_0 + ((id < PAD_ACTIONS) ? id : PAD_ACTIONS - 1)];
}

static int chip_remapped(int i)
{
  int m = chips[i].map;
  return gamepad_config_map[m] != gamepad_config_map_init[m];
}

/* The button itself on the drawn PSP, lime when its chip is selected. */
static void draw_pad_button(int i, u16 color)
{
  int dx = PAD_BX + 26, dy = PAD_BY + 46, fx = PAD_BX + PAD_BW - 26, fy = PAD_BY + 46;

  switch(chips[i].map)
  {
    case 4:  ui_round_fill(PAD_BX + 6, PAD_BY - 8, 40, 10, 3, color); break;
    case 5:  ui_round_fill(PAD_BX + PAD_BW - 46, PAD_BY - 8, 40, 10, 3, color); break;
    case 8:  ui_fill(dx - 5, dy - 16, 10, 10, color); break;
    case 6:  ui_fill(dx - 5, dy + 6, 10, 10, color); break;
    case 7:  ui_fill(dx - 16, dy - 5, 10, 10, color); break;
    case 9:  ui_fill(dx + 6, dy - 5, 10, 10, color); break;
    case 0:  ui_disc(fx, fy - 13, 5, color); break;
    case 1:  ui_disc(fx, fy + 13, 5, color); break;
    case 3:  ui_disc(fx - 13, fy, 5, color); break;
    case 2:  ui_disc(fx + 13, fy, 5, color); break;
    case 10: ui_round_fill(PAD_BX + 74, PAD_BY + 92, 26, 7, 2, color); break;
    case 11: ui_round_fill(PAD_BX + 124, PAD_BY + 92, 26, 7, 2, color); break;
  }
}

static void draw_chip(int i, int selected)
{
  const chip_t *c = &chips[i];
  int lift = selected ? 2 : 0, x = c->x - lift, y = c->y - lift, gx;
  int ex = (i < 5) ? c->x + CHIP_W : c->x, ey = c->y + 9;
  u16 line = selected ? UI_COL_LIME : UI_COL_MUTE;

  if(i >= 10)
  {
    ex = c->x + CHIP_W / 2;
    ey = c->y;
  }
  ui_line(ex, ey, c->ax, c->ay, line);
  if(selected)
    ui_round_fill(c->x + 1, c->y + 1, CHIP_W, CHIP_H, 2, UI_COL_SHADE);
  ui_round_fill(x, y, CHIP_W, CHIP_H, 2, selected ? UI_COL_LIME : UI_COL_CARD);
  gx = ui_glyph(x + 4, y + 3, c->key) + 4;
  ui_print_fit(gx, y + 2, action_name(gamepad_config_map[c->map]),
   selected ? UI_COL_PLUM : UI_COL_CREAM, UI_FONT_SMALL, 0, x + 100 - gx);  /* clear of the dot */
  if(chip_remapped(i))  /* remapped: a dot inside the chip's right end */
    ui_fill(x + 102, y + 7, 4, 4, selected ? UI_COL_PLUM : UI_COL_TANGERINE);
  draw_pad_button(i, selected ? UI_COL_LIME :
   (((c->map >= 6) && (c->map <= 9)) || (c->map <= 3) ? UI_COL_CREAM : UI_COL_MUTE));
}

#define PAD_SHOT_W 96
#define PAD_SHOT_H 64
static u16 pad_shot[PAD_SHOT_W * PAD_SHOT_H];
static int pad_shot_ok;  /* cleared each time Settings opens: a new picture */

static void draw_pad_body(void)
{
  int i;

  ui_round_fill(PAD_BX + 3, PAD_BY + 3, PAD_BW, PAD_BH, 4, UI_COL_SHADE);
  ui_round_fill(PAD_BX, PAD_BY, PAD_BW, PAD_BH, 4, UI_COL_CARD);
  ui_fill(PAD_BX + 52, PAD_BY + 14, 120, 68, UI_COL_SHADE);
  if(ui_settings_game)
  {
    /* the game's 3:2 picture fills the drawn screen's height, centred,
       averaged down once (the ground is rebuilt on every move) */
    if(!pad_shot_ok)
    {
      ui_shrink_box(ui_settings_game, 240, 160, pad_shot, PAD_SHOT_W, PAD_SHOT_H);
      pad_shot_ok = 1;
    }
    ui_blit(pad_shot, PAD_SHOT_W, PAD_BX + 54 + (116 - PAD_SHOT_W) / 2, PAD_BY + 16, PAD_SHOT_W, PAD_SHOT_H);
  }
  for(i = 0; i < 12; i++)
    if(i != s.btn)
      draw_chip(i, 0);
}

/* Spatial moves between the chips; never wraps. */
static int chip_step(int i, u32 dir)
{
  switch(dir)
  {
    case PSP_CTRL_UP:
      if(i == 10) return 4;
      if(i == 11) return 9;
      return ((i % 5) > 0) ? i - 1 : i;
    case PSP_CTRL_DOWN:
      if(i == 4) return 10;
      if(i == 9) return 11;
      return ((i < 10) && ((i % 5) < 4)) ? i + 1 : i;
    case PSP_CTRL_LEFT:
      if(i == 11) return 10;
      return ((i >= 5) && (i < 10)) ? i - 5 : i;
    case PSP_CTRL_RIGHT:
      if(i == 10) return 11;
      return (i < 5) ? i + 5 : i;
  }
  return i;
}

#define PAD_TIP "Confirm picks what this button does in this game."
#define STICK_DIR_TIP "Confirm picks what this direction does in this game."

/* ---- Controls: the action picker ------------------------------------------ */

#define PICK_W 400
#define PICK_H 176
#define PICK_X ((UI_SCREEN_W - PICK_W) / 2)
#define PICK_Y 46
#define PICK_CW 92
#define PICK_CH 20

static int menu_reachable_without(int m, u32 id)
{
  int i;

  if(g_use_home || (id == BUTTON_ID_MENU))
    return 1;
  for(i = 0; i < 12; i++)
    if((chips[i].map != m) && (gamepad_config_map[chips[i].map] == BUTTON_ID_MENU))
      return 1;
  return 0;
}

static void open_picker(int map)
{
  u32 id = gamepad_config_map[map];

  s.pick_map = map;
  s.pick = 1;
  s.pick_f = 0;
  s.pick_out = 0;
  s.pcur = (id < PAD_ACTIONS) ? id : PAD_ACTIONS - 1;
  s.pcur_from = s.pcur;
  s.pcur_f = UI_F_CURSOR;
}

static void picker_input(void)
{
  int to = s.pcur;

  if(s.pick_out)
    return;
  if(ui_pressed(ui_key_back))
  {
    s.pick_out = 1;
    s.pick_f = 0;
    return;
  }
  if(ui_pressed(ui_key_confirm))
  {
    int m = s.pick_map;
    if(!menu_reachable_without(m, s.pcur))
    {
      show_toast("One button has to open the menu", 0, UI_COL_PINK);
      return;
    }
    if(gamepad_config_map[m] != (u32)s.pcur)
    {
      gamepad_config_map[m] = s.pcur;
      g_game_config.use_default_gamepad_map = NO;  /* the game keeps its own map */
      s.dirty = 1;
    }
    s.pick_out = 1;
    s.pick_f = 0;
    return;
  }
  if(ui_repeat(PSP_CTRL_LEFT) && (to % 4 > 0)) to--;
  if(ui_repeat(PSP_CTRL_RIGHT) && (to % 4 < 3) && (to + 1 < PAD_ACTIONS)) to++;
  if(ui_repeat(PSP_CTRL_UP) && (to >= 4)) to -= 4;
  if(ui_repeat(PSP_CTRL_DOWN) && (to + 4 < PAD_ACTIONS)) to += 4;
  if(to != s.pcur)
  {
    s.pcur_from = ui_ease_out_back(s.pcur_from, s.pcur, s.pcur_f, UI_F_CURSOR);
    s.pcur = to;
    s.pcur_f = 0;
  }
}

static void picker_cell(int i, int *x, int *y, int top)
{
  *x = PICK_X + 16 + (i % 4) * (PICK_CW + 2);
  *y = top + 28 + (i / 4) * (PICK_CH + 4);
}

/* Returns 0 once it has closed. */
static int draw_picker(void)
{
  int top, i, fx, fy, ax, ay, bx, by, n;
  ui_hint_t hints[2];

  if(!s.pick_out)
  {
    if(s.pick_f < UI_F_POPUP_IN) s.pick_f++;
    top = ui_ease_out_back(PICK_Y - 24, PICK_Y, s.pick_f, UI_F_POPUP_IN);
  }
  else
  {
    if(++s.pick_f > UI_F_POPUP_OUT)
    {
      s.pick = 0;
      return 0;
    }
    top = ui_ease_in_cubic(PICK_Y, PICK_Y + 24, s.pick_f, UI_F_POPUP_OUT);
  }
  if(s.pcur_f < UI_F_CURSOR) s.pcur_f++;

  ui_checker(0, 0, UI_SCREEN_W, UI_SCREEN_H, UI_COL_SHADE);
  ui_round_fill(PICK_X + 4, top + 4, PICK_W, PICK_H, 3, UI_COL_SHADE);
  ui_round_fill(PICK_X, top, PICK_W, PICK_H, 3, UI_COL_CREAM);
  ui_fill(PICK_X, top + 4, 6, PICK_H - 8, UI_COL_LIME);
  n = ui_print(PICK_X + 16, top + 6, "Button action \xC2\xB7 ", UI_COL_PLUM, UI_FONT_BODY, 0);
  ui_glyph(n, top + 8, ui_key_confirm);
  for(i = 0; i < PAD_ACTIONS; i++)
  {
    int x, y;
    picker_cell(i, &x, &y, top);
    if(i == s.pcur)
    {
      ui_round_fill(x + 1, y + 1, PICK_CW - 2, PICK_CH, 2, UI_COL_SHADE);
      ui_round_fill(x - 1, y - 1, PICK_CW - 2, PICK_CH, 2, UI_COL_LIME);
      ui_print_fit(x + 5, y + 2, action_name(i), UI_COL_PLUM, UI_FONT_SMALL, 0, PICK_CW - 10);
    }
    else
    {
      ui_round_frame(x, y, PICK_CW - 2, PICK_CH, 2, UI_COL_MUTE);
      ui_print_fit(x + 6, y + 3, action_name(i), UI_COL_PLUM, UI_FONT_SMALL, 0, PICK_CW - 10);
    }
  }
  /* the cursor frame glides between cells */
  picker_cell(s.pcur_from, &ax, &ay, top);
  picker_cell(s.pcur, &bx, &by, top);
  fx = ui_ease_out_back(ax, bx, s.pcur_f, UI_F_CURSOR);
  fy = ui_ease_out_back(ay, by, s.pcur_f, UI_F_CURSOR);
  ui_round_frame(fx - 4, fy - 4, PICK_CW + 4, PICK_CH + 6, 2, UI_COL_PLUM);

  hints[0].keys = ui_key_back;    hints[0].verb = "Back";
  hints[1].keys = ui_key_confirm; hints[1].verb = "Pick";
  ui_hint_packed(hints, 2, 14);
  return 1;
}

/* ---- Controls: the Stick page ---------------------------------------------- */

#define STICK_CX 122
#define STICK_CY 140
#define STICK_R  46

static void draw_page_switch(void)
{
  static const char *names[2] = { "Buttons", "Stick" };
  int x = UI_MARGIN, i;

  for(i = 0; i < 2; i++)
  {
    int w = ui_print_width(names[i], UI_FONT_SMALL, 0) + 16;
    if(i == s.page)
      ui_round_fill(x, 38, w, 16, 2, UI_COL_LIME);
    ui_print(x + 8, 39, names[i], (i == s.page) ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_SMALL, 0);
    x += w;
  }
  ui_glyph(x + 6, 40, PSP_CTRL_SELECT);
}

/* The direction chips, in map order 12-15: up, down, left, right. */
static const u32 stick_keys[4] = { PSP_CTRL_UP, PSP_CTRL_DOWN, PSP_CTRL_LEFT, PSP_CTRL_RIGHT };

/* They can be picked while the stick is in use (the old menu remapped them
 * too). */
static int stick_pickable(void)
{
  return g_gpsp_config.enable_analog != NO;
}

static void draw_stick(void)
{
  static const s8 dirs[4][2] = { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } };
  int sens = g_gpsp_config.analog_sensitivity_level + 1;
  int dz = STICK_R * (75 - sens * 6) / 100, use = stick_pickable(), i;
  u16 nub = COLOR16(25, 22, 23);  /* cream and mute, half each */

  draw_page_switch();
  /* the travel, the ring where a push starts to count (smaller = more
     sensitive; mute while the stick is off) and the nub */
  ui_disc(STICK_CX, STICK_CY, STICK_R + 3, UI_COL_SHADE);
  ui_disc(STICK_CX, STICK_CY, STICK_R, UI_COL_CARD);
  ui_ring(STICK_CX, STICK_CY, dz, 2, use ? UI_COL_LIME : UI_COL_MUTE);
  ui_disc(STICK_CX, STICK_CY, 12, UI_COL_CREAM);
  ui_disc(STICK_CX, STICK_CY, 8, nub);
  /* what each push does (map entries 12-15): card chips 6 px off the disc,
     the glyph at +4 and the name 4 px after it. The ← chip stays 20 px in
     and the → chip ends by x 226, 10 px clear of the rows; a long name
     slides over the disc's edge but never over the ring (2 px clear), cut
     to fit. The selected chip is lime and lifted, as on the Buttons page. */
  for(i = 0; i < 4; i++)
  {
    const char *name = action_name(gamepad_config_map[12 + i]);
    int gw = ui_glyph_width(stick_keys[i]), sel = (i == s.dir), px, py, gx, w, room, tw;
    room = (dirs[i][0] == 0) ? 100 : ((dirs[i][0] < 0) ? STICK_CX - dz - 2 - 20 : 226 - (STICK_CX + dz + 2));
    if(room > 100)
      room = 100;
    tw = ui_print_width(name, UI_FONT_SMALL, 0);
    if(tw > room - (gw + 13))
      tw = room - (gw + 13);
    w = 4 + gw + 4 + tw + 5;
    if(dirs[i][0] < 0)
      px = (STICK_CX - (STICK_R + 6) - w > 20) ? STICK_CX - (STICK_R + 6) - w : 20;
    else if(dirs[i][0] > 0)
      px = (STICK_CX + STICK_R + 6 < 226 - w) ? STICK_CX + STICK_R + 6 : 226 - w;
    else
      px = STICK_CX - w / 2;
    py = STICK_CY + dirs[i][1] * (STICK_R + 6) - ((dirs[i][1] == 0) ? 9 : ((dirs[i][1] < 0) ? 18 : 0));
    if(sel)
    {
      ui_round_fill(px + 1, py + 1, w, 18, 2, UI_COL_SHADE);
      px -= UI_LIFT;
      py -= UI_LIFT;
    }
    ui_round_fill(px, py, w, 18, 2, sel ? UI_COL_LIME : UI_COL_CARD);
    gx = ui_glyph(px + 4, py + 3, stick_keys[i]) + 4;
    ui_print_fit(gx, py + 2, name, sel ? UI_COL_PLUM : (use ? UI_COL_CREAM : UI_COL_MUTE),
     UI_FONT_SMALL, 0, tw);
  }
}

/* ---- Controls: reset and undo ------------------------------------------------ */

static void pad_remember(void)
{
  memcpy(s.undo_map, gamepad_config_map, sizeof(s.undo_map));
  s.undo_map_default = g_game_config.use_default_gamepad_map;
}

static void pad_reset(void)
{
  memcpy(gamepad_config_map, gamepad_config_map_init, sizeof(s.undo_map));
  g_game_config.use_default_gamepad_map = NO;
}

static void pad_undo(void)
{
  memcpy(gamepad_config_map, s.undo_map, sizeof(s.undo_map));
  g_game_config.use_default_gamepad_map = s.undo_map_default;
}

/* Steady hints first, then the row's own (spec: Hint strip). */
/* ---- help (the square key) -------------------------------------------------------------------------
 * The selected row's help card. Written help: Smoothing and Frameskip
 * (spec, help_smooth / help_skip); every other row shows its tip as the
 * text until its own help copy is written (PROTOTYPE: help copy uses the
 * tip). Smoothing's three pictures are not drawn yet: their captions stand
 * in as text. Returns 0 when the selection has no help. */
static int help_topic(const char **title, const char **body)
{
  static char text[400];
  const row_t *r;
  const char *t, *b;

  if((s.tab == PAD_TAB) && (s.page == 0))
  {
    t = "Buttons";
    b = PAD_TIP;
  }
  else if((s.tab == PAD_TAB) && (s.dir >= 0))
  {
    t = "Stick";
    b = STICK_DIR_TIP;
  }
  else
  {
    r = &rows[s.tab][s.cur];
    if(!r->label || (r->kind == K_OFF))
      return 0;
    t = r->label;
    b = r->tip;
    if(!strcmp(t, "Smoothing"))
    {
      snprintf(text, sizeof(text), "How the GBA picture is drawn when it is scaled up to the "
       "PSP screen.\nOff: pixels stay hard but uneven.\nSmooth: soft, a little blurry.\n"
       "Sharp: crisp and even. Best for most games.");
      b = text;
    }
    else if(!strcmp(t, "Frameskip"))
      b = "When the PSP can't draw every frame, skipping some keeps the game at full speed. "
       "Auto skips only when the game would slow down; Fixed always skips the number set in "
       "Skip up to.";
    if(!b)
      return 0;
  }
  if(title)
    *title = t;
  if(body)
    *body = b;
  return 1;
}

static void draw_hints(void)
{
  ui_hint_t hints[6];
  int n = 0;

  hints[n].keys = PSP_CTRL_START; hints[n++].verb = "Hold: reset tab";
  if(help_topic(NULL, NULL))
  {
    hints[n].keys = PSP_CTRL_SQUARE; hints[n++].verb = "Help";
  }
  hints[n].keys = ui_key_back;    hints[n++].verb = "Back";
  if(s.tab == PAD_TAB)
  {
    hints[n].keys = PSP_CTRL_SELECT; hints[n++].verb = s.page ? "Buttons" : "Stick";
    if((s.page == 0) || (s.dir >= 0))
    {
      hints[n].keys = ui_key_confirm; hints[n++].verb = "Pick";
      ui_hint_packed(hints, n, 14);
      return;
    }
  }
  switch(action_of(s.tab, s.cur))
  {
    case ACT_RESET_ALL:
      hints[n].keys = ui_key_confirm; hints[n++].verb = "Hold: reset all";
      break;
    case ACT_RESTART:
      hints[n].keys = ui_key_confirm;
      hints[n++].verb = "Hold: restart";
      break;
    default:
      if(rows[s.tab][s.cur].kind == K_SWITCH)
      {
        hints[n].keys = ui_key_confirm; hints[n++].verb = "Switch";
        break;
      }
      hints[n].keys = PSP_CTRL_LEFT | PSP_CTRL_RIGHT; hints[n++].verb = "Change";
      if(is_lang_row(s.tab, s.cur) && lang_pending())
      {
        hints[n].keys = ui_key_confirm; hints[n++].verb = "Hold: restart now";
      }
      break;
  }
  ui_hint_packed(hints, n, 14);
}

/* The ground: the check, the legend, every row but the selected one, the
 * scroll bar and the hint strip. */
/* The bottom of the list as drawn with the list at top: under the last
 * row that fits whole above the tip line, its shadow included. */
static int list_bottom(int top)
{
  int i, n = row_count(s.tab), vb = VIEW_T;

  for(i = 0; i < n; i++)
  {
    int y = row_y(s.tab, i) - top, h = row_height(&rows[s.tab][i]);
    if((y >= VIEW_T - 1) && (y + h + 1 <= VIEW_B) && (y + h + 1 > vb))
      vb = y + h + 1;
  }
  return vb;
}

static void build_ground(void)
{
  int i, n = row_count(s.tab), per_game = 0, bottom = rows_bottom(s.tab), top;

  s.dirty = 0;
  if(!ui_layer_begin())
    return;
  if((s.tab == PAD_TAB) && (s.page == 0))
  {
    draw_pad_body();
    draw_hints();
    ui_layer_end();
    return;
  }
  if(s.tab == PAD_TAB)
    draw_stick();
  for(i = 0; i < n; i++)
    per_game |= rows[s.tab][i].per_game;
  if(per_game && (s.tab != PAD_TAB))
  {
    ui_per_game_mark(UI_MARGIN, 40, UI_COL_TANGERINE);
    ui_text(UI_MARGIN + 16, 39, "Saved for this game only", UI_COL_MUTE);
  }
  /* every row as it looks unselected; the selected look is drawn over it
     each frame inside the cursor frame. While the list slides, rows are cut
     at the top of the list and at the bottom of the last whole row. */
  top = top_now();
  ui_clip_set(0, VIEW_T, UI_SCREEN_W, list_bottom(top) - VIEW_T);
  for(i = 0; i < n; i++)
  {
    int y = row_y(s.tab, i) - top, h = row_height(&rows[s.tab][i]);
    if((y + h < VIEW_T) || (y > VIEW_B))
      continue;
    draw_row(s.tab, i, y, 0, UI_F_VALUE);
  }
  ui_clip_reset();
  if(bottom > VIEW_B)
  {
    int track = VIEW_B - VIEW_T - 4, th = track * (VIEW_B - VIEW_T) / (bottom - VIEW_T);
    int ty = VIEW_T + 2 + (track - th) * ((top < bottom - VIEW_B) ? top : bottom - VIEW_B) / (bottom - VIEW_B);
    ui_round_fill(468, VIEW_T + 2, 4, track, 1, UI_COL_CARD);
    ui_round_fill(468, ty, 4, th, 1, UI_COL_MUTE);
  }
  draw_hints();
  ui_layer_end();
}

/* ---- the selection and the tip line ---------------------------------------- */

/* The cursor frame flies between rows with the cursor token; inside it the
 * rows show their selected look (the lime fill moves with the frame), and
 * outside it they stay as the ground drew them. */
static void draw_selection(void)
{
  int top = top_now(), n = row_count(s.tab), i;
  int rx = row_x(s.tab), rw = row_w(s.tab);
  int hh = ui_ease_out_back(s.cursor_h_from, row_height(&rows[s.tab][s.cur]), s.cursor_f, UI_F_CURSOR);
  int cy = ui_ease_out_back(s.cursor_from, row_y(s.tab, s.cur), s.cursor_f, UI_F_CURSOR) - top;
  int bx = rx - 4, by = cy - 2, bw = rw + 6, bh = hh + 3, lb = list_bottom(top);

  /* the box, cut to the list area */
  if(by < VIEW_T) { bh -= VIEW_T - by; by = VIEW_T; }
  if(by + bh > lb) bh = lb - by;
  if(bh > 0)
  {
    ui_clip_set(bx, by, bw, bh);
    ui_ground_rect(bx, by, bw, bh);
    for(i = 0; i < n; i++)
    {
      int y = row_y(s.tab, i) - top, h = row_height(&rows[s.tab][i]);
      if((y + h + 1 < by) || (y - 2 > by + bh))
        continue;
      if(i != s.cur)
        draw_row(s.tab, i, y, row_selectable(&rows[s.tab][i]), UI_F_VALUE);
    }
    /* the selected row last, so its value motion is on top */
    draw_row(s.tab, s.cur, row_y(s.tab, s.cur) - top, 1, s.value_f);
    ui_clip_reset();
  }
  ui_cursor_frame(rx - UI_LIFT, cy - UI_LIFT, rw, hh);
}

/* A hold's bar in the tip line's place: 440 x 6 at (20, 235) on a card
 * track, from 12 f like every hold. Returns 1 while it shows. */
static int tip_hold_bar(const ui_hold_t *h, u16 color)
{
  if(!h->shown || (h->p <= 0))
    return 0;
  ui_fill(UI_MARGIN, 235, ROW_W, 6, UI_COL_CARD);
  ui_fill(UI_MARGIN, 235, ROW_W * ((h->p < 256) ? h->p : 256) / 256, 6, color);
  return 1;
}

/* The tip line: "▸ " and one line at (20, 231), cut at 440 px. It waits
 * for the cursor to rest, then fades in rising 6 px. A hold's bar takes its
 * place: START's tangerine, Restart kai's and Language's sky, Reset all's
 * pink (it loses data). */
static void draw_tip_line(void)
{
  const char *tip = NULL;
  char text[160];
  int k;

  if(tip_hold_bar(&s.reset_hold, UI_COL_TANGERINE))
    return;
  if(tip_hold_bar(&s.action_hold, (action_of(s.tab, s.cur) == ACT_RESET_ALL) ? UI_COL_PINK : UI_COL_SKY))
    return;
  if(s.reset_hold.fired || s.action_hold.fired)
    return;  /* done, key still down: the tip stays away */

  if(s.rest < UI_F_TIP_DELAY + 8)
    s.rest++;
  if(s.rest <= UI_F_TIP_DELAY)
    return;
  k = s.rest - UI_F_TIP_DELAY;  /* 1..8 */
  if(k > 8)
    k = 8;

  if(s.tab == PAD_TAB)
    tip = (s.page == 0) ? PAD_TIP : ((s.dir >= 0) ? STICK_DIR_TIP : rows[s.tab][s.cur].tip);
  else if(is_lang_row(s.tab, s.cur) && lang_pending())
  {
    /* the glyph is whichever key confirms */
    snprintf(text, sizeof(text), "Applies after a restart: hold %s to restart now, or it waits for the next start.",
     (ui_key_confirm == PSP_CTRL_CIRCLE) ? "\xE2\x97\x8B" : "\xC3\x97");
    tip = text;
  }
  else
    tip = rows[s.tab][s.cur].tip;
  if(!tip)
    return;
  if(tip != text)
  {
    snprintf(text, sizeof(text), "%s", tip);
    tip = text;
  }
  {
    char line[168];
    int c, col = UI_COL_MUTE & 0x8000;
    snprintf(line, sizeof(line), "\xE2\x96\xB8 %s", text);
    /* plum to mute as it rises */
    for(c = 0; c < 15; c += 5)
    {
      int a = (UI_COL_PLUM >> c) & 31, b = (UI_COL_MUTE >> c) & 31;
      col |= ((a + (b - a) * k / 8) & 31) << c;
    }
    ui_print_fit(UI_MARGIN, 231 + 6 - 6 * UI_EASE_OUT_CUBIC_8[k - 1] / 256, line, (u16)col,
     UI_FONT_SMALL, 0, ROW_W);
  }
}

/* ---- actions ---------------------------------------------------------------- */

static void remember_tab(int tab)
{
  int i;

  if(tab == PAD_TAB)
    pad_remember();
  for(i = 0; i < row_count(tab); i++)
    if(rows[tab][i].value)
      s.undo[tab][i] = *rows[tab][i].value;
}

static void reset_tab(int tab)
{
  int i;

  if(tab == PAD_TAB)
    pad_reset();
  for(i = 0; i < row_count(tab); i++)
    if(rows[tab][i].value)
      *rows[tab][i].value = rows[tab][i].def;
}

/* edge: the group colour of what happened (a tab reset lime; a refused
 * pick pink, with no button as nothing changed) */
static void show_toast(const char *text, int undo, u16 edge)
{
  snprintf(s.toast, sizeof(s.toast), "%s", text);
  s.toast_undo = undo;
  s.toast_edge = edge;
  s.toast_f = 0;
}

static void undo_reset(void)
{
  int t, i;

  for(t = 0; t < TABS; t++)
  {
    if(!s.undo_all && (t != s.tab))
      continue;
    if(t == PAD_TAB)
      pad_undo();
    for(i = 0; i < row_count(t); i++)
      if(rows[t][i].value)
        *rows[t][i].value = s.undo[t][i];
  }
  show_toast("Settings are back as they were", 0, UI_COL_LIME);
  s.dirty = 1;
}

static void move_cursor(int to)
{
  if(to == s.cur)
    return;
  s.cursor_from = ui_ease_out_back(s.cursor_from, row_y(s.tab, s.cur), s.cursor_f, UI_F_CURSOR);
  s.cursor_h_from = ui_ease_out_back(s.cursor_h_from, row_height(&rows[s.tab][s.cur]), s.cursor_f, UI_F_CURSOR);
  s.cur = to;
  s.cursor_f = 0;
  s.rest = 0;
  s.value_f = UI_F_VALUE;
  s.action_hold.p = 0;
  scroll_to_cursor();
  s.dirty = 1;
  if(ui_toast_up(s.toast_f) && s.toast_undo)
    s.toast_f = (s.toast_f < UI_F_TOAST_IN + UI_F_TOAST_HOLD) ? UI_F_TOAST_IN + UI_F_TOAST_HOLD : s.toast_f;
}

/* The next selectable row from cur in direction dir; a fresh press wraps. */
static int next_row(int dir)
{
  int n = row_count(s.tab), i = s.cur, k;

  for(k = 0; k < n; k++)
  {
    i += dir;
    if((i < 0) || (i >= n))
    {
      if(!ui_pressed(PSP_CTRL_UP | PSP_CTRL_DOWN))
        return s.cur;  /* a repeat stops at the end */
      i = (i + n) % n;
    }
    if(row_selectable(&rows[s.tab][i]))
      return i;
  }
  return s.cur;
}

static void first_row(void)
{
  int n = row_count(s.tab), i;

  s.cur = 0;
  for(i = 0; i < n; i++)
    if(row_selectable(&rows[s.tab][i]))
    {
      s.cur = i;
      break;
    }
  s.top = s.top_from = 0;
  s.top_f = UI_F_SCROLL;
  scroll_to_cursor();
  s.top_from = s.top;   /* a new tab or page opens in place */
  s.top_f = UI_F_SCROLL;
  s.cursor_from = row_y(s.tab, s.cur);
  s.cursor_h_from = row_height(&rows[s.tab][s.cur]);
  s.cursor_f = UI_F_CURSOR;
  s.rest = 0;
}

static void change_value(int dir)
{
  row_t *r = &rows[s.tab][s.cur];
  int k = r->value ? row_index(r) : 0;

  switch(r->kind)
  {
    case K_SEG:
    case K_SCREEN:
      if(k < 0)
        k = (dir > 0) ? -1 : r->count;
      k += dir;
      if((k < 0) || (k >= r->count))
        return;  /* segments stop at their ends */
      s.value_from = row_index(r);
      row_set_index(r, k);
      break;
    case K_SWITCH:
      *r->value = !*r->value;
      break;
    case K_BLOCKS:
      if(((dir > 0) && (k >= r->count - 1)) || ((dir < 0) && (k <= 0)))
        return;
      row_set_index(r, k + dir);
      break;
    case K_CYCLE:
      if(r->names)
        *r->value = (*r->value + r->count + dir) % r->count;
      else
      {
        int v = (int)*r->value + dir;
        if(v < r->lo) v = r->hi;
        if(v > r->hi) v = r->lo;
        *r->value = v;
      }
      break;
    default:
      return;
  }
  s.value_f = 0;
}

/* ---- the screen ---------------------------------------------------------------- */

int ui_settings(int tab, int row)
{
  u32 mark = ui_pool_mark();

  memset(&s, 0, sizeof(s));
  s.tab_prev = -1;
  s.tab_f = UI_F_TAB;
  s.value_f = UI_F_VALUE;
  s.toast_f = -1;
  s.dir = -1;
  pad_shot_ok = 0;
  /* the language row's default is the PSP's own language */
  {
    int lang = 1;
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &lang);
    rows[4][0].def = lang;
  }
  if((tab > 0) && (tab < TABS))
    s.tab = tab;  /* back where a restart left it */
  first_row();
  if((row > 0) && (row < row_count(s.tab)) && row_selectable(&rows[s.tab][row]))
  {
    s.cur = row;
    scroll_to_cursor();
    s.top_from = s.top;  /* back where it was, without a slide */
    s.top_f = UI_F_SCROLL;
    s.cursor_from = row_y(s.tab, s.cur);
    s.cursor_h_from = row_height(&rows[s.tab][s.cur]);
  }
  ui_input_init();
  ui_status_read();
  build_ground();

  while(1)
  {
    int moved = 0;

    ui_input_update();
    if(s.pick)
    {
      picker_input();  /* the picker takes every key while it is up */
      goto draw;
    }
    if(s.help)
    {
      /* the help card takes every key; □ or ○ closes it */
      if(!s.help_out && (ui_pressed(PSP_CTRL_SQUARE) || ui_pressed(ui_key_back)))
      {
        s.help_out = 1;
        s.help_f = 0;
      }
      goto draw;
    }
    if(ui_pressed(PSP_CTRL_SQUARE) && help_topic(NULL, NULL))
    {
      s.help = 1;
      s.help_out = 0;
      s.help_f = 0;
      goto draw;
    }
    if(ui_pressed(ui_key_back))
      break;

    /* tabs: L R step and repeat while held, stopping at the first and the
       last tab */
    if(ui_repeat(PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER) &&
     (ui_repeat(PSP_CTRL_RTRIGGER) ? (s.tab < TABS - 1) : (s.tab > 0)))
    {
      s.tab_prev = s.tab;
      s.tab += ui_repeat(PSP_CTRL_RTRIGGER) ? 1 : -1;
      s.tab_f = 0;
      s.reset_hold.p = 0;
      s.dir = -1;
      first_row();
      s.dirty = 1;
      moved = 1;
    }

    if((s.tab == PAD_TAB) && (s.page == 0))
    {
      /* the Buttons page: move between the chips, × opens the picker */
      static const u32 dirs[4] = { PSP_CTRL_UP, PSP_CTRL_DOWN, PSP_CTRL_LEFT, PSP_CTRL_RIGHT };
      int d;
      for(d = 0; d < 4; d++)
        if(!moved && ui_repeat(dirs[d]))
        {
          int to = chip_step(s.btn, dirs[d]);
          if(to != s.btn)
          {
            s.btn = to;
            s.dirty = 1;
            s.rest = 0;
          }
          break;
        }
      if(ui_pressed(ui_key_confirm))
        open_picker(chips[s.btn].map);
    }
    else if((s.tab == PAD_TAB) && (s.dir >= 0))
    {
      /* the stick's direction chips: each arrow takes its own chip; → on
         the right one goes back to the rows beside it */
      int d;
      for(d = 0; d < 4; d++)
        if(!moved && ui_repeat(stick_keys[d]))
        {
          if((d == 3) && (s.dir == 3))
            s.dir = -1;
          else
            s.dir = d;
          s.dirty = 1;
          s.rest = 0;
          break;
        }
      if((s.dir >= 0) && ui_pressed(ui_key_confirm))
        open_picker(12 + s.dir);
    }
    /* rows; ↓ from the last row on the Stick page goes to the stick's chips */
    else if(!moved && ui_repeat(PSP_CTRL_UP | PSP_CTRL_DOWN))
    {
      int dn = ui_repeat(PSP_CTRL_DOWN) != 0, to = next_row(dn ? 1 : -1);
      if((s.tab == PAD_TAB) && dn && (to <= s.cur) && stick_pickable())
      {
        s.dir = 3;  /* the right chip sits nearest the rows */
        s.dirty = 1;
        s.rest = 0;
      }
      else
        move_cursor(to);
    }
    if(!((s.tab == PAD_TAB) && ((s.page == 0) || (s.dir >= 0))) && row_selectable(&rows[s.tab][s.cur]))
    {
      int pending = lang_pending();
      if(ui_repeat(PSP_CTRL_RIGHT)) change_value(1);
      if(ui_repeat(PSP_CTRL_LEFT)) change_value(-1);
      if(lang_pending() != pending)
        s.dirty = 1;  /* the hint strip gains or loses "Restart now" */
      if((s.tab == PAD_TAB) && (ui_repeat(PSP_CTRL_LEFT | PSP_CTRL_RIGHT) || ui_pressed(ui_key_confirm)))
        s.dirty = 1;  /* the drawn stick follows the sensitivity */
      if((rows[s.tab][s.cur].value == &g_gpsp_config.confirm_button) && ui_repeat(PSP_CTRL_LEFT | PSP_CTRL_RIGHT))
      {
        ui_input_init();  /* the new confirm and back keys, at once */
        s.dirty = 1;      /* every hint shows them */
      }
      if((rows[s.tab][s.cur].kind == K_SWITCH) && ui_pressed(ui_key_confirm))
        change_value(1);  /* a switch also flips on confirm, as in Cheats */
    }

    /* SELECT: Undo while the toast offers it; on Controls, Buttons/Stick */
    if(ui_released(PSP_CTRL_SELECT))
    {
      if(ui_toast_up(s.toast_f) && s.toast_undo)
      {
        undo_reset();
        s.toast_undo = 0;
      }
      else if(s.tab == PAD_TAB)
      {
        s.page ^= 1;
        s.dir = -1;
        first_row();
        s.dirty = 1;
      }
    }

    /* hold START: reset this tab */
    ui_hold_step(&s.reset_hold, PSP_CTRL_START);
    if(s.reset_hold.done)
    {
      char text[48];
      remember_tab(s.tab);
      s.undo_all = 0;
      reset_tab(s.tab);
      snprintf(text, sizeof(text), "%s reset", tab_labels[s.tab]);
      show_toast(text, 1, UI_COL_LIME);
      s.reset_hold.p = 0;
      s.reset_hold.shown = 0;
      s.dirty = 1;
    }

    if((s.dir >= 0) && !stick_pickable())
    {
      s.dir = -1;  /* a reset turned the stick off */
      s.dirty = 1;
    }

    /* Restart kai, or a changed Language: hold × so a stray press never
       restarts */
    if((action_of(s.tab, s.cur) == ACT_RESTART) || (is_lang_row(s.tab, s.cur) && lang_pending()))
    {
      ui_hold_step(&s.action_hold, ui_key_confirm);
      if(s.action_hold.done)
      {
        s.restart = 1;
        break;
      }
    }

    /* hold confirm on Reset all */
    if(action_of(s.tab, s.cur) == ACT_RESET_ALL)
    {
      ui_hold_step(&s.action_hold, ui_key_confirm);
      if(s.action_hold.done)
      {
        int t;
        for(t = 0; t < TABS; t++)
          remember_tab(t);
        s.undo_all = 1;
        for(t = 0; t < TABS; t++)
          reset_tab(t);
        show_toast("Every setting reset", 1, UI_COL_PINK);
        s.action_hold.p = 0;
        s.dirty = 1;
      }
    }

  draw:
    if(s.top_f < UI_F_SCROLL)
    {
      s.top_f++;
      s.dirty = 1;  /* the list slides: its ground follows */
    }
    if(s.dirty)
      build_ground();

    /* draw */
    ui_frame_begin();
    ui_ground();
    if(s.tab_f < UI_F_TAB) s.tab_f++;
    ui_tabs(UI_MARGIN, tab_labels, TABS, s.tab, s.tab_prev, s.tab_f);
    ui_status_chip();
    if(s.value_f < UI_F_VALUE) s.value_f++;
    if(s.cursor_f < UI_F_CURSOR) s.cursor_f++;
    if((s.tab == PAD_TAB) && (s.page == 0))
      draw_chip(s.btn, 1);
    else if(row_selectable(&rows[s.tab][s.cur]) && (s.dir < 0))
      draw_selection();
    draw_tip_line();
    if(s.pick && !draw_picker())
      s.dirty = 1;  /* closed: the chip shows its new job */
    if(s.help)
    {
      const char *t, *b;
      if(help_topic(&t, &b))
        ui_help_card(t, b, UI_COL_LIME, s.help_f, s.help_out);
      if(s.help_f < (s.help_out ? UI_F_HELP_OUT : UI_F_HELP_IN))
        s.help_f++;
      else if(s.help_out)
        s.help = 0;
    }
    if(s.toast_f >= 0)
    {
      if(!ui_toast(s.toast, s.toast_edge, s.toast_undo ? "Undo" : NULL, s.toast_f))
        s.toast_f = -1;
      else
        s.toast_f++;
    }
    ui_frame_end();
  }

  /* the screen keeps no memory; the caller rebuilds its own ground */
  ui_pool_release(mark);
  return s.restart;
}

/* Where Settings stands, for the restart note. */
void ui_settings_where(int *tab, int *row)
{
  *tab = s.tab;
  *row = s.cur;
}

/* How many of a game's own settings differ from their defaults, read from
 * a game_config_t that need not be the running game's (the library's game
 * card: "Settings: 5 saved for this game"). */
int ui_settings_saved_for_game(const game_config_t *cfg)
{
  int tab, i, n = 0;

  for(tab = 0; tab < TABS; tab++)
    for(i = 0; i < row_count(tab); i++)
    {
      const row_t *r = &rows[tab][i];
      u8 *base = (u8 *)&g_game_config, *v = (u8 *)r->value;
      if(!r->per_game || !r->value || (v < base) || (v >= base + sizeof(game_config_t)))
        continue;
      if((int)*(const u32 *)((const u8 *)cfg + (v - base)) != r->def)
        n++;
    }
  return n;
}
