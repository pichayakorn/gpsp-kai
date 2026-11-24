/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
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

/* cheat_engine.c - kai 4.0's cheat engine (#166, part 1)
 *
 * Files. A game's cheats come from GBA/CHT/<rom name>.cht in any of three
 * shapes, line by line:
 *   - kai's own: a header line "gameshark_v1 Name" (also gameshark_v2/v3,
 *     PAR_v1/v2/v3, direct_v1/v3), then "XXXXXXXX YYYYYYYY" code lines up to
 *     a blank line. Kept exactly as kai has always run them, so existing
 *     files work unchanged; the one difference is that ROM patches (v1
 *     opcode 6) go in once when the cheat is switched on and the original
 *     bytes come back when it is switched off.
 *   - the cheat-database format: cheatN_desc = "...", cheatN_code = "..."
 *     ('+' joins lines). cheatN_enable is ignored: a downloaded file never
 *     switches a cheat on.
 *   - a plain list: a name line, then code lines. "[Name]" or "Name" in
 *     quotes for a name that would read as a code; # and // are comments.
 *
 * Codes in the last two: CodeBreaker (8 + 4 digits), raw "address:value"
 * writes, and GameShark v1/v2 or Action Replay v3 marked "GS1:", "AR3:",
 * "GS1RAW:" or "AR3RAW:". They may only read and write the work RAM; a
 * code that would touch the save, I/O, video or ROM can't be switched on,
 * and bare 8 + 8 digit codes wait for the player to say which kind they
 * are. Ported from another GPL-2.0 PSP GBA emulator; the cipher tables in
 * cheats_gs_cipher.h keep their own MPL-2.0 notice.
 *
 * What is on. GBA/CFG/<rom name>.on lists the cheats that are on, one
 * "HHHHHHHH O|L name" line each: a hash of the cheat's name and code, so
 * reordering or rewrapping the file keeps the choice. L: locked on.
 */

#include "common.h"
#include "cheat_engine.h"

extern u32 gamepak_size;

#define CE_RECORDS      12288  /* code records of every cheat, shared */
#define CE_PATCHES      4096   /* ROM patch halfwords of every cheat */
#define CE_FRAME_MAX    8192   /* worst-case RAM reads and writes a frame */
#define CE_SLIDE_MAX    4096u
#define CE_CODE_TEXT    4096   /* one cheat's code text while it is read */
#define CE_DB_TEXT      (64 * 1024)  /* a database file's descriptions and codes */

typedef struct
{
  u32 address;
  u32 value;
} cheat_rec;

typedef struct
{
  u32 offset;       /* in the ROM */
  u16 value, old;   /* the patch, the byte pair it replaced */
  u8 has_old;
} rom_patch;

enum { FAM_CB, FAM_GS, FAM_KAI1, FAM_KAI3 };

typedef struct
{
  char name[CE_NAME_LENGTH];
  u32 hash;
  u32 first, count;               /* records */
  u32 first_patch, patches;       /* ROM patches */
  u32 work;                       /* worst-case accesses a frame */
  u8 family, status, on, locked;
} cheat_entry;

static cheat_entry ce[CE_MAX_CHEATS];
static u32 ce_count;
static cheat_rec ce_recs[CE_RECORDS];
static u32 ce_rec_used;
static rom_patch ce_patch[CE_PATCHES];
static u32 ce_patch_used;
static u32 ce_work_used;
static char ce_rom[MAX_FILE];
static u32 ce_smc_pending;
static u32 ce_bad_region;  /* the region of the last code refused for it */

/* ---- work RAM access (the only memory new-format codes can touch) -----------
 * kai keeps the translation tag of every RAM halfword beside its data: EWRAM
 * in 64 KB pages (32 KB tags, then 32 KB data), IWRAM as 32 KB tags then
 * 32 KB data. A write that changes bytes the recompiler has translated
 * flushes the RAM cache once, after every cheat has run. A write that
 * doesn't change anything is not made. */

static u8 *ce_ram(u32 address, u8 **tag)
{
  switch((address >> 24) & 0xF)
  {
    case 0x2:
    {
      u32 o = address & 0x3FFFF;
      *tag = ewram + ((o >> 15) * 0x10000) + (o & 0x7FFF);
      return *tag + 0x8000;
    }
    case 0x3:
      *tag = iwram + (address & 0x7FFF);
      return *tag + 0x8000;
    default:
      return NULL;
  }
}

static void ce_w8(u32 address, u8 value)
{
  u8 *tag, *p = ce_ram(address, &tag);
  if(!p || (*p == value))
    return;
  *p = value;
  if(*tag)
    ce_smc_pending = 1;
}

static void ce_w16(u32 address, u16 value)
{
  u8 *tag, *p = ce_ram(address & ~1U, &tag);
  if(!p || (ADDRESS16(p, 0) == value))
    return;
  ADDRESS16(p, 0) = value;
  if(ADDRESS16(tag, 0))
    ce_smc_pending = 1;
}

static void ce_w32(u32 address, u32 value)
{
  u8 *tag, *p = ce_ram(address & ~3U, &tag);
  if(!p || (ADDRESS32(p, 0) == value))
    return;
  ADDRESS32(p, 0) = value;
  if(ADDRESS16(tag, 0) || ADDRESS16(tag, 2))
    ce_smc_pending = 1;
}

static u32 ce_r8(u32 address)
{
  u8 *tag, *p = ce_ram(address, &tag);
  return p ? *p : 0;
}

static u32 ce_r16(u32 address)
{
  u8 *tag, *p = ce_ram(address & ~1U, &tag);
  return p ? ADDRESS16(p, 0) : 0;
}

