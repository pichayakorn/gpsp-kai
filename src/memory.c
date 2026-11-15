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

#include "common.h"

#define SAVESTATE_SIZE 506951
#define SAVESTATE_SIZE_OLD 506947

const char g_state_str[4] = "SVS";
const u32 g_state_ver = 0x00010000;

// TODO: PSP-1000 only needs 256 KB for the framebuffer, so use VRAM
u8 savestate_write_buffer[512 * 1024];
u8 *g_state_buffer_ptr;
u32 g_state_size;


typedef enum
{
  FLASH_DEVICE_MACRONIX_64KB   = 0x1C,
  FLASH_DEVICE_AMTEL_64KB      = 0x3D,
  FLASH_DEVICE_SST_64K         = 0xD4,
  FLASH_DEVICE_PANASONIC_64KB  = 0x1B,
  FLASH_DEVICE_MACRONIX_128KB  = 0x09
} FLASH_DEVICE_ID_TYPE;

typedef enum
{
  FLASH_MANUFACTURER_MACRONIX  = 0xC2,
  FLASH_MANUFACTURER_AMTEL     = 0x1F,
  FLASH_MANUFACTURER_PANASONIC = 0x32,
  FLASH_MANUFACTURER_SST       = 0xBF
} FLASH_MANUFACTURER_ID_TYPE;



void memory_read_mem_savestate(u32 ver);
void memory_write_mem_savestate(u32 ver);
void memopy_get_size_savestate(u32 ver);

u8 read_backup(u32 address);
void write_eeprom(u32 address, u32 value);
u32 read_eeprom();
CPU_ALERT_TYPE write_io_register8(u32 address, u32 value);
CPU_ALERT_TYPE write_io_register16(u32 address, u32 value);
CPU_ALERT_TYPE write_io_register32(u32 address, u32 value);
void write_backup(u32 address, u32 value);
u32 encode_bcd(u8 value);
void write_cart_io(u32 address, u32 value);
u32 save_backup(char *name);
s32 parse_config_line(char *current_line, char *current_variable, char *current_value);
s32 load_game_config(char *gamepak_title, char *gamepak_code, char *gamepak_maker);
char *skip_spaces(char *line_ptr);
s32 load_gamepak_raw(char *name);
u32 evict_gamepak_page();
void init_memory_gamepak();

// SIO
u32 get_sio_mode(u16 io1, u16 io2);
u32 send_adhoc_multi();

u32 g_multi_mode;


#define SIO_NORMAL8     0
#define SIO_NORMAL32    1
#define SIO_MULTIPLAYER 2
#define SIO_UART        3
#define SIO_JOYBUS      4
#define SIO_GP          5

// This table is configured for sequential access on system defaults
#ifdef OLD_COUNT
u32 waitstate_cycles_sequential[16][3] =
{
  { 1, 1, 1 }, // BIOS
  { 1, 1, 1 }, // Invalid
  { 3, 3, 6 }, // EWRAM (default settings)
  { 1, 1, 1 }, // IWRAM
  { 1, 1, 1 }, // IO Registers
  { 1, 1, 2 }, // Palette RAM
  { 1, 1, 2 }, // VRAM
  { 1, 1, 2 }, // OAM
  { 3, 3, 6 }, // Gamepak (wait 0)
  { 3, 3, 6 }, // Gamepak (wait 0)
  { 5, 5, 9 }, // Gamepak (wait 1)
  { 5, 5, 9 }, // Gamepak (wait 1)
  { 9, 9, 17 }, // Gamepak (wait 2)
  { 9, 9, 17 }  // Gamepak (wait 2)
};

u32 gamepak_waitstate_sequential[2][3][3] =
{
  {
    { 3, 3, 6 },
    { 5, 5, 9 },
    { 9, 9, 17 }
  },
  {
    { 2, 2, 3 },
    { 2, 2, 3 },
    { 2, 2, 3 }
  }
};
#else
u8 waitstate_cycles_seq[2][16] =
{
 /* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
  { 1, 1, 3, 1, 1, 1, 1, 1, 3, 3, 5, 5, 9, 9, 5, 1 }, /* 8,16bit */
  { 1, 1, 6, 1, 1, 2, 2, 1, 5, 5, 9, 9,17,17, 1, 1 }  /* 32bit */
};

u8 waitstate_cycles_non_seq[2][16] =
{
 /* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
  { 1, 1, 3, 1, 1, 1, 1, 1, 5, 5, 5, 5, 5, 5, 5, 1 }, /* 8,16bit */
  { 1, 1, 6, 1, 1, 2, 2, 1, 7, 7, 9, 9,13,13, 1, 1 }  /* 32bit */
};

// Different settings for gamepak ws0-2 sequential (2nd) access

u8 gamepak_waitstate_seq[2][2][3] =
{
  {{ 3, 5, 9 }, { 5, 9,17 }},
  {{ 2, 2, 2 }, { 3, 3, 3 }}
};

u8 cpu_waitstate_cycles_seq[2][16] =
{
 /* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
  { 1, 1, 3, 1, 1, 1, 1, 1, 3, 3, 5, 5, 9, 9, 5, 1 }, /* 8,16bit */
  { 1, 1, 6, 1, 1, 2, 2, 1, 5, 5, 9, 9,17,17, 1, 1 }  /* 32bit */
};
#endif

// GBA ROM/RAM, 962 KB total
// Palette RAM, 1 KB
u16 palette_ram[512];
// OAM, 1 KB
u16 oam_ram[512];
// I/O registers, 32 KB
u16 io_registers[1024 * 16];
// EWRAM, 256 KB x2
u8 ewram[1024 * 256 * 2];
// IWRAM, 32 KB x2
u8 iwram[1024 * 32 * 2];
// VRAM 192kb
u8 vram[1024 * 96 * 2];
// BIOS ROM 32kb
u8 bios_rom[1024 * 32];
// SRAM/flash/EEPROM 128kb
u8 gamepak_backup[1024 * 128];
u8 *flash_bank_ptr = gamepak_backup;

u32 bios_read_protect;

// Keeps us knowing how much we have left.
u8 *gamepak_rom;
u32 gamepak_size;

#ifdef USE_EXT_MEM
u8 *gamepak_rom_resume;
#endif

DMA_TRANSFER_TYPE dma[4];

u8 *memory_regions[16];
u32 memory_limits[16];

typedef struct
{
  u32 page_timestamp;
  u32 physical_index;
} gamepak_swap_entry_type;

u32 gamepak_ram_buffer_size;
u32 gamepak_ram_pages;

char gamepak_title[13];
char gamepak_code[5];
char gamepak_maker[3];
char gamepak_filename[MAX_FILE];
char gamepak_filename_full_path[MAX_PATH];
u32 gamepak_crc32;

u32 mem_save_flag;

// Enough to map the gamepak RAM space.
gamepak_swap_entry_type *gamepak_memory_map;

// This is global so that it can be kept open for large ROMs to swap
// pages from, so there's no slowdown with opening and closing the file
// a lot.

FILE_TAG_TYPE gamepak_file_large = -1;

u32 direct_map_vram = 0;

// Writes to these respective locations should trigger an update
// so the related subsystem may react to it.

// If OAM is written to:
u32 oam_update = 1;

// If GBC audio is written to:
u32 gbc_sound_update = 0;

// If the GBC audio waveform is modified:
u32 gbc_sound_wave_update = 0;

// If the backup space is written (only update once this hits 0)
u32 backup_update = 0;

// Write out backup file this many cycles after the most recent
// backup write.
const u32 write_backup_delay = 10;

typedef enum
{
  BACKUP_SRAM,
  BACKUP_FLASH,
  BACKUP_EEPROM,
  BACKUP_NONE
} backup_type_type;

typedef enum
{
  SRAM_SIZE_32KB,
  SRAM_SIZE_64KB
} sram_size_type;

// Keep it 32KB until the upper 64KB is accessed, then make it 64KB.

backup_type_type backup_type = BACKUP_NONE;
sram_size_type sram_size = SRAM_SIZE_32KB;

typedef enum
{
  FLASH_BASE_MODE,
  FLASH_ERASE_MODE,
  FLASH_ID_MODE,
  FLASH_WRITE_MODE,
  FLASH_BANKSWITCH_MODE
} flash_mode_type;

typedef enum
{
  FLASH_SIZE_64KB,
  FLASH_SIZE_128KB
} flash_size_type;

flash_mode_type flash_mode = FLASH_BASE_MODE;
u32 flash_command_position = 0;

FLASH_DEVICE_ID_TYPE flash_device_id = FLASH_DEVICE_MACRONIX_64KB;
FLASH_MANUFACTURER_ID_TYPE flash_manufacturer_id =
 FLASH_MANUFACTURER_MACRONIX;
flash_size_type flash_size = FLASH_SIZE_64KB;

const u8 gba_md5[16] = { 0xA8, 0x60, 0xE8, 0xC0, 0xB6, 0xD5, 0x73, 0xD1, 0x91, 0xE4, 0xEC, 0x7D, 0xB1, 0xB1, 0xE4, 0xF6 };
const u8 nds_md5[16] = { 0x1C, 0x0D, 0x67, 0xDB, 0x9E, 0x12, 0x08, 0xB9, 0x5A, 0x15, 0x06, 0xB1, 0x68, 0x8A, 0x0A, 0xD6 };

u8 read_backup(u32 address)
{
  u8 value = 0;

  if(backup_type == BACKUP_EEPROM)
  {
    switch(address & 0x8f00)
    {
      case 0x8200:
        value = tilt_sensor_x & 255;
        break;
      case 0x8300:
        value = (tilt_sensor_x >> 8) | 0x80;
        break;
      case 0x8400:
        value = tilt_sensor_y & 255;
        break;
      case 0x8500:
        value = tilt_sensor_y >> 8;
        break;
    }
    return value;
  }

  if(backup_type == BACKUP_NONE)
    backup_type = BACKUP_SRAM;

  if(backup_type == BACKUP_SRAM)
  {
    value = gamepak_backup[address];
  }
  else

  if(flash_mode == FLASH_ID_MODE)
  {
    /* ID manufacturer type */
    if(address == 0x0000)
      value = flash_manufacturer_id;
    else

    /* ID device type */
    if(address == 0x0001)
      value = flash_device_id;
  }
  else
  {
    value = flash_bank_ptr[address];
  }
  return value;
}

#define read_backup8()                                                        \
  value = read_backup(address & 0xFFFF)                                       \

#define read_backup16()                                                       \
  value = 0                                                                   \

#define read_backup32()                                                       \
  value = 0                                                                   \


// EEPROM is 512 bytes by default; it is autodetecte as 8KB if
// 14bit address DMAs are made (this is done in the DMA handler).

typedef enum
{
  EEPROM_512_BYTE,
  EEPROM_8_KBYTE
} eeprom_size_type;

typedef enum
{
  EEPROM_BASE_MODE,
  EEPROM_READ_MODE,
  EEPROM_READ_HEADER_MODE,
  EEPROM_ADDRESS_MODE,
  EEPROM_WRITE_MODE,
  EEPROM_WRITE_ADDRESS_MODE,
  EEPROM_ADDRESS_FOOTER_MODE,
  EEPROM_WRITE_FOOTER_MODE
} eeprom_mode_type;


eeprom_size_type eeprom_size = EEPROM_512_BYTE;
eeprom_mode_type eeprom_mode = EEPROM_BASE_MODE;
u32 eeprom_address_length;
u32 eeprom_address = 0;
s32 eeprom_counter = 0;
u8 eeprom_buffer[8];


void write_eeprom(u32 address, u32 value)
{
  if(gamepak_size > 0x1FFFF00) // eeprom_v126 ?
  {                            // ROM is restricted to 8000000h-9FFFeFFh
    gamepak_size = 0x1FFFF00;  // (max.1FFFF00h bytes = 32MB minus 256 bytes)
  }

  switch(eeprom_mode)
  {
    case EEPROM_BASE_MODE:
      backup_type = BACKUP_EEPROM;
      eeprom_buffer[0] |= (value & 0x01) << (1 - eeprom_counter);
      eeprom_counter++;
      if(eeprom_counter == 2)
      {
        if(eeprom_size == EEPROM_512_BYTE)
          eeprom_address_length = 6;
        else
          eeprom_address_length = 14;

        eeprom_counter = 0;

        switch(eeprom_buffer[0] & 0x03)
        {
          case 0x02:
            eeprom_mode = EEPROM_WRITE_ADDRESS_MODE;
            break;

          case 0x03:
            eeprom_mode = EEPROM_ADDRESS_MODE;
            break;
        }
        ADDRESS16(eeprom_buffer, 0) = 0;
      }
      break;

    case EEPROM_ADDRESS_MODE:
    case EEPROM_WRITE_ADDRESS_MODE:
      eeprom_buffer[eeprom_counter / 8]
       |= (value & 0x01) << (7 - (eeprom_counter % 8));
      eeprom_counter++;
      if(eeprom_counter == eeprom_address_length)
      {
        if(eeprom_size == EEPROM_512_BYTE)
        {
          eeprom_address =
           (ADDRESS16(eeprom_buffer, 0) >> 2) * 8;
        }
        else
        {
          eeprom_address = (((u32)eeprom_buffer[1] >> 2) |
           ((u32)eeprom_buffer[0] << 6)) * 8;
        }

        ADDRESS16(eeprom_buffer, 0) = 0;
        eeprom_counter = 0;

        if(eeprom_mode == EEPROM_ADDRESS_MODE)
        {
          eeprom_mode = EEPROM_ADDRESS_FOOTER_MODE;
        }
        else
        {
          eeprom_mode = EEPROM_WRITE_MODE;
          memset(gamepak_backup + eeprom_address, 0, 8);
        }
      }
      break;

    case EEPROM_WRITE_MODE:
      gamepak_backup[eeprom_address + (eeprom_counter / 8)] |=
       (value & 0x01) << (7 - (eeprom_counter % 8));
      eeprom_counter++;
      if(eeprom_counter == 64)
      {
        backup_update = write_backup_delay;
        eeprom_counter = 0;
        eeprom_mode = EEPROM_WRITE_FOOTER_MODE;
      }
      break;

    case EEPROM_ADDRESS_FOOTER_MODE:
    case EEPROM_WRITE_FOOTER_MODE:
      eeprom_counter = 0;
      if(eeprom_mode == EEPROM_ADDRESS_FOOTER_MODE)
      {
        eeprom_mode = EEPROM_READ_HEADER_MODE;
      }
      else
      {
        eeprom_mode = EEPROM_BASE_MODE;
      }
      break;
    default:
      ;
      break;
  }
}

#define read_memory_gamepak(type)                                             \
  u32 gamepak_index = address >> 15;                                          \
  u8 *map = memory_map_read[gamepak_index];                                   \
                                                                              \
  if(map == NULL)                                                             \
    map = load_gamepak_page(gamepak_index & 0x3FF);                           \
                                                                              \
  value = ADDRESS##type(map, address & 0x7FFF)                                \

#define read_open8()                                                          \
  if(reg[REG_CPSR] & 0x20)                                                    \
    value = read_memory8(reg[REG_PC] + 2 + (address & 0x01));                 \
  else                                                                        \
    value = read_memory8(reg[REG_PC] + 4 + (address & 0x03))                  \

#define read_open16()                                                         \
  if(reg[REG_CPSR] & 0x20)                                                    \
    value = read_memory16(reg[REG_PC] + 2);                                   \
  else                                                                        \
    value = read_memory16(reg[REG_PC] + 4 + (address & 0x02))                 \

#define read_open32()                                                         \
  if(reg[REG_CPSR] & 0x20)                                                    \
  {                                                                           \
    u32 current_instruction = read_memory16(reg[REG_PC] + 2);                 \
    value = current_instruction | (current_instruction << 16);                \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    value = read_memory32(reg[REG_PC] + 4);                                   \
  }                                                                           \

u32 read_eeprom()
{
  u32 value;

  switch(eeprom_mode)
  {
    case EEPROM_BASE_MODE:
      value = 1;
      break;

    case EEPROM_READ_MODE:
      value = (gamepak_backup[eeprom_address + (eeprom_counter / 8)] >>
       (7 - (eeprom_counter % 8))) & 0x01;
      eeprom_counter++;
      if(eeprom_counter == 64)
      {
        eeprom_counter = 0;
        eeprom_mode = EEPROM_BASE_MODE;
      }
      break;

    case EEPROM_READ_HEADER_MODE:
      value = 0;
      eeprom_counter++;
      if(eeprom_counter == 4)
      {
        eeprom_mode = EEPROM_READ_MODE;
        eeprom_counter = 0;
      }
      break;

    default:
      value = 0;
      break;
  }

  return value;
}

