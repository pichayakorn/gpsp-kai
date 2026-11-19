/* The 4.0 pause screen (spec section 4, "Pause"; step 3). The menu button
 * opens it over the screen-doored game: the savestate strip on top (slots
 * 0-9 and Last as pictures), tiles below. SELECT saves at once and hold
 * SELECT deletes, with Undo on a toast. Confirm saves into an empty slot
 * and loads a full one, never saving over a savestate. A load goes straight
 * back to the game: with "Save when leaving a game" on, the game before is
 * kept in Last first (loading Last swaps the two); with it off, the load is
 * a hold, since nothing keeps the game before. Exit saves to Last first.
 * Settings, Cheats and Library still open the old menu until their 4.0
 * screens exist. Texts are English placeholders until they move to the
 * .msg files. */
#include "../common.h"
#include "ui.h"

#define SLOTS     11          /* 0-9 and Last */
#define LAST_SLOT MEM_STATE_NUM
#define BIG_W     144
#define BIG_H     96
#define SMALL_W   60          /* exactly 1/4 of the screenshot (the .thm) */
#define SMALL_H   40
#define THUMB_PITCH 64        /* a multiple of 8, for the GE */
#define CHUNK_ROWS 20         /* the screenshot is read 20 rows at a time: whole
                                 steps of both filters (4 for the .thm, 5 for 5:3) */
#define CHUNKS (160 / CHUNK_ROWS)
#define STRIP_CX  240
#define TILE_W    70
#define TILE_H    62
#define TILE_PITCH 74         /* 4 px apart */
#define TILE_X(i) (18 + (i) * TILE_PITCH)  /* the last one ends at 458 */
#define TILE_Y    176         /* 10 px above the hint strip */
#define F_OPEN    UI_F_PAUSE_OPEN
#define F_CLOSE   UI_F_PAUSE_CLOSE

/* 20 x 20 two-tone tile icons, from gpsp-kai-design app-ui/scripts/icons20.py:
 * per row, ink then fill, bit 19 the left pixel. */
static const u32 icon20_gear[2][20] =
{
  { 0x00600, 0x00900, 0x08910, 0x14928, 0x230C4, 0x10008, 0x08610, 0x08910, 0x7108E, 0x82081,
    0x82081, 0x7108E, 0x08F10, 0x08010, 0x10008, 0x230C4, 0x14928, 0x08910, 0x00900, 0x00600 },
  { 0x00000, 0x00600, 0x00600, 0x08610, 0x1CF38, 0x0FFF0, 0x079E0, 0x070E0, 0x0E070, 0x7C07E,
    0x7C07E, 0x0E070, 0x070E0, 0x07FE0, 0x0FFF0, 0x1CF38, 0x08610, 0x00600, 0x00600, 0x00000 }
};
static const u32 icon20_cheats[2][20] =
{
  { 0x00000, 0x00600, 0x00900, 0x00900, 0x01080, 0x01080, 0x7F0FE, 0x20004, 0x10008, 0x08010,
    0x04020, 0x04020, 0x08010, 0x08910, 0x11088, 0x12048, 0x24024, 0x3801C, 0x00000, 0x00000 },
  { 0x00000, 0x00000, 0x00600, 0x00600, 0x00F00, 0x00F00, 0x00F00, 0x1FFF8, 0x0FFF0, 0x07FE0,
    0x03FC0, 0x03FC0, 0x07FE0, 0x070E0, 0x0E070, 0x0C030, 0x18018, 0x00000, 0x00000, 0x00000 }
};
static const u32 icon20_camera[2][20] =
{
  { 0x00000, 0x00000, 0x00000, 0x07E00, 0x04200, 0x1C3F8, 0x20004, 0x20034, 0x20F04, 0x21084,
    0x22044, 0x22644, 0x22644, 0x22044, 0x21084, 0x20F04, 0x1FFF8, 0x00000, 0x00000, 0x00000 },
  { 0x00000, 0x00000, 0x00000, 0x00000, 0x03C00, 0x03C00, 0x1FFF8, 0x1FFC8, 0x1F0F8, 0x1EF78,
    0x1DFB8, 0x1D9B8, 0x1D9B8, 0x1DFB8, 0x1EF78, 0x1F0F8, 0x00000, 0x00000, 0x00000, 0x00000 }
};
static const u32 icon20_restart[2][20] =
{
  { 0x00000, 0x00100, 0x00140, 0x03F20, 0x04010, 0x09F20, 0x12140, 0x12180, 0x24018, 0x24024,
    0x24024, 0x24024, 0x24024, 0x24024, 0x12048, 0x11088, 0x08F10, 0x06060, 0x01F80, 0x00000 },
  { 0x00000, 0x00000, 0x00080, 0x000C0, 0x03FE0, 0x060C0, 0x0C080, 0x0C000, 0x18000, 0x18018,
    0x18018, 0x18018, 0x18018, 0x18018, 0x0C030, 0x0E070, 0x070E0, 0x01F80, 0x00000, 0x00000 }
};
static const u32 icon20_folder[2][20] =
{
  { 0x00000, 0x00000, 0x00000, 0x00000, 0x3F800, 0x20400, 0x203F8, 0x20004, 0x20004, 0x20004,
    0x20004, 0x20004, 0x20004, 0x20004, 0x20004, 0x20004, 0x1FFF8, 0x00000, 0x00000, 0x00000 },
  { 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x1F800, 0x1FC00, 0x1FFF8, 0x1FFF8, 0x1FFF8,
    0x1FFF8, 0x1FFF8, 0x1FFF8, 0x1FFF8, 0x1FFF8, 0x1FFF8, 0x00000, 0x00000, 0x00000, 0x00000 }
};
static const u32 icon20_exit[2][20] =
{
  { 0x00000, 0x00F00, 0x00900, 0x00900, 0x00900, 0x0C930, 0x12948, 0x14928, 0x24924, 0x28914,
    0x28F14, 0x28014, 0x28014, 0x24024, 0x12048, 0x11988, 0x08610, 0x06060, 0x01F80, 0x00000 },
  { 0x00000, 0x00000, 0x00600, 0x00600, 0x00600, 0x00600, 0x0C630, 0x08610, 0x18618, 0x10608,
    0x10008, 0x10008, 0x10008, 0x18018, 0x0C030, 0x0E070, 0x079E0, 0x01F80, 0x00000, 0x00000 }
};

/* No Resume tile (maintainer, 2026-11-20): △, HOME and ○ resume, as the
   hint strip says */
enum { TILE_SETTINGS, TILE_CHEATS, TILE_SHOT, TILE_RESTART, TILE_LIBRARY, TILE_EXIT,
       TILE_COUNT };

typedef struct
{
  const char *label;
  const u32 (*icon)[20];
  u16 color;
} tile_t;

static const tile_t tiles[TILE_COUNT] =
{
  { "Settings",   icon20_gear,    UI_COL_LIME },
  { "Cheats",     icon20_cheats,  UI_COL_LIME },
  { "Screenshot", icon20_camera,  UI_COL_SKY },
  { "Restart",    icon20_restart, UI_COL_TANGERINE },
  { "Library",    icon20_folder,  UI_COL_TANGERINE },
  { "Exit",       icon20_exit,    UI_COL_PINK },
};

enum { UNDO_NONE, UNDO_SAVE, UNDO_SAVE_NEW, UNDO_DELETE };

static struct
{
  u16 *game;                 /* the running game's 240x160 picture */
  u32 has[SLOTS];
  u64 time[SLOTS];
  u32 thumb_ok[SLOTS];
  u32 thumb_tried[SLOTS];
  u16 *thumbs;               /* SLOTS x 60x40 at THUMB_PITCH, in the UI pool */
  u16 *big;                  /* the selected slot box-filtered to 144x96 */
  int big_slot;              /* the slot p.big shows, -1: none */
  u16 *chunk;                /* 240 x CHUNK_ROWS of a screenshot being read */
  int cur, pos, pos_from, pos_f;
  int row, tile, tile_from, tile_f;
  int open_f, close_f, closing;
  char toast[64];
  u16 toast_edge;
  int toast_f, toast_undo;
  int title_f;      /* frames since the screen opened: the name's marquee */
  int switch_slot;  /* a savestate of another game: play that game from it */
  int undo_ended;            /* the cursor moved: the toast leaves, no Undo */
  int undo, undo_slot;
  ui_hold_t hold;
  ui_hold_t load_hold;       /* hold confirm: load with the save off */
  ui_hold_t exit_hold;       /* hold confirm on Exit, Restart or Library with the save off */
  int wrap_dir, wrap_from;   /* a wrap of the strip slides the short way */
  int bar_f;                 /* frames the progress card has shown (card_rise) */
  const char *bar_text;      /* the saving card while a save runs */
  u16 bar_col;               /* its edge and blocks: pink, sky for a restart */
  const char *bar_note;
  int bar_phase, bar_phases; /* a load after a save fills one bar */
  int busy;                  /* a save or load runs (nothing to press) */
  u32 busy_start;            /* when it started (us) */
  char hold_line[48];        /* the hold card's line once its load runs */
  char game_toast[64];       /* said over the game after a load */
  int changed;               /* the machine state changed (load, restart) */
  int layer_full;            /* the header and tiles at rest are in the ground */
} p;

/* ---- slots ------------------------------------------------------------- */

