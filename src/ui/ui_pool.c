/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Hachi
 */

#include "../common.h"
#include <malloc.h>
#include "ui.h"

#define UI_POOL_SIZE (1024 * 1024)
#define UI_POOL_ALIGN 16

static u8 *pool_base = NULL;
static u32 pool_used = 0;

void ui_pool_init(void)
{
  if(pool_base == NULL)
    pool_base = memalign(UI_POOL_ALIGN, UI_POOL_SIZE);
  pool_used = 0;
}

void *ui_pool_alloc(u32 size)
{
  void *p;

  size = (size + UI_POOL_ALIGN - 1) & ~(UI_POOL_ALIGN - 1);
  if((pool_base == NULL) || (size > UI_POOL_SIZE - pool_used))
    return NULL;
  p = pool_base + pool_used;
  pool_used += size;
  return p;
}

u32 ui_pool_mark(void)
{
  return pool_used;
}

void ui_pool_release(u32 mark)
{
  if(mark <= pool_used)
    pool_used = mark;
}

u32 ui_pool_free(void)
{
  return pool_base ? UI_POOL_SIZE - pool_used : 0;
}

u32 ui_pool_size(void)
{
  return pool_base ? UI_POOL_SIZE : 0;
}
