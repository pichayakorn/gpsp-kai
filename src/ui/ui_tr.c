/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Hachi
 */

#include "../common.h"
#include "ui.h"

/* GNU gettext .po files (lang/<code>.po), read once at start. Only the
 * translations are kept, each found by the FNV-1a hash of its English text;
 * tools/po.py check makes sure no two English texts share a hash. */

#define TR_MAX 2048

static const char *const lang_code[12] =
{
  "ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko", "zh_Hant", "zh_Hans"
};

typedef struct
{
  u32 hash;
  u32 offset;
} tr_entry;

static char *tr_text;
static tr_entry *tr_table;
static u32 tr_count;

static u32 tr_hash(const char *s)
{
  u32 h = 2166136261u;

  while(*s)
  {
    h ^= (u8)*s++;
    h *= 16777619u;
  }
  return h;
}

/* The printf conversions of s, in order, written to out: a % followed by
   flags, width, precision, a length and a conversion letter. A % that
   isn't one ("100 % of") is text. tools/po.py reads them the same way. */
static void tr_formats(const char *s, char *out, u32 size)
{
  u32 n = 0;

  while(*s)
  {
    const char *p;
    if(*s++ != '%')
      continue;
    if(*s == '%')
    {
      s++;
      continue;
    }
    p = s;
    while(*p && strchr("-+ #0", *p))
      p++;
    while((*p >= '0') && (*p <= '9'))
      p++;
    if(*p == '.')
    {
      p++;
      while((*p >= '0') && (*p <= '9'))
        p++;
    }
    while(*p && strchr("hlz", *p))
      p++;
    if(!*p || !strchr("diouxXcsfeEgGp", *p))
      continue;
    p++;
    if(n + (p - s) + 2 >= size)
      break;
    memcpy(out + n, s, p - s);
    n += p - s;
    out[n++] = ';';
    s = p;
  }
  out[n] = 0;
}

/* Decodes the quoted string that starts at *p (spaces before it allowed)
 * to dest, which is never after it. Returns the end of the decoded text, or
 * NULL when the line holds no quoted string. */
static char *po_unquote(char *p, char *dest)
{
  while((*p == ' ') || (*p == '\t'))
    p++;
  if(*p++ != '"')
    return NULL;
  while(*p && (*p != '"'))
  {
    char c = *p++;
    if(c == '\\')
    {
      c = *p++;
      if(c == 'n')
        c = '\n';
      else if(c == 't')
        c = '\t';
      else if(c == 0)
        return NULL;
    }
    *dest++ = c;
  }
  if(*p != '"')
    return NULL;
  *dest = 0;
  return dest;
}

static int tr_order(const void *a, const void *b)
{
  u32 x = ((const tr_entry *)a)->hash, y = ((const tr_entry *)b)->hash;
  return (x > y) - (x < y);
}

static char *tr_read(const char *path, u32 *size)
{
  SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
  char *data = NULL;
  s32 n;

  if(fd < 0)
    return NULL;
  n = sceIoLseek32(fd, 0, PSP_SEEK_END);
  sceIoLseek32(fd, 0, PSP_SEEK_SET);
  if(n > 0)
    data = ui_pool_alloc(n + 1);
  if(data && (sceIoRead(fd, data, n) == n))
  {
    data[n] = 0;
    *size = n;
  }
  else
    data = NULL;
  sceIoClose(fd);
  return data;
}

void ui_tr_init(u32 language)
{
  static char *ids[TR_MAX], *strs[TR_MAX];
  static u32 hashes[TR_MAX], offsets[TR_MAX];
  static u8 skip[TR_MAX];
  char path[MAX_PATH], *data, *p, *end = NULL, *pack;
  u32 mark = ui_pool_mark(), size, n = 0, kept = 0, i, fuzzy = 0;
  int field = 0;  /* 1: in a msgid, 2: in a msgstr */

  tr_text = NULL;
  tr_table = NULL;
  tr_count = 0;
  if(language >= 12)
    return;
  path_printf(path, sizeof(path), "%s/lang/%s.po", main_path, lang_code[language]);
  data = tr_read(path, &size);
  if(data == NULL)
    return;

  /* pass 1: decode each entry's msgid and msgstr in place */
  for(p = data; *p; )
  {
    char *line = p, *next = strchr(p, '\n');
    if(next)
      *next++ = 0;
    else
      next = p + strlen(p);
    if(line[0] == '"')
    {
      if(field && end)
        end = po_unquote(line, end);
      if(!end && (n > 0))
        skip[n - 1] = 1;
    }
    else
    {
      field = 0;
      if(!strncmp(line, "#,", 2))
      {
        if(strstr(line, "fuzzy"))
          fuzzy = 1;
      }
      else if(!strncmp(line, "msgctxt ", 8) && (n < TR_MAX))
        fuzzy = 1;  /* translations with a context aren't used */
      else if(!strncmp(line, "msgid ", 6) && (n < TR_MAX))
      {
        ids[n] = line;
        strs[n] = NULL;
        skip[n] = fuzzy;
        fuzzy = 0;
        end = po_unquote(line + 6, line);
        field = 1;
        n++;
      }
      else if(!strncmp(line, "msgstr ", 7) && n && !strs[n - 1])
      {
        strs[n - 1] = line;
        end = po_unquote(line + 7, line);
        field = 2;
      }
      if((field == 1 || field == 2) && !end)
        skip[n - 1] = 1;
    }
    p = next;
  }

  /* pass 2: keep the translations that keep the English text's printf
     conversions, packed from the start of the buffer */
  pack = data;
  for(i = 0; i < n; i++)
  {
    char fa[64], fb[64];
    u32 len;
    if(skip[i] || !strs[i] || !ids[i][0] || !strs[i][0])
      continue;
    tr_formats(ids[i], fa, sizeof(fa));
    tr_formats(strs[i], fb, sizeof(fb));
    if(strcmp(fa, fb))
      continue;
    hashes[kept] = tr_hash(ids[i]);
    len = strlen(strs[i]) + 1;
    memmove(pack, strs[i], len);
    offsets[kept] = pack - data;
    pack += len;
    kept++;
  }
  if(kept == 0)
  {
    ui_pool_release(mark);
    return;
  }

  ui_pool_release(mark);
  tr_text = ui_pool_alloc(pack - data);
  tr_table = ui_pool_alloc(kept * sizeof(tr_entry));
  if((tr_text != data) || (tr_table == NULL))
  {
    ui_pool_release(mark);
    tr_text = NULL;
    tr_table = NULL;
    return;
  }
  for(i = 0; i < kept; i++)
  {
    tr_table[i].hash = hashes[i];
    tr_table[i].offset = offsets[i];
  }
  qsort(tr_table, kept, sizeof(tr_entry), tr_order);
  tr_count = kept;
}

const char *ui_tr(const char *en)
{
  u32 h, lo = 0, hi = tr_count;

  if(!tr_count || !en)
    return en;
  h = tr_hash(en);
  while(lo < hi)
  {
    u32 mid = (lo + hi) / 2;
    if(tr_table[mid].hash < h)
      lo = mid + 1;
    else
      hi = mid;
  }
  if((lo < tr_count) && (tr_table[lo].hash == h))
    return tr_text + tr_table[lo].offset;
  return en;
}
