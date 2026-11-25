/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Hachi
 */

#include "../common.h"
#include "ui.h"

static int body_text(int x, int y, const char *s, u16 color, int flags)
{
  if(ui_font_loaded())
    return ui_print(x, y, s, color, UI_FONT_BODY, flags);
  return ui_text(x, y + 3, (char *)s, color);
}

static int body_width(const char *s, int flags)
{
  if(ui_font_loaded())
    return ui_print_width(s, UI_FONT_BODY, flags);
  return ui_text_width((char *)s);
}

static int small_text(int x, int y, const char *s, u16 color)
{
  return ui_text(x, y, (char *)s, color);
}

static int small_width(const char *s)
{
  return ui_text_width((char *)s);
}

/* Rounded boxes: radius r cuts r-1 pixels off each corner, stepping in. */
void ui_round_fill(int x, int y, int w, int h, int r, u16 color)
{
  int i;

  for(i = 0; i < r - 1; i++)
  {
    int inset = r - 1 - i;
    ui_fill(x + inset, y + i, w - inset * 2, 1, color);
    ui_fill(x + inset, y + h - 1 - i, w - inset * 2, 1, color);
  }
  if(r < 1)
    r = 1;
  ui_fill(x, y + r - 1, w, h - (r - 1) * 2, color);
}

void ui_round_frame(int x, int y, int w, int h, int r, u16 color)
{
  int i, c = (r > 1) ? r - 1 : 0;

  ui_fill(x + c, y, w - c * 2, 1, color);
  ui_fill(x + c, y + h - 1, w - c * 2, 1, color);
  ui_fill(x, y + c, 1, h - c * 2, color);
  ui_fill(x + w - 1, y + c, 1, h - c * 2, color);
  for(i = 1; i < c; i++)
  {
    ui_fill(x + c - i, y + i, 1, 1, color);
    ui_fill(x + w - 1 - c + i, y + i, 1, 1, color);
    ui_fill(x + c - i, y + h - 1 - i, 1, 1, color);
    ui_fill(x + w - 1 - c + i, y + h - 1 - i, 1, 1, color);
  }
}

void ui_cursor_frame(int x, int y, int w, int h)
{
  ui_round_frame(x - 3, y - 3, w + 6, h + 6, 3, UI_COL_CREAM);
  ui_round_frame(x - 2, y - 2, w + 4, h + 4, 2, UI_COL_CREAM);
}

static void lifted_card(int x, int y, int w, int h, u16 color, int r, int lift, int shadow)
{
  if(lift)
    ui_round_fill(x + shadow - lift, y + shadow - lift, w, h, r, UI_COL_SHADE);
  ui_round_fill(x - lift, y - lift, w, h, r, color);
}

static const u16 icon_rom[11] =
{
  0x000, 0x3F8, 0x20C, 0x2E4, 0x2E4, 0x204, 0x2AC, 0x3F8, 0x000, 0x000, 0x000
};

static void draw_icon(int x, int y, const u16 *bits, u16 color)
{
  int i, j;

  for(j = 0; j < 11; j++)
    for(i = 0; i < 11; i++)
      if(bits[j] & (0x400 >> i))
        ui_fill(x + i, y + j, 1, 1, color);
}

void ui_per_game_mark(int x, int y, u16 color)
{
  draw_icon(x, y, icon_rom, color);
}

/* Returns where the value goes: its right edge in *vx and the row's top
 * (lifted) in *vy. */
void ui_card_row(int x, int y, int w, const char *label, int selected, u16 group,
 int flags, int *vx, int *vy)
{
  int lift = selected ? UI_LIFT : 0;
  u16 tc = selected ? UI_COL_PLUM : UI_COL_CREAM;
  int end;

  if(flags & UI_ROW_DISABLED)
  {
    ui_round_fill(x, y, w, UI_ROW_H, 2, UI_COL_PLUM_2);
    end = body_text(x + 8, y + 2, label, UI_COL_MUTE, 0);
    if(flags & UI_ROW_PER_GAME)
      draw_icon(end + 5, y + 2 + 4, icon_rom, UI_COL_MUTE);
    *vx = x + w - 8;
    *vy = y;
    return;
  }

  lifted_card(x, y, w, UI_ROW_H, selected ? group : UI_COL_CARD, 2, lift, UI_SHADOW);
  if(flags & UI_ROW_CHANGED)
    ui_fill(x - 9, y + 8, 4, 4, UI_COL_TANGERINE);
  x -= lift;
  y -= lift;
  end = body_text(x + 8, y + 2, label, tc, 0);
  if(flags & UI_ROW_PER_GAME)
    draw_icon(end + 5, y + 2 + 4, icon_rom, selected ? UI_COL_PLUM : UI_COL_TANGERINE);
  *vx = x + w - 8;
  *vy = y;
}

void ui_row_note(int xr, int y, const char *note)
{
  small_text(xr - small_width(note), y + 3, note, UI_COL_MUTE);
}

