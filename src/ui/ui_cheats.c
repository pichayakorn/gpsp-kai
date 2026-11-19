/* ui_cheats.c - Cheats (spec section 4, Cheats; step 6, #45)
 *
 * The pause screen's Cheats tile: the game's cheats from its .cht file as
 * one scrolling list of switches. × switches a cheat, △ turns every
 * unlocked one off (with Undo), SELECT locks a cheat on: All off skips it
 * and it turns off only after unlocking (ROM fixes, master codes; those
 * lock themselves when switched on). Locks are kept per game in its .cfg. With no .cht file the page says where to
 * put one.
 */

#include "../common.h"
#include "ui.h"

#define LIST_T     36          /* the list area: under the title ... */
#define LIST_B     226         /* ... to the tip line */
#define ROW_Y0     (LIST_T + 2)
#define PITCH      UI_ROW_PITCH
#define ROWS_SEEN  ((LIST_B - ROW_Y0) / PITCH)   /* whole rows only: 7 */
#define TOAST_Y    214

static const u16 icon_lock[11] =
 { 0x000, 0x0F8, 0x104, 0x104, 0x3FE, 0x3FE, 0x3DE, 0x3DE, 0x3FE, 0x3FE, 0x000 };
static const u16 icon_cheats[11] =
 { 0x020, 0x020, 0x070, 0x7DF, 0x202, 0x104, 0x124, 0x252, 0x28A, 0x306, 0x000 };

static struct
{
  int n;                       /* cheats */
  int cur, from, cur_f;        /* the cursor and its flight */
  int top, top_from, top_f;    /* the first row in view; the slide from top_from px */
  int value_f, value_row;      /* the switch that moved */
  int lock_f, lock_row;        /* the padlock that popped */
  int rest;                    /* frames the cursor has rested (the tip) */
  int toast_f;                 /* -1: none */
  int dirty;
  int view_b;                  /* the bottom of the last whole row in view */
} c;

static u8 undo_on[CE_MAX_CHEATS];  /* the cheats All off turned off */

static void draw_icon11(int x, int y, const u16 *bits, u16 color, int scale)
{
  int r, k;

  for(r = 0; r < 11; r++)
    for(k = 0; k < 11; k++)
      if(bits[r] & (1 << (10 - k)))
        ui_fill(x + k * scale, y + r * scale, scale, scale, color);
}

static int locked(int i)
{
  return cheat_is_locked(i);
}

/* a ROM fix or a master code, by its name */
static int fix_or_master(int i)
{
  char low[CHEAT_NAME_LENGTH];
  int k;

  const char *name = cheat_name(i);

  for(k = 0; name[k] && (k < (int)sizeof(low) - 1); k++)
    low[k] = tolower((unsigned char)name[k]);
  low[k] = 0;
  return strstr(low, "fix") || strstr(low, "master");
}

static int is_on(int i)
{
  return cheat_is_on(i);
}

/* the engine checked it: it can be switched on */
static int runs(int i)
{
  return cheat_status(i) == CE_OK;
}

static int count_on(void)
{
  int i, n = 0;
  for(i = 0; i < c.n; i++)
    n += is_on(i);
  return n;
}

static int count_locked(void)
{
  int i, n = 0;
  for(i = 0; i < c.n; i++)
    n += locked(i);
  return n;
}

/* is there an unlocked cheat on (△ All off applies)? */
static int any_off_able(void)
{
  int i;
  for(i = 0; i < c.n; i++)
    if(is_on(i) && !locked(i))
      return 1;
  return 0;
}

static int top_px(void)
{
  return ui_ease_out_back(c.top_from, c.top * PITCH, c.top_f, UI_F_SCROLL);
}

/* ---- drawing ------------------------------------------------------------------------- */

