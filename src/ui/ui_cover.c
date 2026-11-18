/* ui_cover.c - preset covers (spec section 5, set B "Abstract")
 *
 * Every game without box art gets a cover drawn from its title, so a game
 * keeps its cover between runs and two games rarely look alike:
 *   seed   = crc32 of the lower-cased UTF-8 title
 *   colour = pair (seed >> 8) % 12, swapped when bit 28 is set
 *   motif  = (seed >> 16) % 12, mirrored when bit 29 is set
 * From 32 px up the title sits on a dark band at the bottom; the caller
 * draws the title text (ui_cover_band tells it where).
 *
 * Plain C over a 16-bit buffer and zlib's crc32, nothing else, so the same
 * file builds on a PC to compare its pictures with the design's.
 */

#include <string.h>
#include <zlib.h>
#include "ui_cover.h"

/* bg, figure, the figure at 45 % over bg, bg darkened 45 %, the title band
   (bg darkened 55 %); the second twelve are the first swapped */
static const u16 duos[24][5] =
{
  { COLOR16(6, 10, 20), COLOR16(28, 20, 8), COLOR16(16, 14, 15), COLOR16(3, 6, 11), COLOR16(3, 4, 9) },
  { COLOR16(20, 6, 8), COLOR16(31, 27, 18), COLOR16(25, 15, 12), COLOR16(11, 3, 4), COLOR16(9, 3, 4) },
  { COLOR16(4, 16, 12), COLOR16(26, 30, 12), COLOR16(14, 22, 12), COLOR16(2, 9, 7), COLOR16(2, 7, 5) },
  { COLOR16(14, 6, 20), COLOR16(31, 16, 22), COLOR16(22, 10, 21), COLOR16(8, 3, 11), COLOR16(6, 3, 9) },
  { COLOR16(24, 14, 4), COLOR16(8, 4, 2), COLOR16(17, 10, 3), COLOR16(13, 8, 2), COLOR16(11, 6, 2) },
  { COLOR16(8, 18, 26), COLOR16(30, 30, 28), COLOR16(18, 23, 27), COLOR16(4, 10, 14), COLOR16(4, 8, 12) },
  { COLOR16(26, 24, 18), COLOR16(6, 8, 14), COLOR16(17, 17, 16), COLOR16(14, 13, 10), COLOR16(12, 11, 8) },
  { COLOR16(4, 6, 8), COLOR16(12, 28, 30), COLOR16(8, 16, 18), COLOR16(2, 3, 4), COLOR16(2, 3, 4) },
  { COLOR16(28, 10, 6), COLOR16(30, 26, 12), COLOR16(29, 17, 9), COLOR16(15, 6, 3), COLOR16(13, 4, 3) },
  { COLOR16(3, 12, 18), COLOR16(31, 13, 16), COLOR16(16, 12, 17), COLOR16(2, 7, 10), COLOR16(1, 5, 8) },
  { COLOR16(22, 26, 10), COLOR16(5, 10, 8), COLOR16(14, 19, 9), COLOR16(12, 14, 6), COLOR16(10, 12, 4) },
  { COLOR16(12, 4, 10), COLOR16(12, 26, 20), COLOR16(12, 14, 14), COLOR16(7, 2, 6), COLOR16(5, 2, 4) },
  { COLOR16(28, 20, 8), COLOR16(6, 10, 20), COLOR16(18, 16, 13), COLOR16(15, 11, 4), COLOR16(13, 9, 4) },
  { COLOR16(31, 27, 18), COLOR16(20, 6, 8), COLOR16(26, 18, 14), COLOR16(17, 15, 10), COLOR16(14, 12, 8) },
  { COLOR16(26, 30, 12), COLOR16(4, 16, 12), COLOR16(16, 24, 12), COLOR16(14, 16, 7), COLOR16(12, 14, 5) },
  { COLOR16(31, 16, 22), COLOR16(14, 6, 20), COLOR16(23, 12, 21), COLOR16(17, 9, 12), COLOR16(14, 7, 10) },
  { COLOR16(8, 4, 2), COLOR16(24, 14, 4), COLOR16(15, 8, 3), COLOR16(4, 2, 1), COLOR16(4, 2, 1) },
  { COLOR16(30, 30, 28), COLOR16(8, 18, 26), COLOR16(20, 25, 27), COLOR16(16, 16, 15), COLOR16(14, 14, 13) },
  { COLOR16(6, 8, 14), COLOR16(26, 24, 18), COLOR16(15, 15, 16), COLOR16(3, 4, 8), COLOR16(3, 4, 6) },
  { COLOR16(12, 28, 30), COLOR16(4, 6, 8), COLOR16(8, 18, 20), COLOR16(7, 15, 16), COLOR16(5, 13, 14) },
  { COLOR16(30, 26, 12), COLOR16(28, 10, 6), COLOR16(29, 19, 9), COLOR16(16, 14, 7), COLOR16(14, 12, 5) },
  { COLOR16(31, 13, 16), COLOR16(3, 12, 18), COLOR16(18, 13, 17), COLOR16(17, 7, 9), COLOR16(14, 6, 7) },
  { COLOR16(5, 10, 8), COLOR16(22, 26, 10), COLOR16(13, 17, 9), COLOR16(3, 6, 4), COLOR16(2, 4, 4) },
  { COLOR16(12, 26, 20), COLOR16(12, 4, 10), COLOR16(12, 16, 16), COLOR16(7, 14, 11), COLOR16(5, 12, 9) },
};

