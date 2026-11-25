/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Hachi
 */

#include "../common.h"
#include "ui.h"
#include "ui_help_pics.h"
#include "ui_lang_masks.h"

enum { K_SEG, K_SWITCH, K_BLOCKS, K_CYCLE, K_SCREEN, K_ACTION, K_OFF };

typedef struct
{
  const char *label;
  const char *tip;
  u8 kind;
  u8 per_game;
  u8 count;
  const char *opts[4];
  u32 *value;
  const u8 *map;
  int def;
  int lo, hi;
  const char *fmt;
  const char **names;
} row_t;

#define TABS 5
#define MAX_ROWS 10
#define ROWS_Y 62
#define VIEW_T 60
#define VIEW_B 226
#define ROW_X  UI_MARGIN
#define ROW_W  (UI_SCREEN_W - UI_MARGIN * 2)
#define SCREEN_ROW_H 40

enum { ACT_RESET_ALL, ACT_RESTART, ACT_QUIT };
#define LANG_TAB 4
#define PAD_TAB 2

static const u8 map_frameskip[3] = { no_frameskip, auto_frameskip, manual_frameskip };
static const u8 map_ff[2] = { FF_HOLD, FF_TOGGLE };
static const u8 map_clock[3] = { 10, 7, 9 };
static const u8 map_sens[10] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

static const char *language_names[12] =
{
  "Japanese", "English", "French", "Spanish", "German", "Italian",
  "Dutch", "Portuguese", "Russian", "Korean", "Chinese (traditional)",
  "Chinese (simplified)"
};

static row_t rows[TABS][MAX_ROWS] =
{
  {
    { TN("Screen size"), TN("Original keeps the GBA's pixels exact; Fit fills the height; Full fills the screen."),
      K_SCREEN, 0, 3, { TN("Original"), TN("Fit"), TN("Full") }, &g_gpsp_config.screen_mode, NULL, scaled_aspect },
    { TN("Smoothing"), TN("Sharp keeps pixels crisp when the picture is scaled; Smooth softens them."),
      K_SEG, 0, 3, { TN("Off"), TN("Smooth"), TN("Sharp") }, &g_gpsp_config.screen_filter, NULL, filter_sharp_bilinear },
    { TN("Side bars"), TN("What fills the space beside the picture in 3:2 sizes."),
      K_SEG, 0, 3, { TN("Black"), TN("Pattern"), TN("Game colours") }, &g_gpsp_config.side_bars, NULL, SIDE_BARS_BLACK },
    { TN("Frameskip"), TN("Auto skips frames only when the game would slow down."),
      K_SEG, 1, 3, { TN("Off"), TN("Auto"), TN("Fixed") }, &g_game_config.frameskip_type, map_frameskip, auto_frameskip },
    { TN("Skip up to"), TN("The most frames Auto skips in a row; Fixed skips this many."),
      K_CYCLE, 1, 0, { NULL }, &g_game_config.frameskip_value, NULL, 9, 0, 99, TN("%d frames") },
    { TN("Picture scale"), TN("Below 100 % the picture shrinks inside its screen size."),
      K_CYCLE, 0, 0, { NULL }, &g_gpsp_config.screen_scale, NULL, 100, 50, 200, "%d%%" },
  },
  {
    { TN("Sound"), TN("Off mutes the game and runs a little faster."), K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.enable_audio, NULL, YES },
    { TN("Sound delay"), TN("Less delay feels faster; more delay stops crackles in busy games."),
      K_BLOCKS, 1, 11, { NULL }, &g_game_config.audio_buffer_size_number, NULL, 2 },
  },
  {
    { TN("Use the stick"), TN("The stick works like the d-pad, or does what you pick for each direction."),
      K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.enable_analog, NULL, NO },
    { TN("Sensitivity"), TN("The ring shows where a push starts to count."),
      K_BLOCKS, 0, 10, { NULL }, &g_gpsp_config.analog_sensitivity_level, map_sens, 4, 0, 0, "%d" },
    { TN("Tilt sensor"), TN("For tilt-cartridge games: the stick stands in for tilting."),
      K_SWITCH, 1, 2, { NULL }, &g_game_config.allocate_sensor, NULL, NO },
  },
  {
    { TN("Save when leaving a game"), TN("Continue on the Home screen picks up exactly here."),
      K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.save_on_leave, NULL, YES },
    { TN("Fast forward"), TN("Hold: fast while the button is down. Toggle: one press on, one press off."),
      K_SEG, 0, 2, { TN("Hold"), TN("Toggle") }, &g_gpsp_config.fast_forward_mode, map_ff, FF_TOGGLE },
    { TN("Fast forward speed"), TN("Max runs as fast as the PSP can, so its speed changes with each game."),
      K_SEG, 0, 3, { "2\xC3\x97", "3\xC3\x97", TN("Max") }, &g_gpsp_config.ff_speed, NULL, FF_SPEED_3X },
    { TN("Sound while fast"), TN("Off keeps fast forward quiet."), K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.fast_forward_sound, NULL, YES },
    { TN("Rapid-fire speed"), TN("How fast a rapid-fire button presses while you hold it."), K_SEG, 1, 3, { TN("Slow"), TN("Medium"), TN("Fast") },
      &g_game_config.rapidfire_speed, NULL, RAPIDFIRE_FAST },
    { TN("Show FPS"), TN("Shows the frames per second in the top-left corner."), K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.show_fps, NULL, NO },
  },
  {
    { TN("Language"), TN("The language comes from your PSP until you pick one."),
      K_CYCLE, 0, 12, { NULL }, &g_gpsp_config.language, NULL, 1, 0, 0, NULL, language_names },
    { TN("CPU speed"), TN("333 MHz runs every game at full speed; lower saves battery."),
      K_SEG, 1, 3, { "222", "266", TN("333 MHz") }, &g_game_config.clock_speed_number, map_clock, 9 },
    { TN("Write game saves"), TN("Right away writes the game's save at once; On exit waits until you leave."),
      K_SEG, 1, 2, { TN("On exit"), TN("Right away") }, &g_game_config.update_backup_flag, NULL, YES },
    { TN("Start games with"), TN("GBA BIOS shows the GBA's start-up logo first; Game Cart skips it."), K_SEG, 0, 2, { TN("Game Cart"), TN("GBA BIOS") }, &g_gpsp_config.boot_mode, NULL, GAME_CART },
    { TN("No-BIOS notice"), TN("Shows a line in the library each time kai starts without a GBA BIOS."),
      K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.bios_note, NULL, YES },
    { TN("Show Recent tab"), TN("Off: the library opens on All."),
      K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.show_recent, NULL, YES },
    { TN("Confirm button"), TN("PSP setting follows the console: \xC3\x97 on most PSPs, \xE2\x97\x8B on Japanese models."),
      K_SEG, 0, 3, { TN("PSP setting"), UI_SEG_CROSS, UI_SEG_CIRCLE }, &g_gpsp_config.confirm_button, NULL, CONFIRM_SYSTEM },
    { TN("Restart kai"), TN("Saves like Exit, starts kai again and brings you back here."),
      K_ACTION, 0, 0, { NULL }, NULL, NULL, ACT_RESTART },
    { TN("Quit kai"), TN("Saves like Exit, then closes kai."),
      K_ACTION, 0, 0, { NULL }, NULL, NULL, ACT_QUIT },
    { TN("Reset all settings"), TN("Puts kai's and this game's settings back to their defaults."),
      K_ACTION, 0, 0, { NULL }, NULL, NULL, ACT_RESET_ALL },
  },
};

