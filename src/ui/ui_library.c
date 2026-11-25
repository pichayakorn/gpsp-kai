#include <stddef.h>
#include "../common.h"
#include "ui.h"
#include "ui_cover.h"
#include "../game_name.h"
#include "../unicode.h"

#define MAX_GAMES   1024
#define MAX_DIRS    128
#define MAX_FOLD_ROWS (MAX_DIRS + MAX_GAMES + 1)
#define ARENA_SIZE  (64 * 1024)
#define SCAN_DEPTH  4

#define COVER      80
#define GRID_X     20
#define GRID_Y     grid_y0()
#define GRID_PX    90
#define GRID_PY    88
#define GRID_B     216
#define GRID_COLS  5
#define GRID_ROWS  grid_view_rows()
#define VIEW_T     38
#define VIEW_B     248

#define LIST_SM    22
#define LIST_BIG   84
#define LIST_GAP   4
#define LIST_FY    88

#define POP_X  20
#define POP_Y  128
#define POP_W  300
#define POP_FIRST_ROW (has_view_row() ? 0 : 1)
#define POP_H  (has_view_row() ? 98 : 64)

enum { VIEW_GRID, VIEW_LIST, VIEW_PANEL };
#define VIEW_SHELF VIEW_LIST

#define PANEL_LX   184
#define PANEL_LW   276
#define PANEL_PX   20
#define PANEL_PW   152
#define PANEL_ART  116
enum { TAB_RECENT, TAB_ALL, TAB_FAV, TAB_FOLDERS };
enum { SORT_NAME, SORT_RECENT, SORT_SIZE, SORTS };

typedef struct
{
  const char *name;
  const char *title;
  u32 size;
  u16 dir;
  s16 recent;
  u8 fav;
  u8 art;
} game_t;

enum { ART_NONE, ART_BY_FILE, ART_BY_NAME };
#define ART_SLOTS  2048
#define ICON_CACHE 32
#define ROW_CACHE  8

typedef struct
{
  int game;
  u32 used;
  u16 *px;
} cover_t;
#define MAX_COVERS 16

static struct
{
  game_t *games;
  int count;
  int *order;
  int n;
  const char **dirs;
  const char **dir_shown;
  int dir_count;
  char *arena;
  int arena_used;

  int view, sort;
  int views[2];
  int ptop, ptop_from, ptop_f;
  int rest_f;
  int art_game;
  u16 art[(PANEL_ART - 4) * (PANEL_ART - 4)];
  int cur, cur_from, cur_f;
  int top, off_from, top_f;
  int pos, pos_from, pos_f;
  int open_row, open_f;
  int close_row, close_from, close_f;
  int *ys;
  int marq_f;
  int jump_t, rail_f;
  int letter_f, letter_keep;
  int letter;

  int tab, tab_prev, tab_f;
  int tab_ids[4], tab_count;
  int recent[30], recent_count;
  int rec_row, rec_cur;
  int rec_from, rec_f;
  u16 *pic;
  int pic_ok, cont_slot;
  u64 cont_time;
  int chosen_game, chosen_slot;

  int sorts[3], tab_cur[4];
  int dir_parent[MAX_DIRS], dir_games[MAX_DIRS];
  int fold_dir, fold_count, fold_open;
  int fold_kind[MAX_FOLD_ROWS], fold_item[MAX_FOLD_ROWS];
  int fold_cur, fold_from, fold_f;
  int fold_top, fold_top_from, fold_top_f;
  int card, card_f, card_out, card_game;
  u32 card_mark;
  u16 *thumbs, *card_cover;
  int tile_slot[12], tile_pic[12], tile_count;
  u64 tile_time[12];
  int card_cur, card_from, card_cf;
  int card_first, card_first_from, card_first_f;
  int fav_flash, fav_changed, card_changed;
  ui_hold_t hold;
  char toast[64];
  u16 toast_edge;
  int toast_f, toast_undo;
  char facts[96];
  int help, help_f, help_out;
  int name_mode;
  char name_l1[256], name_l2[256];
  int dirty;
  u32 frame;

  int pop, pop_f, pop_out, pop_row, pop_flash;
  u32 played;
  u32 last_played;
  struct { u32 secs, last; u8 known; } *play;

  cover_t covers[MAX_COVERS];
  int cover_count;
  struct { int game; u32 used; u16 px[16 * 16]; } icons[ICON_CACHE];
  struct { int game; u32 used; u16 px[32 * 32]; } rowart[ROW_CACHE];
} L;

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
static void find_art(void);
static int art_read(int game, int size, u16 *out);
static const u16 icon_heart[11];
static const u16 icon_folder[11];
static void draw_icon11(int x, int y, const u16 *bits, u16 color, int scale);
static void read_favourites(void);
static void view_reset(void);
static int jump_here(void);
static int list_banner(void);
static void draw_banner_at(int y);

/* The PSP hands file names over in Shift_JIS; the UI's text is UTF-8.
   A byte of it becomes at most 3 of UTF-8. */
static void name_utf8(const char *name, char *out)
{
  char sjis[256];

  snprintf(sjis, sizeof(sjis), "%s", name);
  sjis_to_utf8(sjis, out);
}

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
  {
    const char *s = strrchr(path, '/');
    char shown[256 * 3];
    name_utf8(s ? s + 1 : path, shown);
    L.dir_shown[dir] = arena_str(shown, strlen(shown));
    if(L.dir_shown[dir] == NULL)
      L.dir_shown[dir] = s ? dir_path + (s + 1 - path) : dir_path;
  }
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
      char shown[256 * 3];
      const char *dot;
      memset(g, 0, sizeof(*g));
      g->name = arena_str(e.d_name, strlen(e.d_name));
      name_utf8(e.d_name, shown);
      dot = strrchr(shown, '.');
      g->title = g->name ? display_name(shown, dot - shown) : NULL;
      if(g->title == NULL)
        break;
      g->size = (u32)e.d_stat.st_size;
      g->dir = dir;
      g->recent = -1;
      L.count++;
    }
    memset(&e, 0, sizeof(e));
  }
  sceIoDclose(d);
}

static const char *display_name(const char *name, int n)
{
  char out[256];
  int o = game_name_clean(name, n, out);
  return arena_str(out, o);
}

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

static char launched_file[MAX_FILE], launched_title[256];

void ui_display_name(const char *file, char *out)
{
  const char *dot = strrchr(file, '.');
  int n = dot ? dot - file : (int)strlen(file);
  if(launched_file[0] && !strcmp(file, launched_file))
  {
    strcpy(out, launched_title);
    return;
  }
  game_name_clean(file, n, out);
}

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
  L.dir_shown = ui_pool_alloc(MAX_DIRS * sizeof(char *));
  L.arena = ui_pool_alloc(ARENA_SIZE);
  L.ys = ui_pool_alloc(MAX_GAMES * sizeof(int));
  L.play = ui_pool_alloc(MAX_GAMES * sizeof(*L.play));
  if(L.play)
    memset(L.play, 0, MAX_GAMES * sizeof(*L.play));
  if(!L.games || !L.order || !L.dirs || !L.dir_shown || !L.arena || !L.ys)
    return 0;
  L.count = L.dir_count = L.arena_used = 0;
  scan_dir(g_default_rom_dir, 0, -1);
  tell_apart();
  find_art();
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
  L.cur = L.cur_from = 0;
  return 1;
}

static void covers_init(void)
{
  int i;

  L.cover_count = 0;
  for(i = 0; i < ICON_CACHE; i++)
    L.icons[i].game = -1;
  for(i = 0; i < ROW_CACHE; i++)
    L.rowart[i].game = -1;
  for(i = 0; i < MAX_COVERS; i++)
  {
    if(ui_pool_free() < 160 * 1024 + COVER * COVER * 2)
      break;
    L.covers[i].px = ui_pool_alloc(COVER * COVER * 2);
    if(L.covers[i].px == NULL)
      break;
    L.covers[i].game = -1;
    L.cover_count++;
  }
}

