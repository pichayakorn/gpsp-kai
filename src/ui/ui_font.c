/* UI text: Fusion Pixel (OFL) in kai's .fbm format, drawn into the frame.
 *
 * .fbm layout (tools/fonts/bdf2fbm.py), little-endian: a 9-byte header
 * (u16 glyph count, u16 map count, u16 default glyph, u8 cell width, u8 cell
 * height, u8 bytes per glyph), maps of (u16 first, u16 last, u16 distance)
 * where code point c in [first, last] is glyph c - distance, then per glyph
 * one advance byte and the bitmap, rows of ceil(width / 8) bytes with the
 * leftmost pixel in the top bit. */
#include "../common.h"
#include "ui.h"

typedef struct
{
  const u8 *maps;
  const u8 *glyphs;
  u16 map_count;
  u8 width, height, glyph_bytes, row_bytes;
} ui_fbm_t;

/* [size][0 = Latin, 1 = CJK] */
static ui_fbm_t fonts[2][2];
static int fonts_loaded = 0;
static u32 fonts_bytes = 0;

#define RD16(p) ((u32)(p)[0] | ((u32)(p)[1] << 8))

static int load_fbm(ui_fbm_t *font, const char *name)
{
  char path[MAX_PATH];
  SceUID fd;
  s32 size;
  u8 *data;

  memset(font, 0, sizeof(*font));
  path_printf(path, sizeof(path), "%s/font/ui/%s", main_path, name);
  fd = sceIoOpen(path, PSP_O_RDONLY, 0);
  if(fd < 0)
    return 0;
  size = sceIoLseek32(fd, 0, PSP_SEEK_END);
  sceIoLseek32(fd, 0, PSP_SEEK_SET);
  data = (size > 9) ? ui_pool_alloc(size) : NULL;
  if((data == NULL) || (sceIoRead(fd, data, size) != size))
  {
    sceIoClose(fd);
    return 0;
  }
  sceIoClose(fd);

  font->map_count = RD16(data + 2);
  font->width = data[6];
  font->height = data[7];
  font->glyph_bytes = data[8];
  font->row_bytes = (font->width + 7) / 8;
  font->maps = data + 9;
  font->glyphs = font->maps + font->map_count * 6;
  return size;
}

u32 ui_font_init(u32 language)
{
  /* kai's language order: 0 Japanese ... 9 Korean, 10 Chinese (traditional),
   * 11 Chinese (simplified). Other languages load the Japanese 12 px set
   * for game titles. */
  static const char *cjk[12] =
    { "ja", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "ko", "zh_hant", "zh_hans" };
  const char *set = (language < 12) ? cjk[language] : NULL;
  char name[32];
  u32 total = 0;

  total += load_fbm(&fonts[UI_FONT_SMALL][0], "fp10-latin.fbm");
  total += load_fbm(&fonts[UI_FONT_BODY][0], "fp12-latin.fbm");
  if(set)
  {
    sprintf(name, "fp10-%s.fbm", set);
    total += load_fbm(&fonts[UI_FONT_SMALL][1], name);
  }
  sprintf(name, "fp12-%s.fbm", set ? set : "ja");
  total += load_fbm(&fonts[UI_FONT_BODY][1], name);

  fonts_loaded = (fonts[UI_FONT_SMALL][0].maps != NULL) && (fonts[UI_FONT_BODY][0].maps != NULL);
  fonts_bytes = fonts_loaded ? total : 0;
  return fonts_bytes;
}

u32 ui_font_bytes(void)
{
  return fonts_bytes;
}

int ui_font_loaded(void)
{
  return fonts_loaded;
}

static const u8 *find_in(const ui_fbm_t *font, u32 c)
{
  int lo = 0, hi = font->map_count - 1;

  if(font->maps == NULL)
    return NULL;
  while(lo <= hi)
  {
    int mid = (lo + hi) / 2;
    const u8 *m = font->maps + mid * 6;
    if(c < RD16(m))
      hi = mid - 1;
    else if(c > RD16(m + 2))
      lo = mid + 1;
    else
      return font->glyphs + (c - RD16(m + 4)) * (1 + font->glyph_bytes);
  }
  return NULL;
}

/* The glyph for c: this size's Latin, then its CJK set, then the other
 * size's CJK set (a Latin-script menu has CJK only at 12 px). */
static const u8 *find_glyph(u32 c, int size, const ui_fbm_t **font)
{
  const u8 *g;

  if((g = find_in(&fonts[size][0], c))) { *font = &fonts[size][0]; return g; }
  if((g = find_in(&fonts[size][1], c))) { *font = &fonts[size][1]; return g; }
  if((g = find_in(&fonts[size ^ 1][1], c))) { *font = &fonts[size ^ 1][1]; return g; }
  return NULL;
}

