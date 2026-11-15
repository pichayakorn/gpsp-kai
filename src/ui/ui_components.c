/* Step 2 component screen (UI4=1 builds only; hold L while kai starts).
 * A Settings-like page that uses every component: tabs, card rows with a
 * cycle value, segments, a switch, blocks, a per-game mark, a disabled row
 * and an action, the cursor frame, a toast with Undo, a hold bar and a
 * dialog. Shows how long each frame takes to draw. Texts are placeholders
 * in English; the real screens take them from the .msg files. */
#include "../common.h"
#include "ui.h"

enum { ROW_CYCLE, ROW_SEGMENTS, ROW_SWITCH, ROW_BLOCKS, ROW_PER_GAME,
       ROW_DISABLED, ROW_ACTION, ROW_COUNT };

#define ROWS_Y 44
#define ROW_X  UI_MARGIN
#define ROW_W  (UI_SCREEN_W - UI_MARGIN * 2)

static const char *tab_labels[5] = { "Display", "Sound", "Controls", "Play", "System" };
static const char *smooth_values[3] = { "Sharp", "Soft", "Smooth" };
static const char *view_values[2] = { "Grid", "List" };
static const char *rapid_values[3] = { "Slow", "Medium", "Fast" };
static const char *dialog_buttons[2] = { "Cancel", "Reset" };
static const char *row_tips[ROW_COUNT] =
{
  "Cycle value: left and right change it; the new value slides in.",
  "Segments: the fill springs to the new choice.",
  "Switch: the knob springs across.",
  "Blocks: the new block pops.",
  "The cartridge mark: this setting is saved for this game only.",
  "A disabled row: no card, and the cursor skips it.",
  "An action row: confirm opens a dialog."
};

