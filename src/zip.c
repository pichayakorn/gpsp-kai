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

#define ZIP_EOCD_SIZE        22
#define ZIP_CDIR_HEADER_SIZE 46
#define ZIP_MAX_COMMENT      0xFFFF

static u32 zip_le16(const u8 *p)
{
  return p[0] | (p[1] << 8);
}

static u32 zip_le32(const u8 *p)
{
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
}

// For an entry whose general purpose flag has bit 3 set, the local header
// holds zero for the CRC-32 and the sizes. Take them from the entry's
// central directory record instead, which is found through the end of
// central directory record at the end of the file. Only the entry whose
// local header is at offset 0 is looked up, since that is the one loaded.
// work must hold at least ZIP_EOCD_SIZE + ZIP_MAX_COMMENT bytes.
// Returns 0 on success, -1 if the records are missing or inconsistent.
static s32 zip_central_sizes(FILE_ID fd, u32 method, u8 *work,
 struct SZIPFileDataDescriptor *desc)
{
  u8 *eocd = NULL;
  u8 *hdr;
  s32 file_size;
  u32 tail_size;
  u32 cdir_offset;
  u32 cdir_end;
  u32 entries;
  u32 pos;
  s32 i;

  file_size = FILE_SEEK(fd, 0, SEEK_END);
  if(file_size < ZIP_EOCD_SIZE)
    return -1;

  tail_size = ZIP_EOCD_SIZE + ZIP_MAX_COMMENT;
  if(tail_size > (u32)file_size)
    tail_size = file_size;

  FILE_SEEK(fd, file_size - tail_size, SEEK_SET);
  if(FILE_READ(fd, work, tail_size) != tail_size)
    return -1;

  // Search backwards for "PK\5\6" whose comment length reaches exactly the
  // end of the file, so a signature inside the comment is not taken
  for(i = tail_size - ZIP_EOCD_SIZE; i >= 0; i--)
  {
    u8 *p = work + i;

    if((p[0] == 0x50) && (p[1] == 0x4B) && (p[2] == 0x05) && (p[3] == 0x06) &&
     (zip_le16(p + 20) == tail_size - i - ZIP_EOCD_SIZE))
    {
      eocd = p;
      break;
    }
  }

  if(eocd == NULL)
    return -1;

  // Multi-disk archives are not supported. ZIP64 archives store all ones
  // in the offset, which fails the range check below.
  if((zip_le16(eocd + 4) != 0) || (zip_le16(eocd + 6) != 0))
    return -1;

  entries = zip_le16(eocd + 10);
  cdir_offset = zip_le32(eocd + 16);
  cdir_end = (u32)file_size - tail_size + i;
  if((cdir_offset > cdir_end) || (zip_le32(eocd + 12) > cdir_end - cdir_offset))
    return -1;
  cdir_end = cdir_offset + zip_le32(eocd + 12);

  hdr = work;
  pos = cdir_offset;
  while(entries > 0)
  {
    if(pos + ZIP_CDIR_HEADER_SIZE > cdir_end)
      return -1;

    FILE_SEEK(fd, pos, SEEK_SET);
    if(FILE_READ(fd, hdr, ZIP_CDIR_HEADER_SIZE) != ZIP_CDIR_HEADER_SIZE)
      return -1;

    // Central directory file header signature "PK\1\2"
    if((hdr[0] != 0x50) || (hdr[1] != 0x4B) ||
     (hdr[2] != 0x01) || (hdr[3] != 0x02))
      return -1;

    if(zip_le32(hdr + 42) == 0)
    {
      // Same compression method as the local header, and stored data must
      // not change size
      if(zip_le16(hdr + 10) != method)
        return -1;
      if((method == 0) && (zip_le32(hdr + 20) != zip_le32(hdr + 24)))
        return -1;

      desc->CRC32 = zip_le32(hdr + 16);
      desc->CompressedSize = zip_le32(hdr + 20);
      desc->UncompressedSize = zip_le32(hdr + 24);
      return 0;
    }

    pos += ZIP_CDIR_HEADER_SIZE + zip_le16(hdr + 28) + zip_le16(hdr + 30) +
     zip_le16(hdr + 32);
    entries--;
  }

  return -1;
}

// Load a ROM from a ZIP file. The first entry must be a .gba or .bin file.
// Returns the ROM size, -2 if the ROM was written to ZIP_TMP because it is
// larger than the ROM buffer, or -1 on any error: not a ZIP, an unsupported
// entry (encrypted, unknown compression), a missing or inconsistent central
// directory when the sizes are stored after the data, or damaged or
// truncated data (checked against the entry's CRC-32).
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

  // Bit 0: encrypted
  if(data.GeneralBitFlag & 0x0001)
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

  // Bit 3: the CRC-32 and sizes follow the data, so the local header holds
  // zero for them. Read them from the central directory, then come back to
  // the start of the data.
  if(data.GeneralBitFlag & 0x0008)
  {
    if(zip_central_sizes(fd, (u16)data.CompressionMethod, cbuffer,
     &data.DataDescriptor) != 0)
      goto close_zip;
    FILE_SEEK(fd, sizeof(data) + name_length + (u16)data.ExtraFieldLength,
     SEEK_SET);
  }
  else if(data.ExtraFieldLength)
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