#define PLATE_LINES 3
#define PLATE_PITCH 14

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
  return (n > PLATE_LINES) ? -PLATE_LINES : n;
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
    if(cut && (i == n - 1))
      ui_print_fit(x + 4, ty, rest, UI_COL_CREAM, UI_FONT_BODY, 0, size - 8);
    else
      ui_print(x + 4, ty, lines[i], UI_COL_CREAM, UI_FONT_BODY, 0);
  }
}

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
    static u16 px[COVER * COVER];
    if(art_read(game, COVER, px))
      ui_blit(px, COVER, x, y, COVER, COVER);
    else
      draw_cover_any(x, y, COVER, L.games[game].title, 1);
    return;
  }
  c = &L.covers[(free_i >= 0) ? free_i : old_i];
  c->game = game;
  c->used = L.frame;
  if(!art_read(game, COVER, c->px))
  {
    ui_cover_art(c->px, COVER, COVER, L.games[game].title);
    if(ui_target_begin(c->px, COVER, COVER, COVER))
    {
      cover_plate(0, 0, COVER, L.games[game].title);
      ui_target_end();
    }
  }
  ui_blit(c->px, COVER, x, y, COVER, COVER);
}

/* .art (gpsp-kai-tools docs/art-format.md):
 * "KART", version 1, the number of pictures, 2 bytes 0; then per picture
 * its width, height and 2 bytes 0; then the pictures, rows top to bottom,
 * as kai's 16-bit colours. */

static void art_dir(char *out, int size)
{
  char *slash;

  snprintf(out, size, "%s", g_default_rom_dir);
  while((strlen(out) > 1) && (out[strlen(out) - 1] == '/'))
    out[strlen(out) - 1] = 0;
  slash = strrchr(out, '/');
  if(slash)
    *slash = 0;
  snprintf(out + strlen(out), size - strlen(out), "/ART");
}

static u32 name_hash(const char *s, int n)
{
  u32 h = 2166136261u;
  int i;

  for(i = 0; i < n; i++)
    h = (h ^ (u8)tolower((unsigned char)s[i])) * 16777619u;
  return h ? h : 1;
}

static int hash_has(const u32 *tab, u32 h)
{
  u32 k = h & (ART_SLOTS - 1);

  while(tab[k])
  {
    if(tab[k] == h)
      return 1;
    k = (k + 1) & (ART_SLOTS - 1);
  }
  return 0;
}

static void find_art(void)
{
  u32 mark = ui_pool_mark(), *tab = ui_pool_alloc(ART_SLOTS * 4), used = 0;
  char dir[MAX_PATH], clean[256];
  SceIoDirent e;
  SceUID d;
  int i;

  if(tab == NULL)
    return;
  memset(tab, 0, ART_SLOTS * 4);
  art_dir(dir, sizeof(dir));
  d = sceIoDopen(dir);
  if(d < 0)
  {
    ui_pool_release(mark);
    return;
  }
  memset(&e, 0, sizeof(e));
  while((sceIoDread(d, &e) > 0) && (used < ART_SLOTS / 2))
  {
    const char *dot = strrchr(e.d_name, '.');
    if(dot && !strcasecmp(dot, ".art") && !FIO_S_ISDIR(e.d_stat.st_mode))
    {
      u32 h = name_hash(e.d_name, dot - e.d_name), k = h & (ART_SLOTS - 1);
      while(tab[k] && (tab[k] != h))
        k = (k + 1) & (ART_SLOTS - 1);
      if(!tab[k])
        used++;
      tab[k] = h;
    }
    memset(&e, 0, sizeof(e));
  }
  sceIoDclose(d);
  for(i = 0; i < L.count; i++)
  {
    game_t *g = &L.games[i];
    const char *dot = strrchr(g->name, '.');
    g->art = ART_NONE;
    if(hash_has(tab, name_hash(g->name, dot ? dot - g->name : (int)strlen(g->name))))
      g->art = ART_BY_FILE;
    else
    {
      game_name_of_file(g->name, clean);
      if(hash_has(tab, name_hash(clean, strlen(clean))))
        g->art = ART_BY_NAME;
    }
  }
  ui_pool_release(mark);
}

static int art_read(int game, int size, u16 *out)
{
  static u16 big[128 * 128];
  game_t *g = &L.games[game];
  char path[MAX_PATH], name[256], *dot;
  u8 head[8], list[8 * 4];
  u32 off, best_at = 0;
  int count, i, best = 0;
  FILE *f;

  if(g->art == ART_NONE)
    return 0;
  if(g->art == ART_BY_FILE)
  {
    snprintf(name, sizeof(name), "%s", g->name);
    dot = strrchr(name, '.');
    if(dot)
      *dot = 0;
  }
  else
    game_name_of_file(g->name, name);
  art_dir(path, sizeof(path));
  snprintf(path + strlen(path), sizeof(path) - strlen(path), "/%s.art", name);
  f = fopen(path, "rb");
  if(f == NULL)
    goto none;
  if((fread(head, 1, 8, f) != 8) || memcmp(head, "KART", 4) || (head[4] != 1) ||
   (head[5] < 1) || (head[5] > 8))
    goto bad;
  count = head[5];
  if(fread(list, 4, count, f) != (size_t)count)
    goto bad;
  off = 8 + count * 4;
  for(i = 0; i < count; i++)
  {
    int w = list[i * 4], h = list[i * 4 + 1];
    if((w == h) && (w >= size) && (w <= 128) && (!best || (w < best)))
    {
      best = w;
      best_at = off;
    }
    off += w * h * 2;
  }
  if(!best)
  {
    fclose(f);
    return 0;
  }
  if(fseek(f, best_at, SEEK_SET))
    goto bad;
  if(best == size)
  {
    if(fread(out, 2, size * size, f) != (size_t)(size * size))
      goto bad;
  }
  else
  {
    if(fread(big, 2, best * best, f) != (size_t)(best * best))
      goto bad;
    ui_shrink_box(big, best, best, out, size, size);
  }
  fclose(f);
  return 1;
bad:
  fclose(f);
none:
  g->art = ART_NONE;
  return 0;
}

static u16 *small_cover(int game, int size)
{
  int i, old = 0, n = (size == 16) ? ICON_CACHE : ROW_CACHE;

  for(i = 0; i < n; i++)
  {
    int *g = (size == 16) ? &L.icons[i].game : &L.rowart[i].game;
    u32 *used = (size == 16) ? &L.icons[i].used : &L.rowart[i].used;
    u32 old_used = (size == 16) ? L.icons[old].used : L.rowart[old].used;
    if(*g == game)
    {
      *used = L.frame;
      return (size == 16) ? L.icons[i].px : L.rowart[i].px;
    }
    if(*used < old_used)
      old = i;
  }
  {
    u16 *px = (size == 16) ? L.icons[old].px : L.rowart[old].px;
    if(!art_read(game, size, px))
      ui_cover_art(px, size, size, L.games[game].title);
    if(size == 16)
    {
      L.icons[old].game = game;
      L.icons[old].used = L.frame;
    }
    else
    {
      L.rowart[old].game = game;
      L.rowart[old].used = L.frame;
    }
    return px;
  }
}

