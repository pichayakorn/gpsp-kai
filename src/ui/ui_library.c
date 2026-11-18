/* ui_library.c - the Library (spec section 4 "Library", step 5)
 *
 * Every game under the ROM folder and its subfolders (.gba and .zip), shown
 * as a grid of covers or a list. A game without box art gets its preset
 * cover (ui_cover.c). The name is the file name, cleaned up (display_name).
 *
 * This part: the All tab (grid and list), View & sort, the empty library
 * and starting a game. Recent with its Continue card, the game card,
 * Favourites, Folders and the letter jump come in the next parts.
 */

#include <stddef.h>
#include "../common.h"
#include "ui.h"
#include "ui_cover.h"

#define MAX_GAMES   1024
#define MAX_DIRS    128
#define ARENA_SIZE  (64 * 1024)
#define SCAN_DEPTH  4

/* the grid: 64 px covers, five across on a 94 px pitch from x 20, rows at
   y 40 + 70 k (three in view; the third ends at 244, clear of the hints) */
#define COVER      64
#define GRID_X     20
#define GRID_Y     40
#define GRID_PX    94
#define GRID_PY    70
#define GRID_COLS  5
#define GRID_ROWS  3
#define VIEW_T     38   /* the list area, under the header */
#define VIEW_B     248  /* the hint strip */

/* the list: 22 px rows, 4 px apart; the selected game opens into an 84 px
   card with 4 px more ground above it, its top at y 88 when at rest */
#define LIST_SM    22
#define LIST_BIG   84
#define LIST_GAP   4
#define LIST_FY    88

#define POP_X  20
#define POP_Y  128
#define POP_W  300
#define POP_H  98

enum { VIEW_GRID, VIEW_LIST };
enum { SORT_NAME, SORT_RECENT, SORT_SIZE, SORTS };

typedef struct
{
  const char *name;   /* file name */
  const char *title;  /* display name */
  u32 size;           /* bytes */
  u16 dir;            /* index into dirs */
  s16 recent;         /* place in recent.txt, -1: not there */
} game_t;

typedef struct
{
  int game;           /* -1: free */
  u32 used;           /* frame it was last drawn */
  u16 *px;            /* COVER x COVER */
} cover_t;
#define MAX_COVERS 24

static struct
{
  game_t *games;
  int count;
  int *order;         /* the games in view order */
  const char **dirs;
  int dir_count;
  char *arena;
  int arena_used;

  int view, sort;
  int cur, cur_from, cur_f;   /* selection (in view order) and its move */
  int top, off_from, top_f;   /* grid: first row in view, sliding from off_from px */
  int pos, pos_from, pos_f;   /* list: selection in 1/256, eased (list_scroll) */
  int open_row, open_f;       /* list: the card opening or open (-1: none) */
  int close_row, close_from, close_f;  /* list: the card closing, from how open */
  int *ys;                    /* list: every row's top this frame */
  int marq_f;                 /* frames the selection has rested (marquee) */
  int dirty;
  u32 frame;

  int pop, pop_f, pop_out, pop_row, pop_flash;  /* View & sort */
  int played_game;            /* the game whose play time is read */
  u32 played;                 /* its seconds */

  cover_t covers[MAX_COVERS];
  int cover_count;
} L;

/* ---- the game list ------------------------------------------------------------ */

static char *arena_str(const char *s, int n)
{
  char *p;

  if(L.arena_used + n + 1 > ARENA_SIZE)
    return NULL;
  p = L.arena + L.arena_used;
  memcpy(p, s, n);
  p[n] = 0;
  L.arena_used += n + 1;
  return p;
}

static const char *display_name(const char *name, int n);

static int is_game_file(const char *name)
{
  const char *dot = strrchr(name, '.');

  return dot && (!strcasecmp(dot, ".gba") || !strcasecmp(dot, ".zip"));
}

static void scan_dir(const char *path, int depth)
{
  SceUID d;
  SceIoDirent e;
  int dir;
  const char *dir_path;

  if((L.dir_count >= MAX_DIRS) || (L.count >= MAX_GAMES))
    return;
  dir_path = arena_str(path, strlen(path));
  if(dir_path == NULL)
    return;
  dir = L.dir_count++;
  L.dirs[dir] = dir_path;

  d = sceIoDopen(path);
  if(d < 0)
    return;
  memset(&e, 0, sizeof(e));
  while((sceIoDread(d, &e) > 0) && (L.count < MAX_GAMES))
  {
    if(e.d_name[0] == '.')
      continue;
    if(FIO_S_ISDIR(e.d_stat.st_mode))
    {
      if(depth < SCAN_DEPTH)
      {
        char sub[MAX_PATH];
        path_printf(sub, sizeof(sub), "%s/%s", dir_path, e.d_name);
        sceIoDclose(d);
        scan_dir(sub, depth + 1);
        /* reopen and skip back to where this folder was */
        {
          char seen[256];
          snprintf(seen, sizeof(seen), "%s", e.d_name);
          d = sceIoDopen(dir_path);
          if(d < 0)
            return;
          while(sceIoDread(d, &e) > 0)
            if(!strcmp(e.d_name, seen))
              break;
        }
      }
    }
    else if(is_game_file(e.d_name))
    {
      game_t *g = &L.games[L.count];
      const char *dot = strrchr(e.d_name, '.');
      g->name = arena_str(e.d_name, strlen(e.d_name));
      g->title = g->name ? display_name(e.d_name, dot - e.d_name) : NULL;
      if(g->title == NULL)
        break;  /* out of room: the list ends here */
      g->size = (u32)e.d_stat.st_size;
      g->dir = dir;
      g->recent = -1;
      L.count++;
    }
    memset(&e, 0, sizeof(e));
  }
  sceIoDclose(d);
}

