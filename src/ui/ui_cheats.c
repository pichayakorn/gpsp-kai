/* ui_cheats.c - Cheats (spec section 4, "Cheats 4.0: groups and rules"; #166)
 *
 * The pause screen's Cheats tile, over the cheat engine (cheat_engine.c).
 * A game's cheats come in groups (a file each in its cheat folder): the
 * list of groups opens first, with On now above them (every cheat that is
 * on, from every group); a game with one group opens straight on it.
 *
 * A group's page: × switches a cheat, △ turns every unlocked one off (with
 * Undo), SELECT locks a cheat on (All off skips it; it turns off only after
 * unlocking; a ROM fix or master code locks itself when switched on, and
 * kai never switches a cheat on by itself). A cheat the engine can't run is
 * greyed, and the tip line says why. A group's rule changes its rows:
 * one at a time has round marks (choosing one switches the one before
 * off, with Undo), runs once has Apply (it runs when the game goes on,
 * then switches itself off), needs a restart offers to restart the game.
 * □ explains a row.
 */

#include "../common.h"
#include "ui.h"

#define LIST_T     36          /* the list area: under the title ... */
#define LIST_B     226         /* ... to the tip line */
#define ROW_Y0     (LIST_T + 2)
#define PITCH      UI_ROW_PITCH
#define ROWS_SEEN  ((LIST_B - ROW_Y0) / PITCH)   /* whole rows only: 7 */
#define TOAST_Y    214
#define ON_NOW     (-1)        /* the page of every cheat that is on */
#define GROUPS     (-2)        /* the list of groups */

static const u16 icon_lock[11] =
 { 0x000, 0x0F8, 0x104, 0x104, 0x3FE, 0x3FE, 0x3DE, 0x3DE, 0x3FE, 0x3FE, 0x000 };
static const u16 icon_cheats[11] =
 { 0x020, 0x020, 0x070, 0x7DF, 0x202, 0x104, 0x124, 0x252, 0x28A, 0x306, 0x000 };
static const u16 icon_folder[11] =
 { 0x000, 0x3C0, 0x27C, 0x204, 0x3FE, 0x202, 0x202, 0x202, 0x3FE, 0x000, 0x000 };
static const u16 icon_star[11] =
 { 0x020, 0x020, 0x070, 0x070, 0x7FF, 0x3FE, 0x1FC, 0x1FC, 0x3DE, 0x306, 0x202 };

/* what a row is */
enum { R_ON_NOW, R_GROUP, R_CANT, R_SWITCH, R_RADIO, R_ONCE };

/* what Undo puts back */
enum { UNDO_NONE, UNDO_ALL_OFF, UNDO_CHOICE };

static struct
{
  int page;                    /* GROUPS, ON_NOW or a group */
  int n;                       /* rows */
  int single;                  /* one group: no list of groups */
  int list_cur;                /* the list's cursor while a page is open */
  int cur, from, cur_f;        /* the cursor and its flight */
  int top, top_from, top_f;    /* the first row in view; the slide from top_from px */
  int value_f, value_row;      /* the switch or mark that moved */
  int lock_f, lock_row;        /* the padlock that popped */
  int rest;                    /* frames the cursor has rested (the tip) */
  int toast_f;                 /* -1: none */
  char toast[80];
  int undo, undo_cheat;        /* UNDO_*; the cheat a choice switched off */
  int help, help_f, help_out;  /* the □ help card */
  int restart;                 /* Restart now was chosen */
  int dirty;
  int view_b;                  /* the bottom of the last whole row in view */
} c;

static u16 rows[CE_MAX_CHEATS + 1];   /* a page's cheats, or the list's groups */
static u8 undo_on[CE_MAX_CHEATS];     /* the cheats All off turned off */

static void draw_icon11(int x, int y, const u16 *bits, u16 color, int scale)
{
  int r, k;

  for(r = 0; r < 11; r++)
    for(k = 0; k < 11; k++)
      if(bits[r] & (1 << (10 - k)))
        ui_fill(x + k * scale, y + r * scale, scale, scale, color);
}

/* ---- rows ------------------------------------------------------------------------- */

static int on_list(void)
{
  return c.page == GROUPS;
}

/* the cheat a page row shows */
static int cheat_at(int r)
{
  return rows[r];
}

