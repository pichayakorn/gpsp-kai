/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Hachi
 */

#include "../common.h"
#include "ui.h"

#define LIST_T     36
#define LIST_B     226
#define ROW_Y0     (LIST_T + 2)
#define PITCH      UI_ROW_PITCH
#define ROWS_SEEN  ((LIST_B - ROW_Y0) / PITCH)
#define TOAST_Y    214
#define ON_NOW     (-1)
#define GROUPS     (-2)
#define FOLD       0xFFFF
#define ON_ROW     0xFFFE
#define MARKS      11
#define RAIL_X     464
#define RAIL_Y     38
#define MARK_PITCH 17
#define JUMP_F     24

static const u16 icon_lock[11] =
 { 0x000, 0x0F8, 0x104, 0x104, 0x3FE, 0x3FE, 0x3DE, 0x3DE, 0x3FE, 0x3FE, 0x000 };
static const u16 icon_cheats[11] =
 { 0x020, 0x020, 0x070, 0x7DF, 0x202, 0x104, 0x124, 0x252, 0x28A, 0x306, 0x000 };
static const u16 icon_folder[11] =
 { 0x000, 0x3C0, 0x27C, 0x204, 0x3FE, 0x202, 0x202, 0x202, 0x3FE, 0x000, 0x000 };
static const u16 icon_star[11] =
 { 0x020, 0x020, 0x070, 0x070, 0x7FF, 0x3FE, 0x1FC, 0x1FC, 0x3DE, 0x306, 0x202 };

enum { R_ON_NOW, R_GROUP, R_CANT, R_SWITCH, R_RADIO, R_ONCE, R_FOLD };

enum { UNDO_NONE, UNDO_ALL_OFF, UNDO_CHOICE };

static struct
{
  int page;
  int n;
  int single;
  int list_cur;
  int cur, from, cur_f;
  int top, top_from, top_f;
  int value_f, value_row;
  int lock_f, lock_row;
  int rest;
  int toast_f;
  char toast[128];
  int undo, undo_cheat;
  int help, help_f, help_out;
  int restart;
  int dirty;
  int view_b;
  int fold_open;
  int n_cant;
  int marks, mark_row[MARKS];
  char mark_text[MARKS][12];
  int dots;
  int jump_f, jump_mark;
  int marq_f;
  int label_x, label_w;
} c;

static u16 rows[CE_MAX_CHEATS + 1];
static u8 undo_on[CE_MAX_CHEATS];

static void draw_icon11(int x, int y, const u16 *bits, u16 color, int scale)
{
  int r, k;

  for(r = 0; r < 11; r++)
    for(k = 0; k < 11; k++)
      if(bits[r] & (1 << (10 - k)))
        ui_fill(x + k * scale, y + r * scale, scale, scale, color);
}

static int on_list(void)
{
  return c.page == GROUPS;
}

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
  if(rows[r] == ON_ROW)
    return R_ON_NOW;
  if(rows[r] == FOLD)
    return R_FOLD;
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

static int is_ready(int i)
{
  return cheat_is_on(i) && (rule_of(i) == CE_RULE_ONCE);
}

static int count_on_all(void)
{
  int i, n = 0;
  for(i = 0; i < (int)cheat_count(); i++)
    n += cheat_is_on(i) && !is_ready(i);
  return n;
}

static int count_ready_all(void)
{
  int i, n = 0;
  for(i = 0; i < (int)cheat_count(); i++)
    n += is_ready(i);
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

static int lockable(int r)
{
  int k = row_kind(r);
  return (k == R_SWITCH) || ((k == R_RADIO) && (c.page == ON_NOW));
}

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
    if((rows[r] < ON_ROW) && cheat_is_on(cheat_at(r)) && !cheat_is_locked(cheat_at(r)))
      return 1;
  return 0;
}

static int top_px(void)
{
  return ui_ease_out_back(c.top_from, c.top * PITCH, c.top_f, UI_F_SCROLL);
}

