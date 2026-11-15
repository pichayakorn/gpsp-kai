/* UI drawing: CPU writes into the 480 x 272 menu buffer. */
#include "../common.h"
#include "ui.h"

u16 *ui_fb;
u32 ui_fb_pitch;
#define fb ui_fb
#define fb_pitch ui_fb_pitch

/* The CPU writes the menu buffer through the uncached VRAM address: going
 * through the data cache makes every write miss fill a cache line from
 * VRAM first, which made a plain copy run at about 80 MB/s at 222 MHz. */
#define UNCACHED(p) ((u16 *)((u32)(p) | 0x40000000))

static u32 __attribute__((aligned(16))) ge_list[64];

void ui_frame_begin(void)
{
  fb = UNCACHED(screen_address);
  fb_pitch = screen_pitch;
}

void ui_frame_end(void)
{
  flip_screen();
  sceDisplayWaitVblankStart();
}

/* Show the frame without waiting for the vblank: a bar redrawn between
 * chunks of a save must not slow the save down. */
void ui_frame_end_now(void)
{
  flip_screen();
}

static int clip(int *x, int *y, int *w, int *h)
{
  if(*x < 0) { *w += *x; *x = 0; }
  if(*y < 0) { *h += *y; *y = 0; }
  if(*x + *w > UI_SCREEN_W) *w = UI_SCREEN_W - *x;
  if(*y + *h > UI_SCREEN_H) *h = UI_SCREEN_H - *y;
  return (*w > 0) && (*h > 0);
}

void ui_fill(int x, int y, int w, int h, u16 color)
{
  int i;
  u16 *row;

  if(!clip(&x, &y, &w, &h))
    return;
  row = fb + y * fb_pitch + x;
  while(h--)
  {
    for(i = 0; i < w; i++)
      row[i] = color;
    row += fb_pitch;
  }
}

/* The ground: plum with an 8 px check in plum-2, kept as a 480 x 272 layer
 * in the UI pool and copied into the frame by the GE, so the CPU spends
 * almost nothing on it. Static parts of a screen can be drawn into the
 * layer once. Without pool memory it is drawn by the CPU from two rows. */
static u16 *ground_layer = NULL;

static void build_ground(u16 *dest, u32 pitch)
{
  int x, y;
  u16 rows[2][UI_SCREEN_W];

  for(x = 0; x < UI_SCREEN_W; x++)
  {
    int odd = (x / UI_CHECK) & 1;
    rows[0][x] = odd ? UI_COL_PLUM_2 : UI_COL_PLUM;
    rows[1][x] = odd ? UI_COL_PLUM : UI_COL_PLUM_2;
  }
  for(y = 0; y < UI_SCREEN_H; y++)
    memcpy(dest + y * pitch, rows[(y / UI_CHECK) & 1], UI_SCREEN_W * 2);
}

static int alloc_ground_layer(void)
{
  if(ground_layer == NULL)
  {
    ground_layer = ui_pool_alloc(UI_SCREEN_W * UI_SCREEN_H * 2);
    if(ground_layer == NULL)
      return 0;
    build_ground(ground_layer, UI_SCREEN_W);
    sceKernelDcacheWritebackRange(ground_layer, UI_SCREEN_W * UI_SCREEN_H * 2);
  }
  return 1;
}

/* Draw the static parts of a screen into the ground layer once: between
 * ui_layer_begin() and ui_layer_end() every ui_* drawing goes to the layer.
 * ui_layer_begin() starts from the plain ground and returns 0 without
 * pool memory (then draw them every frame instead). */
static u16 *saved_fb;
static u32 saved_fb_pitch;

int ui_layer_begin(void)
{
  if(!alloc_ground_layer())
    return 0;
  build_ground(ground_layer, UI_SCREEN_W);
  saved_fb = fb;
  saved_fb_pitch = fb_pitch;
  fb = ground_layer;
  fb_pitch = UI_SCREEN_W;
  return 1;
}