static int rule_of(int i)
{
  return cheat_group_rule(cheat_group_of(i));
}

static int row_kind(int r)
{
  int i;

  if(on_list())
    return (r == 0) ? R_ON_NOW : R_GROUP;
  i = cheat_at(r);
  if(cheat_status(i) != CE_OK)
    return R_CANT;
  switch(rule_of(i))
  {
    case CE_RULE_ONE:  return R_RADIO;
    case CE_RULE_ONCE: return R_ONCE;
  }
  return R_SWITCH;
}

/* a ROM fix or a master code, by its name */
static int fix_or_master(int i)
{
  char low[CE_NAME_LENGTH];
  const char *name = cheat_name(i);
  int k;

  for(k = 0; name[k] && (k < (int)sizeof(low) - 1); k++)
    low[k] = tolower((unsigned char)name[k]);
  low[k] = 0;
  return strstr(low, "fix") || strstr(low, "master");
}

static int count_on_all(void)
{
  int i, n = 0;
  for(i = 0; i < (int)cheat_count(); i++)
    n += cheat_is_on(i);
  return n;
}

static int count_cant_all(void)
{
  int i, n = 0;
  for(i = 0; i < (int)cheat_count(); i++)
    n += cheat_status(i) != CE_OK;
  return n;
}

static void group_counts(int g, int *on, int *cant)
{
  int i, first = cheat_group_first(g), end = first + cheat_group_size(g);

  *on = *cant = 0;
  for(i = first; i < end; i++)
  {
    *on += cheat_is_on(i);
    *cant += cheat_status(i) != CE_OK;
  }
}

/* SELECT locks a switch; on On now a chosen mark too. Runs-once cheats
   and the marks of a one-at-a-time page are not locked. */
static int lockable(int r)
{
  int k = row_kind(r);
  return (k == R_SWITCH) || ((k == R_RADIO) && (c.page == ON_NOW));
}

/* does △ All off apply on this page? */
static int all_off_here(void)
{
  int i, r;

  if(on_list())
  {
    for(i = 0; i < (int)cheat_count(); i++)
      if(cheat_is_on(i) && !cheat_is_locked(i))
        return 1;
    return 0;
  }
  if(c.page != ON_NOW)
  {
    int rule = cheat_group_rule(c.page);
    if((rule == CE_RULE_ONE) || (rule == CE_RULE_ONCE))
      return 0;
  }
  for(r = 0; r < c.n; r++)
    if(cheat_is_on(cheat_at(r)) && !cheat_is_locked(cheat_at(r)))
      return 1;
  return 0;
}

static int top_px(void)
{
  return ui_ease_out_back(c.top_from, c.top * PITCH, c.top_f, UI_F_SCROLL);
}

/* ---- drawing ------------------------------------------------------------------------- */

/* a chip of the header at x; returns the next x, or 0 when it doesn't fit */
static int chip(int x, const char *s, u16 fill)
{
  int w = ui_print_width(s, UI_FONT_SMALL, 0) + 12;

  if(!x || (x + w > 388))
    return 0;
  ui_round_fill(x, 12, w, 16, 2, fill);
  ui_print(x + 6, 13, s, UI_COL_PLUM, UI_FONT_SMALL, 0);
  return x + w + 6;
}

static void draw_header(void)
{
  const char *title = (on_list() || !cheat_count()) ? "Cheats" :
   (c.page == ON_NOW) ? "On now" : cheat_group_name(c.page);
  char s1[32], s2[32];
  int x, w = ui_print_width(title, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);

  if(w > 300)
    w = 300;
  ui_print_fit(UI_MARGIN, 2, title, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X, 300);
  x = UI_MARGIN + w + 12;
  if(cheat_count() == 0)
    return;
  if(on_list())
  {
    snprintf(s1, sizeof(s1), "%d cheats", (int)cheat_count());
    x = chip(x, s1, UI_COL_LIME);
    if(count_cant_all())
    {
      snprintf(s2, sizeof(s2), "%d can't run", count_cant_all());
      chip(x, s2, UI_COL_MUTE);
    }
  }
  else if(c.page == ON_NOW)
  {
    snprintf(s1, sizeof(s1), "%d on", count_on_all());
    chip(x, s1, UI_COL_LIME);
  }
  else
  {
    int on, cant, rule = cheat_group_rule(c.page);
    group_counts(c.page, &on, &cant);
    snprintf(s1, sizeof(s1), "%d of %d on", on, c.n);
    if(rule != CE_RULE_ONCE)
      x = chip(x, s1, UI_COL_LIME);
    switch(rule)
    {
      case CE_RULE_ONE:     x = chip(x, "One at a time", UI_COL_SKY); break;
      case CE_RULE_ONCE:    x = chip(x, "Runs once", UI_COL_SKY); break;
      case CE_RULE_RESTART: x = chip(x, "Needs a restart", UI_COL_TANGERINE); break;
    }
    if(cant)
    {
      snprintf(s2, sizeof(s2), "%d can't run", cant);
      chip(x, s2, UI_COL_MUTE);
    }
  }
}