static int chip(int x, const char *s, u16 fill)
{
  int w = ui_print_width(s, UI_FONT_SMALL, 0) + 12;

  if(!x || (x + w > 388))
    return 0;
  ui_round_fill(x, 12, w, 16, 2, fill);
  ui_print(x + 6, 13, s, UI_COL_PLUM, UI_FONT_SMALL, 0);
  return x + w + 6;
}

static int limit_chip(int x)
{
  char s[40];

  if(cheat_total() <= cheat_count())
    return x;
  snprintf(s, sizeof(s), T("first %d of %d"), (int)cheat_count(), (int)cheat_total());
  return chip(x, s, UI_COL_MUTE);
}

static void draw_header(void)
{
  const char *title = (on_list() || !cheat_count()) ? T("Cheats") :
   (c.page == ON_NOW) ? T("On now") : cheat_group_name(c.page);
  char s1[64], s2[64];
  int x, w = ui_print_width(title, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);

  if(w > 300)
    w = 300;
  ui_print_fit(UI_MARGIN, 2, title, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X, 300);
  x = UI_MARGIN + w + 12;
  if(cheat_count() == 0)
    return;
  if(on_list())
  {
    snprintf(s1, sizeof(s1), T("%d cheats"), (int)cheat_count());
    x = chip(x, s1, UI_COL_LIME);
    x = limit_chip(x);
    if(count_cant_all())
    {
      snprintf(s2, sizeof(s2), T("%d can't run"), count_cant_all());
      chip(x, s2, UI_COL_MUTE);
    }
  }
  else if(c.page == ON_NOW)
  {
    snprintf(s1, sizeof(s1), T("%d on"), count_on_all());
    x = chip(x, s1, UI_COL_LIME);
    if(count_ready_all())
    {
      snprintf(s2, sizeof(s2), T("%d ready"), count_ready_all());
      chip(x, s2, UI_COL_SKY);
    }
  }
  else
  {
    int on, cant, i, rule = cheat_group_rule(c.page);
    group_counts(c.page, &on, &cant);
    for(i = cheat_group_first(c.page); i < (int)(cheat_group_first(c.page) + cheat_group_size(c.page)); i++)
      on -= is_ready(i);
    snprintf(s1, sizeof(s1), T("%d of %d on"), on, (int)cheat_group_size(c.page));
    if(rule != CE_RULE_ONCE)
      x = chip(x, s1, UI_COL_LIME);
    if(c.single)
      x = limit_chip(x);
    switch(rule)
    {
      case CE_RULE_ONE:     x = chip(x, T("One at a time"), UI_COL_SKY); break;
      case CE_RULE_ONCE:    x = chip(x, T("Runs once"), UI_COL_SKY); break;
      case CE_RULE_RESTART: x = chip(x, T("Needs a restart"), UI_COL_TANGERINE); break;
    }
    if(cant)
    {
      snprintf(s2, sizeof(s2), T("%d can't run"), cant);
      chip(x, s2, UI_COL_MUTE);
    }
  }
}

static int print_r(int xr, int y, const char *s, u16 color)
{
  int w = ui_print_width(s, UI_FONT_SMALL, 0);
  ui_print(xr - w, y, s, color, UI_FONT_SMALL, 0);
  return xr - w;
}

static int pill_r(int xr, int y, const char *s, u16 fill, u16 ink)
{
  int w = ui_print_width(s, UI_FONT_SMALL, 0) + 12;
  ui_round_fill(xr - w, y, w, 14, 2, fill);
  ui_print(xr - w + 6, y, s, ink, UI_FONT_SMALL, 0);
  return xr - w;
}

