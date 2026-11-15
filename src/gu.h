/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef GU_H
#define GU_H

#define FONT_WIDTH  6
#define FONT_HEIGHT 10

#define GBA_SCREEN_WIDTH 240
#define GBA_SCREEN_HEIGHT 160

#define PSP_SCREEN_WIDTH 480
#define PSP_SCREEN_HEIGHT 272

#define FRAME_GAME 0
#define FRAME_MENU 1

void init_video();
void video_resolution(u32 mode);
void video_resolution_keep(u32 mode);
void clear_screen(u16 color);
void blit_to_screen(u16 *src, u32 w, u32 h, u32 x, u32 y);
u16 *copy_screen();
void get_game_rect(int *x, int *y, int *w, int *h);

#ifdef UI4
/* In-game overlay; see gu.c. Row pitch OSD_W. */
#define OSD_W 512
#define OSD_H 32
extern u16 osd_pixels[OSD_W * OSD_H];
extern int (*osd_tick)(int *x, int *y, int *w, int *h);
#define OSD_CHIP_W 64
#define OSD_CHIP_H 16
extern u16 osd_chip_pixels[OSD_CHIP_W * OSD_CHIP_H];
extern int osd_chip_rect[4];  /* x, y, w, h; w 0: hidden */
void osd_fade_start(void);
#endif
void flip_screen();
void video_read_mem_savestate(u32 ver);
void video_write_mem_savestate(u32 ver);
void video_get_size_savestate(u32 ver);

void debug_screen_clear();
void debug_screen_start();
void debug_screen_end();
void debug_screen_printf(const char *format, ...);
void debug_screen_printl(const char *format, ...);
void debug_screen_newline(u32 count);
void debug_screen_update();

typedef enum
{
  unscaled,
  scaled_aspect,
  fullscreen,
  option1,
  option2,
} video_scale_type;

typedef enum
{
  filter_nearest,
  filter_bilinear,
  filter_sharp_bilinear
} video_filter_type;

typedef enum
{
  PROGRESSIVE,
  INTERLACE
} VIDEO_INTERLACE_TYPE;

typedef enum
{
  R4_3,
  R16_9
} VIDEO_RATIO_TYPE;

typedef struct
{
  float u1;
  float v1;
  float x1;
  float y1;
  float z1;
  float u2;
  float v2;
  float x2;
  float y2;
  float z2;
} SPRITE;

typedef struct
{
  int u;
  int displaymode;
  int width;
  int height;
  int x;
  int y;
  int z;
} VIDEO_OUT_PARAMETER;

typedef struct
{
  int x;
  int y;
  int width;
  int height;
} VIEW_PORT;

typedef struct
{
  u32 x;
  u32 y;
} TEXTURE_BIT;


typedef struct
{
  u32 pitch;
  u32 width;
  u32 height;
} TEXTURE_SIZE;

typedef struct
{
  u32 width;
  u32 height;
} SCREEN_SIZE;

typedef struct
{
  VIDEO_OUT_PARAMETER video_out;  /* parameters for pspDveMgrSetVideoOut */
  int filter[2];                  /* texture filter while the menu is shown */
  TEXTURE_SIZE texture_size;
  TEXTURE_BIT texture_bit;        /* texture width/height as powers of two */
  SCREEN_SIZE screen_size;        /* display buffer size */
  VIEW_PORT view;                 /* visible area */
  SPRITE screen_setting_1;
  SPRITE screen_setting_2;
} SCREEN_PARAMETER;

extern float *temp_vertex;
extern u16 *vram_data;

extern u16 *screen_address;
extern u32 screen_pitch;
extern u32 screen_width;
extern u32 screen_height;
extern u32 screen_width2;
extern u32 screen_height2;
extern u32 video_out_mode;

extern u32 current_scale;

extern const SCREEN_PARAMETER screen_parameter_psp_game_init[];
extern const SCREEN_PARAMETER screen_parameter_psp_menu_init;

extern const SCREEN_PARAMETER screen_parameter_composite_game_init[];
extern const SCREEN_PARAMETER screen_parameter_composite_menu_init[];

extern const SCREEN_PARAMETER screen_parameter_component_game_init[];
extern const SCREEN_PARAMETER screen_parameter_component_menu_init[];

extern SCREEN_PARAMETER *current_parameter;

void set_resolution_parameter_game(video_scale_type scale);
void set_resolution_parameter_menu();

#define UNIVERSAL_VRAM_ADDR (0x441A5C00)

#endif