/* text right-aligned at xr in the small font; returns its left edge */
static int print_r(int xr, int y, const char *s, u16 color)
{
  int w = ui_print_width(s, UI_FONT_SMALL, 0);
  ui_print(xr - w, y, s, color, UI_FONT_SMALL, 0);
  return xr - w;
}

/* a pill whose right edge is xr; returns its left edge */
static int pill_r(int xr, int y, const char *s, u16 fill, u16 ink)
{
  int w = ui_print_width(s, UI_FONT_SMALL, 0) + 12;
  ui_round_fill(xr - w, y, w, 14, 2, fill);
  ui_print(xr - w + 6, y, s, ink, UI_FONT_SMALL, 0);
  return xr - w;
}

/* one row at y; sel: the lifted look */
static void draw_row(int r, int y, int sel)
{
  static const char *const rule_words[] = { "", "one at a time", "runs once", "needs a restart" };
  int kind = row_kind(r), lift = sel ? UI_LIFT : 0, x = UI_MARGIN - lift, yy = y - lift;
  int lx = x + 8, xr = x + 440 - 8, i = on_list() ? -1 : cheat_at(r);
  int cant = kind == R_CANT;
  u16 ink = sel ? UI_COL_PLUM : (cant ? UI_COL_MUTE : UI_COL_CREAM);
  u16 note = sel ? UI_COL_PLUM : UI_COL_MUTE;
  char meta[64];
  const char *label;

  if(sel)
    ui_round_fill(UI_MARGIN + 1, y + 1, 440, UI_ROW_H, 2, UI_COL_SHADE);
  ui_round_fill(x, yy, 440, UI_ROW_H, 2, sel ? (cant ? UI_COL_MUTE : UI_COL_LIME) :
   (cant ? UI_COL_PLUM_2 : UI_COL_CARD));
  meta[0] = 0;

  if(on_list())
  {
    if(kind == R_ON_NOW)
    {
      draw_icon11(lx, yy + 4, icon_star, sel ? UI_COL_PLUM : UI_COL_LIME, 1);
      label = "On now";
      snprintf(meta, sizeof(meta), "%d on", count_on_all());
    }
    else
    {
      int g = rows[r], on, n;
      draw_icon11(lx, yy + 4, icon_folder, sel ? UI_COL_PLUM : UI_COL_TANGERINE, 1);
      label = cheat_group_name(g);
      group_counts(g, &on, &n);
      snprintf(meta, sizeof(meta), "%d of %d on%s%s", on, (int)cheat_group_size(g),
       cheat_group_rule(g) ? " \xC2\xB7 " : "", rule_words[cheat_group_rule(g) & 3]);
    }
    lx += 22;
  }
  else
  {
    label = cheat_name(i);
    if(c.page == ON_NOW)
      snprintf(meta, sizeof(meta), "%s", cheat_group_name(cheat_group_of(i)));
    switch(kind)
    {
      case R_SWITCH:
      {
        int p = (r == c.value_row) ? ui_switch_pos(cheat_is_on(i), c.value_f) :
         (cheat_is_on(i) ? 256 : 0);
        ui_row_switch(xr, yy, p, sel);
        xr -= 36;
        break;
      }
      case R_RADIO:
      {
        /* a round mark: the rows read as a choice */
        int cx = xr - 7, cy = yy + 10, on = cheat_is_on(i);
        ui_ring(cx, cy, 7, 2, sel ? UI_COL_PLUM : (on ? UI_COL_LIME : UI_COL_MUTE));
        if(on && !((r == c.value_row) && (c.value_f < 3)))
          ui_disc(cx, cy, 3, sel ? UI_COL_PLUM : UI_COL_LIME);
        xr -= 22;
        break;
      }
      case R_ONCE:
        if(cheat_is_on(i))
          xr = pill_r(xr, yy + 3, "Ready", sel ? UI_COL_PLUM : UI_COL_LIME,
           sel ? UI_COL_LIME : UI_COL_PLUM) - 8;
        else
          xr = print_r(xr, yy + 3, "\xE2\x96\xB6 Apply", sel ? UI_COL_PLUM : UI_COL_LIME) - 8;
        break;
      default:
        xr = print_r(xr, yy + 3, "\xE2\x80\x94", note) - 10;
        break;
    }
    if(cheat_is_locked(i) && !((r == c.lock_row) && (c.lock_f < 3)))  /* it pops in */
    {
      draw_icon11(xr - 14, yy + 4, icon_lock, sel ? UI_COL_PLUM : UI_COL_TANGERINE, 1);
      xr -= 20;
    }
    if(cheat_waits_restart(i))
      xr = pill_r(xr, yy + 3, "next start", sel ? UI_COL_PLUM : UI_COL_SKY,
       sel ? UI_COL_SKY : UI_COL_PLUM) - 8;
  }
  if(meta[0])
  {
    /* the note keeps at most half the row; the name runs to 6 px before it */
    int w = ui_print_width(meta, UI_FONT_SMALL, 0), max = (xr - lx) / 2;
    if(w > max)
    {
      ui_print_fit(xr - max, yy + 3, meta, note, UI_FONT_SMALL, 0, max);
      xr -= max;
    }
    else
      xr = print_r(xr, yy + 3, meta, note);
  }
  ui_print_fit(lx, yy + 2, label[0] ? label : "\xE2\x80\x94", ink, UI_FONT_BODY, 0, xr - 6 - lx);
}

