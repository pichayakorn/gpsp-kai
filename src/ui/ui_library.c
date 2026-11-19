/* ui_library.c - the Library (spec section 4 "Library", step 5)
 *
 * Every game under the ROM folder and its subfolders (.gba and .zip), shown
 * as a grid of covers or a list. A game without box art gets its preset
 * cover (ui_cover.c). The name is the file name, cleaned up (display_name).
 *
 * This part: the All tab (the list), Sort, the empty library
 * and starting a game. Recent with its Continue card, the game card,
 * Favourites, Folders and the letter jump come in the next parts.
 */

#include <stddef.h>
#include "../common.h"
#include "ui.h"
#include "ui_cover.h"

#define MAX_GAMES   1024
#define MAX_DIRS    128
#define MAX_FOLD_ROWS (MAX_DIRS + MAX_GAMES + 1)
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
/* The first release ships the list view only (maintainer, 2026-11-19):
   the grid is postponed and will be reworked. Its code stays behind this
   switch; with it on, SELECT opens View & sort with a View row. */
#define LIBRARY_GRID 0
#define POP_FIRST_ROW (LIBRARY_GRID ? 0 : 1)
#define POP_H  (LIBRARY_GRID ? 98 : 64)

enum { VIEW_GRID, VIEW_LIST };
enum { TAB_RECENT, TAB_ALL, TAB_FAV, TAB_FOLDERS };
enum { SORT_NAME, SORT_RECENT, SORT_SIZE, SORTS };

typedef struct
{
  const char *name;   /* file name */
  const char *title;  /* display name */
  u32 size;           /* bytes */
  u16 dir;            /* index into dirs */
  s16 recent;         /* place in recent.txt, -1: not there */
  u8 fav;             /* hearted: on Favourites */
} game_t;

typedef struct
{
  int game;           /* -1: free */
  u32 used;           /* frame it was last drawn */
  u16 *px;            /* COVER x COVER */
} cover_t;
#define MAX_COVERS 24  /* the grid's; without it, the Recent row's five and spares: 8 */

static struct
{
  game_t *games;
  int count;
  int *order;         /* the games in view order (the tab's games) */
  int n;              /* how many: every game on All, the hearted on Favourites */
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

  int tab, tab_prev, tab_f;   /* the tab shown (TAB_*), the one before, its slide */
  int tab_ids[4], tab_count;  /* the tabs on the bar */
  int recent[30], recent_count;  /* games in recent.txt order */
  int rec_row, rec_cur;       /* Recent: 0 the Continue card, 1 the covers; which */
  int rec_from, rec_f;        /* the place the frame flies from, its frames */
  u16 *pic;                   /* the Continue card's picture, 144 x 96 */
  int pic_ok, cont_slot;      /* it was read; the slot Continue resumes (-1 none) */
  u64 cont_time;              /* that slot's save time */
  int chosen_game, chosen_slot;