/* f = frames since the change; f >= UI_F_VALUE = still. */
void ui_row_cycle(int xr, int y, const char *value, int selected, u16 group, int f)
{
  u16 tc = selected ? UI_COL_PLUM : UI_COL_CREAM;
  u16 ac = selected ? UI_COL_PLUM : group;
  int ex, off = 0, vw = body_width(value, 0);

  ex = xr - body_width("\xE2\x96\xB6", 0);
  body_text(ex, y + 2, "\xE2\x96\xB6", ac, 0);
  ex -= 4;
  if(f < UI_F_VALUE)
    off = ui_ease_out_back(10, 0, f, UI_F_VALUE);
  body_text(ex - vw + off, y + 2, value, tc, 0);
  body_text(ex - vw - 4 - body_width("\xE2\x97\x80", 0), y + 2, "\xE2\x97\x80", ac, 0);
}

/* A cycle value drawn from a 1-bit mask (16 rows of 13 bytes, w px wide)
 * instead of text. */
void ui_row_cycle_mask(int xr, int y, const u8 (*bits)[13], int w, int selected, u16 group,
 int f)
{
  u16 tc = selected ? UI_COL_PLUM : UI_COL_CREAM;
  u16 ac = selected ? UI_COL_PLUM : group;
  int ex, off = 0, r, c;

  ex = xr - body_width("\xE2\x96\xB6", 0);
  body_text(ex, y + 2, "\xE2\x96\xB6", ac, 0);
  ex -= 4;
  if(f < UI_F_VALUE)
    off = ui_ease_out_back(10, 0, f, UI_F_VALUE);
  for(r = 0; r < 16; r++)
    for(c = 0; c < w; c++)
      if(bits[r][c >> 3] & (0x80 >> (c & 7)))
        ui_fill(ex - w + off + c, y + 2 + r, 1, 1, tc);
  body_text(ex - w - 4 - body_width("\xE2\x97\x80", 0), y + 2, "\xE2\x97\x80", ac, 0);
}

static int seg_disc(const char *label)
{
  return (label[0] == '\x01') ? label[1] : 0;
}

static void seg_disc_draw(int x, int y, int which, u16 col, u16 under)
{
  static const u8 cross[5] = { 0x11, 0x0A, 0x04, 0x0A, 0x11 };
  static const u8 circle[5] = { 0x0E, 0x11, 0x11, 0x11, 0x0E };
  const u8 *mark = (which == 'x') ? cross : circle;
  int i, j;

  ui_disc(x + 4, y + 4, 4, col);
  for(j = 0; j < 5; j++)
    for(i = 0; i < 5; i++)
      if(mark[j] & (0x10 >> i))
        ui_fill(x + 2 + i, y + 2 + j, 1, 1, under);
}

void ui_row_segments(int xr, int y, const char **labels, int count, int k,
 int from, int f, int selected, u16 group)
{
  int widths[8], starts[8];
  int j, sx = xr, fill_x, fill_w;
  u16 fill = selected ? UI_COL_PLUM : group;

  if(count > 8)
    count = 8;
  if(count < 1)
    return;
  if((k < 0) || (k >= count))
    k = 0;
  for(j = 0; j < count; j++)
  {
    widths[j] = (seg_disc(labels[j]) ? 9 : small_width(labels[j])) + 12;
    sx -= widths[j];
  }
  for(j = 0; j < count; j++)
  {
    starts[j] = sx;
    sx += widths[j];
  }

  fill_x = starts[k];
  fill_w = widths[k];
  if((f < UI_F_VALUE) && (from >= 0) && (from < count) && (from != k))
  {
    fill_x = ui_ease_out_back(starts[from], starts[k], f, UI_F_VALUE);
    fill_w = ui_ease_out_back(widths[from], widths[k], f, UI_F_VALUE);
  }
  ui_round_fill(fill_x, y + 3, fill_w, 14, 2, fill);

  for(j = 0; j < count; j++)
  {
    u16 col;
    if(j == k)
      col = selected ? group : UI_COL_PLUM;
    else
      col = selected ? UI_COL_PLUM : UI_COL_MUTE;
    if(seg_disc(labels[j]))
      seg_disc_draw(starts[j] + 6, y + 6, seg_disc(labels[j]), col,
       (j == k) ? fill : (selected ? group : UI_COL_CARD));
    else
      small_text(starts[j] + 6, y + 3 + ((ui_font_loaded()) ? 0 : 3), labels[j], col);
  }
}

/* p = knob position in 1/256 (0 off .. 256 on; out-back overshoots). */
void ui_row_switch(int xr, int y, int p, int selected)
{
  int x = xr - 28, on = p >= 128, kx;

  y += 3;
  if(on)
    ui_round_fill(x, y, 28, 14, 3, selected ? UI_COL_PLUM : UI_COL_LIME);
  else
    ui_round_frame(x, y, 28, 14, 3, selected ? UI_COL_PLUM : UI_COL_MUTE);
  if(p < 0) p = 0;
  if(p > 294) p = 294;
  kx = x + 2 + (14 * p + 128) / 256;
  ui_round_fill(kx, y + 2, 10, 10, 2, (on || !selected) ? UI_COL_CREAM : UI_COL_PLUM);
  if(on && selected)
    ui_fill(x + 5, y + 6, 6, 2, UI_COL_LIME);
}