static void draw_hints(void)
{
  ui_hint_t h[6];
  int n = 0, kind;

  h[n].keys = ui_key_back; h[n++].verb = "Back";
  h[n].keys = PSP_CTRL_SQUARE; h[n++].verb = "Help";
  if(c.n == 0)
  {
    ui_hint_packed(h, n, 14);
    return;
  }
  kind = row_kind(c.cur);
  if(!on_list() && (c.n > ROWS_SEEN))
  {
    h[n].keys = PSP_CTRL_LEFT | PSP_CTRL_RIGHT; h[n++].verb = "Page";
  }
  /* while a toast offers Undo, SELECT is Undo */
  if(c.undo != UNDO_NONE)
  {
    h[n].keys = PSP_CTRL_SELECT; h[n++].verb = "Undo";
  }
  else if(!on_list() && lockable(c.cur))
  {
    h[n].keys = PSP_CTRL_SELECT;
    h[n++].verb = cheat_is_locked(cheat_at(c.cur)) ? "Unlock" : "Lock";
  }
  if(all_off_here())
  {
    h[n].keys = PSP_CTRL_TRIANGLE; h[n++].verb = "All off";
  }
  h[n].keys = ui_key_confirm;
  switch(kind)
  {
    case R_ON_NOW:
    case R_GROUP:  h[n++].verb = "Open"; break;
    case R_SWITCH:
      if(!cheat_is_locked(cheat_at(c.cur)))  /* a locked cheat ignores confirm */
        h[n++].verb = "Switch";
      break;
    case R_RADIO:
      if(!cheat_is_locked(cheat_at(c.cur)))
        h[n++].verb = cheat_is_on(cheat_at(c.cur)) ? "Switch" : "Choose";
      break;
    case R_ONCE:   h[n++].verb = cheat_is_on(cheat_at(c.cur)) ? "Clear" : "Apply"; break;
  }
  ui_hint_packed(h, n, 14);
}