static u32 ce_r32(u32 address)
{
  u8 *tag, *p = ce_ram(address & ~3U, &tag);
  return p ? ADDRESS32(p, 0) : 0;
}

static int ce_in_ram(u32 address)
{
  u32 r = (address >> 24) & 0xF;
  if((r == 0x2) || (r == 0x3))
    return 1;
  ce_bad_region = r;
  return 0;
}

/* ---- CodeBreaker ------------------------------------------------------------- */

typedef enum
{
  CE_E_NONE = 0, CE_E_SYNTAX, CE_E_GAMESHARK, CE_E_REGION, CE_E_ALIGN, CE_E_MIXED,
  CE_E_UNSUPPORTED, CE_E_ENCRYPTED, CE_E_TOOBIG, CE_E_WORK
} ce_error;

/* records one CodeBreaker code takes: a slide (4) and a super code (5)
   carry payload records */
static u32 cb_len(const cheat_rec *r, u32 count, u32 i)
{
  u32 op = r[i].address >> 28;
  (void)count;
  if(op == 4)
    return 2;
  if(op == 5)
    return 1 + ((u32)(u16)r[i].value * 2 + 5) / 6;
  return 1;
}

/* records to skip when a compare skips "the next code": all of it */
static u32 cb_next(const cheat_rec *r, u32 count, u32 i)
{
  return (i + 1 < count) ? cb_len(r, count, i + 1) : 1;
}

static void run_codebreaker(const cheat_entry *c, u16 pad)
{
  const cheat_rec *r = ce_recs + c->first;
  u32 i, j, n = c->count;

  for(i = 0; i < n; i++)
  {
    u32 code = r[i].address, address = code & 0xFFFFFFF;
    u16 value = r[i].value;

    switch(code >> 28)
    {
      case 0x0:  /* game id */
      case 0x1:  /* master code: nothing is hooked */
        break;
      case 0x2:  /* 16-bit OR */
        ce_w16(address, ce_r16(address) | value);
        break;
      case 0x3:  /* 8-bit write */
        ce_w8(address, value);
        break;
      case 0x4:  /* slide: count, value step, address step */
        if(i + 1 < n)
        {
          u16 count = r[++i].address, vstep = r[i].address >> 16, astep = r[i].value;
          for(j = 0; j < count; j++)
          {
            ce_w16(address, value);
            address += astep;
            value += vstep;
          }
        }
        break;
      case 0x5:  /* super code: bytes from the records after it */
        if(((u32)value * 2 + 5) / 6 > n - i - 1)
        {
          i = n;
          break;
        }
        for(j = 0; j < (u32)value * 2; j++)
        {
          u8 b, off = j % 6;
          if(off == 0)
            b = r[++i].address >> 24;
          else if(off <= 3)
            b = r[i].address >> (24 - off * 8);
          else
            b = r[i].value >> (40 - off * 8);
          ce_w8(address++, b);
        }
        break;
      case 0x6:  /* 16-bit AND */
        ce_w16(address, ce_r16(address) & value);
        break;
      case 0x7:  /* run the next code if equal */
        if(ce_r16(address) != value)
          i += cb_next(r, n, i);
        break;
      case 0x8:  /* 16-bit write */
        ce_w16(address, value);
        break;
      case 0xA:  /* skip the next code if equal */
        if(ce_r16(address) == value)
          i += cb_next(r, n, i);
        break;
      case 0xB:
        if(ce_r16(address) <= value)
          i += cb_next(r, n, i);
        break;
      case 0xC:
        if(ce_r16(address) >= value)
          i += cb_next(r, n, i);
        break;
      case 0xD:  /* buttons */
        switch((address >> 4) & 0xF)
        {
          case 0:
            if(((~pad) & 0x3FF) == value)
              i += cb_next(r, n, i);
            break;
          case 1:
            if((pad & value) == value)
              i += cb_next(r, n, i);
            break;
          case 2:
            if((pad & value) == 0)
              i += cb_next(r, n, i);
            break;
        }
        break;
      case 0xE:  /* add, 16 or (address bit 0) 32 bits */
        if(address & 1)
        {
          address &= ~1U;
          ce_w32(address, ce_r32(address) + (u32)(s32)(s16)value);
        }
        else
          ce_w16(address, ce_r16(address) + value);
        break;
      case 0xF:  /* skip the next code if no bit in common */
        if((ce_r16(address) & value) == 0)
          i += cb_next(r, n, i);
        break;
    }
  }
}

/* ---- the code text reader ------------------------------------------------------ */

static int is_sep(char c)
{
  return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '+') ||
   (c == ',') || (c == ';');
}

static int hex_n(const char *p, u32 n, u32 *out)
{
  u32 v = 0, i;

  if((n == 0) || (n > 8))
    return 0;
  for(i = 0; i < n; i++)
  {
    char c = p[i];
    v <<= 4;
    if((c >= '0') && (c <= '9')) v |= c - '0';
    else if((c >= 'a') && (c <= 'f')) v |= c - 'a' + 10;
    else if((c >= 'A') && (c <= 'F')) v |= c - 'A' + 10;
    else return 0;
  }
  *out = v;
  return 1;
}

static int all_hex(const char *p, u32 n)
{
  u32 d, i;

  if(n == 0)
    return 0;
  for(i = 0; i < n; i += 8)
    if(!hex_n(p + i, (n - i > 8) ? 8 : n - i, &d))
      return 0;
  return 1;
}

static int token(const char **s, const char **t, u32 *n)
{
  const char *p = *s;

  while(*p && is_sep(*p))
    p++;
  if(!*p)
  {
    *s = p;
    return 0;
  }
  *t = p;
  while(*p && !is_sep(*p))
    p++;
  *n = p - *t;
  *s = p;
  return 1;
}