static void game_cover(int game, int x, int y, int size, int plate)
{
  static int last_game = -1, last_size;
  static u16 last[128 * 128];

  if((size == 16) || (size == 32))
  {
    ui_blit(small_cover(game, size), size, x, y, size, size);
    return;
  }
  if(L.games[game].art != ART_NONE)
  {
    if(size == COVER)
    {
      draw_cover(x, y, game);
      return;
    }
    if(size <= 128)
    {
      if(((last_game == game) && (last_size == size)) || art_read(game, size, last))
      {
        last_game = game;
        last_size = size;
        ui_blit(last, size, x, y, size, size);
        return;
      }
    }
  }
  draw_cover_any(x, y, size, L.games[game].title, plate);
}

static int marquee_shift(int over)
{
  int t = L.marq_f - UI_MARQUEE_DELAY, cycle = over + 2 * UI_MARQUEE_DELAY;

  if((over <= 0) || (t <= 0))
    return 0;
  t %= cycle;
  return (t < over) ? t : (t < over + UI_MARQUEE_DELAY) ? over : 0;
}

static void grid_marquee(int x, int y, int game)
{
  char lines[PLATE_LINES][128], rest[256];
  const char *title = L.games[game].title;
  int n, w, ly;

  if(L.games[game].art != ART_NONE)
    return;
  n = plate_lines(title, COVER, lines, rest, sizeof(rest));
  if(n >= 0)
    return;
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

static int grid_y0(void)
{
  return list_banner() ? 64 : 42;
}

static int grid_view_rows(void)
{
  return list_banner() ? 1 : 2;
}

static int grid_rows(void)
{
  return (L.n + GRID_COLS - 1) / GRID_COLS;
}

static int grid_off(void)
{
  return ui_ease_out_cubic(L.off_from, L.top * GRID_PY, L.top_f, UI_F_LIST_SCROLL);
}

static void grid_heart(int x, int y, int game, u16 ink)
{
  if(L.games[game].fav)
    draw_icon11(x + COVER - 13, y + 3, icon_heart, ink, 1);
}

static void grid_item_xy(int i, int off, int *x, int *y)
{
  *x = GRID_X + (i % GRID_COLS) * GRID_PX;
  *y = GRID_Y + (i / GRID_COLS) * GRID_PY - off;
}

static void grid_cover(int i, int x, int y)
{
  int bottom = (y + COVER < GRID_B) ? y + COVER : GRID_B, top = GRID_Y - 2;

  ui_clip_set(0, top, UI_SCREEN_W, bottom - top);
  if(y + COVER > GRID_B)
    if(L.games[L.order[i]].art != ART_NONE)
      draw_cover(x, y, L.order[i]);
    else
      draw_cover_any(x, y, COVER, L.games[L.order[i]].title, 0);
  else
    draw_cover(x, y, L.order[i]);
  grid_heart(x, y, L.order[i], UI_COL_PINK);
  ui_clip_reset();
}

static void last_text(u32 unix_time, char *out, int n);
static u32 game_played(int game);
static void played_text(u32 seconds, char *out, int n);
static void played_info(int game, char *out, int n);

static void grid_bar(void)
{
  const game_t *g = &L.games[L.order[L.cur]];
  char info[96];
  int iw;

  ui_round_fill(23, 223, 440, 22, 2, UI_COL_SHADE);
  ui_round_fill(20, 220, 440, 22, 2, UI_COL_CARD);
  ui_fill(20, 222, 4, 18, UI_COL_TANGERINE);
  played_info(L.order[L.cur], info, sizeof(info));
  iw = ui_print_width(info, UI_FONT_SMALL, 0);
  ui_print(452 - iw, 224, info, UI_COL_MUTE, UI_FONT_SMALL, 0);
  ui_print_fit(32, 223, g->title, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD, 452 - iw - 16 - 32);
}

static void grid_ground(void)
{
  int i, off = grid_off(), rows = grid_rows();

  if(list_banner())
    draw_banner_at(40);
  for(i = 0; i < L.n; i++)
  {
    int x, y;
    grid_item_xy(i, off, &x, &y);
    if((y + COVER <= GRID_Y - 2) || (y >= GRID_B))
      continue;
    grid_cover(i, x, y);
  }
  if(L.n > 0)
    grid_bar();
  if(rows > 2)
  {
    int th = 176 * 2 / rows, ty;
    if(th < 12)
      th = 12;
    ty = (rows > GRID_ROWS) ? (176 - th) * off / ((rows - GRID_ROWS) * GRID_PY) : 0;
    if(ty < 0) ty = 0;
    if(ty > 176 - th) ty = 176 - th;
    ui_round_fill(468, 42, 4, 176, 1, UI_COL_CARD);
    ui_round_fill(468, 42 + ty, 4, th, 1, UI_COL_MUTE);
  }
}

static void grid_selection(void)
{
  int off = grid_off(), x, y, fx, fy, x0, y0, x1, y1;

  if(L.n == 0)
    return;
  grid_item_xy(L.cur, off, &x, &y);
  if((y >= GRID_Y - 2) && (y + COVER <= GRID_B))
  {
    ui_clip_set(0, GRID_Y - 4, UI_SCREEN_W, GRID_B - (GRID_Y - 4));
    ui_fill(x + 1, y + 1, COVER, COVER, UI_COL_SHADE);
    draw_cover(x - UI_LIFT, y - UI_LIFT, L.order[L.cur]);
    grid_heart(x - UI_LIFT, y - UI_LIFT, L.order[L.cur], UI_COL_PINK);
    ui_clip_reset();
    if(L.top_f >= UI_F_LIST_SCROLL)
      grid_marquee(x - UI_LIFT, y - UI_LIFT, L.order[L.cur]);
  }
  grid_item_xy(L.cur_from, off, &x0, &y0);
  grid_item_xy(L.cur, off, &x1, &y1);
  fx = ui_ease_out_back(x0, x1, L.cur_f, UI_F_CURSOR);
  fy = ui_ease_out_back(y0, y1, L.cur_f, UI_F_CURSOR);
  ui_cursor_frame(fx - UI_LIFT, fy - UI_LIFT, COVER, COVER);
}

static int list_pos(void)
{
  return ui_ease_out_cubic(L.pos_from, L.pos, L.pos_f, UI_F_LIST_SCROLL);
}

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

static int list_gap(int i)
{
  return LIST_GAP + 4 * list_open_p(i) / 256;
}

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

typedef char play_pair_check[(offsetof(game_config_t, last_played) ==
 offsetof(game_config_t, play_seconds) + 4) ? 1 : -1];

static u32 game_played(int game)
{
  char path[MAX_PATH], name[MAX_PATH];
  FILE *f;
  char *dot;
  u32 head[2], v[2];

  if(L.play && L.play[game].known)
  {
    L.last_played = L.play[game].last;
    return L.play[game].secs;
  }
  L.played = L.last_played = 0;
  snprintf(name, sizeof(name), "%s", L.games[game].name);
  dot = strrchr(name, '.');
  if(dot)
    *dot = 0;
  path_printf(path, sizeof(path), "%s/%s.cfg", g_default_cfg_dir, name);
  f = fopen(path, "rb");
  if(f)
  {
    if((fread(head, 4, 2, f) == 2) && !memcmp(head, "gcfg", 4) &&
     !fseek(f, 8 + offsetof(game_config_t, play_seconds), SEEK_SET) &&
     (fread(v, 4, 2, f) == 2))
    {
      L.played = v[0];
      L.last_played = v[1];
    }
    fclose(f);
  }
  if(L.play)
  {
    L.play[game].secs = L.played;
    L.play[game].last = L.last_played;
    L.play[game].known = 1;
  }
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

static void played_info(int game, char *out, int n)
{
  char when[32], played[32];
  u32 seconds = game_played(game);

  last_text(L.last_played, when, sizeof(when));
  played_text(seconds, played, sizeof(played));
  if(!seconds && !when[0])
    snprintf(out, n, "Not played yet");
  else if(!when[0])
    snprintf(out, n, "%s", played);
  else
    snprintf(out, n, "%s \xC2\xB7 %s", when, played);
}

static void list_slim_contents(const game_t *g, int y, int lift, int on_card)
{
  char size[16];

  game_cover(g - L.games, 23 - lift, y + 3 - lift, 16, 0);
  ui_print_fit(46 - lift, y + 3 - lift, g->title, on_card ? UI_COL_PLUM : UI_COL_CREAM,
   UI_FONT_BODY, on_card ? UI_TEXT_BOLD : 0, 330);
  if(g->fav)
    draw_icon11(400 - lift, y + 5 - lift, icon_heart, on_card ? UI_COL_PLUM : UI_COL_PINK, 1);
  size_text(g->size, size, sizeof(size));
  ui_print(452 - lift - ui_print_width(size, UI_FONT_SMALL, 0), y + 4 - lift, size,
   on_card ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_SMALL, 0);
}

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
  ui_fill(20, y, 80, 80, UI_COL_PLUM);
  game_cover(L.order[i], 22, y + 2, 76, 0);
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

static int bold_line(const char *t, int n, int w, char *out, int size);

static int day_number(int y, int m, int d)
{
  if(m < 3)
  {
    y--;
    m += 12;
  }
  return 365 * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + d;
}

static void last_text(u32 unix_time, char *out, int n)
{
  u64 utc, local;
  ScePspDateTime t, now;
  int days;

  out[0] = 0;
  if(unix_time == 0)
    return;
  utc = (u64)unix_time * 1000000ULL + 62135596800000000ULL;  /* ticks from year 1 */
  if((sceRtcConvertUtcToLocalTime(&utc, &local) < 0) || (sceRtcSetTick(&t, &local) < 0))
    return;
  sceRtcGetCurrentClockLocalTime(&now);
  days = day_number(now.year, now.month, now.day) - day_number(t.year, t.month, t.day);
  if(days <= 0)
    snprintf(out, n, "Today %02d:%02d", t.hour, t.minute);
  else if(days == 1)
    snprintf(out, n, "Yesterday %02d:%02d", t.hour, t.minute);
  else
    snprintf(out, n, "%d days ago", days);
}

static int panel_y0(void)
{
  return list_banner() ? 66 : 42;
}

static int panel_rows(void)
{
  return list_banner() ? 7 : 8;
}

static int panel_top_for(int cur)
{
  int view = panel_rows(), top = L.ptop;

  if(cur < top + 2)
    top = cur - 2;
  if(cur > top + view - 3)
    top = cur - view + 3;
  if(top > L.n - view)
    top = L.n - view;
  if(top < 0)
    top = 0;
  return top;
}

static int panel_top_px(void)
{
  return ui_ease_out_cubic(L.ptop_from, L.ptop * UI_ROW_PITCH, L.ptop_f, UI_F_LIST_SCROLL);
}

static void panel_row(int i, int y, int sel)
{
  const game_t *g = &L.games[L.order[i]];
  int l2 = sel ? UI_LIFT : 0, x = PANEL_LX - l2, yy = y - l2;
  char size[16];

  if(sel)
    ui_round_fill(PANEL_LX + 1, y + 1, PANEL_LW, UI_ROW_H, 2, UI_COL_SHADE);
  ui_round_fill(x, yy, PANEL_LW, UI_ROW_H, 2, sel ? UI_COL_TANGERINE : UI_COL_CARD);
  game_cover(g - L.games, x + 3, yy + 2, 16, 0);
  ui_print_fit(x + 26, yy + 2, g->title, sel ? UI_COL_PLUM : UI_COL_CREAM, UI_FONT_BODY, 0,
   PANEL_LW - 82);
  if(g->fav)
    draw_icon11(x + PANEL_LW - 58, yy + 4, icon_heart, sel ? UI_COL_PLUM : UI_COL_PINK, 1);
  size_text(g->size, size, sizeof(size));
  ui_print(x + PANEL_LW - 8 - ui_print_width(size, UI_FONT_SMALL, 0), yy + 3, size,
   sel ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_SMALL, 0);
}

static void panel_card(void)
{
  const game_t *g = &L.games[L.order[L.cur]];
  int py = panel_y0(), ph = 244 - py, art = ph - 86, ax, ny, w = PANEL_PW - 16;
  char l1[256], l2[256], when[32], played[32];
  u32 seconds;

  if(art > PANEL_ART)
    art = PANEL_ART;
  ax = PANEL_PX + (PANEL_PW - art) / 2;
  ui_round_fill(PANEL_PX + 3, py + 3, PANEL_PW, ph, 3, UI_COL_SHADE);
  ui_round_fill(PANEL_PX, py, PANEL_PW, ph, 3, UI_COL_CARD);
  ui_fill(PANEL_PX + 4, py, PANEL_PW - 8, 4, UI_COL_TANGERINE);
  ui_fill(ax, py + 10, art, art, UI_COL_PLUM);
  if(L.art_game != L.order[L.cur])
  {
    if(!art_read(L.order[L.cur], art - 4, L.art))
      ui_cover_art(L.art, art - 4, art - 4, g->title);
    L.art_game = L.order[L.cur];
  }
  ui_blit(L.art, art - 4, ax + 2, py + 12, art - 4, art - 4);
  if(g->fav)
    draw_icon11(ax + art - 16, py + 14, icon_heart, UI_COL_PINK, 1);
  ny = py + 10 + art + 8;
  if(bold_line(g->title, 0, w, l1, sizeof(l1)))
  {
    ui_print(PANEL_PX + 8, ny, l1, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD);
    if(bold_line(g->title, 1, w, l2, sizeof(l2)))
    {
      if(bold_line(g->title, 2, w, l2, sizeof(l2)))
      {
        const char *rest = strstr(g->title, l1);
        rest = rest ? rest + strlen(l1) : g->title;
        while(*rest == ' ')
          rest++;
        snprintf(l2, sizeof(l2), "%s", rest);
      }
      ui_print_fit(PANEL_PX + 8, ny + 16, l2, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD, w);
    }
  }
  seconds = game_played(L.order[L.cur]);
  last_text(L.last_played, when, sizeof(when));
  if(!when[0] && !seconds)
    ui_print(PANEL_PX + 8, py + ph - 34, "Not played yet", UI_COL_CREAM, UI_FONT_SMALL, 0);
  else
  {
    if(when[0])
      ui_print(PANEL_PX + 8, py + ph - 34, when, UI_COL_CREAM, UI_FONT_SMALL, 0);
    played_text(seconds, played, sizeof(played));
    ui_print(PANEL_PX + 8, py + ph - 20, played, UI_COL_MUTE, UI_FONT_SMALL, 0);
  }
}

static void panel_ground(void)
{
  int i, y0 = panel_y0(), top = panel_top_px(), view = panel_rows();

  if(list_banner())
    draw_banner_at(40);
  ui_clip_set(0, y0 - 4, UI_SCREEN_W, VIEW_B - (y0 - 4));
  for(i = 0; i < L.n; i++)
  {
    int y = y0 + i * UI_ROW_PITCH - top;
    if((y + UI_ROW_H < y0 - 4) || (y > VIEW_B))
      continue;
    panel_row(i, y, 0);
  }
  ui_clip_reset();
  panel_card();
  if(L.n > view)
  {
    int th = 196 * view / L.n, ty;
    if(th < 12)
      th = 12;
    ty = (196 - th) * top / ((L.n - view) * UI_ROW_PITCH);
    ui_round_fill(468, 42, 4, 196, 1, UI_COL_CARD);
    ui_round_fill(468, 42 + ty, 4, th, 1, UI_COL_MUTE);
  }
}

static void panel_selection(void)
{
  int y0 = panel_y0(), top = panel_top_px();

  if(L.n == 0)
    return;
  ui_clip_set(0, y0 - 4, UI_SCREEN_W, VIEW_B - (y0 - 4));
  panel_row(L.cur, y0 + L.cur * UI_ROW_PITCH - top, 1);
  ui_cursor_frame(PANEL_LX - UI_LIFT, y0 + list_pos() * UI_ROW_PITCH / 256 - top - UI_LIFT,
   PANEL_LW, UI_ROW_H);
  ui_clip_reset();
}

static void panel_step(void)
{
  if(L.pos_f < UI_F_LIST_SCROLL)
  {
    L.pos_f++;
    L.dirty = 1;
  }
  if(L.ptop_f < UI_F_LIST_SCROLL)
  {
    L.ptop_f++;
    L.dirty = 1;
  }
}

#define RECENT_MAX   30
#define CONT_X       20
#define CONT_Y       40
#define CONT_W       204
#define CONT_H       200
#define PIC_W        180
#define PIC_H        120
#define REC_X        232
#define REC_W        228
#define REC_H        44
#define REC_PITCH    52
#define SVS_SHOT     8
#define SVS_TIME     (SVS_SHOT + 240 * 160 * 2)
#define SHOT_ROWS    20

static int banner_up(void);

static int recent_tab_on(void)
{
  return (L.recent_count > 0) && g_gpsp_config.show_recent;
}

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
        ui_shrink_43_rows(rows, SHOT_ROWS, L.pic + n * (SHOT_ROWS * 3 / 4) * PIC_W);
      }
      ok = (n == 160 / SHOT_ROWS);
    }
    sceIoClose(fd);
  }
  ui_pool_release(mark);
  return ok;
}

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

