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
#define SMALL_W   64
#define SMALL_H   43
#define STRIP_CX  240
#define TILE_W    60
#define TILE_H    62
#define TILE_Y    176         /* 10 px above the hint strip */
#define F_OPEN    UI_F_PAUSE_OPEN
#define F_CLOSE   UI_F_PAUSE_CLOSE

/* 20 x 20 two-tone tile icons, from gpsp-kai-design app-ui/scripts/icons20.py:
 * per row, ink then fill, bit 19 the left pixel. */
static const u32 icon20_play[2][20] =
{
  { 0x00000, 0x00000, 0x00000, 0x0C000, 0x0B000, 0x08C00, 0x08300, 0x080C0, 0x08030, 0x0800C,
    0x0800C, 0x08030, 0x080C0, 0x08300, 0x08C00, 0x0B000, 0x0C000, 0x00000, 0x00000, 0x00000 },
  { 0x00000, 0x00000, 0x00000, 0x00000, 0x04000, 0x07000, 0x07C00, 0x07F00, 0x07FC0, 0x07FF0,
    0x07FF0, 0x07FC0, 0x07F00, 0x07C00, 0x07000, 0x04000, 0x00000, 0x00000, 0x00000, 0x00000 }
};
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
static const u16 icon_battery[11] = { 0x000, 0x000, 0x3FE, 0x203, 0x203, 0x203, 0x3FE, 0x000, 0x000, 0x000, 0x000 };

enum { TILE_RESUME, TILE_SETTINGS, TILE_CHEATS, TILE_SHOT, TILE_RESTART, TILE_LIBRARY,
       TILE_EXIT, TILE_COUNT };

typedef struct
{
  const char *label;
  const u32 (*icon)[20];
  u16 color;
} tile_t;

static const tile_t tiles[TILE_COUNT] =
{
  { "Resume",     icon20_play,    UI_COL_LIME },
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
  u16 *thumbs;               /* SLOTS x 64x43, in the UI pool */
  u16 *shot;                 /* 240x160 of the selected slot, in the UI pool */
  int shot_slot;
  int cur, pos, pos_from, pos_f;
  int row, tile, tile_from, tile_f;
  int open_f, close_f, closing;
  char toast[64];
  u16 toast_edge;
  int toast_f, toast_undo;
  int undo_ended;            /* the cursor moved: the toast leaves, no Undo */
  int undo, undo_slot;
  ui_hold_t hold;
  ui_hold_t load_hold;       /* hold confirm: load with the save off */
  ui_hold_t exit_hold;
  int exit_holding;
  const char *bar_text;      /* the saving card while a save runs */
  const char *bar_note;
  int bar_phase, bar_phases; /* a load after a save fills one bar */
  char game_toast[64];       /* said over the game after a load */
  int changed;               /* the machine state changed (load, restart) */
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
  return p.thumbs + slot * SMALL_W * SMALL_H;
}

/* the big picture's read, a slice a frame (load_big_picture) */
static SceUID shot_fd = -1;
static int shot_state, shot_reading = -1, shot_failed = -1;

static void read_slot(int slot)
{
  p.has[slot] = read_state_time(slot, &p.time[slot]);
  p.thumb_ok[slot] = p.has[slot] && p.thumbs &&
   read_state_thumbnail(slot, p.time[slot], thumb(slot));
  p.thumb_tried[slot] = 0;
  if(p.shot_slot == slot)
    p.shot_slot = -1;
  if(shot_failed == slot)
    shot_failed = -1;  /* the file changed: try again */
}

/* The big picture: the full screenshot of the selected slot. Once the
 * cursor moves it is read with the PSP's asynchronous file calls (open,
 * then the 8-byte head and the picture), polled once a frame, so the frame
 * never waits for the Memory Stick and the picture is sharp by the time
 * the strip stops; the 64 x 43 picture stands in meanwhile. A read the
 * cursor has left is finished and dropped. Every other file action waits
 * for it first (shot_wait). A savestate without its .thm (from an older
 * kai) is read at once, which makes the .thm. */
#define SHOT_BYTES (240 * 160 * 2)
enum { SHOT_IDLE, SHOT_OPENING, SHOT_HEAD, SHOT_PIXELS };

static u8 shot_head[8] __attribute__((aligned(64)));