void ui_layer_end(void)
{
  sceKernelDcacheWritebackRange(ground_layer, UI_SCREEN_W * UI_SCREEN_H * 2);
  fb = saved_fb;
  fb_pitch = saved_fb_pitch;
}

int ui_layer_ready(void)
{
  return ground_layer != NULL;
}

/* Back to the plain ground, for the next screen. */
void ui_layer_reset(void)
{
  if(ground_layer == NULL)
    return;
  build_ground(ground_layer, UI_SCREEN_W);
  sceKernelDcacheWritebackRange(ground_layer, UI_SCREEN_W * UI_SCREEN_H * 2);
}

void ui_ground(void)
{
  if(!alloc_ground_layer())
  {
    build_ground(fb, fb_pitch);
    return;
  }

  sceGuStart(GU_DIRECT, ge_list);
  sceGuCopyImage(GU_PSM_5551, 0, 0, UI_SCREEN_W, UI_SCREEN_H, UI_SCREEN_W,
   ground_layer, 0, 0, fb_pitch, screen_address);
  sceGuTexSync();
  sceGuFinish();
  sceGuSync(0, 0);
}

/* Every other pixel in one colour: the screen door (shade over the game, a
 * dim without alpha) and the checker of an empty savestate. */
void ui_checker(int x, int y, int w, int h, u16 color)
{
  int i, j;
  u16 *row;

  if(!clip(&x, &y, &w, &h))
    return;
  row = fb + y * fb_pitch;
  for(j = 0; j < h; j++, row += fb_pitch)
    for(i = x + ((x + y + j) & 1); i < x + w; i += 2)
      row[i] = color;
}

void ui_door(int x, int y, int w, int h)
{
  ui_checker(x, y, w, h, UI_COL_SHADE);
}

/* A picture (sw x sh, pitch sw) scaled to w x h at x, y, nearest pixel. */
void ui_blit_scaled(const u16 *src, int sw, int sh, int x, int y, int w, int h)
{
  int i, j, x0 = x, y0 = y, cw = w, ch = h;
  u16 *row;

  if((w <= 0) || (h <= 0) || !clip(&x0, &y0, &cw, &ch))
    return;
  row = fb + y0 * fb_pitch;
  for(j = y0; j < y0 + ch; j++, row += fb_pitch)
  {
    const u16 *line = src + ((j - y) * sh / h) * sw;
    for(i = x0; i < x0 + cw; i++)
      row[i] = line[(i - x) * sw / w];
  }
}

void ui_rect(int x, int y, int w, int h, int t, u16 color)
{
  ui_fill(x, y, w, t, color);
  ui_fill(x, y + h - t, w, t, color);
  ui_fill(x, y + t, t, h - t * 2, color);
  ui_fill(x + w - t, y + t, t, h - t * 2, color);
}

/* A card; a lifted one moves up-left over a hard shadow. */
void ui_card(int x, int y, int w, int h, u16 color, int lifted)
{
  if(lifted)
  {
    ui_fill(x - UI_LIFT + UI_SHADOW, y - UI_LIFT + UI_SHADOW, w, h, UI_COL_SHADE);
    x -= UI_LIFT;
    y -= UI_LIFT;
  }
  ui_fill(x, y, w, h, color);
}

/* Small text: the UI font when it is loaded, else kai's old font. */
int ui_text(int x, int y, char *str, u16 color)
{
  if(ui_font_loaded())
    return ui_print(x, y, str, color, UI_FONT_SMALL, 0);
  fbm_printVRAM(fb, fb_pitch, x, y, str, color, 0, FBM_FONT_FILL);
  return x + fbm_getwidth(str);
}

int ui_text_width(char *str)
{
  if(ui_font_loaded())
    return ui_print_width(str, UI_FONT_SMALL, 0);
  return fbm_getwidth(str);
}

/* Button glyphs: × ○ △ □ as 13 px cream discs with the symbol in plum;
 * other keys as cream pills with plum text. */