int ui_switch_pos(int on, int f)
{
  return on ? ui_ease_out_back(0, 256, f, UI_F_VALUE)
            : ui_ease_out_back(256, 0, f, UI_F_VALUE);
}

void ui_row_blocks(int xr, int y, int n, int k, const char *text, int f,
 int selected, u16 group)
{
  u16 on = selected ? UI_COL_PLUM : group;
  u16 off = selected ? COLOR16(11, 13, 8) : UI_COL_PLUM;
  int bw = n * 10, bx = xr - bw + 2, i;

  for(i = 0; i < n; i++)
  {
    int h = 12, yy = y + 4;
    if((i == k) && (f < UI_F_VALUE))
    {
      h = ui_ease_out_back(2, 12, f, UI_F_VALUE);
      if(h < 2) h = 2;
      yy = y + 4 + 12 - h;
    }
    ui_fill(bx + i * 10, yy, 8, h, (i <= k) ? on : off);
  }
  if(text)
    body_text(xr - bw - 6 - body_width(text, 0), y + 2, text,
     selected ? UI_COL_PLUM : UI_COL_CREAM, 0);
}

void ui_row_action(int xr, int y, int selected)
{
  body_text(xr - body_width("\xE2\x96\xB6", 0), y + 2, "\xE2\x96\xB6",
   selected ? UI_COL_PLUM : UI_COL_CREAM, 0);
}

/* The pill slides from tab prev to k over UI_F_TAB frames. Returns the
 * right edge. */
int ui_tabs(int x, const char **labels, int count, int k, int prev, int f)
{
  int xs[8], ws[8], j, px, pw;

  if(count > 8)
    count = 8;
  x = ui_glyph(x, 11, PSP_CTRL_LTRIGGER) + 6;
  for(j = 0; j < count; j++)
  {
    ws[j] = body_width(labels[j], 0) + 18;
    xs[j] = x;
    x += ws[j] + 4;
  }
  px = xs[k];
  pw = ws[k];
  if((f < UI_F_TAB) && (prev >= 0) && (prev < count) && (prev != k))
  {
    px = ui_ease_out_back(xs[prev], xs[k], f, UI_F_TAB);
    pw = ui_ease_out_back(ws[prev], ws[k], f, UI_F_TAB);
  }
  ui_round_fill(px + 2, 8, pw, 22, 3, UI_COL_SHADE);
  ui_round_fill(px, 6, pw, 22, 3, UI_COL_CREAM);
  for(j = 0; j < count; j++)
  {
    int lit = (j == k) && ((f >= UI_F_TAB / 2) || (prev == k) || (prev < 0));
    body_text(xs[j] + 9, 9, labels[j], lit ? UI_COL_PLUM : UI_COL_MUTE, 0);
  }
  return ui_glyph(x + 2, 11, PSP_CTRL_RTRIGGER);
}

/* f = frames since it was shown. Returns 0 once it is gone. */
int ui_toast(const char *text, u16 edge, const char *undo, int f)
{
  return ui_toast_at(text, edge, undo, f, 214);
}

int ui_toast_at(const char *text, u16 edge, const char *undo, int f, int y)
{
  int w, x, right = 466, out;

  if(f < 0)
    return 0;
  w = 12 + body_width(text, 0) + 12;
  if(undo)
    w += 10 + ui_glyph_width(PSP_CTRL_SELECT) + 4 + small_width(undo) + 1;
  out = w + 24;
  if(f < UI_F_TOAST_IN)
    x = right - w + ui_ease_out_back(out, 0, f + 1, UI_F_TOAST_IN);
  else if(f < UI_F_TOAST_IN + UI_F_TOAST_HOLD)
    x = right - w;
  else if(f < UI_F_TOAST_IN + UI_F_TOAST_HOLD + UI_F_TOAST_OUT)
    x = right - w + ui_ease_in_cubic(0, out, f - UI_F_TOAST_IN - UI_F_TOAST_HOLD + 1, UI_F_TOAST_OUT);
  else
    return 0;

  ui_round_fill(x + 3, y + 3, w, 26, 2, UI_COL_SHADE);
  ui_round_fill(x, y, w, 26, 2, UI_COL_CREAM);
  ui_fill(x, y + 3, 4, 20, edge);
  if(undo)
  {
    int ex = body_text(x + 12, y + 5, text, UI_COL_PLUM, 0);
    int gx = ui_glyph_inverted(ex + 10, y + 7, PSP_CTRL_SELECT) + 4;
    if(ui_font_loaded())
      ui_print(gx, y + 6, undo, UI_COL_PLUM, UI_FONT_SMALL, UI_TEXT_BOLD);
    else
      small_text(gx, y + 6, undo, UI_COL_PLUM);
  }
  else
    body_text(x + 12, y + 5, text, UI_COL_PLUM, 0);
  return 1;
}

#define GAME_TOAST_KEY 0x7C1F
static int game_toast_f, game_toast_w;