  int sorts[3], tab_cur[4];   /* All, Favourites, Folders: the sort; every tab: its cursor */
  int dir_parent[MAX_DIRS], dir_games[MAX_DIRS];  /* a folder's parent; its games, all levels */
  int fold_dir, fold_count, fold_open;           /* Folders: the folder shown, its rows */
  int fold_kind[MAX_FOLD_ROWS], fold_item[MAX_FOLD_ROWS];
  int fold_cur, fold_from, fold_f;               /* the cursor and its flight */
  int fold_top, fold_top_from, fold_top_f;       /* the first row in view, its slide */
  int card, card_f, card_out, card_game;  /* the game card, its motion, its game */
  u32 card_mark;              /* pool mark: the card's memory goes when it closes */
  u16 *thumbs, *card_cover;   /* the tiles' 60 x 40 pictures; the 96 px cover */
  int tile_slot[12], tile_pic[12], tile_count;  /* -1 Start fresh, else the slot */
  u64 tile_time[12];
  int card_cur, card_from, card_cf;            /* the focused tile, its frame's flight */
  int card_first, card_first_from, card_first_f;  /* the first tile in view, scroll */
  int fav_flash, fav_changed, card_changed;
  ui_hold_t hold;             /* SELECT held: delete */
  char toast[64];
  u16 toast_edge;
  int toast_f, toast_undo;    /* -1: no toast; the slot Undo brings back, -1 none */
  char facts[96];             /* the card's cheats and settings line */
  int help, help_f, help_out; /* the No BIOS help card */
  int name_mode;              /* the card's name: NAME_* */
  char name_l1[256], name_l2[256];  /* its two 2x lines */
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
static void tell_apart(void);
static const u16 icon_heart[11];
static const u16 icon_folder[11];
static void draw_icon11(int x, int y, const u16 *bits, u16 color, int scale);
static void read_favourites(void);
static void cover_heart(int x, int y, int game);
static void view_reset(void);
static int list_banner(void);
static void draw_banner_at(int y);

static int is_game_file(const char *name)
{
  const char *dot = strrchr(name, '.');

  return dot && (!strcasecmp(dot, ".gba") || !strcasecmp(dot, ".zip"));
}

static void scan_dir(const char *path, int depth, int parent)
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
  L.dir_parent[dir] = parent;

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
        scan_dir(sub, depth + 1, dir);
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
static int clean_name(const char *name, int n, char *out)
{
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
  {
    memcpy(out, name, n);  /* nothing left: the plain name */
    o = n;
  }
  out[o] = 0;
  return o;
}

static const char *display_name(const char *name, int n)
{
  char out[256];
  int o = clean_name(name, n, out);
  return arena_str(out, o);
}

/* The i-th (...) or [...] part of a file name, brackets included.
   Returns 0 when there are fewer parts. */
static int name_part(const char *file, int i, const char **at, int *len)
{
  const char *dot = strrchr(file, '.');
  int n = dot ? dot - file : (int)strlen(file), k, start = -1, depth = 0;

  for(k = 0; k < n; k++)
  {
    char c = file[k];
    if((c == '(') || (c == '['))
    {
      if(depth++ == 0)
        start = k;
    }
    else if(((c == ')') || (c == ']')) && depth && (--depth == 0))
    {
      if(i-- == 0)
      {
        *at = file + start;
        *len = k + 1 - start;
        return 1;
      }
    }
  }
  return 0;
}

static int has_part(const char *file, const char *part, int len)
{
  const char *at;
  int i, n;

  for(i = 0; name_part(file, i, &at, &n); i++)
    if((n == len) && !strncasecmp(at, part, len))
      return 1;
  return 0;
}

static int title_order(const void *a, const void *b)
{
  return strcasecmp(L.games[*(const int *)a].title, L.games[*(const int *)b].title);
}

/* Same-named games (spec section 4, #14): when two games would show the
 * same name, each keeps the parts of its file name that tell them apart,
 * e.g. "Pokemon - FireRed Version (Rev 1)". When no part does, the whole
 * file name (without its extension) is shown. */
static void tell_apart(void)
{
  u32 mark = ui_pool_mark();
  int *idx = ui_pool_alloc(L.count * sizeof(int)), i, j, k, m;

  if(idx == NULL)
    return;
  for(i = 0; i < L.count; i++)
    idx[i] = i;
  qsort(idx, L.count, sizeof(int), title_order);
  for(i = 0; i < L.count; i = j)
  {
    for(j = i + 1; (j < L.count) && !title_order(&idx[i], &idx[j]); j++)
      ;
    if(j - i < 2)
      continue;
    for(k = i; k < j; k++)
    {
      game_t *g = &L.games[idx[k]];
      char out[256];
      const char *at;
      int o = snprintf(out, sizeof(out), "%s", g->title), p, n;
      for(p = 0; name_part(g->name, p, &at, &n); p++)
      {
        int shared = 1;
        for(m = i; m < j; m++)
          if(!has_part(L.games[idx[m]].name, at, n))
            shared = 0;
        if(!shared && (o + 1 + n < (int)sizeof(out)))
          o += snprintf(out + o, sizeof(out) - o, " %.*s", n, at);
      }
      g->title = arena_str(out, o);
      if(g->title == NULL)
        g->title = g->name;
    }
    /* still the same: the file names */
    for(k = i; k < j; k++)
      for(m = i; m < j; m++)
        if((m != k) && !strcasecmp(L.games[idx[k]].title, L.games[idx[m]].title))
        {
          game_t *g = &L.games[idx[k]];
          const char *dot = strrchr(g->name, '.');
          const char *t = arena_str(g->name, dot ? dot - g->name : (int)strlen(g->name));
          g->title = t ? t : g->name;
          break;
        }
  }
  ui_pool_release(mark);
}

/* The game started from the library and the name it showed there */
static char launched_file[MAX_FILE], launched_title[256];

/* A file's name cleaned up as the library does, without telling same-named
   games apart (the cheat engine finds a game's cheats by it). */
void ui_clean_name(const char *file, char *out)
{
  const char *dot = strrchr(file, '.');
  clean_name(file, dot ? dot - file : (int)strlen(file), out);
}

/* The name the library shows for a file (for the pause screen's chip). */
void ui_display_name(const char *file, char *out)
{
  const char *dot = strrchr(file, '.');
  int n = dot ? dot - file : (int)strlen(file);
  if(launched_file[0] && !strcmp(file, launched_file))
  {
    strcpy(out, launched_title);
    return;
  }
  clean_name(file, n, out);
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
  int i, keep = (L.n > 0) ? L.order[L.cur] : 0;

  qsort(L.order, L.n, sizeof(int), compare_games);
  /* the cursor stays on its game */
  for(i = 0; i < L.n; i++)
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
  scan_dir(g_default_rom_dir, 0, -1);
  tell_apart();
  /* every folder's games, its subfolders' included */
  memset(L.dir_games, 0, sizeof(L.dir_games));
  for(i = 0; i < L.count; i++)
  {
    int d = L.games[i].dir;
    while(d >= 0)
    {
      L.dir_games[d]++;
      d = L.dir_parent[d];
    }
  }
  read_recent();
  read_favourites();
  for(i = 0; i < L.count; i++)
    L.order[i] = i;
  L.n = L.count;
  sort_games();
  L.cur = L.cur_from = 0;  /* a fresh list opens on its first game */
  return 1;
}

/* ---- covers --------------------------------------------------------------------- */

static void covers_init(void)
{
  int i;

  L.cover_count = 0;
  for(i = 0; i < (LIBRARY_GRID ? MAX_COVERS : 8); i++)
  {
    /* only what the pool can spare, keeping room for the game card; with
       none, covers are drawn each time */
    if(ui_pool_free() < 160 * 1024 + COVER * COVER * 2)
      break;
    L.covers[i].px = ui_pool_alloc(COVER * COVER * 2);
    if(L.covers[i].px == NULL)
      break;
    L.covers[i].game = -1;
    L.cover_count++;
  }
}

/* The name plate (spec section 5, "the name is the cover"): from 32 px up
 * the name sits on a dark plate at the bottom, 12 px regular cream at x 4, on
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
      ui_print_fit(x + 4, ty, rest, UI_COL_CREAM, UI_FONT_BODY, 0, size - 8);
    else
      ui_print(x + 4, ty, lines[i], UI_COL_CREAM, UI_FONT_BODY, 0);
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
  w = ui_print_width(rest, UI_FONT_BODY, 0);
  if(L.marq_f <= UI_MARQUEE_DELAY)
    return;
  ly = y + COVER - (PLATE_PITCH * PLATE_LINES + 6) + 2 + PLATE_PITCH * 2;
  ui_fill(x, ly, COVER, PLATE_PITCH, ui_cover_plate_color(title));
  ui_clip_set(x + 4, ly, COVER - 8, PLATE_PITCH);
  ui_print(x + 4 - marquee_shift(w - (COVER - 8)), ly, rest, UI_COL_CREAM,
   UI_FONT_BODY, 0);
  ui_clip_reset();
}

/* ---- the grid --------------------------------------------------------------------- */

static int grid_rows(void)
{
  return (L.n + GRID_COLS - 1) / GRID_COLS;
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

  for(i = 0; i < L.n; i++)
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

  if(L.n == 0)
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
  int lo = pos >> 8, fr = pos & 0xFF, i, d = 0, bottom, first;

  if(lo >= L.n - 1)
  {
    lo = L.n - 1;
    fr = 0;
  }
  L.ys[lo] = LIST_FY - fr * (list_h(lo) + list_gap(lo + 1)) / 256;
  for(i = lo + 1; i < L.n; i++)
    L.ys[i] = L.ys[i - 1] + list_h(i - 1) + list_gap(i);
  for(i = lo - 1; i >= 0; i--)
    L.ys[i] = L.ys[i + 1] - list_h(i) - list_gap(i + 1);
  bottom = L.ys[L.n - 1] + list_h(L.n - 1);
  /* before the first game the No BIOS notice is the list's first line,
     24 px above row 0: the top bound moves down to make room */
  first = list_banner() ? LIST_FIRST + 22 : LIST_FIRST;
  if(L.ys[0] > first)
    d = first - L.ys[0];
  else if(bottom < LIST_LAST)
  {
    d = LIST_LAST - bottom;
    if(d > first - L.ys[0])
      d = first - L.ys[0];
  }
  for(i = 0; i < L.n; i++)
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
  u32 head[2];

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
  /* the file: "gcfg", the version, then the settings (gui.c) */
  memset(&cfg, 0, sizeof(cfg));
  if((fread(head, 4, 2, f) == 2) && !memcmp(head, "gcfg", 4))
  {
    n = fread(&cfg, 1, sizeof(cfg), f);
    if(n >= offsetof(game_config_t, play_seconds) + sizeof(u32))
      L.played = cfg.play_seconds;
  }
  fclose(f);
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
  if(g->fav)
    draw_icon11(400 - lift, y + 5 - lift, icon_heart, on_card ? UI_COL_PLUM : UI_COL_PINK, 1);
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
  for(i = 0; i < L.n; i++)
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
  if(list_banner() && (L.ys[0] - 24 + 18 > VIEW_T - 2))
    draw_banner_at(L.ys[0] - 24);
  ui_clip_reset();
  if(L.n > 1)
  {
    int ty = 176 * pos / ((L.n - 1) * 256);
    ui_round_fill(468, 42, 4, 196, 1, UI_COL_CARD);
    ui_round_fill(468, 42 + ty, 4, 20, 1, UI_COL_MUTE);
  }
}

/* the cursor frame between rows lo and lo + 1, with their current heights;
   on the open card at rest, a long name runs sideways */
static void list_selection(void)
{
  int pos = list_pos(), lo = pos >> 8, hi, fr = pos & 0xFF, y0, y1, h0, h1;

  if(L.n == 0)
    return;
  list_layout(pos);
  if(lo >= L.n - 1)
  {
    lo = L.n - 1;
    fr = 0;
  }
  hi = (lo + 1 < L.n) ? lo + 1 : lo;
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

/* ---- Recent and the Continue card (spec section 4, Home) -----------------------------
 * The first tab once a game has been played: the Continue card for the last
 * game (its Last picture, name, when, play time) and the recent games as
 * covers below it. × resumes a game from Last, or from its newest
 * savestate, here and on every list row (only the game card differs). Before any game has been played there is no Recent
 * tab and the library opens on All. */

#define RECENT_MAX   30
#define RECENT_ROW   5
#define CONT_X       20
#define CONT_Y       40
#define CONT_W       440
#define CONT_H       104
#define RECENT_Y     recent_y()  /* 166; 172 under the No BIOS notice */
#define PIC_W        144
#define PIC_H        96
#define SVS_SHOT     8                       /* the screenshot follows the 8-byte header */
#define SVS_TIME     (SVS_SHOT + 240 * 160 * 2)
#define SHOT_ROWS    20

static int banner_up(void);
static int recent_y(void)
{
  return banner_up() ? 172 : 166;
}

/* a game's savestate file for slot 0-10 */
static void state_path(int game, int slot, char *out)
{
  char ext[16], name[MAX_FILE];

  sprintf(ext, "_%d.svs", slot);
  change_ext((char *)L.games[game].name, name, ext);
  if(g_default_save_dir[0])
    path_printf(out, MAX_PATH, "%s/%s", g_default_save_dir, name);
  else
    snprintf(out, MAX_PATH, "%s", name);
}

/* the save time of a game's slot (RTC ticks); 0 when the slot is empty or
   too old to have one; -1 in *has when there is no file */
static u64 state_time(int game, int slot, int *has)
{
  char path[MAX_PATH];
  u8 head[4];
  u64 t = 0;
  SceUID fd;

  state_path(game, slot, path);
  fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
  *has = fd >= 0;
  if(fd < 0)
    return 0;
  if((sceIoRead(fd, head, 4) == 4) && !memcmp(head, "SVS", 4) &&
   (sceIoLseek32(fd, SVS_TIME, PSP_SEEK_SET) == SVS_TIME) &&
   (sceIoRead(fd, &t, 8) != 8))
    t = 0;
  sceIoClose(fd);
  return t;
}

/* Where Continue resumes a game: Last (10) when it has one, else its newest
 * savestate, else -1 (from the start). */
static int resume_slot(int game, u64 *when)
{
  int has, slot, best = -1;
  u64 t, best_t = 0;

  t = state_time(game, 10, &has);
  if(has)
  {
    *when = t;
    return 10;
  }
  for(slot = 0; slot < 10; slot++)
  {
    t = state_time(game, slot, &has);
    if(has && ((best < 0) || (t > best_t)))
    {
      best = slot;
      best_t = t;
    }
  }
  *when = best_t;
  return best;
}

/* The Continue card's picture: the resume slot's screenshot box-filtered
 * 5:3 to 144 x 96, read SHOT_ROWS rows at a time. */
static int load_resume_picture(int game, int slot)
{
  char path[MAX_PATH];
  u8 head[8];
  u16 *rows;
  SceUID fd;
  int n, ok = 0;
  u32 mark = ui_pool_mark();

  if((slot < 0) || !L.pic)
    return 0;
  rows = ui_pool_alloc(240 * SHOT_ROWS * 2);
  if(rows == NULL)
    return 0;
  state_path(game, slot, path);
  fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
  if(fd >= 0)
  {
    if((sceIoRead(fd, head, 8) == 8) && !memcmp(head, "SVS", 4))
    {
      for(n = 0; n < 160 / SHOT_ROWS; n++)
      {
        if(sceIoRead(fd, rows, 240 * SHOT_ROWS * 2) != 240 * SHOT_ROWS * 2)
          break;
        ui_shrink_53_rows(rows, SHOT_ROWS, L.pic + n * (SHOT_ROWS * 3 / 5) * PIC_W);
      }
      ok = (n == 160 / SHOT_ROWS);
    }
    sceIoClose(fd);
  }
  ui_pool_release(mark);  /* the rows only lived for the read */
  return ok;
}

/* "today 21:46" or "11/13" from RTC ticks in UTC */
static void when_text(u64 tick, char *out, int n)
{
  u64 local;
  ScePspDateTime t, now;

  out[0] = 0;
  if((tick == 0) || (sceRtcConvertUtcToLocalTime(&tick, &local) < 0) ||
   (sceRtcSetTick(&t, &local) < 0))
    return;
  sceRtcGetCurrentClockLocalTime(&now);
  if((t.year == now.year) && (t.month == now.month) && (t.day == now.day))
    snprintf(out, n, "today %02d:%02d", t.hour, t.minute);
  else
    snprintf(out, n, "%02d/%02d", t.month, t.day);
}

static void recent_init(void)
{
  int i, k, n = 0;

  /* games in recent.txt order (read_recent numbered them) */
  for(k = 0; (k < RECENT_MAX) && (n < RECENT_MAX); k++)
    for(i = 0; i < L.count; i++)
      if(L.games[i].recent == k)
        L.recent[n++] = i;
  L.recent_count = n;
  L.rec_row = 0;
  L.rec_cur = L.rec_from = 0;
  L.rec_f = UI_F_CURSOR;
  if(n == 0)
    return;
  L.pic = ui_pool_alloc(PIC_W * PIC_H * 2);
  L.cont_slot = resume_slot(L.recent[0], &L.cont_time);
  L.pic_ok = load_resume_picture(L.recent[0], L.cont_slot);
}

/* the cursor's box for a place on the Recent tab: 0 the Continue card,
   1 + k the k-th cover (both as lifted) */
static void recent_box(int place, int *x, int *y, int *w, int *h)
{
  if(place == 0)
  {
    *x = CONT_X - UI_LIFT;
    *y = CONT_Y - UI_LIFT;
    *w = CONT_W;
    *h = CONT_H;
  }
  else
  {
    *x = GRID_X + (place - 1) * GRID_PX - UI_LIFT;
    *y = RECENT_Y - UI_LIFT;
    *w = *h = COVER;
  }
}

static int recent_place(void)
{
  return L.rec_row ? 1 + L.rec_cur : 0;
}

/* A headline title (spec section 4, Long game names; the Continue card and
 * the game card): 12 px bold at 2x when it fits w; else 12 px bold on up
 * to two lines 16 px apart, 2 px higher, so the block keeps the 2x
 * title's height. Past two lines the second is cut with "…"; with shift >
 * 0 (the focused card's marquee) it runs sideways instead, over bg. */
/* Line n of a headline wrapped at w, measured in bold as it is drawn:
   breaks at spaces; text without spaces (CJK) breaks between characters
   (ui_wrap_line). Returns 0 past the end. */
static int bold_line(const char *t, int n, int w, char *out, int size)
{
  const char *p = t;
  int line = 0;

  if(!strchr(t, ' '))
    return ui_wrap_line(t, n, w, UI_FONT_BODY, out, size);
  while(*p)
  {
    const char *end = p, *q;
    char buf[256];
    while(*p == ' ')
      p++;
    if(!*p)
      break;
    /* take words while the line, in bold, fits w */
    end = p;
    for(q = p; ; )
    {
      const char *we = q;
      int len;
      while(*we && (*we != ' '))
        we++;
      len = we - p;
      if(len >= (int)sizeof(buf))
        break;
      memcpy(buf, p, len);
      buf[len] = 0;
      if((end != p) && (ui_print_width(buf, UI_FONT_BODY, UI_TEXT_BOLD) > w))
        break;
      end = we;
      if(!*we)
        break;
      q = we + 1;
    }
    if(line == n)
    {
      int len = end - p;
      if(len >= size)
        len = size - 1;
      memcpy(out, p, len);
      out[len] = 0;
      return 1;
    }
    line++;
    p = end;
  }
  return 0;
}

static void big_title(int x, int y, int w, const char *t, u16 ink, u16 bg, int shift)
{
  char line[128], rest[256];
  int n, k;

  if(ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X) <= w)
  {
    ui_print(x, y, t, ink, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);
    return;
  }
  rest[0] = 0;
  for(n = 0; bold_line(t, n, w, line, sizeof(line)); n++)
  {
    char *l = line;
    while(*l == ' ')
      l++;
    if(n == 0)
      ui_print(x, y - 2, l, ink, UI_FONT_BODY, UI_TEXT_BOLD);
    else
    {
      k = strlen(rest);
      snprintf(rest + k, sizeof(rest) - k, "%s%s", k ? " " : "", l);
    }
  }
  if(rest[0] == 0)
    return;
  if(shift <= 0)
  {
    ui_print_fit(x, y + 14, rest, ink, UI_FONT_BODY, UI_TEXT_BOLD, w);
    return;
  }
  ui_fill(x, y + 14, w, 16, bg);
  ui_clip_set(x, y + 14, w, 16);
  ui_print(x - shift, y + 14, rest, ink, UI_FONT_BODY, UI_TEXT_BOLD);
  ui_clip_reset();
}

/* how far a headline's second line runs past w (0: it fits, or one line) */
static int big_title_over(const char *t, int w)
{
  char line[128], rest[256];
  int n, k;

  if(ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X) <= w)
    return 0;
  rest[0] = 0;
  for(n = 1; bold_line(t, n, w, line, sizeof(line)); n++)
  {
    char *l = line;
    while(*l == ' ')
      l++;
    k = strlen(rest);
    snprintf(rest + k, sizeof(rest) - k, "%s%s", k ? " " : "", l);
  }
  k = ui_print_width(rest, UI_FONT_BODY, UI_TEXT_BOLD) - w;
  return (k > 0) ? k : 0;
}