static const char *tab_labels[TABS] = { TN("Display"), TN("Sound"), TN("Controls"), TN("Play"), TN("System") };

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

static int row_pitch(int tab, const row_t *r)
{
  return row_height(r) + ((tab == PAD_TAB) ? UI_ROW_PITCH_COMPACT : UI_ROW_PITCH) - UI_ROW_H;
}

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

static struct
{
  int tab, tab_prev, tab_f;
  int cur, cursor_from, cursor_f, rest;
  int cursor_h_from;
  int top;
  int top_from, top_f;
  int value_f, value_from;
  int dirty;
  ui_hold_t reset_hold;
  ui_hold_t action_hold;
  u32 undo[TABS][MAX_ROWS];
  int undo_all;
  char toast[128];
  int toast_f, toast_undo;
  u16 toast_edge;
  int restart;
  int page;
  int btn;
  int dir;
  int pick_map;
  int pick, pick_f, pick_out;
  int help, help_f, help_out;
  int pcur, pcur_from, pcur_f;
  u32 undo_map[MAX_GAMEPAD_CONFIG_MAP];
  u32 undo_map_default;
} s;

const u16 *ui_settings_game;

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

static int top_now(void)
{
  return ui_ease_out_back(s.top_from, s.top, s.top_f, UI_F_SCROLL);
}

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