/* The name shown for a file: its name without the extension, cleaned up
 * so a downloaded library reads well (#14): bracketed tags such as
 * "(USA, Europe)", "[!]" or "(Rev 1)" go, so does a leading release number
 * ("1986 - Golden Sun"), and underscores become spaces. The cover is drawn
 * from this name, so these rules must not change once released. */
static const char *display_name(const char *name, int n)
{
  char out[256];
  int i = 0, o = 0, depth = 0;

  if(n > 255)
    n = 255;
  /* a leading release number: digits, then " - " */
  while((i < n) && (name[i] >= '0') && (name[i] <= '9'))
    i++;
  if(i > 0)
  {
    int j = i;
    while((j < n) && ((name[j] == ' ') || (name[j] == '_')))
      j++;
    if((j < n) && (name[j] == '-'))
    {
      j++;
      while((j < n) && ((name[j] == ' ') || (name[j] == '_')))
        j++;
      i = j;
    }
    else
      i = 0;
  }
  for(; i < n; i++)
  {
    char c = name[i];
    if((c == '(') || (c == '['))
      depth++;
    else if(((c == ')') || (c == ']')) && depth)
      depth--;
    else if(depth == 0)
    {
      if(c == '_')
        c = ' ';
      /* one space between words, none at the start */
      if((c == ' ') && ((o == 0) || (out[o - 1] == ' ')))
        continue;
      out[o++] = c;
    }
  }
  while((o > 0) && ((out[o - 1] == ' ') || (out[o - 1] == '-')))
    o--;
  if(o == 0)
    return arena_str(name, n);  /* nothing left: the plain name */
  return arena_str(out, o);
}

/* recent.txt (written on leaving a game): full paths, newest first */
static void read_recent(void)
{
  char path[MAX_PATH], line[MAX_PATH];
  FILE *f;
  int n = 0;

  path_printf(path, sizeof(path), "%s/recent.txt", g_default_cfg_dir);
  f = fopen(path, "r");
  if(f == NULL)
    return;
  while(fgets(line, sizeof(line), f))
  {
    char *slash;
    int i;
    line[strcspn(line, "\r\n")] = 0;
    slash = strrchr(line, '/');
    if(slash == NULL)
      continue;
    *slash = 0;
    for(i = 0; i < L.count; i++)
      if((L.games[i].recent < 0) && !strcasecmp(L.games[i].name, slash + 1) &&
       !strcasecmp(L.dirs[L.games[i].dir], line))
        L.games[i].recent = n;
    n++;
  }
  fclose(f);
}

static int compare_games(const void *a, const void *b)
{
  const game_t *ga = &L.games[*(const int *)a], *gb = &L.games[*(const int *)b];

  if(L.sort == SORT_RECENT)
  {
    int ra = (ga->recent < 0) ? 0x7FFF : ga->recent;
    int rb = (gb->recent < 0) ? 0x7FFF : gb->recent;
    if(ra != rb)
      return ra - rb;
  }
  else if(L.sort == SORT_SIZE)
  {
    if(ga->size != gb->size)
      return (ga->size > gb->size) ? -1 : 1;
  }
  return strcasecmp(ga->title, gb->title);
}

static void sort_games(void)
{
  int i, keep = (L.count > 0) ? L.order[L.cur] : 0;

  qsort(L.order, L.count, sizeof(int), compare_games);
  /* the cursor stays on its game */
  for(i = 0; i < L.count; i++)
    if(L.order[i] == keep)
      L.cur = L.cur_from = i;
}

static int load_games(void)
{
  int i;

  L.games = ui_pool_alloc(MAX_GAMES * sizeof(game_t));
  L.order = ui_pool_alloc(MAX_GAMES * sizeof(int));
  L.dirs = ui_pool_alloc(MAX_DIRS * sizeof(char *));
  L.arena = ui_pool_alloc(ARENA_SIZE);
  L.ys = ui_pool_alloc(MAX_GAMES * sizeof(int));
  if(!L.games || !L.order || !L.dirs || !L.arena || !L.ys)
    return 0;
  L.count = L.dir_count = L.arena_used = 0;
  scan_dir(g_default_rom_dir, 0);
  read_recent();
  for(i = 0; i < L.count; i++)
    L.order[i] = i;
  sort_games();
  L.cur = L.cur_from = 0;  /* a fresh list opens on its first game */
  return 1;
}

/* ---- covers --------------------------------------------------------------------- */

static void covers_init(void)
{
  int i;

  L.cover_count = 0;
  for(i = 0; i < MAX_COVERS; i++)
  {
    /* only what the pool can spare; with none, covers are drawn each time */
    if(ui_pool_free() < 64 * 1024 + COVER * COVER * 2)
      break;
    L.covers[i].px = ui_pool_alloc(COVER * COVER * 2);
    if(L.covers[i].px == NULL)
      break;
    L.covers[i].game = -1;
    L.cover_count++;
  }
}

/* The name plate (spec section 5, "the name is the cover"): from 32 px up
 * the name sits on a dark plate at the bottom, 12 px bold cream at x 4, on
 * up to three lines 14 px apart; the plate is 14 per line + 6 high. A name
 * longer than three lines has its third line cut with "…". */
#define PLATE_LINES 3
#define PLATE_PITCH 14