static int recent_rows(void)
{
  int n = L.recent_count - 1, max = banner_up() ? 3 : 4;
  return (n < 0) ? 0 : (n > max) ? max : n;
}

static int recent_row_y(int k)
{
  return CONT_Y + (banner_up() ? REC_PITCH : 0) + k * REC_PITCH;
}

static int recent_game(int place)
{
  return L.recent[place];
}

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
    *x = REC_X - UI_LIFT;
    *y = recent_row_y(place - 1) - UI_LIFT;
    *w = REC_W;
    *h = REC_H;
  }
}

static int recent_place(void)
{
  return L.rec_row ? 1 + L.rec_cur : 0;
}

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

static u16 mix35(u16 ink, u16 bg)
{
  u16 out = ink & 0x8000;
  int c;

  for(c = 0; c < 15; c += 5)
  {
    int a = (ink >> c) & 31, b = (bg >> c) & 31;
    out |= ((a + (b - a) * 35 / 100) & 31) << c;
  }
  return out;
}

static void draw_continue(int hero)
{
  int game = L.recent[0], lift = hero ? UI_LIFT : 0, x, y, w;
  u16 ink = hero ? UI_COL_PLUM : UI_COL_CREAM, bg = hero ? UI_COL_TANGERINE : UI_COL_CARD;
  char line[96], when[32], played[32];
  const char *chip = "CONTINUE";
  u32 seconds = game_played(game);

  ui_round_fill(CONT_X + 3, CONT_Y + 3, CONT_W, CONT_H, 3, UI_COL_SHADE);
  x = CONT_X - lift;
  y = CONT_Y - lift;
  ui_round_fill(x, y, CONT_W, CONT_H, 3, bg);
  ui_round_fill(x + 8, y + 8, PIC_W + 8, PIC_H + 8, 3, UI_COL_PLUM);
  if(L.pic_ok)
    ui_blit(L.pic, PIC_W, x + 12, y + 12, PIC_W, PIC_H);
  else
    game_cover(game, x + 12 + (PIC_W - 112) / 2, y + 12 + (PIC_H - 112) / 2, 112, 0);
  w = ui_print_width(chip, UI_FONT_SMALL, 0) + 12;
  ui_round_fill(x + 12, y + 12, w, 16, 2, UI_COL_TANGERINE);
  ui_print(x + 18, y + 13, chip, UI_COL_PLUM, UI_FONT_SMALL, 0);
  ui_print_fit(x + 8, y + 144, L.games[game].title, ink, UI_FONT_BODY, UI_TEXT_BOLD,
   CONT_W - 16);
  if(L.cont_slot >= 0)
  {
    char slot[16];
    when[0] = 0;
    if(L.cont_time > 62135596800000000ULL)
      last_text((u32)((L.cont_time - 62135596800000000ULL) / 1000000ULL), when, sizeof(when));
    if(L.cont_slot == 10)
      snprintf(slot, sizeof(slot), "Last");
    else
      snprintf(slot, sizeof(slot), "Slot %d", L.cont_slot);
    if(when[0])
      snprintf(line, sizeof(line), "%s \xC2\xB7 %s", slot, when);
    else
      snprintf(line, sizeof(line), "%s", slot);
  }
  else
  {
    last_text(L.last_played, line, sizeof(line));
    if(!line[0] && !seconds)
      snprintf(line, sizeof(line), "Not played yet");
  }
  ui_print_fit(x + 8, y + 164, line, ink, UI_FONT_SMALL, 0, CONT_W - 16);
  if(seconds)
  {
    played_text(seconds, played, sizeof(played));
    ui_print(x + 8, y + 178, played, mix35(ink, bg), UI_FONT_SMALL, 0);
  }
}