static void draw_continue(int hero)
{
  int game = L.recent[0], lift = hero ? UI_LIFT : 0, x, y, w;
  u16 ink = hero ? UI_COL_PLUM : UI_COL_CREAM;
  char meta[96], when[32], played[32];
  const char *chip = "CONTINUE";

  if(hero)
    ui_round_fill(CONT_X + 1, CONT_Y + 1, CONT_W, CONT_H, 3, UI_COL_SHADE);
  x = CONT_X - lift;
  y = CONT_Y - lift;
  ui_round_fill(x, y, CONT_W, CONT_H, 3, hero ? UI_COL_TANGERINE : UI_COL_CARD);
  ui_fill(x + 4, y + 4, PIC_W, PIC_H, UI_COL_PLUM);
  if(L.pic_ok)
    ui_blit(L.pic, PIC_W, x + 4, y + 4, PIC_W, PIC_H);
  else  /* nothing saved yet: its cover */
    draw_cover_any(x + 4 + (PIC_W - PIC_H) / 2, y + 4, PIC_H, L.games[game].title, 0);
  w = ui_print_width(chip, UI_FONT_SMALL, 0) + 12;
  ui_round_fill(x + 160, y + 10, w, 16, 2, hero ? UI_COL_PLUM : UI_COL_TANGERINE);
  ui_print(x + 166, y + 11, chip, hero ? UI_COL_TANGERINE : UI_COL_PLUM, UI_FONT_SMALL, 0);
  big_title(x + 160, y + 30, 270, L.games[game].title, ink, 0, 0);
  when_text(L.cont_time, when, sizeof(when));
  played_text(game_played(game), played, sizeof(played));
  if(L.cont_slot == 10)
    snprintf(meta, sizeof(meta), "Last \xC2\xB7 %s  \xC2\xB7  %s", when, played);
  else if(L.cont_slot >= 0)
    snprintf(meta, sizeof(meta), "Slot %d \xC2\xB7 %s  \xC2\xB7  %s", L.cont_slot, when, played);
  else
    snprintf(meta, sizeof(meta), "%s", played);
  ui_print_fit(x + 160, y + 64, meta, ink, UI_FONT_SMALL, 0, 270);
  draw_cover_any(x + 160, y + 82, 16, L.games[game].title, 0);
  ui_print_fit(x + 182, y + 82, L.games[game].name,
   hero ? COLOR16(14, 8, 7) : COLOR16(24, 21, 22), UI_FONT_SMALL, 0, 248);
}

static void recent_ground(void)
{
  int i, n = (L.recent_count < RECENT_ROW) ? L.recent_count : RECENT_ROW;

  draw_continue(L.rec_row == 0);
  if(!banner_up())  /* the No BIOS notice takes this line */
    ui_print(20, 146, "Recent", UI_COL_MUTE, UI_FONT_SMALL, 0);
  for(i = 0; i < n; i++)
  {
    draw_cover(GRID_X + i * GRID_PX, RECENT_Y, L.recent[i]);
    cover_heart(GRID_X + i * GRID_PX, RECENT_Y, L.recent[i]);
  }
}

static void recent_selection(void)
{
  int x0, y0, w0, h0, x1, y1, w1, h1;

  if(L.rec_row == 1)
  {
    int x = GRID_X + L.rec_cur * GRID_PX;
    ui_fill(x + 1, RECENT_Y + 1, COVER, COVER, UI_COL_SHADE);
    draw_cover(x - UI_LIFT, RECENT_Y - UI_LIFT, L.recent[L.rec_cur]);
    grid_marquee(x - UI_LIFT, RECENT_Y - UI_LIFT, L.recent[L.rec_cur]);
    cover_heart(x - UI_LIFT, RECENT_Y - UI_LIFT, L.recent[L.rec_cur]);
  }
  /* the focused Continue card: a long name's second line runs sideways */
  if((L.rec_row == 0) && (L.rec_f >= UI_F_CURSOR))
  {
    const char *t = L.games[L.recent[0]].title;
    int shift = marquee_shift(big_title_over(t, 270));
    if(shift > 0)
      big_title(CONT_X - UI_LIFT + 160, CONT_Y - UI_LIFT + 30, 270, t, UI_COL_PLUM,
       UI_COL_TANGERINE, shift);
  }
  /* the frame flies straight to the new place, also between the card and
     the row */
  recent_box(L.rec_from, &x0, &y0, &w0, &h0);
  recent_box(recent_place(), &x1, &y1, &w1, &h1);
  ui_cursor_frame(ui_ease_out_back(x0, x1, L.rec_f, UI_F_CURSOR),
   ui_ease_out_back(y0, y1, L.rec_f, UI_F_CURSOR),
   ui_ease_out_back(w0, w1, L.rec_f, UI_F_CURSOR),
   ui_ease_out_back(h0, h1, L.rec_f, UI_F_CURSOR));
}

static void recent_move(void)
{
  int n = (L.recent_count < RECENT_ROW) ? L.recent_count : RECENT_ROW;
  int from = recent_place(), row = L.rec_row, cur = L.rec_cur;

  if((row == 0) && ui_repeat(PSP_CTRL_DOWN))
    row = 1;
  else if((row == 1) && ui_repeat(PSP_CTRL_UP))
    row = 0;
  else if(row == 1)
    cur = ui_step(cur, n, PSP_CTRL_LEFT, PSP_CTRL_RIGHT);
  if((row != L.rec_row) || (cur != L.rec_cur))
  {
    L.rec_from = from;
    L.rec_row = row;
    L.rec_cur = cur;
    L.rec_f = 0;
    L.marq_f = 0;
    L.dirty = 1;  /* the Continue card's look changes with the row */
  }
}

/* ---- Favourites ------------------------------------------------------------------------
 * The hearted games, kept in CFG/favourites.txt (one full path per line);
 * △ on the game card turns a game's heart on or off. */

static const u16 icon_heart[11] =
 { 0x000, 0x38E, 0x7DF, 0x7FF, 0x7FF, 0x3FE, 0x1FC, 0x0F8, 0x070, 0x020, 0x000 };

static void draw_icon11(int x, int y, const u16 *bits, u16 color, int scale)
{
  int r, c;

  for(r = 0; r < 11; r++)
    for(c = 0; c < 11; c++)
      if(bits[r] & (1 << (10 - c)))
        ui_fill(x + c * scale, y + r * scale, scale, scale, color);
}

static void read_favourites(void)
{
  char path[MAX_PATH], line[MAX_PATH];
  FILE *f;

  path_printf(path, sizeof(path), "%s/favourites.txt", g_default_cfg_dir);
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
      if(!strcasecmp(L.games[i].name, slash + 1) && !strcasecmp(L.dirs[L.games[i].dir], line))
        L.games[i].fav = 1;
  }
  fclose(f);
}

static void write_favourites(void)
{
  char path[MAX_PATH];
  FILE *f;
  int i;

  path_printf(path, sizeof(path), "%s/favourites.txt", g_default_cfg_dir);
  f = fopen(path, "w");
  if(f == NULL)
    return;
  for(i = 0; i < L.count; i++)
    if(L.games[i].fav)
      fprintf(f, "%s/%s\n", L.dirs[L.games[i].dir], L.games[i].name);
  fclose(f);
}

/* the heart corner on a 64 px cover */
static void cover_heart(int x, int y, int game)
{
  if(L.games[game].fav)
    draw_icon11(x + 52, y + 2, icon_heart, UI_COL_PINK, 1);
}

/* Favourites with nothing hearted yet: the empty folder's layout with a
   pink heart */
static void draw_no_favourites(void)
{
  static const char t[] = "No favourites yet";
  static const char b[] = "\xE2\x96\xB3 on a game, then \xE2\x96\xB3 again, to add it here";

  ui_round_fill(208, 82, 64, 52, 3, UI_COL_CARD);
  draw_icon11(224, 92, icon_heart, UI_COL_PINK, 3);
  ui_print(240 - ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD) / 2, 146, t, UI_COL_CREAM,
   UI_FONT_BODY, UI_TEXT_BOLD);
  ui_print(240 - ui_print_width(b, UI_FONT_SMALL, 0) / 2, 166, b, UI_COL_MUTE, UI_FONT_SMALL, 0);
}

/* ---- the game card (spec section 4, Game card; r4 card, r5 card_delete) -------------------
 * △ on any game: a card over the library with the game's cover, facts and
 * its savestates as tiles (Start fresh first, then every used slot newest
 * first, the newest focused). × plays from the focused tile, △ hearts the
 * game, SELECT held deletes the focused savestate (Undo on the toast), ○
 * drops the card away. */

#define CARD_X      14
#define CARD_Y      22
#define CARD_W      452
#define CARD_H      222
#define TILE_W      68
#define TILE_H      76
#define TILE_PITCH  72
#define TILES_SEEN  6
#define THUMB_W     60
#define THUMB_H     40
#define CARD_COVER  96

/* the pause tiles' 20 x 20 play icon (design icons20.py), as Start fresh's
   picture: per row ink then fill, bit 19 the left pixel; a 1 px plum
   shadow down-right, plum ink, lime fill */
static const u32 play20[2][20] =
{
  { 0x00000, 0x00000, 0x00000, 0x0C000, 0x0B000, 0x08C00, 0x08300, 0x080C0, 0x08030, 0x0800C,
    0x0800C, 0x08030, 0x080C0, 0x08300, 0x08C00, 0x0B000, 0x0C000, 0x00000, 0x00000, 0x00000 },
  { 0x00000, 0x00000, 0x00000, 0x00000, 0x04000, 0x07000, 0x07C00, 0x07F00, 0x07FC0, 0x07FF0,
    0x07FF0, 0x07FC0, 0x07F00, 0x07C00, 0x07000, 0x04000, 0x00000, 0x00000, 0x00000, 0x00000 }
};

static void draw_play20(int x, int y)
{
  int r, c;

  for(r = 0; r < 20; r++)
    for(c = 0; c < 20; c++)
      if((play20[0][r] | play20[1][r]) & (0x80000 >> c))
        ui_fill(x + c + 1, y + r + 1, 1, 1, UI_COL_PLUM);
  for(r = 0; r < 20; r++)
    for(c = 0; c < 20; c++)
    {
      if(play20[0][r] & (0x80000 >> c))
        ui_fill(x + c, y + r, 1, 1, UI_COL_PLUM);
      else if(play20[1][r] & (0x80000 >> c))
        ui_fill(x + c, y + r, 1, 1, UI_COL_LIME);
    }
}

/* the card's tiles: -1 Start fresh, else the slot, newest first */
static void card_read_tiles(void)
{
  char path[MAX_PATH];
  int slot, i, n = 1;
  u64 t;

  L.tile_slot[0] = -1;
  L.tile_pic[0] = 0;
  for(slot = 0; slot <= 10; slot++)
  {
    u32 got;
    state_path(L.card_game, slot, path);
    got = read_thumbnail_for(path, L.thumbs + n * THUMB_W * THUMB_H, &t);
    if(!got)
      continue;
    L.tile_slot[n] = slot;
    L.tile_time[n] = t;
    L.tile_pic[n] = (got == 2);
    /* newest first: move it up past older ones */
    for(i = n; (i > 1) && (L.tile_time[i] > L.tile_time[i - 1]); i--)
    {
      int s = L.tile_slot[i], p = L.tile_pic[i];
      u64 tt = L.tile_time[i];
      u16 tmp[THUMB_W * THUMB_H];
      L.tile_slot[i] = L.tile_slot[i - 1];
      L.tile_time[i] = L.tile_time[i - 1];
      L.tile_pic[i] = L.tile_pic[i - 1];
      L.tile_slot[i - 1] = s;
      L.tile_time[i - 1] = tt;
      L.tile_pic[i - 1] = p;
      memcpy(tmp, L.thumbs + i * THUMB_W * THUMB_H, sizeof(tmp));
      memcpy(L.thumbs + i * THUMB_W * THUMB_H, L.thumbs + (i - 1) * THUMB_W * THUMB_H, sizeof(tmp));
      memcpy(L.thumbs + (i - 1) * THUMB_W * THUMB_H, tmp, sizeof(tmp));
    }
    n++;
  }
  L.tile_count = n;
}