static void draw_row(int r, int y, int sel)
{
  static const char *const rule_words[] = { "", TN("one at a time"), TN("runs once"), TN("needs a restart") };
  int kind = row_kind(r), lift = sel ? UI_LIFT : 0, x = UI_MARGIN - lift, yy = y - lift;
  int lx = x + 8, xr = x + 440 - 8;
  int i = (on_list() || (kind == R_FOLD) || (kind == R_ON_NOW)) ? -1 : cheat_at(r);
  int cant = (kind == R_CANT) || (kind == R_FOLD);
  char fold[64];
  u16 ink = sel ? UI_COL_PLUM : (cant ? UI_COL_MUTE : UI_COL_CREAM);
  u16 note = sel ? UI_COL_PLUM : UI_COL_MUTE;
  char meta[128];
  const char *label;

  if(sel)
    ui_round_fill(UI_MARGIN + 1, y + 1, 440, UI_ROW_H, 2, UI_COL_SHADE);
  ui_round_fill(x, yy, 440, UI_ROW_H, 2, sel ? (cant ? UI_COL_MUTE : UI_COL_LIME) :
   (cant ? UI_COL_PLUM_2 : UI_COL_CARD));
  meta[0] = 0;

  if(on_list() || (kind == R_ON_NOW))
  {
    if(kind == R_ON_NOW)
    {
      draw_icon11(lx, yy + 4, icon_star, sel ? UI_COL_PLUM : UI_COL_LIME, 1);
      label = T("On now");
      snprintf(meta, sizeof(meta), T("%d on"), count_on_all());
      if(count_ready_all())
      {
        char ready[64];
        snprintf(ready, sizeof(ready), T("%d ready"), count_ready_all());
        snprintf(meta + strlen(meta), sizeof(meta) - strlen(meta), " \xC2\xB7 %s", ready);
      }
    }
    else
    {
      int g = rows[r], on, n;
      draw_icon11(lx, yy + 4, icon_folder, sel ? UI_COL_PLUM : UI_COL_TANGERINE, 1);
      label = cheat_group_name(g);
      group_counts(g, &on, &n);
      snprintf(meta, sizeof(meta), T("%d of %d on"), on, (int)cheat_group_size(g));
      if(cheat_group_rule(g))
        snprintf(meta + strlen(meta), sizeof(meta) - strlen(meta), " \xC2\xB7 %s",
         T(rule_words[cheat_group_rule(g) & 3]));
    }
    lx += 22;
  }
  else if(kind == R_FOLD)
  {
    snprintf(fold, sizeof(fold), T("Can't run (%d)"), c.n_cant);
    label = fold;
    xr = print_r(xr, yy + 3, c.fold_open ? "\xE2\x96\xBE" : "\xE2\x96\xB8", note) - 10;
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
        int cx = xr - 7, cy = yy + 10, on = cheat_is_on(i);
        ui_ring(cx, cy, 7, 2, sel ? UI_COL_PLUM : (on ? UI_COL_LIME : UI_COL_MUTE));
        if(on && !((r == c.value_row) && (c.value_f < 3)))
          ui_disc(cx, cy, 3, sel ? UI_COL_PLUM : UI_COL_LIME);
        xr -= 22;
        break;
      }
      case R_ONCE:
        if(cheat_is_on(i))
          xr = pill_r(xr, yy + 3, T("Ready"), sel ? UI_COL_PLUM : UI_COL_LIME,
           sel ? UI_COL_LIME : UI_COL_PLUM) - 8;
        else
          xr = print_r(xr, yy + 3, T("\xE2\x96\xB6 Apply"), sel ? UI_COL_PLUM : UI_COL_LIME) - 8;
        break;
      default:
        xr = print_r(xr, yy + 3, "\xE2\x80\x94", note) - 10;
        break;
    }
    if(cheat_is_locked(i) && !((r == c.lock_row) && (c.lock_f < 3)))
    {
      draw_icon11(xr - 14, yy + 4, icon_lock, sel ? UI_COL_PLUM : UI_COL_TANGERINE, 1);
      xr -= 20;
    }
    if(cheat_waits_restart(i))
      xr = pill_r(xr, yy + 3, T("next start"), sel ? UI_COL_PLUM : UI_COL_SKY,
       sel ? UI_COL_SKY : UI_COL_PLUM) - 8;
  }
  if(meta[0])
  {
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
  if(sel)
  {
    c.label_x = lx;
    c.label_w = xr - 6 - lx;
  }
}