static int push(cheat_entry *c, u32 address, u32 value)
{
  if(c->first + c->count >= CE_RECORDS)
    return 0;
  ce_recs[c->first + c->count].address = address;
  ce_recs[c->first + c->count].value = value;
  c->count++;
  return 1;
}

/* "GS1:", "GS1RAW:", "AR3:", "AR3RAW:" at the start, any case */
enum { TAG_GS1 = 1, TAG_GS1_RAW, TAG_AR3, TAG_AR3_RAW };
static u32 family_tag(const char *s, u32 n, u32 *used)
{
  static const char *const tags[] = { "GS1:", "GS1RAW:", "AR3:", "AR3RAW:" };
  u32 k, i;

  for(k = 0; k < 4; k++)
  {
    for(i = 0; tags[k][i] && (i < n); i++)
    {
      char c = s[i];
      if((c >= 'a') && (c <= 'z'))
        c += 'A' - 'a';
      if(c != tags[k][i])
        break;
    }
    if(!tags[k][i])
    {
      *used = i;
      return k + 1;
    }
  }
  return 0;
}

#include "cheats_gs_cipher.h"

/* GameShark / Action Replay, decoded once into private records:
 * top nibble 0/1/2 write 8/16/32; 3/4/5 a fill (count record follows);
 * 6/7/8 compare 8/16/32 (a record with the comparison and how many whole
 * operations to skip follows). */
enum { GS_EQ, GS_NE, GS_LE, GS_GE, GS_ULT, GS_UGT, GS_AND };

static u32 gs_len(const cheat_rec *r, u32 i)
{
  return ((r[i].address >> 28) >= 3) ? 2 : 1;
}

static void run_gs(const cheat_entry *c)
{
  const cheat_rec *r = ce_recs + c->first;
  u32 i = 0, n = c->count;

  while(i < n)
  {
    u32 a = r[i].address & 0x0FFFFFFF, v = r[i].value, op = r[i].address >> 28;
    u32 width = 1u << (op % 3), count = 1;
    i++;
    if(op >= 6)
    {
      u32 got, cond = r[i].address, skip = r[i++].value;
      int pass = 0;
      got = (width == 1) ? ce_r8(a) : (width == 2) ? ce_r16(a) : ce_r32(a);
      switch(cond)
      {
        case GS_EQ:  pass = got == v; break;
        case GS_NE:  pass = got != v; break;
        case GS_LE:  pass = got <= v; break;
        case GS_GE:  pass = got >= v; break;
        case GS_ULT: pass = got < v; break;
        case GS_UGT: pass = got > v; break;
        case GS_AND: pass = (got & v) != 0; break;
      }
      if(!pass)
        while(skip-- && (i < n))
          i += gs_len(r, i);
      continue;
    }
    if(op >= 3)
      count = r[i++].value;
    while(count--)
    {
      if(width == 1) ce_w8(a, v);
      else if(width == 2) ce_w16(a, v);
      else ce_w32(a, v);
      a += width;
    }
  }
}

static ce_error gs_emit(cheat_entry *c, u32 a, u32 v, u32 width, u32 count, int cond,
 u32 skip)
{
  u32 op = (width == 1) ? 0 : (width == 2) ? 1 : 2, last;

  if(!count)
    return CE_E_UNSUPPORTED;
  if(count > CE_FRAME_MAX)
    return CE_E_WORK;
  if(!ce_in_ram(a))
    return CE_E_REGION;
  if(a & (width - 1))
    return CE_E_ALIGN;
  last = a + count * width - 1;
  if((last < a) || ((last >> 24) != (a >> 24)))
    return CE_E_REGION;
  if((width < 4) && (v >= (1u << (width * 8))))
    return CE_E_SYNTAX;
  if(count > CE_FRAME_MAX - ce_work_used - c->work)
    return CE_E_WORK;
  c->work += count;
  if(cond >= 0)
    op += 6;
  else if(count > 1)
    op += 3;
  if(!push(c, a | (op << 28), v))
    return CE_E_TOOBIG;
  if(cond >= 0)
  {
    if(!push(c, (u32)cond, skip))
      return CE_E_TOOBIG;
  }
  else if((count > 1) && !push(c, 0, count))
    return CE_E_TOOBIG;
  return CE_E_NONE;
}