static void draw_screen_choices(const row_t *r, int x, int y, int selected, u16 group)
{
  static const u8 inner_w[3] = { 15, 26, 30 }, inner_h[3] = { 10, 17, 17 };
  int k = row_index(r), j, xr = x;

  for(j = 2; j >= 0; j--)
  {
    const char *opt = T(r->opts[j]);
    int tw = ui_text_width((char *)opt), bx, by = y + 11;
    int on = (j == k);

    xr -= tw + 8;
    if(on)
      ui_round_fill(xr - 4, y + 10, tw + 8, 15, 2, selected ? UI_COL_PLUM : UI_COL_LIME);
    ui_text(xr, y + 11, (char *)opt, on ? (selected ? UI_COL_CREAM : UI_COL_PLUM) :
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
  char text[64];
  int rx = row_x(tab), rw = row_w(tab);

  if(r->kind == K_OFF)
  {
    ui_card_row(rx, y, rw, T(r->label), 0, g, UI_ROW_DISABLED, &vx, &vy);
    ui_row_note(vx, vy, T(r->opts[0]));
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
    ui_print(rx + 8 - lift, y + 12 - lift, T(r->label), selected ? UI_COL_PLUM : UI_COL_CREAM,
     UI_FONT_BODY, 0);
    draw_screen_choices(r, rx + rw - 8 - lift, y - lift, selected, g);
    return;
  }

  ui_card_row(rx, y, rw, T(r->label), selected, g, flags, &vx, &vy);
  if(is_lang_row(tab, i) && lang_pending())
  {
    int lift = selected ? UI_LIFT : 0;
    int px = rx + 8 - lift + ui_print_width(T(r->label), UI_FONT_BODY, 0) + 10;
    int pw = ui_print_width(T("next start"), UI_FONT_SMALL, 0) + 14;
    ui_round_fill(px, y + 3 - lift, pw, 14, 2, UI_COL_SKY);
    ui_print(px + 7, y + 3 - lift, T("next start"), UI_COL_PLUM, UI_FONT_SMALL, 0);
  }
  k = (r->value) ? row_index(r) : 0;
  switch(r->kind)
  {
    case K_SEG:
    {
      const char *opts[4];
      int j;
      for(j = 0; j < 4; j++)
        opts[j] = r->opts[j] ? T(r->opts[j]) : NULL;
      ui_row_segments(vx, vy, opts, r->count, k, selected ? s.value_from : k, f, selected, g);
      break;
    }
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
      if(r->names == language_names)
      {
        const ui_lang_mask_t *m = &ui_lang_masks[(k >= 0) && (k < 12) ? k : 1];
        ui_row_cycle_mask(vx, vy, m->bits, m->w, selected, g, selected ? f : UI_F_VALUE);
        break;
      }
      if(r->names)
        snprintf(text, sizeof(text), "%s", r->names[(k >= 0) && (k < r->count) ? k : 0]);
      else
        snprintf(text, sizeof(text), T(r->fmt), (int)*r->value);
      ui_row_cycle(vx, vy, text, selected, g, selected ? f : UI_F_VALUE);
      break;
    case K_ACTION:
      ui_row_action(vx, vy, selected);
      break;
  }
}

static void show_toast(const char *text, int undo, u16 edge);

extern const u32 gamepad_config_map_init[MAX_GAMEPAD_CONFIG_MAP];

#define PAD_BX 128
#define PAD_BY 56
#define PAD_BW 224
#define PAD_BH 120
#define CHIP_W 112
#define CHIP_H 18
#define PAD_ACTIONS 22

typedef struct
{
  u8 map;
  u32 key;
  s16 x, y;
  s16 ax, ay;
} chip_t;

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

#define PICKS 21
static const u8 pick_ids[PICKS] =
{
  BUTTON_ID_UP, BUTTON_ID_DOWN, BUTTON_ID_LEFT, BUTTON_ID_RIGHT, BUTTON_ID_A, BUTTON_ID_B,
  BUTTON_ID_L, BUTTON_ID_R, BUTTON_ID_START, BUTTON_ID_SELECT, BUTTON_ID_MENU,
  BUTTON_ID_FASTFORWARD, BUTTON_ID_LOADSTATE, BUTTON_ID_SAVESTATE, BUTTON_ID_RAPIDFIRE_A,
  BUTTON_ID_RAPIDFIRE_B, BUTTON_ID_RAPIDFIRE_L, BUTTON_ID_RAPIDFIRE_R, BUTTON_ID_FPS,
  BUTTON_ID_CHEATS, BUTTON_ID_NONE
};

static const char *action_name(u32 id)
{
  static const char *names[PAD_ACTIONS] =
  {
    TN("Up"), TN("Down"), TN("Left"), TN("Right"), "A", "B", "L", "R", "START", "SELECT", TN("Pause screen"),
    TN("Fast forward"), TN("Quick load"), TN("Quick save"), TN("Rapid A"), TN("Rapid B"), TN("Rapid L"), TN("Rapid R"),
    TN("None"), TN("None"), TN("Show FPS"), TN("None")
  };
  if(id == BUTTON_ID_CHEATS)
    return T("Cheats");
  return T(names[(id < PAD_ACTIONS) ? id : PAD_ACTIONS - 1]);
}

static int pick_cell(u32 id)
{
  int i;
  for(i = 0; i < PICKS; i++)
    if(pick_ids[i] == id)
      return i;
  return PICKS - 1;
}

static int chip_remapped(int i)
{
  int m = chips[i].map;
  return gamepad_config_map[m] != gamepad_config_map_init[m];
}

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
   selected ? UI_COL_PLUM : UI_COL_CREAM, UI_FONT_SMALL, 0, x + 100 - gx);
  if(chip_remapped(i))
    ui_fill(x + 102, y + 7, 4, 4, selected ? UI_COL_PLUM : UI_COL_TANGERINE);
  draw_pad_button(i, selected ? UI_COL_LIME :
   (((c->map >= 6) && (c->map <= 9)) || (c->map <= 3) ? UI_COL_CREAM : UI_COL_MUTE));
}

#define PAD_SHOT_W 96
#define PAD_SHOT_H 64
static u16 pad_shot[PAD_SHOT_W * PAD_SHOT_H];
static int pad_shot_ok;