/* the plate's lines for a cover of this size; returns how many (0-3).
   rest: the name from the third line on (for the marquee), may be NULL */
static int plate_lines(const char *title, int size, char lines[PLATE_LINES][128],
 char *rest, int rest_size)
{
  int n, k;
  char line[128];

  if(size < 32)
    return 0;
  if(rest)
    rest[0] = 0;
  for(n = 0; ui_wrap_line(title, n, size - 8, UI_FONT_BODY, line, sizeof(line)); n++)
  {
    char *s = line;
    while(*s == ' ')
      s++;
    if(n < PLATE_LINES)
      snprintf(lines[n], 128, "%s", s);
    if(rest && (n >= PLATE_LINES - 1))
    {
      k = strlen(rest);
      snprintf(rest + k, rest_size - k, "%s%s", k ? " " : "", s);
    }
  }
  return (n > PLATE_LINES) ? -PLATE_LINES : n;  /* negative: cut */
}

static void cover_plate(int x, int y, int size, const char *title)
{
  char lines[PLATE_LINES][128], rest[256];
  int n = plate_lines(title, size, lines, rest, sizeof(rest)), cut = n < 0, h, i;

  if(n == 0)
    return;
  if(cut)
    n = -n;
  h = PLATE_PITCH * n + 6;
  ui_fill(x, y + size - h, size, h, ui_cover_plate_color(title));
  for(i = 0; i < n; i++)
  {
    int ty = y + size - h + 2 + PLATE_PITCH * i;
    if(cut && (i == n - 1))  /* the rest of the name, cut with "…" */
      ui_print_fit(x + 4, ty, rest, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD, size - 8);
    else
      ui_print(x + 4, ty, lines[i], UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD);
  }
}

/* A cover of any size at x, y on the current target (clipped); plate 0:
   the art alone (the list card, a grid row cut by the hint strip). */
static void draw_cover_any(int x, int y, int size, const char *title, int plate)
{
  static u16 buf[96 * 96];

  if(size > 96)
    size = 96;
  ui_cover_art(buf, size, size, title);
  ui_blit(buf, size, x, y, size, size);
  if(plate)
    cover_plate(x, y, size, title);
}

/* A 64 px grid cover with its plate, from the cache when it has room. */
static void draw_cover(int x, int y, int game)
{
  int i, free_i = -1, old_i = 0;
  cover_t *c;

  for(i = 0; i < L.cover_count; i++)
  {
    if(L.covers[i].game == game)
    {
      L.covers[i].used = L.frame;
      ui_blit(L.covers[i].px, COVER, x, y, COVER, COVER);
      return;
    }
    if(L.covers[i].game < 0)
      free_i = i;
    else if(L.covers[i].used < L.covers[old_i].used)
      old_i = i;
  }
  if(L.cover_count == 0)
  {
    draw_cover_any(x, y, COVER, L.games[game].title, 1);
    return;
  }
  c = &L.covers[(free_i >= 0) ? free_i : old_i];
  c->game = game;
  c->used = L.frame;
  ui_cover_art(c->px, COVER, COVER, L.games[game].title);
  /* the plate goes into the cached picture too */
  if(ui_target_begin(c->px, COVER, COVER, COVER))
  {
    cover_plate(0, 0, COVER, L.games[game].title);
    ui_target_end();
  }
  ui_blit(c->px, COVER, x, y, COVER, COVER);
}

/* ---- the marquee ---------------------------------------------------------------------
 * A long name on the selected item runs sideways once the cursor has rested
 * UI_MARQUEE_DELAY frames, 1 px a frame; it waits at the end as long, then
 * starts again. Only the selected item, clipped to its own line. */

static int marquee_shift(int over)
{
  int t = L.marq_f - UI_MARQUEE_DELAY, cycle = over + 2 * UI_MARQUEE_DELAY;

  if((over <= 0) || (t <= 0))
    return 0;
  t %= cycle;
  return (t < over) ? t : (t < over + UI_MARQUEE_DELAY) ? over : 0;
}

/* the grid's selected cover at x, y: a cut third line runs the rest of the
   name (clipped to x 4-60 of the cover) */
static void grid_marquee(int x, int y, int game)
{
  char lines[PLATE_LINES][128], rest[256];
  const char *title = L.games[game].title;
  int n = plate_lines(title, COVER, lines, rest, sizeof(rest)), w, ly;

  if(n >= 0)
    return;  /* three lines or fewer: nothing moves */
  w = ui_print_width(rest, UI_FONT_BODY, UI_TEXT_BOLD);
  if(L.marq_f <= UI_MARQUEE_DELAY)
    return;
  ly = y + COVER - (PLATE_PITCH * PLATE_LINES + 6) + 2 + PLATE_PITCH * 2;
  ui_fill(x, ly, COVER, PLATE_PITCH, ui_cover_plate_color(title));
  ui_clip_set(x + 4, ly, COVER - 8, PLATE_PITCH);
  ui_print(x + 4 - marquee_shift(w - (COVER - 8)), ly, rest, UI_COL_CREAM,
   UI_FONT_BODY, UI_TEXT_BOLD);
  ui_clip_reset();
}

/* ---- the grid --------------------------------------------------------------------- */

static int grid_rows(void)
{
  return (L.count + GRID_COLS - 1) / GRID_COLS;
}

static int grid_off(void)
{
  return ui_ease_out_back(L.off_from, L.top * GRID_PY, L.top_f, UI_F_SCROLL);
}