void ui_components(void)
{
  ui_hint_t hints[] =
  {
    { PSP_CTRL_LEFT | PSP_CTRL_RIGHT, "Change" },
    { PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, "Tabs" },
    { PSP_CTRL_SELECT, "Toast" },
    { PSP_CTRL_START, "Hold: reset" },
    { 0, "Exit" },
  };
  ui_hint_t dialog_hints[] =
  {
    { PSP_CTRL_LEFT | PSP_CTRL_RIGHT, "Choose" },
    { 0, "OK" },
    { 0, "Cancel" },
  };
  int tab = 0, tab_prev = -1, tab_f = UI_F_TAB;
  int cursor = 0, cursor_from = ROWS_Y, cursor_f = UI_F_CURSOR;
  int smooth = 0, view = 0, fps = 0, volume = 7, rapid = 2;
  int value_f[ROW_COUNT], view_from = 0;
  int toast_f = -1, toast_undo = 0;
  const char *toast_text = "";
  u16 toast_edge = UI_COL_SKY;
  int dialog = 0, dialog_f = 0, dialog_closing = 0, dialog_button = 0;
  ui_hold_t hold = { 0, 0, 0 };
  u32 frames = 0, sum_us = 0, max_us = 0, avg_us = 0, peak_us = 0;
  char line[96];
  int i;

  for(i = 0; i < ROW_COUNT; i++)
    value_f[i] = UI_F_VALUE;

  video_resolution(FRAME_MENU);
  ui_input_init();
  hints[4].keys = ui_key_back;
  dialog_hints[1].keys = ui_key_confirm;
  dialog_hints[2].keys = ui_key_back;

  while(1)
  {
    u32 t0, t1;
    int changed = 0, dir = 0;

    ui_input_update();

    if(dialog)
    {
      if(dialog_closing)
      {
        if(++dialog_f > UI_F_DIALOG_OUT)
          dialog = 0;
      }
      else
      {
        if(dialog_f < UI_F_DIALOG_IN)
          dialog_f++;
        if(ui_repeat(PSP_CTRL_LEFT | PSP_CTRL_RIGHT))
          dialog_button ^= 1;
        if(ui_pressed(ui_key_confirm) || ui_pressed(ui_key_back))
        {
          if(ui_pressed(ui_key_confirm) && (dialog_button == 1))
          {
            smooth = 0; view = 0; fps = 0; volume = 7; rapid = 2;
            toast_text = "Display reset";
            toast_edge = UI_COL_LIME;
            toast_undo = 0;
            toast_f = 0;
          }
          dialog_closing = 1;
          dialog_f = 0;
        }
      }
    }
    else
    {
      if(ui_pressed(ui_key_back))
        break;

      if(ui_pressed(PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER))
      {
        tab_prev = tab;
        tab = (tab + (ui_pressed(PSP_CTRL_RTRIGGER) ? 1 : 4)) % 5;
        tab_f = 0;
      }

      if(ui_repeat(PSP_CTRL_UP | PSP_CTRL_DOWN))
      {
        int step = ui_repeat(PSP_CTRL_DOWN) ? 1 : ROW_COUNT - 1;
        cursor_from = ui_ease_out_back(cursor_from, ROWS_Y + cursor * UI_ROW_PITCH,
         cursor_f, UI_F_CURSOR);
        cursor = (cursor + step) % ROW_COUNT;
        if(cursor == ROW_DISABLED)  /* the cursor skips a disabled row */
          cursor = (cursor + step) % ROW_COUNT;
        cursor_f = 0;
      }

      if(ui_repeat(PSP_CTRL_RIGHT)) dir = 1;
      if(ui_repeat(PSP_CTRL_LEFT)) dir = -1;
      if(dir)
      {
        changed = 1;
        switch(cursor)
        {
          case ROW_CYCLE:    smooth = (smooth + 3 + dir) % 3; break;
          case ROW_SEGMENTS: view_from = view; view = (view + 2 + dir) % 2; break;
          case ROW_SWITCH:   fps ^= 1; break;
          case ROW_BLOCKS:
            if(((dir > 0) && (volume < 10)) || ((dir < 0) && (volume > 0)))
              volume += dir;
            else
              changed = 0;
            break;
          case ROW_PER_GAME: rapid = (rapid + 3 + dir) % 3; break;
          default:           changed = 0; break;
        }
        if(changed)
          value_f[cursor] = 0;
      }

      if(ui_pressed(ui_key_confirm) && (cursor == ROW_ACTION))
      {
        dialog = 1;
        dialog_f = 0;
        dialog_closing = 0;
        dialog_button = 0;
      }

      if(ui_pressed(PSP_CTRL_SELECT))
      {
        if(ui_toast_up(toast_f) && toast_undo)
        {
          toast_text = "Slot 3 is back as it was";
          toast_undo = 0;
        }
        else
        {
          toast_text = "Saved to slot 3";
          toast_undo = 1;
        }
        toast_edge = UI_COL_SKY;
        toast_f = 0;
      }

      ui_hold_step(&hold, PSP_CTRL_START);
      if(hold.done)
      {
        smooth = 0; view = 0; fps = 0; volume = 7; rapid = 2;
        toast_text = "Display reset";
        toast_edge = UI_COL_LIME;
        toast_undo = 0;
        toast_f = 0;
      }
    }

    t0 = sceKernelGetSystemTimeLow();
    ui_frame_begin();
    ui_ground();

    if(tab_f < UI_F_TAB) tab_f++;
    ui_tabs(UI_MARGIN, tab_labels, 5, tab, tab_prev, tab_f);

    sprintf(line, "%d.%02d / %d.%02d ms", (int)(avg_us / 1000), (int)(avg_us % 1000 / 10),
     (int)(peak_us / 1000), (int)(peak_us % 1000 / 10));
    ui_text(UI_SCREEN_W - UI_MARGIN - ui_text_width(line), 12, line, UI_COL_MUTE);

    if(cursor_f < UI_F_CURSOR) cursor_f++;
    for(i = 0; i < ROW_COUNT; i++)
    {
      int y = ROWS_Y + i * UI_ROW_PITCH, sel = (i == cursor), vx, vy;
      u16 g = UI_COL_LIME;

      if(value_f[i] < UI_F_VALUE)
        value_f[i]++;
      switch(i)
      {
        case ROW_CYCLE:
          ui_card_row(ROW_X, y, ROW_W, "Smoothing", sel, g, smooth ? UI_ROW_CHANGED : 0, &vx, &vy);
          ui_row_cycle(vx, vy, smooth_values[smooth], sel, g, value_f[i]);
          break;
        case ROW_SEGMENTS:
          ui_card_row(ROW_X, y, ROW_W, "Library view", sel, g, view ? UI_ROW_CHANGED : 0, &vx, &vy);
          ui_row_segments(vx, vy, view_values, 2, view, view_from, value_f[i], sel, g);
          break;
        case ROW_SWITCH:
          ui_card_row(ROW_X, y, ROW_W, "Show FPS", sel, g, fps ? UI_ROW_CHANGED : 0, &vx, &vy);
          ui_row_switch(vx, vy, ui_switch_pos(fps, value_f[i]), sel);
          break;
        case ROW_BLOCKS:
          sprintf(line, "%d", volume);
          ui_card_row(ROW_X, y, ROW_W, "Volume", sel, g, (volume != 7) ? UI_ROW_CHANGED : 0, &vx, &vy);
          ui_row_blocks(vx, vy, 11, volume, line, value_f[i], sel, g);
          break;
        case ROW_PER_GAME:
          ui_card_row(ROW_X, y, ROW_W, "Rapid fire speed", sel, g,
           UI_ROW_PER_GAME | ((rapid != 2) ? UI_ROW_CHANGED : 0), &vx, &vy);
          ui_row_cycle(vx, vy, rapid_values[rapid], sel, g, value_f[i]);
          break;
        case ROW_DISABLED:
          ui_card_row(ROW_X, y, ROW_W, "Side bars", 0, g, UI_ROW_DISABLED, &vx, &vy);
          ui_row_note(vx, vy, "Needs Scale: Fit");
          break;
        case ROW_ACTION:
          ui_card_row(ROW_X, y, ROW_W, "Reset this tab", sel, g, 0, &vx, &vy);
          ui_row_action(vx, vy, sel);
          break;
      }
    }
    {
      int cy = ui_ease_out_back(cursor_from, ROWS_Y + cursor * UI_ROW_PITCH, cursor_f, UI_F_CURSOR);
      ui_cursor_frame(ROW_X - UI_LIFT, cy - UI_LIFT, ROW_W, UI_ROW_H);
    }

    ui_text(UI_MARGIN, 210, (char *)row_tips[cursor], UI_COL_MUTE);

    /* the hold bar for START, while it fills or drains */
    if(hold.p > 0)
    {
      ui_round_fill(UI_MARGIN + 3, 227, 200, 18, 2, UI_COL_SHADE);
      ui_round_fill(UI_MARGIN, 224, 200, 18, 2, UI_COL_CARD);
      ui_hold_bar(UI_MARGIN + 8, 230, 184, hold.p, UI_COL_PINK);
    }

    if(toast_f >= 0)
    {
      if(!ui_toast(toast_text, toast_edge, toast_undo ? "Undo" : NULL, toast_f))
        toast_f = -1;
      else
        toast_f++;
    }

    if(dialog)
    {
      ui_dialog("Reset Display?",
       "Every setting on this tab goes back to its default. Settings saved "
       "for one game are reset for the game that is running only.",
       dialog_buttons, 2, dialog_button, UI_COL_PINK, dialog_f, dialog_closing);
      ui_hint_strip(dialog_hints, 3);
    }
    else
      ui_hint_strip(hints, sizeof(hints) / sizeof(hints[0]));

    t1 = sceKernelGetSystemTimeLow();
    sum_us += t1 - t0;
    if(t1 - t0 > max_us)
      max_us = t1 - t0;
    if(++frames == 60)
    {
      avg_us = sum_us / frames;
      peak_us = max_us;
      frames = sum_us = max_us = 0;
    }

    ui_frame_end();
  }
}