static void draw_recent_row(int k, int sel)
{
  int game = recent_game(1 + k), y = recent_row_y(k), l2 = sel ? UI_LIFT : 0;
  int x = REC_X - l2, yy = y - l2;
  char info[96];

  ui_round_fill(REC_X + 3, y + 3, REC_W, REC_H, 2, UI_COL_SHADE);
  ui_round_fill(x, yy, REC_W, REC_H, 2, sel ? UI_COL_TANGERINE : UI_COL_CARD);
  ui_round_fill(x + 4, yy + 4, 36, 36, 2, UI_COL_PLUM);
  game_cover(game, x + 6, yy + 6, 32, 0);
  if(L.games[game].fav)
    draw_icon11(x + REC_W - 20, yy + 7, icon_heart, sel ? UI_COL_PLUM : UI_COL_PINK, 1);
  ui_print_fit(x + 48, yy + 5, L.games[game].title, sel ? UI_COL_PLUM : UI_COL_CREAM,
   UI_FONT_BODY, UI_TEXT_BOLD, REC_W - 72);
  played_info(game, info, sizeof(info));
  ui_print_fit(x + 48, yy + 24, info, sel ? UI_COL_PLUM : UI_COL_MUTE, UI_FONT_SMALL, 0,
   REC_W - 56);
}

static void draw_bios_row(int x, int y)
{
  static const char t[] = "No GBA BIOS: games start in Game Cart mode.";
  char line[128];
  int hw = ui_print_width("Help", UI_FONT_SMALL, UI_TEXT_BOLD), hx = x + REC_W - 8 - hw - 17, j;

  ui_round_fill(x + 3, y + 3, REC_W, REC_H, 2, UI_COL_SHADE);
  ui_round_fill(x, y, REC_W, REC_H, 2, UI_COL_CREAM);
  ui_fill(x, y + 4, 4, REC_H - 8, UI_COL_TANGERINE);
  for(j = 0; (j < 2) && ui_wrap_line(t, j, REC_W - 20, UI_FONT_SMALL, line, sizeof(line)); j++)
    ui_print(x + 12, y + 5 + 14 * j, line, UI_COL_PLUM, UI_FONT_SMALL, 0);
  ui_glyph_inverted(hx, y + REC_H - 17, PSP_CTRL_SQUARE);
  ui_print(hx + 17, y + REC_H - 18, "Help", UI_COL_PLUM, UI_FONT_SMALL, UI_TEXT_BOLD);
}