#define read_memory(type)                                                     \
  switch(address >> 24)                                                       \
  {                                                                           \
    case 0x00:                                                                \
      /* BIOS */                                                              \
      if(reg[REG_PC] >= 0x4000)                                               \
      {                                                                       \
        if(address < 0x4000)                                                  \
        {                                                                     \
          value = ADDRESS##type(&bios_read_protect, address & 0x03);          \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          read_open##type();                                                  \
        }                                                                     \
      }                                                                       \
      else                                                                    \
        value = ADDRESS##type(bios_rom, address & 0x3FFF);                    \
      break;                                                                  \
                                                                              \
    case 0x02:                                                                \
      /* external work RAM */                                                 \
      address = (address & 0x7FFF) + ((address & 0x38000) << 1) + 0x8000;     \
      value = ADDRESS##type(ewram, address);                                  \
      break;                                                                  \
                                                                              \
    case 0x03:                                                                \
      /* internal work RAM */                                                 \
      value = ADDRESS##type(iwram, (address & 0x7FFF) + 0x8000);              \
      break;                                                                  \
                                                                              \
    case 0x04:                                                                \
      /* I/O registers */                                                     \
      if(address < 0x04000400)                                                \
      {                                                                       \
        value = ADDRESS##type(io_registers, address & 0x3FF);                 \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        read_open##type();                                                    \
      }                                                                       \
      break;                                                                  \
                                                                              \
    case 0x05:                                                                \
      /* palette RAM */                                                       \
      value = ADDRESS##type(palette_ram, address & 0x3FF);                    \
      break;                                                                  \
                                                                              \
    case 0x06:                                                                \
      /* VRAM */                                                              \
      if(address & 0x10000)                                                   \
      {                                                                       \
        value = ADDRESS##type(vram, address & 0x17FFF);                       \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        value = ADDRESS##type(vram, address & 0x1FFFF);                       \
      }                                                                       \
      break;                                                                  \
                                                                              \
    case 0x07:                                                                \
      /* OAM RAM */                                                           \
      value = ADDRESS##type(oam_ram, address & 0x3FF);                        \
      break;                                                                  \
                                                                              \
    case 0x08 ... 0x0C:                                                       \
      /* gamepak ROM */                                                       \
      if((address & 0x1FFFFFF) < gamepak_size)                                \
      {                                                                       \
        read_memory_gamepak(type);                                            \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        value = (address / 2) & 0xFFFF;                                       \
      }                                                                       \
      break;                                                                  \
                                                                              \
    case 0x0D:                                                                \
      if((address & 0x1FFFFFF) < gamepak_size)                                \
      {                                                                       \
        read_memory_gamepak(type);                                            \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        value = read_eeprom();                                                \
      }                                                                       \
                                                                              \
      break;                                                                  \
                                                                              \
    case 0x0E:                                                                \
    case 0x0F:                                                                \
      read_backup##type();                                                    \
      break;                                                                  \
                                                                              \
    case 0x20 ... 0xFF:                                                       \
      value = 0;                                                              \
      break;                                                                  \
                                                                              \
    default:                                                                  \
      read_open##type();                                                      \
      break;                                                                  \
  }                                                                           \