/* How many cheats a game's .cht file holds (as add_cheats() counts them). */
static int cht_count(int game)
{
  static const char *kinds[] = { "gameshark_v1", "gameshark_v2", "gameshark_v3", "PAR_v1",
   "PAR_v2", "PAR_v3", "direct_v1", "direct_v2", "direct_v3", NULL };
  char name[MAX_FILE], path[MAX_PATH], line[256];
  FILE *f;
  int n = 0, k;

  change_ext((char *)L.games[game].name, name, ".cht");
  path_printf(path, sizeof(path), "%s/%s", g_default_cheat_dir, name);
  f = fopen(path, "rb");
  if(f == NULL)
    return 0;
  while(fgets(line, sizeof(line), f) && (n < MAX_CHEATS))
  {
    char *sp = strchr(line, ' ');
    if(sp)
      *sp = 0;
    for(k = 0; kinds[k]; k++)
      if(!strcasecmp(line, kinds[k]))
      {
        n++;
        break;
      }
  }
  fclose(f);
  return n;
}

/* The card's facts line: "Cheats: 2 / 8  ·  Settings: 5 saved for this
 * game", from the running game's config or the game's .cfg. */
static void card_facts(int game)
{
  const game_t *g = &L.games[game];
  const game_config_t *cfg = NULL;
  int total = cht_count(game), on = 0, saved = 0, i;
  u32 mark = ui_pool_mark();
  char ch[32], st[48];

  if(gamepak_filename[0] && !strcasecmp(gamepak_filename, g->name) &&
   !strcasecmp(rom_path, L.dirs[g->dir]))
    cfg = &g_game_config;  /* the game behind the pause screen */
  else
  {
    char name[MAX_FILE], path[MAX_PATH];
    game_config_t *c = ui_pool_alloc(sizeof(game_config_t));
    FILE *f;
    change_ext((char *)g->name, name, ".cfg");
    path_printf(path, sizeof(path), "%s/%s", g_default_cfg_dir, name);
    f = c ? fopen(path, "rb") : NULL;
    if(f)
    {
      u32 head[2];
      if((fread(head, 4, 2, f) == 2) && !memcmp(head, "gcfg", 4) &&
       (fread(c, 1, sizeof(*c), f) >= offsetof(game_config_t, rapidfire_speed)))
        cfg = c;
      fclose(f);
    }
  }
  if(cfg)
  {
    for(i = 0; i < total; i++)
      on += (cfg->cheats_flag[i].cheat_active == 1);
    saved = ui_settings_saved_for_game(cfg);
  }
  if(total)
    snprintf(ch, sizeof(ch), "Cheats: %d / %d", on, total);
  else
    snprintf(ch, sizeof(ch), "Cheats: none");
  if(saved)
    snprintf(st, sizeof(st), "Settings: %d saved for this game", saved);
  else
    snprintf(st, sizeof(st), "Settings: none saved for this game");
  snprintf(L.facts, sizeof(L.facts), "%s  \xC2\xB7  %s", ch, st);
  ui_pool_release(mark);
}

/* ---- the game card's name ("B balanced", maintainer, 2026-11-20) ----------------------
 * 316 px wide. 2x on one line when it fits; else 2x on two lines, each at
 * most 158 px at 1x bold, broken after a subtitle mark (" -", ":", "~" and
 * their full-width forms; the mark stays on line 1), else at the break
 * that makes the two lines closest in width. Too long for that: 1x on one
 * line (the rest moves up 12 px), or the headline rule (big_title) on two.
 * No file name line: the facts sit 22 px under the chips. */
enum { NAME_2X, NAME_2X_TWO, NAME_1X, NAME_BIG };

static int cjk_cp(u32 c)
{
  return ((c >= 0x3000) && (c <= 0x9FFF)) || ((c >= 0xFF00) && (c <= 0xFFEF));
}

static u32 utf8_at(const char *s, int *len)
{
  const u8 *u = (const u8 *)s;
  if(u[0] < 0x80) { *len = 1; return u[0]; }
  if((u[0] & 0xE0) == 0xC0) { *len = 2; return ((u[0] & 0x1F) << 6) | (u[1] & 0x3F); }
  if((u[0] & 0xF0) == 0xE0)
  {
    *len = 3;
    return ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F);
  }
  *len = 4;
  return 0xFFFD;
}

static int ends_with(const char *a, int n, const char *mark)
{
  int m = strlen(mark);
  return (n >= m) && !memcmp(a + n - m, mark, m);
}

/* The two lines for t (each at most w at 1x bold), or 0. */
static int name_lines2(const char *t, int w, char *l1, char *l2, int size)
{
  int best_rank = 3, best_diff = 0, i = 0, prev_cjk = 0, len;

  while(t[i])
  {
    u32 c = utf8_at(t + i, &len);
    if((i > 0) && ((c == ' ') || prev_cjk))
    {
      char a[256], b[256];
      int na = i, wa, wb, rank, diff;
      const char *bs = t + i;
      while((na > 0) && (t[na - 1] == ' '))
        na--;
      while(*bs == ' ')
        bs++;
      if((na > 0) && *bs && (na < (int)sizeof(a)) && (strlen(bs) < sizeof(b)))
      {
        memcpy(a, t, na);
        a[na] = 0;
        snprintf(b, sizeof(b), "%s", bs);
        wa = ui_print_width(a, UI_FONT_BODY, UI_TEXT_BOLD);
        wb = ui_print_width(b, UI_FONT_BODY, UI_TEXT_BOLD);
        if((wa <= w) && (wb <= w))
        {
          rank = (ends_with(a, na, " -") || ends_with(a, na, ":") || ends_with(a, na, "~") ||
           ends_with(a, na, "\xEF\xBC\x9A") || ends_with(a, na, "\xEF\xBD\x9E")) ? 0 : 1;
          diff = (wa > wb) ? wa - wb : wb - wa;
          if((rank < best_rank) || ((rank == best_rank) && (diff < best_diff)))
          {
            best_rank = rank;
            best_diff = diff;
            snprintf(l1, size, "%s", a);
            snprintf(l2, size, "%s", b);
          }
        }
      }
    }
    prev_cjk = cjk_cp(c);
    i += len;
  }
  return best_rank < 3;
}

static void card_name_layout(const char *t)
{
  if(ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X) <= 316)
    L.name_mode = NAME_2X;
  else if(name_lines2(t, 158, L.name_l1, L.name_l2, sizeof(L.name_l1)))
    L.name_mode = NAME_2X_TWO;
  else if(ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD) <= 316)
    L.name_mode = NAME_1X;
  else
    L.name_mode = NAME_BIG;
}

static int card_open(int game)
{
  L.card_mark = ui_pool_mark();
  L.thumbs = ui_pool_alloc(12 * THUMB_W * THUMB_H * 2);
  L.card_cover = ui_pool_alloc(CARD_COVER * CARD_COVER * 2);
  if(!L.thumbs || !L.card_cover)
  {
    ui_pool_release(L.card_mark);
    return 0;
  }
  L.card_game = game;
  card_facts(game);
  card_name_layout(L.games[game].title);
  card_read_tiles();
  /* the art alone: the name is beside it */
  ui_cover_art(L.card_cover, CARD_COVER, CARD_COVER, L.games[game].title);
  L.card = 1;
  L.card_f = 0;
  L.card_out = 0;
  L.card_cur = L.card_from = (L.tile_count > 1) ? 1 : 0;  /* the newest */
  L.card_cf = UI_F_CURSOR;
  L.card_first = 0;
  L.card_first_from = 0;
  L.card_first_f = UI_F_SCROLL;
  L.fav_flash = UI_F_FLASH;
  L.card_changed = 0;
  L.marq_f = 0;  /* a long name waits again before it runs */
  memset(&L.hold, 0, sizeof(L.hold));
  L.toast_f = -1;
  L.dirty = 1;
  return 1;
}

static void card_close_now(void)
{
  L.card = 0;
  ui_pool_release(L.card_mark);
  L.thumbs = NULL;
  L.card_cover = NULL;
  if(L.card_changed && (L.recent_count > 0))
  {
    /* a savestate went or came back: Continue may resume another one */
    L.cont_slot = resume_slot(L.recent[0], &L.cont_time);
    L.pic_ok = load_resume_picture(L.recent[0], L.cont_slot);
  }
  L.dirty = 1;
}

static int card_y(void)
{
  if(L.card_out)
    return CARD_Y + ui_ease_in_cubic(0, 250, L.card_f, UI_F_POPUP_OUT);
  return CARD_Y + 250 - ui_ease_out_back(0, 250, L.card_f, UI_F_POPUP_IN);
}

static int card_first_px(void)
{
  return ui_ease_out_back(L.card_first_from, L.card_first * TILE_PITCH, L.card_first_f,
   UI_F_SCROLL);
}

static void slot_label(int slot, char *out, int n)
{
  if(slot < 0)
    snprintf(out, n, "Start fresh");
  else if(slot == 10)
    snprintf(out, n, "Last");
  else
    snprintf(out, n, "Slot %d", slot);
}

/* "21:46" today, else "11/13" */
static void tile_when(u64 tick, char *out, int n)
{
  when_text(tick, out, n);
  if(!strncmp(out, "today ", 6))
    memmove(out, out + 6, strlen(out + 6) + 1);
}

