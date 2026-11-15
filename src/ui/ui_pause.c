/* The 4.0 pause screen (spec section 4, "Pause"; step 3). The menu button
 * opens it over the screen-doored game: the savestate strip on top (slots
 * 0-9 and Last as pictures), tiles below. SELECT saves at once, hold
 * SELECT deletes, confirm loads, and a toast offers Undo for each (load
 * only while "Save when leaving a game" is on). Exit saves to Last first.
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
#define TILE_Y    166
#define F_OPEN    UI_F_PAUSE_OPEN
#define F_CLOSE   UI_F_PAUSE_CLOSE
#define SHOT_REST 4           /* frames the cursor rests before the big picture loads */

static const u16 icon_play[11]    = { 0x000, 0x100, 0x180, 0x1C0, 0x1E0, 0x1F0, 0x1E0, 0x1C0, 0x180, 0x100, 0x000 };
static const u16 icon_gear[11]    = { 0x020, 0x174, 0x0F8, 0x38E, 0x18C, 0x707, 0x18C, 0x38E, 0x0F8, 0x174, 0x020 };
static const u16 icon_cheats[11]  = { 0x020, 0x020, 0x070, 0x7DF, 0x202, 0x104, 0x124, 0x252, 0x28A, 0x306, 0x000 };
static const u16 icon_camera[11]  = { 0x000, 0x0E0, 0x3FE, 0x202, 0x272, 0x28A, 0x28A, 0x272, 0x3FE, 0x000, 0x000 };
static const u16 icon_restart[11] = { 0x000, 0x078, 0x084, 0x102, 0x207, 0x202, 0x200, 0x102, 0x084, 0x078, 0x000 };
static const u16 icon_folder[11]  = { 0x000, 0x3C0, 0x27C, 0x204, 0x3FE, 0x202, 0x202, 0x202, 0x3FE, 0x000, 0x000 };
static const u16 icon_exit[11]    = { 0x020, 0x124, 0x222, 0x421, 0x401, 0x401, 0x202, 0x104, 0x0F8, 0x000, 0x000 };
static const u16 icon_battery[11] = { 0x000, 0x000, 0x3FE, 0x203, 0x203, 0x203, 0x3FE, 0x000, 0x000, 0x000, 0x000 };

enum { TILE_RESUME, TILE_SETTINGS, TILE_CHEATS, TILE_SHOT, TILE_RESTART, TILE_LIBRARY,
       TILE_EXIT, TILE_COUNT };

typedef struct
{
  const char *label;
  const u16 *icon;
  u16 color;
} tile_t;

static const tile_t tiles[TILE_COUNT] =
{
  { "Resume",     icon_play,    UI_COL_LIME },
  { "Settings",   icon_gear,    UI_COL_LIME },
  { "Cheats",     icon_cheats,  UI_COL_LIME },
  { "Screenshot", icon_camera,  UI_COL_SKY },
  { "Restart",    icon_restart, UI_COL_TANGERINE },
  { "Library",    icon_folder,  UI_COL_TANGERINE },
  { "Exit",       icon_exit,    UI_COL_PINK },
};

enum { UNDO_NONE, UNDO_SAVE, UNDO_SAVE_NEW, UNDO_DELETE, UNDO_LOAD };

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
  int cur, pos, pos_from, pos_f, rest;
  int row, tile, tile_from, tile_f;
  int open_f, close_f, closing;
  char toast[64];
  u16 toast_edge;
  int toast_f, toast_undo;
  int undo, undo_slot;
  ui_hold_t hold;
  ui_hold_t exit_hold;
  int exit_holding;
  const char *bar_text;      /* the saving card while a save runs */
  const char *bar_note;
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

static void read_slot(int slot)
{
  p.has[slot] = read_state_time(slot, &p.time[slot]);
  p.thumb_ok[slot] = p.has[slot] && p.thumbs &&
   read_state_thumbnail(slot, p.time[slot], thumb(slot));
  p.thumb_tried[slot] = 0;
  if(p.shot_slot == slot)
    p.shot_slot = -1;
  if(slot == cur_slot())
    p.rest = 0;  /* the big picture loads again once the cursor rests */
}

/* A savestate without its .thm (from before them, or saved by an older
 * kai) gets one the first time the cursor rests on it. The full
 * screenshot is read for the big picture anyway. */