static int game_toast_tick(int *x, int *y, int *w, int *h)
{
  int f = game_toast_f++, out = game_toast_w + 24, right = 466 - game_toast_w;

  if(f < UI_F_TOAST_IN)
    *x = right + ui_ease_out_back(out, 0, f + 1, UI_F_TOAST_IN);
  else if(f < UI_F_TOAST_IN + UI_F_TOAST_HOLD)
    *x = right;
  else if(f < UI_F_TOAST_IN + UI_F_TOAST_HOLD + UI_F_TOAST_OUT)
    *x = right + ui_ease_in_cubic(0, out, f - UI_F_TOAST_IN - UI_F_TOAST_HOLD + 1, UI_F_TOAST_OUT);
  else
    return 0;
  *y = 6;
  *w = game_toast_w + 3;
  *h = 29;
  return 1;
}

void ui_game_toast(const char *text, u16 edge)
{
  u16 *saved = ui_fb;
  u32 saved_pitch = ui_fb_pitch, i;
  int w = body_width(text, 0) + 28, x0;

  if(w > OSD_W - 3)
    w = OSD_W - 3;
  for(i = 0; i < OSD_W * OSD_H; i++)
    osd_pixels[i] = GAME_TOAST_KEY;
  x0 = 460 - w;
  ui_fb = osd_pixels - (x0 + 212 * OSD_W);
  ui_fb_pitch = OSD_W;
  ui_toast(text, edge, NULL, UI_F_TOAST_IN);
  ui_fb = saved;
  ui_fb_pitch = saved_pitch;
  for(i = 0; i < OSD_W * OSD_H; i++)
    osd_pixels[i] = (osd_pixels[i] == GAME_TOAST_KEY) ? 0 : (osd_pixels[i] | 0x8000);

  game_toast_w = w;
  game_toast_f = 0;
  osd_tick = game_toast_tick;
}

void ui_game_battery(int level, int on_battery)
{
  static int shown = -1;
  u16 *saved = ui_fb;
  u32 saved_pitch = ui_fb_pitch, i;
  char pct[8];
  int w, gx, gy, gw, gh, bar;

  if(!on_battery || (level < 0) || (level > 10))
  {
    osd_chip_rect[2] = 0;
    shown = -1;
    return;
  }
  sprintf(pct, "%d%%", level);
  w = ui_text_width(pct) + 18;
  if(w > OSD_CHIP_W)
    w = OSD_CHIP_W;
  if(level != shown)
  {
    shown = level;
    for(i = 0; i < OSD_CHIP_W * OSD_CHIP_H; i++)
      osd_chip_pixels[i] = GAME_TOAST_KEY;
    ui_fb = osd_chip_pixels;
    ui_fb_pitch = OSD_CHIP_W;
    ui_round_fill(0, 0, w, 16, 2, UI_COL_PINK);
    ui_fill(4, 5, 8, 1, UI_COL_PLUM);
    ui_fill(4, 10, 8, 1, UI_COL_PLUM);
    ui_fill(4, 5, 1, 6, UI_COL_PLUM);
    ui_fill(11, 5, 1, 6, UI_COL_PLUM);
    ui_fill(12, 7, 1, 2, UI_COL_PLUM);
    ui_text(15, 1, pct, UI_COL_PLUM);
    ui_fb = saved;
    ui_fb_pitch = saved_pitch;
    for(i = 0; i < OSD_CHIP_W * OSD_CHIP_H; i++)
      osd_chip_pixels[i] = (osd_chip_pixels[i] == GAME_TOAST_KEY) ? 0 : (osd_chip_pixels[i] | 0x8000);
  }

  get_game_rect(&gx, &gy, &gw, &gh);
  bar = UI_SCREEN_W - (gx + gw);
  osd_chip_rect[0] = (bar >= w) ? gx + gw + (bar - w) / 2 : 474 - w;
  osd_chip_rect[1] = 250;
  osd_chip_rect[3] = 16;
  osd_chip_rect[2] = w;
}

#define STATUS_W 28

static void status_foot(int y, int h, int ff)
{
  int a, i;

  if(ff >= 2)
  {
    const char *t = (ff == 2) ? "2\xC3\x97" : "3\xC3\x97";
    /* centred on the ink: the advance of "×" ends in a blank column */
    ui_print((STATUS_W - ui_print_width(t, UI_FONT_SMALL, 0)) / 2 + 1, y + (h - 14) / 2, t, UI_COL_PLUM,
     UI_FONT_SMALL, 0);
    return;
  }

  for(a = 0; a < 2; a++)
    for(i = 0; i < 4; i++)
      ui_fill((STATUS_W - 10) / 2 + a * 6 + i, y + (h - 8) / 2 + i, 1, 8 - 2 * i, UI_COL_PLUM);
}