static void recent_ground(void)
{
  int k, n = recent_rows();

  draw_continue(L.rec_row == 0);
  if(banner_up())
    draw_bios_row(REC_X, CONT_Y);
  for(k = 0; k < n; k++)
    draw_recent_row(k, (L.rec_row == 1) && (k == L.rec_cur));
}

static void recent_selection(void)
{
  int x0, y0, w0, h0, x1, y1, w1, h1;

  recent_box(L.rec_from, &x0, &y0, &w0, &h0);
  recent_box(recent_place(), &x1, &y1, &w1, &h1);
  ui_cursor_frame(ui_ease_out_back(x0, x1, L.rec_f, UI_F_CURSOR),
   ui_ease_out_back(y0, y1, L.rec_f, UI_F_CURSOR),
   ui_ease_out_back(w0, w1, L.rec_f, UI_F_CURSOR),
   ui_ease_out_back(h0, h1, L.rec_f, UI_F_CURSOR));
}

static void recent_move(void)
{
  int n = recent_rows(), from = recent_place(), row = L.rec_row, cur = L.rec_cur;

  if((row == 0) && (n > 0) && ui_repeat(PSP_CTRL_RIGHT))
  {
    row = 1;
    if(cur >= n)
      cur = n - 1;
  }
  else if((row == 1) && ui_repeat(PSP_CTRL_LEFT))
    row = 0;
  else if(row == 1)
    cur = ui_step(cur, n, PSP_CTRL_UP, PSP_CTRL_DOWN);
  if((row != L.rec_row) || (cur != L.rec_cur))
  {
    L.rec_from = from;
    L.rec_row = row;
    L.rec_cur = cur;
    L.rec_f = 0;
    L.marq_f = 0;
    L.dirty = 1;
  }
}

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

static void card_facts(int game)
{
  const game_t *g = &L.games[game];
  const game_config_t *cfg = NULL;
  u32 on = 0, total = cheat_engine_peek(g->name, &on);
  int saved = 0;
  u32 mark = ui_pool_mark();
  char ch[32], st[48];

  if(gamepak_filename[0] && !strcasecmp(gamepak_filename, g->name) &&
   !strcasecmp(rom_path, L.dirs[g->dir]))
    cfg = &g_game_config;
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
    saved = ui_settings_saved_for_game(cfg);
  if(total)
    snprintf(ch, sizeof(ch), "Cheats: %u / %u", (unsigned)on, (unsigned)total);
  else
    snprintf(ch, sizeof(ch), "Cheats: none");
  if(saved)
    snprintf(st, sizeof(st), "Settings: %d saved for this game", saved);
  else
    snprintf(st, sizeof(st), "Settings: none saved for this game");
  snprintf(L.facts, sizeof(L.facts), "%s  \xC2\xB7  %s", ch, st);
  ui_pool_release(mark);
}

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
  if(!art_read(game, CARD_COVER, L.card_cover))
    ui_cover_art(L.card_cover, CARD_COVER, CARD_COVER, L.games[game].title);
  L.card = 1;
  L.card_f = 0;
  L.card_out = 0;
  L.card_cur = L.card_from = (L.tile_count > 1) ? 1 : 0;
  L.card_cf = UI_F_CURSOR;
  L.card_first = 0;
  L.card_first_from = 0;
  L.card_first_f = UI_F_SCROLL;
  L.fav_flash = UI_F_FLASH;
  L.card_changed = 0;
  L.marq_f = 0;
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
      big_title(x + 124, y + 12, 316, g->title, UI_COL_CREAM, UI_COL_CARD,
       (L.card_f >= UI_F_POPUP_IN) ? marquee_shift(big_title_over(g->title, 316)) : 0);
      dy = 0;
      break;
  }

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
  ui_print_fit(x + 124, y + 68 + dy, L.facts,
   UI_COL_MUTE, UI_FONT_SMALL, 0, 300);

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
  {
    int fx = ui_ease_out_back(L.card_from * TILE_PITCH, L.card_cur * TILE_PITCH, L.card_cf,
     UI_F_CURSOR);
    ui_cursor_frame(x + 12 + fx - first - UI_LIFT, y + 136, TILE_W, TILE_H);
  }
  ui_clip_reset();

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
  L.dirty = 1;
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
  L.dirty = 1;
}

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

#define FOLD_Y      60
#define FOLD_PITCH  22
#define FOLD_ROWS   8
enum { ROW_UP, ROW_DIR, ROW_GAME };

static const u16 icon_up[11] =
 { 0x000, 0x020, 0x070, 0x0A8, 0x124, 0x020, 0x020, 0x020, 0x020, 0x000, 0x000 };

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
  return L.dir_shown[dir];
}

static void fold_go(int dir, int focus_dir)
{
  int i;

  L.fold_dir = dir;
  fold_fill();
  L.fold_cur = 0;
  for(i = 0; i < L.fold_count; i++)
    if((L.fold_kind[i] == ROW_DIR) && (L.fold_item[i] == focus_dir))
      L.fold_cur = i;
  if((focus_dir < 0) && (L.fold_count > 0) && (L.fold_kind[0] == ROW_UP) &&
   (L.fold_count > 1))
    L.fold_cur = 1;
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
    game_cover(item, 23 - lift, y + 2 - lift, 16, 0);
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
  L.dirty = 1;
}

int ui_no_bios = 0;
static int banner_gone = 0;

static int banner_up(void)
{
  return ui_no_bios && !banner_gone && g_gpsp_config.bios_note;
}

static int notice_in_view(void)
{
  if(!banner_up() || (L.count == 0))
    return 0;
  if(L.tab_ids[L.tab] == TAB_RECENT)
    return 1;
  if((L.view == VIEW_PANEL) || (L.view == VIEW_GRID))
    return list_banner();
  return list_banner() && (L.ys[0] - 24 + 16 > VIEW_T);
}

static int list_banner(void)
{
  return banner_up() && !recent_tab_on() && (L.tab_ids[L.tab] == TAB_ALL) && (L.n > 0);
}

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

static int tab_is(int id);
static const char *view_labels[3] = { "Panel", "Shelf", "Grid" };
static const int view_ids[3] = { VIEW_PANEL, VIEW_SHELF, VIEW_GRID };

static int view_index(int view)
{
  return (view == VIEW_SHELF) ? 1 : (view == VIEW_GRID) ? 2 : 0;
}
static const char *sort_labels[SORTS] = { "Name", "Recently played", "Size" };

