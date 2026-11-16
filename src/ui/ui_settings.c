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
    { "Smoothing", "Sharp keeps pixels crisp at any size; Smooth blurs them; Off shows them as they fall.",
      K_SEG, 0, 3, { "Off", "Smooth", "Sharp" }, &g_gpsp_config.screen_filter, NULL, filter_sharp_bilinear },
    { "Side bars", NULL, K_OFF, 0, 0, { "Comes later" } },
    { "Frameskip", "Auto skips drawing only when the game can't keep up.",
      K_SEG, 1, 3, { "Off", "Auto", "Fixed" }, &g_game_config.frameskip_type, map_frameskip, auto_frameskip },
    { "Skip up to", "The most frames in a row that may go undrawn.",
      K_CYCLE, 1, 0, { NULL }, &g_game_config.frameskip_value, NULL, 9, 0, 99, "%d frames" },
    { "Picture scale", "Shrinks or grows the picture around its centre.",
      K_CYCLE, 0, 0, { NULL }, &g_gpsp_config.screen_scale, NULL, 100, 50, 200, "%d%%" },
    { "TV output", NULL, K_OFF, 0, 0, { "Comes later" } },
  },
  { /* Sound */
    { "Sound", NULL, K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.enable_audio, NULL, YES },
    { "Sound delay", "Less delay feels faster; more delay stops crackles in busy games.",
      K_BLOCKS, 1, 11, { NULL }, &g_game_config.audio_buffer_size_number, NULL, 2 },
  },
  { /* Controls */
    { "Buttons and stick", NULL, K_OFF, 0, 0, { "In the next update" } },
  },
  { /* Play */
    { "Save when leaving a game", "Exit and Library save to Last first; Continue picks up there.",
      K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.save_on_leave, NULL, YES },
    { "Fast forward", "Hold: fast while the button is down. Toggle: one press on, one press off.",
      K_SEG, 0, 2, { "Hold", "Toggle" }, &g_gpsp_config.fast_forward_mode, map_ff, FF_TOGGLE },
    { "Sound while fast", NULL, K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.fast_forward_sound, NULL, YES },
    { "Rapid-fire speed", NULL, K_SEG, 1, 3, { "Slow", "Medium", "Fast" },
      &g_game_config.rapidfire_speed, NULL, RAPIDFIRE_FAST },
    { "Show FPS", NULL, K_SWITCH, 0, 2, { NULL }, &g_gpsp_config.show_fps, NULL, NO },
  },
  { /* System */
    { "Language", "The menu language; it applies the next time kai starts.",
      K_CYCLE, 0, 12, { NULL }, &g_gpsp_config.language, NULL, 1, 0, 0, NULL, language_names },
    { "CPU speed", "Faster runs heavy games smoothly; slower saves battery.",
      K_SEG, 1, 3, { "222", "266", "333 MHz" }, &g_game_config.clock_speed_number, map_clock, 9 },
    { "Write game saves", "Right away writes the game's own save as soon as it changes.",
      K_SEG, 1, 2, { "On exit", "Right away" }, &g_game_config.update_backup_flag, NULL, YES },
    { "Start games with", NULL, K_SEG, 0, 2, { "Game Cart", "GBA BIOS" }, &g_gpsp_config.boot_mode, NULL, GAME_CART },
    { "Confirm button", NULL, K_OFF, 0, 0, { "Comes later" } },
    { "Restart kai", "Saves like Exit, starts kai again and brings you back here.",
      K_ACTION, 0, 0, { NULL }, NULL, NULL, ACT_RESTART },
    { "Reset all settings (hold)", "Every setting on every tab goes back to its default.",
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

static int row_pitch(const row_t *r)
{
  return row_height(r) + (UI_ROW_PITCH - UI_ROW_H);
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
  int top;                        /* scroll: content y at VIEW_T */
  int value_f, value_from;        /* the selected row's value motion */
  int dirty;                      /* the ground needs drawing again */
  ui_hold_t reset_hold;           /* START: reset this tab */
  ui_hold_t action_hold;          /* confirm on Reset all */
  u32 undo[TABS][MAX_ROWS];       /* the values before a reset */
  int undo_all;                   /* the undo covers every tab */
  char toast[48];
  int toast_f, toast_undo;
  int restart;                    /* leave Settings and restart kai */
} s;

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

/* Restart kai is a tap, unless play could be lost: Save when leaving a game
 * is off (a game always runs under Settings). */
static int restart_is_hold(void)
{
  return !g_gpsp_config.save_on_leave;
}

static int action_of(int tab, int i)
{
  return (rows[tab][i].kind == K_ACTION) ? rows[tab][i].def : -1;
}

static int row_y(int tab, int i)
{
  int y = ROWS_Y, j;

  for(j = 0; j < i; j++)
    y += row_pitch(&rows[tab][j]);
  return y;
}

static int rows_bottom(int tab)
{
  return row_y(tab, row_count(tab));
}

/* Scroll by whole rows, so the first row in view always sits at ROWS_Y and
 * no row is ever cut at the top. Keep the row after the cursor in view when
 * there is one, and the row before it when moving up. */
static void scroll_to_cursor(void)
{
  int n = row_count(s.tab), first = 0, top;

  if(rows_bottom(s.tab) > VIEW_B)
  {
    int want = (s.cur + 1 < n) ? s.cur + 1 : s.cur;

    while((first < n - 1) && (row_y(s.tab, first + 1) - ROWS_Y <= s.top))
      first++;  /* the first row in view now */
    if((s.cur <= first) && (first > 0))
      first = (s.cur > 0) ? s.cur - 1 : 0;
    while((first < s.cur) && (row_y(s.tab, want) + row_height(&rows[s.tab][want])
                              - (row_y(s.tab, first) - ROWS_Y) > VIEW_B))
      first++;
  }
  top = row_y(s.tab, first) - ROWS_Y;
  if(top != s.top)
  {
    s.top = top;
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

  if(r->kind == K_OFF)
  {
    ui_card_row(ROW_X, y, ROW_W, r->label, 0, g, UI_ROW_DISABLED, &vx, &vy);
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
      ui_round_fill(ROW_X + 1, y + 1, ROW_W, SCREEN_ROW_H, 2, UI_COL_SHADE);
    ui_round_fill(ROW_X - lift, y - lift, ROW_W, SCREEN_ROW_H, 2, selected ? g : UI_COL_CARD);
    if(flags & UI_ROW_CHANGED)
      ui_fill(ROW_X - 9, y + 18, 4, 4, UI_COL_TANGERINE);
    ui_print(ROW_X + 8 - lift, y + 12 - lift, r->label, selected ? UI_COL_PLUM : UI_COL_CREAM,
     UI_FONT_BODY, 0);
    draw_screen_choices(r, ROW_X + ROW_W - 8 - lift, y - lift, selected, g);
    return;
  }

  ui_card_row(ROW_X, y, ROW_W, r->label, selected, g, flags, &vx, &vy);
  if(is_lang_row(tab, i) && lang_pending())
  {
    /* "next start": a sky pill 6 px after the label, 3 px down */
    int lift = selected ? UI_LIFT : 0;
    int px = ROW_X + 8 - lift + ui_print_width(r->label, UI_FONT_BODY, 0) + 6;
    int pw = ui_print_width("next start", UI_FONT_SMALL, 0) + 10;
    ui_round_fill(px, y + 3 - lift, pw, 14, 2, UI_COL_SKY);
    ui_print(px + 5, y + 3 - lift + 1, "next start", UI_COL_PLUM, UI_FONT_SMALL, 0);
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
      ui_row_blocks(vx, vy, r->count, k, msg[MSG_SB_2048 + k], selected ? f : UI_F_VALUE, selected, g);
      break;
    case K_CYCLE:
      if(r->names)
        snprintf(text, sizeof(text), "%s", r->names[(k >= 0) && (k < r->count) ? k : 0]);
      else
        snprintf(text, sizeof(text), r->fmt, (int)*r->value);
      ui_row_cycle(vx, vy, text, selected, g, selected ? f : UI_F_VALUE);
      break;
    case K_ACTION:
      if(selected && (s.action_hold.p > 0))
        ui_hold_bar(vx - 120, vy + 7, 112, s.action_hold.p, UI_COL_PINK);
      else
        ui_row_action(vx, vy, selected);
      break;
  }
}

static void draw_hints(void)
{
  ui_hint_t hints[5];
  int n = 0;

  switch(action_of(s.tab, s.cur))
  {
    case ACT_RESET_ALL:
      hints[n].keys = ui_key_confirm; hints[n++].verb = "Hold: reset all";
      break;
    case ACT_RESTART:
      hints[n].keys = ui_key_confirm;
      hints[n++].verb = restart_is_hold() ? "Hold: restart" : "Restart";
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
        hints[n].keys = ui_key_confirm; hints[n++].verb = "Restart now";
      }
      break;
  }
  hints[n].keys = PSP_CTRL_START;                 hints[n++].verb = "Hold: reset tab";
  if(n < 4)
  {
    hints[n].keys = PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER; hints[n++].verb = "Tabs";
  }
  hints[n].keys = ui_key_back;                    hints[n++].verb = "Back";
  ui_hint_strip(hints, n);
}

/* The ground: the check, the legend, every row but the selected one, the
 * scroll bar and the hint strip. */
static void build_ground(void)
{
  int i, n = row_count(s.tab), per_game = 0, bottom = rows_bottom(s.tab);

  s.dirty = 0;
  if(!ui_layer_begin())
    return;
  for(i = 0; i < n; i++)
    per_game |= rows[s.tab][i].per_game;
  if(per_game)
  {
    ui_per_game_mark(UI_MARGIN, 40, UI_COL_TANGERINE);
    ui_text(UI_MARGIN + 16, 39, "Saved for this game only", UI_COL_MUTE);
  }
  for(i = 0; i < n; i++)
  {
    int y = row_y(s.tab, i) - s.top, h = row_height(&rows[s.tab][i]);
    if((i == s.cur) || (y < VIEW_T) || (y + h > VIEW_B))
      continue;
    draw_row(s.tab, i, y, 0, UI_F_VALUE);
  }
  if(bottom > VIEW_B)
  {
    int track = VIEW_B - VIEW_T - 4, th = track * (VIEW_B - VIEW_T) / (bottom - VIEW_T);
    int ty = VIEW_T + 2 + (track - th) * ((s.top < bottom - VIEW_B) ? s.top : bottom - VIEW_B) / (bottom - VIEW_B);
    ui_round_fill(468, VIEW_T + 2, 4, track, 1, UI_COL_CARD);
    ui_round_fill(468, ty, 4, th, 1, UI_COL_MUTE);
  }
  draw_hints();
  ui_layer_end();
}

/* ---- actions ---------------------------------------------------------------- */

static void remember_tab(int tab)
{
  int i;

  for(i = 0; i < row_count(tab); i++)
    if(rows[tab][i].value)
      s.undo[tab][i] = *rows[tab][i].value;
}

static void reset_tab(int tab)
{
  int i;

  for(i = 0; i < row_count(tab); i++)
    if(rows[tab][i].value)
      *rows[tab][i].value = rows[tab][i].def;
}

static void show_toast(const char *text, int undo)
{
  snprintf(s.toast, sizeof(s.toast), "%s", text);
  s.toast_undo = undo;
  s.toast_f = 0;
}

static void undo_reset(void)
{
  int t, i;

  for(t = 0; t < TABS; t++)
  {
    if(!s.undo_all && (t != s.tab))
      continue;
    for(i = 0; i < row_count(t); i++)
      if(rows[t][i].value)
        *rows[t][i].value = s.undo[t][i];
  }
  show_toast("Settings are back as they were", 0);
  s.dirty = 1;
}

static void move_cursor(int to)
{
  if(to == s.cur)
    return;
  s.cursor_from = ui_ease_out_back(s.cursor_from, row_y(s.tab, s.cur) - s.top, s.cursor_f, UI_F_CURSOR);
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
  s.top = 0;
  scroll_to_cursor();
  s.cursor_from = row_y(s.tab, s.cur) - s.top;
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
  }
  ui_input_init();
  ui_status_read();
  build_ground();

  while(1)
  {
    int moved = 0;

    ui_input_update();
    if(ui_pressed(ui_key_back))
      break;

    /* tabs */
    if(ui_pressed(PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER))
    {
      s.tab_prev = s.tab;
      s.tab = (s.tab + (ui_pressed(PSP_CTRL_RTRIGGER) ? 1 : TABS - 1)) % TABS;
      s.tab_f = 0;
      s.reset_hold.p = 0;
      first_row();
      s.dirty = 1;
      moved = 1;
    }

    /* rows */
    if(!moved && ui_repeat(PSP_CTRL_UP | PSP_CTRL_DOWN))
      move_cursor(next_row(ui_repeat(PSP_CTRL_DOWN) ? 1 : -1));
    if(row_selectable(&rows[s.tab][s.cur]))
    {
      int pending = lang_pending();
      if(ui_repeat(PSP_CTRL_RIGHT)) change_value(1);
      if(ui_repeat(PSP_CTRL_LEFT)) change_value(-1);
      if(lang_pending() != pending)
        s.dirty = 1;  /* the hint strip gains or loses "Restart now" */
      if((rows[s.tab][s.cur].kind == K_SWITCH) && ui_pressed(ui_key_confirm))
        change_value(1);  /* a switch also flips on confirm, as in Cheats */
    }

    /* SELECT: Undo while the toast offers it */
    if(ui_released(PSP_CTRL_SELECT) && ui_toast_up(s.toast_f) && s.toast_undo)
    {
      undo_reset();
      s.toast_undo = 0;
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
      show_toast(text, 1);
      s.reset_hold.p = 0;
      s.reset_hold.shown = 0;
      s.dirty = 1;
    }

    /* Language: × restarts now when the pick differs from the running one */
    if(is_lang_row(s.tab, s.cur) && lang_pending() && ui_pressed(ui_key_confirm))
    {
      s.restart = 1;
      break;
    }

    /* Restart kai: a tap, or a hold when play could be lost */
    if(action_of(s.tab, s.cur) == ACT_RESTART)
    {
      if(!restart_is_hold())
      {
        if(ui_pressed(ui_key_confirm))
        {
          s.restart = 1;
          break;
        }
      }
      else
      {
        ui_hold_step(&s.action_hold, ui_key_confirm);
        if(s.action_hold.done)
        {
          s.restart = 1;
          break;
        }
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
        show_toast("Every setting reset", 1);
        s.action_hold.p = 0;
        s.dirty = 1;
      }
    }

    if(s.dirty)
      build_ground();

    /* draw */
    ui_frame_begin();
    ui_ground();
    if(s.tab_f < UI_F_TAB) s.tab_f++;
    ui_tabs(UI_MARGIN, tab_labels, TABS, s.tab, s.tab_prev, s.tab_f);
    ui_status_chip();
    if(s.reset_hold.p > 0)  /* the reset bar, under the tabs */
    {
      ui_fill(UI_MARGIN, 32, ROW_W, 2, UI_COL_CARD);
      ui_fill(UI_MARGIN, 32, ROW_W * s.reset_hold.p / 256, 2, UI_COL_TANGERINE);
    }
    if(s.value_f < UI_F_VALUE) s.value_f++;
    if(s.cursor_f < UI_F_CURSOR) s.cursor_f++;
    {
      int y = row_y(s.tab, s.cur) - s.top, h = row_height(&rows[s.tab][s.cur]);
      int cy = ui_ease_out_back(s.cursor_from, y, s.cursor_f, UI_F_CURSOR);
      if(row_selectable(&rows[s.tab][s.cur]))
      {
        draw_row(s.tab, s.cur, y, 1, s.value_f);
        ui_cursor_frame(ROW_X - UI_LIFT, cy - UI_LIFT, ROW_W, h);
      }
    }
    /* the tip line waits for the cursor to rest */
    if(s.rest < UI_F_TIP_DELAY)
      s.rest++;
    else
    {
      const char *tip = rows[s.tab][s.cur].tip;
      if(is_lang_row(s.tab, s.cur) && lang_pending())
        tip = "Applies after a restart: \xC3\x97 restarts now, or it waits for the next start.";
      if(tip)
        ui_print_fit(UI_MARGIN, 231, tip, UI_COL_MUTE, UI_FONT_SMALL, 0, ROW_W);
    }
    if(s.toast_f >= 0)
    {
      if(!ui_toast(s.toast, UI_COL_LIME, s.toast_undo ? "Undo" : NULL, s.toast_f))
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