void ui_game_status(int fps, int ff)
{
  static int shown = -2, shown_ff = -1, shown_x = -1;
  u16 *saved = ui_fb;
  u32 saved_pitch = ui_fb_pitch, i;
  char num[8];
  int h, x, gx, gy, gw, gh;

  if((fps < 0) && !ff)
  {
    osd_fps_rect[2] = 0;
    shown = -2;
    return;
  }
  if(fps > 999)
    fps = 999;
  get_game_rect(&gx, &gy, &gw, &gh);
  x = ((gx > 0) && (gx < 60)) ? (gx - STATUS_W) / 2 : 6;
  if((fps == shown) && (ff == shown_ff) && (x == shown_x))
    return;
  shown = fps;
  shown_ff = ff;
  shown_x = x;

  for(i = 0; i < OSD_FPS_W * OSD_FPS_H; i++)
    osd_fps_pixels[i] = GAME_TOAST_KEY;
  ui_fb = osd_fps_pixels;
  ui_fb_pitch = OSD_FPS_W;
  if(fps >= 0)
  {
    h = ff ? 30 : 16;
    if(ff)
    {
      ui_round_fill(0, 0, STATUS_W, h, 2, UI_COL_TANGERINE);
      ui_fill(0, 8, STATUS_W, 8, UI_COL_SHADE);
      status_foot(16, 14, ff);
    }
    ui_round_fill(0, 0, STATUS_W, ff ? 12 : 16, 2, UI_COL_SHADE);
    snprintf(num, sizeof(num), "%d", fps);
    ui_print((STATUS_W - ui_print_width(num, UI_FONT_SMALL, 0)) / 2, 1, num,
     (fps < 50) ? UI_COL_PINK : UI_COL_CREAM, UI_FONT_SMALL, 0);
  }
  else
  {
    h = 16;
    ui_round_fill(0, 0, STATUS_W, h, 2, UI_COL_TANGERINE);
    status_foot(0, 16, ff);
  }
  ui_fb = saved;
  ui_fb_pitch = saved_pitch;
  for(i = 0; i < OSD_FPS_W * OSD_FPS_H; i++)
    osd_fps_pixels[i] = (osd_fps_pixels[i] == GAME_TOAST_KEY) ? 0 : (osd_fps_pixels[i] | 0x8000);

  osd_fps_rect[0] = x;
  osd_fps_rect[1] = 6;
  osd_fps_rect[3] = h;
  osd_fps_rect[2] = STATUS_W;
}

static const u16 icon_battery[11] = { 0x000, 0x000, 0x3FE, 0x203, 0x203, 0x203, 0x3FE, 0x000, 0x000, 0x000, 0x000 };

static const u8 bolt[7] = { 0x03, 0x06, 0x0C, 0x1F, 0x06, 0x0C, 0x18 };

/* Each scePower call asks the power chip and costs milliseconds. */
static int status_age;

static ScePspDateTime status_now;
static int status_level, status_format, status_battery, status_charging;

void ui_status_read(void)
{
  status_level = scePowerGetBatteryLifePercent();
  status_battery = scePowerIsBatteryExist() > 0;
  status_charging = status_battery && (scePowerIsBatteryCharging() > 0);
  sceRtcGetCurrentClockLocalTime(&status_now);
  status_format = 0;
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_TIME_FORMAT, &status_format);
  status_age = 60;
}

void ui_status_chip(void)
{
  ui_status_chip_at(8);
}

void ui_status_chip_at(int y)
{
  int shown_level;
  char clock[16], pct[8];
  int w, x, tx, i, j, critical = 0;
  u16 color = UI_COL_LIME;

  if(status_age <= 0)
  {
    /* reading the system settings costs 10-20 ms: the 12/24-hour setting
       is read in ui_status_read() only */
    status_age = 60;
    sceRtcGetCurrentClockLocalTime(&status_now);
  }
  else if(status_age == 45)
    status_level = scePowerGetBatteryLifePercent();
  else if(status_age == 30)
    status_battery = scePowerIsBatteryExist() > 0;
  else if(status_age == 15)
    status_charging = status_battery && (scePowerIsBatteryCharging() > 0);
  status_age--;
  shown_level = (status_level < 0) ? 0 : ((status_level > 100) ? 100 : status_level);
  if(status_format == PSP_SYSTEMPARAM_TIME_FORMAT_12HR)
    snprintf(clock, sizeof(clock), "%d:%02d %s", (status_now.hour + 11) % 12 + 1, status_now.minute, (status_now.hour < 12) ? T("AM") : T("PM"));
  else
    sprintf(clock, "%02d:%02d", status_now.hour, status_now.minute);
  if(status_charging)
    color = UI_COL_SKY;
  else if(shown_level <= 10)
  {
    color = UI_COL_PINK;
    critical = status_battery;
  }
  else if(shown_level <= 20)
    color = UI_COL_TANGERINE;
  sprintf(pct, "%d%%", shown_level);

  w = 6 + ui_text_width(clock) + 4 + (critical ? ui_text_width(pct) + 4 : 0) +
   (status_charging ? 7 : 0) + (status_battery ? 11 + 5 : 2);
  x = 460 - w;
  ui_round_fill(x, y, w, 16, 2, UI_COL_CARD);
  ui_text(x + 6, y + 1, clock, UI_COL_CREAM);
  if(!status_battery)
    return;
  tx = x + 6 + ui_text_width(clock) + 4;
  if(critical)
  {
    ui_text(tx, y + 1, pct, UI_COL_PINK);
    tx += ui_text_width(pct) + 4;
  }
  if(status_charging)
  {
    for(j = 0; j < 7; j++)
      for(i = 0; i < 5; i++)
        if(bolt[j] & (0x10 >> i))
          ui_fill(tx + i, y + 4 + j, 1, 1, UI_COL_SKY);
    tx += 7;
  }
  draw_icon(tx - 1, y + 3, icon_battery, color);
  ui_fill(tx + 1, y + 6, 7 * shown_level / 100, 3, color);
}


