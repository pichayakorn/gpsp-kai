/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
 * Copyright (C) 2007 ????? <?????>
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

#include "common.h"

static u32 __attribute__((aligned(32))) display_list[2048];

s32 load_video_config();
void set_resolution_parameter(video_scale_type scale);
void set_video_out();

#define MENU_LINE_SIZE  512
#define GBA_LINE_SIZE   240
#define FRAME_LINE_SIZE 768

#define VIDEO_CONFIG_FILENAME "cfg/video.cfg"

// VRAM memory map (managed manually, not through the VRAM allocation API)
// 0x000000-0x0BFFFF (0xC0000 768.0kb) display frame buffer
// 0x0C0000-0x0D2DFF (0x12E00  75.5kb) GBA frame buffer 1
// 0x0D2E00-0x0E5BFF (0x12E00  75.5kb) GBA frame buffer 2
// 0x0E5C00-0x125BFF (0x40000 256.0kb) menu frame buffer 1
// 0x125C00-0x165BFF (0x40000 256.0kb) menu frame buffer 2
// 0x165C00-0x1A5BFF (0x40000 256.0kb) menu skin buffer (reserved)
// 0x1A5C00-0x1FBFFF (0x56400 345.0kb) general-purpose data buffer
// 0x1FC000-0x1FC0FF (0x00100 256byte) GU command buffer for frame buffer transfer
// 0x1FC100-0x1FC1FF (0x00100 256byte) vertex buffer for frame buffer transfer
// 0x1FC200-0x1FFFFF (0x03E00  15.5kb) general-purpose vertex buffer
static float *screen_vertex = (float *)0x441FC100;
static u32 *ge_cmd = (u32 *)0x441FC000;
static u16 *psp_gu_vram_base = (u16 *)(0x44000000);
static u32 *ge_cmd_ptr = (u32 *)0x441FC000;
static u32 gecbid;
float *temp_vertex = (float *)0x441FC200;
u16 *vram_data = (u16 *)UNIVERSAL_VRAM_ADDR;

u32 video_draw_frame = FRAME_GAME;

#define GBA_BUFFER_SIZE (240*161*2)  /* one extra line for interlaced output */
#define MENU_BUFFER_SIZE (480*273*2) /* one extra line for interlaced output */
#define PSP_DISPLAY_BUFFER_SIZE (768 * 512 * 2)
u16 *screen_address = (u16 *)(0x04000000 + PSP_DISPLAY_BUFFER_SIZE);
u16 *screen_offset_address = (u16 *)PSP_DISPLAY_BUFFER_SIZE;
u32 screen_pitch = 240;
u32 screen_width = 240;
u32 screen_height = 160;
u32 screen_width2 = 240 / 2;
u32 screen_height2 = 160 / 2;

u32 flip_address[2][2] =
{
  { 0x04000000 + PSP_DISPLAY_BUFFER_SIZE, 0x04000000 + PSP_DISPLAY_BUFFER_SIZE + GBA_BUFFER_SIZE },
  { 0x04000000 + PSP_DISPLAY_BUFFER_SIZE + GBA_BUFFER_SIZE * 2, 0x04000000 + PSP_DISPLAY_BUFFER_SIZE + GBA_BUFFER_SIZE * 2 + MENU_BUFFER_SIZE }
};

u32 flip_offset_address[2][2] =
{
  { PSP_DISPLAY_BUFFER_SIZE, PSP_DISPLAY_BUFFER_SIZE + GBA_BUFFER_SIZE },
  { PSP_DISPLAY_BUFFER_SIZE + GBA_BUFFER_SIZE * 2, PSP_DISPLAY_BUFFER_SIZE + GBA_BUFFER_SIZE * 2 + MENU_BUFFER_SIZE }
};

#define GE_CMD_NOP    0x00
#define GE_CMD_VADDR  0x01
#define GE_CMD_IADDR  0x02
#define GE_CMD_PRIM   0x04
#define GE_CMD_SIGNAL 0x0C
#define GE_CMD_FINISH 0x0F
#define GE_CMD_BASE   0x10
#define GE_CMD_VTYPE  0x12
#define GE_CMD_WMS    0x3A
#define GE_CMD_WORLD  0x3B
#define GE_CMD_VMS    0x3C
#define GE_CMD_VIEW   0x3D
#define GE_CMD_PMS    0x3E
#define GE_CMD_PROJ   0x3F
#define GE_CMD_FBP    0x9C
#define GE_CMD_FBW    0x9D
#define GE_CMD_TBP0   0xA0
#define GE_CMD_TBW0   0xA8
#define GE_CMD_TSIZE0 0xB8
#define GE_CMD_TFLUSH 0xCB
#define GE_CMD_CLEAR  0xD3

