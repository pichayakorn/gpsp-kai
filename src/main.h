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

#ifndef MAIN_H
#define MAIN_H

typedef enum
{
  TIMER_INACTIVE,
  TIMER_PRESCALE,
  TIMER_CASCADE
} TIMER_STATUS_TYPE;

typedef enum
{
  TIMER_NO_IRQ,
  TIMER_TRIGGER_IRQ
} TIMER_IRQ_TYPE;

typedef enum
{
  TIMER_DS_CHANNEL_NONE,
  TIMER_DS_CHANNEL_A,
  TIMER_DS_CHANNEL_B,
  TIMER_DS_CHANNEL_BOTH
} TIMER_DS_CHANNEL_TYPE;

typedef struct
{
  s32 count;
  u32 reload;
  u32 prescale;
  u32 stop_cpu_ticks; /* NOT USE */
  FIXED16_16 frequency_step;
  TIMER_DS_CHANNEL_TYPE direct_sound_channels;
  TIMER_IRQ_TYPE irq;
  TIMER_STATUS_TYPE status;
} TIMER_TYPE;

typedef enum
{
  auto_frameskip,
  manual_frameskip,
  no_frameskip
} FRAMESKIP_TYPE;

typedef enum
{
  PSP_1000, /* PSP-1000 all CFW or PSP-2000*/
  PSP_2000, /* PSP-2000 CFW 3.71 M33-3 or higher */
} MODEL_TYPE;

// TODO: revisit timer counter handling

// Set the reload counter value (not loaded into the timer yet).
// It is loaded into the counter when the timer starts,
// except on 32-bit access, where it is loaded immediately.
// Hardware range is 0-0xFFFF; gpSP uses (0x10000-n) << prescale (0,6,8,10).
// The value is shifted by prescale when reloaded into each counter.
// TODO: 32-bit and 8/16-bit accesses need separate handling.
// Maybe no need to call this on 8/16-bit access?
#define COUNT_TIMER(timer_number)                                             \
  timer[timer_number].reload = 0x10000 - value;                               \
  if(timer_number < 2)                                                        \
  {                                                                           \
    u32 timer_reload =                                                        \
     timer[timer_number].reload;              \
    SOUND_UPDATE_FREQUENCY_STEP(timer_number);                                \
  }                                                                           \

// TODO: needs tuning
#define ADJUST_SOUND_BUFFER(timer_number, channel)                            \
  if(timer[timer_number].direct_sound_channels & (0x01 << channel))           \
  {                                                                           \
    direct_sound_channel[channel].buffer_index = gbc_sound_buffer_index;      \
  }                                                                           \

#define TRIGGER_TIMER(timer_number)                                           \
  if(value & 0x80)                                                            \
  {                                                                           \
    if(timer[timer_number].status == TIMER_INACTIVE)                          \
    {                                                                         \
                                                                              \
      u32 timer_reload = timer[timer_number].reload;                          \
                                                                              \
      if(((value >> 2) & 0x01) && (timer_number != 0))                        \
      {                                                                       \
        timer[timer_number].status = TIMER_CASCADE;                           \
        timer[timer_number].prescale = 0;                                     \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        timer[timer_number].status = TIMER_PRESCALE;                          \
        u32 prescale = prescale_table[value & 0x03];                          \
        timer[timer_number].prescale = prescale;                              \
      }                                                                       \
                                                                              \
      timer[timer_number].irq = (value >> 6) & 0x01;                          \
                                                                              \
      timer[timer_number].count = timer_reload << timer[timer_number].prescale; \
      ADDRESS16(io_registers, 0x100 + (timer_number * 4)) =                   \
      0x10000 - timer_reload;                                                 \
                                                                              \
      if(timer[timer_number].count < g_execute_cycles)                        \
        g_execute_cycles = timer[timer_number].count;                         \
                                                                              \
      if(timer_number < 2)                                                    \
      {                                                                       \
        /* Avoid truncating the fraction; same handling as GBC sound */       \
        SOUND_UPDATE_FREQUENCY_STEP(timer_number);                            \
        ADJUST_SOUND_BUFFER(timer_number, 0);                                 \
        ADJUST_SOUND_BUFFER(timer_number, 1);                                 \
      }                                                                       \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    if(timer[timer_number].status != TIMER_INACTIVE)                          \
    {                                                                         \
      timer[timer_number].status = TIMER_INACTIVE;                            \
    }                                                                         \
  }                                                                           \
  ADDRESS16(io_registers, 0x102 + (timer_number * 4)) = value;                \

extern u32 g_execute_cycles;
//extern u32 global_cycles_per_instruction;
extern u32 g_synchronize_flag;
extern u32 skip_next_frame_flag;
extern TIMER_TYPE timer[4];
extern u32 prescale_table[];
extern char main_path[MAX_PATH];
extern char rom_path[MAX_PATH];
extern volatile u32 real_frame_count;
extern u32 virtual_frame_count;
extern int date_format;
extern MODEL_TYPE psp_model;
extern char *lang[12];
extern u32 g_use_home;

void set_cpu_clock(u32 clock);
u32 update_gba();
void reset_gba();
void synchronize();
#ifdef GAME_BENCH
void game_bench_arm(void);
extern u32 game_bench_lookups;
#endif
void quit(u32 mode);
void game_name_ext(u8 *src, u8 *buffer, u8 *extension);
void main_read_mem_savestate(u32 ver);
void main_write_mem_savestate(u32 ver);
void main_get_size_savestate(u32 ver);

void error_msg(char *text);
void set_cpu_mode(CPU_MODE_TYPE new_mode);
u32 *user_reg_ptr(u32 index);
u32 user_reg_load(u32 index);
void user_reg_store(u32 value, u32 index);
void raise_interrupt(IRQ_TYPE irq_raised);
void change_ext(char *src, char *buffer, char *extension);
u32 file_length(const char *filename);
int path_printf(char *dst, size_t size, const char *fmt, ...)
  __attribute__((format(__printf__, 3, 4)));
MODEL_TYPE get_model();

#endif /* MAIN_H */