#define UI_DISC 13
static const u8 glyph_cross[7]    = { 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41 };
static const u8 glyph_circle[7]   = { 0x1C, 0x22, 0x41, 0x41, 0x41, 0x22, 0x1C };
static const u8 glyph_triangle[7] = { 0x08, 0x08, 0x14, 0x14, 0x22, 0x22, 0x7F };
static const u8 glyph_square[7]   = { 0x7F, 0x41, 0x41, 0x41, 0x41, 0x41, 0x7F };

/* Top of a small text line centred in a box: the UI font's capitals sit
 * on rows 4-10 of its 14 px cell; kai's old font starts higher. */
static int small_text_top(int box_y, int box_h)
{
  return ui_font_loaded() ? box_y + box_h / 2 - 7 : box_y + 2;
}

static char *key_label(u32 key)
{
  switch(key)
  {
    case PSP_CTRL_LTRIGGER: return "L";
    case PSP_CTRL_RTRIGGER: return "R";
    case PSP_CTRL_START:    return "START";
    case PSP_CTRL_SELECT:   return "SELECT";
    case PSP_CTRL_HOME:     return "HOME";
    /* the d-pad: a pair shares one pill */
    case PSP_CTRL_UP:       return "\xE2\x86\x91";
    case PSP_CTRL_DOWN:     return "\xE2\x86\x93";
    case PSP_CTRL_LEFT:     return "\xE2\x86\x90";
    case PSP_CTRL_RIGHT:    return "\xE2\x86\x92";
    case PSP_CTRL_UP | PSP_CTRL_DOWN:    return "\xE2\x86\x91\xE2\x86\x93";
    case PSP_CTRL_LEFT | PSP_CTRL_RIGHT: return "\xE2\x86\x90\xE2\x86\x92";
    default:                return "?";
  }
}

static const u8 *key_symbol(u32 key)
{
  switch(key)
  {
    case PSP_CTRL_CROSS:    return glyph_cross;
    case PSP_CTRL_CIRCLE:   return glyph_circle;
    case PSP_CTRL_TRIANGLE: return glyph_triangle;
    case PSP_CTRL_SQUARE:   return glyph_square;
    default:                return NULL;
  }
}

int ui_glyph_width(u32 key)
{
  return key_symbol(key) ? UI_DISC : ui_text_width(key_label(key)) + 8;
}

int ui_glyph(int x, int y, u32 key)
{
  const u8 *symbol = key_symbol(key);
  int i, j, w = ui_glyph_width(key);

  if(symbol)
  {
    for(j = 0; j < UI_DISC; j++)
      for(i = 0; i < UI_DISC; i++)
      {
        int dx = i * 2 - (UI_DISC - 1), dy = j * 2 - (UI_DISC - 1);
        if(dx * dx + dy * dy <= UI_DISC * UI_DISC)
          ui_fill(x + i, y + j, 1, 1, UI_COL_CREAM);
      }
    for(j = 0; j < 7; j++)
      for(i = 0; i < 7; i++)
        if(symbol[j] & (0x40 >> i))
          ui_fill(x + 3 + i, y + 3 + j, 1, 1, UI_COL_PLUM);
  }
  else
  {
    ui_fill(x + 1, y, w - 2, UI_DISC, UI_COL_CREAM);
    ui_fill(x, y + 1, 1, UI_DISC - 2, UI_COL_CREAM);
    ui_fill(x + w - 1, y + 1, 1, UI_DISC - 2, UI_COL_CREAM);
    ui_text(x + 4, small_text_top(y, UI_DISC), key_label(key), UI_COL_PLUM);
  }
  return x + w;
}

/* Hint strip: glyph + verb, 14 px apart; keys that share a verb sit
 * together. */