enum { HORIZON, SLASH, PEAKS, TARGET, SPLIT, DOTS, ARCS, SUNSET, ORBIT,
 BLOCKS, ZIGZAG, TILES };

/* the canvas being drawn: a size x size square */
static u16 *cv;
static int cv_pitch, cv_size;

/* v at this size: v * size / 64, rounded half to even as the design's
   Python does */
static int px(int v)
{
  int n = v * cv_size, q, r;

  if(n < 0)
    return -px(-v);
  q = n / 64;
  r = n % 64;
  if((r > 32) || ((r == 32) && (q & 1)))
    q++;
  return q;
}

static void plot_span(int y, int x0, int x1, u16 c)
{
  u16 *row;

  if((y < 0) || (y >= cv_size))
    return;
  if(x0 < 0)
    x0 = 0;
  if(x1 > cv_size - 1)
    x1 = cv_size - 1;
  row = cv + y * cv_pitch;
  for(; x0 <= x1; x0++)
    row[x0] = c;
}

/* a filled rectangle, both corners inside */
static void rect(int x0, int y0, int x1, int y1, u16 c)
{
  for(; y0 <= y1; y0++)
    plot_span(y0, x0, x1, c);
}

/* Is pixel (x, y) inside the ellipse in the box x0..x1, y0..y1 (both ends
   inside)? Tested at the pixel's centre, in doubled coordinates. */
static int in_ellipse(int x, int y, int x0, int y0, int x1, int y1)
{
  long long a = x1 - x0 + 1, b = y1 - y0 + 1;
  long long dx = 2 * x + 1 - (x0 + x1 + 1), dy = 2 * y + 1 - (y0 + y1 + 1);

  if((a <= 0) || (b <= 0))
    return 0;
  return dx * dx * b * b + dy * dy * a * a <= a * a * b * b;
}

/* half: 0 the whole ellipse, 1 only its top half (a sun on the horizon) */
static void ellipse(int x0, int y0, int x1, int y1, u16 c, int half)
{
  int x, y, ye = half ? (y0 + y1) / 2 : y1;

  for(y = y0; y <= ye; y++)
    for(x = x0; x <= x1; x++)
      if((x >= 0) && (x < cv_size) && (y >= 0) && (y < cv_size) &&
       in_ellipse(x, y, x0, y0, x1, y1))
        cv[y * cv_pitch + x] = c;
}