static u32 next_utf8(const char **s)
{
  const u8 *p = (const u8 *)*s;
  u32 c = *p++;

  if(c >= 0xF0 && p[0] && p[1] && p[2])
  {
    c = ((c & 0x07) << 18) | ((p[0] & 0x3F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    p += 3;
  }
  else if(c >= 0xE0 && p[0] && p[1])
  {
    c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F);
    p += 2;
  }
  else if(c >= 0xC0 && p[0])
  {
    c = ((c & 0x1F) << 6) | (p[0] & 0x3F);
    p += 1;
  }
  *s = (const char *)p;
  return c;
}

/* Bold only where it reads as bold: Latin, Greek, Cyrillic. */
static int boldable(u32 c)
{
  return c < 0x0530;
}

static void put(int x, int y, u16 color)
{
  if((x >= ui_clip_x0) && (x < ui_clip_x1) && (y >= ui_clip_y0) && (y < ui_clip_y1))
    ui_fb[y * ui_fb_pitch + x] = color;
}

/* Draw one glyph's set pixels; scale is 1, 2 or 3. */
static void blit(const ui_fbm_t *font, const u8 *bits, int x, int y, u16 color, int scale)
{
  int j, inside = (x >= ui_clip_x0) && (y >= ui_clip_y0) && (x + font->width * scale <= ui_clip_x1) &&
   (y + font->height * scale <= ui_clip_y1);

  for(j = 0; j < font->height; j++, bits += font->row_bytes)
  {
    u32 v = bits[0] << 24;
    u16 *row;

    if(font->row_bytes > 1)
      v |= bits[1] << 16;
    if(v == 0)
      continue;
    row = ui_fb + (y + j * scale) * ui_fb_pitch + x;
    while(v)
    {
      int i = __builtin_clz(v);
      v &= ~(0x80000000u >> i);
      if(inside && (scale < 3))
      {
        if(scale == 1)
          row[i] = color;
        else
        {
          row[i * 2] = row[i * 2 + 1] = color;
          row[ui_fb_pitch + i * 2] = row[ui_fb_pitch + i * 2 + 1] = color;
        }
      }
      else
      {
        int a, b;
        for(b = 0; b < scale; b++)
          for(a = 0; a < scale; a++)
            put(x + i * scale + a, y + j * scale + b, color);
      }
    }
  }
}

/* Advance of c in pixels, drawing it when draw is set. A character the
 * fonts don't have is a hollow box. */
static int glyph(int x, int y, u32 c, u16 color, int size, int flags, int draw)
{
  const ui_fbm_t *font = NULL;
  const u8 *g = find_glyph(c, size, &font);
  int scale = (flags & UI_TEXT_3X) ? 3 : (flags & UI_TEXT_2X) ? 2 : 1;
  int bold = (flags & UI_TEXT_BOLD) && boldable(c);
  /* bold advances 1 px per glyph; the "…" is not struck (its dots would
     run into a bar) but advances the same (spec section 1, Type) */
  int wider = bold || ((flags & UI_TEXT_BOLD) && (c == 0x2026));
  int advance;

  if(g == NULL)
  {
    int h = (size == UI_FONT_BODY) ? 12 : 10;
    advance = h * 6 / 10 + 2;
    if(draw)
      ui_rect(x + scale, y + 2 * scale, (advance - 2) * scale, (h - 2) * scale, scale, color);
    return advance * scale;
  }

  advance = g[0];
  if(draw)
  {
    blit(font, g + 1, x, y, color, scale);
    if(bold)
      blit(font, g + 1, x + scale, y, color, scale);
  }
  return (advance + wider) * scale;
}

int ui_print(int x, int y, const char *s, u16 color, int font, int flags)
{
  while(*s)
    x += glyph(x, y, next_utf8(&s), color, font, flags, 1);
  return x;
}

int ui_print_width(const char *s, int font, int flags)
{
  int w = 0;

  while(*s)
    w += glyph(0, 0, next_utf8(&s), 0, font, flags, 0);
  return w;
}

/* Print, cutting the text with "…" when it is wider than max_width. */
int ui_print_fit(int x, int y, const char *s, u16 color, int font, int flags, int max_width)
{
  const char *p = s;
  int w = 0, ellipsis = glyph(0, 0, 0x2026, 0, font, flags, 0);

  if(ui_print_width(s, font, flags) <= max_width)
    return ui_print(x, y, s, color, font, flags);

  while(*p)
  {
    const char *q = p;
    int cw = glyph(0, 0, next_utf8(&q), 0, font, flags, 0);
    if(w + cw + ellipsis > max_width)
      break;
    x += glyph(x, y, next_utf8(&p), color, font, flags, 1);
    w += cw;
  }
  return x + glyph(x, y, 0x2026, color, font, flags, 1);
}
