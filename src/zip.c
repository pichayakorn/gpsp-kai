/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2006 SiberianSTAR
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

#define ZIP_BUFFER_SIZE (320 * 1024) // 320KB

struct SZIPFileDataDescriptor
{
  s32 CRC32;
  s32 CompressedSize;
  s32 UncompressedSize;
} __attribute__((packed));

struct SZIPFileHeader
{
  char Sig[4]; // EDIT: Used to be s32 Sig;
  s16 VersionToExtract;
  s16 GeneralBitFlag;
  s16 CompressionMethod;
  s16 LastModFileTime;
  s16 LastModFileDate;
  struct SZIPFileDataDescriptor DataDescriptor;
  s16 FilenameLength;
  s16 ExtraFieldLength;
}  __attribute__((packed));

// Load a ROM from a ZIP file. The first entry must be a .gba or .bin file.
// Returns the ROM size, -2 if the ROM was written to ZIP_TMP because it is
// larger than the ROM buffer, or -1 on any error: not a ZIP, an unsupported
// entry (encrypted, sizes stored after the data, unknown compression), or
// damaged or truncated data (checked against the entry's CRC-32).
s32 load_file_zip(char *filename)
{
  struct SZIPFileHeader data;
  char tmp[MAX_PATH];
  s32 retval = -1;
  u8 *buffer = gamepak_rom;
  u8 *cbuffer;
  char *ext;
  FILE_ID fd;
  FILE_ID tmp_fd = -1;
  u32 zip_buffer_size;
  u32 use_vram = NO;
  u32 name_length;
  u32 rom_size;
  u32 out_size;
  uLong crc = crc32(0L, Z_NULL, 0);

  zip_buffer_size = 16 * 1024 * 1024;
  cbuffer = malloc(zip_buffer_size);

  while(cbuffer == NULL)
  {
    zip_buffer_size -= (256 * 1024);
    if(zip_buffer_size == 0) break;
    cbuffer = malloc(zip_buffer_size);
  }

  if(zip_buffer_size == 0)
  {
    use_vram = YES;
    zip_buffer_size = ZIP_BUFFER_SIZE;
    cbuffer = (u8 *)UNIVERSAL_VRAM_ADDR; // borrow the general-purpose VRAM buffer
  }

  chdir(rom_path);
  FILE_OPEN(fd, filename, READ);

  if(!FILE_CHECK_VALID(fd))
    goto free_buffer;

  if(FILE_READ(fd, &data, sizeof(data)) != sizeof(data))
    goto close_zip;

  // Local file header signature "PK\3\4"
  if((data.Sig[0] != 0x50) || (data.Sig[1] != 0x4B) ||
   (data.Sig[2] != 0x03) || (data.Sig[3] != 0x04))
    goto close_zip;

  // Bit 0: encrypted. Bit 3: the sizes follow the data, so the header
  // fields read below are zero.
  if(data.GeneralBitFlag & 0x0009)
    goto close_zip;

  name_length = (u16)data.FilenameLength;
  if((name_length == 0) || (name_length >= MAX_PATH))
    goto close_zip;

  if(FILE_READ(fd, tmp, name_length) != name_length)
    goto close_zip;
  tmp[name_length] = 0;

  ext = strrchr(tmp, '.');
  if((ext == NULL) ||
   (strncasecmp(ext + 1, "bin", 3) && strncasecmp(ext + 1, "gba", 3)))
    goto close_zip;

  if(data.ExtraFieldLength)
    FILE_SEEK(fd, (u16)data.ExtraFieldLength, SEEK_CUR);

  // A GBA ROM is at most 32 MB
  rom_size = (u32)data.DataDescriptor.UncompressedSize;
  if((rom_size == 0) || (rom_size > 32 * 1024 * 1024))
    goto close_zip;

  // Larger than the ROM buffer: write it to a temp file and page it in later
  if(rom_size > gamepak_ram_buffer_size)
  {
    FILE_OPEN(tmp_fd, ZIP_TMP, WRITE);
    if(!FILE_CHECK_VALID(tmp_fd))
      goto close_zip;
    out_size = gamepak_ram_buffer_size;
  }
  else
    out_size = rom_size;

  switch(data.CompressionMethod)
  {
    case 0: // stored
    {
      u32 left = rom_size;

      while(left > 0)
      {
        u32 chunk = (left < out_size) ? left : out_size;

        if(FILE_READ(fd, buffer, chunk) != chunk)
          goto close_zip;
        crc = crc32(crc, buffer, chunk);
        if(FILE_CHECK_VALID(tmp_fd) && (FILE_WRITE(tmp_fd, buffer, chunk) != chunk))
          goto close_zip;
        left -= chunk;
      }
      break;
    }

    case 8: // deflate
    {
      z_stream stream = {0};
      s32 err;
      s32 read_size;
      u32 ok = NO;

      if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        goto close_zip;

      stream.next_out = (Bytef *)buffer;
      stream.avail_out = out_size;

      while(1)
      {
        if(stream.avail_in == 0)
        {
          read_size = FILE_READ(fd, cbuffer, zip_buffer_size);
          if(read_size <= 0)
            break; // truncated
          sceKernelDcacheWritebackAll();
          stream.next_in = (Bytef *)cbuffer;
          stream.avail_in = read_size;
        }

        err = inflate(&stream, Z_NO_FLUSH);

        if(FILE_CHECK_VALID(tmp_fd) &&
         ((stream.avail_out == 0) || (err == Z_STREAM_END)))
        {
          u32 used = out_size - stream.avail_out;

          crc = crc32(crc, buffer, used);
          if(FILE_WRITE(tmp_fd, buffer, used) != used)
            break;
          stream.next_out = (Bytef *)buffer;
          stream.avail_out = out_size;
        }

        if(err == Z_STREAM_END)
        {
          if(!FILE_CHECK_VALID(tmp_fd))
            crc = crc32(crc, buffer, stream.total_out);
          ok = (stream.total_out == rom_size) ? YES : NO;
          break;
        }

        // Damaged data, or more data than the header says (no room left
        // in the output while input remains)
        if((err != Z_OK) && ((err != Z_BUF_ERROR) || (stream.avail_in != 0)))
          break;
      }

      inflateEnd(&stream);
      if(ok == NO)
        goto close_zip;
      break;
    }

    default:
      goto close_zip;
  }

  // The CRC catches damage that still decompresses, e.g. in stored blocks
  if(crc != (u32)data.DataDescriptor.CRC32)
    goto close_zip;

  retval = FILE_CHECK_VALID(tmp_fd) ? -2 : (s32)rom_size;

close_zip:
  FILE_CLOSE(fd);

  if(FILE_CHECK_VALID(tmp_fd))
  {
    FILE_CLOSE(tmp_fd);
    if(retval == -1)
      FILE_DELETE(ZIP_TMP);
  }

free_buffer:
  if(use_vram == NO)
    free(cbuffer);

  chdir(main_path);
  return retval;
}