static void draw_hints(void)
{
  ui_hint_t h[6];
  int n = 0, kind;

  h[n].keys = ui_key_back; h[n++].verb = (char *)T("Back");
  h[n].keys = PSP_CTRL_SQUARE; h[n++].verb = (char *)T("Help");
  if(c.n == 0)
  {
    ui_hint_packed(h, n, 14);
    return;
  }
  kind = row_kind(c.cur);
  if(c.marks)
  {
    h[n].keys = PSP_CTRL_LEFT | PSP_CTRL_RIGHT; h[n++].verb = (char *)T("Jump");
  }
  if(c.undo != UNDO_NONE)
  {
    h[n].keys = PSP_CTRL_SELECT; h[n++].verb = (char *)T("Undo");
  }
  else if(!on_list() && lockable(c.cur))
  {
    h[n].keys = PSP_CTRL_SELECT;
    h[n++].verb = (char *)(cheat_is_locked(cheat_at(c.cur)) ? T("Unlock") : T("Lock"));
  }
  if(all_off_here())
  {
    h[n].keys = PSP_CTRL_TRIANGLE; h[n++].verb = (char *)T("All off");
  }
  h[n].keys = ui_key_confirm;
  switch(kind)
  {
    case R_ON_NOW:
    case R_GROUP:  h[n++].verb = (char *)T("Open"); break;
    case R_FOLD:   h[n++].verb = (char *)(c.fold_open ? T("Close") : T("Open")); break;
    case R_SWITCH:
      if(!cheat_is_locked(cheat_at(c.cur)))
        h[n++].verb = (char *)T("Switch");
      break;
    case R_RADIO:
      if(!cheat_is_locked(cheat_at(c.cur)))
        h[n++].verb = (char *)(cheat_is_on(cheat_at(c.cur)) ? T("Switch") : T("Choose"));
      break;
    case R_ONCE:   h[n++].verb = (char *)(cheat_is_on(cheat_at(c.cur)) ? T("Clear") : T("Apply")); break;
  }
  ui_hint_packed(h, n, 14);
}

static void draw_empty(void)
{
  const char *t = T("No cheats for this game");
  char b[MAX_PATH + 32], folder[MAX_PATH];
  int w;

  cheat_engine_folder(folder, sizeof(folder));
  snprintf(b, sizeof(b), T("Put .cht files in %s"), folder);
  ui_round_fill(208, 82, 64, 52, 3, UI_COL_CARD);
  draw_icon11(224, 92, icon_cheats, UI_COL_LIME, 3);
  ui_print(240 - ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD) / 2, 146, t, UI_COL_CREAM,
   UI_FONT_BODY, UI_TEXT_BOLD);
  w = ui_print_width(b, UI_FONT_SMALL, 0);
  if(w > 440)
    w = 440;
  ui_print_fit(240 - w / 2, 166, b, UI_COL_MUTE, UI_FONT_SMALL, 0, 440);
}

static int mark_at(int r)
{
  int j = 0;
  while((j + 1 < c.marks) && (c.mark_row[j + 1] <= r))
    j++;
  return j;
}

static void draw_rail(void)
{
  int j, cur = mark_at(c.cur);

  ui_fill(RAIL_X, RAIL_Y, 16, LIST_B - RAIL_Y, UI_COL_PLUM);
  for(j = 0; j < c.marks; j++)
  {
    int y = RAIL_Y + 2 + j * MARK_PITCH, w = ui_print_width(c.mark_text[j], UI_FONT_SMALL, 0);
    if(j == cur)
      ui_round_fill(RAIL_X, y + 1, 16, 14, 2, UI_COL_TANGERINE);
    ui_print(RAIL_X + 8 - w / 2, y + 1, c.mark_text[j], (j == cur) ? UI_COL_PLUM : UI_COL_MUTE,
     UI_FONT_SMALL, 0);
  }
}

static void draw_jump_tile(void)
{
  const char *t = c.mark_text[c.jump_mark];
  int w = ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);

  ui_round_fill(270, 98, 64, 64, 4, UI_COL_SHADE);
  ui_round_fill(267, 95, 64, 64, 4, UI_COL_PLUM);
  ui_print(299 - w / 2, 95 + 18, t, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);
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
    if(c.marks)
      draw_rail();
    else if(c.n > ROWS_SEEN)
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

