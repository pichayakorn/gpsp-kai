/* UI drawing: CPU writes into the 480 x 272 menu buffer. */
#include "../common.h"
#include "ui.h"

static u16 *fb;
static u32 fb_pitch;

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

void ui_ground(void)
{
  if(ground_layer == NULL)
  {
    ground_layer = ui_pool_alloc(UI_SCREEN_W * UI_SCREEN_H * 2);
    if(ground_layer == NULL)
    {
      build_ground(fb, fb_pitch);
      return;
    }
    build_ground(ground_layer, UI_SCREEN_W);
    sceKernelDcacheWritebackRange(ground_layer, UI_SCREEN_W * UI_SCREEN_H * 2);
  }

  sceGuStart(GU_DIRECT, ge_list);
  sceGuCopyImage(GU_PSM_5551, 0, 0, UI_SCREEN_W, UI_SCREEN_H, UI_SCREEN_W,
   ground_layer, 0, 0, fb_pitch, screen_address);
  sceGuTexSync();
  sceGuFinish();
  sceGuSync(0, 0);
}

/* Screen door: every other pixel in shade, a dim without alpha. */
void ui_door(int x, int y, int w, int h)
{
  int i, j;
  u16 *row;

  if(!clip(&x, &y, &w, &h))
    return;
  row = fb + y * fb_pitch;
  for(j = 0; j < h; j++, row += fb_pitch)
    for(i = x + ((x + y + j) & 1); i < x + w; i += 2)
      row[i] = UI_COL_SHADE;
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

int ui_text(int x, int y, char *str, u16 color)
{
  fbm_printVRAM(fb, fb_pitch, x, y, str, color, 0, FBM_FONT_FILL);
  return x + fbm_getwidth(str);
}

int ui_text_width(char *str)
{
  return fbm_getwidth(str);
}

/* Button glyphs: × ○ △ □ as 13 px cream discs with the symbol in plum;
 * other keys as cream pills with plum text. */
#define UI_DISC 13
static const u8 glyph_cross[7]    = { 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41 };
static const u8 glyph_circle[7]   = { 0x1C, 0x22, 0x41, 0x41, 0x41, 0x22, 0x1C };
static const u8 glyph_triangle[7] = { 0x08, 0x08, 0x14, 0x14, 0x22, 0x22, 0x7F };
static const u8 glyph_square[7]   = { 0x7F, 0x41, 0x41, 0x41, 0x41, 0x41, 0x7F };

static char *key_label(u32 key)
{
  switch(key)
  {
    case PSP_CTRL_LTRIGGER: return "L";
    case PSP_CTRL_RTRIGGER: return "R";
    case PSP_CTRL_START:    return "START";
    case PSP_CTRL_SELECT:   return "SELECT";
    case PSP_CTRL_HOME:     return "HOME";
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
    ui_text(x + 4, y + 2, key_label(key), UI_COL_PLUM);
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
    for(bit = 1; bit; bit <<= 1)
      if(hints[n].keys & bit)
        x = ui_glyph(x, y, bit) + 2;
    x = ui_text(x + 2, y + 2, hints[n].verb, UI_COL_CREAM) + 14;
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
