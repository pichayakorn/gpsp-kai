/* UI components (spec section 2): card rows and their values, tabs,
 * toast, hold bar and dialog card. Each one draws a single frame from the
 * state it is given; the screen keeps the state and the frame counters.
 * Sizes follow the reference drawings in gpsp-kai-design app-ui/scripts
 * (r2_toybox.py, r3_toybox.py, r4_ux.py, round5_details.py). */
#include "../common.h"
#include "ui.h"

/* Text helpers: the UI font, or kai's old font when it is missing. */
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

/* The cursor frame: 2 px of cream around the selection. */
void ui_cursor_frame(int x, int y, int w, int h)
{
  ui_round_frame(x - 3, y - 3, w + 6, h + 6, 3, UI_COL_CREAM);
  ui_round_frame(x - 2, y - 2, w + 4, h + 4, 2, UI_COL_CREAM);
}

/* A lifted card: up-left by UI_LIFT over a hard shadow. */
static void lifted_card(int x, int y, int w, int h, u16 color, int r, int lift, int shadow)
{
  if(lift)
    ui_round_fill(x + shadow - lift, y + shadow - lift, w, h, r, UI_COL_SHADE);
  ui_round_fill(x - lift, y - lift, w, h, r, color);
}

/* 11 x 11 icons, one bit per pixel, MSB = left. */
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

/* Card row: a 20 px card with the label; selected rows take the group
 * colour and lift. Returns where the value goes: its right edge in *vx and
 * the row's top (lifted) in *vy. */
void ui_card_row(int x, int y, int w, const char *label, int selected, u16 group,
 int flags, int *vx, int *vy)
{
  int lift = selected ? UI_LIFT : 0;
  u16 tc = selected ? UI_COL_PLUM : UI_COL_CREAM;
  int end;

  if(flags & UI_ROW_DISABLED)
  {
    ui_round_fill(x, y, w, UI_ROW_H, 2, UI_COL_PLUM_2);
    body_text(x + 8, y + 2, label, UI_COL_MUTE, 0);
    *vx = x + w - 8;
    *vy = y;
    return;
  }

  lifted_card(x, y, w, UI_ROW_H, selected ? group : UI_COL_CARD, 2, lift, UI_SHADOW);
  if(flags & UI_ROW_CHANGED)  /* not the default: a dot in the margin */
    ui_fill(x - 9, y + 8, 4, 4, UI_COL_TANGERINE);
  x -= lift;
  y -= lift;
  end = body_text(x + 8, y + 2, label, tc, 0);
  /* Saved for this game only: the cartridge, its ink 6 px after the label
   * and its 7 rows of ink (bitmap rows 1-7) centred on the capitals, which
   * sit on rows 4-12 of the 12 px font's cell (CJK ink is centred there
   * too). The label's cell starts at y + 2. */
  if(flags & UI_ROW_PER_GAME)
    draw_icon(end + 5, y + 2 + 4, icon_rom, selected ? UI_COL_PLUM : UI_COL_TANGERINE);
  *vx = x + w - 8;
  *vy = y;
}

/* Disabled row's note, right aligned. */
void ui_row_note(int xr, int y, const char *note)
{
  small_text(xr - small_width(note), y + 3, note, UI_COL_MUTE);
}

/* ◀ value ▶, right aligned. A new value slides in from the right over
 * UI_F_VALUE frames (f = frames since the change; f >= UI_F_VALUE = still). */
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

/* Segments: the chosen one filled; on a change the fill springs from the
 * old segment to the new one. */
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
    widths[j] = small_width(labels[j]) + 12;
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
    small_text(starts[j] + 6, y + 3 + ((ui_font_loaded()) ? 0 : 3), labels[j], col);
  }
}

/* Switch, 28 x 14: off = mute outline, on = lime with the knob right.
 * p = knob position in 1/256 (0 off .. 256 on; out-back overshoots). */
void ui_row_switch(int xr, int y, int p, int selected)
{
  int x = xr - 28, on = p >= 128, kx;

  y += 3;
  if(on)
    ui_round_fill(x, y, 28, 14, 3, selected ? UI_COL_PLUM : UI_COL_LIME);
  else
    ui_round_frame(x, y, 28, 14, 3, selected ? UI_COL_PLUM : UI_COL_MUTE);
  if(p < 0) p = 0;
  if(p > 294) p = 294;  /* 1.15 */
  kx = x + 2 + (14 * p + 128) / 256;
  ui_round_fill(kx, y + 2, 10, 10, 2, (on || !selected) ? UI_COL_CREAM : UI_COL_PLUM);
}

/* The switch knob over UI_F_VALUE frames after a flip to state on. */
int ui_switch_pos(int on, int f)
{
  return on ? ui_ease_out_back(0, 256, f, UI_F_VALUE)
            : ui_ease_out_back(256, 0, f, UI_F_VALUE);
}

/* Blocks: n blocks of 8 x 12, 2 px apart, 0..k filled, with the text left
 * of them. The block that just changed pops up from the bottom. */
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