static const char *tip_text(void)
{
  int i;

  if(c.n == 0)
    return NULL;
  if(on_list())
    return T("Each file in the game's cheat folder is a group.");
  if(rows[c.cur] == ON_ROW)
    return NULL;
  if(rows[c.cur] == FOLD)
    return T("Cheats kai can't run, folded away: \xC3\x97 shows them.");
  i = cheat_at(c.cur);
  if(cheat_status(i) != CE_OK)
    return T(cheat_why(i));
  if(cheat_is_locked(i))
    return T("Locked cheats stay on: All off skips them. SELECT unlocks.");
  switch(rule_of(i))
  {
    case CE_RULE_ONE:     return T("Switching one on switches the others off.");
    case CE_RULE_ONCE:    return T("Runs once when you go back to the game, then switches itself off.");
    case CE_RULE_RESTART: return T("Works from the game's start: it needs a restart.");
  }
  return NULL;
}

static void help_text(const char **t, const char **b)
{
  int rule = CE_RULE_NONE;

  if((c.n > 0) && !on_list() && (rows[c.cur] != ON_ROW))
  {
    int i = cheat_at(c.cur);
    if((rows[c.cur] == FOLD) || (cheat_status(i) != CE_OK))
    {
      *t = T("Can't run");
      *b = T("kai checks every code when the file loads. A greyed cheat can't be switched on; "
       "the tip line says why. Master codes (\"must be on\") are hidden: kai doesn't need them.");
      return;
    }
    rule = rule_of(i);
  }
  switch(rule)
  {
    case CE_RULE_ONE:
      *t = T("One at a time");
      *b = T("These cheats replace each other: switching one on switches the one before off.");
      break;
    case CE_RULE_ONCE:
      *t = T("Runs once");
      *b = T("\xC3\x97 gets it ready; it runs once when you go back to the game, then switches "
       "itself off.");
      break;
    case CE_RULE_RESTART:
      *t = T("Needs a restart");
      *b = T("This cheat changes the game as it starts, so the game has to start again for it "
       "to work.");
      break;
    default:
      *t = T("Cheat groups");
      *b = T("Each .cht file in the game's cheat folder is a group. On now lists every cheat that "
       "is on, from all groups.");
      break;
  }
}

static void draw_marquee(int yy)
{
  int kind = row_kind(c.cur), w, over, t, cycle, shift;
  const char *name;

  if(on_list() || (kind == R_FOLD) || (kind == R_ON_NOW) || (c.top_f < UI_F_SCROLL) || (yy < LIST_T - 4) ||
   (yy + UI_ROW_H > c.view_b + 3))
    return;
  name = cheat_name(cheat_at(c.cur));
  w = ui_print_width(name, UI_FONT_BODY, 0);
  over = w - c.label_w;
  t = c.marq_f - UI_MARQUEE_DELAY;
  if((over <= 0) || (t <= 0))
    return;
  cycle = over + 2 * UI_MARQUEE_DELAY;
  t %= cycle;
  shift = (t < over) ? t : (t < over + UI_MARQUEE_DELAY) ? over : 0;
  ui_fill(c.label_x, yy + 1, c.label_w + 6, UI_ROW_H - 2,
   (kind == R_CANT) ? UI_COL_MUTE : UI_COL_LIME);
  ui_clip_set(c.label_x, yy, c.label_w, UI_ROW_H);
  ui_print(c.label_x - shift, yy + 2, name, UI_COL_PLUM, UI_FONT_BODY, 0);
  ui_clip_reset();
}

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
  draw_marquee(y1 - UI_LIFT);
  if(c.jump_f >= 0)
    draw_jump_tile();
  tip = tip_text();
  if(tip && (c.rest > UI_F_TIP_DELAY))
  {
    int k = c.rest - UI_F_TIP_DELAY;
    ui_tip_line(tip, (k > 8) ? 8 : k);
  }
}

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
      h[1].keys = PSP_CTRL_LEFT | PSP_CTRL_RIGHT; h[1].verb = (char *)T("Move");
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