/* an ellipse outline w px thick */
static void ellipse_ring(int x0, int y0, int x1, int y1, int w, u16 c)
{
  int x, y;

  for(y = y0; y <= y1; y++)
    for(x = x0; x <= x1; x++)
      if((x >= 0) && (x < cv_size) && (y >= 0) && (y < cv_size) &&
       in_ellipse(x, y, x0, y0, x1, y1) &&
       !in_ellipse(x, y, x0 + w, y0 + w, x1 - w, y1 - w))
        cv[y * cv_pitch + x] = c;
}

/* a filled polygon (even-odd, sampled at pixel centres; the edges' own
   pixels count, as in the design's pictures) */
static void polygon(const int *pts, int n, u16 c)
{
  int y, i, j, ymin = pts[1], ymax = pts[1];

  for(i = 1; i < n; i++)
  {
    if(pts[i * 2 + 1] < ymin) ymin = pts[i * 2 + 1];
    if(pts[i * 2 + 1] > ymax) ymax = pts[i * 2 + 1];
  }
  if(ymin < 0) ymin = 0;
  if(ymax > cv_size - 1) ymax = cv_size - 1;
  for(y = ymin; y <= ymax; y++)
  {
    int xs[64] = { 0 }, k = 0;
    /* in eighths of a pixel: the crossing at the row's centre */
    int cy = y * 8 + 4;

    for(i = 0, j = n - 1; (i < n) && (k < 64); j = i++)
    {
      int ax = pts[j * 2] * 8, ay = pts[j * 2 + 1] * 8;
      int bx = pts[i * 2] * 8, by = pts[i * 2 + 1] * 8;
      if((ay <= cy) == (by <= cy))
        continue;
      xs[k++] = ax + (cy - ay) * (bx - ax) / (by - ay);
    }
    /* sort the crossings */
    for(i = 1; i < k; i++)
      for(j = i; (j > 0) && (xs[j - 1] > xs[j]); j--)
      {
        int t = xs[j];
        xs[j] = xs[j - 1];
        xs[j - 1] = t;
      }
    for(i = 0; i + 1 < k; i += 2)
    {
      /* pixels whose centre lies inside, plus the edge pixels */
      int a = xs[i] >> 3, b = (xs[i + 1] - 1) >> 3;
      if(b >= a)
        plot_span(y, a, b, c);
    }
  }
}

/* ---- Python's random, for the "tiles" motif ---------------------------------
 * The design picks tiles with random.Random(seed).choice(...), Python's
 * Mersenne Twister, so the same few lines here give the same tiles. */

static u32 mt[624];
static int mt_i;

static void mt_init(u32 s)
{
  int i;
  mt[0] = s;
  for(i = 1; i < 624; i++)
    mt[i] = 1812433253U * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i;
  mt_i = 624;
}

/* init_by_array with a one-word key: how Python seeds from an int below 2^32 */
static void mt_seed(u32 key)
{
  int i = 1, j = 0, k;

  mt_init(19650218U);
  for(k = 624; k; k--)
  {
    mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525U)) + key + j;
    i++;
    j = 0;  /* the key has one word */
    if(i >= 624)
    {
      mt[0] = mt[623];
      i = 1;
    }
  }
  for(k = 623; k; k--)
  {
    mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941U)) - i;
    i++;
    if(i >= 624)
    {
      mt[0] = mt[623];
      i = 1;
    }
  }
  mt[0] = 0x80000000U;
  mt_i = 624;
}

static u32 mt_next(void)
{
  u32 y;

  if(mt_i >= 624)
  {
    int k;
    for(k = 0; k < 624; k++)
    {
      y = (mt[k] & 0x80000000U) | (mt[(k + 1) % 624] & 0x7FFFFFFFU);
      mt[k] = mt[(k + 397) % 624] ^ (y >> 1) ^ ((y & 1) ? 0x9908B0DFU : 0);
    }
    mt_i = 0;
  }
  y = mt[mt_i++];
  y ^= y >> 11;
  y ^= (y << 7) & 0x9D2C5680U;
  y ^= (y << 15) & 0xEFC60000U;
  y ^= y >> 18;
  return y;
}