static ce_error gs_compile(cheat_entry *c, const char *s, u32 family)
{
  static const u32 initial[2][4] =
  {
    { 0x09F4FBBDu, 0x9681884Au, 0x352027E9u, 0xF3DEE5A7u },
    { 0x7AA9648Fu, 0x7FAE6994u, 0xC0EFAAD5u, 0x42712C57u }
  };
  int ar3 = family >= TAG_AR3;
  int encrypted = (family == TAG_GS1) || (family == TAG_AR3);
  u32 seeds[4], n, n2, i;
  const char *t, *t2;

  memcpy(seeds, initial[ar3], sizeof(seeds));
  c->family = FAM_GS;
  while(token(&s, &t, &n))
  {
    u32 a, v, addr, width, count = 1, skip = 1, used;
    int cond = -1;
    ce_error err;
    if(family_tag(t, n, &used) || memchr(t, ':', n))
      return CE_E_MIXED;
    if(n == 16)
    {
      if(!hex_n(t, 8, &a) || !hex_n(t + 8, 8, &v))
        return CE_E_SYNTAX;
    }
    else if(n == 8)
    {
      if(!hex_n(t, 8, &a) || !token(&s, &t2, &n2))
        return CE_E_SYNTAX;
      if((n2 == 4) && all_hex(t2, n2))
        return CE_E_MIXED;
      if((n2 != 8) || !hex_n(t2, 8, &v))
        return CE_E_SYNTAX;
    }
    else
      return CE_E_SYNTAX;
    if(encrypted)
      cheat_gs_decrypt(&a, &v, seeds);
    if(a == 0xDEADFACEu)
    {
      if(v > 0xFFFF)
        return CE_E_UNSUPPORTED;
      cheat_gs_reseed(seeds, (u16)v, ar3);
      continue;
    }
    if(!ar3)
    {
      u32 type = a >> 28;
      addr = a & 0x0FFFFFFF;
      if(type <= 2)
        width = 1u << type;
      else if(type == 0xD)
      {
        static const int conditions[4] = { GS_EQ, GS_NE, GS_LE, GS_GE };
        if(v & 0xFFCF0000u)
          return CE_E_UNSUPPORTED;
        cond = conditions[v >> 20];
        v &= 0xFFFF;
        width = 2;
      }
      else
        return CE_E_UNSUPPORTED;
    }
    else
    {
      u32 compare = (a >> 27) & 7, action = a >> 30, w = (a >> 25) & 3;
      if(!a || (v == 0x001DC0DEu) || (a & 0x01000000u) || (w == 3))
        return CE_E_UNSUPPORTED;
      width = 1u << w;
      addr = (a & 0xFFFFF) | ((a << 4) & 0x0F000000u);
      if(compare)
      {
        if(action > 1)
          return CE_E_UNSUPPORTED;
        skip = action + 1;
        switch(compare)
        {
          case 1: cond = GS_EQ; break;
          case 2: cond = GS_NE; break;
          case 5: cond = GS_ULT; break;
          case 6: cond = GS_UGT; break;
          case 7: cond = GS_AND; break;
          default: return CE_E_UNSUPPORTED;
        }
      }
      else
      {
        if(action)
          return CE_E_UNSUPPORTED;
        if(width < 4)
        {
          count = (v >> (width * 8)) + 1;
          v &= (1u << (width * 8)) - 1;
        }
      }
    }
    err = gs_emit(c, addr, v, width, count, cond, skip);
    if(err != CE_E_NONE)
      return err;
  }
  if(!c->count)
    return CE_E_SYNTAX;
  /* a compare needs the operations it controls */
  for(i = 0; i < c->count; i += gs_len(ce_recs + c->first, i))
    if((ce_recs[c->first + i].address >> 28) >= 6)
    {
      u32 j = i + 2, left = ce_recs[c->first + i + 1].value;
      while(left && (j < c->count))
      {
        j += gs_len(ce_recs + c->first, j);
        left--;
      }
      if(left)
        return CE_E_UNSUPPORTED;
    }
  return CE_E_NONE;
}

static ce_error cb_validate(cheat_entry *c)
{
  const cheat_rec *r = ce_recs + c->first;
  u32 i, len;

  for(i = 0; i < c->count; i += len)
  {
    u32 code = r[i].address, address = code & 0xFFFFFFF, op = code >> 28, work = 1;
    len = cb_len(r, c->count, i);
    if(len > c->count - i)
      return CE_E_UNSUPPORTED;
    switch(op)
    {
      case 0x0:
      case 0x1:
      case 0xD:
        break;
      case 0x9:
        return CE_E_ENCRYPTED;
      case 0x3:
      case 0x5:
        if(!ce_in_ram(address))
          return CE_E_REGION;
        break;
      case 0xE:
        if(!ce_in_ram(address))
          return CE_E_REGION;
        if((address & 1) && ((address & ~1U) & 3))
          return CE_E_ALIGN;
        break;
      default:
        if(!ce_in_ram(address))
          return CE_E_REGION;
        if(address & 1)
          return CE_E_ALIGN;
        if((op == 0x4) && (len == 2) && ((r[i + 1].address & 0xFFFF) > CE_SLIDE_MAX))
          return CE_E_UNSUPPORTED;
        break;
    }
    switch(op)
    {
      case 0x0: case 0x1: case 0xD: work = 0; break;
      case 0x2: case 0x6: case 0xE: work = 2; break;
      case 0x4: work = r[i + 1].address & 0xFFFF; break;
      case 0x5: work = (u32)(u16)r[i].value * 2; break;
    }
    if(work > CE_FRAME_MAX - ce_work_used - c->work)
      return CE_E_WORK;
    c->work += work;
  }
  return CE_E_NONE;
}