static int has_view_row(void)
{
  return !tab_is(TAB_FOLDERS);
}

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
  ui_print(x + 16, y + 6, has_view_row() ? "View & sort" : "Sort", UI_COL_PLUM,
   UI_FONT_BODY, UI_TEXT_BOLD);
  for(j = POP_FIRST_ROW; j < 2; j++)
  {
    int ry = y + 32 + (j - POP_FIRST_ROW) * 34, sx = x + 16 + 64, i, n = j ? SORTS : 3;
    int k = j ? L.sort : view_index(L.view);
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

static const char *tab_names[4] = { "Recent", "All", "\xE2\x99\xA5 Favourites", "Folders" };

static int tab_is(int id)
{
  return L.tab_ids[L.tab] == id;
}

static void fill_order(void)
{
  int i, fav = tab_is(TAB_FAV);

  L.n = 0;
  for(i = 0; i < L.count; i++)
    if(!fav || L.games[i].fav)
      L.order[L.n++] = i;
  L.sort = L.sorts[fav];
  L.view = L.views[fav];
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
  if(recent_tab_on())
    L.tab_ids[L.tab_count++] = TAB_RECENT;
  L.tab_ids[L.tab_count++] = TAB_ALL;
  L.tab_ids[L.tab_count++] = TAB_FAV;
  L.tab_ids[L.tab_count++] = TAB_FOLDERS;
  L.fold_open = 0;
  L.tab = 0;
  L.tab_prev = 0;
  L.tab_f = UI_F_TAB;
  memset(L.tab_cur, 0, sizeof(L.tab_cur));
  if(!tab_is(TAB_RECENT))
  {
    fill_order();
    view_reset();
  }
}

static void turn_tab(int d)
{
  L.tab_cur[L.tab] = L.cur;
  L.tab_prev = L.tab;
  L.tab += d;
  L.tab_f = 0;
  L.marq_f = 0;
  if(tab_is(TAB_FOLDERS))
  {
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
    if(has_view_row())
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
      h[n].keys = PSP_CTRL_SELECT; h[n++].verb = has_view_row() ? "View" : "Sort";
    }
    h[n].keys = PSP_CTRL_START;    h[n++].verb = "Settings";
    if(!empty)
    {
      h[n].keys = ui_key_confirm;  h[n++].verb =
       (L.fold_kind[L.fold_cur] == ROW_GAME) ? "Play" : "Open";
    }
  }
  else if(L.n == 0)
  {
    h[n].keys = PSP_CTRL_SELECT; h[n++].verb = has_view_row() ? "View" : "Sort";
    h[n].keys = PSP_CTRL_START;  h[n++].verb = "Settings";
  }
  else
  {
    h[n].keys = PSP_CTRL_TRIANGLE; h[n++].verb = "Game card";
    h[n].keys = PSP_CTRL_SELECT; h[n++].verb = has_view_row() ? "View" : "Sort";
    h[n].keys = PSP_CTRL_START;  h[n++].verb = "Settings";
    if(jump_here())
    {
      h[n].keys = PSP_CTRL_LEFT | PSP_CTRL_RIGHT; h[n++].verb = "Jump";
    }
    h[n].keys = ui_key_confirm;  h[n++].verb = "Play";
  }
  ui_hint_packed(h, n, 14);
}

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
  else if(L.view == VIEW_PANEL)
    panel_ground();
  else
    list_ground();
  if(L.card)
  {
    draw_tab_bar();
    ui_status_chip();
    ui_door(0, 0, UI_SCREEN_W, UI_STRIP_Y);
  }
  draw_hints();
  ui_layer_end();
}

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
      L.off_from = grid_off();
      L.top = top;
      L.top_f = 0;
    }
  }
  else
  {
    if(L.open_row >= 0)
    {
      if(L.open_f >= UI_F_LIST_OPEN)
      {
        L.close_from = list_open_p(L.open_row);
        L.close_row = L.open_row;
        L.close_f = 0;
      }
      L.open_row = -1;
    }
    L.pos_from = list_pos();
    L.pos = i * 256;
    L.pos_f = 0;
    L.rest_f = 0;
    if(L.view == VIEW_PANEL)
    {
      int top = panel_top_for(i);
      if(top != L.ptop)
      {
        L.ptop_from = panel_top_px();
        L.ptop = top;
        L.ptop_f = 0;
      }
    }
  }
  L.marq_f = 0;
  L.dirty = 1;
}

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
  if((L.pos_f >= UI_F_LIST_SCROLL) && !ui_down(PSP_CTRL_UP | PSP_CTRL_DOWN))
  {
    if(L.rest_f < UI_F_LIST_REST)
      L.rest_f++;
  }
  else
    L.rest_f = 0;
  if((L.open_row < 0) && (L.rest_f >= UI_F_LIST_REST))
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

static void view_reset(void)
{
  int row = L.cur / GRID_COLS;

  L.jump_t = L.letter_f = -1;

  L.top = (row >= GRID_ROWS) ? row - GRID_ROWS + 1 : 0;
  L.off_from = L.top * GRID_PY;
  L.top_f = UI_F_LIST_SCROLL;
  L.pos = L.pos_from = L.cur * 256;
  L.pos_f = UI_F_LIST_SCROLL;
  L.ptop = 0;
  L.ptop = panel_top_for(L.cur);
  L.ptop_from = L.ptop * UI_ROW_PITCH;
  L.ptop_f = UI_F_LIST_SCROLL;
  L.rest_f = UI_F_LIST_REST;
  L.open_row = L.cur;
  L.open_f = UI_F_LIST_OPEN;
  L.close_row = -1;
  L.marq_f = 0;
  L.cur_from = L.cur;
  L.cur_f = UI_F_CURSOR;
  L.dirty = 1;
}

#define RAIL_X      464
#define RAIL_T      38
#define RAIL_H      208
#define RAIL_SLOTS  13
#define RAIL_STAY   60

static int letter_of(int game)
{
  int ch = toupper((unsigned char)L.games[game].title[0]);
  return ((ch >= 'A') && (ch <= 'Z')) ? ch : '#';
}

static int letters(int *key, int *first)
{
  int i, j, n = 0;

  for(i = 0; i < L.n; i++)
  {
    int k = letter_of(L.order[i]);
    for(j = 0; (j < n) && (key[j] != k); j++)
      ;
    if((j == n) && (n < 27))
    {
      key[n] = k;
      first[n++] = i;
    }
  }
  return n;
}

static int jump_here(void)
{
  return (L.n > 0) && !tab_is(TAB_RECENT) && !tab_is(TAB_FOLDERS) && (L.view != VIEW_GRID);
}

static void letter_jump(int dir)
{
  int key[27], first[27], n = letters(key, first), j, here = letter_of(L.order[L.cur]);

  for(j = 0; (j < n) && (key[j] != here); j++)
    ;
  if(dir > 0)
    j++;
  else if(L.cur <= first[j])
    j--;
  if((j < 0) || (j >= n))
    return;
  select_game(first[j]);
  L.letter_keep = (L.letter_f >= 0);
  L.letter_f = 0;
  L.letter = key[j];
  if(L.jump_t < 0)
    L.rail_f = 0;
  L.jump_t = 0;
}

static void letter_step(void)
{
  if(L.jump_t >= 0)
  {
    if(L.rail_f < UI_F_RAIL_IN)
      L.rail_f++;
    if(++L.jump_t >= RAIL_STAY + UI_F_RAIL_OUT)
      L.jump_t = -1;
  }
  if((L.letter_f >= 0) && (++L.letter_f >= UI_F_LETTER_POP))
    L.letter_f = -1;
}

static u16 mix_to(u16 a, u16 b, int t)
{
  int r = (a & 31) + (((b & 31) - (a & 31)) * t + 128) / 256;
  int g = ((a >> 5) & 31) + ((((b >> 5) & 31) - ((a >> 5) & 31)) * t + 128) / 256;
  int bl = ((a >> 10) & 31) + ((((b >> 10) & 31) - ((a >> 10) & 31)) * t + 128) / 256;
  return r | (g << 5) | (bl << 10);
}

