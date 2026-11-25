#include "../common.h"
#include "ui.h"

u16 *ui_fb;
u32 ui_fb_pitch;
#define fb ui_fb
#define fb_pitch ui_fb_pitch

/* The CPU writes the menu buffer through the uncached VRAM address: going
 * through the data cache makes every write miss fill a cache line from
 * VRAM first. */
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

void ui_frame_end_now(void)
{
  flip_screen();
}

int ui_clip_x0 = 0, ui_clip_y0 = 0, ui_clip_x1 = UI_SCREEN_W, ui_clip_y1 = UI_SCREEN_H;

void ui_clip_set(int x, int y, int w, int h)
{
  ui_clip_x0 = (x > 0) ? x : 0;
  ui_clip_y0 = (y > 0) ? y : 0;
  ui_clip_x1 = (x + w < UI_SCREEN_W) ? x + w : UI_SCREEN_W;
  ui_clip_y1 = (y + h < UI_SCREEN_H) ? y + h : UI_SCREEN_H;
}

void ui_clip_reset(void)
{
  ui_clip_x0 = ui_clip_y0 = 0;
  ui_clip_x1 = UI_SCREEN_W;
  ui_clip_y1 = UI_SCREEN_H;
}

static int clip(int *x, int *y, int *w, int *h)
{
  if(*x < ui_clip_x0) { *w -= ui_clip_x0 - *x; *x = ui_clip_x0; }
  if(*y < ui_clip_y0) { *h -= ui_clip_y0 - *y; *y = ui_clip_y0; }
  if(*x + *w > ui_clip_x1) *w = ui_clip_x1 - *x;
  if(*y + *h > ui_clip_y1) *h = ui_clip_y1 - *y;
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

static u16 *ground_layer = NULL;

void ui_ground_rect(int x, int y, int w, int h)
{
  int x0 = x, y0 = y, j;

  if(!clip(&x0, &y0, &w, &h))
    return;
  for(j = y0; j < y0 + h; j = (j / UI_CHECK + 1) * UI_CHECK)
  {
    int jh = (j / UI_CHECK + 1) * UI_CHECK - j, i;
    if(j + jh > y0 + h)
      jh = y0 + h - j;
    for(i = x0; i < x0 + w; i = (i / UI_CHECK + 1) * UI_CHECK)
    {
      int iw = (i / UI_CHECK + 1) * UI_CHECK - i;
      if(i + iw > x0 + w)
        iw = x0 + w - i;
      ui_fill(i, j, iw, jh, (((i / UI_CHECK) ^ (j / UI_CHECK)) & 1) ? UI_COL_PLUM_2 : UI_COL_PLUM);
    }
  }
}

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

void ui_layer_init(void)
{
  alloc_ground_layer();
}

static u16 *target_fb;
static u32 target_fb_pitch;
static int target_clip[4];

int ui_target_begin(u16 *buf, int pitch, int w, int h)
{
  target_fb = fb;
  target_fb_pitch = fb_pitch;
  target_clip[0] = ui_clip_x0;
  target_clip[1] = ui_clip_y0;
  target_clip[2] = ui_clip_x1;
  target_clip[3] = ui_clip_y1;
  fb = buf;
  fb_pitch = pitch;
  ui_clip_x0 = ui_clip_y0 = 0;
  ui_clip_x1 = w;
  ui_clip_y1 = h;
  return 1;
}

void ui_target_end(void)
{
  fb = target_fb;
  fb_pitch = target_fb_pitch;
  ui_clip_x0 = target_clip[0];
  ui_clip_y0 = target_clip[1];
  ui_clip_x1 = target_clip[2];
  ui_clip_y1 = target_clip[3];
}

/* Between ui_layer_begin() and ui_layer_end() every ui_* drawing goes to
 * the layer. ui_layer_begin() returns 0 without pool memory. */
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
  {
    u32 sx = ((u32)sw << 16) / w, sy = ((u32)sh << 16) / h;
    u32 u0 = (x0 - x) * sx, v = (y0 - y) * sy;

    row = fb + y0 * fb_pitch;
    for(j = 0; j < ch; j++, row += fb_pitch, v += sy)
    {
      const u16 *line = src + (v >> 16) * sw;
      u16 *out = row + x0;
      u32 u = u0;
      for(i = 0; i < cw; i++, u += sx)
        out[i] = line[u >> 16];
    }
  }
}