static void draw_pad_body(void)
{
  int i;

  ui_round_fill(PAD_BX + 3, PAD_BY + 3, PAD_BW, PAD_BH, 4, UI_COL_SHADE);
  ui_round_fill(PAD_BX, PAD_BY, PAD_BW, PAD_BH, 4, UI_COL_CARD);
  ui_fill(PAD_BX + 52, PAD_BY + 14, 120, 68, UI_COL_SHADE);
  if(ui_settings_game)
  {
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

#define PAD_TIP TN("Confirm picks what this button does in this game.")
#define STICK_DIR_TIP TN("Confirm picks what this direction does in this game.")

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
  s.pcur = pick_cell(id);
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
    if(!menu_reachable_without(m, pick_ids[s.pcur]))
    {
      show_toast(T("One button has to open the menu"), 0, UI_COL_PINK);
      return;
    }
    if(gamepad_config_map[m] != pick_ids[s.pcur])
    {
      gamepad_config_map[m] = pick_ids[s.pcur];
      g_game_config.use_default_gamepad_map = NO;
      s.dirty = 1;
    }
    s.pick_out = 1;
    s.pick_f = 0;
    return;
  }
  if(ui_repeat(PSP_CTRL_LEFT) && (to % 4 > 0)) to--;
  if(ui_repeat(PSP_CTRL_RIGHT) && (to % 4 < 3) && (to + 1 < PICKS)) to++;
  if(ui_repeat(PSP_CTRL_UP) && (to >= 4)) to -= 4;
  if(ui_repeat(PSP_CTRL_DOWN) && (to + 4 < PICKS)) to += 4;
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
  n = ui_print(PICK_X + 16, top + 6, T("Button action \xC2\xB7 "), UI_COL_PLUM, UI_FONT_BODY, 0);
  ui_glyph(n, top + 8, ui_key_confirm);
  for(i = 0; i < PICKS; i++)
  {
    int x, y;
    picker_cell(i, &x, &y, top);
    if(i == s.pcur)
    {
      ui_round_fill(x + 1, y + 1, PICK_CW - 2, PICK_CH, 2, UI_COL_SHADE);
      ui_round_fill(x - 1, y - 1, PICK_CW - 2, PICK_CH, 2, UI_COL_LIME);
      ui_print_fit(x + 5, y + 2, action_name(pick_ids[i]), UI_COL_PLUM, UI_FONT_SMALL, 0, PICK_CW - 10);
    }
    else
    {
      ui_round_frame(x, y, PICK_CW - 2, PICK_CH, 2, UI_COL_MUTE);
      ui_print_fit(x + 6, y + 3, action_name(pick_ids[i]), UI_COL_PLUM, UI_FONT_SMALL, 0, PICK_CW - 10);
    }
  }
  picker_cell(s.pcur_from, &ax, &ay, top);
  picker_cell(s.pcur, &bx, &by, top);
  fx = ui_ease_out_back(ax, bx, s.pcur_f, UI_F_CURSOR);
  fy = ui_ease_out_back(ay, by, s.pcur_f, UI_F_CURSOR);
  ui_round_frame(fx - 4, fy - 4, PICK_CW + 4, PICK_CH + 6, 2, UI_COL_PLUM);

  hints[0].keys = ui_key_back;    hints[0].verb = (char *)T("Back");
  hints[1].keys = ui_key_confirm; hints[1].verb = (char *)T("Pick");
  ui_hint_packed(hints, 2, 14);
  return 1;
}

#define STICK_CX 122
#define STICK_CY 140
#define STICK_R  46

static void draw_page_switch(void)
{
  static const char *names[2] = { TN("Buttons"), TN("Stick") };
  int x = UI_MARGIN, i;

  for(i = 0; i < 2; i++)
  {
    const char *name = T(names[i]);
    int w = ui_print_width(name, UI_FONT_SMALL, 0) + 16;
    if(i == s.page)
      ui_round_fill(x, 38, w, 16, 2, UI_COL_LIME);
    ui_print(x + 8, 39, name, (i == s.page) ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_SMALL, 0);
    x += w;
  }
  ui_glyph(x + 6, 40, PSP_CTRL_SELECT);
}

static const u32 stick_keys[4] = { PSP_CTRL_UP, PSP_CTRL_DOWN, PSP_CTRL_LEFT, PSP_CTRL_RIGHT };

static int stick_pickable(void)
{
  return g_gpsp_config.enable_analog != NO;
}

static void draw_stick(void)
{
  static const s8 dirs[4][2] = { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } };
  int sens = g_gpsp_config.analog_sensitivity_level + 1;
  int dz = STICK_R * (75 - sens * 6) / 100, use = stick_pickable(), i;
  u16 nub = COLOR16(25, 22, 23);

  draw_page_switch();
  ui_disc(STICK_CX, STICK_CY, STICK_R + 3, UI_COL_SHADE);
  ui_disc(STICK_CX, STICK_CY, STICK_R, UI_COL_CARD);
  ui_ring(STICK_CX, STICK_CY, dz, 2, use ? UI_COL_LIME : UI_COL_MUTE);
  ui_disc(STICK_CX, STICK_CY, 12, UI_COL_CREAM);
  ui_disc(STICK_CX, STICK_CY, 8, nub);
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