/* Wait for the asynchronous call in flight, then close. */
static void shot_wait(void)
{
  SceInt64 res;

  if(shot_state == SHOT_IDLE)
    return;
  if(shot_fd >= 0)
  {
    sceIoWaitAsync(shot_fd, &res);
    if((shot_state != SHOT_OPENING) || (res >= 0))  /* a failed open has nothing to close */
      sceIoClose(shot_fd);
  }
  shot_fd = -1;
  shot_state = SHOT_IDLE;
  shot_reading = -1;
}

/* The whole picture now (opening, and savestates without a .thm). */
static void load_big_picture_now(void)
{
  int slot = cur_slot(), make_thm = !p.thumb_ok[slot] && !p.thumb_tried[slot];

  shot_wait();
  if((p.shot == NULL) || !p.has[slot] || (p.shot_slot == slot))
    return;
  p.shot_slot = -1;
  if(read_state_shot(slot, p.shot, make_thm))
    p.shot_slot = slot;
  else
    shot_failed = slot;
  if(make_thm)
  {
    p.thumb_tried[slot] = 1;
    p.thumb_ok[slot] = p.thumbs && read_state_thumbnail(slot, p.time[slot], thumb(slot));
  }
}

static void shot_fail(int slot)
{
  shot_wait();
  shot_failed = slot;
}

/* Once a frame: start, advance or finish the read of the selected slot. */
static void load_big_picture(void)
{
  int slot = cur_slot();
  SceInt64 res;

  if(shot_state != SHOT_IDLE)
  {
    if(sceIoPollAsync(shot_fd, &res) == 1)
      return;  /* still busy */
    if(shot_reading != slot)
    {
      /* the cursor moved on: this call is done, so close and start over */
      if((shot_state != SHOT_OPENING) || (res >= 0))
        sceIoClose(shot_fd);
      shot_fd = -1;
      shot_state = SHOT_IDLE;
      shot_reading = -1;
    }
    else
    {
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
          shot_state = SHOT_PIXELS;
          p.shot_slot = -1;  /* p.shot is being written */
          sceIoReadAsync(shot_fd, p.shot, SHOT_BYTES);
          return;
        case SHOT_PIXELS:
          if(res != SHOT_BYTES)
          {
            shot_fail(slot);
            return;
          }
          sceIoClose(shot_fd);
          shot_fd = -1;
          shot_state = SHOT_IDLE;
          shot_reading = -1;
          p.shot_slot = slot;
          return;
      }
    }
  }

  if((p.shot == NULL) || !p.has[slot] || (p.shot_slot == slot) || (shot_failed == slot))
    return;
  if(!p.thumb_ok[slot] && !p.thumb_tried[slot])
  {
    load_big_picture_now();
    return;
  }
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

static void draw_icon1x(int x, int y, const u16 *bits, u16 color)
{
  int i, j;

  for(j = 0; j < 11; j++)
    for(i = 0; i < 11; i++)
      if(bits[j] & (0x400 >> i))
        ui_fill(x + i, y + j, 1, 1, color);
}

/* The static ground: the game picture as it was on the LCD, screen-doored. */
static void build_layer(void)
{
  int x, y, w, h;

  if(!ui_layer_begin())
    return;
  ui_fill(0, 0, UI_SCREEN_W, UI_SCREEN_H, COLOR16(0, 0, 0));
  get_game_rect(&x, &y, &w, &h);
  ui_blit_scaled(p.game, 240, 160, x, y, w, h);
  ui_door(0, 0, UI_SCREEN_W, UI_SCREEN_H);
  ui_layer_end();
}

static void draw_header(int hy)
{
  char title[MAX_FILE];
  char *dot;
  int tw, pw = ui_text_width("Paused");

  strcpy(title, gamepak_filename);
  dot = strrchr(title, '.');
  if(dot)
    *dot = 0;
  tw = ui_print_width(title, UI_FONT_BODY, UI_TEXT_BOLD);
  if(tw > 300)
    tw = 300;
  ui_round_fill(14, hy, tw + pw + 34, 24, 3, UI_COL_PLUM);
  ui_print_fit(22, hy + 4, title, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD, 300);
  ui_text(30 + tw, hy + 6, "Paused", UI_COL_MUTE);
}