int ui_toast_up(int f)
{
  return (f >= 0) && (f < UI_F_TOAST_IN + UI_F_TOAST_HOLD + UI_F_TOAST_OUT);
}

void ui_hold_bar(int x, int y, int w, int p, u16 color)
{
  ui_fill(x, y, w, 6, UI_COL_PLUM_2);
  if(p > 256) p = 256;
  if(p > 0)
    ui_fill(x, y, w * p / 256, 6, color);
}

/* Call once a frame. */
int ui_hold_step(ui_hold_t *h, u32 key)
{
  u32 frames = ui_hold_frames(key);

  h->done = 0;
  if(ui_down(key))
  {
    if(frames >= UI_F_HOLD_SHOW)
      h->shown = 1;
    if(frames >= UI_F_HOLD)
    {
      if(!h->fired)
      {
        h->fired = 1;
        h->done = 1;
        h->p = 256;
      }
      else
      {
        h->p = 0;
        h->shown = 0;
      }
    }
    else
      h->p = (frames * 256) / UI_F_HOLD;
  }
  else
  {
    h->fired = 0;
    if(h->p > 0)
    {
      h->p -= (256 + UI_F_HOLD_DRAIN - 1) / UI_F_HOLD_DRAIN;
      if(h->p < 0)
        h->p = 0;
    }
    if(h->p == 0)
      h->shown = 0;
  }
  return h->p;
}

void ui_tip_line(const char *text, int k)
{
  char line[200];
  int c, col = UI_COL_MUTE & 0x8000;

  if(k < 1)
    return;
  if(k > 8)
    k = 8;
  snprintf(line, sizeof(line), "\xE2\x96\xB8 %s", text);
  for(c = 0; c < 15; c += 5)
  {
    int a = (UI_COL_PLUM >> c) & 31, b = (UI_COL_MUTE >> c) & 31;
    col |= ((a + (b - a) * k / 8) & 31) << c;
  }
  ui_print_fit(UI_MARGIN, 231 + 6 - 6 * UI_EASE_OUT_CUBIC_8[k - 1] / 256, line, (u16)col,
   UI_FONT_SMALL, 0, UI_SCREEN_W - 2 * UI_MARGIN);
}

static u32 wrap_cp(const char *s)
{
  const u8 *u = (const u8 *)s;

  if(u[0] < 0x80)
    return u[0];
  if((u[0] & 0xE0) == 0xC0)
    return ((u[0] & 0x1F) << 6) | (u[1] & 0x3F);
  if((u[0] & 0xF0) == 0xE0)
    return ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F);
  return 0xFFFD;
}

static int wrap_cjk(u32 c)
{
  return ((c >= 0x3000) && (c <= 0x9FFF)) || ((c >= 0xAC00) && (c <= 0xD7AF)) ||
   ((c >= 0xFF00) && (c <= 0xFFEF));
}

static int wrap_no_start(u32 c)
{
  static const u16 no[] =
  {
    0x3002, 0x3001, 0xFF0C, 0xFF0E, 0xFF01, 0xFF1F, 0xFF1A, 0xFF1B, 0xFF09, 0x300D, 0x300F,
    0x3011, 0x2026, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083, 0x3085, 0x3087,
    0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30C3, 0x30E3, 0x30E5, 0x30E7, 0x30FC, 0
  };
  int i;

  for(i = 0; no[i]; i++)
    if(c == no[i])
      return 1;
  return 0;
}

/* Copies line n of text (fitting max_width in font) into out. Returns 0
 * past the end. */
int ui_wrap_line(const char *text, int n, int max_width, int font, char *out, int out_size)
{
  const char *p = text;
  int line = 0;

  while(*p)
  {
    const char *q, *brk = NULL, *end;
    char buf[256];
    u32 prev = 0;

    while(*p == ' ')
      p++;
    if(!*p)
      break;
    q = end = p;
    while(*q && (*q != '\n'))
    {
      const char *next = q + 1;
      u32 cp = wrap_cp(q);
      int len;
      while((*next & 0xC0) == 0x80)
        next++;
      len = next - p;
      if(len >= (int)sizeof(buf))
        break;
      if(cp == ' ')
      {
        brk = q;
        q = next;
        end = q;
        prev = cp;
        continue;
      }
      if((q > p) && (prev != ' ') && (wrap_cjk(cp) || wrap_cjk(prev)) && !wrap_no_start(cp))
        brk = q;
      memcpy(buf, p, len);
      buf[len] = 0;
      if(((font == UI_FONT_BODY) ? body_width(buf, 0) : small_width(buf)) > max_width)
        break;
      q = next;
      end = q;
      prev = cp;
    }
    if(*q && (*q != '\n') && brk && (brk > p))
      end = brk;
    if(end == p)
    {
      end = p + 1;
      while((*end & 0xC0) == 0x80)
        end++;
    }
    {
      int len = end - p;
      while((len > 0) && (p[len - 1] == ' '))
        len--;
      if(line == n)
      {
        if(len >= out_size)
          len = out_size - 1;
        memcpy(out, p, len);
        out[len] = 0;
        return 1;
      }
    }
    line++;
    p = end;
    if(*p == '\n')
      p++;
  }
  return 0;
}