static void ask_restart(void)
{
  const char *body = g_gpsp_config.save_on_leave ?
   T("This cheat works from the game's start. kai saves to Last, then starts the game again.") :
   T("The game starts again from its beginning. Your save file is kept.");

  if(ask(T("Restart the game?"), body, T("Restart now"), T("Later"), UI_COL_TANGERINE, T("Later"), NULL) == 0)
    c.restart = 1;
}

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
  c.marq_f = 0;
  if((i < c.top) || (i >= c.top + ROWS_SEEN))
  {
    c.top_from = top_px();
    c.top = (i < c.top) ? i : i - ROWS_SEEN + 1;
    c.top_f = 0;
  }
  c.dirty = 1;
}

static void show_toast(const char *text, int undo)
{
  snprintf(c.toast, sizeof(c.toast), "%s", text);
  c.undo = undo;
  c.toast_f = 0;
  c.dirty = 1;
}

static void mark_text(const char *name, int skip, char *out, long long *num)
{
  int k = 0, w;

  *num = -1;
  name += skip;
  while(*name == ' ')
    name++;
  for(k = 0; (k < 11) && isdigit((unsigned char)name[k]); k++)
    ;
  if(k && (!name[k] || (name[k] == ' ')))
  {
    long long v = 0;
    int i;
    for(i = 0; i < k; i++)
      v = v * 10 + (name[i] - '0');
    *num = v;
    if(v < 1000)
    {
      while((*name == '0') && isdigit((unsigned char)name[1]) && (k > 3))
      {
        name++;
        k--;
      }
      memcpy(out, name, k);
      out[k] = 0;
    }
    else if(v < 100000)
      snprintf(out, 12, "%dk", (int)(v / 1000));
    else if(v < 1000000)
      snprintf(out, 12, ".%dM", (int)(v / 100000));
    else
      snprintf(out, 12, "%dM", (v / 1000000 > 99) ? 99 : (int)(v / 1000000));
    return;
  }
  k = 0;
  while(name[k] && (name[k] != ' ') && (k < 11))
  {
    int len = 1;
    while((((u8)name[k + len] & 0xC0) == 0x80) && (k + len < 11))
      len++;
    memcpy(out, name, k + len);
    out[k + len] = 0;
    w = ui_print_width(out, UI_FONT_SMALL, 0);
    if((w > 15) && k)
      break;
    k += len;
  }
  out[k] = 0;
  if(!k)
    snprintf(out, 12, "\xC2\xB7");
}

static void build_marks(void)
{
  static u16 at[CE_MAX_CHEATS + 1];
  long long num[MARKS];
  int r, n = 0, j, skip = -1;

  c.marks = 0;
  c.dots = 0;
  if(on_list() || (c.n <= ROWS_SEEN))
    return;
  for(r = 0; r < c.n; r++)
    if(rows[r] < ON_ROW)
      at[n++] = r;
  if(n < 2)
    return;
  for(r = 1; r < n; r++)
  {
    const char *a = cheat_name(cheat_at(at[0])), *b = cheat_name(cheat_at(at[r]));
    int k = 0;
    while(a[k] && (a[k] == b[k]) && ((skip < 0) || (k < skip)))
      k++;
    skip = k;
  }
  {
    const char *a = cheat_name(cheat_at(at[0]));
    while((skip > 0) && (a[skip - 1] != ' '))
      skip--;
  }
  c.marks = (n < MARKS) ? n : MARKS;
  for(j = 0; j < c.marks; j++)
  {
    c.mark_row[j] = at[j * (n - 1) / (c.marks - 1)];
    mark_text(cheat_name(cheat_at(c.mark_row[j])), skip, c.mark_text[j], &num[j]);
  }
  {
    int up = 1, down = 1, words = 1;
    for(j = 0; j < c.marks; j++)
      if((num[j] < 0) != (num[0] < 0))
        up = down = words = 0;
    for(j = 1; j < c.marks; j++)
    {
      if(num[0] >= 0)
      {
        up &= num[j] >= num[j - 1];
        down &= num[j] <= num[j - 1];
      }
      else
        words &= strcasecmp(c.mark_text[j - 1], c.mark_text[j]) <= 0;
    }
    c.dots = (num[0] >= 0) ? !(up || down) : !words;
    if(c.dots)
      for(j = 0; j < c.marks; j++)
        snprintf(c.mark_text[j], 12, "\xC2\xB7");
  }
}