static void draw_card(void)
{
  const game_t *g = &L.games[L.card_game];
  int x = CARD_X, y = card_y(), i, cx, first = card_first_px(), dy;
  char chip[48], size[16], played[32];

  ui_round_fill(x + 3, y + 3, CARD_W, CARD_H, 3, UI_COL_SHADE);
  ui_round_fill(x, y, CARD_W, CARD_H, 3, UI_COL_CARD);
  ui_fill(x + 4, y, CARD_W - 8, 4, UI_COL_TANGERINE);
  ui_fill(x + 12, y + 14, 100, 100, UI_COL_PLUM);
  ui_blit(L.card_cover, CARD_COVER, x + 14, y + 16, CARD_COVER, CARD_COVER);
  /* the name (card_name_layout); dy moves what follows it */
  switch(L.name_mode)
  {
    case NAME_2X:
      ui_print(x + 124, y + 12, g->title, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);
      dy = 0;
      break;
    case NAME_2X_TWO:
      ui_print(x + 124, y + 10, L.name_l1, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);
      ui_print(x + 124, y + 38, L.name_l2, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);
      dy = 28;
      break;
    case NAME_1X:
      ui_print(x + 124, y + 12, g->title, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD);
      dy = -12;
      break;
    default:
      /* the card is focused: a cut second line runs sideways */
      big_title(x + 124, y + 12, 316, g->title, UI_COL_CREAM, UI_COL_CARD,
       (L.card_f >= UI_F_POPUP_IN) ? marquee_shift(big_title_over(g->title, 316)) : 0);
      dy = 0;
      break;
  }

  /* chips: size, play time, the heart (△ flashes it) */
  cx = x + 124;
  size_text(g->size, size, sizeof(size));
  snprintf(chip, sizeof(chip), "GBA \xC2\xB7 %s", size);
  ui_round_fill(cx, y + 46 + dy, ui_print_width(chip, UI_FONT_SMALL, 0) + 12, 16, 2, UI_COL_PLUM);
  ui_print(cx + 6, y + 47 + dy, chip, UI_COL_TANGERINE, UI_FONT_SMALL, 0);
  cx += ui_print_width(chip, UI_FONT_SMALL, 0) + 18;
  played_text(game_played(L.card_game), played, sizeof(played));
  ui_round_fill(cx, y + 46 + dy, ui_print_width(played, UI_FONT_SMALL, 0) + 12, 16, 2, UI_COL_PLUM);
  ui_print(cx + 6, y + 47 + dy, played, UI_COL_SKY, UI_FONT_SMALL, 0);
  cx += ui_print_width(played, UI_FONT_SMALL, 0) + 18;
  {
    int w = 11 + 4 + ui_print_width("Favourite", UI_FONT_SMALL, 0) + 12;
    u16 ink = g->fav ? UI_COL_PINK : UI_COL_MUTE;
    u16 fill = (L.fav_flash < UI_F_FLASH) ? ((L.fav_flash < UI_F_FLASH / 2) ? UI_COL_CREAM :
     COLOR16(18, 16, 16)) : UI_COL_PLUM;
    ui_round_fill(cx, y + 46 + dy, w, 16, 2, fill);
    draw_icon11(cx + 5, y + 49 + dy, icon_heart, ink, 1);
    ui_print(cx + 20, y + 47 + dy, "Favourite", ink, UI_FONT_SMALL, 0);
  }
  /* the facts line, 22 px under the chips */
  ui_print_fit(x + 124, y + 68 + dy, L.facts,
   UI_COL_MUTE, UI_FONT_SMALL, 0, 300);

  /* the savestates */
  ui_print(x + 12, y + 118, "Savestates", UI_COL_MUTE, UI_FONT_SMALL, 0);
  ui_clip_set(x + 8, y + 130, CARD_W - 16, 90);
  for(i = 0; i < L.tile_count; i++)
  {
    int sx = x + 12 + i * TILE_PITCH - first, sy = y + 138, sel = (i == L.card_cur);
    int lift = sel ? UI_LIFT : 0;
    char label[24], when[24];
    if((sx + TILE_W < x) || (sx > x + CARD_W))
      continue;
    if(sel)
      ui_round_fill(sx + 1, sy + 1, TILE_W, TILE_H, 2, UI_COL_SHADE);
    ui_round_fill(sx - lift, sy - lift, TILE_W, TILE_H, 2,
     sel ? ((L.tile_slot[i] < 0) ? UI_COL_TANGERINE : UI_COL_SKY) : UI_COL_PLUM_2);
    sx -= lift;
    sy -= lift;
    ui_fill(sx + 4, sy + 3, THUMB_W, THUMB_H, UI_COL_PLUM);
    if(L.tile_slot[i] < 0)
      draw_play20(sx + 24, sy + 13);
    else if(L.tile_pic[i])
      ui_blit(L.thumbs + i * THUMB_W * THUMB_H, THUMB_W, sx + 4, sy + 3, THUMB_W, THUMB_H);
    slot_label(L.tile_slot[i], label, sizeof(label));
    ui_print_fit(sx + 4, sy + 47, label, sel ? UI_COL_PLUM : UI_COL_CREAM, UI_FONT_SMALL, 0, 62);
    if(L.tile_slot[i] >= 0)
    {
      tile_when(L.tile_time[i], when, sizeof(when));
      ui_print(sx + 4, sy + 60, when, sel ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_SMALL, 0);
    }
  }
  /* the cursor frame flies along the strip */
  {
    int fx = ui_ease_out_back(L.card_from * TILE_PITCH, L.card_cur * TILE_PITCH, L.card_cf,
     UI_F_CURSOR);
    ui_cursor_frame(x + 12 + fx - first - UI_LIFT, y + 136, TILE_W, TILE_H);
  }
  ui_clip_reset();

  /* SELECT held on a savestate: the pause screen's hold card over its picture */
  if(L.hold.shown && (L.tile_slot[L.card_cur] >= 0))
  {
    static const char verb[] = "Delete (hold)";
    int sx = x + 12 + L.card_cur * TILE_PITCH - first - UI_LIFT, sy = y + 138 - UI_LIFT;
    int cw = ui_print_width(verb, UI_FONT_SMALL, 0) + 20, c0, c1;
    if(cw < 96)
      cw = 96;
    ui_checker(sx + 4, sy + 3, THUMB_W, THUMB_H, UI_COL_SHADE);
    c0 = sx + 34 - cw / 2;
    if(c0 < x + 8) c0 = x + 8;
    if(c0 > x + CARD_W - 8 - cw) c0 = x + CARD_W - 8 - cw;
    c1 = sy + 3 + 3;
    ui_round_fill(c0 + 3, c1 + 3, cw, 34, 2, UI_COL_SHADE);
    ui_round_fill(c0, c1, cw, 34, 2, UI_COL_CARD);
    ui_print(c0 + (cw - ui_print_width(verb, UI_FONT_SMALL, 0)) / 2, c1 + 3, verb,
     UI_COL_CREAM, UI_FONT_SMALL, 0);
    ui_fill(c0 + 8, c1 + 22, cw - 16, 6, UI_COL_PLUM_2);
    ui_fill(c0 + 8, c1 + 22, (cw - 16) * L.hold.p / 256, 6, UI_COL_PINK);
  }
}

static void card_select(int i)
{
  if((i < 0) || (i >= L.tile_count) || (i == L.card_cur))
    return;
  L.card_from = L.card_cur;
  L.card_cur = i;
  L.card_cf = 0;
  L.dirty = 1;  /* SELECT Hold: delete shows on a savestate only */
  if((i < L.card_first) || (i >= L.card_first + TILES_SEEN))
  {
    L.card_first_from = card_first_px();
    L.card_first = (i < L.card_first) ? i : i - TILES_SEEN + 1;
    L.card_first_f = 0;
  }
  memset(&L.hold, 0, sizeof(L.hold));
}

static void card_toast(const char *text, u16 edge, int undo_slot)
{
  snprintf(L.toast, sizeof(L.toast), "%s", text);
  L.toast_edge = edge;
  L.toast_undo = undo_slot;
  L.toast_f = 0;
  L.dirty = 1;  /* SELECT reads Undo while it is up */
}

/* returns 1 when × chose a tile to play */
static int card_input(void)
{
  if(L.card_out)
    return 0;
  if(ui_pressed(ui_key_back))
  {
    L.card_out = 1;
    L.card_f = 0;
    return 0;
  }
  if(ui_pressed(PSP_CTRL_TRIANGLE))
  {
    L.games[L.card_game].fav ^= 1;
    write_favourites();
    L.fav_flash = 0;
    L.fav_changed = 1;
    return 0;
  }
  /* while the toast offers Undo, SELECT is Undo */
  if((L.toast_f >= 0) && (L.toast_undo >= 0) && ui_pressed(PSP_CTRL_SELECT))
  {
    char path[MAX_PATH];
    int slot = L.toast_undo, i;
    state_path(L.card_game, slot, path);
    undo_state_files(path);
    card_read_tiles();
    for(i = 0; i < L.tile_count; i++)
      if(L.tile_slot[i] == slot)
        card_select(i);
    L.card_changed = 1;
    card_toast("The savestate is back", UI_COL_SKY, -1);
    memset(&L.hold, 0, sizeof(L.hold));
    return 0;
  }
  if(L.tile_slot[L.card_cur] >= 0)
  {
    ui_hold_step(&L.hold, PSP_CTRL_SELECT);
    if(L.hold.done)
    {
      char path[MAX_PATH], text[48], label[24];
      int slot = L.tile_slot[L.card_cur];
      state_path(L.card_game, slot, path);
      delete_state_files(path);
      card_read_tiles();
      if(L.card_cur >= L.tile_count)
        L.card_cur = L.card_from = L.tile_count - 1;
      L.card_changed = 1;
      slot_label(slot, label, sizeof(label));
      snprintf(text, sizeof(text), "%s deleted", label);
      card_toast(text, UI_COL_PINK, slot);
      return 0;
    }
  }
  card_select(ui_step(L.card_cur, L.tile_count, PSP_CTRL_LEFT, PSP_CTRL_RIGHT));
  if(ui_pressed(ui_key_confirm) && !L.hold.shown)
  {
    L.chosen_game = L.card_game;
    L.chosen_slot = L.tile_slot[L.card_cur];
    return 1;
  }
  return 0;
}

static void card_step(void)
{
  if(L.card_f < (L.card_out ? UI_F_POPUP_OUT : UI_F_POPUP_IN))
    L.card_f++;
  else if(L.card_out)
  {
    card_close_now();
    return;
  }
  if(L.card_cf < UI_F_CURSOR) L.card_cf++;
  if(L.card_first_f < UI_F_SCROLL) L.card_first_f++;
  if(L.fav_flash < UI_F_FLASH) L.fav_flash++;
}

/* ---- Folders (spec section 4, Folders; r5 folders, folder_empty) ---------------------------
 * The ROM folder as it is on the Memory Stick: a breadcrumb path, then
 * ".." (below the top), the subfolders with their game counts, then the
 * games. × opens a folder or plays a game, ○ goes up a folder. */

#define FOLD_Y      60
#define FOLD_PITCH  22
#define FOLD_ROWS   8     /* rows in view, y 60-236 */
enum { ROW_UP, ROW_DIR, ROW_GAME };

static const u16 icon_up[11] =
 { 0x000, 0x020, 0x070, 0x0A8, 0x124, 0x020, 0x020, 0x020, 0x020, 0x000, 0x000 };

/* the rows of the folder shown: ".." below the top, its folders, its
   games (in the Folders tab's sort) */
static void fold_fill(void)
{
  int i, n = 0;

  if(L.fold_dir != 0)
  {
    L.fold_kind[n] = ROW_UP;
    L.fold_item[n++] = L.dir_parent[L.fold_dir];
  }
  for(i = 1; i < L.dir_count; i++)
    if((L.dir_parent[i] == L.fold_dir) && (n < MAX_FOLD_ROWS))
    {
      L.fold_kind[n] = ROW_DIR;
      L.fold_item[n++] = i;
    }
  /* folders by name */
  {
    int a, b, first = (L.fold_dir != 0) ? 1 : 0;
    for(a = first + 1; a < n; a++)
      for(b = a; (b > first) &&
       (strcasecmp(L.dirs[L.fold_item[b - 1]], L.dirs[L.fold_item[b]]) > 0); b--)
      {
        int t = L.fold_item[b];
        L.fold_item[b] = L.fold_item[b - 1];
        L.fold_item[b - 1] = t;
      }
  }
  /* its games, sorted like a list tab */
  L.n = 0;
  for(i = 0; i < L.count; i++)
    if(L.games[i].dir == L.fold_dir)
      L.order[L.n++] = i;
  L.sort = L.sorts[2];
  L.cur = 0;
  sort_games();
  for(i = 0; (i < L.n) && (n < MAX_FOLD_ROWS); i++)
  {
    L.fold_kind[n] = ROW_GAME;
    L.fold_item[n++] = L.order[i];
  }
  L.fold_count = n;
}

static const char *dir_name(int dir)
{
  const char *s = strrchr(L.dirs[dir], '/');
  return s ? s + 1 : L.dirs[dir];
}

static void fold_go(int dir, int focus_dir)
{
  int i;

  L.fold_dir = dir;
  fold_fill();
  L.fold_cur = 0;
  /* going up: the cursor on the folder we came from */
  for(i = 0; i < L.fold_count; i++)
    if((L.fold_kind[i] == ROW_DIR) && (L.fold_item[i] == focus_dir))
      L.fold_cur = i;
  if((focus_dir < 0) && (L.fold_count > 0) && (L.fold_kind[0] == ROW_UP) &&
   (L.fold_count > 1))
    L.fold_cur = 1;  /* opening a folder: its first entry, not ".." */
  L.fold_from = L.fold_cur;
  L.fold_f = UI_F_CURSOR;
  L.fold_top = (L.fold_cur >= FOLD_ROWS) ? L.fold_cur - FOLD_ROWS + 1 : 0;
  L.fold_top_from = L.fold_top * FOLD_PITCH;
  L.fold_top_f = UI_F_SCROLL;
  L.marq_f = 0;
  L.dirty = 1;
}