static const char *const help_copy[][2] =
{
  { "Screen size", TN("Original shows the GBA's pixels at their own size. Fit fills the height and keeps the shape. Full stretches to the whole screen.") },
  { "Smoothing", TN("How the GBA picture is drawn when it is scaled up to the PSP screen.") },
  { "Side bars", TN("What fills the space beside the picture in Original and Fit: plain black, kai's pattern, or colours taken from the game.") },
  { "Frameskip", TN("When the PSP can't draw every frame, skipping some keeps the game at full speed. Auto skips only when the game would slow down; Fixed always skips the number set in Skip up to.") },
  { "Skip up to", TN("The most frames in a row that Auto may skip; Fixed always skips this many. Lower looks smoother, higher keeps the speed.") },
  { "Picture scale", TN("Shrinks the picture inside its screen size, for a TV that cuts off the edges of the picture.") },
  { "Sound", TN("Off mutes the game. With no sound to make, the PSP has a little more time for the picture.") },
  { "Sound delay", TN("If the sound crackles in a busy game, add a block. If the sound feels late, take one away.") },
  { "Use the stick", TN("On: the stick works like the d-pad, or does what you pick for each direction. Off: the stick does nothing.") },
  { "Sensitivity", TN("How light a push counts. Higher reacts to a lighter push; lower ignores small wobbles.") },
  { "Tilt sensor", TN("For games made for a tilt cartridge: the stick stands in for tilting, with the centre as level. Use the stick must be on.") },
  { "Save when leaving a game", TN("On: leaving a game saves to Last, so Continue picks up exactly where you were. Off: Last stays as it was.") },
  { "Fast forward", TN("Hold: the game runs fast while you hold the button. Toggle: one press turns it on, the next turns it off.") },
  { "Fast forward speed", TN("2\xC3\x97 and 3\xC3\x97 are steady. Max runs as fast as the PSP can, so its speed changes with each game.") },
  { "Sound while fast", TN("Off keeps fast forward quiet. On plays the game's sound, sped up.") },
  { "Rapid-fire speed", TN("How fast a rapid-fire button presses while you hold it. Choose which buttons fire rapidly in Controls.") },
  { "Show FPS", TN("Shows the frames per second in the top-left corner. 60 is full speed.") },
  { "Language", TN("The language comes from your PSP until you pick one here. A new language applies after a restart.") },
  { "CPU speed", TN("333 MHz runs every game at full speed. 222 and 266 save battery, but some games slow down.") },
  { "Write game saves", TN("Right away writes the game's own save as soon as the game saves. On exit waits until you leave the game.") },
  { "Start games with", TN("GBA BIOS shows the GBA's start-up logo first, as on a real GBA. Game Cart skips it and works without a BIOS.") },
  { "No-BIOS notice", TN("Off hides the No GBA BIOS line in the library, for when you play without a BIOS.") },
  { "Show Recent tab", TN("Off hides the Recent tab and its Continue card; the library opens on All. \xC3\x97 Play still picks up where you left off.") },
  { "Confirm button", TN("PSP setting follows the console: \xC3\x97 on most PSPs, \xE2\x97\x8B on Japanese models. Or choose one here.") },
  { "Restart kai", TN("Saves like Exit, then starts kai again and brings you back here.") },
  { "Quit kai", TN("Saves like Exit, then closes kai and goes back to the PSP's home menu.") },
  { "Reset all settings", TN("Puts kai's settings and this game's own settings back to their defaults. Savestates, game saves and other games' settings stay.") },
  { NULL, NULL }
};

#define HELP_PAD TN("Pick what this button does in this game: a GBA button, rapid fire, fast forward, quick save or load, the pause screen, or Show FPS.")

static int help_topic(const char **title, const char **body)
{
  const row_t *r;
  const char *t, *b = NULL;
  int i;

  if((s.tab == PAD_TAB) && (s.page == 0))
  {
    t = TN("Buttons");
    b = HELP_PAD;
  }
  else if((s.tab == PAD_TAB) && (s.dir >= 0))
  {
    t = TN("Stick");
    b = HELP_PAD;
  }
  else
  {
    r = &rows[s.tab][s.cur];
    if(!r->label)
      return 0;
    t = r->label;
    for(i = 0; help_copy[i][0]; i++)
      if(!strcmp(help_copy[i][0], t))
        b = help_copy[i][1];
    if(!b)
      return 0;
  }
  if(title)
    *title = t;
  if(body)
    *body = b;
  return 1;
}

static void draw_smooth_help(int f, int closing)
{
  static const char *names[3] = { TN("Off"), TN("Smooth"), TN("Sharp") };
  static const char *lines[3] = { TN("Pixels stay hard but uneven."), TN("Soft, a little blurry."),
   TN("Crisp and even. Best for most games.") };
  int w = 440, h = 214, x = (UI_SCREEN_W - w) / 2, y, i, j, r, c;
  ui_hint_t hh[2];
  char line[96];

  if(closing)
    y = 26 + ui_ease_in_cubic(0, UI_SCREEN_H - 26, f, UI_F_HELP_OUT);
  else
    y = 26 + 30 - ui_ease_out_back(0, 30, f, UI_F_HELP_IN);
  if(!(closing && (f >= UI_F_HELP_OUT)))
    ui_door(0, 0, UI_SCREEN_W, UI_STRIP_Y);
  ui_round_fill(x + 4, y + 4, w, h, 3, UI_COL_SHADE);
  ui_round_fill(x, y, w, h, 3, UI_COL_CREAM);
  ui_fill(x, y + 4, 6, h - 8, UI_COL_LIME);
  ui_print(x + 18, y + 4, T("Smoothing"), UI_COL_PLUM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);
  ui_print_fit(x + 18, y + 38, T(help_copy[1][1]), UI_COL_PLUM, UI_FONT_SMALL, 0, w - 36);
  for(i = 0; i < 3; i++)
  {
    int px = x + 27 + i * 134, py = y + 60, on = (i == 2);
    ui_fill(px - 3, py - 3, 123 + 6, 81 + 6, on ? UI_COL_LIME : COLOR16(23, 21, 19));
    for(r = 0; r < HELP_PIC_H; r++)
      for(c = 0; c < HELP_PIC_W; c++)
        ui_fill(px + c * 3, py + r * 3, 3, 3, help_smooth_pics[i][r * HELP_PIC_W + c]);
    ui_print(px, py + 88, T(names[i]), UI_COL_PLUM, UI_FONT_BODY, on ? UI_TEXT_BOLD : 0);
    for(j = 0; (j < 3) && ui_wrap_line(T(lines[i]), j, 123, UI_FONT_SMALL, line, sizeof(line)); j++)
      ui_print(px, py + 106 + j * 13, line, UI_COL_PLUM, UI_FONT_SMALL, 0);
  }
  hh[0].keys = PSP_CTRL_SQUARE; hh[0].verb = "";
  hh[1].keys = ui_key_back;     hh[1].verb = (char *)T("Close");
  ui_hint_packed(hh, 2, 14);
}