static void draw_header(void)
{
  char s1[32], s2[32];
  int x;

  ui_print_fit(UI_MARGIN, 2, "Cheats", UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X, 180);
  if(c.n == 0)
    return;
  x = UI_MARGIN + ui_print_width("Cheats", UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X) + 12;
  snprintf(s1, sizeof(s1), "%d of %d on", count_on(), c.n);
  ui_round_fill(x, 12, ui_print_width(s1, UI_FONT_SMALL, 0) + 12, 16, 2, UI_COL_LIME);
  ui_print(x + 6, 13, s1, UI_COL_PLUM, UI_FONT_SMALL, 0);
  x += ui_print_width(s1, UI_FONT_SMALL, 0) + 18;
  if(count_locked())
  {
    snprintf(s2, sizeof(s2), "%d locked", count_locked());
    ui_round_fill(x, 12, ui_print_width(s2, UI_FONT_SMALL, 0) + 26, 16, 2, UI_COL_TANGERINE);
    draw_icon11(x + 4, 14, icon_lock, UI_COL_PLUM, 1);
    ui_print(x + 18, 13, s2, UI_COL_PLUM, UI_FONT_SMALL, 0);
  }
}

/* one cheat row at y; sel: the lime, lifted look */
static void draw_row(int i, int y, int sel)
{
  int lift = sel ? UI_LIFT : 0, x = UI_MARGIN - lift, yy = y - lift, lx, maxw;
  const char *name = cheat_name(i);
  char num[16];
  int p;

  if(!runs(i))
  {
    /* can't run: a plum-2 card, mute text and a dash for the switch
       (selected: a mute card with plum text); the tip says why */
    if(sel)
      ui_round_fill(UI_MARGIN + 1, y + 1, 440, UI_ROW_H, 2, UI_COL_SHADE);
    ui_round_fill(x, yy, 440, UI_ROW_H, 2, sel ? UI_COL_MUTE : UI_COL_PLUM_2);
    snprintf(num, sizeof(num), "%2d", i + 1);
    ui_print(x + 8, yy + 2, num, sel ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_BODY, 0);
    ui_print_fit(x + 8 + 22, yy + 2, name, sel ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_BODY, 0,
     394 - lift - (x + 8 + 22));
    ui_print(x + 440 - 8 - ui_print_width("\xE2\x80\x94", UI_FONT_BODY, 0), yy + 2,
     "\xE2\x80\x94", sel ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_BODY, 0);
    return;
  }
  if(sel)
    ui_round_fill(UI_MARGIN + 1, y + 1, 440, UI_ROW_H, 2, UI_COL_SHADE);
  ui_round_fill(x, yy, 440, UI_ROW_H, 2, sel ? UI_COL_LIME : UI_COL_CARD);
  snprintf(num, sizeof(num), "%2d", i + 1);
  ui_print(x + 8, yy + 2, num, sel ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_BODY, 0);
  lx = x + 8 + 22;
  maxw = 394 - lift - lx;  /* the name runs to 6 px before the padlock */
  if(name[0])
    ui_print_fit(lx, yy + 2, name, sel ? UI_COL_PLUM : UI_COL_CREAM, UI_FONT_BODY, 0, maxw);
  else
    ui_print(lx, yy + 2, "\xE2\x80\x94", UI_COL_MUTE, UI_FONT_BODY, 0);
  p = (i == c.value_row) ? ui_switch_pos(is_on(i), c.value_f) : (is_on(i) ? 256 : 0);
  ui_row_switch(x + 440 - 8, yy, p, sel);
  if(locked(i) && !((i == c.lock_row) && (c.lock_f < 3)))  /* it pops in */
    draw_icon11(400 - lift, y + 4 - lift, icon_lock, sel ? UI_COL_PLUM : UI_COL_TANGERINE, 1);
}

static void draw_hints(void)
{
  ui_hint_t h[4];
  int n = 0;

  h[n].keys = ui_key_back; h[n++].verb = "Back";
  if(c.n == 0)
  {
    ui_hint_packed(h, n, 14);
    return;
  }
  /* while the All off toast offers Undo, SELECT is Undo; a cheat that
     can't run takes neither SELECT nor × */
  if((c.toast_f >= 0) || runs(c.cur))
  {
    h[n].keys = PSP_CTRL_SELECT;
    h[n++].verb = (c.toast_f >= 0) ? "Undo" : locked(c.cur) ? "Unlock" : "Lock";
  }
  if(any_off_able())
  {
    h[n].keys = PSP_CTRL_TRIANGLE; h[n++].verb = "All off";
  }
  if(!locked(c.cur) && runs(c.cur))  /* a locked cheat ignores confirm */
  {
    h[n].keys = ui_key_confirm; h[n++].verb = "Switch";
  }
  ui_hint_packed(h, n, 14);
}