/* The status chip, top right and as wide as its content: the clock (the
 * PSP's 12/24-hour setting), then the battery: lime, tangerine at 20 % or
 * less, pink with the % at 10 % or less, sky with a bolt while charging. */
static const u8 bolt[7] = { 0x03, 0x06, 0x0C, 0x1F, 0x06, 0x0C, 0x18 };

static void draw_status(void)
{
  ScePspDateTime now;
  char clock[16], pct[8];
  int level = scePowerGetBatteryLifePercent(), format = 0, battery, charging;
  int w, x, tx, i, j, critical = 0;
  u16 color = UI_COL_LIME;

  sceRtcGetCurrentClockLocalTime(&now);
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_TIME_FORMAT, &format);
  if(format == PSP_SYSTEMPARAM_TIME_FORMAT_12HR)
    sprintf(clock, "%d:%02d %s", (now.hour + 11) % 12 + 1, now.minute, (now.hour < 12) ? "AM" : "PM");
  else
    sprintf(clock, "%02d:%02d", now.hour, now.minute);

  battery = scePowerIsBatteryExist() > 0;
  charging = battery && (scePowerIsBatteryCharging() > 0);
  if(level < 0)
    level = 0;
  if(level > 100)
    level = 100;
  if(charging)
    color = UI_COL_SKY;
  else if(level <= 10)
  {
    color = UI_COL_PINK;
    critical = battery;
  }
  else if(level <= 20)
    color = UI_COL_TANGERINE;
  sprintf(pct, "%d%%", level);

  w = 6 + ui_text_width(clock) + 4 + (critical ? ui_text_width(pct) + 4 : 0) +
   (charging ? 7 : 0) + (battery ? 11 + 5 : 2);
  x = 460 - w;
  ui_round_fill(x, 8, w, 16, 2, UI_COL_CARD);
  ui_text(x + 6, 9, clock, UI_COL_CREAM);
  if(!battery)
    return;
  tx = x + 6 + ui_text_width(clock) + 4;
  if(critical)
  {
    ui_text(tx, 9, pct, UI_COL_PINK);
    tx += ui_text_width(pct) + 4;
  }
  if(charging)
  {
    for(j = 0; j < 7; j++)
      for(i = 0; i < 5; i++)
        if(bolt[j] & (0x10 >> i))
          ui_fill(tx + i, 12 + j, 1, 1, UI_COL_SKY);
    tx += 7;
  }
  /* the bitmap's first column is empty: one to the left */
  draw_icon1x(tx - 1, 11, icon_battery, color);
  ui_fill(tx + 1, 14, 7 * level / 100, 3, color);
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
    y = 72 + dy;
    ui_round_fill(x - 2, y - 2, w + 4, h + 4, 2, UI_COL_CARD);
  }

  if(p.has[slot])
  {
    if(big && (p.shot_slot == slot))
      ui_blit_scaled(p.shot, 240, 160, x, y, w, h);
    else if(p.thumb_ok[slot])
      ui_blit_scaled(thumb(slot), SMALL_W, SMALL_H, x, y, w, h);
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
    const char *verb = p.hold.shown ? "Delete (hold)" : "Load (hold)";
    int cw = ui_text_width((char *)verb) + 20, cx, cy;

    if(cw < 96)
      cw = 96;
    cx = x + (w - cw) / 2;
    cy = y + (h - 34) / 2;
    ui_checker(x, y, w, h, UI_COL_SHADE);
    ui_round_fill(cx + 2, cy + 2, cw, 34, 2, UI_COL_SHADE);
    ui_round_fill(cx, cy, cw, 34, 2, UI_COL_CARD);
    ui_text(cx + (cw - ui_text_width((char *)verb)) / 2, cy + 3, (char *)verb, UI_COL_CREAM);
    ui_hold_bar(cx + 8, cy + 22, cw - 16, hd->p, p.hold.shown ? UI_COL_PINK : UI_COL_LIME);
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
  int tx = 18 + i * 64, lift = selected ? UI_LIFT : 0, j, lines = 0;
  char line[2][64];

  if(selected)
    ui_round_fill(tx + 1, ty + 1, TILE_W, TILE_H, 2, UI_COL_SHADE);
  ui_round_fill(tx - lift, ty - lift, TILE_W, TILE_H, 2, selected ? tiles[i].color : UI_COL_CARD);
  tx -= lift;
  ty -= lift;
  ui_round_fill(tx + 16, ty + 6, 28, 28, 2, selected ? UI_COL_CREAM : tiles[i].color);
  draw_icon20(tx + 19, ty + 9, tiles[i].icon, selected ? tiles[i].color : UI_COL_CREAM);
  for(j = 0; j < 2; j++)
    if(ui_wrap_line(tiles[i].label, j, 56, UI_FONT_SMALL, line[j], sizeof(line[j])))
      lines++;
  for(j = 0; j < lines; j++)
    ui_text(tx + 30 - ui_text_width(line[j]) / 2, ty + 36 + j * 12 - ((lines == 2) ? 6 : 0),
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

static void draw_hints(int done)
{
  ui_hint_t c1[4], c2[3], c3[1], c4[1], c5[1];
  ui_hint_col_t cols[5];
  u32 resume_keys = PSP_CTRL_TRIANGLE | (g_use_home ? PSP_CTRL_HOME : 0);
  int i, full = p.has[cur_slot()];

  if(p.bar_text || !done || p.closing)
  {
    ui_hint_strip(c1, 0);  /* nothing to press while saving */
    return;
  }
  if(p.exit_holding)
  {
    c1[0].keys = ui_key_confirm; c1[0].verb = "Hold: quit";
    c1[1].keys = ui_key_back;    c1[1].verb = "Back";
    ui_hint_strip(c1, 2);
    return;
  }

  /* fixed columns, each as wide as its widest variant, so a changing verb
     never moves the others (spec section 2, hint strip) */
  for(i = 0; i < 4; i++)
    c1[i].keys = ui_key_confirm;
  c1[0].verb = "Save";
  c1[1].verb = "Load";
  c1[2].verb = "Load (hold)";
  c1[3].verb = "Open";
  c2[0].keys = PSP_CTRL_SELECT; c2[0].verb = "Save \xC2\xB7 hold: Delete";
  c2[1].keys = PSP_CTRL_SELECT; c2[1].verb = "Undo";
  c2[2].keys = PSP_CTRL_UP;     c2[2].verb = "Savestates";
  c3[0].keys = resume_keys;       c3[0].verb = "Resume";
  c4[0].keys = PSP_CTRL_LTRIGGER; c4[0].verb = "Last";
  c5[0].keys = PSP_CTRL_RTRIGGER; c5[0].verb = "Slot 0";

  cols[0].variants = c1; cols[0].count = 4;
  cols[1].variants = c2; cols[1].count = 3;
  cols[2].variants = c3; cols[2].count = 1; cols[2].shown = 0;
  cols[3].variants = c4; cols[3].count = 1;
  cols[4].variants = c5; cols[4].count = 1;
  if(p.row == 0)
  {
    cols[0].shown = !full ? 0 : (g_gpsp_config.save_on_leave ? 1 : 2);
    cols[1].shown = (undo_offered()) ? 1 : 0;
    cols[3].shown = 0;
    cols[4].shown = 0;
  }
  else
  {
    cols[0].shown = 3;
    cols[1].shown = 2;
    cols[3].shown = -1;  /* L and R columns stay empty on the tile row */
    cols[4].shown = -1;
  }
  ui_hint_columns(cols, 5);
}

/* The saving card: a title, 30 blocks and a note (spec "exit"). */
static void draw_bar_card(int fill_256)
{
  int w = 332, h = 76, x = (UI_SCREEN_W - w) / 2, y = 98, k = (30 * fill_256 + 128) / 256 - 1;

  /* no dim behind it: a second door over the screen-doored game reads as
     noise (maintainer, 2026-11-15) */
  ui_round_fill(x + 4, y + 4, w, h, 3, UI_COL_SHADE);
  ui_round_fill(x, y, w, h, 3, UI_COL_CARD);
  ui_fill(x, y + 4, 4, h - 8, UI_COL_PINK);
  ui_print(x + 16, y + 10, p.bar_text, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD);
  {
    int i;
    for(i = 0; i < 30; i++)
      ui_fill(x + 16 + i * 10, y + 32, 8, 12, (i <= k) ? UI_COL_PINK : UI_COL_PLUM_2);
  }
  if(p.bar_note)
    ui_print_fit(x + 16, y + 52, p.bar_note, UI_COL_MUTE, UI_FONT_SMALL, 0, w - 28);
}

/* Exit with "Save when leaving a game" off: hold confirm to quit. */
static void draw_exit_hold(void)
{
  int w = 312, x = 462 - w, y = 100, gx;

  ui_round_fill(x + 3, y + 3, w, 56, 3, UI_COL_SHADE);
  ui_round_fill(x, y, w, 56, 3, UI_COL_CARD);
  ui_fill(x, y + 4, 4, 48, UI_COL_PINK);
  gx = ui_glyph(x + 14, y + 9, ui_key_confirm) + 6;
  ui_print_fit(gx, y + 7, "Hold to quit without saving", UI_COL_CREAM, UI_FONT_BODY, 0, x + w - 10 - gx);
  ui_hold_bar(x + 14, y + 28, w - 28, p.exit_hold.p, UI_COL_PINK);
  ui_print_fit(x + 14, y + 38, "'Save when leaving a game' is off.", UI_COL_MUTE, UI_FONT_SMALL, 0, w - 28);
  ui_round_fill(462 - 18, y + 56, 10, 6, 1, UI_COL_CARD);  /* tail toward the Exit tile */
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
    ui_fill(0, 0, UI_SCREEN_W, UI_SCREEN_H, COLOR16(0, 0, 0));
    get_game_rect(&x, &y, &w, &h);
    ui_blit_scaled(p.game, 240, 160, x, y, w, h);
    ui_door(0, 0, UI_SCREEN_W, UI_SCREEN_H);
  }

  motion(&hy, &dy, tile_dy, &done);
  draw_header(hy);
  if(done && !p.closing)
    draw_status();
  PERF_SECTION(1);

  /* the strip: the selected slot big, its neighbours small */
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
      ui_round_fill(STRIP_CX - iw / 2 - 6, 152, iw + 12, 15, 2, UI_COL_PLUM);
      ui_text(STRIP_CX - iw / 2, 153, info, UI_COL_CREAM);
    }
  }
  PERF_SECTION(3);

  for(i = 0; i < TILE_COUNT; i++)
    if(tile_dy[i] < 999)
      draw_tile(i, TILE_Y + tile_dy[i], (p.row == 1) && (i == p.tile) && done);
  if((p.row == 1) && done && !p.closing && !p.bar_text)
  {
    int tx = (p.tile_f < UI_F_CURSOR) ?
     ui_ease_out_back(p.tile_from, 18 + p.tile * 64, p.tile_f, UI_F_CURSOR) : 18 + p.tile * 64;
    ui_cursor_frame(tx - 2, TILE_Y - 2, TILE_W, TILE_H);
  }
  PERF_SECTION(4);

  if(p.exit_holding)
    draw_exit_hold();
  if(p.toast_f >= 0)
  {
    if(!ui_toast(p.toast, p.toast_edge, p.toast_undo ? "Undo" : NULL, p.toast_f))
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
  draw_screen(fill);
  ui_frame_end_now();
}