#define trigger_dma(dma_number)                                               \
  if(value & 0x8000)                                                          \
  {                                                                           \
    if(dma[dma_number].start_type == DMA_INACTIVE)                            \
    {                                                                         \
      u32 start_type = (value >> 12) & 0x03;                                  \
      u32 dest_address = ADDRESS32(io_registers, (dma_number * 12) + 0xB4) &  \
       0xFFFFFFF;                                                             \
                                                                              \
      dma[dma_number].dma_channel = dma_number;                               \
      dma[dma_number].source_address =                                        \
       ADDRESS32(io_registers, (dma_number * 12) + 0xB0) & 0xFFFFFFF;         \
      dma[dma_number].dest_address = dest_address;                            \
      dma[dma_number].source_direction = (value >>  7) & 0x03;                \
      dma[dma_number].repeat_type = (value >> 9) & 0x01;                      \
      dma[dma_number].start_type = start_type;                                \
      dma[dma_number].irq = (value >> 14) & 0x01;                             \
                                                                              \
      /* If it is sound FIFO DMA make sure the settings are a certain way */  \
      if((dma_number >= 1) && (dma_number <= 2) &&                            \
       (start_type == DMA_START_SPECIAL))                                     \
      {                                                                       \
        dma[dma_number].length_type = DMA_32BIT;                              \
        dma[dma_number].length = 4;                                           \
        dma[dma_number].dest_direction = DMA_FIXED;                           \
        if(dest_address == 0x40000A4)                                         \
          dma[dma_number].direct_sound_channel = DMA_DIRECT_SOUND_B;          \
        else                                                                  \
          dma[dma_number].direct_sound_channel = DMA_DIRECT_SOUND_A;          \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        u32 length =                                                          \
         ADDRESS16(io_registers, (dma_number * 12) + 0xB8);                   \
                                                                              \
        if((dma_number == 3) && ((dest_address >> 24) == 0x0D) &&             \
         ((length & 0x1F) == 17))                                             \
        {                                                                     \
          eeprom_size = EEPROM_8_KBYTE;                                       \
        }                                                                     \
                                                                              \
        if(dma_number < 3)                                                    \
          length &= 0x3FFF;                                                   \
                                                                              \
        if(length == 0)                                                       \
        {                                                                     \
          if(dma_number == 3)                                                 \
            length = 0x10000;                                                 \
          else                                                                \
            length = 0x04000;                                                 \
        }                                                                     \
                                                                              \
        dma[dma_number].length = length;                                      \
        dma[dma_number].length_type = (value >> 10) & 0x01;                   \
        dma[dma_number].dest_direction = (value >> 5) & 0x03;                 \
      }                                                                       \
                                                                              \
      ADDRESS16(io_registers, (dma_number * 12) + 0xBA) = value;              \
      if(start_type == DMA_START_IMMEDIATELY)                                 \
        return dma_transfer(dma + dma_number);                                \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dma[dma_number].start_type = DMA_INACTIVE;                                \
    dma[dma_number].direct_sound_channel = DMA_NO_DIRECT_SOUND;               \
    ADDRESS16(io_registers, (dma_number * 12) + 0xBA) = value;                \
  }                                                                           \

// configure game pak access timings
#define waitstate_control()                                                   \
{                                                                             \
  u8 i;                                                                       \
  u8 waitstate_table[4] = { 5, 4, 3, 9 };                                     \
                                                                              \
  waitstate_cycles_non_seq[0][0x0e] = waitstate_cycles_seq[0][0x0e]           \
   = waitstate_table[value & 0x03];                                           \
                                                                              \
  for(i = 0; i < 2; i++)                                                      \
  {                                                                           \
    waitstate_cycles_seq[i][0x08] = waitstate_cycles_seq[i][0x09]             \
     = gamepak_waitstate_seq[(value >> 4) & 0x01][i][0];                      \
    waitstate_cycles_seq[i][0x0A] = waitstate_cycles_seq[i][0x0B]             \
     = gamepak_waitstate_seq[(value >> 7) & 0x01][i][1];                      \
    waitstate_cycles_seq[i][0x0C] = waitstate_cycles_seq[i][0x0D]             \
     = gamepak_waitstate_seq[(value >> 10) & 0x01][i][2];                     \
  }                                                                           \
                                                                              \
  waitstate_cycles_non_seq[0][0x08] = waitstate_cycles_non_seq[0][0x09]       \
   = waitstate_table[(value >> 2) & 0x03];                                    \
  waitstate_cycles_non_seq[0][0x0A] = waitstate_cycles_non_seq[0][0x0B]       \
   = waitstate_table[(value >> 5) & 0x03];                                    \
  waitstate_cycles_non_seq[0][0x0C] = waitstate_cycles_non_seq[0][0x0D]       \
   = waitstate_table[(value >> 8) & 0x03];                                    \
                                                                              \
  /* 32bit access ( split into two 16bit accsess ) */                         \
  waitstate_cycles_non_seq[1][0x08] = waitstate_cycles_non_seq[1][0x09]       \
   = (waitstate_cycles_non_seq[0][0x08] + waitstate_cycles_seq[0][0x08] - 1); \
  waitstate_cycles_non_seq[1][0x0A] = waitstate_cycles_non_seq[1][0x0B]       \
   = (waitstate_cycles_non_seq[0][0x0A] + waitstate_cycles_seq[0][0x0A] - 1); \
  waitstate_cycles_non_seq[1][0x0C] = waitstate_cycles_non_seq[1][0x0D]       \
  =  (waitstate_cycles_non_seq[0][0x0C] + waitstate_cycles_seq[0][0x0C] - 1); \
                                                                              \
  /* gamepak prefetch */                                                      \
  if(value & 0x4000)                                                          \
  {                                                                           \
    for(i = 0x08; i <= 0x0D; i++)                                             \
    {                                                                         \
      cpu_waitstate_cycles_seq[0][i] = 1;                                     \
      cpu_waitstate_cycles_seq[1][i] = 2;                                     \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    for(i = 0; i < 2; i++)                                                    \
    {                                                                         \
      cpu_waitstate_cycles_seq[i][0x08] = cpu_waitstate_cycles_seq[i][0x09]   \
       = waitstate_cycles_seq[i][0x08];                                       \
      cpu_waitstate_cycles_seq[i][0x0A] = cpu_waitstate_cycles_seq[i][0x0B]   \
       = waitstate_cycles_seq[i][0x0A];                                       \
      cpu_waitstate_cycles_seq[i][0x0C] = cpu_waitstate_cycles_seq[i][0x0D]   \
       = waitstate_cycles_seq[i][0x0C];                                       \
    }                                                                         \
  }                                                                           \
                                                                              \
  ADDRESS16(io_registers, 0x204) =                                            \
   (ADDRESS16(io_registers, 0x204) & 0x8000) | (value & 0x7FFF);              \
}                                                                             \

#define access_register8_high(address)                                        \
  value = (value << 8) | (ADDRESS8(io_registers, address))                    \

#define access_register8_low(address)                                         \
  value = ((ADDRESS8(io_registers, address + 1)) << 8) | value                \

#define access_register16_high(address)                                       \
  value = (value << 16) | (ADDRESS16(io_registers, address))                  \

#define access_register16_low(address)                                        \
  value = ((ADDRESS16(io_registers, address + 2)) << 16) | value              \

CPU_ALERT_TYPE write_io_register8(u32 address, u32 value)
{
  switch(address)
  {
    case 0x00:
      {
        u32 dispcnt = io_registers[REG_DISPCNT];

        if((value & 0x07) != (dispcnt & 0x07))
          oam_update = 1;

        ADDRESS8(io_registers, 0x00) = value;
      }
      break;

    // DISPSTAT (lower byte)
    case 0x04:
      ADDRESS8(io_registers, 0x04) =
       (ADDRESS8(io_registers, 0x04) & 0x07) | (value & ~0x07);
      break;

    // VCOUNT
    case 0x06:
    case 0x07:
      break;

    // BG2 reference X
    case 0x28:
      access_register8_low(0x28);
      access_register16_low(0x28);
      affine_reference_x[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x28) = value;
      break;

    case 0x29:
      access_register8_high(0x28);
      access_register16_low(0x28);
      affine_reference_x[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x28) = value;
      break;

    case 0x2A:
      access_register8_low(0x2A);
      access_register16_high(0x28);
      affine_reference_x[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x28) = value;
      break;

    case 0x2B:
      access_register8_high(0x2A);
      access_register16_high(0x28);
      affine_reference_x[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x28) = value;
      break;

    // BG2 reference Y
    case 0x2C:
      access_register8_low(0x2C);
      access_register16_low(0x2C);
      affine_reference_y[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    case 0x2D:
      access_register8_high(0x2C);
      access_register16_low(0x2C);
      affine_reference_y[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    case 0x2E:
      access_register8_low(0x2E);
      access_register16_high(0x2C);
      affine_reference_y[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    case 0x2F:
      access_register8_high(0x2E);
      access_register16_high(0x2C);
      affine_reference_y[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    // BG3 reference X
    case 0x38:
      access_register8_low(0x38);
      access_register16_low(0x38);
      affine_reference_x[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x38) = value;
      break;

    case 0x39:
      access_register8_high(0x38);
      access_register16_low(0x38);
      affine_reference_x[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x38) = value;
      break;

    case 0x3A:
      access_register8_low(0x3A);
      access_register16_high(0x38);
      affine_reference_x[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x38) = value;
      break;

    case 0x3B:
      access_register8_high(0x3A);
      access_register16_high(0x38);
      affine_reference_x[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x38) = value;
      break;

    // BG3 reference Y
    case 0x3C:
      access_register8_low(0x3C);
      access_register16_low(0x3C);
      affine_reference_y[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    case 0x3D:
      access_register8_high(0x3C);
      access_register16_low(0x3C);
      affine_reference_y[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    case 0x3E:
      access_register8_low(0x3E);
      access_register16_high(0x3C);
      affine_reference_y[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    case 0x3F:
      access_register8_high(0x3E);
      access_register16_high(0x3C);
      affine_reference_y[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    // Sound 1 control sweep
    case 0x60:
      access_register8_low(0x60);
      GBC_SOUND_TONE_CONTROL_SWEEP();
      break;

    case 0x61:
      access_register8_low(0x60);
      GBC_SOUND_TONE_CONTROL_SWEEP();
      break;

    // Sound 1 control duty/length/envelope
    case 0x62:
      access_register8_low(0x62);
      GBC_SOUND_TONE_CONTROL_LOW(0, 0x62);
      break;

    case 0x63:
      access_register8_high(0x62);
      GBC_SOUND_TONE_CONTROL_LOW(0, 0x62);
      break;

    // Sound 1 control frequency
    case 0x64:
      access_register8_low(0x64);
      GBC_SOUND_TONE_CONTROL_HIGH(0, 0x64);
      break;

    case 0x65:
      access_register8_high(0x64);
      GBC_SOUND_TONE_CONTROL_HIGH(0, 0x64);
      break;

    // Sound 2 control duty/length/envelope
    case 0x68:
      access_register8_low(0x68);
      GBC_SOUND_TONE_CONTROL_LOW(1, 0x68);
      break;

    case 0x69:
      access_register8_high(0x68);
      GBC_SOUND_TONE_CONTROL_LOW(1, 0x68);
      break;

    // Sound 2 control frequency
    case 0x6C:
      access_register8_low(0x6C);
      GBC_SOUND_TONE_CONTROL_HIGH(1, 0x6C);
      break;

    case 0x6D:
      access_register8_high(0x6C);
      GBC_SOUND_TONE_CONTROL_HIGH(1, 0x6C);
      break;

    // Sound 3 control wave
    case 0x70:
      access_register8_low(0x70);
      GBC_SOUND_WAVE_CONTROL();
      break;

    case 0x71:
      access_register8_high(0x70);
      GBC_SOUND_WAVE_CONTROL();
      break;

    // Sound 3 control length/volume
    case 0x72:
      access_register8_low(0x72);
      GBC_SOUND_TONE_CONTROL_LOW_WAVE();
      break;

    case 0x73:
      access_register8_high(0x72);
      GBC_SOUND_TONE_CONTROL_LOW_WAVE();
      break;

    // Sound 3 control frequency
    case 0x74:
      access_register8_low(0x74);
      GBC_SOUND_TONE_CONTROL_HIGH_WAVE();
      break;

    case 0x75:
      access_register8_high(0x74);
      GBC_SOUND_TONE_CONTROL_HIGH_WAVE();
      break;

    // Sound 4 control length/envelope
    case 0x78:
      access_register8_low(0x78);
      GBC_SOUND_TONE_CONTROL_LOW(3, 0x78);
      break;

    case 0x79:
      access_register8_high(0x78);
      GBC_SOUND_TONE_CONTROL_LOW(3, 0x78);
      break;

    // Sound 4 control frequency
    case 0x7C:
      access_register8_low(0x7C);
      GBC_SOUND_NOISE_CONTROL();
      break;

    case 0x7D:
      access_register8_high(0x7C);
      GBC_SOUND_NOISE_CONTROL();
      break;

    // Sound control L
    case 0x80:
      access_register8_low(0x80);
      GBC_TRIGGER_SOUND();
      break;

    case 0x81:
      access_register8_high(0x80);
      GBC_TRIGGER_SOUND();
      break;

    // Sound control H
    case 0x82:
      access_register8_low(0x82);
      TRIGGER_SOUND();
      break;

    case 0x83:
      access_register8_high(0x82);
      TRIGGER_SOUND();
      break;

    // Sound control X
    case 0x84:
      SOUND_ON();
      break;

    // Sound wave RAM
    case 0x90 ... 0x9F:
      gbc_sound_wave_update = 1;
      ADDRESS8(io_registers, address) = value;
      break;

    // Sound FIFO A
    case 0xA0 ... 0xA3:
      ADDRESS8(io_registers, address) = value;
      sound_timer_queue32(0);
      break;

    // Sound FIFO B
    case 0xA4 ... 0xA7:
      ADDRESS8(io_registers, address) = value;
      sound_timer_queue32(1);
      break;

    // DMA control (trigger byte)
    case 0xBB:
      access_register8_high(0xBA);
      trigger_dma(0);
      break;

    case 0xC7:
      access_register8_high(0xC6);
      trigger_dma(1);
      break;

    case 0xD3:
      access_register8_high(0xD2);
      trigger_dma(2);
      break;

    case 0xDF:
      access_register8_high(0xDE);
      trigger_dma(3);
      break;

    // Timer counts
    case 0x100:
      access_register8_low(0x100);
      COUNT_TIMER(0);
      break;

    case 0x101:
      access_register8_high(0x100);
      COUNT_TIMER(0);
      break;

    case 0x104:
      access_register8_low(0x104);
      COUNT_TIMER(1);
      break;

    case 0x105:
      access_register8_high(0x104);
      COUNT_TIMER(1);
      break;

    case 0x108:
      access_register8_low(0x108);
      COUNT_TIMER(2);
      break;

    case 0x109:
      access_register8_high(0x108);
      COUNT_TIMER(2);
      break;

    case 0x10C:
      access_register8_low(0x10C);
      COUNT_TIMER(3);
      break;

    case 0x10D:
      access_register8_high(0x10C);
      COUNT_TIMER(3);
      break;

    // Timer control (trigger byte)
    case 0x103:
      access_register8_high(0x102);
      TRIGGER_TIMER(0);
      break;

    case 0x107:
      access_register8_high(0x106);
      TRIGGER_TIMER(1);
      break;

    case 0x10B:
      access_register8_high(0x10A);
      TRIGGER_TIMER(2);
      break;

    case 0x10F:
      access_register8_high(0x10E);
      TRIGGER_TIMER(3);
      break;

    case 0x128:
#ifdef USE_ADHOC
      if(g_adhoc_link_flag == NO)
        ADDRESS8(io_registers, 0x128) |= 0x0C;
#endif
      break;

    case 0x129:
      break;

    case 0x134:
      break;

    case 0x135:
      break;

    // P1
    case 0x130:
    case 0x131:
      /* Read only */
      break;

      // IE
      case 0x200:
        ADDRESS8(io_registers, 0x200) = value;
        break;

      case 0x201:
        ADDRESS8(io_registers, 0x201) = value;
        break;

    // IF
    case 0x202:
      ADDRESS8(io_registers, 0x202) &= ~value;
      break;

    case 0x203:
      ADDRESS8(io_registers, 0x203) &= ~value;
      break;

    // WAITCNT
    case 0x204:
#ifndef OLD_COUNT
      access_register8_low(0x204);
      waitstate_control();
#endif
      break;

    case 0x205:
#ifndef OLD_COUNT
      access_register8_high(0x204);
      waitstate_control();
#endif
      break;

    // Halt
    case 0x301:
      if(value & 0x80)
        reg[CPU_HALT_STATE] = CPU_STOP;
      else
        reg[CPU_HALT_STATE] = CPU_HALT;
      return CPU_ALERT_HALT;

    default:
      ADDRESS8(io_registers, address) = value;
      break;
  }

  return CPU_ALERT_NONE;
}

CPU_ALERT_TYPE write_io_register16(u32 address, u32 value)
{
  switch(address)
  {
    case 0x00:
    {
      u32 dispcnt = io_registers[REG_DISPCNT];
      if((value & 0x07) != (dispcnt & 0x07))
        oam_update = 1;

      ADDRESS16(io_registers, 0x00) = value;
      break;
    }

    // DISPSTAT
    case 0x04:
      ADDRESS16(io_registers, 0x04) =
       (ADDRESS16(io_registers, 0x04) & 0x07) | (value & ~0x07);
      break;

    // VCOUNT
    case 0x06:
      break;

    // BG2 reference X
    case 0x28:
      access_register16_low(0x28);
      affine_reference_x[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x28) = value;
      break;

    case 0x2A:
      access_register16_high(0x28);
      affine_reference_x[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x28) = value;
      break;

    // BG2 reference Y
    case 0x2C:
      access_register16_low(0x2C);
      affine_reference_y[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    case 0x2E:
      access_register16_high(0x2C);
      affine_reference_y[0] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    // BG3 reference X

    case 0x38:
      access_register16_low(0x38);
      affine_reference_x[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x38) = value;
      break;

    case 0x3A:
      access_register16_high(0x38);
      affine_reference_x[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x38) = value;
      break;

    // BG3 reference Y
    case 0x3C:
      access_register16_low(0x3C);
      affine_reference_y[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    case 0x3E:
      access_register16_high(0x3C);
      affine_reference_y[1] = (s32)(value << 4) >> 4;
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    // Sound 1 control sweep
    case 0x60:
      GBC_SOUND_TONE_CONTROL_SWEEP();
      break;

    // Sound 1 control duty/length/envelope
    case 0x62:
      GBC_SOUND_TONE_CONTROL_LOW(0, 0x62);
      break;

    // Sound 1 control frequency
    case 0x64:
      GBC_SOUND_TONE_CONTROL_HIGH(0, 0x64);
      break;

    // Sound 2 control duty/length/envelope
    case 0x68:
      GBC_SOUND_TONE_CONTROL_LOW(1, 0x68);
      break;

    // Sound 2 control frequency
    case 0x6C:
      GBC_SOUND_TONE_CONTROL_HIGH(1, 0x6C);
      break;

    // Sound 3 control wave
    case 0x70:
      GBC_SOUND_WAVE_CONTROL();
      break;

    // Sound 3 control length/volume
    case 0x72:
      GBC_SOUND_TONE_CONTROL_LOW_WAVE();
      break;

    // Sound 3 control frequency
    case 0x74:
      GBC_SOUND_TONE_CONTROL_HIGH_WAVE();
      break;

    // Sound 4 control length/envelope
    case 0x78:
      GBC_SOUND_TONE_CONTROL_LOW(3, 0x78);
      break;

    // Sound 4 control frequency
    case 0x7C:
      GBC_SOUND_NOISE_CONTROL();
      break;

    // Sound control L
    case 0x80:
      GBC_TRIGGER_SOUND();
      break;

    // Sound control H
    case 0x82:
      TRIGGER_SOUND();
      break;

    // Sound control X
    case 0x84:
      SOUND_ON();
      break;

    // Sound wave RAM
    case 0x90 ... 0x9E:
      gbc_sound_wave_update = 1;
      ADDRESS16(io_registers, address) = value;
      break;

    // Sound FIFO A
    case 0xA0:
    case 0xA2:
      ADDRESS16(io_registers, address) = value;
      sound_timer_queue32(0);
      break;

    // Sound FIFO B
    case 0xA4:
    case 0xA6:
      ADDRESS16(io_registers, address) = value;
      sound_timer_queue32(1);
      break;

    // DMA control
    case 0xBA:
      trigger_dma(0);
      break;

    case 0xC6:
      trigger_dma(1);
      break;

    case 0xD2:
      trigger_dma(2);
      break;

    case 0xDE:
      trigger_dma(3);
      break;

    // Timer counts
    case 0x100:
//      ADDRESS16(io_registers, address) = value;
      COUNT_TIMER(0);
      break;

    case 0x104:
//      ADDRESS16(io_registers, address) = value;
      COUNT_TIMER(1);
      break;

    case 0x108:
//      ADDRESS16(io_registers, address) = value;
      COUNT_TIMER(2);
      break;

    case 0x10C:
//      ADDRESS16(io_registers, address) = value;
      COUNT_TIMER(3);
      break;

    // Timer control
    case 0x102:
      TRIGGER_TIMER(0);
      break;

    case 0x106:
      TRIGGER_TIMER(1);
      break;

    case 0x10A:
      TRIGGER_TIMER(2);
      break;

    case 0x10E:
      TRIGGER_TIMER(3);
      break;

#ifdef USE_ADHOC
// SIOCNT
//      Bit   Expl.
//      0-1   Baud Rate          (0-3: 9600,38400,57600,115200 bps)
//      2     SI-Terminal        (0=Parent, 1=Child)                  (Read Only)
//      3     SD-Terminal        (0=Bad connection, 1=All GBAs Ready) (Read Only)

//      4-5   Multi-Player ID    (0=Parent, 1-3=1st-3rd child)        (Read Only)
//      6     Multi-Player Error (0=Normal, 1=Error)                  (Read Only)
//      7     Start/Busy Bit     (0=Inactive, 1=Start/Busy) (Read Only for Slaves)

//      8-11  Not used           (R/W, should be 0)

//      12    Must be "0" for Multi-Player mode
//      13    Must be "1" for Multi-Player mode
//      14    IRQ Enable         (0=Disable, 1=Want IRQ upon completion)
//      15    Not used           (Read only, always 0)
    case 0x128:
      if(g_adhoc_link_flag == NO)
        ADDRESS16(io_registers, 0x128) |= 0x0C;
      else
      {
        switch(get_sio_mode(value, ADDRESS16(io_registers, 0x134)))
        {
          case SIO_MULTIPLAYER:
            if(value & 0x80) // bit 7 (start bit) set: begin transfer
            {
              if(!g_multi_id)  // parent: g_multi_id == 0
              {
                if(!g_adhoc_transfer_flag) // no transfer in progress
                {
                  g_multi_mode = MULTI_START;
                }
                value |= (g_adhoc_transfer_flag != 0)<<7;
              }
            }
            if(g_multi_id)
            {
              value &= 0xf00b;
              value |= 0x8;
              value |= g_multi_id << 4;
              ADDRESS16(io_registers, 0x128) = value;
              ADDRESS16(io_registers, 0x134) = 7; // parent and child use different RCNT values
            }
            else
            {
              value &= 0xf00f;
              value |= 0xc;
              value |= g_multi_id << 4;
              ADDRESS16(io_registers, 0x128) = value;
              ADDRESS16(io_registers, 0x134) = 3;
            }
            break;
        }
      }

    // SIOMLT_SEND
    case 0x12A:
      ADDRESS16(io_registers, 0x12A) = value;
      break;

    // RCNT
    case 0x134:
      if(!value)
      {
        ADDRESS16(io_registers, 0x134) = 0;
        return CPU_ALERT_NONE;
      }
      switch(get_sio_mode(ADDRESS16(io_registers, 0x128), value))
      {
        case SIO_MULTIPLAYER:
          value &= 0xc0f0;
          value |= 3;
          if(g_multi_id) value |= 4;
          ADDRESS16(io_registers, 0x134) = value;
          ADDRESS16(io_registers, 0x128) = ((ADDRESS16(io_registers, 0x128)&0xff8b)|(g_multi_id ? 0xc : 8)|(g_multi_id<<4));
          break;

        default:
          ADDRESS16(io_registers, 0x134) = value;
          break;
      }
      break;
#else
    case 0x128:
      ADDRESS16(io_registers, 0x128) |= 0x0C;
      break;
#endif

    // IE
    case 0x200:
      ADDRESS16(io_registers, 0x200) = value;
      break;

    // Interrupt flag
    case 0x202:
      ADDRESS16(io_registers, 0x202) &= ~value;
      break;

    // WAITCNT
    case 0x204:
#ifndef OLD_COUNT
      waitstate_control();
#endif
      break;

    // Halt
    case 0x300:
      if(value & 0x8000)
        reg[CPU_HALT_STATE] = CPU_STOP;
      else
        reg[CPU_HALT_STATE] = CPU_HALT;

      return CPU_ALERT_HALT;

    default:
      ADDRESS16(io_registers, address) = value;
      break;
  }

  return CPU_ALERT_NONE;
}


CPU_ALERT_TYPE write_io_register32(u32 address, u32 value)
{
  switch(address)
  {
    // BG2 reference X
    case 0x28:
      affine_reference_x[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x28) = value;
      break;

    // BG2 reference Y
    case 0x2C:
      affine_reference_y[0] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x2C) = value;
      break;

    // BG3 reference X
    case 0x38:
      affine_reference_x[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x38) = value;
      break;

    // BG3 reference Y
    case 0x3C:
      affine_reference_y[1] = ((s32)(value << 4) >> 4);
      ADDRESS32(io_registers, 0x3C) = value;
      break;

    // Sound FIFO A
    case 0xA0:
      ADDRESS32(io_registers, 0xA0) = value;
      sound_timer_queue32(0);
      break;

    // Sound FIFO B
    case 0xA4:
      ADDRESS32(io_registers, 0xA4) = value;
      sound_timer_queue32(1);
      break;

    default:
    {
      CPU_ALERT_TYPE alert_low =
        write_io_register16(address, value & 0xFFFF);

      CPU_ALERT_TYPE alert_high =
        write_io_register16(address + 2, value >> 16);

//      if(alert_high)
//        return alert_high;

      return alert_high | alert_low;
    }
  }

  return CPU_ALERT_NONE;
}

#define write_palette8(address, value)                                        \
  ADDRESS16(palette_ram, (address & ~0x01)) = ((value << 8) | value)          \

#define write_palette16(address, value)                                       \
  ADDRESS16(palette_ram, address) = value                                     \

#define write_palette32(address, value)                                       \
  ADDRESS32(palette_ram, address) = value                                     \


void write_backup(u32 address, u32 value)
{
  value &= 0xFF;

  if(backup_type == BACKUP_NONE)
    backup_type = BACKUP_SRAM;


  // gamepak SRAM or Flash ROM
  if((address == 0x5555) && (flash_mode != FLASH_WRITE_MODE))
  {
    if((flash_command_position == 0) && (value == 0xAA))
    {
      backup_type = BACKUP_FLASH;
      flash_command_position = 1;
    }

    if(flash_command_position == 2)
    {
      switch(value)
      {
        case 0x90:
          // Enter ID mode, this also tells the emulator that we're using
          // flash, not SRAM

          if(flash_mode == FLASH_BASE_MODE)
            flash_mode = FLASH_ID_MODE;

          break;

        case 0x80:
          // Enter erase mode
          if(flash_mode == FLASH_BASE_MODE)
            flash_mode = FLASH_ERASE_MODE;
          break;

        case 0xF0:
          // Terminate ID mode
          if(flash_mode == FLASH_ID_MODE)
            flash_mode = FLASH_BASE_MODE;
          break;

        case 0xA0:
          // Write mode
          if(flash_mode == FLASH_BASE_MODE)
            flash_mode = FLASH_WRITE_MODE;
          break;

        case 0xB0:
          // Bank switch
          // Here the chip is now officially 128KB.
          flash_size = FLASH_SIZE_128KB;
          if(flash_mode == FLASH_BASE_MODE)
            flash_mode = FLASH_BANKSWITCH_MODE;
          break;

        case 0x10:
          // Erase chip
          if(flash_mode == FLASH_ERASE_MODE)
          {
            if(flash_size == FLASH_SIZE_64KB)
              memset(gamepak_backup, 0xFF, 1024 * 64);
            else
              memset(gamepak_backup, 0xFF, 1024 * 128);
            backup_update = write_backup_delay;
            flash_mode = FLASH_BASE_MODE;
          }
          break;

        default:
          break;
      }
      flash_command_position = 0;
    }
    if(backup_type == BACKUP_SRAM)
      gamepak_backup[0x5555] = value;
  }
  else

  if((address == 0x2AAA) && (value == 0x55) &&
   (flash_command_position == 1))
  {
    flash_command_position = 2;
  }
  else
  {
    if((flash_command_position == 2) &&
     (flash_mode == FLASH_ERASE_MODE) && (value == 0x30))
    {
      // Erase sector
      memset(flash_bank_ptr + (address & 0xF000), 0xFF, 1024 * 4);
      backup_update = write_backup_delay;
      flash_mode = FLASH_BASE_MODE;
      flash_command_position = 0;
    }
    else

    if((flash_command_position == 0) &&
     (flash_mode == FLASH_BANKSWITCH_MODE) && (address == 0x0000) &&
     (flash_size == FLASH_SIZE_128KB))
    {
      flash_bank_ptr = gamepak_backup + ((value & 0x01) * (1024 * 64));
      flash_mode = FLASH_BASE_MODE;
    }
    else

    if((flash_command_position == 0) && (flash_mode == FLASH_WRITE_MODE))
    {
      // Write value to flash ROM
      backup_update = write_backup_delay;
      flash_bank_ptr[address] = value;
      flash_mode = FLASH_BASE_MODE;
    }
    else

    if(backup_type == BACKUP_SRAM)
    {
      // Write value to SRAM
      backup_update = write_backup_delay;
      // Hit 64KB territory?
      if(address >= 0x8000)
        sram_size = SRAM_SIZE_64KB;
      gamepak_backup[address] = value;
    }
  }
}

#define write_backup8()                                                       \
  write_backup(address & 0xFFFF, value)                                       \

#define write_backup16()                                                      \

#define write_backup32()                                                      \

// TODO: behavior differs by bitmap mode
#define write_vram8()                                                         \
  address &= 0x1FFFE;                                                         \
  if(address >= 0x18000)                                                      \
    address -= 0x8000;                                                        \
  ADDRESS16(vram, address) = ((value << 8) | value)                           \

#define write_vram16()                                                        \
  address &= 0x1FFFF;                                                         \
  if(address >= 0x18000)                                                      \
    address -= 0x8000;                                                        \
  ADDRESS16(vram, address) = value                                            \

#define write_vram32()                                                        \
  address &= 0x1FFFF;                                                         \
  if(address >= 0x18000)                                                      \
    address -= 0x8000;                                                        \
  ADDRESS32(vram, address) = value                                            \

#define write_oam_ram8()                                                      \

#define write_oam_ram16()                                                     \
  oam_update = 1;                                                             \
  ADDRESS16(oam_ram, address & 0x3FF) = value                                 \

#define write_oam_ram32()                                                     \
  oam_update = 1;                                                             \
  ADDRESS32(oam_ram, address & 0x3FF) = value                                 \

// RTC code derived from VBA's (due to lack of any real publically available
// documentation...)

typedef enum
{
  RTC_DISABLED,
  RTC_IDLE,
  RTC_COMMAND,
  RTC_OUTPUT_DATA,
  RTC_INPUT_DATA
} rtc_state_type;

typedef enum
{
  RTC_COMMAND_RESET_1          = 0x60,
  RTC_COMMAND_RESET_2          = 0x61,
  RTC_COMMAND_WRITE_STATUS     = 0x62,
  RTC_COMMAND_READ_STATUS      = 0x63,
  RTC_COMMAND_WRITE_TIME_FULL  = 0x64,
  RTC_COMMAND_READ_TIME_FULL   = 0x65,
  RTC_COMMAND_WRITE_TIME       = 0x66,
  RTC_COMMAND_READ_TIME        = 0x67
} rtc_command_type;

typedef enum
{
  RTC_WRITE_TIME,
  RTC_WRITE_TIME_FULL,
  RTC_WRITE_STATUS
} rtc_write_mode_type;

rtc_state_type rtc_state = RTC_DISABLED;
rtc_write_mode_type rtc_write_mode;
u8 rtc_registers[3];
u32 rtc_command;
u32 rtc_data[12];
u32 rtc_status = 0x42;
u32 rtc_data_bytes;
s32 rtc_bit_count;
u32 solar_counter = 0;
u32 solar_status = 0;

u32 encode_bcd(u8 value)
{
  return ((value / 10) << 4) | (value % 10);
}

#define write_cart_io_register(index, _value)                                 \
  update_address = 0x80000C4 + (index * 2);                                   \
  rtc_registers[index] = _value;                                              \
  rtc_page_index = update_address >> 15;                                      \
  map = memory_map_read[rtc_page_index];                                      \
                                                                              \
  if(map == NULL)                                                             \
    map = load_gamepak_page(rtc_page_index & 0x3FF);                          \
                                                                              \
  ADDRESS16(map, update_address & 0x7FFF) = _value                            \

void write_cart_io(u32 address, u32 value)
{
  u32 rtc_page_index;
  u32 update_address;
  u8 *map;

  value &= 0xFFFF;

  switch(address)
  {
    // RTC command
    // Bit 0: SCHK, perform action
    // Bit 1: IO, input/output command data
    // Bit 2: CS, select input/output? If high make I/O write only
  case 0xC4:
#if 0
    if((value & 0x04) == 0)
      {
        value &= 0x07;
        if(value & 0x02)
          {
            solar_counter = 0;
            solar_status = 0;
          }
        else
          {
            if((value & 0x01) == 1)
              {
                solar_status = 1;
              }
            if((value & 0x01) == 0)
              {
                if(solar_status == 1)
                  {
                    solar_counter++;
                    if(solar_counter == 256)
                      solar_counter = 0;
                    if((g_gpsp_config.solar_level * 10) <= solar_counter)
                      value = 0x80;
                    else
                      value = 0x00;
                    write_cart_io_register(0, ~rtc_registers[1] & value);
                  }
                solar_status = 0;
              }
          }
      }
#else
    if(rtc_state == RTC_DISABLED)
      rtc_state = RTC_IDLE;
    if(!(rtc_registers[0] & 0x04))
      value = (rtc_registers[0] & 0x02) | (value & ~0x02);
    if(rtc_registers[2] & 0x01)
      {
        // To begin writing a command 1, 5 must be written to the command
        // registers.
        if((rtc_state == RTC_IDLE) && (rtc_registers[0] == 0x01) && (value == 0x05))
          {
            // We're now ready to begin receiving a command.
            write_cart_io_register(0, value);
            rtc_state = RTC_COMMAND;
            rtc_command = 0;
            rtc_bit_count = 7;
          }
        else
          {
            write_cart_io_register(0, value);
            switch(rtc_state)
            {
              // Accumulate RTC command by receiving the next bit, and if we
              // have accumulated enough bits to form a complete command
              // execute it.
            case RTC_COMMAND:
              if(rtc_registers[0] & 0x01)
                {
                  rtc_command |= ((value & 0x02) >> 1) << rtc_bit_count;
                  rtc_bit_count--;
                }

              // Have we received a full RTC command? If so execute it.
              if(rtc_bit_count < 0)
                {
                  switch(rtc_command)
                  {
                    // Resets RTC
                  case RTC_COMMAND_RESET_1:
                  case RTC_COMMAND_RESET_2:
                    rtc_state = RTC_IDLE;
                    memset(rtc_registers, 0, sizeof(rtc_registers));
                    break;

                    // Sets status of RTC
                  case RTC_COMMAND_WRITE_STATUS:
                    rtc_state = RTC_INPUT_DATA;
                    rtc_data_bytes = 1;
                    rtc_write_mode = RTC_WRITE_STATUS;
                    break;

                    // Outputs current status of RTC
                  case RTC_COMMAND_READ_STATUS:
                    rtc_state = RTC_OUTPUT_DATA;
                    rtc_data_bytes = 1;
                    rtc_data[0] = rtc_status;
                    break;

                    // Actually outputs the time, all of it
                    // 0x65
                  case RTC_COMMAND_READ_TIME_FULL:
                    {
                      ScePspDateTime current_time;
                      sceRtcGetCurrentClockLocalTime(&current_time);

                      int day_of_week = sceRtcGetDayOfWeek(current_time.year, current_time.month , current_time.day);
                      if(day_of_week == 0)
                        day_of_week = 6;
                      else
                        day_of_week--;

                      rtc_state = RTC_OUTPUT_DATA;
                      rtc_data_bytes = 7;
                      rtc_data[0] = encode_bcd(current_time.year % 100);
                      rtc_data[1] = encode_bcd(current_time.month);
                      rtc_data[2] = encode_bcd(current_time.day);
                      rtc_data[3] = encode_bcd(day_of_week);
                      rtc_data[4] = encode_bcd(current_time.hour);
                      rtc_data[5] = encode_bcd(current_time.minute);
                      rtc_data[6] = encode_bcd(current_time.second);
                      break;
                    }

                    // Only outputs the current time of day.
                    // 0x67
                  case RTC_COMMAND_READ_TIME:
                    {
                      ScePspDateTime current_time;
                      sceRtcGetCurrentClockLocalTime(&current_time);

                      rtc_state = RTC_OUTPUT_DATA;
                      rtc_data_bytes = 3;
                      rtc_data[0] = encode_bcd(current_time.hour);
                      rtc_data[1] = encode_bcd(current_time.minute);
                      rtc_data[2] = encode_bcd(current_time.second);
                      break;
                    }
                  }
                  rtc_bit_count = 0;
                }
              break;

              // Receive parameters from the game as input to the RTC
              // for a given command. Read one bit at a time.
            case RTC_INPUT_DATA:
              // Bit 1 of parameter A must be high for input
              if(rtc_registers[1] & 0x02)
                {
                  // Read next bit for input
                  if(!(value & 0x01))
                    {
                      rtc_data[rtc_bit_count >> 3] |=
                        ((value & 0x01) << (7 - (rtc_bit_count & 0x07)));
                    }
                  else
                    {
                      rtc_bit_count++;

                      if(rtc_bit_count == (rtc_data_bytes * 8))
                        {
                          rtc_state = RTC_IDLE;
                          switch(rtc_write_mode)
                          {
                          case RTC_WRITE_STATUS:
                            rtc_status = rtc_data[0];
                            break;

                          default:
                            ;
                            break;
                          }
                        }
                    }
                }
              break;

            case RTC_OUTPUT_DATA:
              // Bit 1 of parameter A must be low for output
              if(!(rtc_registers[1] & 0x02))
                {
                  // Write next bit to output, on bit 1 of parameter B
                  if(!(value & 0x01))
                    {
                      u8 current_output_byte = rtc_registers[2];

                      current_output_byte =
                        (current_output_byte & ~0x02) |
                        (((rtc_data[rtc_bit_count >> 3] >>
                            (rtc_bit_count & 0x07)) & 0x01) << 1);

                      write_cart_io_register(0, current_output_byte);

                    }
                  else
                    {
                      rtc_bit_count++;

                      if(rtc_bit_count == (rtc_data_bytes * 8))
                        {
                          rtc_state = RTC_IDLE;
                          memset(rtc_registers, 0, sizeof(rtc_registers));
                        }
                    }
                }
              break;

            default:
              ;
              break;
            }
          }
      }
    else
    {
      write_cart_io_register(2, value);
    }
#endif
    break;
    // Write parameter A
  case 0xC6:
    write_cart_io_register(1, value);
    break;

    // Write parameter B
  case 0xC8:
    write_cart_io_register(2, value);
    break;
  }
}

#define write_cart_io8()                                                      \

#define write_cart_io16()                                                     \
  write_cart_io(address & 0xFF, value)                                        \

#define write_cart_io32()                                                     \

// type = 8 / 16 / 32
#define write_memory(type)                                                    \
  switch(address >> 24)                                                       \
  {                                                                           \
    case 0x02:                                                                \
      /* external work RAM */                                                 \
      address = (address & 0x7FFF) + ((address & 0x38000) * 2) + 0x8000;      \
      ADDRESS##type(ewram, address) = value;                                  \
      break;                                                                  \
                                                                              \
    case 0x03:                                                                \
      /* internal work RAM */                                                 \
      ADDRESS##type(iwram, (address & 0x7FFF) + 0x8000) = value;              \
      break;                                                                  \
                                                                              \
    case 0x04:                                                                \
      /* I/O registers */                                                     \
      if(address < 0x04000400)                                                \
        return write_io_register##type(address & 0x3FF, value);                 /* I/O exists up to 0x803 */ \
      break;                                                                    /* TODO: mirrors every 0x800 */  \
                                                                              \
    case 0x05:                                                                \
      /* palatte RAM */                                                       \
      write_palette##type(address & 0x3FF, value);                            \
      break;                                                                  \
                                                                              \
    case 0x06:                                                                \
      /* VRAM */                                                              \
      /*address &= 0x1FFFF;*/                                                 \
      /*if(address >= 0x18000)*/                                              \
      /*  address -= 0x8000;*/                                                \
                                                                              \
      write_vram##type();                                                     \
      break;                                                                  \
                                                                              \
    case 0x07:                                                                \
      /* OAM RAM */                                                           \
      write_oam_ram##type();                                                  \
      break;                                                                  \
                                                                              \
    case 0x08:                                                                \
      /* gamepak ROM or RTC */                                                \
      write_cart_io##type();                                                  \
      break;                                                                  \
                                                                              \
    case 0x09 ... 0x0C:                                                       \
      /* gamepak ROM space */                                                 \
      break;                                                                  \
                                                                              \
    case 0x0D:                                                                \
      write_eeprom(address, value);                                           \
      break;                                                                  \
                                                                              \
    case 0x0E:                                                                \
      write_backup##type();                                                   \
      break;                                                                  \
                                                                              \
    default:                                                                  \
      /* unwritable */                                                        \
      break;                                                                  \
  }                                                                           \

u8 read_memory8(u32 address)
{
  u8 value;
  read_memory(8);
  return value;
}

u16 read_memory16_signed(u32 address)
{
  u16 value;

  if(address & 0x01)
  {
    return (s8)read_memory8(address);
  }
  else
  {
    read_memory(16);
  }

  return value;
}

// unaligned reads are actually 32bit

u32 read_memory16(u32 address)
{
  u32 value;

  if(address & 0x01)
  {
    address &= ~0x01;
    read_memory(16);
    ROR(value, value, 8);
  }
  else
  {
    read_memory(16);
  }

  return value;
}


u32 read_memory32(u32 address)
{
  u32 value;
  if(address & 0x03)
  {
    u32 rotate = (address & 0x03) * 8;
    address &= ~0x03;
    read_memory(32);
    ROR(value, value, rotate);
  }
  else
  {
    read_memory(32);
  }

  return value;
}

CPU_ALERT_TYPE write_memory8(u32 address, u8 value)
{
  write_memory(8);
  return CPU_ALERT_NONE;
}

CPU_ALERT_TYPE write_memory16(u32 address, u16 value)
{
  write_memory(16);
  return CPU_ALERT_NONE;
}

CPU_ALERT_TYPE write_memory32(u32 address, u32 value)
{
  write_memory(32);
  return CPU_ALERT_NONE;
}

char backup_filename[MAX_FILE];

u32 load_backup(char *name)
{
  char backup_path[MAX_PATH];
  FILE_ID backup_file;

  if (g_default_save_dir[0] != '\0') {
    path_printf(backup_path, sizeof(backup_path), "%s/%s", g_default_save_dir, name);
  }
  else
  {
    strcpy(backup_path, name);
  }

  FILE_OPEN(backup_file, backup_path, READ);

  if(FILE_CHECK_VALID(backup_file))
  {
    u32 backup_size = file_length(backup_path);

    FILE_READ(backup_file, gamepak_backup, backup_size);
    FILE_CLOSE(backup_file);

    // The size might give away what kind of backup it is.
    switch(backup_size)
    {
      case 0x200:
        backup_type = BACKUP_EEPROM;
        eeprom_size = EEPROM_512_BYTE;
        break;

      case 0x2000:
        backup_type = BACKUP_EEPROM;
        eeprom_size = EEPROM_8_KBYTE;
        break;

      case 0x8000:
        backup_type = BACKUP_SRAM;
        sram_size = SRAM_SIZE_32KB;
        break;

      // Could be either flash or SRAM, go with flash
      case 0x10000:
        backup_type = BACKUP_FLASH;
        sram_size = FLASH_SIZE_64KB;
        break;

      case 0x20000:
        backup_type = BACKUP_FLASH;
        flash_size = FLASH_SIZE_128KB;
        break;
    }
    return 1;
  }
  else
  {
    backup_type = BACKUP_NONE;
    memset(gamepak_backup, 0xFF, 1024 * 128);
  }

  return 0;
}

u32 save_backup(char *name)
{
  char backup_path[1024];
  FILE_ID backup_file;

  if(backup_type != BACKUP_NONE)
  {
    if (g_default_save_dir[0] != '\0') {
      sprintf(backup_path, "%s/%s", g_default_save_dir, name);
    }
    else
    {
      strcpy(backup_path, name);
    }

    FILE_OPEN(backup_file, backup_path, WRITE);

    if(FILE_CHECK_VALID(backup_file))
    {
      u32 backup_size = 0x8000;

      switch(backup_type)
      {
        case BACKUP_SRAM:
          if(sram_size == SRAM_SIZE_32KB)
            backup_size = 0x8000;
          else
            backup_size = 0x10000;
          break;

        case BACKUP_FLASH:
          if(flash_size == FLASH_SIZE_64KB)
            backup_size = 0x10000;
          else
            backup_size = 0x20000;
          break;

        case BACKUP_EEPROM:
          if(eeprom_size == EEPROM_512_BYTE)
            backup_size = 0x200;
          else
            backup_size = 0x2000;
          break;

        default:
          ;
          break;
      }

      FILE_WRITE(backup_file, gamepak_backup, backup_size);
      FILE_CLOSE(backup_file);
      return 1;
    }
  }

  return 0;
}

void update_backup()
{
  if(backup_update != (write_backup_delay + 1))
    backup_update--;

  if(backup_update == 0)
  {
    save_backup(backup_filename);
    backup_update = write_backup_delay + 1;
  }
}

void update_backup_force()
{
  save_backup(backup_filename);
}

#define CONFIG_FILENAME "game_config.txt"

char *skip_spaces(char *line_ptr)
{
  while(*line_ptr == ' ')
    line_ptr++;

  return line_ptr;
}

s32 parse_config_line(char *current_line, char *current_variable, char *current_value)
{
  char *line_ptr = current_line;
  char *line_ptr_new;

  if((current_line[0] == 0) || (current_line[0] == '#'))
    return -1;

  line_ptr_new = strchr(line_ptr, ' ');
  if(line_ptr_new == NULL)
    return -1;

  *line_ptr_new = 0;
  strcpy(current_variable, line_ptr);
  line_ptr_new = skip_spaces(line_ptr_new + 1);

  if(*line_ptr_new != '=')
    return -1;

  line_ptr_new = skip_spaces(line_ptr_new + 1);
  strcpy(current_value, line_ptr_new);
  line_ptr_new = current_value + strlen(current_value) - 1;
  if(*line_ptr_new == '\n')
  {
    line_ptr_new--;
    *line_ptr_new = 0;
  }

  if(*line_ptr_new == '\r')
    *line_ptr_new = 0;

  return 0;
}

s32 load_game_config(char *gamepak_title, char *gamepak_code, char *gamepak_maker)
{
  char current_line[256];
  char current_variable[256];
  char current_value[256];
  char config_path[MAX_PATH];
//  u8 *line_ptr;
//  u32 fgets_value;
  FILE *config_file;

  idle_loop_targets = 0;
  idle_loop_target_pc[0] = 0xFFFFFFFF;
  iwram_stack_optimize = 1;
  // The bios_rom_hack_* settings patch the official BIOS. The built-in one
  // has other code at these offsets (0x2C is in its IRQ handler).
  if(!bios_builtin)
  {
    bios_rom[0x39] = 0x00;
    bios_rom[0x2C] = 0x00;
  }
  translation_gate_targets = 0;
  flash_device_id = FLASH_DEVICE_MACRONIX_64KB;
  backup_type = BACKUP_NONE;


  path_printf(config_path, sizeof(config_path), "%s/%s", main_path, CONFIG_FILENAME);

  config_file = fopen(config_path, "rb");

  if(config_file)
  {
    while(fgets(current_line, 256, config_file))
    {
      if(parse_config_line(current_line, current_variable, current_value) != -1)
      {
        if(strcasecmp(current_variable, "game_name") || strcasecmp(current_value, gamepak_title))
          continue;

        if(!fgets(current_line, 256, config_file) || (parse_config_line(current_line, current_variable, current_value) == -1) ||
         strcasecmp(current_variable, "game_code") || strcasecmp(current_value, gamepak_code))
          continue;

        if(!fgets(current_line, 256, config_file) || (parse_config_line(current_line, current_variable, current_value) == -1) ||
         strcasecmp(current_variable, "vender_code") || strcasecmp(current_value, gamepak_maker))
          continue;

        while(fgets(current_line, 256, config_file))
        {
          if(parse_config_line(current_line, current_variable, current_value)
           != -1)
          {
            if(!strcasecmp(current_variable, "game_name"))
            {
              fclose(config_file);
              return 0;
            }

            if(!strcasecmp(current_variable, "idle_loop_eliminate_target"))
            {
              if(idle_loop_targets < MAX_IDLE_LOOPS)
              {
                idle_loop_target_pc[idle_loop_targets] =
                  strtol(current_value, NULL, 16);
                idle_loop_targets++;
              }
            }

            if(!strcasecmp(current_variable, "translation_gate_target"))
            {
              if(translation_gate_targets < MAX_TRANSLATION_GATES)
              {
                translation_gate_target_pc[translation_gate_targets] =
                 strtol(current_value, NULL, 16);
                translation_gate_targets++;
              }
            }

            if(!strcasecmp(current_variable, "iwram_stack_optimize") && !strcasecmp(current_value, "no"))
            {
                iwram_stack_optimize = 0;
            }

            if(!strcasecmp(current_variable, "flash_rom_type") && !strcasecmp(current_value, "128KB"))
            {
              flash_device_id = FLASH_DEVICE_MACRONIX_128KB;
            }

            // Work around the DBZLGCYGOKU2 copy protection
            // Works if set to the special EEPROM_V124 type (cannot be auto-detected yet)
            if(!strcasecmp(current_variable, "save_type"))
            {
              if(!strcasecmp(current_value, "sram"))
                backup_type = BACKUP_SRAM;
              else
              if(!strcasecmp(current_value, "flash"))
                backup_type = BACKUP_FLASH;
              else
              if(!strcasecmp(current_value, "eeprom"))
                backup_type = BACKUP_EEPROM;
            }

            if(!strcasecmp(current_variable, "bios_rom_hack_39") &&
              !strcasecmp(current_value, "yes") && !bios_builtin)
            {
              bios_rom[0x39] = 0xC0;
            }

            if(!strcasecmp(current_variable, "bios_rom_hack_2C") &&
              !strcasecmp(current_value, "yes") && !bios_builtin)
            {
               bios_rom[0x2C] = 0x02;
            }
          }
        }

        fclose(config_file);
        return 0;
      }
    }

    fclose(config_file);
  }

  return -1;
}

#define LOAD_ON_MEMORY -1

s32 load_gamepak_raw(char *name)
{
  FILE_ID gamepak_file;

  chdir(rom_path);
  FILE_OPEN(gamepak_file, name, READ);

  if(FILE_CHECK_VALID(gamepak_file))
  {
    u32 gamepak_size = file_length(name);

    // If it's a big file size keep it don't close it, we'll
    // probably want to load it later
    if(gamepak_size <= gamepak_ram_buffer_size)
    {
      FILE_READ(gamepak_file, gamepak_rom, gamepak_size);
      FILE_CLOSE(gamepak_file);

      gamepak_file_large = LOAD_ON_MEMORY;
    }
    else
    {
      // Read in just enough for the header
      FILE_READ(gamepak_file, gamepak_rom, 0x100);
      gamepak_file_large = gamepak_file;
      // Use the full path: the file list may have changed the current directory
      char temp_path[MAX_PATH];
      getcwd(temp_path, MAX_PATH);
      path_printf(gamepak_filename_full_path, sizeof(gamepak_filename_full_path), "%s/%s", temp_path, name);
    }

    chdir(main_path);
    return gamepak_size;
  }

  return -1;
}

s32 load_gamepak(char *name)
{
  char *dot_position = strrchr(name, '.');
  s32 file_size;
  char cheats_filename[MAX_FILE];

  if(FILE_CHECK_VALID(gamepak_file_large))
    FILE_CLOSE(gamepak_file_large);

  gamepak_file_large = -1;

  if(!strcasecmp(dot_position, ".zip"))
  {
    set_cpu_clock(9); // temporarily 333 MHz
    init_progress(5, msg[MSG_LOAD_ZIP_ROM]);
    file_size = load_file_zip(name);
    if(file_size == -2)
      file_size = load_gamepak_raw(ZIP_TMP);
  }
  else
  {
    init_progress(5, msg[MSG_LOAD_ROM]);
    file_size = load_gamepak_raw(name);
  }
  update_progress();

  if(file_size != -1)
  {
    gamepak_size = (file_size + 0x7FFF) & ~0x7FFF;

    strcpy(gamepak_filename, name);
//    strncpy(backup_filename, name, MAX_FILE);
    change_ext(gamepak_filename, backup_filename, ".sav");

    load_backup(backup_filename);
    update_progress();

    memcpy(gamepak_title, gamepak_rom + 0xA0, 12);
    memcpy(gamepak_code, gamepak_rom + 0xAC, 4);
    memcpy(gamepak_maker, gamepak_rom + 0xB0, 2);
    gamepak_title[12] = 0;
    gamepak_code[4] = 0;
    gamepak_maker[2] = 0;

    if (gamepak_file_large == LOAD_ON_MEMORY)
      gamepak_crc32 = crc32(gamepak_crc32, gamepak_rom, file_size);
    else
      gamepak_crc32 = 0;

    mem_save_flag = 0;

    load_game_config(gamepak_title, gamepak_code, gamepak_maker);
    update_progress();
    load_game_config_file();
    update_progress();

    change_ext(gamepak_filename, cheats_filename, ".cht");
    add_cheats(cheats_filename);
    update_progress();

    show_progress(msg[MSG_LOAD_ROM_OK]);

    return 0;
  }

  show_progress(msg[MSG_LOAD_ROM_ERROR]);
  button_up_wait();
  error_msg("");  // wait for any button
  return -1;
}


// Returns 0 on success, -2 on MD5 mismatch, -1 if unreadable
extern u8 open_gba_bios_rom[0x4000];

// 1 when the built-in BIOS is in use (src/bios_data.S).
u32 bios_builtin = 0;

void load_builtin_bios(void)
{
  memcpy(bios_rom, open_gba_bios_rom, 0x4000);
  bios_builtin = 1;
}

// Returns 0 for a known BIOS dump, -2 for an unknown one, and -1 when the
// file is missing or does not look like a GBA BIOS: its first instruction
// is the reset vector's branch, so the first byte is 0x18 (as libretro gpSP
// checks).
s32 load_bios(char *name)
{
  u8 md5[16];
  FILE_ID bios_file;
  FILE_OPEN(bios_file, name, READ);

  bios_builtin = 0;

  if(FILE_CHECK_VALID(bios_file))
  {
    FILE_READ(bios_file, bios_rom, 0x4000);
    FILE_CLOSE(bios_file);
    if(bios_rom[0] != 0x18)
      return -1;
    sceKernelUtilsMd5Digest(bios_rom, 0x4000, md5);
    if ((memcmp(md5,gba_md5,16) == 0) || (memcmp(md5,nds_md5,16) == 0))
      return 0;
    else
      return -2;
  }

  return -1;
}

// DMA memory regions can be one of the following:
// IWRAM - 32kb offset from the contiguous iwram region.
// EWRAM - like segmented but with self modifying code check.
// VRAM - 96kb offset from the contiguous vram region, should take care
// Palette RAM - Converts palette entries when written to.
// OAM RAM - Sets OAM modified flag to true.
// I/O registers - Uses the I/O register function.
// of mirroring properly.
// Segmented RAM/ROM - a region >= 32kb, the translated address has to
//  be reloaded if it wraps around the limit (cartride ROM)
// Ext - should be handled by the memory read/write function.

// The following map determines the region of each (assumes DMA access
// is not done out of bounds)

typedef enum
{
  DMA_REGION_IWRAM,
  DMA_REGION_EWRAM,
  DMA_REGION_VRAM,
  DMA_REGION_PALETTE_RAM,
  DMA_REGION_OAM_RAM,
  DMA_REGION_IO,
  DMA_REGION_GAMEPAK,
  DMA_REGION_EXT,
  DMA_REGION_BIOS,
  DMA_REGION_NULL
} dma_region_type;

dma_region_type dma_region_map[16] =
{
  DMA_REGION_BIOS,          // 0x00 - BIOS
  DMA_REGION_NULL,          // 0x01 - Nothing
  DMA_REGION_EWRAM,         // 0x02 - EWRAM
  DMA_REGION_IWRAM,         // 0x03 - IWRAM
  DMA_REGION_IO,            // 0x04 - I/O registers
  DMA_REGION_PALETTE_RAM,   // 0x05 - palette RAM
  DMA_REGION_VRAM,          // 0x06 - VRAM
  DMA_REGION_OAM_RAM,       // 0x07 - OAM RAM
  DMA_REGION_GAMEPAK,       // 0x08 - gamepak ROM
  DMA_REGION_GAMEPAK,       // 0x09 - gamepak ROM
  DMA_REGION_GAMEPAK,       // 0x0A - gamepak ROM
  DMA_REGION_GAMEPAK,       // 0x0B - gamepak ROM
  DMA_REGION_GAMEPAK,       // 0x0C - gamepak ROM
  DMA_REGION_EXT,           // 0x0D - EEPROM
  DMA_REGION_EXT,           // 0x0E - gamepak SRAM/flash ROM
  DMA_REGION_EXT            // 0x0F - gamepak SRAM/flash ROM
};

#define dma_adjust_ptr_inc(ptr, size)                                         \
  ptr += (size / 8)                                                           \

#define dma_adjust_ptr_dec(ptr, size)                                         \
  ptr -= (size / 8)                                                           \

#define dma_adjust_ptr_fix(ptr, size)                                         \

#define dma_adjust_ptr_writeback()                                            \
  dma->dest_address = dest_ptr                                                \

#define dma_adjust_ptr_reload()                                               \

#define dma_smc_vars_src()                                                    \

#define dma_smc_vars_dest()                                                   \
  u32 smc_trigger = 0                                                         \

#define dma_vars_iwram(type)                                                  \
  dma_smc_vars_##type()                                                       \

#define dma_vars_vram(type)                                                   \
  type##_ptr &= 0x1FFFF;                                                      \
  if(type##_ptr >= 0x18000)                                                   \
    type##_ptr -= 0x8000                                                      \

#define dma_vars_palette_ram(type)                                            \

#define dma_oam_ram_src()                                                     \

#define dma_oam_ram_dest()                                                    \
  oam_update = 1                                                              \

#define dma_vars_oam_ram(type)                                                \
  dma_oam_ram_##type()                                                        \

#define dma_vars_io(type)                                                     \

#define dma_segmented_load_src()                                              \
  memory_map_read[src_current_region]                                         \

#define dma_segmented_load_dest()                                             \
  memory_map_write[dest_current_region]                                       \

#define dma_vars_gamepak(type)                                                \
  u32 type##_new_region;                                                      \
  u32 type##_current_region = type##_ptr >> 15;                               \
  u8 *type##_address_block = dma_segmented_load_##type();                     \
  if(type##_address_block == NULL)                                            \
  {                                                                           \
    if((type##_ptr & 0x1FFFFFF) >= gamepak_size)                              \
      break;                                                                  \
    type##_address_block = load_gamepak_page(type##_current_region & 0x3FF);  \
  }                                                                           \

#define dma_vars_ewram(type)                                                  \
  dma_smc_vars_##type();                                                      \
  u32 type##_new_region;                                                      \
  u32 type##_current_region = type##_ptr >> 15;                               \
  u8 *type##_address_block = dma_segmented_load_##type()                      \

#define dma_vars_bios(type)                                                   \

#define dma_vars_ext(type)                                                    \

#define dma_ewram_check_region(type)                                          \
  type##_new_region = (type##_ptr >> 15);                                     \
  if(type##_new_region != type##_current_region)                              \
  {                                                                           \
    type##_current_region = type##_new_region;                                \
    type##_address_block = dma_segmented_load_##type();                       \
  }                                                                           \

#define dma_gamepak_check_region(type)                                        \
  type##_new_region = (type##_ptr >> 15);                                     \
  if(type##_new_region != type##_current_region)                              \
  {                                                                           \
    type##_current_region = type##_new_region;                                \
    type##_address_block = dma_segmented_load_##type();                       \
    if(type##_address_block == NULL)                                          \
    {                                                                         \
      type##_address_block =                                                  \
       load_gamepak_page(type##_current_region & 0x3FF);                      \
    }                                                                         \
  }                                                                           \

#define dma_read_iwram(type, transfer_size)                                   \
  read_value = ADDRESS##transfer_size(iwram + 0x8000, type##_ptr & 0x7FFF)    \

#define dma_read_vram(type, transfer_size)                                    \
  read_value = ADDRESS##transfer_size(vram, type##_ptr & 0x1FFFF)             \

#define dma_read_io(type, transfer_size)                                      \
  read_value = ADDRESS##transfer_size(io_registers, type##_ptr & 0x7FFF)      \

#define dma_read_oam_ram(type, transfer_size)                                 \
  read_value = ADDRESS##transfer_size(oam_ram, type##_ptr & 0x3FF)            \

#define dma_read_palette_ram(type, transfer_size)                             \
  read_value = ADDRESS##transfer_size(palette_ram, type##_ptr & 0x3FF)        \

#define dma_read_ewram(type, transfer_size)                                   \
  dma_ewram_check_region(type);                                               \
  read_value = ADDRESS##transfer_size(type##_address_block,                   \
   type##_ptr & 0x7FFF)                                                       \

#define dma_read_gamepak(type, transfer_size)                                 \
  dma_gamepak_check_region(type);                                             \
  read_value = ADDRESS##transfer_size(type##_address_block,                   \
   type##_ptr & 0x7FFF)                                                       \

// DMAing from the BIOS is funny, just returns 0..

#define dma_read_bios(type, transfer_size)                                    \
  read_value = 0                                                              \

#define dma_read_ext(type, transfer_size)                                     \
  read_value = read_memory##transfer_size(type##_ptr)                         \

#define dma_write_iwram(type, transfer_size)                                  \
  ADDRESS##transfer_size(iwram + 0x8000, type##_ptr & 0x7FFF) = read_value;   \
  smc_trigger |= ADDRESS##transfer_size(iwram, type##_ptr & 0x7FFF)           \

#define dma_write_vram(type, transfer_size)                                   \
  ADDRESS##transfer_size(vram, type##_ptr & 0x1FFFF) = read_value             \

#define dma_write_io(type, transfer_size)                                     \
  write_io_register##transfer_size(type##_ptr & 0x3FF, read_value)            \

#define dma_write_oam_ram(type, transfer_size)                                \
  ADDRESS##transfer_size(oam_ram, type##_ptr & 0x3FF) = read_value            \

#define dma_write_palette_ram(type, transfer_size)                            \
  write_palette##transfer_size(type##_ptr & 0x3FF, read_value)                \

#define dma_write_ext(type, transfer_size)                                    \
  write_memory##transfer_size(type##_ptr, read_value)                         \

#define dma_write_ewram(type, transfer_size)                                  \
  dma_ewram_check_region(type);                                               \
                                                                              \
  ADDRESS##transfer_size(type##_address_block, type##_ptr & 0x7FFF) =         \
    read_value;                                                               \
  smc_trigger |= ADDRESS##transfer_size(type##_address_block,                 \
   (type##_ptr & 0x7FFF) - 0x8000)                                            \

#define dma_epilogue_iwram()                                                  \
  if(smc_trigger)                                                             \
  {                                                                           \
    /* Special return code indicating to retranslate to the CPU code */       \
    return_value |= CPU_ALERT_SMC;                                             \
  }                                                                           \

#define dma_epilogue_ewram()                                                  \
  if(smc_trigger)                                                             \
  {                                                                           \
    /* Special return code indicating to retranslate to the CPU code */       \
    return_value |= CPU_ALERT_SMC;                                             \
  }                                                                           \

#define dma_epilogue_vram()                                                   \

#define dma_epilogue_io()                                                     \

#define dma_epilogue_oam_ram()                                                \

#define dma_epilogue_palette_ram()                                            \

#define dma_epilogue_GAMEPAK()                                                \

#define dma_epilogue_ext()                                                    \

#define print_line()                                                          \
  dma_print(src_op, dest_op, transfer_size, wb)                               \

#define dma_transfer_loop_region(src_region_type, dest_region_type, src_op,   \
 dest_op, transfer_size, wb)                                                  \
{                                                                             \
  dma_vars_##src_region_type(src);                                            \
  dma_vars_##dest_region_type(dest);                                          \
                                                                              \
  for(i = 0; i < length; i++)                                                 \
  {                                                                           \
    dma_read_##src_region_type(src, transfer_size);                           \
    dma_write_##dest_region_type(dest, transfer_size);                        \
    dma_adjust_ptr_##src_op(src_ptr, transfer_size);                          \
    dma_adjust_ptr_##dest_op(dest_ptr, transfer_size);                        \
  }                                                                           \
  dma->source_address = src_ptr;                                              \
  dma_adjust_ptr_##wb();                                                      \
  dma_epilogue_##dest_region_type();                                          \
}                                                                             \
break;                                                                        \

#define dma_transfer_loop(src_op, dest_op, transfer_size, wb)                 \
{                                                                             \
  u32 src_region = src_ptr >> 24;                                             \
  u32 dest_region = dest_ptr >> 24;                                           \
  dma_region_type src_region_type = dma_region_map[src_region];               \
  dma_region_type dest_region_type = dma_region_map[dest_region];             \
                                                                              \
  switch(src_region_type | (dest_region_type << 4))                           \
  {                                                                           \
    case (DMA_REGION_BIOS | (DMA_REGION_IWRAM << 4)):                         \
      dma_transfer_loop_region(bios, iwram, src_op, dest_op,                  \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IWRAM | (DMA_REGION_IWRAM << 4)):                        \
      dma_transfer_loop_region(iwram, iwram, src_op, dest_op,                 \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EWRAM | (DMA_REGION_IWRAM << 4)):                        \
      dma_transfer_loop_region(ewram, iwram, src_op, dest_op,                 \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_VRAM | (DMA_REGION_IWRAM << 4)):                         \
      dma_transfer_loop_region(vram, iwram, src_op, dest_op,                  \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_PALETTE_RAM | (DMA_REGION_IWRAM << 4)):                  \
      dma_transfer_loop_region(palette_ram, iwram, src_op, dest_op,           \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_OAM_RAM | (DMA_REGION_IWRAM << 4)):                      \
      dma_transfer_loop_region(oam_ram, iwram, src_op, dest_op,               \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IO | (DMA_REGION_IWRAM << 4)):                           \
      dma_transfer_loop_region(io, iwram, src_op, dest_op,                    \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_GAMEPAK | (DMA_REGION_IWRAM << 4)):                      \
      dma_transfer_loop_region(gamepak, iwram, src_op, dest_op,               \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EXT | (DMA_REGION_IWRAM << 4)):                          \
      dma_transfer_loop_region(ext, iwram, src_op, dest_op,                   \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_BIOS | (DMA_REGION_EWRAM << 4)):                         \
      dma_transfer_loop_region(bios, ewram, src_op, dest_op,                  \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IWRAM | (DMA_REGION_EWRAM << 4)):                        \
      dma_transfer_loop_region(iwram, ewram, src_op, dest_op,                 \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EWRAM | (DMA_REGION_EWRAM << 4)):                        \
      dma_transfer_loop_region(ewram, ewram, src_op, dest_op,                 \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_VRAM | (DMA_REGION_EWRAM << 4)):                         \
      dma_transfer_loop_region(vram, ewram, src_op, dest_op,                  \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_PALETTE_RAM | (DMA_REGION_EWRAM << 4)):                  \
      dma_transfer_loop_region(palette_ram, ewram, src_op, dest_op,           \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_OAM_RAM | (DMA_REGION_EWRAM << 4)):                      \
      dma_transfer_loop_region(oam_ram, ewram, src_op, dest_op,               \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IO | (DMA_REGION_EWRAM << 4)):                           \
      dma_transfer_loop_region(io, ewram, src_op, dest_op,                    \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_GAMEPAK | (DMA_REGION_EWRAM << 4)):                      \
      dma_transfer_loop_region(gamepak, ewram, src_op, dest_op,               \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EXT | (DMA_REGION_EWRAM << 4)):                          \
      dma_transfer_loop_region(ext, ewram, src_op, dest_op,                   \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_BIOS | (DMA_REGION_VRAM << 4)):                          \
      dma_transfer_loop_region(bios, vram, src_op, dest_op,                   \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IWRAM | (DMA_REGION_VRAM << 4)):                         \
      dma_transfer_loop_region(iwram, vram, src_op, dest_op,                  \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EWRAM | (DMA_REGION_VRAM << 4)):                         \
      dma_transfer_loop_region(ewram, vram, src_op, dest_op,                  \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_VRAM | (DMA_REGION_VRAM << 4)):                          \
      dma_transfer_loop_region(vram, vram, src_op, dest_op,                   \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_PALETTE_RAM | (DMA_REGION_VRAM << 4)):                   \
      dma_transfer_loop_region(palette_ram, vram, src_op, dest_op,            \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_OAM_RAM | (DMA_REGION_VRAM << 4)):                       \
      dma_transfer_loop_region(oam_ram, vram, src_op, dest_op,                \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IO | (DMA_REGION_VRAM << 4)):                            \
      dma_transfer_loop_region(io, vram, src_op, dest_op,                     \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_GAMEPAK | (DMA_REGION_VRAM << 4)):                       \
      dma_transfer_loop_region(gamepak, vram, src_op, dest_op,                \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EXT | (DMA_REGION_VRAM << 4)):                           \
      dma_transfer_loop_region(ext, vram, src_op, dest_op,                    \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_BIOS | (DMA_REGION_PALETTE_RAM << 4)):                   \
      dma_transfer_loop_region(bios, palette_ram, src_op, dest_op,            \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IWRAM | (DMA_REGION_PALETTE_RAM << 4)):                  \
      dma_transfer_loop_region(iwram, palette_ram, src_op, dest_op,           \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EWRAM | (DMA_REGION_PALETTE_RAM << 4)):                  \
      dma_transfer_loop_region(ewram, palette_ram, src_op, dest_op,           \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_VRAM | (DMA_REGION_PALETTE_RAM << 4)):                   \
      dma_transfer_loop_region(vram, palette_ram, src_op, dest_op,            \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_PALETTE_RAM | (DMA_REGION_PALETTE_RAM << 4)):            \
      dma_transfer_loop_region(palette_ram, palette_ram, src_op, dest_op,     \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_OAM_RAM | (DMA_REGION_PALETTE_RAM << 4)):                \
      dma_transfer_loop_region(oam_ram, palette_ram, src_op, dest_op,         \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IO | (DMA_REGION_PALETTE_RAM << 4)):                     \
      dma_transfer_loop_region(io, palette_ram, src_op, dest_op,              \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_GAMEPAK | (DMA_REGION_PALETTE_RAM << 4)):                \
      dma_transfer_loop_region(gamepak, palette_ram, src_op, dest_op,         \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EXT | (DMA_REGION_PALETTE_RAM << 4)):                    \
      dma_transfer_loop_region(ext, palette_ram, src_op, dest_op,             \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_BIOS | (DMA_REGION_OAM_RAM << 4)):                       \
      dma_transfer_loop_region(bios, oam_ram, src_op, dest_op,                \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IWRAM | (DMA_REGION_OAM_RAM << 4)):                      \
      dma_transfer_loop_region(iwram, oam_ram, src_op, dest_op,               \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EWRAM | (DMA_REGION_OAM_RAM << 4)):                      \
      dma_transfer_loop_region(ewram, oam_ram, src_op, dest_op,               \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_VRAM | (DMA_REGION_OAM_RAM << 4)):                       \
      dma_transfer_loop_region(vram, oam_ram, src_op, dest_op,                \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_PALETTE_RAM | (DMA_REGION_OAM_RAM << 4)):                \
      dma_transfer_loop_region(palette_ram, oam_ram, src_op, dest_op,         \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_OAM_RAM | (DMA_REGION_OAM_RAM << 4)):                    \
      dma_transfer_loop_region(oam_ram, oam_ram, src_op, dest_op,             \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IO | (DMA_REGION_OAM_RAM << 4)):                         \
      dma_transfer_loop_region(io, oam_ram, src_op, dest_op,                  \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_GAMEPAK | (DMA_REGION_OAM_RAM << 4)):                    \
      dma_transfer_loop_region(gamepak, oam_ram, src_op, dest_op,             \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EXT | (DMA_REGION_OAM_RAM << 4)):                        \
      dma_transfer_loop_region(ext, oam_ram, src_op, dest_op,                 \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_BIOS | (DMA_REGION_IO << 4)):                            \
      dma_transfer_loop_region(bios, io, src_op, dest_op,                     \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IWRAM | (DMA_REGION_IO << 4)):                           \
      dma_transfer_loop_region(iwram, io, src_op, dest_op,                    \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EWRAM | (DMA_REGION_IO << 4)):                           \
      dma_transfer_loop_region(ewram, io, src_op, dest_op,                    \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_VRAM | (DMA_REGION_IO << 4)):                            \
      dma_transfer_loop_region(vram, io, src_op, dest_op,                     \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_PALETTE_RAM | (DMA_REGION_IO << 4)):                     \
      dma_transfer_loop_region(palette_ram, io, src_op, dest_op,              \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_OAM_RAM | (DMA_REGION_IO << 4)):                         \
      dma_transfer_loop_region(oam_ram, io, src_op, dest_op,                  \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IO | (DMA_REGION_IO << 4)):                              \
      dma_transfer_loop_region(io, io, src_op, dest_op,                       \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_GAMEPAK | (DMA_REGION_IO << 4)):                         \
      dma_transfer_loop_region(gamepak, io, src_op, dest_op,                  \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EXT | (DMA_REGION_IO << 4)):                             \
      dma_transfer_loop_region(ext, io, src_op, dest_op,                      \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_BIOS | (DMA_REGION_EXT << 4)):                           \
      dma_transfer_loop_region(bios, ext, src_op, dest_op,                    \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IWRAM | (DMA_REGION_EXT << 4)):                          \
      dma_transfer_loop_region(iwram, ext, src_op, dest_op,                   \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EWRAM | (DMA_REGION_EXT << 4)):                          \
      dma_transfer_loop_region(ewram, ext, src_op, dest_op,                   \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_VRAM | (DMA_REGION_EXT << 4)):                           \
      dma_transfer_loop_region(vram, ext, src_op, dest_op,                    \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_PALETTE_RAM | (DMA_REGION_EXT << 4)):                    \
      dma_transfer_loop_region(palette_ram, ext, src_op, dest_op,             \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_OAM_RAM | (DMA_REGION_EXT << 4)):                        \
      dma_transfer_loop_region(oam_ram, ext, src_op, dest_op,                 \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_IO | (DMA_REGION_EXT << 4)):                             \
      dma_transfer_loop_region(io, ext, src_op, dest_op,                      \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_GAMEPAK | (DMA_REGION_EXT << 4)):                        \
      dma_transfer_loop_region(gamepak, ext, src_op, dest_op,                 \
       transfer_size, wb);                                                    \
                                                                              \
    case (DMA_REGION_EXT | (DMA_REGION_EXT << 4)):                            \
      dma_transfer_loop_region(ext, ext, src_op, dest_op,                     \
       transfer_size, wb);                                                    \
  }                                                                           \
}                                                                             \
break;                                                                        \


#define dma_transfer_expand(transfer_size)                                    \
  switch((dma->dest_direction << 2) | dma->source_direction)                  \
  {                                                                           \
    case 0x00:                                                                \
      dma_transfer_loop(inc, inc, transfer_size, writeback);                  \
                                                                              \
    case 0x01:                                                                \
      dma_transfer_loop(dec, inc, transfer_size, writeback);                  \
                                                                              \
    case 0x02:                                                                \
      dma_transfer_loop(fix, inc, transfer_size, writeback);                  \
                                                                              \
    case 0x03:                                                                \
      break;                                                                  \
                                                                              \
    case 0x04:                                                                \
      dma_transfer_loop(inc, dec, transfer_size, writeback);                  \
                                                                              \
    case 0x05:                                                                \
      dma_transfer_loop(dec, dec, transfer_size, writeback);                  \
                                                                              \
    case 0x06:                                                                \
      dma_transfer_loop(fix, dec, transfer_size, writeback);                  \
                                                                              \
    case 0x07:                                                                \
      break;                                                                  \
                                                                              \
    case 0x08:                                                                \
      dma_transfer_loop(inc, fix, transfer_size, writeback);                  \
                                                                              \
    case 0x09:                                                                \
      dma_transfer_loop(dec, fix, transfer_size, writeback);                  \
                                                                              \
    case 0x0A:                                                                \
      dma_transfer_loop(fix, fix, transfer_size, writeback);                  \
                                                                              \
    case 0x0B:                                                                \
      break;                                                                  \
                                                                              \
    case 0x0C:                                                                \
      dma_transfer_loop(inc, inc, transfer_size, reload);                     \
                                                                              \
    case 0x0D:                                                                \
      dma_transfer_loop(dec, inc, transfer_size, reload);                     \
                                                                              \
    case 0x0E:                                                                \
      dma_transfer_loop(fix, inc, transfer_size, reload);                     \
                                                                              \
    case 0x0F:                                                                \
      break;                                                                  \
  }                                                                           \

CPU_ALERT_TYPE dma_transfer(DMA_TRANSFER_TYPE *dma)
{
  u32 i;
  u32 length = dma->length;
  u32 read_value;
  u32 src_ptr = dma->source_address;
  u32 dest_ptr = dma->dest_address;
  CPU_ALERT_TYPE return_value = CPU_ALERT_NONE;

  // Technically this should be done for source and destination, but
  // chances are this is only ever used (probably mistakingly!) for dest.
  // The only game I know of that requires this is Lucky Luke.
  if((dest_ptr >> 24) != ((dest_ptr + length - 1) >> 24))
  {
    u32 first_length = ((dest_ptr & 0xFF000000) + 0x1000000) - dest_ptr;
    u32 second_length = length - first_length;
    dma->length = first_length;

    dma_transfer(dma);

    dma->length = length;

    length = second_length;
    dest_ptr += first_length;
    src_ptr += first_length;
  }

  if(dma->length_type == DMA_16BIT)
  {
    src_ptr &= ~0x01;
    dest_ptr &= ~0x01;
    dma_transfer_expand(16);
  }
  else
  {
    src_ptr &= ~0x03;
    dest_ptr &= ~0x03;
    dma_transfer_expand(32);
  }

  if((dma->repeat_type == DMA_NO_REPEAT) ||
   (dma->start_type == DMA_START_IMMEDIATELY))
  {
    dma->start_type = DMA_INACTIVE;
    ADDRESS16(io_registers, (dma->dma_channel * 12) + 0xBA) &= (~0x8000);
  }

  if(dma->irq)
  {
    raise_interrupt(IRQ_DMA0 << dma->dma_channel);
    return_value |= CPU_ALERT_IRQ;
  }

  return return_value;
}

// Be sure to do this after loading ROMs.

#define map_region(type, start, end, mirror_blocks, region)                   \
  for(map_offset = (start) / 0x8000; map_offset <                             \
   ((end) / 0x8000); map_offset++)                                            \
  {                                                                           \
    memory_map_##type[map_offset] =                                           \
     ((u8 *)region) + ((map_offset % mirror_blocks) * 0x8000);                \
  }                                                                           \

#define map_null(type, start, end)                                            \
  for(map_offset = start / 0x8000; map_offset < (end / 0x8000);               \
   map_offset++)                                                              \
  {                                                                           \
    memory_map_##type[map_offset] = NULL;                                     \
  }                                                                           \

#define map_ram_region(type, start, end, mirror_blocks, region)               \
  for(map_offset = (start) / 0x8000; map_offset <                             \
   ((end) / 0x8000); map_offset++)                                            \
  {                                                                           \
    memory_map_##type[map_offset] =                                           \
     ((u8 *)region) + ((map_offset % mirror_blocks) * 0x10000) + 0x8000;      \
  }                                                                           \

#define map_vram(type)                                                        \
  for(map_offset = 0x6000000 / 0x8000; map_offset < (0x7000000 / 0x8000);     \
   map_offset += 4)                                                           \
  {                                                                           \
    memory_map_##type[map_offset] = vram;                                     \
    memory_map_##type[map_offset + 1] = vram + 0x8000;                        \
    memory_map_##type[map_offset + 2] = vram + (0x8000 * 2);                  \
    memory_map_##type[map_offset + 3] = vram + (0x8000 * 2);                  \
  }                                                                           \

#define map_vram_firstpage(type)                                              \
  for(map_offset = 0x6000000 / 0x8000; map_offset < (0x7000000 / 0x8000);     \
   map_offset += 4)                                                           \
  {                                                                           \
    memory_map_##type[map_offset] = vram;                                     \
    memory_map_##type[map_offset + 1] = NULL;                                 \
    memory_map_##type[map_offset + 2] = NULL;                                 \
    memory_map_##type[map_offset + 3] = NULL;                                 \
  }                                                                           \


// Picks a page to evict
u32 page_time = 0;

u32 evict_gamepak_page()
{
  // Find the one with the smallest frame timestamp
  u32 page_index = 0;
  u32 physical_index;
  u32 smallest = gamepak_memory_map[0].page_timestamp;
  u32 i;

  for(i = 1; i < gamepak_ram_pages; i++)
  {
    if(gamepak_memory_map[i].page_timestamp <= smallest)
    {
      smallest = gamepak_memory_map[i].page_timestamp;
      page_index = i;
    }
  }

  physical_index = gamepak_memory_map[page_index].physical_index;

  memory_map_read[(0x8000000 / (32 * 1024)) + physical_index] = NULL;
  memory_map_read[(0xA000000 / (32 * 1024)) + physical_index] = NULL;
  memory_map_read[(0xC000000 / (32 * 1024)) + physical_index] = NULL;

  return page_index;
}

u8 *load_gamepak_page(u32 physical_index)
{
  if(physical_index >= (gamepak_size >> 15))
    return gamepak_rom;

  u32 page_index = evict_gamepak_page();
  u32 page_offset = page_index * (32 * 1024);
  u8 *swap_location = gamepak_rom + page_offset;

  gamepak_memory_map[page_index].page_timestamp = page_time;
  gamepak_memory_map[page_index].physical_index = physical_index;
  page_time++;

//  FILE_OPEN(gamepak_file_large, gamepak_filename_full_path, READ);

  FILE_SEEK(gamepak_file_large, physical_index * (32 * 1024), SEEK_SET);
  FILE_READ(gamepak_file_large, swap_location, (32 * 1024));

//  FILE_CLOSE(gamepak_file_large);

  memory_map_read[(0x8000000 / (32 * 1024)) + physical_index] = swap_location;
  memory_map_read[(0xA000000 / (32 * 1024)) + physical_index] = swap_location;
  memory_map_read[(0xC000000 / (32 * 1024)) + physical_index] = swap_location;

  // If RTC is active page the RTC register bytes so they can be read
  if((rtc_state != RTC_DISABLED) && (physical_index == 0))
  {
    memcpy(swap_location + 0xC4, rtc_registers, sizeof(rtc_registers));
  }

  return swap_location;
}

void init_memory_gamepak()
{
  u32 map_offset = 0;

  if(gamepak_size > gamepak_ram_buffer_size)
  {
    // Large ROMs get special treatment because they
    // can't fit into the 16MB ROM buffer.
    u32 i;
    for(i = 0; i < gamepak_ram_pages; i++)
    {
      gamepak_memory_map[i].page_timestamp = 0;
      gamepak_memory_map[i].physical_index = 0;
    }

    map_null(read, 0x8000000, 0xD000000);
  }
  else
  {
    map_region(read, 0x8000000, 0x8000000 + gamepak_size, 1024, gamepak_rom);
    map_null(read, 0x8000000 + gamepak_size, 0xA000000);
    map_region(read, 0xA000000, 0xA000000 + gamepak_size, 1024, gamepak_rom);
    map_null(read, 0xA000000 + gamepak_size, 0xC000000);
    map_region(read, 0xC000000, 0xC000000 + gamepak_size, 1024, gamepak_rom);
    map_null(read, 0xC000000 + gamepak_size, 0xE000000);
  }
}

// TODO
void init_gamepak_buffer()
  {
    gamepak_rom = NULL;
    strcpy(gamepak_title, "gpSP");

#ifdef USE_EXT_MEM
    if(psp_model == PSP_2000)
      {
        gamepak_ram_buffer_size = 32 * 1024 * 1024;
        gamepak_rom = (u8 *)PSP2K_MEM_TOP;
        gamepak_rom_resume = malloc(4 * 1024 * 1024);
      }
    else
#endif
      {
        // Try to initialize 32MB
        gamepak_ram_buffer_size = 32 * 1024 * 1024;
        gamepak_rom = malloc(gamepak_ram_buffer_size);

        if(gamepak_rom == NULL)
          {
            // Try 14MB, for PSP, then lower in 1MB increments
            gamepak_ram_buffer_size = 31 * 1024 * 1024;
            gamepak_rom = malloc(gamepak_ram_buffer_size);

            while(gamepak_rom == NULL)
              {
                gamepak_ram_buffer_size -= (1 * 1024 * 1024);
                gamepak_rom = malloc(gamepak_ram_buffer_size);
              }
          }
      }
    // Here's assuming we'll have enough memory left over for this,
    // and that the above succeeded (if not we're in trouble all around)
    gamepak_ram_pages = gamepak_ram_buffer_size / (32 * 1024);
    gamepak_memory_map = malloc(sizeof(gamepak_swap_entry_type) * gamepak_ram_pages);
  }

void init_memory()
{
  u32 map_offset = 0;

  memory_regions[0x00] = (u8 *)bios_rom;
  memory_regions[0x01] = (u8 *)bios_rom;
  memory_regions[0x02] = (u8 *)ewram;
  memory_regions[0x03] = (u8 *)iwram + 0x8000;
  memory_regions[0x04] = (u8 *)io_registers;
  memory_regions[0x05] = (u8 *)palette_ram;
  memory_regions[0x06] = (u8 *)vram;
  memory_regions[0x07] = (u8 *)oam_ram;
  memory_regions[0x08] = (u8 *)gamepak_rom;
  memory_regions[0x09] = (u8 *)(gamepak_rom + 0xFFFFFF);
  memory_regions[0x0A] = (u8 *)gamepak_rom;
  memory_regions[0x0B] = (u8 *)(gamepak_rom + 0xFFFFFF);
  memory_regions[0x0C] = (u8 *)gamepak_rom;
  memory_regions[0x0D] = (u8 *)(gamepak_rom + 0xFFFFFF);
  memory_regions[0x0E] = (u8 *)gamepak_backup;

  memory_limits[0x00] = 0x3FFF;
  memory_limits[0x01] = 0x3FFF;
  memory_limits[0x02] = 0x3FFFF;
  memory_limits[0x03] = 0x7FFF;
  memory_limits[0x04] = 0x7FFF;
  memory_limits[0x05] = 0x3FF;
  memory_limits[0x06] = 0x17FFF;
  memory_limits[0x07] = 0x3FF;
  memory_limits[0x08] = 0x1FFFFFF;
  memory_limits[0x09] = 0x1FFFFFF;
  memory_limits[0x0A] = 0x1FFFFFF;
  memory_limits[0x0B] = 0x1FFFFFF;
  memory_limits[0x0C] = 0x1FFFFFF;
  memory_limits[0x0D] = 0x1FFFFFF;
  memory_limits[0x0E] = 0xFFFF;

  // Fill memory map regions, areas marked as NULL must be checked directly
  map_region(read, 0x0000000, 0x1000000, 1, bios_rom);
  map_null(read, 0x1000000, 0x2000000);
  map_ram_region(read, 0x2000000, 0x3000000, 8, ewram);
  map_ram_region(read, 0x3000000, 0x4000000, 1, iwram);
  map_region(read, 0x4000000, 0x5000000, 1, io_registers);
  map_null(read, 0x5000000, 0x6000000);
  map_vram(read);
  map_null(read, 0x7000000, 0x8000000);
  init_memory_gamepak();
  map_null(read, 0xE000000, 0x10000000);

  // Fill memory map regions, areas marked as NULL must be checked directly
  map_null(write, 0x0000000, 0x2000000);
  map_ram_region(write, 0x2000000, 0x3000000, 8, ewram);
  map_ram_region(write, 0x3000000, 0x4000000, 1, iwram);
  map_null(write, 0x4000000, 0x5000000);
  map_null(write, 0x5000000, 0x6000000);

  // The problem here is that the current method of handling self-modifying code
  // requires writeable memory to be proceeded by 32KB SMC data areas or be
  // indirectly writeable. It's possible to get around this if you turn off the SMC
  // check altogether, but this will make a good number of ROMs crash (perhaps most
  // of the ones that actually need it? This has yet to be determined).

  // This is because VRAM cannot be efficiently made incontiguous, and still allow
  // the renderer to work as efficiently. It would, at the very least, require a
  // lot of hacking of the renderer which I'm not prepared to do.

  // However, it IS possible to directly map the first page no matter what because
  // there's 32kb of blank stuff sitting beneath it.
  if(direct_map_vram)
  {
    map_vram(write);
  }
  else
  {
    map_null(write, 0x6000000, 0x7000000);
  }

  map_null(write, 0x7000000, 0x8000000);
  map_null(write, 0x8000000, 0xE000000);
  map_null(write, 0xE000000, 0x10000000);

  memset(io_registers, 0, sizeof(io_registers));
  memset(oam_ram, 0, sizeof(oam_ram));
  memset(palette_ram, 0, sizeof(palette_ram));
  memset(iwram, 0, sizeof(iwram));
  memset(ewram, 0, sizeof(ewram));
  memset(vram, 0, sizeof(vram));

  io_registers[REG_DISPCNT] = 0x80;
  io_registers[REG_P1] = 0x3FF;
  io_registers[REG_BG2PA] = 0x100;
  io_registers[REG_BG2PD] = 0x100;
  io_registers[REG_BG3PA] = 0x100;
  io_registers[REG_BG3PD] = 0x100;
  io_registers[REG_RCNT] = 0x8000;
  io_registers[REG_SIOCNT] = 0x000C;

  sram_size = SRAM_SIZE_32KB;
  flash_size = FLASH_SIZE_64KB;

  flash_bank_ptr = gamepak_backup;
  flash_command_position = 0;
  eeprom_size = EEPROM_512_BYTE;
  eeprom_mode = EEPROM_BASE_MODE;
  eeprom_address = 0;
  eeprom_counter = 0;

  flash_mode = FLASH_BASE_MODE;

  rtc_state = RTC_DISABLED;
  memset(rtc_registers, 0, sizeof(rtc_registers));
  bios_read_protect = 0xe129f000;

  load_last_state();
}

void bios_region_read_allow()
{
  memory_map_read[0] = bios_rom;
}

void bios_region_read_protect()
{
  memory_map_read[0] = NULL;
}


// type = read_mem / write_mem
// ver = 0(old_format) / 1(3.4 test 1 format) / 0x00010000(new format)
// The fields of the memory block that come before the ROM file name.
// savestate_rom_name() measures them to find the name in a savestate.
#define memory_savestate_head(type)                                           \
{                                                                             \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, backup_type);                    \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, sram_size);                      \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_mode);                     \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_command_position);         \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_bank_ptr);                 \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_device_id);                \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_manufacturer_id);          \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, flash_size);                     \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_size);                    \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_mode);                    \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_address_length);          \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_address);                 \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, eeprom_counter);                 \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_state);                      \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_write_mode);                 \
  FILE_##type##_ARRAY(g_state_buffer_ptr, rtc_registers);                     \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_command);                    \
  FILE_##type##_ARRAY(g_state_buffer_ptr, rtc_data);                          \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_status);                     \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_data_bytes);                 \
  FILE_##type##_VARIABLE(g_state_buffer_ptr, rtc_bit_count);                  \
  FILE_##type##_ARRAY(g_state_buffer_ptr, eeprom_buffer);                     \
}                                                                             \

#define memory_savestate_body(type, ver)                                      \
{                                                                             \
  u32 i;                                                                      \
                                                                              \
  memory_savestate_head(type);                                                \
  FILE_##type##_ARRAY(g_state_buffer_ptr, gamepak_filename);                  \
  if(ver == 0)                                                                \
    g_state_buffer_ptr += 256;                                                \
  FILE_##type##_ARRAY(g_state_buffer_ptr, dma);                               \
                                                                              \
  FILE_##type(g_state_buffer_ptr, iwram + 0x8000, 0x8000);                    \
  for(i = 0; i < 8; i++)                                                      \
  {                                                                           \
    FILE_##type(g_state_buffer_ptr, ewram + (i * 0x10000) + 0x8000, 0x8000);  \
  }                                                                           \
  FILE_##type(g_state_buffer_ptr, vram, 0x18000);                             \
  FILE_##type(g_state_buffer_ptr, oam_ram, 0x400);                            \
  FILE_##type(g_state_buffer_ptr, palette_ram, 0x400);                        \
  FILE_##type(g_state_buffer_ptr, io_registers, 0x8000);                      \
                                                                              \
  /* This is a hack, for now. */                                              \
  if((flash_bank_ptr < gamepak_backup) ||                                     \
   (flash_bank_ptr > (gamepak_backup + (1024 * 64))))                         \
  {                                                                           \
    flash_bank_ptr = gamepak_backup;                                          \
  }                                                                           \
}                                                                             \

void memory_read_mem_savestate(u32 ver)
memory_savestate_body(READ_MEM, ver);

void memory_write_mem_savestate(u32 ver)
memory_savestate_body(WRITE_MEM, ver);

void memory_get_size_savestate(u32 ver)
memory_savestate_body(GET_SIZE, ver);

// type = read_mem / write_mem
#define savestate_block(type, ver)                                       \
  cpu_##type##_savestate(ver);                                           \
  update_progress();                                                     \
  input_##type##_savestate(ver);                                         \
  update_progress();                                                     \
  main_##type##_savestate(ver);                                          \
  update_progress();                                                     \
  memory_##type##_savestate(ver);                                        \
  update_progress();                                                     \
  sound_##type##_savestate(ver);                                         \
  update_progress();                                                     \
  video_##type##_savestate(ver);                                         \
  update_progress();                                                     \

/*--------------------------------------------------------
  Find the ROM file name stored in a savestate
  input
    u8 *state_data  start of the emulator state, after the header,
                    screenshot and time stamp
    u32 ver         savestate format version
  return
    the stored name (MAX_FILE bytes, not necessarily terminated)
--------------------------------------------------------*/
static char *savestate_rom_name(u8 *state_data, u32 ver)
{
  u8 *saved_ptr = g_state_buffer_ptr;
  char *name;

  // Walk the blocks that come before the name the way savestate_block()
  // reads them, without reading anything.
  g_state_buffer_ptr = state_data;
  cpu_get_size_savestate(ver);
  input_get_size_savestate(ver);
  main_get_size_savestate(ver);
  memory_savestate_head(GET_SIZE);
  name = (char *)g_state_buffer_ptr;

  g_state_buffer_ptr = saved_ptr;
  return name;
}

// Show a savestate error and wait for a button.
static void savestate_error(char *text)
{
  show_progress(text);
  button_up_wait();
  error_msg("");
}

/*--------------------------------------------------------
  Load state
  input
    char *savestate_filename file to load
    u32 slot_num             slot number; also tells whether this is an in-memory load
  return
    0 on failure
    1 on success
--------------------------------------------------------*/
u32 load_state(char *savestate_filename, u32 slot_num)
{
  char savestate_path[MAX_PATH];
  FILE_ID savestate_file;
  char current_gamepak_filename[MAX_FILE];
  u32 i;
  u32 file_size = 0;
  char buf[256];
  u32 ver;
  SceIoStat stat;

  ver = 0x00010000;
  pause_sound(1);

  if (slot_num != MEM_STATE_NUM)
  {
    if (g_default_save_dir[0] != '\0') {
      path_printf(savestate_path, sizeof(savestate_path), "%s/%s", g_default_save_dir, savestate_filename);
    }
    else
    {
      strcpy(savestate_path, savestate_filename);
    }

    // An empty slot has nothing to load, so don't ask.
    if (sceIoGetstat(savestate_path, &stat) < 0)
      return 0;
  }
  else
    if (mem_save_flag == 0)
      return 0;

  sprintf(buf, msg[MSG_LOAD_STATE_NUM], (int)slot_num);
  if(yesno_dialog(buf) == 1)
    return 0;

  init_progress(9, msg[MSG_LOAD_STATE]);

  if (slot_num != MEM_STATE_NUM)
  {
    FILE_OPEN(savestate_file, savestate_path, READ);
    if(FILE_CHECK_VALID(savestate_file))
    {
      file_size = file_length(savestate_path);

      // Anything else is not a savestate, or a damaged one, and a larger
      // file would overflow the buffer.
      if((file_size < SAVESTATE_SIZE_OLD) ||
       (file_size > sizeof(savestate_write_buffer)))
      {
        FILE_CLOSE(savestate_file);
        savestate_error(msg[MSG_LOAD_STATE_ERROR]);
        return 0;
      }

      if(FILE_READ(savestate_file, savestate_write_buffer, file_size) !=
       (s32)file_size)
      {
        FILE_CLOSE(savestate_file);
        // Slot 10 shares this buffer, which now holds part of the file.
        mem_save_flag = 0;
        savestate_error(msg[MSG_LOAD_STATE_ERROR]);
        return 0;
      }
      FILE_CLOSE(savestate_file);
    }
    else
      return 0;
  }
  else
    if (mem_save_flag == 1)
      file_size = SAVESTATE_SIZE;
    else
      return 0;

  g_state_buffer_ptr = savestate_write_buffer;

  if(memcmp(g_state_buffer_ptr, g_state_str, 4) == 0)
  {
    g_state_buffer_ptr += 4;
    FILE_READ_MEM_VARIABLE(g_state_buffer_ptr, ver);
  }
  else
  {
    // Old version check
    ver = 0;
    u32 lp = 255;
    while(savestate_write_buffer[SAVESTATE_SIZE - lp] == 0)
    {
      lp--;
      if(lp == 0) break;
    }
    if(lp == 0) ver = 1;
  }

  g_state_buffer_ptr += (240 * 160 * 2) + sizeof(u64);

  // A savestate holds the whole machine, so applying one made with another
  // ROM runs that game's state on this ROM and freezes. Check the ROM file
  // name stored in it first, and leave the running game alone if it
  // differs. A savestate whose file name starts with this ROM's name is
  // accepted too, so states still load after the ROM file is renamed.
  {
    char *state_rom_name = savestate_rom_name(g_state_buffer_ptr, ver);
    u32 name_length = strcspn(gamepak_filename, ".");
    char *state_file_name = strrchr(savestate_filename, '/');

    state_file_name = state_file_name ? state_file_name + 1 : savestate_filename;

    if((gamepak_filename[0] == 0) ||
     ((strncmp(state_rom_name, gamepak_filename, MAX_FILE) != 0) &&
     (strncmp(state_file_name, gamepak_filename, name_length) != 0)))
    {
      // Slot 10 shares this buffer, which now holds the other game's state.
      mem_save_flag = 0;
      savestate_error(msg[MSG_LOAD_STATE_OTHER_GAME]);
      return 0;
    }
  }

  strcpy(current_gamepak_filename, gamepak_filename);
  update_progress();

  savestate_block(read_mem, ver);

  update_progress();

  flush_translation_cache_ram();
  flush_translation_cache_rom();
  flush_translation_cache_bios();
  update_progress();

  oam_update = 1;
  gbc_sound_update = 1;
  show_progress(msg[MSG_LOAD_STATE_END]);

  mem_save_flag = 1;

  // The savestate's copy of the ROM file name replaced ours; keep the name
  // of the file that is actually loaded (they differ after a rename).
  strcpy(gamepak_filename, current_gamepak_filename);

  // Oops, these contain raw pointers
  for(i = 0; i < 4; i++)
  {
    gbc_sound_channel[i].sample_data = square_pattern_duty[2];
  }

  reg[CHANGED_PC_STATUS] = 1;

  real_frame_count = 0;
  virtual_frame_count = 0;
  pause_sound(0);
  return 1;
}

/*--------------------------------------------------------
  Save state
  input
    char *savestate_filename file to save
    u16 *screen_capture      screenshot image
    u32 slot_num             slot number; also tells whether this is an in-memory save
  return
    0 on failure
    1 on success
--------------------------------------------------------*/
// Build a savestate of the running game in savestate_write_buffer (which is
// also slot 10), with a progress bar.
static void write_state_buffer(u16 *screen_capture)
{
  u64 current_time;
  ScePspDateTime current_time_fix; // time() does not return the date, so fix it up

  g_state_buffer_ptr = savestate_write_buffer;
  init_progress(9, msg[MSG_SAVE_STATE]);

  FILE_WRITE_MEM_VARIABLE(g_state_buffer_ptr, g_state_str);
  FILE_WRITE_MEM_VARIABLE(g_state_buffer_ptr, g_state_ver);

  FILE_WRITE_MEM(g_state_buffer_ptr, screen_capture, 240 * 160 * 2);
  update_progress();

  sceRtcGetCurrentClock(&current_time_fix, 0);
  sceRtcGetTick(&current_time_fix, &current_time);

  FILE_WRITE_MEM_VARIABLE(g_state_buffer_ptr, current_time);
  update_progress();

  savestate_block(write_mem, 0x00010000);
  mem_save_flag = 1;
}

/*--------------------------------------------------------
  Savestate pictures (.thm)
  Every savestate file <game>_N.svs gets a sidecar <game>_N.thm with a
  64 x 43 copy of its screenshot, so a menu can show all slots without
  reading eleven 512 KB savestates. Layout (little endian):
    0  "KTHM"      magic
    4  u8 1        version
    5  u8 64, 43   width, height
    7  u8 0        padding
    8  u64         the savestate's save time, as stored in the .svs; a
                   picture whose time differs belongs to an older save
   16  u16[43][64] pixels, COLOR16 like the screenshot
--------------------------------------------------------*/
#define THM_W 64
#define THM_H 43
#define THM_HEADER 16
#define SVS_SHOT_OFFSET (sizeof(g_state_str) + sizeof(g_state_ver))
#define SVS_TIME_OFFSET (SVS_SHOT_OFFSET + 240 * 160 * 2)

// <name>.svs -> <name>.thm
static void get_thumbnail_path(const char *savestate_path, char *thumbnail_path)
{
  size_t len = strlen(savestate_path);

  strcpy(thumbnail_path, savestate_path);
  if((len > 4) && (strcasecmp(thumbnail_path + len - 4, ".svs") == 0))
    strcpy(thumbnail_path + len - 4, ".thm");
}

// Write the sidecar for the savestate in savestate_write_buffer.
static void write_state_thumbnail(const char *savestate_path)
{
  static u8 thm[THM_HEADER + THM_W * THM_H * 2];
  const u16 *shot = (const u16 *)(savestate_write_buffer + SVS_SHOT_OFFSET);
  u16 *out = (u16 *)(thm + THM_HEADER);
  char thumbnail_path[MAX_PATH];
  FILE_ID thumbnail_file;
  u32 x, y, sx, sy;

  memcpy(thm, "KTHM", 4);
  thm[4] = 1;
  thm[5] = THM_W;
  thm[6] = THM_H;
  thm[7] = 0;
  memcpy(thm + 8, savestate_write_buffer + SVS_TIME_OFFSET, 8);

  // average each 240x160 -> 64x43 source box (3-4 px each way)
  for(y = 0; y < THM_H; y++)
  {
    u32 y0 = y * 160 / THM_H, y1 = (y + 1) * 160 / THM_H;
    for(x = 0; x < THM_W; x++)
    {
      u32 x0 = x * 240 / THM_W, x1 = (x + 1) * 240 / THM_W;
      u32 r = 0, g = 0, b = 0, n = (x1 - x0) * (y1 - y0);
      for(sy = y0; sy < y1; sy++)
        for(sx = x0; sx < x1; sx++)
        {
          u16 c = shot[sy * 240 + sx];
          r += c & 0x1F;
          g += (c >> 5) & 0x1F;
          b += (c >> 10) & 0x1F;
        }
      out[y * THM_W + x] = COLOR16((r + n / 2) / n, (g + n / 2) / n, (b + n / 2) / n);
    }
  }

  get_thumbnail_path(savestate_path, thumbnail_path);
  FILE_OPEN(thumbnail_file, thumbnail_path, WRITE);
  if(FILE_CHECK_VALID(thumbnail_file))
  {
    if(FILE_WRITE(thumbnail_file, thm, sizeof(thm)) != (s32)sizeof(thm))
    {
      FILE_CLOSE(thumbnail_file);
      sceIoRemove(thumbnail_path);  // no half-written picture
      return;
    }
    FILE_CLOSE(thumbnail_file);
  }
}

// Delete a savestate file and its picture.
void delete_state_files(const char *savestate_path)
{
  char thumbnail_path[MAX_PATH];

  get_thumbnail_path(savestate_path, thumbnail_path);
  sceIoRemove(savestate_path);
  sceIoRemove(thumbnail_path);
}

u32 save_state(char *savestate_filename, u16 *screen_capture, u32 slot_num)
{
  char savestate_path[MAX_PATH];
  FILE_ID savestate_file;
  char buf[256];

  pause_sound(1);

  sprintf(buf, msg[MSG_SAVE_STATE_NUM], (int)slot_num);
  if(yesno_dialog(buf) == 1)
    return 0;

  if (g_default_save_dir[0] != '\0') {
    path_printf(savestate_path, sizeof(savestate_path), "%s/%s", g_default_save_dir, savestate_filename);
  }
  else
  {
    strcpy(savestate_path, savestate_filename);
  }

  write_state_buffer(screen_capture);

  // Slot 10 is written to its file too, so it is kept after quitting.
  FILE_OPEN(savestate_file, savestate_path, WRITE);
  if(FILE_CHECK_VALID(savestate_file))
  {
    s32 written = FILE_WRITE(savestate_file, savestate_write_buffer, sizeof(savestate_write_buffer));
    FILE_CLOSE(savestate_file);
    if(written == (s32)sizeof(savestate_write_buffer))
      write_state_thumbnail(savestate_path);
  }

  update_progress();
  show_progress(msg[MSG_SAVE_STATE_END]);
  real_frame_count = 0;
  virtual_frame_count = 0;
  pause_sound(0);
  return 1;
}

/*--------------------------------------------------------
  Slot 10 on the Memory Stick
  Slot 10 is savestate_write_buffer: the last state saved, loaded or made
  on leaving a game. <game>_10.svs keeps it between sessions: it is
  written when the player leaves a game and read back when the game
  starts, so the game can continue from where it was left.
--------------------------------------------------------*/
static void get_last_state_path(char *savestate_path)
{
  char savestate_filename[MAX_FILE];

  get_savestate_filename_noshot(MEM_STATE_NUM, savestate_filename);
  if (g_default_save_dir[0] != '\0')
    path_printf(savestate_path, MAX_PATH, "%s/%s", g_default_save_dir, savestate_filename);
  else
    strcpy(savestate_path, savestate_filename);
}

// Save the running game into slot 10 and its file, without asking.
u32 save_last_state(u16 *screen_capture)
{
  char savestate_path[MAX_PATH];
  FILE_ID savestate_file;
  u32 ret = 0;

  if (gamepak_filename[0] == 0)
    return 0;

  pause_sound(1);
  write_state_buffer(screen_capture);

  get_last_state_path(savestate_path);
  FILE_OPEN(savestate_file, savestate_path, WRITE);
  if(FILE_CHECK_VALID(savestate_file))
  {
    if(FILE_WRITE(savestate_file, savestate_write_buffer, sizeof(savestate_write_buffer)) ==
     (s32)sizeof(savestate_write_buffer))
      ret = 1;
    FILE_CLOSE(savestate_file);
    if(ret)
      write_state_thumbnail(savestate_path);
  }
  update_progress();
  return ret;
}

// Fill slot 10 from the file, if this game has one.
void load_last_state(void)
{
  char savestate_path[MAX_PATH];
  FILE_ID savestate_file;
  SceIoStat stat;
  u32 file_size;

  mem_save_flag = 0;
  if (gamepak_filename[0] == 0)
    return;

  get_last_state_path(savestate_path);
  if (sceIoGetstat(savestate_path, &stat) < 0)
    return;

  file_size = file_length(savestate_path);
  if((file_size < SAVESTATE_SIZE_OLD) || (file_size > sizeof(savestate_write_buffer)))
    return;

  FILE_OPEN(savestate_file, savestate_path, READ);
  if(FILE_CHECK_VALID(savestate_file))
  {
    if(FILE_READ(savestate_file, savestate_write_buffer, file_size) == (s32)file_size)
      mem_save_flag = 1;
    FILE_CLOSE(savestate_file);
  }
}

// Delete slot 10 and its file.
void delete_last_state(void)
{
  char savestate_path[MAX_PATH];

  mem_save_flag = 0;
  get_last_state_path(savestate_path);
  delete_state_files(savestate_path);
}

u32 get_sio_mode(u16 io1, u16 io2)
{
  if(!(io2 & 0x8000))
  {
    switch(io1 & 0x3000)
    {
      case 0x0000:
        return SIO_NORMAL8;
      case 0x1000:
        return SIO_NORMAL32;
      case 0x2000:
        return SIO_MULTIPLAYER;
      case 0x3000:
        return SIO_UART;
    }
  }
  if(io2 & 0x4000)
  {
    return SIO_JOYBUS;
  }
  return SIO_GP;
}

// Send multiplayer-mode data
u32 send_adhoc_multi()
{
  return 0;
}
