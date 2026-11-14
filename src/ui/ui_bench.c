/* Step 1 benchmark screen (UI4=1 builds only; hold R while kai starts).
 * Draws a full frame of typical UI every frame and shows how long the
 * drawing takes, at 222, 266 or 333 MHz, plus the memory left. */
#include "../common.h"
#include "ui.h"

#define BENCH_ROWS 9

/* Per-part timing: MARK(n) adds the time since the last mark to part n. */
enum { PART_GROUND, PART_ROWS, PART_GAME, PART_TEXT, PART_TOAST, PART_STATS,
       PART_HINTS, PART_COUNT };
static const char *part_names[PART_COUNT] =
  { "ground", "rows", "game+door", "text", "toast+bar", "stats", "hints" };
static u32 part_sum[PART_COUNT], part_avg[PART_COUNT], part_t;
#define MARK(n) do { u32 now_ = sceKernelGetSystemTimeLow(); \
                     part_sum[n] += now_ - part_t; part_t = now_; } while(0)

/* VRAM between the end of kai's display, game and menu buffers and the
 * shared scratch buffer (UNIVERSAL_VRAM_ADDR), see gu.c. */
#define VRAM_BUFFERS_END ((768 * 512 * 2) + (240 * 161 * 2) * 2 + (480 * 273 * 2) * 2)
#define VRAM_GAP ((UNIVERSAL_VRAM_ADDR & 0x00FFFFFF) - VRAM_BUFFERS_END)

/* Through a volatile pointer: GCC removes a malloc() whose result is only
 * tested and freed, and assumes it succeeded. */
static void *volatile probe_block;

static u32 largest_malloc_kb(void)
{
  u32 lo = 0, hi = 64 * 1024;  /* KB */

  while(hi - lo > 16)
  {
    u32 mid = (lo + hi) / 2;
    probe_block = malloc(mid * 1024);
    if(probe_block != NULL)
    {
      free(probe_block);
      lo = mid;
    }
    else
      hi = mid;
  }
  probe_block = NULL;
  return lo;
}

static void draw_game_stand_in(int x, int y)
{
  int i;
  static const u16 bars[8] =
  {
    COLOR16(31, 31, 31), COLOR16(31, 31, 0), COLOR16(0, 31, 31), COLOR16(0, 31, 0),
    COLOR16(31, 0, 31), COLOR16(31, 0, 0), COLOR16(0, 0, 31), COLOR16(4, 4, 4)
  };

  for(i = 0; i < 8; i++)
    ui_fill(x + i * 30, y, 30, 160, bars[i]);
}