void ui_hint_strip(const ui_hint_t *hints, int count)
{
  int n, x = UI_MARGIN, y = UI_STRIP_Y + (UI_STRIP_H - UI_DISC) / 2;
  u32 bit;

  ui_fill(0, UI_STRIP_Y, UI_SCREEN_W, UI_STRIP_H, UI_COL_SHADE);
  for(n = 0; n < count; n++)
  {
    u32 keys = hints[n].keys;
    static const u32 pairs[2] = { PSP_CTRL_UP | PSP_CTRL_DOWN, PSP_CTRL_LEFT | PSP_CTRL_RIGHT };
    int j;

    for(j = 0; j < 2; j++)
      if((keys & pairs[j]) == pairs[j])
      {
        x = ui_glyph(x, y, pairs[j]) + 2;
        keys &= ~pairs[j];
      }
    for(bit = 1; bit; bit <<= 1)
      if(keys & bit)
        x = ui_glyph(x, y, bit) + 2;
    x = ui_text(x + 2, small_text_top(y, UI_DISC), hints[n].verb, UI_COL_CREAM) + 14;
  }
}

/* One hint for the column strip: glyphs 4 px apart, the verb 4 px after
 * them and 10 px after the verb (spec: 10 px on the pause screen). With
 * draw 0 it only measures. */
static int hint_column_one(int x, int y, const ui_hint_t *h, int draw)
{
  static const u32 pairs[2] = { PSP_CTRL_UP | PSP_CTRL_DOWN, PSP_CTRL_LEFT | PSP_CTRL_RIGHT };
  u32 keys = h->keys, bit;
  int j;

  for(j = 0; j < 2; j++)
    if((keys & pairs[j]) == pairs[j])
    {
      x = (draw ? ui_glyph(x, y, pairs[j]) : x + ui_glyph_width(pairs[j])) + 4;
      keys &= ~pairs[j];
    }
  for(bit = 1; bit; bit <<= 1)
    if(keys & bit)
      x = (draw ? ui_glyph(x, y, bit) : x + ui_glyph_width(bit)) + 4;
  if(h->verb && h->verb[0])
    x = (draw ? ui_text(x, small_text_top(y, UI_DISC), h->verb, UI_COL_CREAM) :
     x + ui_text_width(h->verb)) + 10;
  return x;
}

void ui_hint_columns(const ui_hint_col_t *cols, int count)
{
  int n, k, x = UI_MARGIN, y = UI_STRIP_Y + (UI_STRIP_H - UI_DISC) / 2;

  ui_fill(0, UI_STRIP_Y, UI_SCREEN_W, UI_STRIP_H, UI_COL_SHADE);
  for(n = 0; n < count; n++)
  {
    int w = 0;

    for(k = 0; k < cols[n].count; k++)
    {
      int vw = hint_column_one(0, y, &cols[n].variants[k], 0);
      if(vw > w)
        w = vw;
    }
    if((cols[n].shown >= 0) && (cols[n].shown < cols[n].count))
      hint_column_one(x, y, &cols[n].variants[cols[n].shown], 1);
    x += w;
  }
}

static const unsigned short *out_back_table(int n)
{
  switch(n)
  {
    case 8:  return UI_EASE_OUT_BACK_8;
    case 10: return UI_EASE_OUT_BACK_10;
    case 12: return UI_EASE_OUT_BACK_12;
    case 14: return UI_EASE_OUT_BACK_14;
    case 20: return UI_EASE_OUT_BACK_20;
    default: return NULL;
  }
}

static int ease(const unsigned short *table, int a, int b, int f, int n)
{
  if(f >= n)
    return b;
  if(f <= 0)
    return a;
  if(table == NULL)
    return a + (b - a) * f / n;
  return a + (((b - a) * (int)table[f - 1]) >> 8);
}

int ui_ease_out_back(int a, int b, int f, int n)
{
  return ease(out_back_table(n), a, b, f, n);
}

int ui_ease_in_cubic(int a, int b, int f, int n)
{
  return ease((n == 8) ? UI_EASE_IN_CUBIC_8 : (n == 10) ? UI_EASE_IN_CUBIC_10 : NULL,
   a, b, f, n);
}