static int fold_top_px(void)
{
  return ui_ease_out_back(L.fold_top_from, L.fold_top * FOLD_PITCH, L.fold_top_f, UI_F_SCROLL);
}

static void fold_breadcrumb(void)
{
  int chain[16], n = 0, d = L.fold_dir, j, x = 20;

  while((d > 0) && (n < 15))
  {
    chain[n++] = d;
    d = L.dir_parent[d];
  }
  chain[n++] = 0;
  for(j = n - 1; j >= 0; j--)
  {
    const char *p = (chain[j] == 0) ? L.dirs[0] : dir_name(chain[j]);
    int last = (j == 0), w = ui_print_width(p, UI_FONT_SMALL, 0) + 12;
    if(x + w > 460)
      w = 460 - x;
    if(w < 20)
      break;
    ui_round_fill(x, 38, w, 16, 2, last ? UI_COL_TANGERINE : UI_COL_CARD);
    ui_print_fit(x + 6, 39, p, last ? UI_COL_PLUM : UI_COL_CREAM, UI_FONT_SMALL, 0, w - 12);
    x += w + 4;
    if(!last)
    {
      ui_print(x, 39, "/", UI_COL_MUTE, UI_FONT_SMALL, 0);
      x += 8;
    }
  }
}

/* one row at y; sel: drawn lifted in tangerine */
static void fold_row(int i, int y, int sel)
{
  int lift = sel ? UI_LIFT : 0, kind = L.fold_kind[i], item = L.fold_item[i];
  u16 ink = sel ? UI_COL_PLUM : UI_COL_CREAM;
  char meta[24];
  const char *name;

  if(sel)
    ui_round_fill(21, y + 1, 440, 20, 2, UI_COL_SHADE);
  ui_round_fill(20 - lift, y - lift, 440, 20, 2, sel ? UI_COL_TANGERINE : UI_COL_CARD);
  if(kind == ROW_GAME)
  {
    const game_t *g = &L.games[item];
    draw_cover_any(23 - lift, y + 2 - lift, 16, g->title, 0);
    name = g->title;
    size_text(g->size, meta, sizeof(meta));
  }
  else
  {
    draw_icon11(25 - lift, y + 4 - lift, (kind == ROW_UP) ? icon_up : icon_folder,
     sel ? UI_COL_PLUM : UI_COL_TANGERINE, 1);
    name = (kind == ROW_UP) ? ".." : dir_name(item);
    if(kind == ROW_UP)
      snprintf(meta, sizeof(meta), "Up a folder");
    else
      snprintf(meta, sizeof(meta), "%d games", L.dir_games[item]);
  }
  ui_print_fit(46 - lift, y + 2 - lift, name, ink, UI_FONT_BODY, 0, 300);
  if((kind == ROW_GAME) && L.games[item].fav)
    draw_icon11(400 - lift, y + 4 - lift, icon_heart, sel ? UI_COL_PLUM : UI_COL_PINK, 1);
  ui_print(452 - lift - ui_print_width(meta, UI_FONT_SMALL, 0), y + 3 - lift, meta,
   sel ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_SMALL, 0);
}

static void fold_ground(void)
{
  int i, top = fold_top_px();

  fold_breadcrumb();
  if((L.fold_count == 0) || ((L.fold_count == 1) && (L.fold_kind[0] == ROW_UP)))
  {
    static const char t[] = "This folder is empty";
    static const char b[] = "Only .gba and .zip files show here.";
    ui_round_fill(208, 82, 64, 52, 3, UI_COL_CARD);
    draw_icon11(224, 92, icon_folder, UI_COL_TANGERINE, 3);
    ui_print(240 - ui_print_width(t, UI_FONT_BODY, UI_TEXT_BOLD) / 2, 146, t, UI_COL_CREAM,
     UI_FONT_BODY, UI_TEXT_BOLD);
    ui_print(240 - ui_print_width(b, UI_FONT_SMALL, 0) / 2, 166, b, UI_COL_MUTE,
     UI_FONT_SMALL, 0);
    return;
  }
  ui_clip_set(0, FOLD_Y - 3, UI_SCREEN_W, VIEW_B - FOLD_Y + 3);
  for(i = 0; i < L.fold_count; i++)
  {
    int y = FOLD_Y + i * FOLD_PITCH - top;
    if((y + 20 < FOLD_Y - 3) || (y > VIEW_B))
      continue;
    fold_row(i, y, 0);
  }
  ui_clip_reset();
  if(L.fold_count > FOLD_ROWS)
  {
    int track = 176, th = track * FOLD_ROWS / L.fold_count;
    int ty = (track - th) * top / ((L.fold_count - FOLD_ROWS) * FOLD_PITCH);
    ui_round_fill(468, FOLD_Y, 4, track, 1, UI_COL_CARD);
    ui_round_fill(468, FOLD_Y + ty, 4, th, 1, UI_COL_MUTE);
  }
}

static void fold_selection(void)
{
  int top = fold_top_px(), y0, y1;

  if((L.fold_count == 0) || ((L.fold_count == 1) && (L.fold_kind[0] == ROW_UP)))
    return;
  ui_clip_set(0, FOLD_Y - 5, UI_SCREEN_W, VIEW_B - FOLD_Y + 5);
  fold_row(L.fold_cur, FOLD_Y + L.fold_cur * FOLD_PITCH - top, 1);
  y0 = FOLD_Y + L.fold_from * FOLD_PITCH - top;
  y1 = FOLD_Y + L.fold_cur * FOLD_PITCH - top;
  ui_cursor_frame(18, ui_ease_out_back(y0, y1, L.fold_f, UI_F_CURSOR) - 2, 440, 20);
  ui_clip_reset();
}

static void fold_move(void)
{
  int i;

  if(L.fold_count == 0)
    return;
  i = ui_step(L.fold_cur, L.fold_count, PSP_CTRL_UP, PSP_CTRL_DOWN);
  if(i == L.fold_cur)
    return;
  L.fold_from = L.fold_cur;
  L.fold_cur = i;
  L.fold_f = 0;
  L.marq_f = 0;
  if((i < L.fold_top) || (i >= L.fold_top + FOLD_ROWS))
  {
    L.fold_top_from = fold_top_px();
    L.fold_top = (i < L.fold_top) ? i : i - FOLD_ROWS + 1;
    L.fold_top_f = 0;
  }
  L.dirty = 1;  /* × reads Open or Play */
}

/* ---- the No BIOS notice (spec section 4, No BIOS; r4 bios, bios_new) ------------------------
 * Without gba_bios.bin kai runs games on its built-in BIOS. A one-line
 * notice says so on every start until the first game is launched (unless
 * System -> No-BIOS notice is off); the library keeps working around it. */

int ui_no_bios = 0;       /* set at start when the built-in BIOS is in use */
static int banner_gone = 0;

static int banner_up(void)
{
  return ui_no_bios && !banner_gone && g_gpsp_config.bios_note;
}

/* is any of the notice on screen (□ Help works then)? */
static int notice_in_view(void)
{
  if(!banner_up() || (L.count == 0))
    return 0;
  if(L.tab_ids[L.tab] == TAB_RECENT)
    return 1;
  return list_banner() && (L.ys[0] - 24 + 16 > VIEW_T);
}

/* before the first game (no Recent tab) it is the top item of the All
   list instead, so it hides no game (design session, 2026-11-19) */
static int list_banner(void)
{
  return banner_up() && (L.recent_count == 0) && (L.tab_ids[L.tab] == TAB_ALL) && (L.n > 0);
}

/* the notice: one cream line, 440 x 16 at x 20 with a 4 px tangerine edge;
   the line in plum, □ Help at its right end (□ opens its help with the
   Help step) */