static void run_with_bar(const char *text, const char *note)
{
  p.bar_text = text;
  p.bar_note = note;
  p.bar_phase = -1;
  p.bar_phases = 1;
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
  build_layer();
}

static void action_save(int slot)
{
  char name[MAX_FILE], text[48];
  int had = p.has[slot];

  shot_wait();  /* the screenshot reader shares p.shot and the Memory Stick */
  get_savestate_filename_noshot(slot, name);
  slot_name(slot, text);
  run_with_bar("Saving\xE2\x80\xA6", NULL);
  save_state(name, p.game, slot);
  end_bar();
  read_slot(slot);
  if(p.has[slot])
  {
    char msg_text[64];
    /* the big picture is the screenshot just saved: no need to read it */
    if(p.shot)
    {
      memcpy(p.shot, p.game, 240 * 160 * 2);
      p.shot_slot = slot;
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
  run_with_bar(bar, NULL);
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
  if(keep)
    read_slot(LAST_SLOT);

  if(!ok)
  {
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

static int action_exit(void)
{
  shot_wait();
  if(!g_gpsp_config.save_on_leave)
  {
    p.exit_holding = 1;
    memset(&p.exit_hold, 0, sizeof(p.exit_hold));
    return 0;
  }
  run_with_bar("Saving to Last\xE2\x80\xA6", "Then kai closes. Continue picks up here.");
  save_last_state(p.game);
  end_bar();
  return 1;
}

/* ---- the screen ------------------------------------------------------------ */

static void move_slot(int to)
{
  if((to < 0) || (to >= SLOTS) || (to == p.cur))
    return;
  end_undo();
  p.pos_from = (p.pos_f < UI_F_SCROLL) ?
   ui_ease_out_back(p.pos_from, p.cur * 256, p.pos_f, UI_F_SCROLL) : p.cur * 256;
  p.cur = to;
  p.pos_f = 0;
  p.hold.p = 0;
  p.hold.shown = 0;
  p.load_hold.p = 0;
  p.load_hold.shown = 0;
}

static void move_tile(int to)
{
  if(to == p.tile)
    return;
  end_undo();
  p.tile_from = (p.tile_f < UI_F_CURSOR) ?
   ui_ease_out_back(p.tile_from, 18 + p.tile * 64, p.tile_f, UI_F_CURSOR) : 18 + p.tile * 64;
  p.tile = to;
  p.tile_f = 0;
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
static int perf_phase, perf_f, perf_n, perf_on;

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

enum { LEAVE_NONE, LEAVE_RESUME, LEAVE_OLD_MENU, LEAVE_QUIT };

u32 ui_pause(u16 *game_picture)
{
  u32 mark, i;
  int leave = LEAVE_NONE, opened = 0;

  memset(&p, 0, sizeof(p));
  p.game = game_picture;
  p.shot_slot = -1;
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
  p.thumbs = ui_pool_alloc(SLOTS * SMALL_W * SMALL_H * 2);
  p.shot = ui_pool_alloc(240 * 160 * 2);
  for(i = 0; i < SLOTS; i++)
    read_slot(i);
  /* the big picture is sharp from the first frame: it loads while the game
     is still on the LCD */
  load_big_picture_now();
  build_layer();
  ui_input_init();
#ifdef UI_PERF
  perf_phase = perf_f = perf_n = perf_on = 0;
  ui_input_script = perf_script;
#endif

  while(leave == LEAVE_NONE)
  {
    ui_input_update();

    if(p.closing)
    {
      if(++p.close_f > F_CLOSE)
        leave = LEAVE_RESUME;
    }
    else if(!opened)
    {
      if(++p.open_f >= F_OPEN)
        opened = 1;
    }

    if(opened && !p.closing)
    {
      int resume = ui_pressed(PSP_CTRL_TRIANGLE | PSP_CTRL_HOME) || ui_pressed(ui_key_back);

      if(p.exit_holding)
      {
        ui_hold_step(&p.exit_hold, ui_key_confirm);
        if(p.exit_hold.done)
          leave = LEAVE_QUIT;
        else if(ui_pressed(ui_key_back) || ui_pressed(PSP_CTRL_UP | PSP_CTRL_LEFT | PSP_CTRL_RIGHT))
          p.exit_holding = 0;
      }
      else if(resume)
        p.closing = 1;
      else if(p.row == 0)
      {
        move_slot(ui_step(p.cur, SLOTS, PSP_CTRL_LEFT, PSP_CTRL_RIGHT));
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
            action_load(cur_slot());
          }
        }
        else
        {
          p.load_hold.p = 0;
          p.load_hold.shown = 0;
          if(ui_pressed(ui_key_confirm))
          {
            if(p.has[cur_slot()])
              action_load(cur_slot());
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

        if(ui_pressed(ui_key_confirm))
        {
          switch(p.tile)
          {
            case TILE_RESUME:
              p.closing = 1;
              break;
            case TILE_SETTINGS:
            case TILE_CHEATS:
            case TILE_LIBRARY:
              leave = LEAVE_OLD_MENU;
              break;
            case TILE_SHOT:
              shot_wait();
              save_ss_bmp(p.game);
              show_toast("Screenshot saved", UI_COL_SKY, UNDO_NONE, 0);
              break;
            case TILE_RESTART:
              reset_gba();
              reg[CHANGED_PC_STATUS] = 1;
              p.changed = 1;
              p.closing = 1;
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

      if(opened && !perf_on)
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
    perf_write();
#endif
  ui_layer_reset();
  ui_pool_release(mark);

  if(leave == LEAVE_QUIT)
    quit(0);
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