static void draw_empty(void)
{
  static const char t[] = "No cheats for this game";
  char b[MAX_PATH + 32], folder[MAX_PATH];
  int w;

  cheat_engine_folder(folder, sizeof(folder));
  snprintf(b, sizeof(b), "Put .cht files in %s", folder);
  ui_round_fill(208, 82, 64, 52, 3, UI_COL_CARD);
  draw_icon11(224, 92, icon_cheats, UI_COL_LIME, 3);
  ui_print(240 - ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD) / 2, 146, t, UI_COL_CREAM,
   UI_FONT_BODY, UI_TEXT_BOLD);
  w = ui_print_width(b, UI_FONT_SMALL, 0);
  if(w > 440)
    w = 440;
  ui_print_fit(240 - w / 2, 166, b, UI_COL_MUTE, UI_FONT_SMALL, 0, 440);
}

static void build_ground(void)
{
  int r, top = top_px();

  c.dirty = 0;
  if(!ui_layer_begin())
    return;
  draw_header();
  if(c.n == 0)
    draw_empty();
  else
  {
    /* every row unselected; whole rows only down to the tip line */
    int vb = LIST_T;
    for(r = 0; r < c.n; r++)
    {
      int y = ROW_Y0 + r * PITCH - top;
      if((y >= LIST_T - UI_ROW_H) && (y + UI_ROW_H + 1 <= LIST_B) && (y + UI_ROW_H + 1 > vb))
        vb = y + UI_ROW_H + 1;
    }
    c.view_b = vb;
    ui_clip_set(0, LIST_T, UI_SCREEN_W, c.view_b - LIST_T);
    for(r = 0; r < c.n; r++)
    {
      int y = ROW_Y0 + r * PITCH - top;
      if((y + UI_ROW_H < LIST_T) || (y > LIST_B))
        continue;
      draw_row(r, y, 0);
    }
    ui_clip_reset();
    if(c.n > ROWS_SEEN)
    {
      int track = LIST_B - LIST_T - 8, th = track * ROWS_SEEN / c.n, ty;
      if(th < 8)
        th = 8;
      ty = (track - th) * top / ((c.n - ROWS_SEEN) * PITCH);
      ui_round_fill(468, LIST_T + 4, 4, track, 1, UI_COL_CARD);
      ui_round_fill(468, LIST_T + 4 + ty, 4, th, 1, UI_COL_MUTE);
    }
  }
  draw_hints();
  ui_layer_end();
}

/* the tip line for the row under the cursor, or NULL */
static const char *tip_text(void)
{
  int i;

  if(c.n == 0)
    return NULL;
  if(on_list())
    return "Each file in the game's cheat folder is a group.";
  i = cheat_at(c.cur);
  if(cheat_status(i) != CE_OK)
    return cheat_why(i);
  if(cheat_is_locked(i))
    return "Locked cheats stay on: All off skips them. SELECT unlocks.";
  switch(rule_of(i))
  {
    case CE_RULE_ONE:     return "Switching one on switches the others off.";
    case CE_RULE_ONCE:    return "Runs once when you go back to the game, then switches itself off.";
    case CE_RULE_RESTART: return "Works from the game's start: it needs a restart.";
  }
  return NULL;
}

/* the help card's words for the row under the cursor */
static void help_text(const char **t, const char **b)
{
  int rule = CE_RULE_NONE;

  if((c.n > 0) && !on_list())
  {
    int i = cheat_at(c.cur);
    if(cheat_status(i) != CE_OK)
    {
      *t = "Can't run";
      *b = "kai checks every code when the file loads. A greyed cheat can't be switched on; "
       "the tip line says why.";
      return;
    }
    rule = rule_of(i);
  }
  switch(rule)
  {
    case CE_RULE_ONE:
      *t = "One at a time";
      *b = "These cheats replace each other: switching one on switches the one before off.";
      break;
    case CE_RULE_ONCE:
      *t = "Runs once";
      *b = "\xC3\x97 gets it ready; it runs once when you go back to the game, then switches "
       "itself off.";
      break;
    case CE_RULE_RESTART:
      *t = "Needs a restart";
      *b = "This cheat changes the game as it starts, so the game has to start again for it "
       "to work.";
      break;
    default:
      *t = "Cheat groups";
      *b = "Each .cht file in the game's cheat folder is a group. On now lists every cheat that "
       "is on, from all groups.";
      break;
  }
}