static void grid_item_xy(int i, int off, int *x, int *y)
{
  *x = GRID_X + (i % GRID_COLS) * GRID_PX;
  *y = GRID_Y + (i / GRID_COLS) * GRID_PY - off;
}

/* A grid cover, cut to the list area; a row cut by the hint strip shows
   its art only, never half a name plate. */
static void grid_cover(int i, int x, int y)
{
  int bottom = (y + COVER < VIEW_B) ? y + COVER : VIEW_B;

  ui_clip_set(0, VIEW_T, UI_SCREEN_W, bottom - VIEW_T);
  if(y + COVER > VIEW_B)
    draw_cover_any(x, y, COVER, L.games[L.order[i]].title, 0);
  else
    draw_cover(x, y, L.order[i]);
  ui_clip_reset();
}

static void grid_ground(void)
{
  int i, off = grid_off(), rows = grid_rows();

  for(i = 0; i < L.count; i++)
  {
    int x, y;
    grid_item_xy(i, off, &x, &y);
    if((y + COVER <= VIEW_T) || (y >= VIEW_B))
      continue;
    grid_cover(i, x, y);
  }
  if(rows > GRID_ROWS)
  {
    int th = 196 * GRID_ROWS / rows;
    int ty = (196 - th) * off / ((rows - GRID_ROWS) * GRID_PY);
    if(ty < 0) ty = 0;
    if(ty > 196 - th) ty = 196 - th;
    ui_round_fill(468, 42, 4, 196, 1, UI_COL_CARD);
    ui_round_fill(468, 42 + ty, 4, th, 1, UI_COL_MUTE);
  }
}

/* the selected cover lifted, and the cursor frame flying to it */
static void grid_selection(void)
{
  int off = grid_off(), x, y, fx, fy, x0, y0, x1, y1;

  if(L.count == 0)
    return;
  grid_item_xy(L.cur, off, &x, &y);
  if((y >= VIEW_T) && (y + COVER <= VIEW_B))
  {
    ui_clip_set(0, VIEW_T, UI_SCREEN_W, VIEW_B - VIEW_T);
    ui_fill(x + 1, y + 1, COVER, COVER, UI_COL_SHADE);
    draw_cover(x - UI_LIFT, y - UI_LIFT, L.order[L.cur]);
    ui_clip_reset();
    if(L.top_f >= UI_F_SCROLL)
      grid_marquee(x - UI_LIFT, y - UI_LIFT, L.order[L.cur]);
  }
  grid_item_xy(L.cur_from, off, &x0, &y0);
  grid_item_xy(L.cur, off, &x1, &y1);
  fx = ui_ease_out_back(x0, x1, L.cur_f, UI_F_CURSOR);
  fy = ui_ease_out_back(y0, y1, L.cur_f, UI_F_CURSOR);
  ui_cursor_frame(fx - UI_LIFT, fy - UI_LIFT, COVER, COVER);
}

/* ---- the list ----------------------------------------------------------------------
 * Motion "A+" (spec section 6: list_scroll, list_close, list_open): the list
 * slides with every row slim and no overshoot; the open card closes as a
 * move starts and the new one opens only once the d-pad is up and the slide
 * has stopped. A card between sizes shows only its slim row's contents. */

/* the selection's place in 1/256 rows, eased */
static int list_pos(void)
{
  return ui_ease_out_cubic(L.pos_from, L.pos, L.pos_f, UI_F_LIST_SCROLL);
}

/* how open row i is, 0-256 */
static int list_open_p(int i)
{
  if(i == L.open_row)
    return ui_ease_out_cubic(0, 256, L.open_f, UI_F_LIST_OPEN);
  if(i == L.close_row)
    return ui_ease_out_cubic(L.close_from, 0, L.close_f, UI_F_LIST_CLOSE);
  return 0;
}

static int list_h(int i)
{
  return LIST_SM + (LIST_BIG - LIST_SM) * list_open_p(i) / 256;
}

/* 4 px more ground above an open card: it lifts 2 px, so its frame keeps
   3 px of ground above and below */
static int list_gap(int i)
{
  return LIST_GAP + 4 * list_open_p(i) / 256;
}

/* Every row's top for this frame, into L.ys: the row under the selection
 * sits at LIST_FY and moves up by its own pitch as the selection passes it;
 * then the whole list shifts so no ground shows above the first row (top
 * at y 42 or above) or below the last (bottom at y 244 or below). */
#define LIST_FIRST 42
#define LIST_LAST  244
static void list_layout(int pos)
{
  int lo = pos >> 8, fr = pos & 0xFF, i, d = 0, bottom;

  if(lo >= L.count - 1)
  {
    lo = L.count - 1;
    fr = 0;
  }
  L.ys[lo] = LIST_FY - fr * (list_h(lo) + list_gap(lo + 1)) / 256;
  for(i = lo + 1; i < L.count; i++)
    L.ys[i] = L.ys[i - 1] + list_h(i - 1) + list_gap(i);
  for(i = lo - 1; i >= 0; i--)
    L.ys[i] = L.ys[i + 1] - list_h(i) - list_gap(i + 1);
  bottom = L.ys[L.count - 1] + list_h(L.count - 1);
  if(L.ys[0] > LIST_FIRST)
    d = LIST_FIRST - L.ys[0];
  else if(bottom < LIST_LAST)
  {
    d = LIST_LAST - bottom;
    if(d > LIST_FIRST - L.ys[0])
      d = LIST_FIRST - L.ys[0];
  }
  for(i = 0; i < L.count; i++)
    L.ys[i] += d;
}