void ui_dialog(const char *title, const char *body, const char **buttons,
 int button_count, int selected, u16 edge, int f, int closing)
{
  int w = 360, h = 132, x = (UI_SCREEN_W - w) / 2, y, j, bx;
  char line[256];

  ui_door(0, 0, UI_SCREEN_W, UI_SCREEN_H);
  if(closing)
    y = 66 + ui_ease_in_cubic(0, UI_SCREEN_H - 66, f, UI_F_DIALOG_OUT);
  else
    y = 66 - ui_ease_out_back(24, 0, f, UI_F_DIALOG_IN);

  ui_round_fill(x + 4, y + 4, w, h, 3, UI_COL_SHADE);
  ui_round_fill(x, y, w, h, 3, UI_COL_CREAM);
  ui_fill(x, y + 4, 6, h - 8, edge);
  if(ui_font_loaded())
    ui_print_fit(x + 18, y + 4, title, UI_COL_PLUM, UI_FONT_BODY,
     UI_TEXT_2X | UI_TEXT_BOLD, w - 30);
  else
    ui_text(x + 18, y + 10, (char *)title, UI_COL_PLUM);
  for(j = 0; j < 3; j++)
    if(ui_wrap_line(body, j, w - 36, UI_FONT_BODY, line, sizeof(line)))
      body_text(x + 18, y + 40 + j * 16, line, UI_COL_PLUM, 0);

  bx = x + w - 16;
  for(j = button_count - 1; j >= 0; j--)
  {
    int bw = body_width(buttons[j], 0) + 24, by = y + h - 32;
    if(bw < 70)
      bw = 70;
    bx -= bw;
    if(j == selected)
    {
      ui_round_fill(bx + 1, by + 1, bw, 22, 2, UI_COL_SHADE);
      ui_round_fill(bx - 2, by - 2, bw, 22, 2, edge);
      body_text(bx - 2 + (bw - body_width(buttons[j], 0)) / 2, by + 1, buttons[j], UI_COL_PLUM, 0);
    }
    else
    {
      ui_round_frame(bx, by, bw, 22, 2, UI_COL_PLUM);
      body_text(bx + (bw - body_width(buttons[j], 0)) / 2, by + 3, buttons[j], UI_COL_PLUM, 0);
    }
    bx -= 10;
  }
}

void ui_line(int x0, int y0, int x1, int y1, u16 color)
{
  int dx = (x1 > x0) ? x1 - x0 : x0 - x1, sx = (x0 < x1) ? 1 : -1;
  int dy = (y1 > y0) ? y0 - y1 : y1 - y0, sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy, e2;

  while(1)
  {
    ui_fill(x0, y0, 1, 1, color);
    if((x0 == x1) && (y0 == y1))
      break;
    e2 = 2 * err;
    if(e2 >= dy) { err += dy; x0 += sx; }
    if(e2 <= dx) { err += dx; y0 += sy; }
  }
}

static int disc_span(int r, int y)
{
  int w = 0;

  while((w + 1) * (w + 1) + y * y <= r * r + r)
    w++;
  return w;
}

void ui_disc(int cx, int cy, int r, u16 color)
{
  int y;

  for(y = -r; y <= r; y++)
  {
    int w = disc_span(r, y);
    ui_fill(cx - w, cy + y, w * 2 + 1, 1, color);
  }
}

void ui_ring(int cx, int cy, int r, int t, u16 color)
{
  int y;

  for(y = -r; y <= r; y++)
  {
    int wo = disc_span(r, y), ri = r - t, wi;
    if((y < -ri) || (y > ri))
    {
      ui_fill(cx - wo, cy + y, wo * 2 + 1, 1, color);
      continue;
    }
    wi = disc_span(ri, y);
    ui_fill(cx - wo, cy + y, wo - wi, 1, color);
    ui_fill(cx + wi + 1, cy + y, wo - wi, 1, color);
  }
}

static u8 box_i[144][3], box_w[144][3];

static void box_tables(void)
{
  int x, k;

  for(x = 0; x < 144; x++)
  {
    int s0 = x * 5, s1 = s0 + 5;
    for(k = 0; k < 3; k++)
    {
      int i = s0 / 3 + k, a = i * 3, b = a + 3;
      if(a < s0) a = s0;
      if(b > s1) b = s1;
      box_i[x][k] = (i < 240) ? i : 239;
      box_w[x][k] = (b > a) ? b - a : 0;
    }
  }
}

static u32 mix43(u16 a, u16 b, int wa, int wb)
{
  u32 r = (a & 31) * wa + (b & 31) * wb, g = ((a >> 5) & 31) * wa + ((b >> 5) & 31) * wb;
  u32 bl = ((a >> 10) & 31) * wa + ((b >> 10) & 31) * wb;
  return ((r + 2) >> 2) | (((g + 2) >> 2) << 5) | (((bl + 2) >> 2) << 10);
}