static int on_row_wanted(void)
{
  int i;

  if(!c.single || (c.page < 0))
    return 0;
  for(i = 0; i < (int)cheat_count(); i++)
    if(cheat_is_on(i))
      return 1;
  return 0;
}

static void fill_rows(void)
{
  int i, k;

  c.n = c.n_cant = 0;
  if(c.page == GROUPS)
  {
    rows[c.n++] = 0;
    for(i = 0; i < (int)cheat_group_count(); i++)
      rows[c.n++] = i;
  }
  else if(c.page == ON_NOW)
  {
    for(k = 0; k < 2; k++)
      for(i = 0; i < (int)cheat_count(); i++)
        if(cheat_is_on(i) && (is_ready(i) == k))
          rows[c.n++] = i;
  }
  else
  {
    int first = cheat_group_first(c.page), end = first + cheat_group_size(c.page);
    if(on_row_wanted())
      rows[c.n++] = ON_ROW;
    for(i = first; i < end; i++)
      if(cheat_status(i) == CE_OK)
        rows[c.n++] = i;
      else
        c.n_cant++;
    if(c.n_cant)
    {
      rows[c.n++] = FOLD;
      if(c.fold_open)
        for(i = first; i < end; i++)
          if(cheat_status(i) != CE_OK)
            rows[c.n++] = i;
    }
  }
  build_marks();
}

static void open_page(int page, int cur)
{
  c.page = page;
  c.fold_open = 0;
  fill_rows();
  c.cur = c.from = (cur < c.n) ? cur : 0;
  c.cur_f = UI_F_CURSOR;
  c.top = (c.cur >= ROWS_SEEN) ? c.cur - ROWS_SEEN + 1 : 0;
  c.top_from = c.top * PITCH;
  c.top_f = UI_F_SCROLL;
  c.value_row = c.lock_row = -1;
  c.rest = 0;
  c.toast_f = -1;
  c.undo = UNDO_NONE;
  c.jump_f = -1;
  c.marq_f = 0;
  c.dirty = 1;
}

static void toggle_fold(void)
{
  c.fold_open = !c.fold_open;
  fill_rows();
  if(c.top > c.n - ROWS_SEEN)
  {
    c.top_from = top_px();
    c.top = (c.n > ROWS_SEEN) ? c.n - ROWS_SEEN : 0;
    c.top_f = 0;
  }
  if(c.fold_open && (c.cur + 1 < c.n) && (c.cur + 1 >= c.top + ROWS_SEEN))
  {
    c.top_from = top_px();
    c.top = c.cur + 2 - ROWS_SEEN;
    c.top_f = 0;
  }
  c.rest = 0;
  c.dirty = 1;
}

static void sync_on_row(void)
{
  int has = (c.n > 0) && (rows[0] == ON_ROW), d;

  if(has == on_row_wanted())
    return;
  d = has ? -1 : 1;
  fill_rows();
  c.cur = (c.cur + d < 0) ? 0 : c.cur + d;
  c.from = (c.from + d < 0) ? 0 : c.from + d;
  if(c.value_row >= 0)
    c.value_row += d;
  if(c.lock_row >= 0)
    c.lock_row += d;
  if((c.top > 0) || (d < 0))
  {
    c.top = (c.top + d < 0) ? 0 : c.top + d;
    c.top_from += d * PITCH;
    if(c.top_from < 0)
      c.top_from = 0;
  }
  if(c.cur < c.top)
    c.top = c.cur;
  c.dirty = 1;
}

static void jump(int dir)
{
  int j = mark_at(c.cur);

  if(dir > 0)
    j++;
  else if(c.cur <= c.mark_row[j])
    j--;
  if((j < 0) || (j >= c.marks))
    return;
  select_row(c.mark_row[j]);
  c.jump_mark = j;
  c.jump_f = c.dots ? -1 : 0;
  c.dirty = 1;
}