/* "▶" for a row that opens something. */
void ui_row_action(int xr, int y, int selected)
{
  body_text(xr - body_width("\xE2\x96\xB6", 0), y + 2, "\xE2\x96\xB6",
   selected ? UI_COL_PLUM : UI_COL_CREAM, 0);
}

/* Tabs between L and R glyphs; the cream pill slides from tab prev to k
 * over UI_F_TAB frames. Returns the right edge. */
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

/* Toast: slides in from the right (UI_F_TOAST_IN, out-back), stays
 * UI_F_TOAST_HOLD, slides out (UI_F_TOAST_OUT, in-cubic); f = frames since
 * it was shown. With an undo label it carries "SELECT Undo". Returns 0
 * once it is gone. */
int ui_toast(const char *text, u16 edge, const char *undo, int f)
{
  int w, x, y = undo ? 214 : 212, right = undo ? 466 : 460, out;

  if(f < 0)
    return 0;
  /* text, SELECT pill and the undo word, with 12 px either side */
  w = body_width(text, 0) + (undo ? small_width(undo) + ui_glyph_width(PSP_CTRL_SELECT) + 40 : 28);
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
  ui_round_fill(x, y, w, 26, 2, UI_COL_CARD);
  ui_fill(x, y + 3, 4, 20, edge);
  if(undo)
  {
    int ex = body_text(x + 12, y + 5, text, UI_COL_CREAM, 0);
    int gx = ui_glyph(ex + 10, y + 7, PSP_CTRL_SELECT) + 4;
    small_text(gx, y + 6, undo, UI_COL_LIME);
  }
  else
    body_text(x + 14, y + 5, text, UI_COL_CREAM, 0);
  return 1;
}

/* Is the toast still up (Undo works while it is)? */
int ui_toast_up(int f)
{
  return (f >= 0) && (f < UI_F_TOAST_IN + UI_F_TOAST_HOLD + UI_F_TOAST_OUT);
}

/* Hold bar: 6 px, filled to p/256 in color over plum-2. */
void ui_hold_bar(int x, int y, int w, int p, u16 color)
{
  ui_fill(x, y, w, 6, UI_COL_PLUM_2);
  if(p > 256) p = 256;
  if(p > 0)
    ui_fill(x, y, w * p / 256, 6, color);
}

/* Hold progress in 1/256: fills while the key is down (UI_F_HOLD frames),
 * drains in UI_F_HOLD_DRAIN after an early release. Call once a frame. */
int ui_hold_step(ui_hold_t *h, u32 key)
{
  u32 frames = ui_hold_frames(key);

  h->done = 0;
  if(ui_down(key))
  {
    if(frames >= UI_F_HOLD)
    {
      if(!h->fired)
      {
        h->fired = 1;
        h->done = 1;
      }
      h->p = 256;
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
  }
  return h->p;
}

/* Word wrap: copies line n of text (fitting max_width) into out. Breaks at
 * spaces, or anywhere in text without spaces (CJK). Returns 0 past the end. */
static int wrap_line(const char *text, int n, int max_width, char *out, int out_size)
{
  const char *p = text;
  int line = 0;

  while(*p)
  {
    const char *q = p, *last_space = NULL, *end = p;
    char buf[256];

    while(*q == ' ')
      q++;
    p = q;
    /* grow the line one UTF-8 character at a time */
    while(*q && (*q != '\n'))
    {
      const char *next = q + 1;
      int len;
      while((*next & 0xC0) == 0x80)
        next++;
      len = next - p;
      if(len >= (int)sizeof(buf))
        break;
      memcpy(buf, p, len);
      buf[len] = 0;
      if(body_width(buf, 0) > max_width)
        break;
      if(*q == ' ')
        last_space = q;
      q = next;
      end = q;
    }
    if(*q && (*q != '\n') && last_space)
      end = last_space;
    if(end == p)  /* one character wider than the line */
    {
      end = p + 1;
      while((*end & 0xC0) == 0x80)
        end++;
    }
    if(line == n)
    {
      int len = end - p;
      if(len >= out_size)
        len = out_size - 1;
      memcpy(out, p, len);
      out[len] = 0;
      return 1;
    }
    line++;
    p = end;
    if(*p == '\n')
      p++;
  }
  return 0;
}

/* Dialog card, only for errors and questions that need one (spec section
 * 4): 360 x 132 in cream with a group-colour edge, a 2x title, up to three
 * lines of body and one or two buttons (the selected one filled). It drops
 * in over UI_F_DIALOG_IN frames (f) and leaves over UI_F_DIALOG_OUT when
 * closing. */
void ui_dialog(const char *title, const char *body, const char **buttons,
 int button_count, int selected, u16 edge, int f, int closing)
{
  int w = 360, h = 132, x = (UI_SCREEN_W - w) / 2, y, j, bx;
  char line[256];

  ui_door(0, 0, UI_SCREEN_W, UI_SCREEN_H);
  if(closing)
    y = 66 + ui_ease_in_cubic(0, 24, f, UI_F_DIALOG_OUT);
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
    if(wrap_line(body, j, w - 36, line, sizeof(line)))
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