/* the selected row over the ground, the cursor frame, the tip */
static void draw_selection(void)
{
  int top = top_px(), y0, y1;
  const char *tip;

  if(c.n == 0)
    return;
  ui_clip_set(0, LIST_T - 4, UI_SCREEN_W, c.view_b + 3 - (LIST_T - 4));
  y1 = ROW_Y0 + c.cur * PITCH - top;
  draw_row(c.cur, y1, 1);
  y0 = ROW_Y0 + c.from * PITCH - top;
  ui_cursor_frame(UI_MARGIN - UI_LIFT, ui_ease_out_back(y0, y1, c.cur_f, UI_F_CURSOR) - UI_LIFT,
   440, UI_ROW_H);
  ui_clip_reset();
  tip = tip_text();
  if(tip && (c.rest > UI_F_TIP_DELAY))
  {
    int k = c.rest - UI_F_TIP_DELAY;
    ui_tip_line(tip, (k > 8) ? 8 : k);
  }
}

/* the page as it is, for a question card to come down over */
static void draw_page(void)
{
  if(c.dirty)
    build_ground();
  ui_frame_begin();
  ui_ground();
  ui_status_chip();
  draw_selection();
  ui_frame_end();
  ui_frame_begin();  /* both buffers hold it */
  ui_ground();
  ui_status_chip();
  draw_selection();
  ui_frame_end();
}

/* ---- the restart question ----------------------------------------------------------- */

/* A dialog card with two buttons over the screen as it is: ←→ moves, ×
 * takes the selected button, ○ goes back. ok_verb: the hint for ×, or NULL
 * for the selected button's own name. Returns 0 or 1, or -1 for back. */
static int ask(const char *title, const char *body, const char *b0, const char *b1, u16 edge,
 const char *back_verb, const char *ok_verb)
{
  const char *buttons[2];
  u32 mark = ui_pool_mark();
  u16 *under = ui_pool_alloc(UI_SCREEN_W * UI_SCREEN_H * 2);
  int f = 0, closing = 0, result = -1, sel = 0, y;
  ui_hint_t h[3];

  buttons[0] = b0;
  buttons[1] = b1;
  draw_page();
  if(under)
  {
    u16 *src = (u16 *)((u32)screen_address | 0x40000000);
    for(y = 0; y < UI_SCREEN_H; y++)
      memcpy(under + y * UI_SCREEN_W, src + y * screen_pitch, UI_SCREEN_W * 2);
  }
  ui_input_init();
  while(1)
  {
    ui_input_update();
    if(!closing)
    {
      if(ui_pressed(PSP_CTRL_LEFT) || ui_pressed(PSP_CTRL_RIGHT))
        sel ^= 1;
      if(ui_pressed(ui_key_confirm))
      {
        result = sel;
        closing = 1;
        f = 0;
      }
      else if(ui_pressed(ui_key_back))
      {
        closing = 1;
        f = 0;
      }
    }
    ui_frame_begin();
    if(under)
      ui_blit(under, UI_SCREEN_W, 0, 0, UI_SCREEN_W, UI_SCREEN_H);
    if(!(closing && (f >= UI_F_DIALOG_OUT)))
    {
      ui_dialog(title, body, buttons, 2, sel, edge, f, closing);
      h[0].keys = ui_key_back;                    h[0].verb = (char *)back_verb;
      h[1].keys = PSP_CTRL_LEFT | PSP_CTRL_RIGHT; h[1].verb = "Move";
      h[2].keys = ui_key_confirm;
      h[2].verb = (char *)(ok_verb ? ok_verb : buttons[sel]);
      ui_hint_packed(h, closing ? 0 : 3, 14);
    }
    ui_frame_end();
    if(f < (closing ? UI_F_DIALOG_OUT : UI_F_DIALOG_IN))
      f++;
    else if(closing)
      break;
    if(!under && !closing && (f >= UI_F_DIALOG_IN))
      f = UI_F_DIALOG_IN;
  }
  ui_pool_release(mark);
  ui_input_init();
  c.dirty = 1;
  return result;
}

/* A cheat that works from the game's start was switched on. */
static void ask_restart(void)
{
  if(ask("Restart the game?", "This cheat works from the game's start. kai saves to Last, then "
   "starts the game again.", "Restart now", "Later", UI_COL_TANGERINE, "Later", NULL) == 0)
    c.restart = 1;
}