static void size_text(u32 bytes, char *out, int n)
{
  u32 mb = (bytes + (512 * 1024)) >> 20;

  if(mb == 0)
    snprintf(out, n, "%u KB", (unsigned)((bytes + 1023) >> 10));
  else
    snprintf(out, n, "%u MB", (unsigned)mb);
}

/* The play time of a game, from its .cfg (read once per selection). */
static u32 game_played(int game)
{
  char path[MAX_PATH], name[MAX_PATH];
  game_config_t cfg;
  FILE *f;
  char *dot;
  size_t n;

  if(L.played_game == game)
    return L.played;
  L.played_game = game;
  L.played = 0;
  snprintf(name, sizeof(name), "%s", L.games[game].name);
  dot = strrchr(name, '.');
  if(dot)
    *dot = 0;
  path_printf(path, sizeof(path), "%s/%s.cfg", g_default_cfg_dir, name);
  f = fopen(path, "rb");
  if(f == NULL)
    return 0;
  memset(&cfg, 0, sizeof(cfg));
  n = fread(&cfg, 1, sizeof(cfg), f);
  fclose(f);
  if(n >= offsetof(game_config_t, play_seconds) + sizeof(u32))
    L.played = cfg.play_seconds;
  return L.played;
}

static void played_text(u32 seconds, char *out, int n)
{
  if(seconds == 0)
    snprintf(out, n, "Not played yet");
  else if(seconds < 3600)
    snprintf(out, n, "Played %u min", (unsigned)((seconds + 59) / 60));
  else
    snprintf(out, n, "Played %u h", (unsigned)(seconds / 3600));
}

/* a slim row's icon, name and size; on a card between sizes they are plum
   and bold, moved with its lift */
static void list_slim_contents(const game_t *g, int y, int lift, int on_card)
{
  char size[16];

  draw_cover_any(23 - lift, y + 3 - lift, 16, g->title, 0);
  ui_print_fit(46 - lift, y + 3 - lift, g->title, on_card ? UI_COL_PLUM : UI_COL_CREAM,
   UI_FONT_BODY, on_card ? UI_TEXT_BOLD : 0, 330);
  size_text(g->size, size, sizeof(size));
  ui_print(452 - lift - ui_print_width(size, UI_FONT_SMALL, 0), y + 4 - lift, size,
   on_card ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_SMALL, 0);
}

/* row i at y as a card (open or between sizes, o of 256) */
static void list_card(int i, int y, int o)
{
  const game_t *g = &L.games[L.order[i]];
  int h = list_h(i), lift = (2 * o + 128) / 256;
  char chip[32], size[16], played[32];
  int cw;

  ui_round_fill(21, y + 1, 440, h, 2, UI_COL_SHADE);
  ui_round_fill(20 - lift, y - lift, 440, h, 2, UI_COL_TANGERINE);
  if(o < 256)
  {
    list_slim_contents(g, y, lift, 1);
    return;
  }
  /* open: the full art (no plate, the name is beside it), 76 px in the
     80 px plum box, 2 px of tangerine round the box */
  ui_fill(20, y, 80, 80, UI_COL_PLUM);
  draw_cover_any(22, y + 2, 76, g->title, 0);
  ui_print_fit(110, y + 6, g->title, UI_COL_PLUM, UI_FONT_BODY, UI_TEXT_BOLD, 330);
  played_text(game_played(L.order[i]), played, sizeof(played));
  ui_print(110, y + 26, played, UI_COL_PLUM, UI_FONT_SMALL, 0);
  size_text(g->size, size, sizeof(size));
  snprintf(chip, sizeof(chip), "GBA \xC2\xB7 %s", size);
  cw = ui_print_width(chip, UI_FONT_SMALL, 0) + 12;
  ui_round_fill(110, y + 48, cw, 16, 2, UI_COL_PLUM);
  ui_print(116, y + 49, chip, UI_COL_TANGERINE, UI_FONT_SMALL, 0);
}

static void list_ground(void)
{
  int i, pos = list_pos();

  list_layout(pos);
  ui_clip_set(0, VIEW_T - 2, UI_SCREEN_W, VIEW_B - VIEW_T + 2);
  for(i = 0; i < L.count; i++)
  {
    int y = L.ys[i], h = list_h(i), o = list_open_p(i);
    if((y + h < VIEW_T - 2) || (y > VIEW_B))
      continue;
    if(o > 0)
    {
      list_card(i, y, o);
      continue;
    }
    ui_round_fill(20, y, 440, h, 2, UI_COL_CARD);
    list_slim_contents(&L.games[L.order[i]], y, 0, 0);
    /* the row past the last whole one: dimmed, a "more below" peek */
    if(y + h > VIEW_B)
      ui_door(20, y, 440, h);
  }
  ui_clip_reset();
  if(L.count > 1)
  {
    int ty = 176 * pos / ((L.count - 1) * 256);
    ui_round_fill(468, 42, 4, 196, 1, UI_COL_CARD);
    ui_round_fill(468, 42 + ty, 4, 20, 1, UI_COL_MUTE);
  }
}

/* the cursor frame between rows lo and lo + 1, with their current heights;
   on the open card at rest, a long name runs sideways */
