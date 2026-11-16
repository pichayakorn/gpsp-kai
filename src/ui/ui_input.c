/* UI input: key edges, taps, holds and d-pad repeat, read once per frame. */
#include "../common.h"
#include "ui.h"

#define UI_KEYS (PSP_CTRL_SELECT | PSP_CTRL_START | PSP_CTRL_UP | PSP_CTRL_RIGHT | \
                 PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | \
                 PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE | PSP_CTRL_CROSS | PSP_CTRL_SQUARE | \
                 PSP_CTRL_HOME)
#define UI_DPAD (PSP_CTRL_UP | PSP_CTRL_RIGHT | PSP_CTRL_DOWN | PSP_CTRL_LEFT)
#define UI_REPEAT_DELAY 18
#define UI_REPEAT_RATE   4
#define UI_STICK_MARGIN 64  /* stick counts as a direction beyond 128 +- this */

u32 ui_key_confirm = PSP_CTRL_CROSS;
u32 ui_key_back    = PSP_CTRL_CIRCLE;

static u32 keys_now, keys_prev, keys_repeat, keys_ignored, keys_held;
static u16 hold_frames[32];
static u16 last_hold[32];

static u32 read_keys(void)
{
  SceCtrlData ctrl;
  u32 keys;

  sceCtrlPeekBufferPositive(&ctrl, 1);
  keys = ctrl.Buttons;
  if(g_use_home == 1)
    keys |= readHomeButton();

  if(ctrl.Lx < 128 - UI_STICK_MARGIN) keys |= PSP_CTRL_LEFT;
  if(ctrl.Lx > 128 + UI_STICK_MARGIN) keys |= PSP_CTRL_RIGHT;
  if(ctrl.Ly < 128 - UI_STICK_MARGIN) keys |= PSP_CTRL_UP;
  if(ctrl.Ly > 128 + UI_STICK_MARGIN) keys |= PSP_CTRL_DOWN;

  return keys & UI_KEYS;
}

void ui_input_init(void)
{
  int enter = 1;

  /* PSP_SYSTEMPARAM_ID_INT_UNKNOWN (9): 0 = ○ confirms (Japanese models) */
  sceUtilityGetSystemParamInt(9, &enter);
  if(g_gpsp_config.confirm_button != CONFIRM_SYSTEM)  /* Settings › Confirm button */
    enter = (g_gpsp_config.confirm_button == CONFIRM_CROSS);
  ui_key_confirm = enter ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
  ui_key_back    = enter ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;

  keys_ignored = read_keys();
  keys_now = keys_prev = keys_repeat = keys_held = 0;
  memset(hold_frames, 0, sizeof(hold_frames));
  memset(last_hold, 0, sizeof(last_hold));
}

#ifdef UI_PERF
u32 (*ui_input_script)(void) = NULL;
#endif

void ui_input_update(void)
{
  u32 i, bit, now = read_keys();

#ifdef UI_PERF
  if(ui_input_script)
    now = ui_input_script();
#endif

  keys_ignored &= now;
  now &= ~keys_ignored;

  keys_prev = keys_now;
  keys_now = now;
  keys_repeat = 0;
  keys_held = 0;

  for(i = 0, bit = 1; i < 32; i++, bit <<= 1)
  {
    if(!(UI_KEYS & bit))
      continue;

    if(now & bit)
    {
      if(hold_frames[i] < 0xFFFF)
        hold_frames[i]++;
      if(hold_frames[i] == UI_F_HOLD)
        keys_held |= bit;
      if((UI_DPAD & bit) && ((hold_frames[i] == 1) ||
       ((hold_frames[i] > UI_REPEAT_DELAY) &&
       (((hold_frames[i] - UI_REPEAT_DELAY) % UI_REPEAT_RATE) == 0))))
        keys_repeat |= bit;
    }
    else
    {
      if(hold_frames[i])
        last_hold[i] = hold_frames[i];
      hold_frames[i] = 0;
    }
  }
}

u32 ui_down(u32 mask)     { return keys_now & mask; }
u32 ui_pressed(u32 mask)  { return keys_now & ~keys_prev & mask; }
u32 ui_released(u32 mask) { return keys_prev & ~keys_now & mask; }
u32 ui_repeat(u32 mask)   { return keys_repeat & mask; }

int ui_step(int i, int count, u32 less, u32 more)
{
  int to = i;

  if(ui_repeat(less)) to--;
  if(ui_repeat(more)) to++;
  if((to < 0) || (to >= count))
  {
    /* a fresh press wraps to the other end; a repeat stops at the end */
    if(!ui_pressed(less | more))
      return i;
    to = (to + count) % count;
  }
  return to;
}
u32 ui_held(u32 mask)     { return keys_held & mask; }

u32 ui_tap(u32 mask)
{
  u32 i, bit, released = ui_released(mask);

  for(i = 0, bit = 1; i < 32; i++, bit <<= 1)
    /* a tap is a release within UI_F_HOLD_SHOW; held longer, the press is a
       hold, and letting go before it fills does nothing */
    if((released & bit) && (last_hold[i] < UI_F_HOLD_SHOW))
      return 1;
  return 0;
}

u32 ui_hold_frames(u32 key)
{
  u32 i;

  for(i = 0; i < 32; i++)
    if(key == (1u << i))
      return hold_frames[i];
  return 0;
}