#define GE_CMD(cmd, operand)                                                \
  *ge_cmd_ptr = (((GE_CMD_##cmd) << 24) | (operand));                       \
  ge_cmd_ptr++                                                              \

static void Ge_Finish_Callback(int id, void *arg)
{
}


u32 screen_flip = 0;

u32 video_out_mode;

u32 use_video_out;

#define PSP_OUT     0
#define ANALOG_OUT  1
#define DIGITAL_OUT 2

#define SCREEN_INTERLACE 2  /* on/off */
#define SCREEN_RATIO     2  /* 4:3/16:9 */
#define SCREEN_SCALE     5  /* non-scale/scale/full/etc 1/etc 2 */

SCREEN_PARAMETER screen_parameter_psp_game[SCREEN_SCALE];
SCREEN_PARAMETER screen_parameter_psp_menu;

SCREEN_PARAMETER screen_parameter_composite_game[SCREEN_RATIO][SCREEN_INTERLACE][SCREEN_SCALE];
SCREEN_PARAMETER screen_parameter_composite_menu[SCREEN_RATIO][SCREEN_INTERLACE];

SCREEN_PARAMETER screen_parameter_component_game[SCREEN_RATIO][SCREEN_INTERLACE][SCREEN_SCALE];
SCREEN_PARAMETER screen_parameter_component_menu[SCREEN_RATIO][SCREEN_INTERLACE];

SCREEN_PARAMETER *current_parameter;

typedef union
{
  float f;
  u32   i;
}IFLOAT;

void init_video()
{
  use_video_out = 0;
  video_out_mode = 0;
  video_draw_frame = FRAME_GAME;
  static u32 first_load = 0;

  if(first_load == 0)
  {
    if(load_video_config() == 0)
      use_video_out = 1;
    first_load = 1;
  }

  sceDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

  sceDisplayWaitVblankStart();
  // 5551 (0BBBBBGGGGGRRRRR) matches the GBA format, so no conversion is needed
  sceDisplaySetFrameBuf((void*)psp_gu_vram_base, FRAME_LINE_SIZE, PSP_DISPLAY_PIXEL_FORMAT_5551, PSP_DISPLAY_SETBUF_NEXTFRAME);

  sceGuInit();

  sceGuStart(GU_DIRECT, display_list);
  sceGuDrawBuffer(GU_PSM_5551, (void*)0, FRAME_LINE_SIZE);
  sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, (void*)0, FRAME_LINE_SIZE);
  sceGuClear(GU_COLOR_BUFFER_BIT);

  sceGuOffset(2048 - (PSP_SCREEN_WIDTH / 2), 2048 - (PSP_SCREEN_HEIGHT / 2));
  sceGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

  sceGuScissor(0, 0, PSP_SCREEN_WIDTH + 0, PSP_SCREEN_HEIGHT + 0);
  sceGuEnable(GU_SCISSOR_TEST);
  sceGuTexMode(GU_PSM_5551, 0, 0, GU_FALSE);
  sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
  sceGuTexFilter(GU_LINEAR, GU_LINEAR);
  sceGuEnable(GU_TEXTURE_2D);

  sceGuFrontFace(GU_CW);
  sceGuDisable(GU_BLEND);

  sceGuFinish();
  sceGuSync(0, 0);

  sceDisplayWaitVblankStart();
  sceGuDisplay(GU_TRUE);

  PspGeCallbackData gecb;
  gecb.signal_func = NULL;
  gecb.signal_arg = NULL;
  gecb.finish_func = Ge_Finish_Callback;
  gecb.finish_arg = NULL;
  gecbid = sceGeSetCallback(&gecb);

  memcpy(screen_vertex, &screen_parameter_psp_game_init, sizeof(float) * 20);
  // Set framebuffer to PSP VRAM
  GE_CMD(FBP, ((u32)psp_gu_vram_base & 0x00FFFFFF));
  GE_CMD(FBW, (((u32)psp_gu_vram_base & 0xFF000000) >> 8) | 768);
  // Set texture 0 to the screen texture
  GE_CMD(TBP0, ((u32)flip_address[0][0] & 0x00FFFFFF));
  GE_CMD(TBW0, (((u32)flip_address[0][0] & 0xFF000000) >> 8) | GBA_SCREEN_WIDTH);
  // Set the texture size to 256 by 256 (2^8 by 2^8)
  GE_CMD(TSIZE0, (9 << 8) | 9);
  // Flush the texture cache
  GE_CMD(TFLUSH, 0);
  // Use 2D coordinates, no indeces, no weights, 32bit float positions,
  // 32bit float texture coordinates
  GE_CMD(VTYPE, (1 << 23) | (0 << 11) | (0 << 9) | (3 << 7) | (0 << 5) | (0 << 2) | 3);
  // Set the base of the index list pointer to 0
  GE_CMD(BASE, 0);
  // Set the rest of index list pointer to 0 (not being used)
  GE_CMD(IADDR, 0);
  // Set the base of the screen vertex list pointer
  GE_CMD(BASE, ((u32)screen_vertex & 0xFF000000) >> 8);
  // Set the rest of the screen vertex list pointer
  GE_CMD(VADDR, ((u32)screen_vertex & 0x00FFFFFF));
  // Primitive kick: render sprite (primitive 6), 2 vertices
  GE_CMD(PRIM, (6 << 16) | 2);
  GE_CMD(PRIM, (6 << 16) | 2);
  // Done with commands
  GE_CMD(FINISH, 0);
  // Raise signal interrupt
  GE_CMD(SIGNAL, 0);
  GE_CMD(NOP, 0);
  GE_CMD(NOP, 0);
}

u32 current_scale = scaled_aspect;

u32 TBP0_temp[2];
u32 TBW0_temp[2];
u32 TSIZE0_temp;
u32 flip_address_temp[2];
u32 flip_offset_address_temp[2];

// Sharp bilinear (#74): crisp, square pixels at a non-integer scale, without
// the uneven pixel widths of nearest. Ported from GBAdhoc (GPL-2.0),
// gpsp/psp/video_psp.c sharp_draw(). The GE has no shaders, so it takes two
// passes per frame:
//  1. prescale: draw the GBA frame with nearest at an integer factor kx x ky
//     (k = the final scale rounded up) into a target in VRAM;
//  2. resample: draw the target with bilinear at the final size. Inside a
//     k x k block every tap reads the same color, so only the screen pixel
//     on the edge between two GBA pixels is blended.
// The target is the display buffer below the 272 rows the LCD shows (its
// stride is 768 for TV-out), so it needs no VRAM of its own. TV-out uses
// those rows, so there sharp bilinear draws plain bilinear.

#define SHARP_STRIDE 512
#define SHARP_OFFSET 0x68000  // byte offset in VRAM: display buffer row 277
#define SHARP_ROWS   ((PSP_DISPLAY_BUFFER_SIZE - SHARP_OFFSET) / (SHARP_STRIDE * 2))

#define GE_CMD_REGION1  0x15
#define GE_CMD_REGION2  0x16
#define GE_CMD_TFILTER  0xC6
#define GE_CMD_TSYNC    0xCC
#define GE_CMD_SCISSOR1 0xD4
#define GE_CMD_SCISSOR2 0xD5

static u32 __attribute__((aligned(16))) sharp_cmd[64];
static float __attribute__((aligned(16))) sharp_vertex[32 * 5];
static u32 *sharp_cmd_end;
static u32 *sharp_texture_slot;  // pass 1's TBP0, TBW0 and TSIZE0
static u32 sharp_active = 0;

#define SHARP_CMD(cmd, operand)                                             \
  *cmd_ptr = (((GE_CMD_##cmd) << 24) | ((operand) & 0xFFFFFF));             \
  cmd_ptr++                                                                 \

static float *sharp_sprite(float *v, float u1, float v1, float x1, float y1,
 float u2, float v2, float x2, float y2)
{
  v[0] = u1; v[1] = v1; v[2] = x1; v[3] = y1; v[4] = 0;
  v[5] = u2; v[6] = v2; v[7] = x2; v[8] = y2; v[9] = 0;
  return v + 10;
}

// Builds the two-pass list for the game picture sprite s. Returns the GE
// filter to use when sharp bilinear does not apply: nearest at an integer
// scale (sharp would look the same), bilinear when the target does not fit.
static u32 sharp_setup(SPRITE *s)
{
  float sw = fabsf(s->u2 - s->u1);
  float sh = fabsf(s->v2 - s->v1);
  float dw = fabsf(s->x2 - s->x1);
  float dh = fabsf(s->y2 - s->y1);
  u32 kx, ky, iw, ih, x, sprites;
  float *v = sharp_vertex;
  u32 *cmd_ptr = sharp_cmd;
  u32 view_w = current_parameter->view.width;
  u32 view_h = current_parameter->view.height;

  sharp_active = 0;

  if((sw != GBA_SCREEN_WIDTH) || (sh != GBA_SCREEN_HEIGHT) ||
   (dw <= 0) || (dh <= 0))
    return GU_LINEAR;

  if((fmodf(dw, sw) == 0) && (fmodf(dh, sh) == 0))
    return GU_NEAREST;

  kx = (u32)ceilf(dw / sw);
  ky = (u32)ceilf(dh / sh);
  iw = kx * GBA_SCREEN_WIDTH;
  ih = ky * GBA_SCREEN_HEIGHT;

  // +1: the duplicated last column and row
  if(((kx == 1) && (ky == 1)) || (iw + 1 > SHARP_STRIDE) || (ih + 1 > SHARP_ROWS))
    return GU_LINEAR;

  // Pass 1 vertices: 60-pixel strips (texture cache), then the last column
  // and row once more, one texel past the picture, so the bilinear taps on
  // the far edge read the edge color instead of stale VRAM.
  for(x = 0; x < GBA_SCREEN_WIDTH; x += 60)
  {
    v = sharp_sprite(v, x, 0, x * kx, 0, x + 60, GBA_SCREEN_HEIGHT,
     (x + 60) * kx, ih);
  }
  v = sharp_sprite(v, GBA_SCREEN_WIDTH - 1, 0, iw, 0,
   GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, iw + 1, ih);
  v = sharp_sprite(v, 0, GBA_SCREEN_HEIGHT - 1, 0, ih,
   GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, iw, ih + 1);
  v = sharp_sprite(v, GBA_SCREEN_WIDTH - 1, GBA_SCREEN_HEIGHT - 1, iw, ih,
   GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, iw + 1, ih + 1);
  sprites = (GBA_SCREEN_WIDTH / 60) + 3;

  SHARP_CMD(VTYPE, (1 << 23) | (3 << 7) | 3);
  SHARP_CMD(BASE, ((u32)sharp_vertex & 0xFF000000) >> 8);
  SHARP_CMD(VADDR, ((u32)sharp_vertex & 0x00FFFFFF));

  // Pass 1: the GBA frame (patched in by flip_screen), nearest, into the
  // target. The scissor and the drawing region open to the whole target.
  sharp_texture_slot = cmd_ptr;
  SHARP_CMD(TBP0, 0);
  SHARP_CMD(TBW0, 0);
  SHARP_CMD(TSIZE0, 0);
  SHARP_CMD(TFLUSH, 0);
  SHARP_CMD(TFILTER, (GU_NEAREST << 8) | GU_NEAREST);
  SHARP_CMD(FBP, SHARP_OFFSET);
  SHARP_CMD(FBW, SHARP_STRIDE);
  SHARP_CMD(SCISSOR1, 0);
  SHARP_CMD(SCISSOR2, (ih << 10) | iw);
  SHARP_CMD(REGION1, 0);
  SHARP_CMD(REGION2, (ih << 10) | iw);
  SHARP_CMD(PRIM, (6 << 16) | (sprites * 2));

  // Pass 2: back to the display buffer, the target with bilinear, in
  // 64-texel strips that share exact edges. The sprite's texture
  // coordinates scale by k, so the rotated screen option works too.
  // TSYNC: pass 1's pixels must be in VRAM before they are read.
  SHARP_CMD(TSYNC, 0);
  SHARP_CMD(FBP, ((u32)psp_gu_vram_base & 0x00FFFFFF));
  SHARP_CMD(FBW, (((u32)psp_gu_vram_base & 0xFF000000) >> 8) | FRAME_LINE_SIZE);
  SHARP_CMD(SCISSOR1, (current_parameter->view.y << 10) | current_parameter->view.x);
  SHARP_CMD(SCISSOR2, ((current_parameter->view.y + view_h - 1) << 10) |
   (current_parameter->view.x + view_w - 1));
  SHARP_CMD(REGION1, 0);
  SHARP_CMD(REGION2, ((current_parameter->screen_size.height - 1) << 10) |
   (current_parameter->screen_size.width - 1));
  SHARP_CMD(TBP0, (0x04000000 + SHARP_OFFSET) & 0x00FFFFFF);
  SHARP_CMD(TBW0, (((0x04000000 + SHARP_OFFSET) & 0xFF000000) >> 8) | SHARP_STRIDE);
  SHARP_CMD(TSIZE0, (9 << 8) | 9);
  SHARP_CMD(TFLUSH, 0);
  SHARP_CMD(TFILTER, (GU_LINEAR << 8) | GU_LINEAR);

  {
    float u1 = s->u1 * kx, u2 = s->u2 * kx;
    float v1 = s->v1 * ky, v2 = s->v2 * ky;
    float t0 = (u1 < u2) ? u1 : u2;
    float xa, xb, t, tb;

    sprites = 0;
    for(t = t0; t < t0 + iw; t += 64)
    {
      tb = (t + 64 < t0 + iw) ? (t + 64) : (t0 + iw);
      xa = s->x1 + (s->x2 - s->x1) * (t - u1) / (u2 - u1);
      xb = s->x1 + (s->x2 - s->x1) * (tb - u1) / (u2 - u1);
      if(xa <= xb)
        v = sharp_sprite(v, t, v1, xa, s->y1, tb, v2, xb, s->y2);
      else
        v = sharp_sprite(v, tb, v1, xb, s->y1, t, v2, xa, s->y2);
      sprites++;
    }
  }
  SHARP_CMD(PRIM, (6 << 16) | (sprites * 2));

  SHARP_CMD(FINISH, 0);
  SHARP_CMD(SIGNAL, 0);
  SHARP_CMD(NOP, 0);
  SHARP_CMD(NOP, 0);
  sharp_cmd_end = cmd_ptr;

  sharp_active = 1;
  return GU_LINEAR;
}

#ifdef UI4
/* In-game overlay (4.0 UI): a small picture over the running game, such
 * as the toast after a load. The owner draws into osd_pixels (OSD_W x
 * OSD_H, 5551, alpha bit 0 is see-through) and sets osd_tick, which
 * flip_screen() calls once a frame for the position; it returns 0 when the
 * overlay is done.
 *
 * The LCD shows the buffer the GE draws into, so a picture drawn after the
 * game sprite would be missing whenever the LCD scans those rows between
 * the two draws. The overlay goes first instead and marks its pixels in the
 * stencil (the alpha bit of the 5551 display); the game sprite then skips
 * marked pixels and clears the mark, so each overlay pixel is written once
 * a frame, already final. The first frame only clears stale marks. Parts of
 * the last position outside the game picture are cleared to black, since
 * the game sprite does not redraw the LCD there. */
#define GE_CMD_ATE  0x22
#define GE_CMD_STE  0x24
#define GE_CMD_ATST 0xDB
#define GE_CMD_STST 0xDC
#define GE_CMD_SOP  0xDD
#define STENCIL_KEEP    0
#define STENCIL_ZERO    1
#define STENCIL_REPLACE 2

u16 __attribute__((aligned(16))) osd_pixels[OSD_W * OSD_H];
int (*osd_tick)(int *x, int *y, int *w, int *h) = NULL;
static u16 __attribute__((aligned(16))) osd_black[8 * 8];
static u32 __attribute__((aligned(16))) osd_cmd[64];
static u32 __attribute__((aligned(16))) osd_end_cmd[8];
static float __attribute__((aligned(16))) osd_vertex[10 * 5];
static int osd_last[4];  /* x, y, w, h; w 0: nothing on the LCD */
static int osd_frames;   /* frames since the overlay started */
static int osd_active;   /* osd_before() enqueued a list this frame */
static u32 game_filter = GU_LINEAR;

static float *osd_sprite(float *v, float u1, float v1, float u2, float v2,
 int x, int y, int w, int h)
{
  v[0] = u1; v[1] = v1; v[2] = x;     v[3] = y;     v[4] = 0;
  v[5] = u2; v[6] = v2; v[7] = x + w; v[8] = y + h; v[9] = 0;
  return v + 10;
}

#define OSD_CMD(cmd, operand)                                               \
  *c++ = (((GE_CMD_##cmd) << 24) | ((operand) & 0xFFFFFF))                  \

#define OSD_TEXTURE(address, width, size)                                   \
  OSD_CMD(TBP0, (u32)(address) & 0x00FFFFFF);                               \
  OSD_CMD(TBW0, (((u32)(address) & 0xFF000000) >> 8) | (width));            \
  OSD_CMD(TSIZE0, (size));                                                  \
  OSD_CMD(TFLUSH, 0)                                                        \

#define OSD_END()                                                           \
  OSD_CMD(FINISH, 0);                                                       \
  OSD_CMD(SIGNAL, 0);                                                       \
  OSD_CMD(NOP, 0);                                                          \
  OSD_CMD(NOP, 0)                                                           \

/* Before the game sprite: the black bands, the overlay, and the stencil
 * test the game sprite runs with. */
static void osd_before(void)
{
  int now[4] = { 0, 0, 0, 0 }, on, i, n = 0, gx, gy, gw, gh;
  float *v = osd_vertex;
  u32 *c = osd_cmd;

  osd_active = 0;
  if((osd_tick == NULL) || (video_draw_frame != FRAME_GAME) || video_out_mode)
  {
    osd_frames = 0;
    return;
  }
  on = osd_tick(&now[0], &now[1], &now[2], &now[3]);
  if(!on)
  {
    osd_tick = NULL;
    now[2] = 0;
  }
  if(osd_frames++ == 0)
    now[2] = 0;  /* the first frame clears stale marks only */

  if(osd_black[0] == 0)
    for(i = 0; i < 8 * 8; i++)
      osd_black[i] = 0x8000;

  /* the last position outside the game picture: up to four black bands */
  if(osd_last[2] && memcmp(osd_last, now, sizeof(now)))
  {
    int x1 = osd_last[0], y1 = osd_last[1];
    int x2 = x1 + osd_last[2], y2 = y1 + osd_last[3];

    get_game_rect(&gx, &gy, &gw, &gh);
    if(y1 < gy)
    {
      v = osd_sprite(v, 0, 0, 8, 8, x1, y1, x2 - x1, ((y2 < gy) ? y2 : gy) - y1); n++;
    }
    if(y2 > gy + gh)
    {
      int t = (y1 > gy + gh) ? y1 : gy + gh;
      v = osd_sprite(v, 0, 0, 8, 8, x1, t, x2 - x1, y2 - t); n++;
    }
    {
      int t = (y1 > gy) ? y1 : gy, b = (y2 < gy + gh) ? y2 : gy + gh;
      if(b > t)
      {
        if(x1 < gx)
        {
          v = osd_sprite(v, 0, 0, 8, 8, x1, t, ((x2 < gx) ? x2 : gx) - x1, b - t); n++;
        }
        if(x2 > gx + gw)
        {
          int l = (x1 > gx + gw) ? x1 : gx + gw;
          v = osd_sprite(v, 0, 0, 8, 8, l, t, x2 - l, b - t); n++;
        }
      }
    }
  }
  if(now[2])
    v = osd_sprite(v, 0, 0, now[2], now[3], now[0], now[1], now[2], now[3]);

  OSD_CMD(VTYPE, (1 << 23) | (3 << 7) | 3);
  OSD_CMD(BASE, ((u32)osd_vertex & 0xFF000000) >> 8);
  OSD_CMD(VADDR, (u32)osd_vertex & 0x00FFFFFF);
  OSD_CMD(TFILTER, (GU_NEAREST << 8) | GU_NEAREST);
  OSD_CMD(STE, 1);
  if(n)
  {
    OSD_TEXTURE(osd_black, 8, (3 << 8) | 3);
    OSD_CMD(STST, GU_ALWAYS | (0 << 8) | (0xFF << 16));
    OSD_CMD(SOP, STENCIL_ZERO | (STENCIL_ZERO << 8) | (STENCIL_ZERO << 16));
    OSD_CMD(PRIM, (6 << 16) | (n * 2));
  }
  if(now[2])
  {
    /* the overlay's opaque pixels, marked */
    OSD_TEXTURE(osd_pixels, OSD_W, (5 << 8) | 9);
    OSD_CMD(ATE, 1);
    OSD_CMD(ATST, GU_GREATER | (0 << 8) | (0xFF << 16));
    OSD_CMD(STST, GU_ALWAYS | (1 << 8) | (0xFF << 16));
    OSD_CMD(SOP, STENCIL_KEEP | (STENCIL_KEEP << 8) | (STENCIL_REPLACE << 16));
    OSD_CMD(PRIM, (6 << 16) | 2);
    OSD_CMD(ATE, 0);
    /* the game sprite: not on marked pixels; every mark is cleared */
    OSD_CMD(STST, GU_NOTEQUAL | (1 << 8) | (0xFF << 16));
  }
  else
  {
    OSD_CMD(STST, GU_ALWAYS | (0 << 8) | (0xFF << 16));
  }
  OSD_CMD(SOP, STENCIL_ZERO | (STENCIL_ZERO << 8) | (STENCIL_ZERO << 16));
  OSD_CMD(TFILTER, (game_filter << 8) | game_filter);
  OSD_END();

  memcpy(osd_last, now, sizeof(now));
  osd_active = 1;
  sceKernelDcacheWritebackAll();
  sceGeListEnQueue(osd_cmd, c, gecbid, NULL);
}

/* After the game sprite: the stencil test off again. */
static void osd_after(void)
{
  u32 *c = osd_end_cmd;

  if(!osd_active)
    return;
  OSD_CMD(STE, 0);
  OSD_END();
  sceKernelDcacheWritebackAll();
  sceGeListEnQueue(osd_end_cmd, c, gecbid, NULL);
}
#else
#define osd_before()
#define osd_after()
#endif

void flip_screen()
{
  u32 *old_ge_cmd_ptr = ge_cmd_ptr;

  sceKernelDcacheWritebackAll();

  if(sharp_active)
  {
    sharp_texture_slot[0] = TBP0_temp[screen_flip];
    sharp_texture_slot[1] = TBW0_temp[screen_flip];
    sharp_texture_slot[2] = TSIZE0_temp;

    screen_flip ^= 1;
    screen_address = (u16 *)flip_address_temp[screen_flip];
    screen_offset_address = (u16 *)flip_offset_address_temp[screen_flip];

    sceKernelDcacheWritebackAll();

    osd_before();
    sceGeListEnQueue(sharp_cmd, sharp_cmd_end, gecbid, NULL);
    osd_after();
    return;
  }

  // Render the current screen
  ge_cmd_ptr = ge_cmd + 2;
  *ge_cmd_ptr = TBP0_temp[screen_flip];
  ge_cmd_ptr++;
  *ge_cmd_ptr = TBW0_temp[screen_flip];
  ge_cmd_ptr++;
  *ge_cmd_ptr = TSIZE0_temp;
  ge_cmd_ptr++;
  ge_cmd_ptr = old_ge_cmd_ptr;

  // Flip to the next screen
  screen_flip ^= 1;
  screen_address = (u16 *)flip_address_temp[screen_flip];
  screen_offset_address = (u16 *)flip_offset_address_temp[screen_flip];

  sceKernelDcacheWritebackAll();

  osd_before();
  sceGeListEnQueue(ge_cmd, ge_cmd_ptr, gecbid, NULL);
  osd_after();
}

static void set_resolution(u32 frame, u32 keep);

void video_resolution(u32 frame)
{
  set_resolution(frame, 0);
}

/* Switches like video_resolution() but without showing black frames: the
 * LCD keeps the last picture until the next flip_screen(), so a screen that
 * draws its own first frame (the 4.0 pause screen) opens and closes without
 * a blink. With a TV cable connected it keeps the black frames. (PSP-2000
 * and later all count as PSP_2000, and video.cfg ships with kai, so
 * use_video_out alone does not mean a TV is connected.) */
void video_resolution_keep(u32 frame)
{
  u32 tv = (psp_model == PSP_2000) && (use_video_out == 1) &&
   (pspDveMgrCheckVideoOut() != 0);

  set_resolution(frame, !tv);
}

static void set_resolution(u32 frame, u32 keep)
{
  static u32 first_load = 0;
  static SCREEN_PARAMETER *old_parameter = NULL;
  float x, y, cx, cy, xx, yy;
  u32 filter;
  SCREEN_PARAMETER sp; // TODO: rename

  if(first_load == 0)
  {
    first_load = 1;
    current_parameter = &screen_parameter_psp_menu;
  }
  else if(!keep)
  {
    flip_screen();
    clear_screen(0);
    flip_screen();
    clear_screen(0);
    flip_screen();
  }

  if(video_draw_frame != frame)
  {
    if((psp_model == PSP_2000) && (use_video_out == 1))
      video_out_mode = pspDveMgrCheckVideoOut();
    else
      video_out_mode = 0;

    switch(((video_out_mode << 1) | frame))
    {
      case ((PSP_OUT << 1) | FRAME_GAME):
        current_parameter = &screen_parameter_psp_game[g_gpsp_config.screen_mode];
        memcpy(&sp.screen_setting_1, &current_parameter->screen_setting_1, sizeof(float) * 20);

        x = (sp.screen_setting_1.x2 - sp.screen_setting_1.x1) / 2;
        y = (sp.screen_setting_1.y2 - sp.screen_setting_1.y1) / 2;
        cx = sp.screen_setting_1.x1 + x;
        cy = sp.screen_setting_1.y1 + y;
        xx = x * g_gpsp_config.screen_scale / 100;
        yy = y * g_gpsp_config.screen_scale / 100;
        sp.screen_setting_1.x1 = cx - xx;
        sp.screen_setting_1.x2 = cx + xx;
        sp.screen_setting_1.y1 = cy - yy;
        sp.screen_setting_1.y2 = cy + yy;
        break;
      case ((PSP_OUT << 1) | FRAME_MENU):
        current_parameter = &screen_parameter_psp_menu;
        memcpy(&sp.screen_setting_1, &current_parameter->screen_setting_1, sizeof(float) * 20);
        break;
      case ((ANALOG_OUT << 1) | FRAME_GAME):
        current_parameter = &screen_parameter_composite_game[g_gpsp_config.screen_ratio][g_gpsp_config.screen_interlace][g_gpsp_config.screen_mode];
        memcpy(&sp.screen_setting_1, &current_parameter->screen_setting_1, sizeof(float) * 20);
       break;
      case ((ANALOG_OUT << 1) | FRAME_MENU):
        current_parameter = &screen_parameter_composite_menu[g_gpsp_config.screen_ratio][g_gpsp_config.screen_interlace];
        memcpy(&sp.screen_setting_1, &current_parameter->screen_setting_1, sizeof(float) * 20);
        break;
      case ((DIGITAL_OUT << 1) | FRAME_GAME):
        current_parameter = &screen_parameter_component_game[g_gpsp_config.screen_ratio][g_gpsp_config.screen_interlace][g_gpsp_config.screen_mode];
        memcpy(&sp.screen_setting_1, &current_parameter->screen_setting_1, sizeof(float) * 20);
       break;
      case ((DIGITAL_OUT << 1) | FRAME_MENU):
        current_parameter = &screen_parameter_component_menu[g_gpsp_config.screen_ratio][g_gpsp_config.screen_interlace];
        memcpy(&sp.screen_setting_1, &current_parameter->screen_setting_1, sizeof(float) * 20);
        break;
    }

    if(old_parameter != current_parameter)
    {
      old_parameter = current_parameter;
      /* Setting the output again (the same LCD mode for game and menu)
         re-syncs the display, which blanks it for a moment. */
      if(keep)
        ;
      else if((psp_model == PSP_2000) && (use_video_out == 1))
      {
        pspDveMgrSetVideoOut(current_parameter->video_out.u, current_parameter->video_out.displaymode, current_parameter->video_out.width,
            current_parameter->video_out.height, current_parameter->video_out.x, current_parameter->video_out.y, current_parameter->video_out.z);
      }
      else
      {
        sceDisplaySetMode(0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
      }
    }

    memcpy(screen_vertex, &current_parameter->screen_setting_1, sizeof(float) * 20);
    memcpy(screen_vertex, &sp.screen_setting_1, sizeof(float) * 20);

    if(g_gpsp_config.screen_filter != filter_sharp_bilinear)
    {
      sharp_active = 0;
      filter = current_parameter->filter[g_gpsp_config.screen_filter];
    }
    else
    {
      if(((video_out_mode << 1) | frame) == ((PSP_OUT << 1) | FRAME_GAME))
        filter = sharp_setup(&sp.screen_setting_1);
      else
      {
        sharp_active = 0;
        filter = current_parameter->filter[filter_bilinear];
      }
    }

    video_draw_frame = frame;
    screen_address = (u16 *)flip_address[frame][0];
    screen_flip = 0;
    screen_pitch = current_parameter->texture_size.pitch;
    screen_width = current_parameter->texture_size.width;
    screen_height = current_parameter->texture_size.height;
    screen_width2 = screen_width / 2;
    screen_height2 = screen_height / 2;

    flip_address_temp[0] = flip_address[frame][0];
    flip_address_temp[1] = flip_address[frame][1];
    flip_offset_address_temp[0] = flip_offset_address[frame][0];
    flip_offset_address_temp[1] = flip_offset_address[frame][1];
    TBP0_temp[0] = ((GE_CMD_TBP0) << 24) | (flip_address_temp[0] & 0x00FFFFFF);
    TBP0_temp[1] = ((GE_CMD_TBP0) << 24) | (flip_address_temp[1] & 0x00FFFFFF);
    TBW0_temp[0] = ((GE_CMD_TBW0) << 24) | (((flip_address_temp[0] & 0xFF000000) >> 8) | screen_pitch);
    TBW0_temp[1] = ((GE_CMD_TBW0) << 24) | (((flip_address_temp[1] & 0xFF000000) >> 8) | screen_pitch);
    TSIZE0_temp  = ((GE_CMD_TSIZE0) << 24) | ((current_parameter->texture_bit.x << 8) | current_parameter->texture_bit.y);

    sceGuStart(GU_DIRECT, display_list);
    /* sceGuDispBuffer() calls sceDisplaySetMode(), which blanks the LCD for
       a moment; on the PSP's own screen the size never changes. */
    if(!keep)
      sceGuDispBuffer(current_parameter->screen_size.width, current_parameter->screen_size.height, (void*)0, FRAME_LINE_SIZE);
    sceGuOffset(2048 - (current_parameter->screen_size.width / 2), 2048 - (current_parameter->screen_size.height / 2));
    sceGuViewport(2048, 2048, current_parameter->screen_size.width, current_parameter->screen_size.height);
    sceGuScissor(current_parameter->view.x, current_parameter->view.y, current_parameter->view.width, current_parameter->view.height);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuTexFilter(filter, filter);
#ifdef UI4
    if(frame == FRAME_GAME)
      game_filter = filter;
#endif
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuFinish();
    sceGuSync(0, 0);

    if(!keep)
    {
      flip_screen();
      clear_screen(0);
      flip_screen();
      clear_screen(0);
      flip_screen();
    }
  }
}

void clear_screen(u16 color)
{
  u32 color32;
  color32 = (((color & 0x1F) * 8) << 0) | ((((color >> 5) & 0x1F) * 8) << 8) | ((((color >> 10) & 0x1F) * 8) << 16) | (0xFF << 24);

  sceGuStart(GU_DIRECT, display_list);
  sceGuDrawBufferList(GU_PSM_5551, (void *)screen_offset_address, screen_pitch);
  sceGuScissor(0, 0, current_parameter->texture_size.width, current_parameter->texture_size.height);
  // The clear color is always 32-bit, regardless of the pixel format
  sceGuClearColor(color32);
  sceGuClear(GU_COLOR_BUFFER_BIT);
  sceGuDrawBufferList(GU_PSM_5551, (void *)0, FRAME_LINE_SIZE);
  sceGuScissor(current_parameter->view.x, current_parameter->view.y, current_parameter->view.width, current_parameter->view.height);
  sceGuFinish();
  sceGuSync(0, 0);
}

/* Where video_resolution(FRAME_GAME) puts the game picture on the LCD. */
void get_game_rect(int *x, int *y, int *w, int *h)
{
  SPRITE *s = &screen_parameter_psp_game[g_gpsp_config.screen_mode].screen_setting_1;
  float hx = (s->x2 - s->x1) / 2, hy = (s->y2 - s->y1) / 2;
  float cx = s->x1 + hx, cy = s->y1 + hy;

  hx = hx * g_gpsp_config.screen_scale / 100;
  hy = hy * g_gpsp_config.screen_scale / 100;
  *x = (int)(cx - hx);
  *y = (int)(cy - hy);
  *w = (int)(hx * 2);
  *h = (int)(hy * 2);
}

u16 *copy_screen()
{
  u16 *copy = malloc(240 * 160 * 2);
  memcpy(copy, screen_address, 240 * 160 * 2);
  return copy;
}

void blit_to_screen(u16 *src, u32 w, u32 h, u32 dest_x, u32 dest_y)
{
  u32 pitch = screen_pitch;
  u16 *dest_ptr = screen_address + dest_x + (dest_y * pitch);

  u16 *src_ptr = src;
  u32 line_skip = pitch - w;
  u32 x, y;

  for(y = 0; y < h; y++)
  {
    for(x = 0; x < w; x++, src_ptr++, dest_ptr++)
    {
      *dest_ptr = *src_ptr;
    }
    dest_ptr += line_skip;
  }
}

#define video_savestate_body(type, ver)                                   \
{                                                                         \
  FILE_##type##_ARRAY(g_state_buffer_ptr, affine_reference_x);            \
  FILE_##type##_ARRAY(g_state_buffer_ptr, affine_reference_y);            \
}                                                                         \

void video_read_mem_savestate(u32 ver)
video_savestate_body(READ_MEM, ver);

void video_write_mem_savestate(u32 ver)
video_savestate_body(WRITE_MEM, ver);

#define LOAD_PARAMETER_NUMBER (SCREEN_SCALE + 1 + (SCREEN_RATIO*SCREEN_INTERLACE*SCREEN_SCALE + SCREEN_RATIO*SCREEN_INTERLACE) * 2)

s32 load_video_config()
{
  FILE *video_file;
  char current_line[256];
  u32 i,loop;
  int p[6][8];
  float f[2][10];
  SCREEN_PARAMETER parameter[LOAD_PARAMETER_NUMBER];

  memcpy(&screen_parameter_psp_menu, &screen_parameter_psp_menu_init, sizeof(SCREEN_PARAMETER));
  memcpy(&screen_parameter_psp_game, &screen_parameter_psp_game_init, sizeof(SCREEN_PARAMETER)*5);

  video_file = fopen(VIDEO_CONFIG_FILENAME, "r");

  if(video_file)
  {
    i = 0;
    loop = 0;
    while(fgets(current_line, 256, video_file))
    {
      if((current_line[0] !='#') && (current_line[0] != '\n') && (current_line[0] != '\r'))
      {
        switch(i)
        {
          case 0:
            sscanf(current_line, " %d, %i, %d, %d, %d, %d, %d, %d",
                &p[i][0],&p[i][1],&p[i][2],&p[i][3],&p[i][4],&p[i][5],&p[i][6],&p[i][7]);
            break;

          case 1 ... 5:
            sscanf(current_line, " %d, %d, %d, %d",
                &p[i][0],&p[i][1],&p[i][2],&p[i][3]);
            break;

          case 6 ... 7:
            sscanf(current_line, " %f, %f, %f, %f, %f, %f, %f, %f, %f, %f",
                &f[i-6][0],&f[i-6][1],&f[i-6][2],&f[i-6][3],&f[i-6][4],&f[i-6][5],&f[i-6][6],&f[i-6][7],&f[i-6][8],&f[i-6][9]);
            break;
        }
        i++;
      }
      if(i == 8)
      {
        memcpy(&parameter[loop].video_out.u,         &p[0][0], sizeof(VIDEO_OUT_PARAMETER));
        memcpy(&parameter[loop].filter[0],           &p[1][0], sizeof(int) * 2);
        memcpy(&parameter[loop].texture_size.pitch,  &p[2][0], sizeof(TEXTURE_SIZE));
        memcpy(&parameter[loop].texture_bit.x,       &p[3][0], sizeof(TEXTURE_BIT));
        memcpy(&parameter[loop].screen_size.width,   &p[4][0], sizeof(SCREEN_SIZE));
        memcpy(&parameter[loop].view.x,              &p[5][0], sizeof(VIEW_PORT));
        memcpy(&parameter[loop].screen_setting_1.u1, &f[0][0], sizeof(SPRITE));
        memcpy(&parameter[loop].screen_setting_2.u1, &f[1][0], sizeof(SPRITE));
        i = 0;
        loop++;
      }
      if(loop == LOAD_PARAMETER_NUMBER)
      {
        memcpy(&screen_parameter_psp_menu,       &parameter[0],  sizeof(SCREEN_PARAMETER));
        memcpy(&screen_parameter_composite_menu, &parameter[1],  sizeof(SCREEN_PARAMETER)*2*2);
        memcpy(&screen_parameter_component_menu, &parameter[5],  sizeof(SCREEN_PARAMETER)*2*2);
        memcpy(&screen_parameter_psp_game,       &parameter[9],  sizeof(SCREEN_PARAMETER)*5);
        memcpy(&screen_parameter_composite_game, &parameter[14], sizeof(SCREEN_PARAMETER)*2*2*5);
        memcpy(&screen_parameter_component_game, &parameter[34], sizeof(SCREEN_PARAMETER)*2*2*5);
        return 0;
      }
    }
  }

  return -1;
}