/* ---- acting ------------------------------------------------------------------------- */

static void select_row(int i)
{
  if(i < 0)
    i = 0;
  if(i >= c.n)
    i = c.n - 1;
  if((c.n == 0) || (i == c.cur))
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
  c.dirty = 1;  /* the hints follow the row */
}

static void show_toast(const char *text, int undo)
{
  snprintf(c.toast, sizeof(c.toast), "%s", text);
  c.undo = undo;
  c.toast_f = 0;
  c.dirty = 1;
}

/* a page: GROUPS, ON_NOW or a group, its cursor on row cur */
static void open_page(int page, int cur)
{
  int i;

  c.page = page;
  c.n = 0;
  if(page == GROUPS)
  {
    rows[c.n++] = 0;  /* On now */
    for(i = 0; i < (int)cheat_group_count(); i++)
      rows[c.n++] = i;
  }
  else if(page == ON_NOW)
  {
    /* what is on now: the list stays as it is while the page is open, so a
       cheat switched off here can be switched back on */
    for(i = 0; i < (int)cheat_count(); i++)
      if(cheat_is_on(i))
        rows[c.n++] = i;
  }
  else
    for(i = 0; i < (int)cheat_group_size(page); i++)
      rows[c.n++] = cheat_group_first(page) + i;
  c.cur = c.from = (cur < c.n) ? cur : 0;
  c.cur_f = UI_F_CURSOR;
  c.top = (c.cur >= ROWS_SEEN) ? c.cur - ROWS_SEEN + 1 : 0;
  c.top_from = c.top * PITCH;
  c.top_f = UI_F_SCROLL;
  c.value_row = c.lock_row = -1;
  c.rest = 0;
  c.toast_f = -1;
  c.undo = UNDO_NONE;
  c.dirty = 1;
}

/* × on a group row: its page */
static void open_group(int g)
{
  c.list_cur = c.cur;
  open_page(g, 0);
}

static void after_on(int i, int r)
{
  /* a ROM fix or master code locks itself as it is switched on: All off
     then leaves it; kai never switches a code on by itself (maintainer,
     2026-11-20) */
  if(cheat_is_on(i) && fix_or_master(i) && (rule_of(i) != CE_RULE_ONCE))
  {
    cheat_set_locked(i, 1);
    c.lock_row = r;
    c.lock_f = 0;
  }
  if(cheat_waits_restart(i))
    ask_restart();
}

static void press_confirm(void)
{
  int i, r = c.cur, k = row_kind(r);

  switch(k)
  {
    case R_ON_NOW:
      c.list_cur = c.cur;
      open_page(ON_NOW, 0);
      return;
    case R_GROUP:
      open_group(rows[r]);
      return;
    case R_CANT:
      return;
  }
  i = cheat_at(r);
  if(cheat_is_locked(i))
    return;
  if((k == R_RADIO) && !cheat_is_on(i))
  {
    /* choosing: the one before goes off, with Undo */
    int g = cheat_group_of(i), j, before = -1;
    int end = cheat_group_first(g) + cheat_group_size(g);
    for(j = cheat_group_first(g); j < end; j++)
      if((j != i) && cheat_is_on(j) && !cheat_is_locked(j))
        before = j;
    cheat_set_on(i, 1);
    if(before >= 0)
    {
      char t[80];
      snprintf(t, sizeof(t), "%s switched off", cheat_name(before));
      c.undo_cheat = before;
      show_toast(t, UNDO_CHOICE);
    }
  }
  else
    cheat_set_on(i, !cheat_is_on(i));
  c.value_row = r;
  c.value_f = 0;
  c.dirty = 1;
  if(cheat_is_on(i))
    after_on(i, r);
}