/* A new-format cheat's code text to records. */
static ce_error compile_new(cheat_entry *c, const char *code)
{
  const char *s = code, *t, *t2;
  u32 n, n2, used, fam;
  int kind = 0;  /* 1 CodeBreaker, 2 raw */

  c->family = FAM_CB;
  while(*s && is_sep(*s))
    s++;
  fam = family_tag(s, strlen(s), &used);
  if(fam)
    return gs_compile(c, s + used, fam);
  while(token(&s, &t, &n))
  {
    const char *colon = memchr(t, ':', n);
    u32 a = 0, v = 0;
    if(colon)
    {
      u32 an = colon - t, vn = n - an - 1;
      if(kind == 1)
        return CE_E_MIXED;
      kind = 2;
      if(!hex_n(t, an, &a) || !hex_n(colon + 1, vn, &v) || ((vn != 2) && (vn != 4) && (vn != 8)))
        return CE_E_SYNTAX;
      if((a > 0x0FFFFFFF) || !ce_in_ram(a))
        return CE_E_REGION;
      if(((vn == 4) && (a & 1)) || ((vn == 8) && (a & 3)))
        return CE_E_ALIGN;
      if(vn == 2)
      {
        if(!push(c, 0x30000000 | a, v))
          return CE_E_TOOBIG;
      }
      else if(vn == 4)
      {
        if(!push(c, 0x80000000 | a, v))
          return CE_E_TOOBIG;
      }
      else if(!push(c, 0x80000000 | a, v & 0xFFFF) || !push(c, 0x80000000 | (a + 2), v >> 16))
        return CE_E_TOOBIG;
      continue;
    }
    if(!all_hex(t, n))
      return CE_E_SYNTAX;
    if(n == 16)
      return CE_E_GAMESHARK;
    if(n == 12)
    {
      hex_n(t, 8, &a);
      hex_n(t + 8, 4, &v);
    }
    else if(n == 8)
    {
      if(!token(&s, &t2, &n2) || !all_hex(t2, n2))
        return CE_E_SYNTAX;
      if(n2 == 8)
        return CE_E_GAMESHARK;
      if(n2 != 4)
        return CE_E_SYNTAX;
      hex_n(t, 8, &a);
      hex_n(t2, 4, &v);
    }
    else
      return CE_E_SYNTAX;
    if(kind == 2)
      return CE_E_MIXED;
    kind = 1;
    if(!push(c, a, v))
      return CE_E_TOOBIG;
  }
  if(!c->count)
    return CE_E_SYNTAX;
  return cb_validate(c);
}

/* ---- kai's own format ------------------------------------------------------------
 * As kai has always run it (process_cheat_gs1 / gs3 in cheats.c): writes go
 * through the memory handlers to wherever they point, the v3 fill takes its
 * repeat count from the value's top byte. Only ROM patches change: see the
 * top of this file. */

static void kai_decrypt(u32 *a, u32 *v, int v3)
{
  static const u32 seeds_v1[4] = { 0x09f4fbbd, 0x9681884a, 0x352027e9, 0xf3dee5a7 };
  static const u32 seeds_v3[4] = { 0x7aa9648f, 0x7fae6994, 0xc0efaad5, 0x42712c57 };
  cheat_gs_decrypt(a, v, v3 ? seeds_v3 : seeds_v1);
}

static int add_patch(cheat_entry *c, u32 offset, u16 value)
{
  rom_patch *p;
  if(ce_patch_used >= CE_PATCHES)
    return 0;
  if(c->patches == 0)
    c->first_patch = ce_patch_used;
  p = &ce_patch[ce_patch_used++];
  p->offset = offset;
  p->value = value;
  p->has_old = 0;
  c->patches++;
  return 1;
}

/* lines: "XXXXXXXX YYYYYYYY" pairs; encrypted: the gameshark / PAR kinds */
static ce_error compile_kai(cheat_entry *c, const char *code, int v3, int encrypted)
{
  const char *s = code, *t, *t2;
  u32 n, n2, payload = 0;

  c->family = v3 ? FAM_KAI3 : FAM_KAI1;
  while(token(&s, &t, &n))
  {
    u32 a, v;
    if((n != 8) || !hex_n(t, 8, &a) || !token(&s, &t2, &n2) || (n2 != 8) || !hex_n(t2, 8, &v))
      return CE_E_SYNTAX;
    if(encrypted)
      kai_decrypt(&a, &v, v3);
    if(!v3 && !payload)
    {
      u32 op = a >> 28;
      if(op == 0x6)
      {
        /* a ROM patch: (address * 2) in the ROM, one halfword */
        u32 offset = ((a & 0x0FFFFFFF) * 2) - 0x08000000;
        if(!add_patch(c, offset & 0x01FFFFFE, v & 0xFFFF))
          return CE_E_TOOBIG;
        continue;
      }
      if(op == 0x3)
        payload = a & 0xFFFF;  /* address pairs follow */
    }
    else if(payload)
      payload--;
    if(!push(c, a, v))
      return CE_E_TOOBIG;
  }
  if(!c->count && !c->patches)
    return CE_E_SYNTAX;
  return CE_E_NONE;
}

static void run_kai1(const cheat_entry *c)
{
  const cheat_rec *r = ce_recs + c->first;
  u32 i, n = c->count;

  for(i = 0; i < n; i++)
  {
    u32 address = r[i].address, value = r[i].value, op = address >> 28;
    address &= 0xFFFFFFF;
    switch(op)
    {
      case 0x0: write_memory8(address, value); break;
      case 0x1: write_memory16(address, value); break;
      case 0x2: write_memory32(address, value); break;
      case 0x3:
      {
        u32 k, num = address & 0xFFFF;
        for(k = 0; (k < num) && (i + 1 < n); k++)
        {
          i++;
          write_memory32(r[i].address, value);
          if(r[i].value != 0)
            write_memory32(r[i].value, value);
        }
        break;
      }
      case 0xD:
        if(read_memory16(address) != (value & 0xFFFF))
          i++;
        break;
      case 0xE:
        if(read_memory16(value & 0xFFFFFFF) != (address & 0xFFFF))
          i += (address >> 16) & 0x03;
        break;
    }
  }
}