static void list_selection(void)
{
  int pos = list_pos(), lo = pos >> 8, hi, fr = pos & 0xFF, y0, y1, h0, h1;

  if(L.count == 0)
    return;
  list_layout(pos);
  if(lo >= L.count - 1)
  {
    lo = L.count - 1;
    fr = 0;
  }
  hi = (lo + 1 < L.count) ? lo + 1 : lo;
  y0 = L.ys[lo] - 2;
  y1 = L.ys[hi] - 2;
  h0 = list_h(lo);
  h1 = list_h(hi);
  ui_clip_set(0, VIEW_T - 4, UI_SCREEN_W, VIEW_B - VIEW_T + 4);
  ui_cursor_frame(18, y0 + (y1 - y0) * fr / 256, 440, h0 + (h1 - h0) * fr / 256);
  ui_clip_reset();
  if((L.open_row == L.cur) && (list_open_p(L.cur) == 256))
  {
    const char *t = L.games[L.order[L.cur]].title;
    int w = ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD), y = L.ys[L.cur];
    int shift = marquee_shift(w - 330);
    if(shift > 0)
    {
      ui_fill(110, y + 6, 330, 16, UI_COL_TANGERINE);
      ui_clip_set(110, y + 6, 330, 16);
      ui_print(110 - shift, y + 6, t, UI_COL_PLUM, UI_FONT_BODY, UI_TEXT_BOLD);
      ui_clip_reset();
    }
  }
}

/* ---- View & sort ------------------------------------------------------------------- */

static const char *view_labels[2] = { "Grid", "List" };
static const char *sort_labels[SORTS] = { "Name", "Recently played", "Size" };

static int popup_y(void)
{
  if(L.pop_out)
    return ui_ease_in_cubic(POP_Y, UI_SCREEN_H, L.pop_f, UI_F_POPUP_OUT);
  return ui_ease_out_back(POP_Y + 24, POP_Y, L.pop_f, UI_F_POPUP_IN);
}

static void draw_popup(void)
{
  int x = POP_X, y = popup_y(), j;

  ui_round_fill(x + 3, y + 3, POP_W, POP_H, 3, UI_COL_SHADE);
  ui_round_fill(x, y, POP_W, POP_H, 3, UI_COL_CREAM);
  ui_fill(x, y + 4, 6, POP_H - 8, UI_COL_SKY);
  ui_print(x + 16, y + 6, "View & sort", UI_COL_PLUM, UI_FONT_BODY, UI_TEXT_BOLD);
  for(j = 0; j < 2; j++)
  {
    int ry = y + 32 + j * 34, sx = x + 16 + 64, i, n = j ? SORTS : 2;
    int k = j ? L.sort : L.view;
    const char **labels = j ? sort_labels : view_labels;
    if(j == L.pop_row)
      ui_round_fill(x + 10, ry - 4, POP_W - 20, 28, 2, COLOR16(21, 26, 27));
    ui_print(x + 16, ry + 2, j ? "Sort by" : "View", UI_COL_PLUM, UI_FONT_SMALL, 0);
    for(i = 0; i < n; i++)
    {
      int ow = ui_print_width(labels[i], UI_FONT_SMALL, 0) + 14;
      if(i == k)
      {
        int g = (L.pop_flash < UI_F_VALUE) && (j == L.pop_row) ?
         2 - ui_ease_out_back(0, 2, L.pop_flash, UI_F_VALUE) : 0;
        ui_round_fill(sx - g, ry, ow + 2 * g, 18, 2, UI_COL_LIME);
      }
      else
        ui_round_frame(sx, ry, ow, 18, 2, COLOR16(22, 20, 19));
      ui_print(sx + 7, ry + 2, labels[i], UI_COL_PLUM, UI_FONT_SMALL, 0);
      sx += ow + 4;
    }
  }
}

/* ---- hints, the empty library, the ground --------------------------------------------- */

static const char *tab_labels[1] = { "All" };

static void draw_hints(void)
{
  ui_hint_t h[4];
  int n = 0;

  if(L.pop && !L.pop_out)
  {
    h[n].keys = PSP_CTRL_UP | PSP_CTRL_DOWN;    h[n++].verb = "Move";
    h[n].keys = PSP_CTRL_LEFT | PSP_CTRL_RIGHT; h[n++].verb = "Change";
    h[n].keys = ui_key_back;                    h[n++].verb = "Close";
  }
  else if(L.count == 0)
  {
    h[n].keys = ui_key_confirm;  h[n++].verb = "Look again";
    h[n].keys = PSP_CTRL_START;  h[n++].verb = "Settings";
  }
  else
  {
    h[n].keys = PSP_CTRL_SELECT; h[n++].verb = "View";
    h[n].keys = PSP_CTRL_START;  h[n++].verb = "Settings";
    h[n].keys = ui_key_confirm;  h[n++].verb = "Play";
  }
  ui_hint_packed(h, n, 14);
}

/* the folder icon (spec icons), drawn at 4x on the empty library */
static const u16 icon_folder[11] =
 { 0x000, 0x3C0, 0x27C, 0x204, 0x3FE, 0x202, 0x202, 0x202, 0x3FE, 0x000, 0x000 };

static void draw_empty(void)
{
  int r, c;
  char line[128];
  static const char body[] =
   "Copy .gba or .zip files to ms0:/GBA/ on the Memory Stick, then look again.";

  ui_round_fill(196, 52, 88, 72, 4, UI_COL_CARD);
  for(r = 0; r < 11; r++)
    for(c = 0; c < 11; c++)
      if(icon_folder[r] & (1 << (10 - c)))
        ui_fill(218 + c * 4, 68 + r * 4, 4, 4, UI_COL_TANGERINE);
  ui_print(240 - ui_print_width("No games yet", UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X) / 2,
   136, "No games yet", UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);
  for(r = 0; r < 3; r++)
  {
    if(!ui_wrap_line(body, r, 360, UI_FONT_BODY, line, sizeof(line)))
      break;
    ui_print(240 - ui_print_width(line, UI_FONT_BODY, 0) / 2, 172 + r * 16, line,
     UI_COL_MUTE, UI_FONT_BODY, 0);
  }
}