/* random.choice over four items: three bits, again while 4 or more */
static int mt_choice4(void)
{
  u32 r;
  do
    r = mt_next() >> 29;
  while(r >= 4);
  return r;
}

/* ---- the motifs -------------------------------------------------------------- */

static void motif(int name, u32 s, const u16 *col)
{
  int size = cv_size, half = size / 2, i;
  u16 F = col[1], M2 = col[2], D = col[3], bg = col[0];

  switch(name)
  {
    case HORIZON:  /* a sun over a banded sea */
    {
      int r = px(14);
      ellipse(half - r, px(18) - r, half + r, px(18) + r, F, 0);
      rect(0, px(30), size, size, D);
      for(i = 0; i < 4; i++)
        rect(0, px(32 + i * 5), size, px(32 + i * 5), M2);
      break;
    }
    case SLASH:    /* diagonal stripes behind a disc */
    {
      int w = px(10) > 4 ? px(10) : 4;
      for(i = -size; i < size; i += w)
      {
        int p[8] = { i, size, i + w / 2, size, i + w / 2 + size, 0, i + size, 0 };
        polygon(p, 4, M2);
      }
      ellipse(px(20), px(10), px(44), px(34), F, 0);
      break;
    }
    case PEAKS:    /* two ranges of mountains */
    {
      int a[14] = { 0, px(40), px(22), px(12), px(40), px(34), px(52), px(20),
       size, px(36), size, size, 0, size };
      int b[10] = { 0, px(48), px(30), px(30), size, px(46), size, size, 0, size };
      polygon(a, 7, F);
      polygon(b, 5, M2);
      break;
    }
    case TARGET:   /* rings */
    {
      static const int rs[3] = { 26, 18, 10 };
      for(i = 0; i < 3; i++)
      {
        int rr = px(rs[i]);
        ellipse(half - rr, px(26) - rr, half + rr, px(26) + rr,
         (i % 2 == 0) ? F : bg, 0);
      }
      break;
    }
    case SPLIT:    /* a diagonal halves the square, a small disc on the line */
    {
      int p[6] = { 0, size, size, 0, size, size };
      int r = px(7);
      polygon(p, 3, M2);
      ellipse(half - r, half - r - px(6), half + r, half + r - px(6), F, 0);
      break;
    }
    case DOTS:     /* a halftone grid, the dots growing toward a corner */
    {
      int step = px(8) > 4 ? px(8) : 4, x, y;
      for(y = step / 2; y < px(50); y += step)
        for(x = step / 2; x < size; x += step)
        {
          int r = (x + y) * px(3) / size;
          if(r < 1)
            r = 1;
          ellipse(x - r, y - r, x + r, y + r, F, 0);
        }
      break;
    }
    case ARCS:     /* quarter circles from two corners */
    {
      static const int r1[3] = { 44, 32, 20 }, r2[2] = { 30, 18 };
      for(i = 0; i < 3; i++)
      {
        int rr = px(r1[i]);
        ellipse(-rr, -rr, rr, rr, (i % 2 == 0) ? F : M2, 0);
      }
      for(i = 0; i < 2; i++)
      {
        int rr = px(r2[i]);
        ellipse(size - rr, px(48) - rr, size + rr, px(48) + rr,
         (i % 2 == 0) ? M2 : F, 0);
      }
      break;
    }
    case SUNSET:   /* bands narrowing toward the horizon, half a sun */
    {
      int r = px(16);
      for(i = 0; i < 6; i++)
      {
        int h = px(4 + i), y = px(4 + i * (8 + i / 2));
        rect(0, y, size, y + h, (i % 2) ? M2 : F);
      }
      ellipse(half - r, px(48) - r, half + r, px(48) + r, F, 1);
      break;
    }
    case ORBIT:    /* a planet with a ring and a moon */
    {
      int r = px(13), cx = half, cy = px(26), m = px(4);
      int w = px(2) > 1 ? px(2) : 1, t = px(1) > 1 ? px(1) : 1;
      ellipse_ring(cx - px(28), cy - px(6), cx + px(28), cy + px(6), w, M2);
      ellipse(cx - r, cy - r, cx + r, cy + r, F, 0);
      rect(cx - px(28), cy, cx + px(28), cy + t, M2);
      ellipse(px(50) - m, px(10) - m, px(50) + m, px(10) + m, F, 0);
      break;
    }
    case BLOCKS:   /* three overlapping blocks */
    {
      int r = px(9);
      rect(px(6), px(8), px(34), px(36), F);
      rect(px(24), px(20), px(56), px(46), M2);
      ellipse(px(40) - r, px(14) - r, px(40) + r, px(14) + r, F, 0);
      break;
    }
    case ZIGZAG:   /* chevron bands */
    {
      int h = px(6) > 3 ? px(6) : 3, p8 = px(8), row;
      if(p8 < 1)
        p8 = 1;
      for(row = 0; row < px(52); row += h * 2)
      {
        int pts[2 * 64], n = 0, x;
        for(x = 0; (x < size + p8) && (n < 32); x += p8)
        {
          pts[n * 2] = x;
          pts[n * 2 + 1] = row + (((x / p8) % 2) ? 0 : h);
          n++;
        }
        for(x = size + p8; (x > -p8) && (n < 64); x -= p8)
        {
          /* Python's // floors: -p8 // p8 is -1, odd */
          int q = (x >= 0) ? x / p8 : -((-x + p8 - 1) / p8);
          pts[n * 2] = x;
          pts[n * 2 + 1] = row + h + ((q & 1) ? 0 : h);
          n++;
        }
        polygon(pts, n, ((row / (h * 2)) % 2 == 0) ? F : M2);
      }
      break;
    }
    case TILES:    /* a 4 x 4 grid, a few tiles lit */
    {
      int q = size / 4, x, y;
      mt_seed(s);
      for(y = 0; y < 3; y++)
        for(x = 0; x < 4; x++)
        {
          int k = mt_choice4();
          if(k < 2)
            rect(x * q + 1, y * q + 1, x * q + q - 2, y * q + q - 2,
             (k == 0) ? F : M2);
        }
      break;
    }
  }
}

