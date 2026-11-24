/* ui_first.c - first run (spec section 4, First run; r5 first; step 7)
 *
 * Once, the first time kai starts: three cards that say how to pause, which
 * keys confirm and go back, and where help is, then Start. The language
 * comes from the PSP (kai's own setting until changed).
 */

#include "../common.h"
#include "ui.h"

#define CARD_W  140
#define CARD_H  140
#define CARD_Y  54

/* the first-run cards' own 26 px glyphs: a cream disc with a 2 px plum
   symbol (never the 13 px glyph scaled) */
static void glyph26(int x, int y, u32 key)
{
  u16 P = UI_COL_PLUM;
  int i, j;

  ui_disc(x + 13, y + 13, 13, UI_COL_CREAM);
  if(key == PSP_CTRL_CROSS)
    for(i = 0; i < 10; i++)
    {
      ui_fill(x + 8 + i, y + 8 + i, 2, 2, P);
      ui_fill(x + 16 - i, y + 8 + i, 2, 2, P);
    }
  else if(key == PSP_CTRL_SQUARE)
    ui_rect(x + 8, y + 8, 10, 10, 2, P);
  else if(key == PSP_CTRL_CIRCLE)
    ui_ring(x + 13, y + 13, 6, 2, P);
  else  /* triangle: apex at the top, a 14 px base, 2 px strokes */
    for(j = 0; j < 11; j++)
    {
      int half = 1 + (j * 6 + 5) / 10, lx = x + 13 - half, rx = x + 12 + half;
      if((j >= 9) || (rx - lx < 4))
        ui_fill(lx, y + 7 + j, rx - lx + 1, 1, P);
      else
      {
        ui_fill(lx, y + 7 + j, 2, 1, P);
        ui_fill(rx - 1, y + 7 + j, 2, 1, P);
      }
    }
}

/* the language in its own script (maintainer, 2026-11-20); the menu
   language's own font is loaded, so it never draws as boxes here */
static const char *language_name(u32 lang)
{
  static const char *names[12] =
  {
    "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "English", "Fran\xC3\xA7" "ais",
    "Espa\xC3\xB1ol", "Deutsch", "Italiano", "Nederlands", "Portugu\xC3\xAAs",
    "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9",
    "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4",
    "\xE4\xB8\xAD\xE6\x96\x87\xEF\xBC\x88\xE7\xB9\x81\xE9\xAB\x94\xEF\xBC\x89",
    "\xE4\xB8\xAD\xE6\x96\x87\xEF\xBC\x88\xE7\xAE\x80\xE4\xBD\x93\xEF\xBC\x89"
  };
  return names[(lang < 12) ? lang : 1];
}

static void draw(int t)
{
  static const char welcome[] = "Welcome to gpSP kai";
  const char *titles[3] = { "Pause any time", "Confirm and back", "Help in Settings" };
  char confirm_body[96];
  const char *bodies[3];
  u32 keys[3];
  u16 cols[3] = { UI_COL_TANGERINE, UI_COL_SKY, UI_COL_LIME };
  int i, j;

  keys[0] = PSP_CTRL_TRIANGLE;
  keys[1] = ui_key_confirm;
  keys[2] = PSP_CTRL_SQUARE;
  /* the glyphs in the sentence follow the keys above it; a no-break space
     keeps each with its word */
  snprintf(confirm_body, sizeof(confirm_body), "Same as your PSP: %s\xC2\xA0" "confirms, %s\xC2\xA0goes back.",
   (ui_key_confirm == PSP_CTRL_CROSS) ? "\xC3\x97" : "\xE2\x97\x8B",
   (ui_key_confirm == PSP_CTRL_CROSS) ? "\xE2\x97\x8B" : "\xC3\x97");
  bodies[0] = "\xE2\x96\xB3 or HOME opens and closes the pause screen.";
  bodies[1] = confirm_body;
  bodies[2] = "\xE2\x96\xA1 explains the selected setting.";

  ui_frame_begin();
  ui_ground();
  ui_print(240 - ui_print_width(welcome, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X) / 2, 10,
   welcome, UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD | UI_TEXT_2X);
  for(i = 0; i < 3; i++)
  {
    /* 12 f each, 4 f apart: they drop 20 px into place */
    int f = t - i * 4, x = 20 + i * 150, y;
    char line[160];
    if(f <= 0)
      continue;
    y = CARD_Y - 20 + ui_ease_out_back(0, 20, f, UI_F_FIRST_RUN);
    ui_round_fill(x + 3, y + 3, CARD_W, CARD_H, 3, UI_COL_SHADE);
    ui_round_fill(x, y, CARD_W, CARD_H, 3, UI_COL_CARD);
    ui_round_fill(x + 46, y + 12, 48, 40, 3, cols[i]);
    glyph26(x + 57, y + 19, keys[i]);
    ui_print_fit(x + 70 - ui_print_width(titles[i], UI_FONT_BODY, UI_TEXT_BOLD) / 2, y + 62,
     titles[i], UI_COL_CREAM, UI_FONT_BODY, UI_TEXT_BOLD, 130);
    for(j = 0; (j < 4) && ui_wrap_line(bodies[i], j, 124, UI_FONT_SMALL, line, sizeof(line)); j++)
      ui_print(x + 8, y + 84 + j * 13, line, UI_COL_MUTE, UI_FONT_SMALL, 0);
  }
  if(t >= 8 + UI_F_FIRST_RUN)
  {
    /* the cards are in: the language line, the Start button, × Start */
    char lang[64];
    int w = ui_print_width("Start", UI_FONT_BODY, 0) + 30;
    ui_hint_t h[1];
    snprintf(lang, sizeof(lang), "Language: %s, from your PSP", language_name(g_gpsp_config.language));
    ui_print(240 - ui_print_width(lang, UI_FONT_SMALL, 0) / 2, 206, lang, UI_COL_MUTE, UI_FONT_SMALL, 0);
    ui_round_fill(240 - w / 2 + 2, 224, w, 20, 2, UI_COL_SHADE);
    ui_round_fill(240 - w / 2, 222, w, 20, 2, UI_COL_TANGERINE);
    ui_glyph(240 - w / 2 + 6, 225, ui_key_confirm);
    ui_print(240 - w / 2 + 24, 223, "Start", UI_COL_PLUM, UI_FONT_BODY, 0);
    h[0].keys = ui_key_confirm;
    h[0].verb = "Start";
    ui_hint_packed(h, 1, 14);
  }
  else
    ui_hint_packed(NULL, 0, 14);  /* nothing to press yet */
  ui_frame_end();
}

/* Shows the first-run cards once; returns at × Start. */
void ui_first_run(void)
{
  int t = 0;

  if(g_gpsp_config.first_run_done)
    return;
  ui_input_init();
  while(1)
  {
    ui_input_update();
    if((t >= 8 + UI_F_FIRST_RUN) && ui_pressed(ui_key_confirm))
      break;
    draw(t);
    if(t < 1000)
      t++;
  }
  g_gpsp_config.first_run_done = YES;
  save_config_file();
}