static void run_kai3(const cheat_entry *c)
{
  const cheat_rec *r = ce_recs + c->first;
  u32 i, k, n = c->count;

  for(i = 0; i < n; i++)
  {
    u32 address = r[i].address, value = r[i].value, op = address >> 28, sub;
    address &= 0xFFFFFFF;
    sub = address >> 24;
    switch(op)
    {
      case 0x0:
        address = (address & 0xFFFFF) + ((address << 4) & 0xF000000);
        switch(sub)
        {
          case 0x0:
            for(k = 0; k <= (value >> 24); k++, address++)
              write_memory8(address, value & 0xFF);
            break;
          case 0x2:
            for(k = 0; k <= (value >> 16); k++, address += 2)
              write_memory16(address, value & 0xFFFF);
            break;
          case 0x4:
            write_memory32(address, value);
            break;
        }
        break;
      case 0x4:
        address = (address & 0xFFFFF) + ((address << 4) & 0xF000000);
        switch(sub)
        {
          case 0x0:
            write_memory8(read_memory32(address) + (value >> 24), value & 0xFF);
            break;
          case 0x2:
            write_memory16(read_memory32(address) + ((value >> 16) * 2), value & 0xFFFF);
            break;
          case 0x4:
            write_memory32(read_memory32(address), value);
            break;
        }
        break;
      case 0x8:
        address = (address & 0xFFFFF) + ((address << 4) & 0xF000000);
        switch(sub)
        {
          case 0x0: write_memory8(address, (value & 0xFF) + read_memory8(address)); break;
          case 0x2: write_memory16(address, (value & 0xFFFF) + read_memory16(address)); break;
          case 0x4: write_memory32(address, value + read_memory32(address)); break;
        }
        break;
      case 0xC:
        address = (address & 0xFFFFFF) + 0x4000000;
        if(sub == 0x6)
          write_memory16(address, value);
        else if(sub == 0x7)
          write_memory32(address, value);
        break;
    }
  }
}

/* ---- ROM patches --------------------------------------------------------------
 * A whole ROM in memory is patched in place. A ROM too big for memory is read
 * in 32 KB pages: a page in memory is patched now, and every page read later
 * gets its patches as it arrives (cheat_engine_page_loaded). The recompiler's
 * ROM translations are thrown away after a change. */

static u8 *rom_byte_pair(u32 offset)
{
  if(offset + 1 >= gamepak_size)
    return NULL;
  if(gamepak_file_large == -1)
    return gamepak_rom + offset;
  {
    u8 *page = memory_map_read[(0x8000000 + offset) >> 15];
    return page ? page + (offset & 0x7FFF) : NULL;
  }
}

static void apply_patches(cheat_entry *c, int on)
{
  u32 k;
  int changed = 0;

  for(k = 0; k < c->patches; k++)
  {
    rom_patch *p = &ce_patch[c->first_patch + k];
    u8 *b = rom_byte_pair(p->offset);
    if(!b)
    {
      if(!on)
        p->has_old = 0;  /* a page not in memory is read back unpatched */
      continue;
    }
    if(on)
    {
      if(!p->has_old)
      {
        p->old = ADDRESS16(b, 0);
        p->has_old = 1;
      }
      ADDRESS16(b, 0) = p->value;
      changed = 1;
    }
    else if(p->has_old)
    {
      ADDRESS16(b, 0) = p->old;
      p->has_old = 0;
      changed = 1;
    }
  }
  if(changed)
  {
    flush_translation_cache_rom();
    reg[CHANGED_PC_STATUS] = 1;
  }
}

void cheat_engine_page_loaded(u32 physical_index, u8 *page)
{
  u32 i, k, base = physical_index << 15;

  for(i = 0; i < ce_count; i++)
  {
    if(!ce[i].on || (ce[i].status != CE_OK))
      continue;
    for(k = 0; k < ce[i].patches; k++)
    {
      rom_patch *p = &ce_patch[ce[i].first_patch + k];
      if((p->offset >= base) && (p->offset + 1 < base + 0x8000))
      {
        p->old = ADDRESS16(page, p->offset - base);
        p->has_old = 1;
        ADDRESS16(page, p->offset - base) = p->value;
      }
    }
  }
}

/* ---- loading a file -------------------------------------------------------------- */

static u32 hash_text(u32 h, const char *s, int code)
{
  for(; *s; s++)
  {
    char ch = *s;
    if(code && (is_sep(ch) || (ch == ':')))
      continue;  /* a code's digits only: rewrapping keeps the hash */
    if((ch >= 'a') && (ch <= 'z'))
      ch -= 'a' - 'A';
    h = (h ^ (u8)ch) * 16777619u;
  }
  return h;
}

static char code_text[CE_CODE_TEXT];
static u32 code_len;

static void code_add(const char *s)
{
  u32 n = strlen(s);
  if(code_len + n + 2 >= CE_CODE_TEXT)
    return;
  if(code_len)
    code_text[code_len++] = ' ';
  memcpy(code_text + code_len, s, n);
  code_len += n;
  code_text[code_len] = 0;
}

/* kinds of a cheat being read: KAI_* from its header, or NEW */
enum { K_NONE, K_NEW, K_KAI1, K_KAI1_ENC, K_KAI3, K_KAI3_ENC };

static void finish_cheat(const char *name, int kind)
{
  cheat_entry *c;
  ce_error err;
  u32 save_rec = ce_rec_used, save_patch = ce_patch_used;

  if((kind == K_NONE) || (code_len == 0) || (ce_count >= CE_MAX_CHEATS))
  {
    code_len = 0;
    return;
  }
  c = &ce[ce_count];
  memset(c, 0, sizeof(*c));
  snprintf(c->name, sizeof(c->name), "%s", name[0] ? name : "Unnamed cheat");
  c->first = ce_rec_used;
  ce_bad_region = 0;
  if(kind == K_NEW)
    err = compile_new(c, code_text);
  else
    err = compile_kai(c, code_text, (kind == K_KAI3) || (kind == K_KAI3_ENC),
     (kind == K_KAI1_ENC) || (kind == K_KAI3_ENC));
  if(err == CE_E_NONE)
  {
    c->status = CE_OK;
    ce_rec_used += c->count;
    ce_work_used += c->work;
  }
  else
  {
    /* kept, greyed, with its reason; its records are given back */
    c->status = (err == CE_E_GAMESHARK) ? CE_ASK :
     ((err == CE_E_REGION) && ((ce_bad_region == 0xD) || (ce_bad_region == 0xE))) ?
     CE_CANT_SAVE : CE_CANT_KIND;
    c->count = c->patches = c->work = 0;
    ce_rec_used = save_rec;
    ce_patch_used = save_patch;
  }
  c->hash = hash_text(hash_text(2166136261u, c->name, 0), code_text, 1);
  ce_count++;
  code_len = 0;
}