static void shrink43_line(const u16 *s, u16 *d)
{
  int i;
  for(i = 0; i < 60; i++, s += 4, d += 3)
  {
    d[0] = mix43(s[0], s[1], 3, 1);
    d[1] = mix43(s[1], s[2], 2, 2);
    d[2] = mix43(s[2], s[3], 1, 3);
  }
}

/* rows is a multiple of 4; dst gets rows * 3 / 4 rows. */
void ui_shrink_43_rows(const u16 *src, int rows, u16 *dst)
{
  static u16 a[180], b[180], c[180], e[180];
  int j, x;

  for(j = 0; j + 4 <= rows; j += 4, src += 4 * 240, dst += 3 * 180)
  {
    shrink43_line(src, a);
    shrink43_line(src + 240, b);
    shrink43_line(src + 480, c);
    shrink43_line(src + 720, e);
    for(x = 0; x < 180; x++)
    {
      dst[x] = mix43(a[x], b[x], 3, 1);
      dst[180 + x] = mix43(b[x], c[x], 2, 2);
      dst[360 + x] = mix43(c[x], e[x], 1, 3);
    }
  }
}

/* rows (a multiple of 5) screenshot rows, 240 wide, to rows * 3 / 5 rows of
   a 144-wide picture: the pause screen's big picture, the Continue card */
void ui_shrink_53_rows(const u16 *src, int rows, u16 *dst)
{
  static u16 sum[3][3][144];
  int y, x, k, c;

  if(!box_i[143][0])
    box_tables();

  for(y = 0; y < rows * 3 / 5; y++)
  {
    for(k = 0; k < 3; k++)
    {
      const u16 *row = src + box_i[y][k] * 240;
      if(!box_w[y][k])
        continue;
      for(x = 0; x < 144; x++)
      {
        u32 r = 0, g = 0, b = 0;
        for(c = 0; c < 3; c++)
        {
          u16 px = row[box_i[x][c]];
          u32 w = box_w[x][c];
          r += (px & 0x1F) * w;
          g += ((px >> 5) & 0x1F) * w;
          b += ((px >> 10) & 0x1F) * w;
        }
        sum[k][0][x] = r;
        sum[k][1][x] = g;
        sum[k][2][x] = b;
      }
    }
    for(x = 0; x < 144; x++)
    {
      u32 ch[3];
      for(c = 0; c < 3; c++)
      {
        u32 t = 0;
        for(k = 0; k < 3; k++)
          if(box_w[y][k])
            t += sum[k][c][x] * box_w[y][k];
        ch[c] = (t + 12) / 25;
      }
      dst[y * 144 + x] = COLOR16(ch[0], ch[1], ch[2]);
    }
  }
}

void ui_help_card(const char *title, const char *body, u16 edge, int f, int closing)
{
  ui_hint_t h[2];
  char line[200];
  int lines = 0, w = 440, x = (UI_SCREEN_W - w) / 2, y, hgt, j;

  while(ui_wrap_line(body, lines, w - 36, UI_FONT_BODY, line, sizeof(line)) && (lines < 9))
    lines++;
  hgt = 38 + lines * 16 + 12;
  if(closing)
    y = 26 + ui_ease_in_cubic(0, UI_SCREEN_H - 26, f, UI_F_HELP_OUT);
  else
    y = 26 + 30 - ui_ease_out_back(0, 30, f, UI_F_HELP_IN);
  if(!(closing && (f >= UI_F_HELP_OUT)))
    ui_door(0, 0, UI_SCREEN_W, UI_STRIP_Y);
  ui_round_fill(x + 4, y + 4, w, hgt, 3, UI_COL_SHADE);
  ui_round_fill(x, y, w, hgt, 3, UI_COL_CREAM);
  ui_fill(x, y + 4, 6, hgt - 8, edge);
  ui_print_fit(x + 18, y + 4, title, UI_COL_PLUM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X, w - 36);
  for(j = 0; j < lines; j++)
    if(ui_wrap_line(body, j, w - 36, UI_FONT_BODY, line, sizeof(line)))
      ui_print(x + 18, y + 38 + j * 16, line, UI_COL_PLUM, UI_FONT_BODY, 0);
  h[0].keys = PSP_CTRL_SQUARE;
  h[0].verb = "";
  h[1].keys = ui_key_back;
  h[1].verb = (char *)T("Close");
  ui_hint_packed(h, 2, 14);
}

int ui_error_card(const char *title, const char *body, const char *ok, const char *cancel,
 u16 edge)
{
  const char *buttons[2];
  u32 mark = ui_pool_mark();
  u16 *under = ui_pool_alloc(UI_SCREEN_W * UI_SCREEN_H * 2);
  int f = 0, closing = 0, result = 0, y, count = cancel ? 2 : 1;
  ui_hint_t h[2];

  buttons[0] = ok;
  buttons[1] = cancel;
  h[0].keys = ui_key_confirm; h[0].verb = (char *)ok;
  h[1].keys = ui_key_back;    h[1].verb = (char *)(cancel ? cancel : T("Close"));
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
      if(ui_pressed(ui_key_confirm))
      {
        result = 1;
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
      ui_dialog(title, body, buttons, count, 0, edge, f, closing);
      ui_hint_packed(h, closing ? 0 : 2, 14);
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
  return result;
}