static void load_big_picture(void)
{
  int slot = cur_slot();

  if((p.shot == NULL) || !p.has[slot] || (p.shot_slot == slot))
    return;
  if(read_state_shot(slot, p.shot, !p.thumb_ok[slot] && !p.thumb_tried[slot]))
  {
    p.shot_slot = slot;
    if(!p.thumb_ok[slot] && !p.thumb_tried[slot])
    {
      p.thumb_tried[slot] = 1;
      p.thumb_ok[slot] = p.thumbs && read_state_thumbnail(slot, p.time[slot], thumb(slot));
    }
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

static void draw_icon2x(int x, int y, const u16 *bits, u16 color)
{
  int i, j;

  for(j = 0; j < 11; j++)
    for(i = 0; i < 11; i++)
      if(bits[j] & (0x400 >> i))
        ui_fill(x + i * 2, y + j * 2, 2, 2, color);
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

static void draw_status(void)
{
  ScePspDateTime now;
  char clock[16];
  int level = scePowerGetBatteryLifePercent();

  sceRtcGetCurrentClockLocalTime(&now);
  sprintf(clock, "%02d:%02d", now.hour, now.minute);
  ui_round_fill(UI_SCREEN_W - 20 - 66, 8, 66, 16, 2, UI_COL_CARD);
  ui_text(UI_SCREEN_W - 20 - 66 + 6, 9, clock, UI_COL_CREAM);
  if(scePowerIsBatteryExist() > 0)
  {
    int bx = UI_SCREEN_W - 20 - 18, by = 11;
    draw_icon1x(bx, by, icon_battery, UI_COL_LIME);
    if(level > 0)
      ui_fill(bx + 2, by + 3, 7 * (level > 100 ? 100 : level) / 100, 3, UI_COL_LIME);
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
    y = 40 + dy;
    ui_round_fill(x + 3, y + 3, w + 8, h + 8, 3, UI_COL_SHADE);
    ui_round_fill(x - 4, y - 4, w + 8, h + 8, 3, UI_COL_SKY);
  }
  else
  {
    int off = (ad >= 256) ? 116 + (ad - 256) * 76 / 256 : ad * 116 / 256;
    x = STRIP_CX + ((dd > 0) ? off : -off) - w / 2;
    y = 66 + dy;
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
      int gw = ui_glyph_width(PSP_CTRL_SELECT) + 4 + ui_print_width("Save", UI_FONT_BODY, 0);
      int gx = ui_glyph(x + (w - gw) / 2, y + h / 2 - 6, PSP_CTRL_SELECT) + 4;
      ui_print(gx, y + h / 2 - 8, "Save", UI_COL_CREAM, UI_FONT_BODY, 0);
    }
  }

  if(slot == LAST_SLOT)
    strcpy(label, "Last");
  else
    sprintf(label, "%d", slot);
  ui_round_fill(x + 2, y + h - 14, ui_text_width(label) + 8, 13, 2, UI_COL_PLUM);
  ui_text(x + 6, y + h - 15, label, UI_COL_CREAM);

  if(big && (p.hold.p > 0) && p.has[slot])  /* hold SELECT: delete */
    ui_fill(x, y + h + 1, w * p.hold.p / 256, 3, UI_COL_PINK);
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
  draw_icon2x(tx + 19, ty + 9, tiles[i].icon, UI_COL_PLUM);
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

static void draw_hints(int done)
{
  ui_hint_t hints[6];
  int n = 0;

  if(p.bar_text || !done || p.closing)
  {
    ui_hint_strip(hints, 0);  /* nothing to press while saving */
    return;
  }
  if(p.exit_holding)
  {
    hints[n].keys = ui_key_confirm; hints[n++].verb = "Hold: quit";
    hints[n].keys = ui_key_back;    hints[n++].verb = "Back";
  }
  else if(p.row == 0)
  {
    hints[n].keys = ui_key_confirm;  hints[n++].verb = "Load";
    hints[n].keys = PSP_CTRL_SELECT;
    hints[n++].verb = (ui_toast_up(p.toast_f) && p.toast_undo) ? "Undo" : "Save \xC2\xB7 hold: Delete";
    hints[n].keys = PSP_CTRL_TRIANGLE | (g_use_home ? PSP_CTRL_HOME : 0); hints[n++].verb = "Resume";
    hints[n].keys = PSP_CTRL_LTRIGGER; hints[n++].verb = "Last";
    hints[n].keys = PSP_CTRL_RTRIGGER; hints[n++].verb = "Slot 0";
  }
  else
  {
    hints[n].keys = ui_key_confirm; hints[n++].verb = "Open";
    hints[n].keys = PSP_CTRL_UP;    hints[n++].verb = "Savestates";
    hints[n].keys = PSP_CTRL_TRIANGLE | (g_use_home ? PSP_CTRL_HOME : 0); hints[n++].verb = "Resume";
  }
  ui_hint_strip(hints, n);
}

/* The saving card: a title, 30 blocks and a note (spec "exit"). */
static void draw_bar_card(int fill_256)
{
  int w = 332, h = 76, x = (UI_SCREEN_W - w) / 2, y = 98, k = (30 * fill_256 + 128) / 256 - 1;

  ui_door(0, 0, UI_SCREEN_W, UI_SCREEN_H);
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

static void draw_screen(int bar_fill)
{
  int hy, dy, tile_dy[TILE_COUNT], done, i;
  char info[48], when[16];

  ui_frame_begin();
  ui_ground();
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

  /* the strip: the selected slot big, its neighbours small */
  {
    int pos = (p.pos_f < UI_F_SCROLL) ?
     ui_ease_out_back(p.pos_from, p.cur * 256, p.pos_f, UI_F_SCROLL) : p.cur * 256;
    for(i = 0; i < SLOTS; i++)
      draw_slot(strip_slot[i], i * 256 - pos, dy, (i == p.cur) && (p.row == 0) && done && !p.closing && !p.bar_text);
  }
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
      ui_round_fill(STRIP_CX - iw / 2 - 6, 145, iw + 12, 15, 2, UI_COL_PLUM);
      ui_text(STRIP_CX - iw / 2, 146, info, UI_COL_CREAM);
    }
  }

  for(i = 0; i < TILE_COUNT; i++)
    if(tile_dy[i] < 999)
      draw_tile(i, TILE_Y + tile_dy[i], (p.row == 1) && (i == p.tile) && done);
  if((p.row == 1) && done && !p.closing && !p.bar_text)
  {
    int tx = (p.tile_f < UI_F_CURSOR) ?
     ui_ease_out_back(p.tile_from, 18 + p.tile * 64, p.tile_f, UI_F_CURSOR) : 18 + p.tile * 64;
    ui_cursor_frame(tx - 2, TILE_Y - 2, TILE_W, TILE_H);
  }

  if(p.exit_holding)
    draw_exit_hold();
  if(p.toast_f >= 0)
  {
    if(!ui_toast(p.toast, p.toast_edge, p.toast_undo ? "Undo" : NULL, p.toast_f))
      p.toast_f = -1;
  }
  if(p.bar_text)
    draw_bar_card(bar_fill);
  draw_hints(done);
}

/* ---- saving with a moving bar -------------------------------------------- */

static void bar_hook(int current, int total)
{
  draw_screen(total ? current * 256 / total : 0);
  ui_frame_end_now();
}

static void run_with_bar(const char *text, const char *note)
{
  p.bar_text = text;
  p.bar_note = note;
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

static void move_tile(int to);

/* After a load the cursor goes to Resume, so one confirm plays on. */
static void cursor_to_resume(void)
{
  p.row = 1;
  move_tile(TILE_RESUME);
}

static void show_toast(const char *text, u16 edge, int undo, int undo_slot)
{
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

  state_no_question = 1;
  progress_hook = bar_hook;
  p.bar_text = NULL;
  state_error_text = NULL;
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
  end_bar();
  return ok;
}

static void action_load(int slot)
{
  char text[48], msg_text[64];
  int undo = UNDO_NONE;

  if(!p.has[slot])
    return;
  /* Load has Undo while "Save when leaving a game" is on: the moment being
     left goes to Last first. Loading Last itself has none. */
  if((slot != LAST_SLOT) && g_gpsp_config.save_on_leave)
  {
    run_with_bar("Saving to Last\xE2\x80\xA6", NULL);
    if(save_last_state(p.game))
      undo = UNDO_LOAD;
    end_bar();
    read_slot(LAST_SLOT);
  }
  if(load_slot(slot))
  {
    p.changed = 1;
    take_loaded_picture();
    slot_name(slot, text);
    sprintf(msg_text, "Loaded %s", text);
    g_game_config.last_slot = slot;
    show_toast(msg_text, UI_COL_SKY, undo, LAST_SLOT);
    cursor_to_resume();
  }
  else
    show_toast(state_error_text ? state_error_text : "Loading failed", UI_COL_PINK, UNDO_NONE, 0);
}

static void action_undo(void)
{
  char path[MAX_PATH];
  int slot = p.undo_slot;

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
    case UNDO_LOAD:
      if(load_slot(LAST_SLOT))
      {
        p.changed = 1;
        take_loaded_picture();
        show_toast("Back where you were", UI_COL_SKY, UNDO_NONE, 0);
        cursor_to_resume();
      }
      break;
  }
}

static int action_exit(void)
{
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
  p.pos_from = (p.pos_f < UI_F_SCROLL) ?
   ui_ease_out_back(p.pos_from, p.cur * 256, p.pos_f, UI_F_SCROLL) : p.cur * 256;
  p.cur = to;
  p.pos_f = 0;
  p.rest = 0;
  p.hold.p = 0;
}

static void move_tile(int to)
{
  if(to == p.tile)
    return;
  p.tile_from = (p.tile_f < UI_F_CURSOR) ?
   ui_ease_out_back(p.tile_from, 18 + p.tile * 64, p.tile_f, UI_F_CURSOR) : 18 + p.tile * 64;
  p.tile = to;
  p.tile_f = 0;
}

enum { LEAVE_NONE, LEAVE_RESUME, LEAVE_OLD_MENU, LEAVE_QUIT };

u32 ui_pause(u16 *game_picture)
{
  u32 mark, i;
  int leave = LEAVE_NONE, opened = 0;

  memset(&p, 0, sizeof(p));
  p.game = game_picture;
  p.shot_slot = -1;
  p.toast_f = -1;
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
  load_big_picture();
  p.rest = SHOT_REST + UI_F_SCROLL;
  build_layer();
  ui_input_init();

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
        if(ui_pressed(PSP_CTRL_DOWN)) p.row = 1;

        if(ui_pressed(ui_key_confirm))
          action_load(cur_slot());

        /* SELECT: Undo while the toast offers it, else save; hold: delete */
        if(p.has[cur_slot()] && !(ui_toast_up(p.toast_f) && p.toast_undo))
        {
          ui_hold_step(&p.hold, PSP_CTRL_SELECT);
          if(p.hold.done)
          {
            p.hold.p = 0;
            action_delete(cur_slot());
          }
        }
        else
          p.hold.p = 0;
        if(ui_tap(PSP_CTRL_SELECT))
        {
          if(ui_toast_up(p.toast_f) && p.toast_undo)
            action_undo();
          else
            action_save(cur_slot());
        }
      }
      else
      {
        move_tile(ui_step(p.tile, TILE_COUNT, PSP_CTRL_LEFT, PSP_CTRL_RIGHT));
        if(ui_pressed(PSP_CTRL_UP)) p.row = 0;
        /* L and R go up to the strip, onto Last or slot 0 */
        if(ui_pressed(PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER))
        {
          p.row = 0;
          move_slot(strip_position(ui_pressed(PSP_CTRL_LTRIGGER) ? LAST_SLOT : 0));
        }
        if(ui_tap(PSP_CTRL_SELECT) && ui_toast_up(p.toast_f) && p.toast_undo)
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
    if(++p.rest == SHOT_REST + UI_F_SCROLL)
      load_big_picture();

    draw_screen(0);
    if(p.toast_f >= 0)
      p.toast_f++;
    ui_frame_end();
  }

  ui_layer_reset();
  ui_pool_release(mark);

  if(leave == LEAVE_QUIT)
    quit(0);
  if(leave == LEAVE_OLD_MENU)
    return menu(p.game) | p.changed;

  /* The last frame is the game as it will be shown, without the door, so
     going back to the game needs no black frames. */
  {
    int x, y, w, h;
    ui_frame_begin();
    ui_fill(0, 0, UI_SCREEN_W, UI_SCREEN_H, COLOR16(0, 0, 0));
    get_game_rect(&x, &y, &w, &h);
    ui_blit_scaled(p.game, 240, 160, x, y, w, h);
    ui_frame_end();
  }
  video_resolution_keep(FRAME_GAME);
  set_cpu_clock(g_game_config.clock_speed_number);
  pause_sound(0);
  real_frame_count = 0;
  virtual_frame_count = 0;
  return p.changed;
}