static void open_group(int g)
{
  c.list_cur = c.cur;
  open_page(g, 0);
}

static void after_on(int i, int r)
{
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
    case R_FOLD:
      toggle_fold();
      return;
    case R_CANT:
      return;
  }
  i = cheat_at(r);
  if(cheat_is_locked(i))
    return;
  if((k == R_RADIO) && !cheat_is_on(i))
  {
    int g = cheat_group_of(i), j, before = -1;
    int end = cheat_group_first(g) + cheat_group_size(g);
    for(j = cheat_group_first(g); j < end; j++)
      if((j != i) && cheat_is_on(j) && !cheat_is_locked(j))
        before = j;
    cheat_set_on(i, 1);
    if(before >= 0)
    {
      char t[80];
      snprintf(t, sizeof(t), T("%s switched off"), cheat_name(before));
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
    cheat_set_on(c.undo_cheat, 1);
  else if(!on_list() && lockable(r))
  {
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
      if(i < ON_ROW)
        undo_on[i] = cheat_is_on(i) && !cheat_is_locked(i);
    }
  for(i = 0; i < (int)cheat_count(); i++)
    if(undo_on[i])
      cheat_set_on(i, 0);
  show_toast(T("All cheats off"), UNDO_ALL_OFF);
}

int ui_cheats(void)
{
  u32 mark = ui_pool_mark();

  memset(&c, 0, sizeof(c));
  c.jump_f = -1;
  c.value_f = UI_F_VALUE;
  c.lock_f = UI_F_VALUE;
  ui_status_read();
  ui_input_init();
  c.single = cheat_group_count() <= 1;
  if(!c.single)
    open_page(GROUPS, 1);
  else
  {
    open_page(0, 0);
    if((c.n > 1) && (rows[0] == ON_ROW))
      open_page(0, 1);
  }

  while(!c.restart)
  {
    ui_input_update();
    if(c.help)
    {
      if(!c.help_out && (ui_pressed(PSP_CTRL_SQUARE) || ui_pressed(ui_key_back)))
      {
        c.help_out = 1;
        c.help_f = 0;
      }
    }
    else if(ui_pressed(ui_key_back))
    {
      if(c.single && (c.page == ON_NOW))
        open_page(0, 0);
      else if(on_list() || c.single)
        break;
      else
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
      if(c.marks)
      {
        if(ui_repeat(PSP_CTRL_LEFT))
          jump(-1);
        else if(ui_repeat(PSP_CTRL_RIGHT))
          jump(1);
      }
      if(ui_pressed(PSP_CTRL_SELECT))
        press_select();
      else if(ui_pressed(PSP_CTRL_TRIANGLE) && all_off_here())
        press_all_off();
      else if(ui_pressed(ui_key_confirm))
        press_confirm();
      sync_on_row();
    }
    if(c.restart)
      break;

    if(c.cur_f < UI_F_CURSOR) c.cur_f++;
    if(c.top_f < UI_F_SCROLL) { c.top_f++; c.dirty = 1; }
    if(c.value_f < UI_F_VALUE) { c.value_f++; c.dirty = 1; }
    if(c.lock_f < UI_F_VALUE) { c.lock_f++; c.dirty = 1; }
    if(c.rest < UI_F_TIP_DELAY + 8) c.rest++;
    if(c.marq_f < 30000) c.marq_f++;
    if(c.jump_f >= 0) { if(++c.jump_f >= JUMP_F) c.jump_f = -1; }
    if(c.dirty)
      build_ground();

    ui_frame_begin();
    ui_ground();
    ui_status_chip();
    draw_selection();
    if(c.toast_f >= 0)
    {
      if(!ui_toast_at(c.toast, UI_COL_LIME, c.undo ? T("Undo") : NULL, c.toast_f, TOAST_Y))
      {
        c.toast_f = -1;
        c.undo = UNDO_NONE;
        c.dirty = 1;
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

  cheat_engine_save();
  ui_layer_reset();
  ui_pool_release(mark);
  return c.restart;
}