static void draw_empty(void)
{
  static const char t[] = "No cheats for this game";
  char b[MAX_PATH + MAX_FILE + 16], name[MAX_FILE];

  change_ext(gamepak_filename, name, ".cht");
  snprintf(b, sizeof(b), "Put %s in %s/", name, g_default_cheat_dir);
  ui_round_fill(208, 82, 64, 52, 3, UI_COL_CARD);
  draw_icon11(224, 92, icon_cheats, UI_COL_LIME, 3);
  ui_print(240 - ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD) / 2, 146, t, UI_COL_CREAM,
   UI_FONT_BODY, UI_TEXT_BOLD);
  {
    int w = ui_print_width(b, UI_FONT_SMALL, 0);
    if(w > 440)
      w = 440;
    ui_print_fit(240 - w / 2, 166, b, UI_COL_MUTE, UI_FONT_SMALL, 0, 440);
  }
}

static void build_ground(void)
{
  int i, top = top_px();

  c.dirty = 0;
  if(!ui_layer_begin())
    return;
  draw_header();
  if(c.n == 0)
    draw_empty();
  else
  {
    /* every row unselected; whole rows only down to the tip line: the
       list is cut under the last row that fits */
    {
      int vb = LIST_T;
      for(i = 0; i < c.n; i++)
      {
        int y = ROW_Y0 + i * PITCH - top;
        if((y >= LIST_T - UI_ROW_H) && (y + UI_ROW_H + 1 <= LIST_B) && (y + UI_ROW_H + 1 > vb))
          vb = y + UI_ROW_H + 1;
      }
      c.view_b = vb;
    }
    ui_clip_set(0, LIST_T, UI_SCREEN_W, c.view_b - LIST_T);
    for(i = 0; i < c.n; i++)
    {
      int y = ROW_Y0 + i * PITCH - top;
      if((y + UI_ROW_H < LIST_T) || (y > LIST_B))
        continue;
      draw_row(i, y, 0);
    }
    ui_clip_reset();
    if(c.n > ROWS_SEEN)
    {
      int track = LIST_B - LIST_T - 8, th = track * ROWS_SEEN / c.n;
      int ty = (track - th) * top / ((c.n - ROWS_SEEN) * PITCH);
      if(th < 16)
        th = 16;
      ui_round_fill(468, LIST_T + 4, 4, track, 1, UI_COL_CARD);
      ui_round_fill(468, LIST_T + 4 + ty, 4, th, 1, UI_COL_MUTE);
    }
  }
  draw_hints();
  ui_layer_end();
}

/* the selected row over the ground, the cursor frame, the tip */
static void draw_selection(void)
{
  int top = top_px(), y0, y1;

  if(c.n == 0)
    return;
  ui_clip_set(0, LIST_T - 4, UI_SCREEN_W, c.view_b + 3 - (LIST_T - 4));
  y1 = ROW_Y0 + c.cur * PITCH - top;
  draw_row(c.cur, y1, 1);
  y0 = ROW_Y0 + c.from * PITCH - top;
  ui_cursor_frame(UI_MARGIN - UI_LIFT, ui_ease_out_back(y0, y1, c.cur_f, UI_F_CURSOR) - UI_LIFT,
   440, UI_ROW_H);
  ui_clip_reset();
  /* the tip line: why a cheat can't run, or what a lock does */
  if(c.rest > UI_F_TIP_DELAY)
  {
    int k = c.rest - UI_F_TIP_DELAY;
    if(!runs(c.cur))
      ui_tip_line(cheat_why(c.cur), (k > 8) ? 8 : k);
    else if(locked(c.cur))
      ui_tip_line("Locked cheats stay on: All off skips them. SELECT unlocks.", (k > 8) ? 8 : k);
  }
}

/* ---- acting ------------------------------------------------------------------------- */