/* kai's header word, as the old parser knew them */
static int kai_header(const char *line, const char **name)
{
  static const struct { const char *word; int kind; } words[] =
  {
    { "gameshark_v1", K_KAI1_ENC }, { "gameshark_v2", K_KAI1_ENC }, { "PAR_v1", K_KAI1_ENC },
    { "PAR_v2", K_KAI1_ENC }, { "gameshark_v3", K_KAI3_ENC }, { "PAR_v3", K_KAI3_ENC },
    { "direct_v1", K_KAI1 }, { "direct_v2", K_KAI1 }, { "direct_v3", K_KAI3 }, { NULL, 0 }
  };
  int i;

  for(i = 0; words[i].word; i++)
  {
    u32 n = strlen(words[i].word);
    if(!strncasecmp(line, words[i].word, n) && ((line[n] == ' ') || (line[n] == '\t') ||
     (line[n] == 0)))
    {
      *name = line + n;
      while((**name == ' ') || (**name == '\t'))
        (*name)++;
      return words[i].kind;
    }
  }
  return K_NONE;
}

/* does the whole line read as codes? */
static int code_line(const char *s)
{
  const char *t;
  u32 n, used, any = 0;

  while(*s && is_sep(*s))
    s++;
  if(family_tag(s, strlen(s), &used))
    return 1;
  while(token(&s, &t, &n))
  {
    const char *colon = memchr(t, ':', n);
    if(colon)
    {
      if(!all_hex(t, colon - t) || !all_hex(colon + 1, n - (colon - t) - 1))
        return 0;
    }
    else if(!all_hex(t, n) || ((n != 4) && (n != 8) && (n != 12) && (n != 16)))
      return 0;
    any = 1;
  }
  return any;
}

static void trim(char *s)
{
  u32 n = strlen(s);
  while(n && ((s[n - 1] == '\n') || (s[n - 1] == '\r') || (s[n - 1] == ' ') ||
   (s[n - 1] == '\t')))
    s[--n] = 0;
}

/* the database format: cheatN_desc / cheatN_code, collected, then compiled
   in N order */
static char db_text[CE_DB_TEXT];
static u32 db_used;
static struct { u32 desc, code; } db[CE_MAX_CHEATS];
static u32 db_top;

static int db_line(char *line)
{
  unsigned int n;
  char key[8], *eq, *v;

  if(sscanf(line, " cheat%u_%7[a-z]", &n, key) != 2)
    return 0;
  eq = strchr(line, '=');
  if(!eq || (n >= CE_MAX_CHEATS))
    return 1;
  v = eq + 1;
  while((*v == ' ') || (*v == '\t'))
    v++;
  if(*v == '"')
  {
    char *q = strrchr(v + 1, '"');
    v++;
    if(q)
      *q = 0;
  }
  if(strcmp(key, "desc") && strcmp(key, "code"))
    return 1;  /* cheatN_enable and the rest: not read */
  if(db_used + strlen(v) + 1 > CE_DB_TEXT)
    return 1;
  if(n + 1 > db_top)
    db_top = n + 1;
  if(!strcmp(key, "desc"))
    db[n].desc = db_used + 1;
  else
    db[n].code = db_used + 1;
  strcpy(db_text + db_used, v);
  db_used += strlen(v) + 1;
  return 1;
}

static void read_file(FILE *f)
{
  static char line[512], name[CE_NAME_LENGTH];
  int kind = K_NONE, closed = 0, is_db = 0;
  u32 i;

  name[0] = 0;
  code_len = 0;
  db_used = db_top = 0;
  memset(db, 0, sizeof(db));
  while(fgets(line, sizeof(line), f))
  {
    const char *after;
    int k;
    char *s = line;
    trim(line);
    if(((u8)s[0] == 0xEF) && ((u8)s[1] == 0xBB) && ((u8)s[2] == 0xBF))
      s += 3;  /* a UTF-8 byte order mark */
    if(db_line(s))
    {
      is_db = 1;
      continue;
    }
    if(is_db)
      continue;
    k = kai_header(s, &after);
    if(k != K_NONE)
    {
      finish_cheat(name, kind);
      snprintf(name, sizeof(name), "%s", after);
      kind = k;
      closed = 0;
      continue;
    }
    while((*s == ' ') || (*s == '\t'))
      s++;
    if(!*s)
    {
      if(kind >= K_KAI1)
        closed = 1;  /* kai's format: a cheat's codes end at a blank line */
      continue;
    }
    if((s[0] == '#') || ((s[0] == '/') && (s[1] == '/')))
      continue;
    if(code_line(s))
    {
      if(kind >= K_KAI1)
      {
        if(!closed)
          code_add(s);
        continue;
      }
      if(kind == K_NONE)
      {
        kind = K_NEW;  /* codes before any name */
        name[0] = 0;
      }
      code_add(s);
      continue;
    }
    /* a name line: the next plain-list cheat */
    finish_cheat(name, kind);
    if(((s[0] == '[') || (s[0] == '"')) && (strlen(s) > 1))
    {
      char close = (s[0] == '[') ? ']' : '"';
      char *e = strrchr(s + 1, close);
      if(e)
        *e = 0;
      s++;
    }
    snprintf(name, sizeof(name), "%.*s", (int)sizeof(name) - 1, s);
    kind = K_NEW;
    closed = 0;
  }
  finish_cheat(name, kind);
  if(is_db)
    for(i = 0; i < db_top; i++)
      if(db[i].code)
      {
        code_len = 0;
        code_add(db_text + db[i].code - 1);
        finish_cheat(db[i].desc ? db_text + db[i].desc - 1 : "", K_NEW);
      }
}