static void draw_hints(void)
{
  ui_hint_t hints[6];
  int n = 0;

  hints[n].keys = PSP_CTRL_START; hints[n++].verb = (char *)T("Hold: reset tab");
  if(help_topic(NULL, NULL))
  {
    hints[n].keys = PSP_CTRL_SQUARE; hints[n++].verb = (char *)T("Help");
  }
  hints[n].keys = ui_key_back;    hints[n++].verb = (char *)T("Back");
  if(s.tab == PAD_TAB)
  {
    hints[n].keys = PSP_CTRL_SELECT; hints[n++].verb = (char *)(s.page ? T("Buttons") : T("Stick"));
    if((s.page == 0) || (s.dir >= 0))
    {
      hints[n].keys = ui_key_confirm; hints[n++].verb = (char *)T("Pick");
      ui_hint_packed(hints, n, 14);
      return;
    }
  }
  switch(action_of(s.tab, s.cur))
  {
    case ACT_RESET_ALL:
      hints[n].keys = ui_key_confirm; hints[n++].verb = (char *)T("Hold: reset all");
      break;
    case ACT_RESTART:
      hints[n].keys = ui_key_confirm;
      hints[n++].verb = (char *)T("Hold: restart");
      break;
    case ACT_QUIT:
      hints[n].keys = ui_key_confirm;
      hints[n++].verb = (char *)T("Hold: quit");
      break;
    default:
      if(rows[s.tab][s.cur].kind == K_SWITCH)
      {
        hints[n].keys = ui_key_confirm; hints[n++].verb = (char *)T("Switch");
        break;
      }
      hints[n].keys = PSP_CTRL_LEFT | PSP_CTRL_RIGHT; hints[n++].verb = (char *)T("Change");
      if(is_lang_row(s.tab, s.cur) && lang_pending())
      {
        hints[n].keys = ui_key_confirm; hints[n++].verb = (char *)T("Hold: restart now");
      }
      break;
  }
  ui_hint_packed(hints, n, 14);
}

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
    ui_text(UI_MARGIN + 16, 39, (char *)T("Saved for this game only"), UI_COL_MUTE);
  }
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

static void draw_selection(void)
{
  int top = top_now(), n = row_count(s.tab), i;
  int rx = row_x(s.tab), rw = row_w(s.tab);
  int hh = ui_ease_out_back(s.cursor_h_from, row_height(&rows[s.tab][s.cur]), s.cursor_f, UI_F_CURSOR);
  int cy = ui_ease_out_back(s.cursor_from, row_y(s.tab, s.cur), s.cursor_f, UI_F_CURSOR) - top;
  int bx = rx - 4, by = cy - 2, bw = rw + 6, bh = hh + 3, lb = list_bottom(top);

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
    draw_row(s.tab, s.cur, row_y(s.tab, s.cur) - top, 1, s.value_f);
    ui_clip_reset();
  }
  ui_cursor_frame(rx - UI_LIFT, cy - UI_LIFT, rw, hh);
}

static int tip_hold_bar(const ui_hold_t *h, u16 color)
{
  if(!h->shown || (h->p <= 0))
    return 0;
  ui_fill(UI_MARGIN, 235, ROW_W, 6, UI_COL_CARD);
  ui_fill(UI_MARGIN, 235, ROW_W * ((h->p < 256) ? h->p : 256) / 256, 6, color);
  return 1;
}

static void draw_tip_line(void)
{
  const char *tip = NULL;
  char text[160];
  int k;

  if(tip_hold_bar(&s.reset_hold, UI_COL_TANGERINE))
    return;
  if(tip_hold_bar(&s.action_hold, ((action_of(s.tab, s.cur) == ACT_RESET_ALL) ||
   (action_of(s.tab, s.cur) == ACT_QUIT)) ? UI_COL_PINK : UI_COL_SKY))
    return;
  if(s.reset_hold.fired || s.action_hold.fired)
    return;

  if(s.rest < UI_F_TIP_DELAY + 8)
    s.rest++;
  if(s.rest <= UI_F_TIP_DELAY)
    return;
  k = s.rest - UI_F_TIP_DELAY;
  if(k > 8)
    k = 8;

  if(s.tab == PAD_TAB)
    tip = (s.page == 0) ? PAD_TIP : ((s.dir >= 0) ? STICK_DIR_TIP : rows[s.tab][s.cur].tip);
  else if(is_lang_row(s.tab, s.cur) && lang_pending())
  {
    snprintf(text, sizeof(text), T("Applies after a restart: hold %s to restart now, or it waits for the next start."),
     (ui_key_confirm == PSP_CTRL_CIRCLE) ? "\xE2\x97\x8B" : "\xC3\x97");
    tip = text;
  }
  else
    tip = rows[s.tab][s.cur].tip;
  if(!tip)
    return;
  if(tip != text)
  {
    snprintf(text, sizeof(text), "%s", T(tip));
    tip = text;
  }
  {
    char line[168];
    int c, col = UI_COL_MUTE & 0x8000;
    snprintf(line, sizeof(line), "\xE2\x96\xB8 %s", text);
    for(c = 0; c < 15; c += 5)
    {
      int a = (UI_COL_PLUM >> c) & 31, b = (UI_COL_MUTE >> c) & 31;
      col |= ((a + (b - a) * k / 8) & 31) << c;
    }
    ui_print_fit(UI_MARGIN, 231 + 6 - 6 * UI_EASE_OUT_CUBIC_8[k - 1] / 256, line, (u16)col,
     UI_FONT_SMALL, 0, ROW_W);
  }
}

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
  show_toast(T("Settings are back as they were"), 0, UI_COL_LIME);
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