static void select_row(int i)
{
  if((i < 0) || (i >= c.n) || (i == c.cur))
    return;
  c.from = c.cur;
  c.cur = i;
  c.cur_f = 0;
  c.rest = 0;
  if((i < c.top) || (i >= c.top + ROWS_SEEN))
  {
    c.top_from = top_px();  /* a new slide starts from where it is now */
    c.top = (i < c.top) ? i : i - ROWS_SEEN + 1;
    c.top_f = 0;
  }
  c.dirty = 1;  /* Lock or Unlock, Switch or not */
}

static void show_toast(void)
{
  c.toast_f = 0;
  c.dirty = 1;
}

/* Returns when the player goes back. */
void ui_cheats(void)
{
  u32 mark = ui_pool_mark();

  memset(&c, 0, sizeof(c));
  c.n = cheat_count();
  c.cur_f = UI_F_CURSOR;
  c.top_f = UI_F_SCROLL;
  c.value_row = c.lock_row = -1;
  c.value_f = UI_F_VALUE;
  c.lock_f = UI_F_VALUE;
  c.toast_f = -1;
  c.dirty = 1;
  ui_status_read();
  ui_input_init();

  while(1)
  {
    ui_input_update();
    if(ui_pressed(ui_key_back))
      break;
    if(c.n > 0)
    {
      select_row(ui_step(c.cur, c.n, PSP_CTRL_UP, PSP_CTRL_DOWN));
      if(ui_pressed(PSP_CTRL_SELECT))
      {
        if(c.toast_f >= 0)
        {
          /* Undo All off */
          int i;
          for(i = 0; i < c.n; i++)
            if(undo_on[i])
              cheat_set_on(i, 1);
          c.toast_f = -1;
        }
        else if(runs(c.cur))
        {
          /* SELECT locks a cheat on; unlocking leaves it as it is */
          int was_on = is_on(c.cur);
          cheat_set_locked(c.cur, !locked(c.cur));
          if(is_on(c.cur) && !was_on)
          {
            c.value_row = c.cur;
            c.value_f = 0;
          }
          c.lock_row = c.cur;
          c.lock_f = 0;
          c.rest = 0;
        }
        c.dirty = 1;
      }
      else if(ui_pressed(PSP_CTRL_TRIANGLE) && any_off_able())
      {
        int i;
        for(i = 0; i < c.n; i++)
        {
          undo_on[i] = is_on(i) && !locked(i);
          if(undo_on[i])
            cheat_set_on(i, 0);
        }
        show_toast();
      }
      else if(ui_pressed(ui_key_confirm) && !locked(c.cur) && runs(c.cur))
      {
        cheat_set_on(c.cur, !is_on(c.cur));
        /* a ROM fix or master code locks itself as it is switched on: All
           off then leaves it; kai never switches a code on by itself
           (maintainer, 2026-11-20) */
        if(is_on(c.cur) && fix_or_master(c.cur))
        {
          cheat_set_locked(c.cur, 1);
          c.lock_row = c.cur;
          c.lock_f = 0;
        }
        c.value_row = c.cur;
        c.value_f = 0;
        c.dirty = 1;
      }
    }

    if(c.cur_f < UI_F_CURSOR) c.cur_f++;
    if(c.top_f < UI_F_SCROLL) { c.top_f++; c.dirty = 1; }
    if(c.value_f < UI_F_VALUE) { c.value_f++; c.dirty = 1; }
    if(c.lock_f < UI_F_VALUE) { c.lock_f++; c.dirty = 1; }
    if(c.rest < UI_F_TIP_DELAY + 8) c.rest++;
    if(c.dirty)
      build_ground();

    ui_frame_begin();
    ui_ground();
    ui_status_chip();
    draw_selection();
    if(c.toast_f >= 0)
    {
      if(!ui_toast_at("All cheats off", UI_COL_LIME, "Undo", c.toast_f, TOAST_Y))
      {
        c.toast_f = -1;
        c.dirty = 1;  /* SELECT goes back to Lock */
      }
      else
        c.toast_f++;
    }
    ui_frame_end();
  }

  cheat_engine_save();  /* which cheats are on and locked */
  ui_layer_reset();
  ui_pool_release(mark);
}