static void draw_banner_at(int y)
{
  static const char t[] = "No GBA BIOS: games start in Game Cart mode.";
  int hw = ui_print_width("Help", UI_FONT_SMALL, UI_TEXT_BOLD), hx = 460 - 8 - hw - 17;

  ui_round_fill(22, y + 2, 440, 16, 2, UI_COL_SHADE);
  ui_round_fill(20, y, 440, 16, 2, UI_COL_CREAM);
  ui_fill(20, y + 2, 4, 12, UI_COL_TANGERINE);
  ui_glyph_inverted(hx, y + 1, PSP_CTRL_SQUARE);
  ui_print(hx + 17, y + 1, "Help", UI_COL_PLUM, UI_FONT_SMALL, UI_TEXT_BOLD);
  ui_print_fit(30, y + 1, t, UI_COL_PLUM, UI_FONT_SMALL, 0, hx - 30 - 10);
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
  ui_print(x + 16, y + 6, LIBRARY_GRID ? "View & sort" : "Sort", UI_COL_PLUM,
   UI_FONT_BODY, UI_TEXT_BOLD);
  for(j = POP_FIRST_ROW; j < 2; j++)
  {
    int ry = y + 32 + (j - POP_FIRST_ROW) * 34, sx = x + 16 + 64, i, n = j ? SORTS : 2;
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

static const char *tab_names[4] = { "Recent", "All", "\xE2\x98\x85 Favourites", "Folders" };

static int tab_is(int id)
{
  return L.tab_ids[L.tab] == id;
}

/* the games of a list tab, in its sort: every game on All, the hearted on
   Favourites; the cursor goes back where it was on that tab */
static void fill_order(void)
{
  int i, fav = tab_is(TAB_FAV);

  L.n = 0;
  for(i = 0; i < L.count; i++)
    if(!fav || L.games[i].fav)
      L.order[L.n++] = i;
  L.sort = L.sorts[fav];
  L.cur = 0;
  sort_games();
  L.cur = L.tab_cur[L.tab];
  if(L.cur >= L.n)
    L.cur = (L.n > 0) ? L.n - 1 : 0;
  L.cur_from = L.cur;
}

static void tabs_init(void)
{
  L.tab_count = 0;
  if(L.recent_count > 0)
    L.tab_ids[L.tab_count++] = TAB_RECENT;
  L.tab_ids[L.tab_count++] = TAB_ALL;
  L.tab_ids[L.tab_count++] = TAB_FAV;
  L.tab_ids[L.tab_count++] = TAB_FOLDERS;
  L.fold_open = 0;
  L.tab = 0;  /* Recent when there is one, else All */
  L.tab_prev = 0;
  L.tab_f = UI_F_TAB;
  memset(L.tab_cur, 0, sizeof(L.tab_cur));
  if(!tab_is(TAB_RECENT))
  {
    fill_order();
    view_reset();
  }
}

/* L or R: tabs never wrap */
static void turn_tab(int d)
{
  L.tab_cur[L.tab] = L.cur;
  L.tab_prev = L.tab;
  L.tab += d;
  L.tab_f = 0;
  L.marq_f = 0;
  if(tab_is(TAB_FOLDERS))
  {
    /* the folder left open last time, else the top */
    if(!L.fold_open)
      fold_go(0, -1);
    else
      fold_fill();
    L.fold_open = 1;
  }
  else if(!tab_is(TAB_RECENT))
  {
    fill_order();
    view_reset();
  }
  L.dirty = 1;
}

static void draw_hints(void)
{
  ui_hint_t h[5];
  int n = 0;

  if(L.card && !L.card_out)
  {
    /* ○ Back · △ Favourite never change; SELECT comes and goes (Start
       fresh is not a savestate; Undo while the toast offers it) */
    h[n].keys = ui_key_back;        h[n++].verb = "Back";
    h[n].keys = PSP_CTRL_TRIANGLE;  h[n++].verb = "Favourite";
    if((L.toast_f >= 0) && (L.toast_undo >= 0))
    {
      h[n].keys = PSP_CTRL_SELECT;  h[n++].verb = "Undo";
    }
    else if(L.tile_slot[L.card_cur] >= 0)
    {
      h[n].keys = PSP_CTRL_SELECT;  h[n++].verb = "Hold: delete";
    }
    h[n].keys = ui_key_confirm;     h[n++].verb = "Play";
  }
  else if(L.pop && !L.pop_out)
  {
    if(LIBRARY_GRID)
    {
      h[n].keys = PSP_CTRL_UP | PSP_CTRL_DOWN;  h[n++].verb = "Move";
    }
    h[n].keys = PSP_CTRL_LEFT | PSP_CTRL_RIGHT; h[n++].verb = "Change";
    h[n].keys = ui_key_back;                    h[n++].verb = "Close";
  }
  else if(L.count == 0)
  {
    h[n].keys = ui_key_confirm;  h[n++].verb = "Look again";
    h[n].keys = PSP_CTRL_START;  h[n++].verb = "Settings";
  }
  else if(tab_is(TAB_RECENT))
  {
    /* × reads Continue on the Continue card */
    h[n].keys = PSP_CTRL_TRIANGLE; h[n++].verb = "Game card";
    h[n].keys = PSP_CTRL_START;  h[n++].verb = "Settings";
    h[n].keys = ui_key_confirm;  h[n++].verb = L.rec_row ? "Play" : "Continue";
  }
  else if(tab_is(TAB_FOLDERS))
  {
    int empty = (L.fold_count == 0) || ((L.fold_count == 1) && (L.fold_kind[0] == ROW_UP));
    if(L.fold_dir != 0)
    {
      h[n].keys = ui_key_back;     h[n++].verb = "Up a folder";
    }
    if(!empty && (L.fold_kind[L.fold_cur] == ROW_GAME))
    {
      h[n].keys = PSP_CTRL_TRIANGLE; h[n++].verb = "Game card";
    }
    if(!empty)
    {
      h[n].keys = PSP_CTRL_SELECT; h[n++].verb = LIBRARY_GRID ? "View" : "Sort";
    }
    h[n].keys = PSP_CTRL_START;    h[n++].verb = "Settings";
    if(!empty)
    {
      h[n].keys = ui_key_confirm;  h[n++].verb =
       (L.fold_kind[L.fold_cur] == ROW_GAME) ? "Play" : "Open";
    }
  }
  else if(L.n == 0)  /* Favourites, nothing hearted: nothing to play */
  {
    h[n].keys = PSP_CTRL_SELECT; h[n++].verb = LIBRARY_GRID ? "View" : "Sort";
    h[n].keys = PSP_CTRL_START;  h[n++].verb = "Settings";
  }
  else
  {
    h[n].keys = PSP_CTRL_TRIANGLE; h[n++].verb = "Game card";
    h[n].keys = PSP_CTRL_SELECT; h[n++].verb = LIBRARY_GRID ? "View" : "Sort";
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

static void draw_tab_bar(void)
{
  const char *labels[4];
  int i;

  for(i = 0; i < L.tab_count; i++)
    labels[i] = tab_names[L.tab_ids[i]];
  ui_tabs(UI_MARGIN, labels, L.tab_count, L.tab, L.tab_prev, L.tab_f);
}

static void build_ground(void)
{
  L.dirty = 0;
  if(!ui_layer_begin())
    return;
  if(L.count == 0)
    draw_empty();
  else if(tab_is(TAB_RECENT))
    recent_ground();
  else if(tab_is(TAB_FOLDERS))
    fold_ground();
  else if(L.n == 0)
    draw_no_favourites();
  else if(L.view == VIEW_GRID)
    grid_ground();
  else
    list_ground();
  /* on Recent in the Recent label's line; before any game, in the All list */
  if((L.count > 0) && banner_up() && tab_is(TAB_RECENT) && !L.card)
    draw_banner_at(149);
  /* the game card: the library, its tabs and chip stay behind the screen
     door (they are drawn here, not each frame, while it is up) */
  if(L.card)
  {
    draw_tab_bar();
    ui_status_chip();
    ui_door(0, 0, UI_SCREEN_W, UI_STRIP_Y);
  }
  draw_hints();
  ui_layer_end();
}

/* ---- moving ------------------------------------------------------------------------- */

static void select_game(int i)
{
  if((i < 0) || (i >= L.n) || (i == L.cur))
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
  if(L.n == 0)
    return;
  if(L.view == VIEW_GRID)
  {
    if(ui_repeat(PSP_CTRL_LEFT) && (L.cur > 0))
      select_game(L.cur - 1);
    else if(ui_repeat(PSP_CTRL_RIGHT) && (L.cur + 1 < L.n))
      select_game(L.cur + 1);
    else if(ui_repeat(PSP_CTRL_UP) && (L.cur >= GRID_COLS))
      select_game(L.cur - GRID_COLS);
    else if(ui_repeat(PSP_CTRL_DOWN) && (L.cur / GRID_COLS + 1 < grid_rows()))
    {
      int next = L.cur + GRID_COLS;
      select_game((next < L.n) ? next : L.n - 1);
    }
  }
  else
    select_game(ui_step(L.cur, L.n, PSP_CTRL_UP, PSP_CTRL_DOWN));
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
    /* the sort is kept per tab */
    L.sorts[tab_is(TAB_FOLDERS) ? 2 : tab_is(TAB_FAV) ? 1 : 0] = L.sort;
    g_gpsp_config.library_view = (LIBRARY_GRID ? (L.view & 1) :
     (g_gpsp_config.library_view & 1)) | ((L.sorts[0] & 3) << 1) |
     ((L.sorts[1] & 3) << 3) | ((L.sorts[2] & 3) << 5);
    save_config_file();
    return;
  }
  if(LIBRARY_GRID && ui_repeat(PSP_CTRL_UP | PSP_CTRL_DOWN))
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
        if(tab_is(TAB_FOLDERS))
        {
          int keep = L.fold_item[L.fold_cur], kind = L.fold_kind[L.fold_cur], i;
          L.sorts[2] = v;
          fold_fill();
          for(i = 0; i < L.fold_count; i++)  /* the cursor stays on its row */
            if((L.fold_kind[i] == kind) && (L.fold_item[i] == keep))
              L.fold_cur = L.fold_from = i;
          L.dirty = 1;
        }
        else
        {
          sort_games();
          view_reset();
        }
        L.pop_flash = 0;
      }
    }
  }
}

/* ---- starting a game ------------------------------------------------------------------ */

/* the tangerine progress card, "Loading" and its three dots (spec section 2) */
static void draw_loading(int t, int fill_256)
{
  int x = (UI_SCREEN_W - 332) / 2, y = 98, w = 332, h = 54, ex, i;
  int k = (30 * fill_256 + 128) / 256 - 1;

  /* nothing can be pressed while a game loads: the hint strip is empty */
  ui_fill(0, UI_STRIP_Y, UI_SCREEN_W, UI_STRIP_H, UI_COL_SHADE);

  ui_round_fill(x + 4, y + 4, w, h, 3, UI_COL_SHADE);
  ui_round_fill(x, y, w, h, 3, UI_COL_CARD);
  ui_fill(x, y + 4, 4, h - 8, UI_COL_TANGERINE);
  ex = ui_print(x + 16, y + 10, "Loading", UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD);
  for(i = 0; i < 3; i++)
    ui_fill(ex + 4 + i * 8, y + 19 - ((i == (t / UI_F_LOADING) % 3) ? 3 : 0), 4, 4,
     UI_COL_TANGERINE);
  for(i = 0; i < 30; i++)
    ui_fill(x + 16 + i * 10, y + 32, 8, 12, (i <= k) ? UI_COL_TANGERINE : UI_COL_PLUM_2);
}

/* While the game loads, its progress fills the Loading card (instead of
   the old menu's dialog), and the dots hop with each step. The card is
   drawn over the library's last frame. */
static void load_hook(int current, int total)
{
  ui_frame_begin();
  draw_loading(current * UI_F_LOADING, total ? current * 256 / total : 0);
  ui_frame_end_now();
}

/* Load the game picked in the library, with the Loading card. */
s32 ui_library_load(const char *dir, char *name)
{
  s32 loaded;

  chdir(dir);
  snprintf(rom_path, sizeof(rom_path), "%s", dir);
  progress_hook = load_hook;
  loaded = load_gamepak(name);
  progress_hook = NULL;
  if(loaded == -1)
    ui_error_card("This game won't open", "The file is damaged, or it is not a GBA game.",
     "OK", NULL, UI_COL_PINK);
  return loaded;
}

/* ---- the screen ----------------------------------------------------------------------- */

/* START: Settings over the library; a restart asked for there starts kai
   again on the library */
static void open_settings(void)
{
  int r = ui_settings(0, 0);

  if(r == 1)
  {
    int tab, row;
    ui_settings_where(&tab, &row);
    ui_restart_now(tab, row, NULL);
  }
  else if(r == 2)
    quit(0);  /* Quit kai: no game runs here, nothing to save but kai's own */
  ui_status_read();
  ui_input_init();
  L.dirty = 1;
}

/* Pick a game. Returns 1 with its folder and file name (no game running:
 * kai's start; from the pause screen: a game was left). */