/* A picture (sw x sh, pitch sw) shrunk into dst (w x h, w <= 160). */
void ui_shrink_box(const u16 *src, int sw, int sh, u16 *dst, int w, int h)
{
  static u16 x0[161];
  static u32 recip[17];
  int i, j;

  if(!recip[1])
    for(i = 1; i <= 16; i++)
      recip[i] = (65536 + i - 1) / i;

  if((w <= 0) || (h <= 0) || (w > 160))
    return;
  for(i = 0; i <= w; i++)
    x0[i] = i * sw / w;
  for(j = 0; j < h; j++)
  {
    int ya = j * sh / h, yb = (j + 1) * sh / h;
    if(yb > ya + 4) yb = ya + 4;
    if(yb == ya) yb = ya + 1;
    for(i = 0; i < w; i++)
    {
      int xa = x0[i], xb = x0[i + 1], x, y;
      u32 r = 0, g = 0, b = 0, n, inv;
      if(xb > xa + 4) xb = xa + 4;
      if(xb == xa) xb = xa + 1;
      for(y = ya; y < yb; y++)
        for(x = xa; x < xb; x++)
        {
          u16 p = src[y * sw + x];
          r += p & 31; g += (p >> 5) & 31; b += (p >> 10) & 31;
        }
      n = (xb - xa) * (yb - ya);
      inv = recip[n];
      dst[j * w + i] = COLOR16((r * inv) >> 16, (g * inv) >> 16, (b * inv) >> 16);
    }
  }
}

/* A picture (w x h, pitch in pixels) at x, y as it is. */
void ui_blit(const u16 *src, int pitch, int x, int y, int w, int h)
{
  int j, x0 = x, y0 = y, cw = w, ch = h;
  u16 *row;

  if(!clip(&x0, &y0, &cw, &ch))
    return;
  src += (y0 - y) * pitch + (x0 - x);
  row = fb + y0 * fb_pitch + x0;
  for(j = 0; j < ch; j++, row += fb_pitch, src += pitch)
    memcpy(row, src, cw * 2);
}

/* A picture (sw x sh, pitch a multiple of 8 pixels, in RAM) stretched to
 * w x h at x, y by the GE with bilinear filtering. Texels at the far edges
 * are not sampled (half a texel inset). */
void ui_stretch_smooth(const u16 *src, int sw, int sh, int pitch, int x, int y, int w, int h)
{
  struct { float u, v, x, y, z; } *v;
  int tw = 1, th = 1, lw = 0, lh = 0;

  while(tw < pitch) { tw <<= 1; lw++; }
  while(th < sh) { th <<= 1; lh++; }
  sceKernelDcacheWritebackRange(src, pitch * sh * 2);
  sceGuStart(GU_DIRECT, ge_list);
  sceGuDrawBufferList(GU_PSM_5551, (void *)((u32)screen_address - 0x04000000), fb_pitch);
  sceGuScissor(0, 0, UI_SCREEN_W, UI_SCREEN_H);
  sceGuTexMode(GU_PSM_5551, 0, 0, GU_FALSE);
  sceGuTexImage(0, tw, th, pitch, src);
  sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
  sceGuTexFilter(GU_LINEAR, GU_LINEAR);
  sceGuTexFlush();
  v = sceGuGetMemory(2 * sizeof(*v));
  v[0].u = 0.5f;            v[0].v = 0.5f;            v[0].x = x;     v[0].y = y;     v[0].z = 0;
  v[1].u = sw - 0.5f;       v[1].v = sh - 0.5f;       v[1].x = x + w; v[1].y = y + h; v[1].z = 0;
  sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
  sceGuDrawBufferList(GU_PSM_5551, (void *)0, 768);
  sceGuScissor(current_parameter->view.x, current_parameter->view.y,
   current_parameter->view.width, current_parameter->view.height);
  sceGuFinish();
  sceGuSync(0, 0);
  (void)lw; (void)lh;
}

void ui_rect(int x, int y, int w, int h, int t, u16 color)
{
  ui_fill(x, y, w, t, color);
  ui_fill(x, y + h - t, w, t, color);
  ui_fill(x, y + t, t, h - t * 2, color);
  ui_fill(x + w - t, y + t, t, h - t * 2, color);
}

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