/* ---- what is on ---------------------------------------------------------------- */

static void on_path(char *path)
{
  char base[MAX_FILE], *dot;

  snprintf(base, sizeof(base), "%s", ce_rom);
  dot = strrchr(base, '.');
  if(dot)
    *dot = 0;
  path_printf(path, MAX_PATH, "%s/%s.on", g_default_cfg_dir, base);
}

static void read_on(void)
{
  char path[MAX_PATH], line[128];
  FILE *f;

  on_path(path);
  f = fopen(path, "r");
  if(!f)
    return;
  while(fgets(line, sizeof(line), f))
  {
    unsigned int h;
    u32 i;
    char mode;
    if(sscanf(line, "%8x %c", &h, &mode) != 2)
      continue;
    for(i = 0; i < ce_count; i++)
      if((ce[i].hash == h) && (ce[i].status == CE_OK))
      {
        ce[i].on = 1;
        ce[i].locked = (mode == 'L');
      }
  }
  fclose(f);
}

void cheat_engine_save(void)
{
  char path[MAX_PATH];
  FILE *f;
  u32 i, any = 0;

  if(!ce_rom[0])
    return;
  on_path(path);
  for(i = 0; i < ce_count; i++)
    any |= ce[i].on;
  if(!any)
  {
    sceIoRemove(path);  /* nothing on: no file */
    return;
  }
  f = fopen(path, "w");
  if(!f)
    return;
  for(i = 0; i < ce_count; i++)
    if(ce[i].on)
      fprintf(f, "%08X %c %s\n", (unsigned)ce[i].hash, ce[i].locked ? 'L' : 'O', ce[i].name);
  fclose(f);
}

/* ---- the engine ------------------------------------------------------------------ */

void cheat_engine_clear(void)
{
  ce_count = ce_rec_used = ce_patch_used = ce_work_used = 0;
  ce_rom[0] = 0;
}

void cheat_engine_load(const char *rom_name)
{
  char path[MAX_PATH], base[MAX_FILE], *dot;
  FILE *f;
  u32 i;

  cheat_engine_clear();
  if(!rom_name || !rom_name[0])
    return;
  snprintf(ce_rom, sizeof(ce_rom), "%s", rom_name);
  snprintf(base, sizeof(base), "%s", rom_name);
  dot = strrchr(base, '.');
  if(dot)
    *dot = 0;
  path_printf(path, sizeof(path), "%s/%s.cht", g_default_cheat_dir, base);
  f = fopen(path, "rb");
  if(!f)
    return;
  read_file(f);
  fclose(f);
  read_on();
  /* the ROM patches of what is on, before the game runs */
  for(i = 0; i < ce_count; i++)
    if(ce[i].on && ce[i].patches)
      apply_patches(&ce[i], 1);
}

void cheat_engine_frame(void)
{
  u32 i;
  u16 pad = 0x3FF ^ io_registers[REG_P1];

  ce_smc_pending = 0;
  for(i = 0; i < ce_count; i++)
  {
    if(!ce[i].on || (ce[i].status != CE_OK) || !ce[i].count)
      continue;
    switch(ce[i].family)
    {
      case FAM_CB:   run_codebreaker(&ce[i], pad); break;
      case FAM_GS:   run_gs(&ce[i]); break;
      case FAM_KAI1: run_kai1(&ce[i]); break;
      case FAM_KAI3: run_kai3(&ce[i]); break;
    }
  }
  if(ce_smc_pending)
  {
    /* a code rewrote RAM the recompiler had translated */
    flush_translation_cache_ram();
    reg[CHANGED_PC_STATUS] = 1;
    ce_smc_pending = 0;
  }
}

u32 cheat_count(void)
{
  return ce_count;
}

const char *cheat_name(u32 i)
{
  return (i < ce_count) ? ce[i].name : "";
}

u32 cheat_status(u32 i)
{
  return (i < ce_count) ? ce[i].status : CE_CANT_KIND;
}

const char *cheat_why(u32 i)
{
  switch(cheat_status(i))
  {
    case CE_CANT_KIND: return "This kind of code isn't supported.";
    case CE_CANT_SAVE: return "This code would write to the game's save.";
    case CE_ASK:       return "Answer which kind of codes these are first.";
  }
  return NULL;
}

u32 cheat_is_on(u32 i)
{
  return (i < ce_count) && ce[i].on;
}

u32 cheat_is_locked(u32 i)
{
  return (i < ce_count) && ce[i].locked;
}

void cheat_set_on(u32 i, u32 on)
{
  if((i >= ce_count) || (ce[i].status != CE_OK) || ((u32)ce[i].on == (on ? 1u : 0u)))
    return;
  if(!on && ce[i].locked)
    return;  /* a locked cheat turns off only after unlocking */
  ce[i].on = on ? 1 : 0;
  if(ce[i].patches)
    apply_patches(&ce[i], ce[i].on);
}

void cheat_set_locked(u32 i, u32 locked)
{
  if((i >= ce_count) || (ce[i].status != CE_OK))
    return;
  ce[i].locked = locked ? 1 : 0;
  if(locked && !ce[i].on)
    cheat_set_on(i, 1);  /* SELECT locks a cheat on */
}