static void draw_letter_jump(void)
{
  int key[27], first[27], n, j, lo = 0, here, rx;
  char t[4];

  if(!jump_here())
    return;
  if(L.jump_t >= 0)
  {
    n = letters(key, first);
    here = letter_of(L.order[L.cur]);
    if(L.jump_t < RAIL_STAY)
      rx = RAIL_X + 16 - ui_ease_out_cubic(0, 16, L.rail_f, UI_F_RAIL_IN);
    else
      rx = RAIL_X + ui_ease_in_cubic(0, 16, L.jump_t - RAIL_STAY, UI_F_RAIL_OUT);
    for(j = 0; (j < n) && (key[j] != here); j++)
      ;
    if(n > RAIL_SLOTS)
    {
      lo = j - RAIL_SLOTS / 2;
      if(lo > n - RAIL_SLOTS)
        lo = n - RAIL_SLOTS;
      if(lo < 0)
        lo = 0;
    }
    ui_clip_set(RAIL_X, RAIL_T, 16, RAIL_H);
    ui_fill(rx, RAIL_T, 16, RAIL_H, UI_COL_PLUM);
    for(j = 0; (j < RAIL_SLOTS) && (lo + j < n); j++)
    {
      int y = RAIL_T + 4 + j * 15, k = lo + j, on = key[k] == here, w;
      if((j == 0) && (lo > 0))
        snprintf(t, sizeof(t), "\xE2\x96\xB2");
      else if((j == RAIL_SLOTS - 1) && (lo + RAIL_SLOTS < n))
        snprintf(t, sizeof(t), "\xE2\x96\xBC");
      else
      {
        t[0] = key[k];
        t[1] = 0;
        if(on)
          ui_round_fill(rx + 1, y + 1, 14, 15, 2, UI_COL_TANGERINE);
      }
      w = ui_print_width(t, UI_FONT_BODY, 0);
      ui_print(rx + 8 - w / 2, y, t, (on && (t[1] == 0)) ? UI_COL_PLUM : UI_COL_MUTE,
       UI_FONT_BODY, 0);
    }
    ui_clip_reset();
  }
  if(L.letter_f >= 0)
  {
    int f = L.letter_f, size, fade, x, y, w;
    u16 tile, ink;
    if(L.letter_keep)
      size = ui_ease_out_back(60, 64, (f < 8) ? f : 8, 8);
    else
      size = (f < UI_F_LETTER_POP / 2) ? ui_ease_out_back(0, 64, f, UI_F_LETTER_POP / 2) : 64;
    fade = (f < UI_F_LETTER_POP - 8) ? 256 : (UI_F_LETTER_POP - f) * 256 / 8;
    if(size < 4)
      return;
    if(size > 72)
      size = 72;
    tile = mix_to(UI_COL_PLUM, UI_COL_CREAM, fade);
    ink = mix_to(UI_COL_CREAM, UI_COL_PLUM, fade);
    x = 300 - size / 2;
    y = 127 - size / 2;
    ui_round_fill(x + 3, y + 3, size, size, 4, UI_COL_SHADE);
    ui_round_fill(x, y, size, size, 4, tile);
    t[0] = L.letter;
    t[1] = 0;
    if(size >= 48)
    {
      w = ui_print_width(t, UI_FONT_BODY, UI_TEXT_3X);
      ui_print(300 - w / 2, y + size / 2 - 26, t, ink, UI_FONT_BODY, UI_TEXT_3X);
    }
    else if(size >= 24)
    {
      w = ui_print_width(t, UI_FONT_BODY, UI_TEXT_2X);
      ui_print(300 - w / 2, y + size / 2 - 17, t, ink, UI_FONT_BODY, UI_TEXT_2X);
    }
  }
}

static void move(void)
{
  if(L.n == 0)
    return;
  if(jump_here())
  {
    if(ui_repeat(PSP_CTRL_LEFT))
      letter_jump(-1);
    else if(ui_repeat(PSP_CTRL_RIGHT))
      letter_jump(1);
  }
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
    L.sorts[tab_is(TAB_FOLDERS) ? 2 : tab_is(TAB_FAV) ? 1 : 0] = L.sort;
    g_gpsp_config.library_view = (g_gpsp_config.library_view & 1) |
     ((L.sorts[0] & 3) << 1) | ((L.sorts[1] & 3) << 3) | ((L.sorts[2] & 3) << 5) |
     (view_index(L.views[0]) << 7) | (view_index(L.views[1]) << 9);
    save_config_file();
    return;
  }
  if(has_view_row() && ui_repeat(PSP_CTRL_UP | PSP_CTRL_DOWN))
    L.pop_row ^= 1;
  if(ui_repeat(PSP_CTRL_LEFT) || ui_repeat(PSP_CTRL_RIGHT))
  {
    int d = ui_down(PSP_CTRL_LEFT) ? -1 : 1;
    if(L.pop_row == 0)
    {
      int k = view_index(L.view) + d;
      if((k >= 0) && (k < 3))
      {
        L.view = view_ids[k];
        L.views[tab_is(TAB_FAV)] = L.view;
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
          for(i = 0; i < L.fold_count; i++)
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

static void draw_loading(int t, int fill_256)
{
  int x = (UI_SCREEN_W - 332) / 2, y = 98, w = 332, h = 54, ex, i;
  int k = (30 * fill_256 + 128) / 256 - 1;

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

static void load_hook(int current, int total)
{
  ui_frame_begin();
  draw_loading(current * UI_F_LOADING, total ? current * 256 / total : 0);
  ui_frame_end_now();
}

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
    quit(0);
  ui_status_read();
  ui_input_init();
  L.dirty = 1;
}

int ui_library(char *dir, int dir_size, char *name, int name_size, int *slot)
{
  u32 mark = ui_pool_mark();
  int chosen = 0;

  memset(&L, 0, sizeof(L));
  L.views[0] = view_ids[((g_gpsp_config.library_view >> 7) & 3) % 3];
  L.views[1] = view_ids[((g_gpsp_config.library_view >> 9) & 3) % 3];
  L.view = L.views[0];
  L.art_game = -1;
  L.sorts[0] = (g_gpsp_config.library_view >> 1) & 3;
  L.sorts[1] = (g_gpsp_config.library_view >> 3) & 3;
  L.sorts[2] = (g_gpsp_config.library_view >> 5) & 3;
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
  sceImposeSetHomePopup(1);

  while(!chosen)
  {
    L.frame++;
    ui_input_update();

    if(L.help)
    {
      if(!L.help_out && (ui_pressed(PSP_CTRL_SQUARE) || ui_pressed(ui_key_back)))
      {
        L.help_out = 1;
        L.help_f = 0;
      }
    }
    else if(notice_in_view() && !L.card && !L.pop && ui_pressed(PSP_CTRL_SQUARE))
    {
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
        card_open(recent_game(L.rec_row ? 1 + L.rec_cur : 0));
      else if(ui_pressed(PSP_CTRL_START))
        open_settings();
      else if(ui_pressed(ui_key_confirm))
      {
        u64 t;
        L.chosen_game = recent_game(L.rec_row ? 1 + L.rec_cur : 0);
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
        fill_order();
        view_reset();
      }
      if(!L.card)
        L.fav_changed = 0;
    }
    if(L.top_f < UI_F_LIST_SCROLL) { L.top_f++; L.dirty = 1; }
    if(L.view == VIEW_LIST)
      list_step();
    else if(L.view == VIEW_PANEL)
      panel_step();
    if(L.cur_f < UI_F_CURSOR) L.cur_f++;
    L.marq_f++;
    letter_step();
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
          L.dirty = 1;
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
      else if(L.view == VIEW_PANEL)
        panel_selection();
      else
        list_selection();
      if(!L.pop)
        draw_letter_jump();
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
        L.dirty = 1;
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
      banner_gone = 1;
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

int ui_library_has_rom(const char *rom_name)
{
  char dir[MAX_PATH];
  return find_rom(g_default_rom_dir, rom_name, 0, dir, sizeof(dir));
}

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

#define STARTUP_NAME_AFTER  20
#define STARTUP_NAME_HOLD   20
#define STARTUP_DOTS_AFTER  30

static u32 startup_t0, startup_name_at;

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