/* The seed of a title: crc32 of it in lower case (ASCII letters only; other
   characters stay as they are, as Python's lower() leaves CJK). */
u32 ui_cover_seed(const char *title)
{
  char low[256];
  int i;

  for(i = 0; title[i] && (i < (int)sizeof(low) - 1); i++)
    low[i] = ((title[i] >= 'A') && (title[i] <= 'Z')) ? title[i] + 32 : title[i];
  low[i] = 0;
  return crc32(0, (const unsigned char *)low, i);
}

int ui_cover_band(int size)
{
  int band = 16 * size / 64;

  if(size < 32)
    return 0;
  return (band < 10) ? 10 : band;
}

u16 ui_cover_band_color(const char *title)
{
  u32 s = ui_cover_seed(title);
  return duos[((s >> 8) % 12) + (((s >> 28) & 1) ? 12 : 0)][4];
}

void ui_cover_art(u16 *dst, int pitch, int size, const char *title)
{
  u32 s = ui_cover_seed(title);
  const u16 *col = duos[((s >> 8) % 12) + (((s >> 28) & 1) ? 12 : 0)];
  int band = ui_cover_band(size), x, y;

  cv = dst;
  cv_pitch = pitch;
  cv_size = size;
  rect(0, 0, size - 1, size - 1, col[0]);
  motif((s >> 16) % 12, s, col);
  if((s >> 29) & 1)
    for(y = 0; y < size; y++)
    {
      u16 *row = dst + y * pitch;
      for(x = 0; x < size / 2; x++)
      {
        u16 t = row[x];
        row[x] = row[size - 1 - x];
        row[size - 1 - x] = t;
      }
    }
  if(band)
    rect(0, size - band, size - 1, size - 1, col[4]);
}