int ui_library(char *dir, int dir_size, char *name, int name_size, int *slot)
{
  u32 mark = ui_pool_mark();
  int chosen = 0;

  memset(&L, 0, sizeof(L));
  L.played_game = -1;
  L.view = LIBRARY_GRID ? (g_gpsp_config.library_view & 1) : VIEW_LIST;
  L.sorts[0] = (g_gpsp_config.library_view >> 1) & 3;  /* All */
  L.sorts[1] = (g_gpsp_config.library_view >> 3) & 3;  /* Favourites */
  L.sorts[2] = (g_gpsp_config.library_view >> 5) & 3;  /* Folders */
  if(L.sorts[2] >= SORTS)
    L.sorts[2] = SORT_NAME;
  if(L.sorts[0] >= SORTS)
    L.sorts[0] = SORT_NAME;
  if(L.sorts[1] >= SORTS)
    L.sorts[1] = SORT_NAME;
  L.sort = L.sorts[0];
  if(!load_games())
  {
    ui_pool_release(mark);
    return 0;
  }
  covers_init();
  view_reset();
  recent_init();
  tabs_init();
  ui_status_read();
  ui_input_init();
  /* HOME shows the PSP's own quit screen here (spec section 3): no game
     runs, so nothing can be lost. kai turns that screen off at start for
     its games; it goes back to that when the library closes. */
  sceImposeSetHomePopup(1);

  while(!chosen)
  {
    L.frame++;
    ui_input_update();

    if(L.help)
    {
      /* □ or ○ closes it */
      if(!L.help_out && (ui_pressed(PSP_CTRL_SQUARE) || ui_pressed(ui_key_back)))
      {
        L.help_out = 1;
        L.help_f = 0;
      }
    }
    else if(notice_in_view() && !L.card && !L.pop && ui_pressed(PSP_CTRL_SQUARE))
    {
      /* □ Help on the No BIOS notice */
      L.help = 1;
      L.help_out = 0;
      L.help_f = 0;
    }
    else if(L.pop)
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
        recent_init();
        tabs_init();
      }
      else if(ui_pressed(PSP_CTRL_START))
        open_settings();
    }
    else if(L.card)
    {
      if(card_input())
        chosen = 1;
    }
    else if(ui_repeat(PSP_CTRL_LTRIGGER) && (L.tab > 0))
      turn_tab(-1);
    else if(ui_repeat(PSP_CTRL_RTRIGGER) && (L.tab + 1 < L.tab_count))
      turn_tab(1);
    else if(tab_is(TAB_RECENT))
    {
      recent_move();
      if(ui_pressed(PSP_CTRL_TRIANGLE))
        card_open(L.rec_row ? L.recent[L.rec_cur] : L.recent[0]);
      else if(ui_pressed(PSP_CTRL_START))
        open_settings();
      else if(ui_pressed(ui_key_confirm))
      {
        u64 t;
        L.chosen_game = L.rec_row ? L.recent[L.rec_cur] : L.recent[0];
        L.chosen_slot = L.rec_row ? resume_slot(L.chosen_game, &t) : L.cont_slot;
        chosen = 1;
      }
    }
    else if(tab_is(TAB_FOLDERS))
    {
      int kind = L.fold_kind[L.fold_cur], item = L.fold_item[L.fold_cur];
      int has = (L.fold_count > 0) && !((L.fold_count == 1) && (kind == ROW_UP));
      fold_move();
      kind = L.fold_kind[L.fold_cur];
      item = L.fold_item[L.fold_cur];
      if(ui_pressed(ui_key_back) && (L.fold_dir != 0))
        fold_go(L.dir_parent[L.fold_dir], L.fold_dir);
      else if(has && (kind == ROW_GAME) && ui_pressed(PSP_CTRL_TRIANGLE))
        card_open(item);
      else if(has && ui_pressed(PSP_CTRL_SELECT))
      {
        L.pop = 1;
        L.pop_f = 0;
        L.pop_out = 0;
        L.pop_row = POP_FIRST_ROW;
        L.pop_flash = UI_F_VALUE;
        L.dirty = 1;
      }
      else if(ui_pressed(PSP_CTRL_START))
        open_settings();
      else if(has && ui_pressed(ui_key_confirm))
      {
        if(kind == ROW_UP)
          fold_go(item, L.fold_dir);
        else if(kind == ROW_DIR)
          fold_go(item, -1);
        else
        {
          u64 t;
          L.chosen_game = item;
          L.chosen_slot = resume_slot(item, &t);
          chosen = 1;
        }
      }
    }
    else
    {
      move();
      if((L.n > 0) && ui_pressed(PSP_CTRL_TRIANGLE))
        card_open(L.order[L.cur]);
      else if(ui_pressed(PSP_CTRL_SELECT))
      {
        L.pop = 1;
        L.pop_f = 0;
        L.pop_out = 0;
        L.pop_row = POP_FIRST_ROW;
        L.pop_flash = UI_F_VALUE;
        L.dirty = 1;
      }
      else if(ui_pressed(PSP_CTRL_START))
        open_settings();
      else if((L.n > 0) && ui_pressed(ui_key_confirm))
      {
        u64 t;
        /* × Play resumes from Last, else the newest savestate, else the
           start (design session, 2026-11-19): a stray × never boots fresh
           and then lets Exit save over Last. Only the game card differs. */
        L.chosen_game = L.order[L.cur];
        L.chosen_slot = resume_slot(L.chosen_game, &t);
        chosen = 1;
      }
    }

    if(L.tab_f < UI_F_TAB) L.tab_f++;
    if(L.rec_f < UI_F_CURSOR) L.rec_f++;
    if(L.fold_f < UI_F_CURSOR) L.fold_f++;
    if(L.fold_top_f < UI_F_SCROLL)
    {
      L.fold_top_f++;
      L.dirty = 1;
    }
    if(L.card)
    {
      int was = L.card;
      card_step();
      if(!L.card && was && L.fav_changed && tab_is(TAB_FAV))
      {
        /* a game left or joined Favourites */
        fill_order();
        view_reset();
      }
      if(!L.card)
        L.fav_changed = 0;
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
    if(!L.card)
    {
      draw_tab_bar();
      ui_status_chip();
    }
    if(L.card)
    {
      draw_card();
      if(L.toast_f >= 0)
      {
        if(!ui_toast_at(L.toast, L.toast_edge, (L.toast_undo >= 0) ? "Undo" : NULL,
         L.toast_f, 6))
        {
          L.toast_f = -1;
          L.dirty = 1;  /* the hint goes back to Hold: delete */
        }
        else
          L.toast_f++;
      }
    }
    else if(L.count > 0)
    {
      if(tab_is(TAB_RECENT))
        recent_selection();
      else if(tab_is(TAB_FOLDERS))
        fold_selection();
      else if(L.view == VIEW_GRID)
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
    if(L.help)
    {
      ui_help_card("Add a GBA BIOS", "Copy gba_bios.bin into kai's folder, then restart kai.",
       UI_COL_TANGERINE, L.help_f, L.help_out);
      if(L.help_f < (L.help_out ? UI_F_HELP_OUT : UI_F_HELP_IN))
        L.help_f++;
      else if(L.help_out)
      {
        L.help = 0;
        L.dirty = 1;
      }
    }
    if(chosen)
    {
      draw_loading(0, 0);
      banner_gone = 1;  /* the banner hides after the first launch */
    }
    ui_frame_end();
  }

  {
    const game_t *g = &L.games[L.chosen_game];
    snprintf(launched_file, sizeof(launched_file), "%s", g->name);
    snprintf(launched_title, sizeof(launched_title), "%s", g->title);
    snprintf(dir, dir_size, "%s", L.dirs[g->dir]);
    snprintf(name, name_size, "%s", g->name);
    *slot = L.chosen_slot;
  }
  sceImposeSetHomePopup(g_gpsp_config.enable_home);
  ui_layer_reset();
  ui_pool_release(mark);
  return 1;
}

static void no_progress(int current, int total);

/* Find a ROM file by name under the ROM folder (four levels, as the
   library scans). */
static int find_rom(const char *path, const char *name, int depth, char *dir, int size)
{
  SceUID d = sceIoDopen(path);
  SceIoDirent e;
  int found = 0;

  if(d < 0)
    return 0;
  memset(&e, 0, sizeof(e));
  while(!found && (sceIoDread(d, &e) > 0))
  {
    if(e.d_name[0] == '.')
      continue;
    if(FIO_S_ISDIR(e.d_stat.st_mode))
    {
      if(depth < SCAN_DEPTH)
      {
        char sub[MAX_PATH];
        path_printf(sub, sizeof(sub), "%s/%s", path, e.d_name);
        sceIoDclose(d);
        found = find_rom(sub, name, depth + 1, dir, size);
        if(found)
          return 1;
        /* reopen and skip back to where this folder was */
        {
          char seen[256];
          snprintf(seen, sizeof(seen), "%s", e.d_name);
          d = sceIoDopen(path);
          if(d < 0)
            return 0;
          while(sceIoDread(d, &e) > 0)
            if(!strcmp(e.d_name, seen))
              break;
        }
      }
    }
    else if(!strcasecmp(e.d_name, name))
    {
      snprintf(dir, size, "%s", path);
      found = 1;
    }
    memset(&e, 0, sizeof(e));
  }
  sceIoDclose(d);
  return found;
}

/* Is a ROM file of that name under the ROM folder? */
int ui_library_has_rom(const char *rom_name)
{
  char dir[MAX_PATH];
  return find_rom(g_default_rom_dir, rom_name, 0, dir, sizeof(dir));
}

/* The err_state card's way on: leave the running game (as the Library tile
 * does) for the game a savestate was made with, then load that savestate
 * file into it. Returns 0 when that game is not in the ROM folder. */
u32 ui_library_switch_to(const char *rom_name, const char *state_file, int slot)
{
  char dir[MAX_PATH], name[MAX_FILE], file[MAX_FILE];

  if(!find_rom(g_default_rom_dir, rom_name, 0, dir, sizeof(dir)))
    return 0;
  snprintf(name, sizeof(name), "%s", rom_name);
  snprintf(file, sizeof(file), "%s", state_file);
  save_config_file();
  update_backup_force();
  note_game_left();
  save_game_config_file();
  if(ui_library_load(dir, name) == -1)
  {
    gamepak_filename[0] = 0;
    return 0;
  }
  reset_gba();
  state_no_question = 1;
  progress_hook = no_progress;
  /* from the file, even for Last: slot 10 would load the new game's own
     Last from memory instead (the number only names the slot) */
  load_state(file, (slot == MEM_STATE_NUM) ? 0 : slot);
  progress_hook = NULL;
  state_no_question = 0;
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

/* The pause screen's Library tile: Last is saved (when the save is on);
 * the game left is noted in Recent, then the one picked is loaded. A load
 * that fails may have overwritten the old game, so the library opens
 * again until one loads. */
/* Continue: after the game has started, load the savestate it resumes
 * from (Last or a slot 0-9), quietly, as the pause screen does. */
static void no_progress(int current, int total)
{
  (void)current;
  (void)total;
}

void ui_library_resume(int slot)
{
  char name[MAX_FILE];

  if((slot < 0) || (slot > 10))
    return;
  state_no_question = 1;
  progress_hook = no_progress;
  get_savestate_filename_noshot(slot, name);
  if(slot == 10)
  {
    load_last_state();
    if(load_state(name, MEM_STATE_NUM))
      g_game_config.last_slot = 10;
  }
  else if(load_state(name, slot))
    g_game_config.last_slot = slot;
  progress_hook = NULL;
  state_no_question = 0;
}

u32 ui_library_switch(void)
{
  char dir[MAX_PATH], name[MAX_FILE];
  int slot;

  save_config_file();
  update_backup_force();
  note_game_left();
  save_game_config_file();

  while(ui_library(dir, sizeof(dir), name, sizeof(name), &slot))
  {
    if(ui_library_load(dir, name) != -1)
    {
      reset_gba();
      ui_library_resume(slot);
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

/* ---- start-up (spec section 4, Start-up; r4 startup, a9db06f) --------------------------
 * While kai starts (about a second), its start-up steps call this instead
 * of drawing the old dialog: the plum ground from the first step. No fixed
 * wait: ready within 20 f, the library follows the ground straight away and
 * the name is never drawn (no flash). Past 20 f, once the 4.0 fonts are in,
 * "gpSP kai" at 2x and the version under it, kept at least 20 f once shown
 * (ui_startup_end) so they never blink; past 30 f, the Loading card's three
 * dots under them (squares, no font needed). The library then cuts in. */
#define STARTUP_NAME_AFTER  20
#define STARTUP_NAME_HOLD   20
#define STARTUP_DOTS_AFTER  30

static u32 startup_t0, startup_name_at;  /* name_at: its first frame + 1 */

static u32 startup_frames(void)
{
  return (sceKernelGetSystemTimeLow() - startup_t0) / 16667;
}

static void startup_draw(void)
{
  u32 f = startup_frames();
  char ver[16];
  int i;

  ui_frame_begin();
  ui_ground();
  if((f >= STARTUP_NAME_AFTER) && ui_font_loaded())
  {
    static const char name[] = "gpSP kai";
    if(startup_name_at == 0)
      startup_name_at = f + 1;
    snprintf(ver, sizeof(ver), "%d.%d", VERSION_MAJOR, VERSION_MINOR);
    ui_print(240 - ui_print_width(name, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X) / 2, 104,
     name, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);
    ui_print(240 - ui_print_width(ver, UI_FONT_SMALL, 0) / 2, 140, ver, UI_COL_MUTE,
     UI_FONT_SMALL, 0);
  }
  if(f >= STARTUP_DOTS_AFTER)
    for(i = 0; i < 3; i++)
      ui_fill(230 + i * 8, 168 - ((i == (f / UI_F_LOADING) % 3) ? 3 : 0), 4, 4,
       UI_COL_TANGERINE);
  ui_frame_end_now();
}

void ui_startup_hook(int current, int total)
{
  (void)total;
  if((current == 0) && (startup_t0 == 0))
  {
    startup_t0 = sceKernelGetSystemTimeLow();
    if(startup_t0 == 0)
      startup_t0 = 1;
  }
  startup_draw();
}

/* Start-up is over: once the name has shown, it stays its 20 frames. */
void ui_startup_end(void)
{
  if(startup_t0 == 0)
    return;
  while(startup_name_at && (startup_frames() + 1 < startup_name_at + STARTUP_NAME_HOLD))
  {
    sceDisplayWaitVblankStart();
    startup_draw();
  }
}