static int next_row(int dir)
{
  int n = row_count(s.tab), i = s.cur, k;

  for(k = 0; k < n; k++)
  {
    i += dir;
    if((i < 0) || (i >= n))
    {
      if(!ui_pressed(PSP_CTRL_UP | PSP_CTRL_DOWN))
        return s.cur;
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
  s.top_from = s.top;
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
        return;
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

#ifdef UI_PERF_SETTINGS
static const struct
{
  const char *name;
  int frames;
  u32 keys;
  int tap_every;
} perf_phases[] =
{
  { "still",       60, 0,                  0 },
  { "tap_down",   160, PSP_CTRL_DOWN,     20 },
  { "hold_down",  120, PSP_CTRL_DOWN,      0 },
  { "hold_up",    120, PSP_CTRL_UP,        0 },
  { "tabs_right", 120, PSP_CTRL_RTRIGGER, 30 },
  { "tabs_left",   60, PSP_CTRL_LTRIGGER, 30 },
  { "pad_still",   30, 0,                  0 },
  { "chips_right",120, PSP_CTRL_RIGHT,    20 },
  { "chips_down",  80, PSP_CTRL_DOWN,     20 },
  { "to_stick",    30, PSP_CTRL_SELECT,   30 },
  { "stick_down", 120, PSP_CTRL_DOWN,     20 },
  { "still_end",   60, 0,                  0 },
};
#define PERF_PHASES (int)(sizeof(perf_phases) / sizeof(perf_phases[0]))
#define PERF_MAX 1100

static struct
{
  u8 phase, tab, page;
  u16 logic, ground, draw;
  u32 period;
} perf_log[PERF_MAX];
static int perf_phase, perf_f, perf_n, perf_on, perf_done;

static u32 perf_script(void)
{
  if(!perf_on || (perf_phase >= PERF_PHASES))
    return 0;
  if(perf_phases[perf_phase].tap_every)
    return ((perf_f % perf_phases[perf_phase].tap_every) < 3) ? perf_phases[perf_phase].keys : 0;
  return perf_phases[perf_phase].keys;
}

static u16 perf_us(u32 t)
{
  return (t > 0xFFFF) ? 0xFFFF : t;
}

static void perf_step(u32 logic, u32 ground, u32 draw, u32 period)
{
  if(!perf_on || (perf_phase >= PERF_PHASES))
    return;
  if(perf_n < PERF_MAX)
  {
    perf_log[perf_n].phase = perf_phase;
    perf_log[perf_n].tab = s.tab;
    perf_log[perf_n].page = s.page;
    perf_log[perf_n].logic = perf_us(logic);
    perf_log[perf_n].ground = perf_us(ground);
    perf_log[perf_n].draw = perf_us(draw);
    perf_log[perf_n].period = period;
    perf_n++;
  }
  if(++perf_f >= perf_phases[perf_phase].frames)
  {
    perf_f = 0;
    perf_phase++;
  }
}

static void perf_write(void)
{
  char path[MAX_PATH], line[128];
  SceUID fd;
  int i;

  path_printf(path, sizeof(path), "%s/perf_settings.csv", main_path);
  fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  if(fd < 0)
    return;
  sprintf(line, "# cpu %d MHz, bus %d MHz, frames %d\n", scePowerGetCpuClockFrequency(),
   scePowerGetBusClockFrequency(), perf_n);
  sceIoWrite(fd, line, strlen(line));
  strcpy(line, "phase,frame,tab,page,logic_us,ground_us,draw_us,work_us,period_us\n");
  sceIoWrite(fd, line, strlen(line));
  for(i = 0; i < perf_n; i++)
  {
    sprintf(line, "%s,%d,%d,%d,%u,%u,%u,%u,%u\n", perf_phases[perf_log[i].phase].name, i,
     perf_log[i].tab, perf_log[i].page, perf_log[i].logic, perf_log[i].ground, perf_log[i].draw,
     (unsigned)(perf_log[i].logic + perf_log[i].ground + perf_log[i].draw),
     (unsigned)perf_log[i].period);
    sceIoWrite(fd, line, strlen(line));
  }
  sceIoClose(fd);
}

static void perf_finish(void)
{
  ui_input_script = NULL;
  if(perf_on)
  {
    perf_write();
    perf_on = 0;
    perf_done = 1;
  }
}
#endif

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
  {
    int lang = 1;
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &lang);
    rows[4][0].def = lang;
  }
  if((tab > 0) && (tab < TABS))
    s.tab = tab;
  first_row();
  if((row > 0) && (row < row_count(s.tab)) && row_selectable(&rows[s.tab][row]))
  {
    s.cur = row;
    scroll_to_cursor();
    s.top_from = s.top;
    s.top_f = UI_F_SCROLL;
    s.cursor_from = row_y(s.tab, s.cur);
    s.cursor_h_from = row_height(&rows[s.tab][s.cur]);
  }
  ui_input_init();
  ui_status_read();
  build_ground();
#ifdef UI_PERF_SETTINGS
  if(!perf_done)
  {
    perf_phase = perf_f = perf_n = 0;
    perf_on = 1;
    ui_input_script = perf_script;
  }
#endif

  while(1)
  {
    int moved = 0;
#ifdef UI_PERF_SETTINGS
    static u32 perf_last;
    u32 perf_t0 = sceKernelGetSystemTimeLow(), perf_t1, perf_t2;
#endif

    ui_input_update();
    if(s.pick)
    {
      picker_input();
      goto draw;
    }
    if(s.help)
    {
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
    else if(!moved && ui_repeat(PSP_CTRL_UP | PSP_CTRL_DOWN))
    {
      int dn = ui_repeat(PSP_CTRL_DOWN) != 0, to = next_row(dn ? 1 : -1);
      if((s.tab == PAD_TAB) && dn && (to <= s.cur) && stick_pickable())
      {
        s.dir = 3;
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
        s.dirty = 1;
      if((s.tab == PAD_TAB) && (ui_repeat(PSP_CTRL_LEFT | PSP_CTRL_RIGHT) || ui_pressed(ui_key_confirm)))
        s.dirty = 1;
      if((rows[s.tab][s.cur].value == &g_gpsp_config.confirm_button) && ui_repeat(PSP_CTRL_LEFT | PSP_CTRL_RIGHT))
      {
        ui_input_init();
        s.dirty = 1;
      }
      if((rows[s.tab][s.cur].kind == K_SWITCH) && ui_pressed(ui_key_confirm))
        change_value(1);
    }

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

    ui_hold_step(&s.reset_hold, PSP_CTRL_START);
    if(s.reset_hold.done)
    {
      char text[128];
      remember_tab(s.tab);
      s.undo_all = 0;
      reset_tab(s.tab);
      snprintf(text, sizeof(text), T("%s reset"), T(tab_labels[s.tab]));
      show_toast(text, 1, UI_COL_LIME);
      s.reset_hold.p = 0;
      s.reset_hold.shown = 0;
      s.dirty = 1;
    }

    if((s.dir >= 0) && !stick_pickable())
    {
      s.dir = -1;
      s.dirty = 1;
    }

    if((action_of(s.tab, s.cur) == ACT_RESTART) || (is_lang_row(s.tab, s.cur) && lang_pending()))
    {
      ui_hold_step(&s.action_hold, ui_key_confirm);
      if(s.action_hold.done)
      {
        s.restart = 1;
        break;
      }
    }

    if(action_of(s.tab, s.cur) == ACT_QUIT)
    {
      ui_hold_step(&s.action_hold, ui_key_confirm);
      if(s.action_hold.done)
      {
        s.restart = 2;
        break;
      }
    }

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
        show_toast(T("Every setting reset"), 1, UI_COL_PINK);
        s.action_hold.p = 0;
        s.dirty = 1;
      }
    }

  draw:
    if(s.top_f < UI_F_SCROLL)
    {
      s.top_f++;
      s.dirty = 1;
    }
#ifdef UI_PERF_SETTINGS
    perf_t1 = sceKernelGetSystemTimeLow();
#endif
    if(s.dirty)
      build_ground();
#ifdef UI_PERF_SETTINGS
    perf_t2 = sceKernelGetSystemTimeLow();
#endif

    ui_frame_begin();
    ui_ground();
    if(s.tab_f < UI_F_TAB) s.tab_f++;
    {
      const char *tl[TABS];
      int i;
      for(i = 0; i < TABS; i++)
        tl[i] = T(tab_labels[i]);
      ui_tabs(UI_MARGIN, tl, TABS, s.tab, s.tab_prev, s.tab_f);
    }
    ui_status_chip();
    if(s.value_f < UI_F_VALUE) s.value_f++;
    if(s.cursor_f < UI_F_CURSOR) s.cursor_f++;
    if((s.tab == PAD_TAB) && (s.page == 0))
      draw_chip(s.btn, 1);
    else if(row_selectable(&rows[s.tab][s.cur]) && (s.dir < 0))
      draw_selection();
    draw_tip_line();
    if(s.pick && !draw_picker())
      s.dirty = 1;
    if(s.help)
    {
      const char *t, *b;
      if(help_topic(&t, &b))
      {
        if(!strcmp(t, "Smoothing"))
          draw_smooth_help(s.help_f, s.help_out);
        else
          ui_help_card(T(t), T(b), UI_COL_LIME, s.help_f, s.help_out);
      }
      if(s.help_f < (s.help_out ? UI_F_HELP_OUT : UI_F_HELP_IN))
        s.help_f++;
      else if(s.help_out)
        s.help = 0;
    }
    if(s.toast_f >= 0)
    {
      if(!ui_toast(s.toast, s.toast_edge, s.toast_undo ? T("Undo") : NULL, s.toast_f))
        s.toast_f = -1;
      else
        s.toast_f++;
    }
#ifdef UI_PERF_SETTINGS
    {
      u32 t3 = sceKernelGetSystemTimeLow();
      perf_step(perf_t1 - perf_t0, perf_t2 - perf_t1, t3 - perf_t2, perf_n ? perf_t0 - perf_last : 0);
      perf_last = perf_t0;
      if(perf_on && (perf_phase >= PERF_PHASES))
        perf_finish();
    }
#endif
    ui_frame_end();
  }

#ifdef UI_PERF_SETTINGS
  perf_finish();
#endif
  ui_pool_release(mark);
  return s.restart;
}

void ui_settings_where(int *tab, int *row)
{
  *tab = s.tab;
  *row = s.cur;
}

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