#define UI_DISC 13
static const u8 glyph_cross[7]    = { 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41 };
static const u8 glyph_circle[7]   = { 0x1C, 0x22, 0x41, 0x41, 0x41, 0x22, 0x1C };
static const u8 glyph_triangle[7] = { 0x08, 0x08, 0x14, 0x14, 0x22, 0x22, 0x7F };
static const u8 glyph_square[7]   = { 0x7F, 0x41, 0x41, 0x41, 0x41, 0x41, 0x7F };

/* The UI font's capitals sit on rows 4-10 of its 14 px cell; kai's old
 * font starts higher. */
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

static u16 glyph_bg = UI_COL_CREAM, glyph_fg = UI_COL_PLUM;

int ui_glyph(int x, int y, u32 key)
{
  const u8 *symbol = key_symbol(key);
  int i, j, w = ui_glyph_width(key);

  if(symbol)
  {
    for(j = 0; j < UI_DISC; j++)
    {
      int dy = j * 2 - (UI_DISC - 1), i0 = 0;
      while(i0 < UI_DISC / 2)
      {
        int dx = i0 * 2 - (UI_DISC - 1);
        if(dx * dx + dy * dy <= UI_DISC * UI_DISC)
          break;
        i0++;
      }
      ui_fill(x + i0, y + j, UI_DISC - i0 * 2, 1, glyph_bg);
    }
    for(j = 0; j < 7; j++)
      for(i = 0; i < 7; i++)
        if(symbol[j] & (0x40 >> i))
          ui_fill(x + 3 + i, y + 3 + j, 1, 1, glyph_fg);
  }
  else
  {
    ui_fill(x + 1, y, w - 2, UI_DISC, glyph_bg);
    ui_fill(x, y + 1, 1, UI_DISC - 2, glyph_bg);
    ui_fill(x + w - 1, y + 1, 1, UI_DISC - 2, glyph_bg);
    ui_text(x + 4, small_text_top(y, UI_DISC), key_label(key), glyph_fg);
  }
  return x + w;
}

int ui_glyph_inverted(int x, int y, u32 key)
{
  int r;

  glyph_bg = UI_COL_PLUM;
  glyph_fg = UI_COL_CREAM;
  r = ui_glyph(x, y, key);
  glyph_bg = UI_COL_CREAM;
  glyph_fg = UI_COL_PLUM;
  return r;
}

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

static int hint_packed_one(int x, int y, const ui_hint_t *h, int gap)
{
  static const u32 pairs[2] = { PSP_CTRL_UP | PSP_CTRL_DOWN, PSP_CTRL_LEFT | PSP_CTRL_RIGHT };
  u32 keys = h->keys, bit;
  int j;

  for(j = 0; j < 2; j++)
    if((keys & pairs[j]) == pairs[j])
    {
      x = ui_glyph(x, y, pairs[j]) + 4;
      keys &= ~pairs[j];
    }
  for(bit = 1; bit; bit <<= 1)
    if(keys & bit)
      x = ui_glyph(x, y, bit) + 4;
  if(h->verb && h->verb[0])
    x = ui_text(x, small_text_top(y, UI_DISC), h->verb, UI_COL_CREAM) + gap;
  return x;
}

void ui_hint_packed(const ui_hint_t *hints, int count, int gap)
{
  int n, x = UI_MARGIN, y = UI_STRIP_Y + (UI_STRIP_H - UI_DISC) / 2;

  ui_fill(0, UI_STRIP_Y, UI_SCREEN_W, UI_STRIP_H, UI_COL_SHADE);
  for(n = 0; n < count; n++)
    x = hint_packed_one(x, y, &hints[n], gap);
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

int ui_ease_out_cubic(int a, int b, int f, int n)
{
  int u;

  if(n == 8)
    return ease(UI_EASE_OUT_CUBIC_8, a, b, f, n);
  if(n == 10)
    return ease(UI_EASE_OUT_CUBIC_10, a, b, f, n);
  if(f >= n)
    return b;
  if(f <= 0)
    return a;
  u = n - f;
  return a + (int)((s64)(b - a) * (256 - 256 * u * u * u / (n * n * n)) >> 8);
}

int ui_ease_in_cubic(int a, int b, int f, int n)
{
  return ease((n == 8) ? UI_EASE_IN_CUBIC_8 : (n == 10) ? UI_EASE_IN_CUBIC_10 : NULL,
   a, b, f, n);
}