static void press_select(void)
{
  int i, r = c.cur;

  if(c.undo == UNDO_ALL_OFF)
  {
    for(i = 0; i < (int)cheat_count(); i++)
      if(undo_on[i])
        cheat_set_on(i, 1);
  }
  else if(c.undo == UNDO_CHOICE)
    cheat_set_on(c.undo_cheat, 1);  /* the one chosen after it goes off */
  else if(!on_list() && lockable(r))
  {
    /* SELECT locks a cheat on; unlocking leaves it as it is */
    int was_on;
    i = cheat_at(r);
    was_on = cheat_is_on(i);
    cheat_set_locked(i, !cheat_is_locked(i));
    if(cheat_is_on(i) && !was_on)
    {
      c.value_row = r;
      c.value_f = 0;
    }
    c.lock_row = r;
    c.lock_f = 0;
    c.rest = 0;
    c.dirty = 1;
    if(cheat_is_on(i) && !was_on && cheat_waits_restart(i))
      ask_restart();
    return;
  }
  else
    return;
  c.undo = UNDO_NONE;
  c.toast_f = -1;
  c.dirty = 1;
}

static void press_all_off(void)
{
  int i, r;

  memset(undo_on, 0, sizeof(undo_on));
  if(on_list())
  {
    for(i = 0; i < (int)cheat_count(); i++)
      undo_on[i] = cheat_is_on(i) && !cheat_is_locked(i);
  }
  else
    for(r = 0; r < c.n; r++)
    {
      i = cheat_at(r);
      undo_on[i] = cheat_is_on(i) && !cheat_is_locked(i);
    }
  for(i = 0; i < (int)cheat_count(); i++)
    if(undo_on[i])
      cheat_set_on(i, 0);
  show_toast("All cheats off", UNDO_ALL_OFF);
}

/* Returns 1 when the player chose Restart now (the pause screen restarts
   the game), 0 when they went back. */
int ui_cheats(void)
{
  u32 mark = ui_pool_mark();

  memset(&c, 0, sizeof(c));
  c.value_f = UI_F_VALUE;
  c.lock_f = UI_F_VALUE;
  ui_status_read();
  ui_input_init();
  c.single = cheat_group_count() <= 1;
  if(!c.single)
    open_page(GROUPS, 1);  /* on the first group, On now above it */
  else
    open_page(0, 0);  /* no cheats: no rows, and where the files go */

  while(!c.restart)
  {
    ui_input_update();
    if(c.help)
    {
      /* the help card takes every key; □ or ○ closes it */
      if(!c.help_out && (ui_pressed(PSP_CTRL_SQUARE) || ui_pressed(ui_key_back)))
      {
        c.help_out = 1;
        c.help_f = 0;
      }
    }
    else if(ui_pressed(ui_key_back))
    {
      if(on_list() || c.single)
        break;
      open_page(GROUPS, c.list_cur);
    }
    else if(ui_pressed(PSP_CTRL_SQUARE))
    {
      c.help = 1;
      c.help_out = 0;
      c.help_f = 0;
    }
    else if(c.n > 0)
    {
      select_row(ui_step(c.cur, c.n, PSP_CTRL_UP, PSP_CTRL_DOWN));
      if(!on_list() && (c.n > ROWS_SEEN))
      {
        /* ←→ a page: the rows in view */
        if(ui_repeat(PSP_CTRL_LEFT))
          select_row(c.cur - ROWS_SEEN);
        else if(ui_repeat(PSP_CTRL_RIGHT))
          select_row(c.cur + ROWS_SEEN);
      }
      if(ui_pressed(PSP_CTRL_SELECT))
        press_select();
      else if(ui_pressed(PSP_CTRL_TRIANGLE) && all_off_here())
        press_all_off();
      else if(ui_pressed(ui_key_confirm))
        press_confirm();
    }
    if(c.restart)
      break;

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
      if(!ui_toast_at(c.toast, UI_COL_LIME, c.undo ? "Undo" : NULL, c.toast_f, TOAST_Y))
      {
        c.toast_f = -1;
        c.undo = UNDO_NONE;
        c.dirty = 1;  /* SELECT goes back to Lock */
      }
      else
        c.toast_f++;
    }
    if(c.help)
    {
      const char *t, *b;
      help_text(&t, &b);
      ui_help_card(t, b, UI_COL_LIME, c.help_f, c.help_out);
      if(c.help_f < (c.help_out ? UI_F_HELP_OUT : UI_F_HELP_IN))
        c.help_f++;
      else if(c.help_out)
        c.help = 0;
    }
    ui_frame_end();
  }

  cheat_engine_save();  /* which cheats are on and locked */
  ui_layer_reset();
  ui_pool_release(mark);
  return c.restart;
}