/* The strip, left to right: Last first, so it is one press from slot 0
 * (maintainer, 2026-11-15), then slots 0-9. p.cur is a strip position. */
static const int strip_slot[SLOTS] = { LAST_SLOT, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

static int cur_slot(void)
{
  return strip_slot[p.cur];
}

static int strip_position(int slot)
{
  return (slot == LAST_SLOT) ? 0 : slot + 1;
}

static u16 *thumb(int slot)
{
  return p.thumbs + slot * THUMB_PITCH * SMALL_H;
}

/* A .thm is read packed (pitch 60); the strip keeps it at THUMB_PITCH. */
static void thumb_spread(u16 *t)
{
  int y;

  for(y = SMALL_H - 1; y > 0; y--)
    memmove(t + y * THUMB_PITCH, t + y * SMALL_W, SMALL_W * 2);
}

/* the big picture's read, a slice a frame (load_big_picture) */
static SceUID shot_fd = -1;
static int shot_state, shot_reading = -1, shot_failed = -1;

static void read_slot(int slot)
{
  p.has[slot] = read_state_time(slot, &p.time[slot]);
  p.thumb_ok[slot] = p.has[slot] && p.thumbs &&
   read_state_thumbnail(slot, p.time[slot], thumb(slot));
  if(p.thumb_ok[slot])
    thumb_spread(thumb(slot));
  p.thumb_tried[slot] = 0;
  if(p.big_slot == slot)
    p.big_slot = -1;
  if(shot_failed == slot)
    shot_failed = -1;  /* the file changed: try again */
}

/* Reading savestate pictures: the screenshot in chunks of CHUNK_ROWS rows
 * (a 9.4 KB buffer, so the pictures fit the UI pool with the Korean font),
 * each box-filtered into p.big (when it is the selected slot) and
 * made into the 60 x 40 picture (when the slot has no right .thm, which is
 * then written). It runs with the PSP's asynchronous file calls, polled
 * once a frame, so a frame never waits for the Memory Stick; the selected
 * slot goes first, then any slot still without its picture. Every other
 * file action waits for it first (shot_wait). */
#define CHUNK_BYTES (240 * CHUNK_ROWS * 2)
enum { SHOT_IDLE, SHOT_OPENING, SHOT_HEAD, SHOT_CHUNK, SHOT_TIME,
       SHOT_WOPEN, SHOT_WRITE, SHOT_WCLOSE };  /* then the .thm, if it was made */
static int shot_chunk;  /* the chunk being read */

static u8 shot_head[8] __attribute__((aligned(64)));
static int shot_for_big, shot_for_thumb;

/* Wait for the asynchronous call in flight, then close. */
static void shot_wait(void)
{
  SceInt64 res;

  if(shot_state == SHOT_IDLE)
    return;
  if(shot_fd >= 0)
  {
    sceIoWaitAsync(shot_fd, &res);
    {
      /* a failed open has nothing to close; a close in flight is done */
      int opening = (shot_state == SHOT_OPENING) || (shot_state == SHOT_WOPEN);
      if((shot_state != SHOT_WCLOSE) && !(opening && (res < 0)))
        sceIoClose(shot_fd);
    }
  }
  shot_fd = -1;
  shot_state = SHOT_IDLE;
  shot_reading = -1;
}

/* Chunk n has arrived in p.chunk. */
static void shot_take_chunk(int slot, int n)
{
  if(shot_for_big)
    ui_shrink_53_rows(p.chunk, CHUNK_ROWS, p.big + n * (CHUNK_ROWS * 3 / 5) * 144);
  if(shot_for_thumb)
    make_thumbnail_rows(p.chunk, CHUNK_ROWS, thumb(slot) + n * (CHUNK_ROWS / 4) * THUMB_PITCH,
     THUMB_PITCH);
}

/* The whole picture has arrived (shot_head holds its save time). */
static void shot_done(int slot)
{
  if(shot_for_big)
    p.big_slot = slot;
  if(shot_for_thumb)
  {
    write_slot_thumbnail(slot, thumb(slot), THUMB_PITCH, shot_head);
    p.thumb_ok[slot] = 1;
  }
}

static int shot_setup(int slot)
{
  shot_for_big = (slot == cur_slot()) && (p.big_slot != slot);
  shot_for_thumb = (p.thumbs != NULL) && !p.thumb_ok[slot];
  if(shot_for_thumb)
    p.thumb_tried[slot] = 1;
  if(shot_for_big)
    p.big_slot = -1;  /* p.big is being written */
  return shot_for_big || shot_for_thumb;
}

/* The selected slot now, before the first frame (the game is still on the
 * LCD): the same steps without waiting a frame between them. */
static void load_big_picture_now(void)
{
  int slot = cur_slot(), ok = 0, n;
  char path[MAX_PATH];
  SceUID fd;

  shot_wait();
  if(!p.big || !p.chunk || !p.has[slot] || (p.big_slot == slot) || !shot_setup(slot))
    return;
  get_state_slot_path(slot, path);
  fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
  if(fd < 0)
  {
    shot_failed = slot;
    return;
  }
  if((sceIoRead(fd, shot_head, 8) == 8) && !memcmp(shot_head, g_state_str, 4))
  {
    for(n = 0; n < CHUNKS; n++)
    {
      if(sceIoRead(fd, p.chunk, CHUNK_BYTES) != CHUNK_BYTES)
        break;
      shot_take_chunk(slot, n);
    }
    ok = (n == CHUNKS) && (sceIoRead(fd, shot_head, 8) == 8);
  }
  sceIoClose(fd);
  if(ok)
    shot_done(slot);
  else
    shot_failed = slot;
}

static void shot_fail(int slot)
{
  shot_wait();
  shot_failed = slot;
  if(shot_for_thumb)
    p.thumb_ok[slot] = 0;
}

/* The next slot to read: the selected one for its big picture, else any
 * slot still without its 60 x 40 picture. -1: nothing to do. */
static int shot_next(void)
{
  int slot = cur_slot(), i;

  if(p.has[slot] && (p.big_slot != slot) && (shot_failed != slot))
    return slot;
  for(i = 0; i < SLOTS; i++)
    if(p.has[i] && !p.thumb_ok[i] && !p.thumb_tried[i] && (shot_failed != i))
      return i;
  return -1;
}

/* Once a frame: start, advance or finish a read. */
static void load_big_picture(void)
{
  SceInt64 res;
  int slot;

  if(!p.big || !p.chunk)
    return;
  if(shot_state != SHOT_IDLE)
  {
    slot = shot_reading;
    if(sceIoPollAsync(shot_fd, &res) == 1)
      return;  /* still busy */
    /* the cursor left a slot read only for its big picture: drop it */
    if(shot_for_big && !shot_for_thumb && (slot != cur_slot()))
    {
      if((shot_state != SHOT_OPENING) || (res >= 0))
        sceIoClose(shot_fd);
      shot_fd = -1;
      shot_state = SHOT_IDLE;
      shot_reading = -1;
    }
    else
    {
      if(shot_for_big && (slot != cur_slot()))
        shot_for_big = 0;  /* still worth finishing for its 60 x 40 picture */
      switch(shot_state)
      {
        case SHOT_OPENING:
          if(res < 0)
          {
            shot_fd = -1;
            shot_state = SHOT_IDLE;
            shot_failed = slot;
            return;
          }
          shot_state = SHOT_HEAD;
          sceIoReadAsync(shot_fd, shot_head, 8);
          return;
        case SHOT_HEAD:
          if((res != 8) || memcmp(shot_head, g_state_str, 4))
          {
            shot_fail(slot);
            return;
          }
          shot_state = SHOT_CHUNK;
          shot_chunk = 0;
          sceIoReadAsync(shot_fd, p.chunk, CHUNK_BYTES);
          return;
        case SHOT_CHUNK:
          if(res != CHUNK_BYTES)
          {
            shot_fail(slot);
            return;
          }
          shot_take_chunk(slot, shot_chunk);
          if(++shot_chunk < CHUNKS)
            sceIoReadAsync(shot_fd, p.chunk, CHUNK_BYTES);
          else
          {
            shot_state = SHOT_TIME;
            sceIoReadAsync(shot_fd, shot_head, 8);
          }
          return;
        case SHOT_TIME:
          sceIoClose(shot_fd);
          shot_fd = -1;
          shot_state = SHOT_IDLE;
          shot_reading = -1;
          if(res != 8)
          {
            shot_failed = slot;
            return;
          }
          if(shot_for_big)
            p.big_slot = slot;
          if(shot_for_thumb)
          {
            /* the new .thm is written asynchronously too: a blocking write
               cost a frame about 25 ms (PERF run) */
            char path[MAX_PATH];

            p.thumb_ok[slot] = 1;
            get_slot_thumbnail_path(slot, path);
            shot_fd = sceIoOpenAsync(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
            if(shot_fd >= 0)
            {
              shot_state = SHOT_WOPEN;
              shot_reading = slot;
            }
            else
              shot_fd = -1;
          }
          return;
        case SHOT_WOPEN:
          if(res < 0)
          {
            shot_fd = -1;
            shot_state = SHOT_IDLE;
            shot_reading = -1;
            return;
          }
          {
            u32 size;
            const u8 *data = thumbnail_file_data(thumb(slot), THUMB_PITCH, shot_head, &size);
            shot_state = SHOT_WRITE;
            sceIoWriteAsync(shot_fd, data, size);
          }
          return;
        case SHOT_WRITE:
          shot_state = SHOT_WCLOSE;
          sceIoCloseAsync(shot_fd);
          return;
        case SHOT_WCLOSE:
          shot_fd = -1;
          shot_state = SHOT_IDLE;
          shot_reading = -1;
          return;
      }
    }
  }

  slot = shot_next();
  if((slot < 0) || !shot_setup(slot))
    return;
  {
    char path[MAX_PATH];

    get_state_slot_path(slot, path);
    shot_fd = sceIoOpenAsync(path, PSP_O_RDONLY, 0777);
    if(shot_fd < 0)
    {
      shot_fd = -1;
      shot_failed = slot;
      return;
    }
    shot_state = SHOT_OPENING;
    shot_reading = slot;
  }
}

static void slot_name(int slot, char *out)
{
  if(slot == LAST_SLOT)
    strcpy(out, "Last");
  else
    sprintf(out, "Slot %d", slot);
}

/* "21:31" for today, else "11/12" (month/day); RTC ticks in UTC. */
static void format_time(u64 tick, char *out)
{
  u64 local;
  ScePspDateTime t, now;

  if((tick == 0) || (sceRtcConvertUtcToLocalTime(&tick, &local) < 0) ||
   (sceRtcSetTick(&t, &local) < 0))
  {
    strcpy(out, "\xE2\x80\x94");
    return;
  }
  sceRtcGetCurrentClockLocalTime(&now);
  if((t.year == now.year) && (t.month == now.month) && (t.day == now.day))
    sprintf(out, "%02d:%02d", t.hour, t.minute);
  else
    sprintf(out, "%02d/%02d", t.month, t.day);
}

/* ---- drawing ------------------------------------------------------------- */

/* A 20 x 20 tile icon at its real size: a 1 px plum shadow down-right,
 * then the plum ink and the fill. */
static void draw_icon20(int x, int y, const u32 (*bits)[20], u16 fill)
{
  int i, j;

  /* on screen (always, but for the edges of a motion): straight into the
     frame, without a clipped fill per pixel */
  if((x >= 0) && (y >= 0) && (x + 21 <= UI_SCREEN_W) && (y + 21 <= UI_SCREEN_H))
  {
    u16 *row = ui_fb + y * ui_fb_pitch + x;
    for(j = 0; j < 20; j++, row += ui_fb_pitch)
    {
      u32 any = bits[0][j] | bits[1][j];
      for(i = 0; i < 20; i++)
        if(any & (0x80000 >> i))
          row[ui_fb_pitch + i + 1] = UI_COL_PLUM;
    }
    row = ui_fb + y * ui_fb_pitch + x;
    for(j = 0; j < 20; j++, row += ui_fb_pitch)
      for(i = 0; i < 20; i++)
      {
        if(bits[0][j] & (0x80000 >> i))
          row[i] = UI_COL_PLUM;
        else if(bits[1][j] & (0x80000 >> i))
          row[i] = fill;
      }
    return;
  }

  for(j = 0; j < 20; j++)
    for(i = 0; i < 20; i++)
      if((bits[0][j] | bits[1][j]) & (0x80000 >> i))
        ui_fill(x + i + 1, y + j + 1, 1, 1, UI_COL_PLUM);
  for(j = 0; j < 20; j++)
    for(i = 0; i < 20; i++)
    {
      if(bits[0][j] & (0x80000 >> i))
        ui_fill(x + i, y + j, 1, 1, UI_COL_PLUM);
      else if(bits[1][j] & (0x80000 >> i))
        ui_fill(x + i, y + j, 1, 1, fill);
    }
}

static void draw_header(int hy);
static void draw_tile(int i, int ty, int selected);

/* Around the game picture: the Side bars setting, as in the game. */
static u16 edge_colour(int side, int j)
{
  u32 r = 0, g = 0, b = 0;
  int y, k, n = 0;

  for(y = j * 10; y < j * 10 + 10; y++)  /* 16 bands of 10 lines */
    for(k = 0; k < 2; k++)
    {
      u16 c = p.game[y * 240 + (side ? 238 + k : k)];
      r += c & 31; g += (c >> 5) & 31; b += (c >> 10) & 31; n++;
    }
  return COLOR16(r / n, g / n, b / n);
}

static u16 mix(u16 a, u16 b, int t, int n)
{
  int i;
  u16 c = 0;

  for(i = 0; i < 15; i += 5)
  {
    int ca = (a >> i) & 31, cb = (b >> i) & 31;
    c |= ((ca + (cb - ca) * t / n) & 31) << i;
  }
  return c;
}

static void draw_side_bars(int x, int y, int w, int h)
{
  int i, j;

  switch(g_gpsp_config.side_bars)
  {
    case SIDE_BARS_PATTERN:  /* the ground's 8 px check */
      for(j = 0; j < UI_SCREEN_H; j += 8)
        for(i = 0; i < UI_SCREEN_W; i += 8)
          ui_fill(i, j, 8, 8, (((i ^ j) >> 3) & 1) ? UI_COL_PLUM_2 : UI_COL_PLUM);
      break;
    case SIDE_BARS_GAME:
      for(j = 0; j < 16; j++)
      {
        u16 l = edge_colour(0, j), r = edge_colour(1, j);
        int y0 = y + j * h / 16, y1 = y + (j + 1) * h / 16;
        if(j == 0) y0 = 0;
        if(j == 15) y1 = UI_SCREEN_H;
        ui_fill(0, y0, x, y1 - y0, l);
        ui_fill(x + w, y0, UI_SCREEN_W - x - w, y1 - y0, r);
        if((j == 0) || (j == 15))
        {
          /* above and below the picture: left edge to right edge */
          int ty = (j == 0) ? 0 : y + h, th = (j == 0) ? y : UI_SCREEN_H - y - h;
          for(i = 0; (th > 0) && (i < 16); i++)
            ui_fill(x + i * w / 16, ty, (i + 1) * w / 16 - i * w / 16, th, mix(l, r, i, 15));
        }
      }
      break;
    default:
      ui_fill(0, 0, UI_SCREEN_W, UI_SCREEN_H, COLOR16(0, 0, 0));
      break;
  }
}

/* The static ground: the game picture as it was on the LCD, screen-doored.
 * Once the screen has opened (full), the header and the tiles at rest go
 * in too, so a frame draws only what moves (spec section 6: repeated
 * actions move little); opening and closing move them, so then they are
 * drawn every frame. */
static void build_layer(int full)
{
  int x, y, w, h, i;

  p.layer_full = 0;
  if(!ui_layer_begin())
    return;
  get_game_rect(&x, &y, &w, &h);
  draw_side_bars(x, y, w, h);
  ui_blit_scaled(p.game, 240, 160, x, y, w, h);
  ui_door(0, 0, UI_SCREEN_W, UI_SCREEN_H);
  if(full)
  {
    draw_header(6);
    for(i = 0; i < TILE_COUNT; i++)
      draw_tile(i, TILE_Y, 0);
  }
  ui_layer_end();
  p.layer_full = full;
}

/* The header chip: the game's name (as the library shows it) and "Paused".
 * It ends by x 380, clear of the clock and battery chip: a longer name is
 * cut with "…", and once the screen has rested 40 f it runs sideways
 * inside its width (shift > 0; spec section 4, Long game names). */
static int header_width(const char *title, int *full)
{
  int pw = ui_text_width("Paused"), maxw = 380 - 14 - 34 - pw;

  *full = ui_print_width(title, UI_FONT_BODY, UI_TEXT_BOLD);
  return (*full < maxw) ? *full : maxw;
}

static void draw_header_shift(int hy, int shift)
{
  char title[256];
  int full, tw;

  ui_display_name(gamepak_filename, title);
  tw = header_width(title, &full);
  ui_round_fill(14, hy, tw + ui_text_width("Paused") + 34, 24, 3, UI_COL_PLUM);
  if((shift <= 0) || (full <= tw))
    ui_print_fit(22, hy + 4, title, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD, tw);
  else
  {
    ui_clip_set(22, hy + 4, tw, 16);
    ui_print(22 - shift, hy + 4, title, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD);
    ui_clip_reset();
  }
  ui_text(30 + tw, hy + 6, "Paused", UI_COL_MUTE);
}

static void draw_header(int hy)
{
  draw_header_shift(hy, 0);
}

/* the marquee: 40 f at rest, then 1 px a frame to the end, a wait as
   long, and again */
static void header_marquee(void)
{
  char title[256];
  int full, tw, over, t, cycle;

  ui_display_name(gamepak_filename, title);
  tw = header_width(title, &full);
  over = full - tw;
  t = p.title_f - UI_MARQUEE_DELAY;
  if((over <= 0) || (t <= 0))
    return;
  cycle = over + 2 * UI_MARQUEE_DELAY;
  t %= cycle;
  t = (t < over) ? t : (t < over + UI_MARQUEE_DELAY) ? over : 0;
  if(t > 0)
  {
    ui_fill(22, 6 + 4, tw, 16, UI_COL_PLUM);
    draw_header_shift(6, t);
  }
}


static void draw_slot(int slot, int dd, int dy, int selected)
{
  int big = (dd > -128) && (dd < 128), ad = dd < 0 ? -dd : dd;
  int w = big ? BIG_W : SMALL_W, h = big ? BIG_H : SMALL_H, x, y;
  char label[16];

  if(ad > 922)  /* 3.6 slots away: off the strip */
    return;
  if(big)
  {
    x = STRIP_CX - w / 2 + dd * 116 / 256;
    y = 46 + dy;  /* 10 px under the header */
    ui_round_fill(x + 3, y + 3, w + 8, h + 8, 3, UI_COL_SHADE);
    ui_round_fill(x - 4, y - 4, w + 8, h + 8, 3, UI_COL_SKY);
  }
  else
  {
    int off = (ad >= 256) ? 116 + (ad - 256) * 76 / 256 : ad * 116 / 256;
    x = STRIP_CX + ((dd > 0) ? off : -off) - w / 2;
    y = 74 + dy;  /* centred on the big picture (46-142) */
    ui_round_fill(x - 2, y - 2, w + 4, h + 4, 2, UI_COL_CARD);
  }

  if(p.has[slot])
  {
    if(big && (p.big_slot == slot))
      ui_blit(p.big, BIG_W, x, y, BIG_W, BIG_H);  /* box-filtered once */
    else if(big && p.thumb_ok[slot])
      ui_stretch_smooth(thumb(slot), SMALL_W, SMALL_H, THUMB_PITCH, x, y, w, h);
    else if(p.thumb_ok[slot])
      ui_blit(thumb(slot), THUMB_PITCH, x, y, SMALL_W, SMALL_H);
    else
    {
      ui_fill(x, y, w, h, UI_COL_PLUM);
      ui_checker(x, y, w, h, UI_COL_CARD);
    }
  }
  else
  {
    ui_fill(x, y, w, h, UI_COL_PLUM);
    ui_checker(x, y, w, h, UI_COL_CARD);
    if(big)
    {
      int gw = ui_glyph_width(ui_key_confirm) + 4 + ui_print_width("Save", UI_FONT_BODY, 0);
      int gx = ui_glyph(x + (w - gw) / 2, y + h / 2 - 6, ui_key_confirm) + 4;
      ui_print(gx, y + h / 2 - 8, "Save", UI_COL_CREAM, UI_FONT_BODY, 0);
    }
  }

  /* a hold past 12 f (delete, or load with the save off): the picture is
     screen-doored and a card names the action over its bar */
  if(big && p.has[slot] && (p.hold.shown || p.load_hold.shown))
  {
    ui_hold_t *hd = p.hold.shown ? &p.hold : &p.load_hold;
    /* with the save off, a filled load hold keeps its card: the line
       becomes "Loading slot 3..." while the load runs */
    const char *verb = p.hold.shown ? "Delete (hold)" :
     (p.hold_line[0] ? p.hold_line : "Load (hold)");
    /* wide enough for both lines, so the card keeps its width when the
       slow load's line replaces the hold's */
    int cw = ((p.hold_line[0] && (ui_text_width(p.hold_line) > ui_text_width("Load (hold)"))) ?
     ui_text_width(p.hold_line) : ui_text_width((char *)verb)) + 20, cx, cy;

    if(cw < 96)
      cw = 96;
    cx = x + (w - cw) / 2;
    cy = y + (h - 34) / 2;
    ui_checker(x, y, w, h, UI_COL_SHADE);
    ui_round_fill(cx + 3, cy + 3, cw, 34, 2, UI_COL_SHADE);
    ui_round_fill(cx, cy, cw, 34, 2, UI_COL_CARD);
    if(p.hold_line[0] && !p.hold.shown)
      /* the hold fired and the load is slow: its line alone, centred in
         the same place and width, no bar */
      ui_text(cx + (cw - ui_text_width((char *)verb)) / 2, cy + 10, (char *)verb, UI_COL_CREAM);
    else
    {
      ui_text(cx + (cw - ui_text_width((char *)verb)) / 2, cy + 3, (char *)verb, UI_COL_CREAM);
      ui_hold_bar(cx + 8, cy + 22, cw - 16, hd->p, p.hold.shown ? UI_COL_PINK : UI_COL_LIME);
    }
  }

  if(slot == LAST_SLOT)
    strcpy(label, "Last");
  else
    sprintf(label, "%d", slot);
  ui_round_fill(x + 2, y + h - 14, ui_text_width(label) + 8, 13, 2, UI_COL_PLUM);
  ui_text(x + 6, y + h - 15, label, UI_COL_CREAM);

  if(selected)
    ui_cursor_frame(x - 6, y - 6, w + 12, h + 12);
}

static void draw_tile(int i, int ty, int selected)
{
  int tx = TILE_X(i), lift = selected ? UI_LIFT : 0, j, lines = 0;
  char line[2][64];

  /* lifted straight up, so with 4 px between tiles the cursor frame keeps
     clear of both neighbours; the shadow 3 px right and 1 down */
  if(selected)
    ui_round_fill(tx + 3, ty + 1, TILE_W, TILE_H, 2, UI_COL_SHADE);
  ui_round_fill(tx, ty - lift, TILE_W, TILE_H, 2, selected ? tiles[i].color : UI_COL_CARD);
  ty -= lift;
  /* centred in the tile: 10 px above the icon box, about 11 under the
     label (design repo 4aff042) */
  ui_round_fill(tx + 21, ty + 10, 28, 28, 2, selected ? UI_COL_CREAM : tiles[i].color);
  draw_icon20(tx + 24, ty + 13, tiles[i].icon, selected ? tiles[i].color : UI_COL_CREAM);
  for(j = 0; j < 2; j++)
    if(ui_wrap_line(tiles[i].label, j, 66, UI_FONT_SMALL, line[j], sizeof(line[j])))
      lines++;
  for(j = 0; j < lines; j++)
    ui_text(tx + TILE_W / 2 - ui_text_width(line[j]) / 2, ty + 41 + j * 12 - ((lines == 2) ? 6 : 0),
     line[j], selected ? UI_COL_PLUM : UI_COL_CREAM);
}

/* Offsets of the moving parts at this point of opening or closing. */
static void motion(int *hy, int *dy, int tile_dy[TILE_COUNT], int *done)
{
  int i;

  *done = 0;
  if(p.closing)
  {
    *hy = 6 - ui_ease_in_cubic(0, 36, p.close_f, F_CLOSE);
    *dy = ui_ease_in_cubic(0, 40, p.close_f, F_CLOSE);
    for(i = 0; i < TILE_COUNT; i++)
      tile_dy[i] = ui_ease_in_cubic(0, 16, p.close_f, F_CLOSE);
    return;
  }
  /* the door is there at once (the ground layer); then the header, the
     strip, and the tiles one frame apart */
  *hy = -30 + ui_ease_out_back(0, 36, p.open_f, F_OPEN);
  *dy = ui_ease_out_back(40, 0, p.open_f, 14);
  for(i = 0; i < TILE_COUNT; i++)
  {
    int f = p.open_f - 2 - i;
    tile_dy[i] = (f <= 0) ? 999 : ui_ease_out_back(16, 0, f, 12);
  }
  *done = p.open_f >= F_OPEN;
}

static int undo_offered(void);

/* With "Save when leaving a game" off, Exit, Restart and Library lose the
 * play since the last save: they take a 60 f hold, a tap does nothing. */
static int tile_needs_hold(int i)
{
  return !g_gpsp_config.save_on_leave &&
   ((i == TILE_EXIT) || (i == TILE_RESTART) || (i == TILE_LIBRARY));
}

static const char *tile_verb(void)
{
  if(!tile_needs_hold(p.tile))
    return "Open";
  return (p.tile == TILE_EXIT) ? "Hold: quit" : ((p.tile == TILE_RESTART) ? "Hold: restart" : "Hold: leave");
}

static void draw_hints(int done)
{
  ui_hint_t c1[2], c[6];
  u32 resume_keys = PSP_CTRL_TRIANGLE | (g_use_home ? PSP_CTRL_HOME : 0);
  int n, full = p.has[cur_slot()];

  if(p.bar_text || p.busy || !done || p.closing)
  {
    ui_hint_strip(c1, 0);  /* nothing to press while saving */
    return;
  }

  /* steady hints first, the changing ones last (spec: Hint strip) */
  n = 0;
  c[n].keys = resume_keys; c[n++].verb = "Resume";
  if(p.row == 0)
  {
    c[n].keys = PSP_CTRL_LTRIGGER; c[n++].verb = "Last";
    c[n].keys = PSP_CTRL_RTRIGGER; c[n++].verb = "Slot 0";
    c[n].keys = PSP_CTRL_SELECT;
    c[n++].verb = undo_offered() ? "Undo" : "Save \xC2\xB7 Hold: delete";
    c[n].keys = ui_key_confirm;
    c[n++].verb = !full ? "Save" : (g_gpsp_config.save_on_leave ? "Load" : "Hold: load");
  }
  else
  {
    c[n].keys = PSP_CTRL_UP;    c[n++].verb = "Savestates";
    c[n].keys = ui_key_confirm; c[n++].verb = (char *)tile_verb();
  }
  ui_hint_packed(c, n, 10);
}

/* The saving card: a title, 30 blocks and a note (spec "exit"). */
static void draw_bar_card(int fill_256)
{
  int w = 332, h = p.bar_note ? 76 : 54, x = (UI_SCREEN_W - w) / 2, y = 72;
  int k = (30 * fill_256 + 128) / 256 - 1;

  /* clear of the slot line (155) and the lifted tiles' frames; it rises
     20 px into place (card_rise) */
  if(p.bar_f < UI_F_CARD_RISE)
    p.bar_f++;
  y = ui_ease_out_back(y + 20, y, p.bar_f, UI_F_CARD_RISE);

  /* no dim behind it: a second door over the screen-doored game reads as
     noise (maintainer, 2026-11-15) */
  ui_round_fill(x + 4, y + 4, w, h, 3, UI_COL_SHADE);
  ui_round_fill(x, y, w, h, 3, UI_COL_CARD);
  ui_fill(x, y + 4, 4, h - 8, p.bar_col);
  ui_print(x + 16, y + 10, p.bar_text, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD);
  {
    int i;
    for(i = 0; i < 30; i++)
      ui_fill(x + 16 + i * 10, y + 32, 8, 12, (i <= k) ? p.bar_col : UI_COL_PLUM_2);
  }
  if(p.bar_note)
    ui_print_fit(x + 16, y + 52, p.bar_note, UI_COL_MUTE, UI_FONT_SMALL, 0, w - 28);
}

/* Exit, Restart or Library with the save off: the hold card, shown from
 * 12 f (already 20 % full) until it fires or drains. */
static void draw_tile_hold(void)
{
  int w = 312, x = 462 - w, y = 88, gx;
  u16 col = (p.tile == TILE_RESTART) ? UI_COL_TANGERINE : UI_COL_PINK;
  const char *line = (p.tile == TILE_EXIT) ? "Hold to quit without saving" :
   ((p.tile == TILE_RESTART) ? "Hold to restart without saving" : "Hold to leave without saving");

  if(!p.exit_hold.shown || (p.exit_hold.p <= 0))
    return;
  ui_round_fill(x + 3, y + 3, w, 56, 3, UI_COL_SHADE);
  ui_round_fill(x, y, w, 56, 3, UI_COL_CARD);
  ui_fill(x, y + 4, 4, 48, col);
  gx = ui_glyph(x + 14, y + 9, ui_key_confirm) + 6;
  ui_print_fit(gx, y + 7, line, UI_COL_CREAM, UI_FONT_BODY, 0, x + w - 10 - gx);
  ui_hold_bar(x + 14, y + 28, w - 28, p.exit_hold.p, col);
  ui_print_fit(x + 14, y + 38, "'Save when leaving a game' is off.", UI_COL_MUTE, UI_FONT_SMALL, 0, w - 28);
}

#ifdef UI_PERF
/* draw_screen() sections, in microseconds: ground, header and status,
   strip, slot line, tiles, cards and toast, hints */
#define PERF_SECTIONS 7
static u32 perf_sec[PERF_SECTIONS], perf_mark;
#define PERF_SECTION(k) do { u32 t_ = sceKernelGetSystemTimeLow(); perf_sec[k] = t_ - perf_mark; perf_mark = t_; } while(0)
#else
#define PERF_SECTION(k) do { } while(0)
#endif

static void draw_screen(int bar_fill)
{
  int hy, dy, tile_dy[TILE_COUNT], done, i;
  char info[48], when[16];

#ifdef UI_PERF
  perf_mark = sceKernelGetSystemTimeLow();
#endif
  ui_frame_begin();
  ui_ground();
  PERF_SECTION(0);
  if(!ui_layer_ready())
  {
    /* no pool memory: the game picture is drawn every frame */
    int x, y, w, h;
    get_game_rect(&x, &y, &w, &h);
    draw_side_bars(x, y, w, h);
    ui_blit_scaled(p.game, 240, 160, x, y, w, h);
    ui_door(0, 0, UI_SCREEN_W, UI_SCREEN_H);
  }

  motion(&hy, &dy, tile_dy, &done);
  if(!p.layer_full)
    draw_header(hy);
  else if(done && !p.closing)
    header_marquee();
  p.title_f++;
  if(done && !p.closing && !ui_toast_up(p.toast_f))
    ui_status_chip_at(10);
  PERF_SECTION(1);

  /* the strip: the selected slot big, its neighbours small */
  if(p.wrap_dir && (p.pos_f < UI_F_SCROLL))
  {
    /* a wrap: the end being left slides out the way the strip moves while
       the other end comes in from the other side, 4 places each, with a
       one-slot move's overshoot; the cursor frame stays in the centre */
    int shift = 3 * 256 * p.pos_f / UI_F_SCROLL + ui_ease_out_back(0, 256, p.pos_f, UI_F_SCROLL);
    int d = p.wrap_dir;
    for(i = 0; i < SLOTS; i++)
    {
      draw_slot(strip_slot[i], (i - p.wrap_from) * 256 - d * shift, dy, 0);
      draw_slot(strip_slot[i], (i - p.cur) * 256 + d * (4 * 256 - shift), dy, 0);
    }
    if((p.row == 0) && done && !p.closing && !p.bar_text)
      ui_cursor_frame(STRIP_CX - BIG_W / 2 - 6, 46 + dy - 6, BIG_W + 12, BIG_H + 12);
  }
  else
  {
    int pos = (p.pos_f < UI_F_SCROLL) ?
     ui_ease_out_back(p.pos_from, p.cur * 256, p.pos_f, UI_F_SCROLL) : p.cur * 256;
    for(i = 0; i < SLOTS; i++)
      draw_slot(strip_slot[i], i * 256 - pos, dy, (i == p.cur) && (p.row == 0) && done && !p.closing && !p.bar_text);
  }
  PERF_SECTION(2);
  if(done && !p.closing)
  {
    slot_name(cur_slot(), info);
    if(p.has[cur_slot()])
    {
      format_time(p.time[cur_slot()], when);
      strcat(info, " \xC2\xB7 ");
      strcat(info, when);
    }
    else
      strcat(info, " \xC2\xB7 \xE2\x80\x94");
    /* on a plum pill: cream text alone is lost on a busy game picture */
    {
      int iw = ui_text_width(info);
      ui_round_fill(STRIP_CX - iw / 2 - 6, 155, iw + 12, 15, 2, UI_COL_PLUM);
      ui_text(STRIP_CX - iw / 2, 156, info, UI_COL_CREAM);
    }
  }
  PERF_SECTION(3);

  for(i = 0; i < TILE_COUNT; i++)
  {
    int sel = (p.row == 1) && (i == p.tile) && done;
    /* at rest the tiles are in the ground; only the selected one is drawn */
    if((tile_dy[i] < 999) && (!p.layer_full || sel))
      draw_tile(i, TILE_Y + tile_dy[i], sel);
  }
  if((p.row == 1) && done && !p.closing && !p.bar_text)
  {
    int tx = (p.tile_f < UI_F_CURSOR) ?
     ui_ease_out_back(p.tile_from, TILE_X(p.tile), p.tile_f, UI_F_CURSOR) : TILE_X(p.tile);
    ui_cursor_frame(tx, TILE_Y - 2, TILE_W, TILE_H);
  }
  PERF_SECTION(4);

  if((p.row == 1) && done && !p.closing)
    draw_tile_hold();
  /* top right, so it never hides the tiles' labels; the status chip makes
     way while it is up */
  if(p.toast_f >= 0)
  {
    if(!ui_toast_at(p.toast, p.toast_edge, p.toast_undo ? "Undo" : NULL, p.toast_f, 6))
      p.toast_f = -1;
  }
  if(p.bar_text)
    draw_bar_card(bar_fill);
  PERF_SECTION(5);
  draw_hints(done);
  PERF_SECTION(6);
}

/* ---- saving with a moving bar -------------------------------------------- */

static void bar_hook(int current, int total)
{
  int fill = total ? current * 256 / total : 0;

  if(current == 0)
    p.bar_phase++;
  if(fill > 256)
    fill = 256;
  /* two phases (the save to Last, then the load): 3/4 and 1/4 of the bar */
  if(p.bar_phases == 2)
    fill = (p.bar_phase <= 0) ? fill * 3 / 4 : 192 + fill / 4;
  /* nothing shows for work done within 10 frames, so a quick save or
     load never flashes a card (spec: progress cards show only past 10 f) */
  if(sceKernelGetSystemTimeLow() - p.busy_start < 10 * 16667)
    return;
  draw_screen(fill);
  ui_frame_end_now();
}

/* col: the action's colour (pink Exit; sky loads, saves and Restart kai;
 * tangerine the game's own Restart) */
static void run_with_bar(const char *text, const char *note, u16 col)
{
  p.bar_text = text;
  p.bar_col = col;
  p.bar_f = 0;
  p.bar_note = note;
  p.bar_phase = -1;
  p.bar_phases = 1;
  p.busy = 1;
  p.busy_start = sceKernelGetSystemTimeLow();
  progress_hook = bar_hook;
  state_no_question = 1;
  state_error_text = NULL;
}

static void end_bar(void)
{
  progress_hook = NULL;
  state_no_question = 0;
  p.bar_text = NULL;
  p.bar_note = NULL;
  p.busy = 0;
  pause_sound(1);  /* the savestate code turns the sound back on */
}

/* ---- actions ------------------------------------------------------------- */

/* Is Undo on offer? While it is, SELECT means Undo. Moving the cursor
 * ends it (maintainer, 2026-11-16): the toast slides out and SELECT saves
 * again at once; staying put keeps it for the toast's 2 s. */
static int undo_offered(void)
{
  return ui_toast_up(p.toast_f) && p.toast_undo && !p.undo_ended;
}

static void end_undo(void)
{
  if(!undo_offered())
    return;
  p.undo_ended = 1;
  if(p.toast_f < UI_F_TOAST_IN + UI_F_TOAST_HOLD)
    p.toast_f = UI_F_TOAST_IN + UI_F_TOAST_HOLD;
}

static void show_toast(const char *text, u16 edge, int undo, int undo_slot)
{
  p.undo_ended = 0;
  snprintf(p.toast, sizeof(p.toast), "%s", text);
  p.toast_edge = edge;
  p.toast_undo = (undo != UNDO_NONE);
  p.undo = undo;
  p.undo_slot = undo_slot;
  p.toast_f = 0;
}

/* After a load the game is another moment: show its picture behind. */
static void take_loaded_picture(void)
{
  memcpy(p.game, savestate_write_buffer + 8, 240 * 160 * 2);
  build_layer(0);
}

static void action_save(int slot)
{
  char name[MAX_FILE], text[48];
  int had = p.has[slot];

  shot_wait();  /* the picture reader shares p.big and the Memory Stick */
  get_savestate_filename_noshot(slot, name);
  slot_name(slot, text);
  run_with_bar("Saving\xE2\x80\xA6", NULL, UI_COL_SKY);
  save_state(name, p.game, slot);
  end_bar();
  read_slot(slot);
  if(p.has[slot])
  {
    char msg_text[64];
    /* the big picture is the screenshot just saved: no need to read it */
    if(p.big)
    {
      ui_shrink_53_rows(p.game, 160, p.big);
      p.big_slot = slot;
    }
    sprintf(msg_text, "Saved to %s", (slot == LAST_SLOT) ? "Last" : text);
    show_toast(msg_text, UI_COL_SKY, had ? UNDO_SAVE : UNDO_SAVE_NEW, slot);
    g_game_config.last_slot = slot;
  }
  else
    show_toast(state_error_text ? state_error_text : "Saving failed", UI_COL_PINK, UNDO_NONE, 0);
}

static void action_delete(int slot)
{
  char path[MAX_PATH], text[48], msg_text[64];

  shot_wait();
  if(slot == LAST_SLOT)
    delete_last_state();
  else
  {
    get_state_slot_path(slot, path);
    delete_state_files(path);
  }
  read_slot(slot);
  slot_name(slot, text);
  sprintf(msg_text, "%s deleted", text);
  show_toast(msg_text, UI_COL_PINK, UNDO_DELETE, slot);
}

static u32 load_slot(int slot)
{
  char name[MAX_FILE];
  u32 ok;

  if(slot == LAST_SLOT)
  {
    load_last_state();
    get_savestate_filename_noshot(LAST_SLOT, name);
    ok = load_state(name, MEM_STATE_NUM);
  }
  else
  {
    get_savestate_filename_noshot(slot, name);
    ok = load_state(name, slot);
  }
  return ok;
}

/* Load and go straight back to the game. With "Save when leaving a game"
 * on, the game before goes to Last first (loading Last swaps the two), so
 * loading Last is the undo; one bar covers both. Returns 1 when loaded. */
static int action_load(int slot)
{
  char text[48], bar[48];
  int keep = g_gpsp_config.save_on_leave, swap = keep && (slot == LAST_SLOT);
  u32 ok;

  shot_wait();
  if(!p.has[slot])
    return 0;
  if(slot == LAST_SLOT)
    strcpy(bar, "Loading Last\xE2\x80\xA6");
  else
    sprintf(bar, "Loading slot %d\xE2\x80\xA6", slot);
  if(keep)
    run_with_bar(bar, NULL, UI_COL_SKY);
  else
  {
    /* the save is off: no new card; the filled hold card says it */
    run_with_bar(NULL, NULL, UI_COL_SKY);
    strcpy(p.hold_line, bar);
    p.load_hold.p = 256;
    p.load_hold.shown = 1;
  }
  if(swap)
  {
    p.bar_phases = 2;
    ok = swap_last_state(p.game);
  }
  else
  {
    ok = 1;
    if(keep)
    {
      p.bar_phases = 2;
      ok = save_last_state(p.game);  /* nothing is loaded if this fails */
    }
    if(ok)
      ok = load_slot(slot);
  }
  end_bar();
  p.hold_line[0] = 0;
  p.load_hold.p = 0;
  p.load_hold.shown = 0;
  if(keep)
    read_slot(LAST_SLOT);

  if(!ok)
  {
    if((state_error_text == msg[MSG_LOAD_STATE_OTHER_GAME]) && state_error_game[0])
    {
      /* spec err_state: say which game it belongs to and offer to play
         it (PROTOTYPE: without the slot's picture on the card) */
      char who[256], body[400], play[300];
      ui_display_name(state_error_game, who);
      snprintf(body, sizeof(body),
       "This savestate file was made while playing %s. It only loads in that game.", who);
      snprintf(play, sizeof(play), "Play %s", who);
      if(ui_error_card("Made by another game", body, play, "Cancel", UI_COL_SKY))
      {
        if(ui_library_has_rom(state_error_game))
        {
          p.switch_slot = slot;
          return -1;  /* the caller leaves for that game */
        }
        /* that game is not there: the player stays in this one */
        build_layer(1);
        ui_input_init();
        {
          char t[64];  /* a toast's text: a long name is cut */
          snprintf(t, sizeof(t), "%.36s isn't in your games folder", who);
          show_toast(t, UI_COL_PINK, UNDO_NONE, 0);
        }
        return 0;
      }
      build_layer(1);
      ui_input_init();
      return 0;
    }
    show_toast(state_error_text ? state_error_text : "Loading failed", UI_COL_PINK, UNDO_NONE, 0);
    return 0;
  }
  p.changed = 1;
  take_loaded_picture();
  g_game_config.last_slot = slot;
  if(slot == LAST_SLOT)
    strcpy(text, "Last");
  else
    sprintf(text, "slot %d", slot);
  if(swap)
    strcpy(p.game_toast, "Swapped with Last");
  else if(keep)
    snprintf(p.game_toast, sizeof(p.game_toast), "Loaded %s \xC2\xB7 the game before is in Last", text);
  else
    snprintf(p.game_toast, sizeof(p.game_toast), "Loaded %s", text);
  p.closing = 1;
  return 1;
}

static void action_undo(void)
{
  char path[MAX_PATH];
  int slot = p.undo_slot;

  shot_wait();
  switch(p.undo)
  {
    case UNDO_SAVE:
    case UNDO_DELETE:
      get_state_slot_path(slot, path);
      undo_state_files(path);
      if(slot == LAST_SLOT)
        load_last_state();
      read_slot(slot);
      show_toast((p.undo == UNDO_SAVE) ? "The old savestate is back" : "The savestate is back",
       UI_COL_SKY, UNDO_NONE, 0);
      break;
    case UNDO_SAVE_NEW:  /* the slot was empty: empty it again */
      get_state_slot_path(slot, path);
      delete_state_files(path);
      if(slot == LAST_SLOT)
        load_last_state();
      read_slot(slot);
      show_toast("The slot is empty again", UI_COL_SKY, UNDO_NONE, 0);
      break;
  }
}

/* Exit, with the save on: Last first. (With it off the hold has already
 * confirmed.) */
static int action_exit(void)
{
  shot_wait();
  if(!g_gpsp_config.save_on_leave)
    return 1;
  run_with_bar("Saving to Last\xE2\x80\xA6", "Then kai closes. Continue picks up here.", UI_COL_PINK);
  save_last_state(p.game);
  end_bar();
  return 1;
}

/* Restart the game, like Exit (spec: Restart): with the save on the game
 * goes to Last first, on a tangerine card, and a toast over the game says
 * where it is; with it off the hold has confirmed. */
static void action_restart_game(void)
{
  shot_wait();
  if(g_gpsp_config.save_on_leave)
  {
    run_with_bar("Saving to Last\xE2\x80\xA6", "Then the game starts again from the beginning.",
     UI_COL_TANGERINE);
    save_last_state(p.game);
    end_bar();
    read_slot(LAST_SLOT);
    strcpy(p.game_toast, "Restarted \xC2\xB7 the game before is in Last");
  }
  reset_gba();
  cheat_engine_restarted();  /* cheats waiting for a restart have it */
  reg[CHANGED_PC_STATUS] = 1;
  p.changed = 1;
  p.closing = 1;
}

/* Library, like Exit: with the save on Last first. */
static void action_library(void)
{
  shot_wait();
  if(!g_gpsp_config.save_on_leave)
    return;
  run_with_bar("Saving to Last\xE2\x80\xA6", NULL, UI_COL_TANGERINE);
  save_last_state(p.game);
  end_bar();
}

/* Quit kai (System, a pink hold): save like Exit, then close kai and go
 * back to the PSP's home menu. Does not return. */
static void quit_kai(void)
{
  build_layer(1);  /* the pause screen behind the card */
  action_exit();
  quit(0);
}

/* Restart kai (spec "Restart kai"): save like Exit, show the sky card and
 * start kai again; the note brings the player back to this Settings row.
 * Does not return. */
static void restart_kai(void)
{
  static const char then[] = "Then kai starts again and brings you back here.";
  int tab, row, save = g_gpsp_config.save_on_leave;

  ui_settings_where(&tab, &row);
  build_layer(1);  /* the pause screen behind the card */
  if(save)
  {
    run_with_bar("Saving to Last\xE2\x80\xA6", then, UI_COL_SKY);
    save_last_state(p.game);
    end_bar();
  }
  /* the relaunch itself is the rest of the work: the card shows full */
  p.bar_text = "Restarting kai\xE2\x80\xA6";
  p.bar_note = then;
  p.bar_col = UI_COL_SKY;
  p.bar_f = UI_F_CARD_RISE;
  p.busy = 1;
  draw_screen(256);
  ui_frame_end_now();
  /* with the save off the game is not reopened: kai starts on the library */
  ui_restart_now(tab, row, save ? gamepak_path : NULL);
}

/* Back from a restart: load Last over the freshly started game and open
 * Settings on the row the player left. */
static void restart_return(void)
{
  int tab = ui_restart_note.tab, row = ui_restart_note.row;

  ui_restart_note.pending = 0;
  run_with_bar(NULL, NULL, UI_COL_SKY);
  if(load_slot(LAST_SLOT))
  {
    p.changed = 1;
    take_loaded_picture();
    g_game_config.last_slot = LAST_SLOT;
  }
  end_bar();
  ui_settings_game = p.game;
  switch(ui_settings(tab, row))
  {
    case 1: restart_kai(); break;  /* do not return */
    case 2: quit_kai(); break;
  }
  build_layer(0);  /* Settings used the layer */
  ui_status_read();
  ui_input_init();
}

/* ---- the screen ------------------------------------------------------------ */

static void move_slot(int to)
{
  if((to < 0) || (to >= SLOTS) || (to == p.cur))
    return;
  end_undo();
  p.wrap_dir = 0;
  p.pos_from = (p.pos_f < UI_F_SCROLL) ?
   ui_ease_out_back(p.pos_from, p.cur * 256, p.pos_f, UI_F_SCROLL) : p.cur * 256;
  p.cur = to;
  p.pos_f = 0;
  p.hold.p = 0;
  p.hold.shown = 0;
  p.load_hold.p = 0;
  p.load_hold.shown = 0;
}

/* A d-pad step; a fresh press past an end wraps, and the wrap slides the
 * short way, as a one-slot move (spec: Pause, maintainer 2026-11-17). */
static void step_slot(int to)
{
  int wrap = 0;

  if((p.cur == SLOTS - 1) && (to == 0))
    wrap = 1;
  else if((p.cur == 0) && (to == SLOTS - 1))
    wrap = -1;
  if(!wrap)
  {
    move_slot(to);
    return;
  }
  move_slot(to);
  p.wrap_dir = wrap;
  p.wrap_from = (wrap > 0) ? SLOTS - 1 : 0;
}

static void move_tile(int to)
{
  if(to == p.tile)
    return;
  end_undo();
  p.tile_from = (p.tile_f < UI_F_CURSOR) ?
   ui_ease_out_back(p.tile_from, TILE_X(p.tile), p.tile_f, UI_F_CURSOR) : TILE_X(p.tile);
  p.tile = to;
  p.tile_f = 0;
  memset(&p.exit_hold, 0, sizeof(p.exit_hold));
}

#ifdef UI_PERF
/* Measuring build (make UI4=1 PERF=1): once the pause screen has opened it
 * drives itself through a fixed script, logs every frame and closes, then
 * writes <kai>/perf_pause.csv. */
static const struct
{
  const char *name;
  int frames;
  u32 keys;
  int tap_every;  /* 0: held the whole phase */
} perf_phases[] =
{
  { "still",      120, 0,              0 },
  { "tap_right",  200, PSP_CTRL_RIGHT, 20 },
  { "hold_right", 150, PSP_CTRL_RIGHT, 0 },
  { "hold_left",  150, PSP_CTRL_LEFT,  0 },
  { "still_end",   60, 0,              0 },
};
#define PERF_PHASES (int)(sizeof(perf_phases) / sizeof(perf_phases[0]))
#define PERF_MAX 800

static struct
{
  u8 phase;
  u32 io, draw, period;
  u16 sec[PERF_SECTIONS];
} perf_log[PERF_MAX];
static int perf_phase, perf_f, perf_n, perf_on, perf_done;

static u32 perf_script(void)
{
  if(!perf_on || (perf_phase >= PERF_PHASES))
    return 0;
  if(perf_phases[perf_phase].tap_every)
    return ((perf_f % perf_phases[perf_phase].tap_every) < 3) ? perf_phases[perf_phase].keys : 0;
  return perf_phases[perf_phase].keys;
}

static void perf_step(u32 io, u32 draw, u32 period)
{
  if(!perf_on || (perf_phase >= PERF_PHASES))
    return;
  if(perf_n < PERF_MAX)
  {
    perf_log[perf_n].phase = perf_phase;
    perf_log[perf_n].io = io;
    perf_log[perf_n].draw = draw;
    perf_log[perf_n].period = period;
    {
      int k;
      for(k = 0; k < PERF_SECTIONS; k++)
        perf_log[perf_n].sec[k] = (perf_sec[k] > 0xFFFF) ? 0xFFFF : perf_sec[k];
    }
    perf_n++;
  }
  if(++perf_f >= perf_phases[perf_phase].frames)
  {
    perf_f = 0;
    if(++perf_phase >= PERF_PHASES)
      p.closing = 1;
  }
}

static void perf_write(void)
{
  char path[MAX_PATH], line[128];
  SceUID fd;
  int i, saved = 0;

  for(i = 0; i < SLOTS; i++)
    saved += p.has[i] ? 1 : 0;
  path_printf(path, sizeof(path), "%s/perf_pause.csv", main_path);
  fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  if(fd < 0)
    return;
  sprintf(line, "# cpu %d MHz, bus %d MHz, filter %d, saved slots %d, frames %d\n",
   scePowerGetCpuClockFrequency(), scePowerGetBusClockFrequency(),
   (int)g_gpsp_config.screen_filter, saved, perf_n);
  sceIoWrite(fd, line, strlen(line));
  strcpy(line, "phase,frame,io_us,draw_us,period_us,ground,header,strip,slotline,tiles,cards,hints\n");
  sceIoWrite(fd, line, strlen(line));
  for(i = 0; i < perf_n; i++)
  {
    sprintf(line, "%s,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", perf_phases[perf_log[i].phase].name, i,
     (unsigned)perf_log[i].io, (unsigned)perf_log[i].draw, (unsigned)perf_log[i].period,
     perf_log[i].sec[0], perf_log[i].sec[1], perf_log[i].sec[2], perf_log[i].sec[3],
     perf_log[i].sec[4], perf_log[i].sec[5], perf_log[i].sec[6]);
    sceIoWrite(fd, line, strlen(line));
  }
  sceIoClose(fd);
}
#endif

enum { LEAVE_NONE, LEAVE_RESUME, LEAVE_OLD_MENU, LEAVE_LIBRARY, LEAVE_SWITCH, LEAVE_QUIT };

/* Sleep (spec: Sleep): the power callback holds the suspend off while a
 * game runs with the save on; the pause screen writes Last and lets the
 * PSP sleep. On wake it opens on Last and says why the game waits. */
volatile int ui_power_locked;
volatile int ui_woke;

static int pause_cheats_only;  /* ui_pause_cheats(): the Cheats page, no pause screen */

u32 ui_pause_cheats(u16 *game_picture)
{
  u32 r;
  pause_cheats_only = 1;
  r = ui_pause(game_picture);
  pause_cheats_only = 0;
  return r;
}

u32 ui_pause(u16 *game_picture)
{
  u32 mark, i;
  int leave = LEAVE_NONE, opened = 0;

  ui_woke = 0;  /* a wake from before this pause is old news */
  if(ui_power_locked)
  {
    if(save_last_state(game_picture))
      g_game_config.last_slot = LAST_SLOT;
    scePowerUnlock(0);
    ui_power_locked = 0;
  }

  memset(&p, 0, sizeof(p));
  p.game = game_picture;
  p.big_slot = -1;
  p.toast_f = -1;
  shot_fd = -1;
  shot_state = 0;
  shot_reading = -1;
  shot_failed = -1;
  /* It opens on the slot last saved or loaded in this game (slot 0 at
     first); L jumps to Last, R to slot 0 (maintainer, 2026-11-15). */
  p.cur = strip_position((g_game_config.last_slot < SLOTS) ? g_game_config.last_slot : 0);
  p.pos_f = UI_F_SCROLL;
  p.tile_f = UI_F_CURSOR;

  pause_sound(1);
  set_cpu_clock(10);  /* menus run at 222 MHz */
  video_resolution_keep(FRAME_MENU);  /* the game stays on the LCD meanwhile */

  mark = ui_pool_mark();
  p.thumbs = ui_pool_alloc(SLOTS * THUMB_PITCH * SMALL_H * 2);
  p.big = ui_pool_alloc(BIG_W * BIG_H * 2);
  p.chunk = ui_pool_alloc(CHUNK_BYTES);
  if(pause_cheats_only)
  {
    /* straight to Cheats; ○ there goes back to the game, through the
       same resume fade as the pause screen (maintainer, 2026-11-20) */
    ui_status_read();
    ui_input_init();
    if(ui_cheats())
      action_restart_game();  /* a cheat that needs it: Restart now */
    leave = LEAVE_RESUME;
  }
  else
  {
    for(i = 0; i < SLOTS; i++)
      read_slot(i);
  }
  /* the big picture is sharp from the first frame: it loads while the game
     is still on the LCD */
  if(!pause_cheats_only)
  {
    load_big_picture_now();
    ui_status_read();  /* while the game is still on the LCD */
    build_layer(0);
    ui_input_init();
    if(ui_restart_note.pending)
      restart_return();
  }
#ifdef UI_PERF
  /* the script runs on the first pause only; later pauses take the pad,
     so the game can still be left with Exit */
  if(!perf_done)
  {
    perf_phase = perf_f = perf_n = perf_on = 0;
    ui_input_script = perf_script;
  }
#endif

  while(leave == LEAVE_NONE)
  {
    ui_input_update();

    if(p.closing)
    {
      if(p.layer_full)
        build_layer(0);  /* the header and tiles fall away: drawn again */
      if(++p.close_f > F_CLOSE)
        leave = LEAVE_RESUME;
    }
    else if(!opened)
    {
      if(++p.open_f >= F_OPEN)
      {
        opened = 1;
        build_layer(1);
      }
    }

    if(ui_woke)
    {
      ui_woke = 0;
      show_toast("Paused while the PSP slept", UI_COL_SKY, UNDO_NONE, 0);
    }

    if(opened && !p.closing)
    {
      int resume = ui_pressed(PSP_CTRL_TRIANGLE | PSP_CTRL_HOME) || ui_pressed(ui_key_back);

      if(resume)
        p.closing = 1;
      else if(p.row == 0)
      {
        step_slot(ui_step(p.cur, SLOTS, PSP_CTRL_LEFT, PSP_CTRL_RIGHT));
        if(ui_pressed(PSP_CTRL_LTRIGGER)) move_slot(strip_position(LAST_SLOT));
        if(ui_pressed(PSP_CTRL_RTRIGGER)) move_slot(strip_position(0));
        if(ui_pressed(PSP_CTRL_DOWN))
        {
          p.row = 1;
          end_undo();
        }

        /* confirm: save into an empty slot, load a full one; with the
           save off a load needs a hold, and a tap does nothing */
        if(p.has[cur_slot()] && !g_gpsp_config.save_on_leave)
        {
          ui_hold_step(&p.load_hold, ui_key_confirm);
          if(p.load_hold.done)
          {
            p.load_hold.p = 0;
            p.load_hold.shown = 0;
            if(action_load(cur_slot()) < 0)
              leave = LEAVE_SWITCH;
          }
        }
        else
        {
          p.load_hold.p = 0;
          p.load_hold.shown = 0;
          if(ui_pressed(ui_key_confirm))
          {
            if(p.has[cur_slot()])
            {
              if(action_load(cur_slot()) < 0)
                leave = LEAVE_SWITCH;
            }
            else
              action_save(cur_slot());
          }
        }

        /* SELECT: Undo while the toast offers it, else save; hold: delete.
           Where it can hold, only a release within 12 f is a tap: a longer
           press is a hold, and letting go early does nothing. */
        {
          int can_hold = p.has[cur_slot()] && !(undo_offered());
          int tap;

          if(can_hold)
          {
            ui_hold_step(&p.hold, PSP_CTRL_SELECT);
            if(p.hold.done)
            {
              p.hold.p = 0;
              p.hold.shown = 0;
              action_delete(cur_slot());
            }
            tap = ui_tap(PSP_CTRL_SELECT);
          }
          else
          {
            p.hold.p = 0;
            p.hold.shown = 0;
            /* no hold here: any release is a tap, except the one ending
               the hold that just deleted this slot */
            tap = ui_released(PSP_CTRL_SELECT) && !p.hold.fired;
            if(!ui_down(PSP_CTRL_SELECT))
              p.hold.fired = 0;
          }
          if(tap)
          {
            if(undo_offered())
              action_undo();
            else
              action_save(cur_slot());
          }
        }
      }
      else
      {
        move_tile(ui_step(p.tile, TILE_COUNT, PSP_CTRL_LEFT, PSP_CTRL_RIGHT));
        if(ui_pressed(PSP_CTRL_UP))
        {
          p.row = 0;
          end_undo();
        }
        /* L and R go up to the strip, onto Last or slot 0 */
        if(ui_pressed(PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER))
        {
          p.row = 0;
          move_slot(strip_position(ui_pressed(PSP_CTRL_LTRIGGER) ? LAST_SLOT : 0));
        }
        if(ui_released(PSP_CTRL_SELECT) && undo_offered())
          action_undo();

        /* with the save off, Exit, Restart and Library are a hold */
        if(tile_needs_hold(p.tile))
          ui_hold_step(&p.exit_hold, ui_key_confirm);
        else
          memset(&p.exit_hold, 0, sizeof(p.exit_hold));
        if(tile_needs_hold(p.tile) ? p.exit_hold.done : ui_pressed(ui_key_confirm))
        {
          switch(p.tile)
          {
            case TILE_SETTINGS:
              /* Settings draws its own ground; the pause screen's comes
                 back after it */
              shot_wait();
              ui_settings_game = p.game;
              switch(ui_settings(0, 0))
              {
                case 1: restart_kai(); break;  /* do not return */
                case 2: quit_kai(); break;
              }
              build_layer(1);
              ui_status_read();
              ui_input_init();
              break;
            case TILE_CHEATS:
              /* Cheats draws its own ground; the pause screen's comes back */
              shot_wait();
              if(ui_cheats())
              {
                /* a cheat that works from the game's start: Restart now */
                build_layer(1);
                action_restart_game();
                break;
              }
              build_layer(1);
              ui_status_read();
              ui_input_init();
              break;
            case TILE_LIBRARY:
              action_library();
              leave = LEAVE_LIBRARY;
              break;
            case TILE_SHOT:
              shot_wait();
              save_ss_bmp(p.game);
              show_toast("Screenshot saved", UI_COL_SKY, UNDO_NONE, 0);
              break;
            case TILE_RESTART:
              action_restart_game();
              break;
            case TILE_EXIT:
              if(action_exit())
                leave = LEAVE_QUIT;
              break;
          }
        }
      }
    }

    if(p.pos_f < UI_F_SCROLL) p.pos_f++;
    if(p.tile_f < UI_F_CURSOR) p.tile_f++;
#ifdef UI_PERF
    {
      static u32 last;
      u32 t0 = sceKernelGetSystemTimeLow(), t1, t2;

      if(opened && !perf_on && !perf_done)
      {
        perf_on = 1;
        last = t0;
      }
      load_big_picture();
      t1 = sceKernelGetSystemTimeLow();
      draw_screen(0);
      t2 = sceKernelGetSystemTimeLow();
      perf_step(t1 - t0, t2 - t1, t0 - last);
      last = t0;
    }
#else
    load_big_picture();

    draw_screen(0);
#endif
    if(p.toast_f >= 0)
      p.toast_f++;
    ui_frame_end();
  }

  shot_wait();
#ifdef UI_PERF
  ui_input_script = NULL;
  if(perf_on)
  {
    perf_write();
    perf_on = 0;
    perf_done = 1;
  }
#endif
  ui_layer_reset();
  ui_pool_release(mark);

  if(leave == LEAVE_QUIT)
    quit(0);
  if(leave == LEAVE_SWITCH)
  {
    char name[MAX_FILE];
    get_savestate_filename_noshot(p.switch_slot, name);
    if(ui_library_switch_to(state_error_game, name, p.switch_slot))
      return 1;
    /* that game is not in the ROM folder: back to this one */
    video_resolution_keep(FRAME_GAME);
    set_cpu_clock(g_game_config.clock_speed_number);
    pause_sound(0);
    return p.changed;
  }
  if(leave == LEAVE_LIBRARY)
  {
    if(ui_library_switch())
      return 1;
    /* no pool memory for the library: the old menu's file list */
    leave = LEAVE_OLD_MENU;
  }
  if(leave == LEAVE_OLD_MENU)
    return menu(p.game) | p.changed;

  /* The last frame is the game where it will be shown, still doored, on
     black: no black frames on the way back. The game then comes back
     multiplied from 50 % to 100 % over UI_F_RESUME_FADE frames, so the dim
     lifts instead of vanishing in one frame. */
  {
    int x, y, w, h;
    ui_frame_begin();
    ui_fill(0, 0, UI_SCREEN_W, UI_SCREEN_H, COLOR16(0, 0, 0));
    get_game_rect(&x, &y, &w, &h);
    ui_blit_scaled(p.game, 240, 160, x, y, w, h);
    ui_door(x, y, w, h);
    ui_frame_end();
  }
  video_resolution_keep(FRAME_GAME);
  osd_fade_start();
  if(p.game_toast[0])
    ui_game_toast(p.game_toast, UI_COL_SKY);
  set_cpu_clock(g_game_config.clock_speed_number);
  pause_sound(0);
  real_frame_count = 0;
  virtual_frame_count = 0;
  return p.changed;
}