static void build_ground(void)
{
  L.dirty = 0;
  if(!ui_layer_begin())
    return;
  if(L.count == 0)
    draw_empty();
  else if(L.view == VIEW_GRID)
    grid_ground();
  else
    list_ground();
  draw_hints();
  ui_layer_end();
}

/* ---- moving ------------------------------------------------------------------------- */

static void select_game(int i)
{
  if((i < 0) || (i >= L.count) || (i == L.cur))
    return;
  L.cur_from = L.cur;
  L.cur = i;
  L.cur_f = 0;
  if(L.view == VIEW_GRID)
  {
    int row = i / GRID_COLS, top = L.top;
    if(row < top)
      top = row;
    if(row >= top + GRID_ROWS)
      top = row - GRID_ROWS + 1;
    if(top != L.top)
    {
      L.off_from = grid_off();  /* a new move starts from where it is now */
      L.top = top;
      L.top_f = 0;
    }
  }
  else
  {
    /* the open card closes as the move starts (from however open it is);
       the list slides with every row slim; the new card opens later */
    if(L.open_row >= 0)
    {
      L.close_from = list_open_p(L.open_row);
      L.close_row = L.open_row;
      L.close_f = 0;
      L.open_row = -1;
    }
    L.pos_from = list_pos();
    L.pos = i * 256;
    L.pos_f = 0;
  }
  L.marq_f = 0;
  L.dirty = 1;
}

/* list_open: once the d-pad is up and the slide has stopped, the selected
   row grows into the card */
static void list_step(void)
{
  if(L.pos_f < UI_F_LIST_SCROLL)
  {
    L.pos_f++;
    L.dirty = 1;
  }
  if((L.close_row >= 0) && (L.close_f < UI_F_LIST_CLOSE))
  {
    if(++L.close_f >= UI_F_LIST_CLOSE)
      L.close_row = -1;
    L.dirty = 1;
  }
  if((L.open_row < 0) && (L.pos_f >= UI_F_LIST_SCROLL) &&
   !ui_down(PSP_CTRL_UP | PSP_CTRL_DOWN))
  {
    L.open_row = L.cur;
    L.open_f = 0;
  }
  if((L.open_row >= 0) && (L.open_f < UI_F_LIST_OPEN))
  {
    L.open_f++;
    L.dirty = 1;
  }
}

/* the view changed (View & sort): the grid rests with the cursor in view */
static void view_reset(void)
{
  int row = L.cur / GRID_COLS;

  L.top = (row >= GRID_ROWS) ? row - GRID_ROWS + 1 : 0;
  L.off_from = L.top * GRID_PY;
  L.top_f = UI_F_SCROLL;
  L.pos = L.pos_from = L.cur * 256;
  L.pos_f = UI_F_LIST_SCROLL;
  L.open_row = L.cur;  /* the list opens at rest, its card open */
  L.open_f = UI_F_LIST_OPEN;
  L.close_row = -1;
  L.marq_f = 0;
  L.cur_from = L.cur;
  L.cur_f = UI_F_CURSOR;
  L.dirty = 1;
}

static void move(void)
{
  if(L.count == 0)
    return;
  if(L.view == VIEW_GRID)
  {
    if(ui_repeat(PSP_CTRL_LEFT) && (L.cur > 0))
      select_game(L.cur - 1);
    else if(ui_repeat(PSP_CTRL_RIGHT) && (L.cur + 1 < L.count))
      select_game(L.cur + 1);
    else if(ui_repeat(PSP_CTRL_UP) && (L.cur >= GRID_COLS))
      select_game(L.cur - GRID_COLS);
    else if(ui_repeat(PSP_CTRL_DOWN) && (L.cur / GRID_COLS + 1 < grid_rows()))
    {
      int next = L.cur + GRID_COLS;
      select_game((next < L.count) ? next : L.count - 1);
    }
  }
  else
    select_game(ui_step(L.cur, L.count, PSP_CTRL_UP, PSP_CTRL_DOWN));
}

static void popup_input(void)
{
  if(L.pop_out)
    return;
  if(ui_pressed(ui_key_back) || ui_pressed(PSP_CTRL_SELECT))
  {
    L.pop_out = 1;
    L.pop_f = 0;
    /* written when the popup closes (spec section 7) */
    g_gpsp_config.library_view = (L.view & 1) | ((L.sort & 3) << 1);
    save_config_file();
    return;
  }
  if(ui_repeat(PSP_CTRL_UP | PSP_CTRL_DOWN))
    L.pop_row ^= 1;
  if(ui_repeat(PSP_CTRL_LEFT) || ui_repeat(PSP_CTRL_RIGHT))
  {
    int d = ui_down(PSP_CTRL_LEFT) ? -1 : 1;
    if(L.pop_row == 0)
    {
      int v = L.view + d;
      if((v >= 0) && (v < 2))
      {
        L.view = v;
        view_reset();
        L.pop_flash = 0;
      }
    }
    else
    {
      int v = L.sort + d;
      if((v >= 0) && (v < SORTS))
      {
        L.sort = v;
        sort_games();
        view_reset();
        L.pop_flash = 0;
      }
    }
  }
}

/* ---- starting a game ------------------------------------------------------------------ */