void ui_bench(void)
{
  static const u32 clocks[3] = { 222, 266, 333 };
  static const ui_hint_t hints[] =
  {
    { PSP_CTRL_CROSS, "Toast" },
    { PSP_CTRL_TRIANGLE, "Door" },
    { PSP_CTRL_SQUARE, "Text" },
    { PSP_CTRL_SELECT, "Hold" },
    { PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, "MHz" },
    { PSP_CTRL_START, "Exit" }
  };
  int old_clock = scePowerGetCpuClockFrequency();
  int clock = 0, cursor = 0, cursor_from = 40, cursor_frame = UI_F_CURSOR;
  int door = 1, heavy = 0, toast_frame = -1, hold_done = 0;
  u32 frames = 0, sum_us = 0, max_us = 0, avg_us = 0, peak_us = 0;
  u32 heap_kb = largest_malloc_kb();
  char line[128];

  scePowerSetClockFrequency(clocks[clock], clocks[clock], clocks[clock] / 2);
  video_resolution(FRAME_MENU);
  ui_input_init();

  while(1)
  {
    u32 t0, t1;
    int i, y, x;

    ui_input_update();
    if(ui_tap(PSP_CTRL_START))
      break;
    if(ui_pressed(PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER))
    {
      clock = (clock + (ui_pressed(PSP_CTRL_RTRIGGER) ? 1 : 2)) % 3;
      scePowerSetClockFrequency(clocks[clock], clocks[clock], clocks[clock] / 2);
      frames = sum_us = max_us = 0;
      memset(part_sum, 0, sizeof(part_sum));
    }
    if(ui_repeat(PSP_CTRL_UP | PSP_CTRL_DOWN))
    {
      /* re-target from where the frame is now, mid-move or not */
      cursor_from = ui_ease_out_back(cursor_from, 40 + cursor * UI_ROW_PITCH,
       cursor_frame, UI_F_CURSOR);
      cursor = (cursor + (ui_repeat(PSP_CTRL_DOWN) ? 1 : BENCH_ROWS - 1)) % BENCH_ROWS;
      cursor_frame = 0;
    }
    if(ui_pressed(PSP_CTRL_TRIANGLE)) door ^= 1;
    if(ui_pressed(PSP_CTRL_SQUARE)) heavy ^= 1;
    if(ui_pressed(PSP_CTRL_CROSS)) toast_frame = 0;
    if(ui_held(PSP_CTRL_SELECT)) hold_done = UI_F_FLASH;

    t0 = sceKernelGetSystemTimeLow();
    part_t = t0;
    ui_frame_begin();

    ui_ground();
    MARK(PART_GROUND);
    ui_text(UI_MARGIN, 12, "UI4 benchmark", UI_COL_CREAM);
    /* the clock the PSP reports; a CFW plugin may override the request */
    sprintf(line, "%d MHz (set %d)", scePowerGetCpuClockFrequency(), (int)clocks[clock]);
    ui_text(UI_SCREEN_W - UI_MARGIN - ui_text_width(line), 12, line, UI_COL_MUTE);

    /* rows with a lifted selection and the cursor frame flying to it */
    if(cursor_frame < UI_F_CURSOR)
      cursor_frame++;
    for(i = 0; i < BENCH_ROWS; i++)
    {
      y = 40 + i * UI_ROW_PITCH;
      sprintf(line, "Row %d: Smoothing", i + 1);
      ui_card(UI_MARGIN, y, 180, UI_ROW_H, (i == cursor) ? UI_COL_TANGERINE : UI_COL_CARD, i == cursor);
      x = (i == cursor) ? UI_MARGIN - UI_LIFT : UI_MARGIN;
      ui_text(x + 6, ((i == cursor) ? y - UI_LIFT : y) + 5, line,
       (i == cursor) ? UI_COL_PLUM : UI_COL_CREAM);
      if(heavy)
        ui_text(x + 120, ((i == cursor) ? y - UI_LIFT : y) + 5, "Sharp", UI_COL_MUTE);
    }
    y = ui_ease_out_back(cursor_from, 40 + cursor * UI_ROW_PITCH, cursor_frame, UI_F_CURSOR);
    ui_rect(UI_MARGIN - UI_LIFT - 2, y - UI_LIFT - 2, 184, UI_ROW_H + 4, 2, UI_COL_CREAM);

    MARK(PART_ROWS);

    /* the game behind, with the screen door */
    draw_game_stand_in(220, 40);
    if(door)
      ui_door(220, 40, 240, 160);
    MARK(PART_GAME);
    if(heavy)
      for(i = 0; i < 8; i++)
        ui_text(228, 48 + i * 18, "The quick brown fox jumps over the dog", UI_COL_CREAM);

    MARK(PART_TEXT);

    /* toast: in 12 f, held 120 f, out 8 f */
    if(toast_frame >= 0)
    {
      int f = toast_frame++, tx;
      if(f < UI_F_TOAST_IN)
        tx = ui_ease_out_back(UI_SCREEN_W, 300, f + 1, UI_F_TOAST_IN);
      else if(f < UI_F_TOAST_IN + UI_F_TOAST_HOLD)
        tx = 300;
      else if(f < UI_F_TOAST_IN + UI_F_TOAST_HOLD + UI_F_TOAST_OUT)
        tx = ui_ease_in_cubic(300, UI_SCREEN_W, f - UI_F_TOAST_IN - UI_F_TOAST_HOLD + 1, UI_F_TOAST_OUT);
      else
      {
        tx = UI_SCREEN_W;
        toast_frame = -1;
      }
      ui_card(tx, 208, 170, 30, UI_COL_CARD, 1);
      ui_fill(tx - UI_LIFT, 208 - UI_LIFT, 3, 30, UI_COL_SKY);
      ui_text(tx + 6, 208 - UI_LIFT + 9, "Saved to slot 3", UI_COL_CREAM);
    }

    /* hold bar: fills in 60 f while SELECT is down */
    {
      u32 held = ui_hold_frames(PSP_CTRL_SELECT);
      if(held > UI_F_HOLD) held = UI_F_HOLD;
      ui_fill(UI_MARGIN, 214, 180, 6, UI_COL_CARD);
      ui_fill(UI_MARGIN, 214, 180 * held / UI_F_HOLD, 6, hold_done ? UI_COL_CREAM : UI_COL_PINK);
      if(hold_done) hold_done--;
    }

    MARK(PART_TOAST);

    /* per-part averages, in hundredths of a ms */
    x = UI_MARGIN;
    for(i = 0; i < PART_COUNT; i++)
    {
      sprintf(line, "%s %d.%02d", part_names[i], (int)(part_avg[i] / 1000),
       (int)(part_avg[i] % 1000 / 10));
      x = ui_text(x, 27, line, UI_COL_MUTE) + 8;
    }

    sprintf(line, "draw %d.%02d ms avg, %d.%02d max  heap %d KB  pool %d KB  game %d MB  VRAM gap %d KB",
     (int)(avg_us / 1000), (int)(avg_us % 1000 / 10), (int)(peak_us / 1000), (int)(peak_us % 1000 / 10),
     (int)heap_kb, (int)(ui_pool_free() / 1024), (int)(gamepak_ram_buffer_size >> 20),
     (int)(VRAM_GAP / 1024));
    ui_text(UI_MARGIN, 230, line, UI_COL_MUTE);

    MARK(PART_STATS);
    ui_hint_strip(hints, sizeof(hints) / sizeof(hints[0]));
    MARK(PART_HINTS);

    t1 = sceKernelGetSystemTimeLow();
    sum_us += t1 - t0;
    if(t1 - t0 > max_us)
      max_us = t1 - t0;
    if(++frames == 60)
    {
      avg_us = sum_us / frames;
      peak_us = max_us;
      for(i = 0; i < PART_COUNT; i++)
      {
        part_avg[i] = part_sum[i] / frames;
        part_sum[i] = 0;
      }
      frames = sum_us = max_us = 0;
    }

    ui_frame_end();
  }

  scePowerSetClockFrequency(old_clock, old_clock, old_clock / 2);
}