/* the tangerine progress card, "Loading" and its three dots (spec section 2) */
static void draw_loading(int t)
{
  int x = (UI_SCREEN_W - 332) / 2, y = 98, w = 332, h = 54, ex, i;

  ui_round_fill(x + 4, y + 4, w, h, 3, UI_COL_SHADE);
  ui_round_fill(x, y, w, h, 3, UI_COL_CARD);
  ui_fill(x, y + 4, 4, h - 8, UI_COL_TANGERINE);
  ex = ui_print(x + 16, y + 10, "Loading", UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD);
  for(i = 0; i < 3; i++)
    ui_fill(ex + 4 + i * 8, y + 19 - ((i == (t / UI_F_LOADING) % 3) ? 3 : 0), 4, 4,
     UI_COL_TANGERINE);
  for(i = 0; i < 30; i++)
    ui_fill(x + 16 + i * 10, y + 32, 8, 12, UI_COL_PLUM_2);
}

/* ---- the screen ----------------------------------------------------------------------- */

/* START: Settings over the library; a restart asked for there starts kai
   again on the library */
static void open_settings(void)
{
  if(ui_settings(0, 0))
  {
    int tab, row;
    ui_settings_where(&tab, &row);
    ui_restart_now(tab, row, NULL);
  }
  ui_status_read();
  ui_input_init();
  L.dirty = 1;
}

/* Pick a game. Returns 1 with its folder and file name (no game running:
 * kai's start; from the pause screen: a game was left). */
int ui_library(char *dir, int dir_size, char *name, int name_size)
{
  u32 mark = ui_pool_mark();
  int chosen = 0;

  memset(&L, 0, sizeof(L));
  L.played_game = -1;
  L.view = g_gpsp_config.library_view & 1;
  L.sort = (g_gpsp_config.library_view >> 1) & 3;
  if(L.sort >= SORTS)
    L.sort = SORT_NAME;
  if(!load_games())
  {
    ui_pool_release(mark);
    return 0;
  }
  covers_init();
  view_reset();
  ui_status_read();
  ui_input_init();

  while(!chosen)
  {
    L.frame++;
    ui_input_update();

    if(L.pop)
      popup_input();
    else if(L.count == 0)
    {
      if(ui_pressed(ui_key_confirm))
      {
        ui_layer_reset();
        ui_pool_release(mark);
        memset(&L, 0, sizeof(L));
        L.played_game = -1;
        load_games();
        covers_init();
        view_reset();
      }
      else if(ui_pressed(PSP_CTRL_START))
        open_settings();
    }
    else
    {
      move();
      if(ui_pressed(PSP_CTRL_SELECT))
      {
        L.pop = 1;
        L.pop_f = 0;
        L.pop_out = 0;
        L.pop_row = 0;
        L.pop_flash = UI_F_VALUE;
        L.dirty = 1;
      }
      else if(ui_pressed(PSP_CTRL_START))
        open_settings();
      else if(ui_pressed(ui_key_confirm))
        chosen = 1;
    }

    if(L.top_f < UI_F_SCROLL) { L.top_f++; L.dirty = 1; }
    if(L.view == VIEW_LIST)
      list_step();
    if(L.cur_f < UI_F_CURSOR) L.cur_f++;
    L.marq_f++;
    if(L.dirty)
      build_ground();

    ui_frame_begin();
    ui_ground();
    ui_tabs(UI_MARGIN, tab_labels, 1, 0, 0, UI_F_TAB);
    ui_status_chip();
    if(L.count > 0)
    {
      if(L.view == VIEW_GRID)
        grid_selection();
      else
        list_selection();
    }
    if(L.pop)
    {
      if(L.pop_flash < UI_F_VALUE)
        L.pop_flash++;
      if(L.pop_f < (L.pop_out ? UI_F_POPUP_OUT : UI_F_POPUP_IN))
        L.pop_f++;
      else if(L.pop_out)
      {
        L.pop = 0;
        L.dirty = 1;
      }
      if(L.pop)
        draw_popup();
      if(L.pop_f == 1)
        L.dirty = 1;  /* the hints change with it */
    }
    if(chosen)
      draw_loading(0);
    ui_frame_end();
  }

  {
    const game_t *g = &L.games[L.order[L.cur]];
    snprintf(dir, dir_size, "%s", L.dirs[g->dir]);
    snprintf(name, name_size, "%s", g->name);
  }
  ui_layer_reset();
  ui_pool_release(mark);
  return 1;
}

/* The pause screen's Library tile: Last is saved (when the save is on);
 * the game left is noted in Recent, then the one picked is loaded. A load
 * that fails may have overwritten the old game, so the library opens
 * again until one loads. */
u32 ui_library_switch(void)
{
  char dir[MAX_PATH], name[MAX_FILE];

  save_config_file();
  update_backup_force();
  note_game_left();
  save_game_config_file();

  while(ui_library(dir, sizeof(dir), name, sizeof(name)))
  {
    chdir(dir);
    snprintf(rom_path, sizeof(rom_path), "%s", dir);
    if(load_gamepak(name) != -1)
    {
      reset_gba();
      reg[CHANGED_PC_STATUS] = 1;
      clear_screen(0);
      flip_screen();
      clear_screen(0);
      flip_screen();
      video_resolution(FRAME_GAME);
      set_cpu_clock(g_game_config.clock_speed_number);
      pause_sound(0);
      real_frame_count = 0;
      virtual_frame_count = 0;
      return 1;
    }
    gamepak_filename[0] = 0;
  }
  return 0;
}
