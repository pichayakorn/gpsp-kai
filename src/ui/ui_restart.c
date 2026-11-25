/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Hachi
 */

#include "../common.h"
#include <psploadexec_kernel.h>
#include "ui.h"

/* From systemctrl.h, which clashes with kai's own model names */
int sctrlKernelLoadExecVSHWithApitype(int apitype, const char *file,
 struct SceKernelLoadExecVSHParam *param);

#define NOTE_NAME "restart.note"

ui_restart_note_t ui_restart_note;

static void note_path(char *path, int size)
{
  path_printf(path, size, "%s/%s", main_path, NOTE_NAME);
}

void ui_restart_note_read(void)
{
  char path[MAX_PATH], buf[MAX_PATH + 32], *nl;
  SceUID fd;
  int n;

  memset(&ui_restart_note, 0, sizeof(ui_restart_note));
  note_path(path, sizeof(path));
  fd = sceIoOpen(path, PSP_O_RDONLY, 0);
  if(fd < 0)
    return;
  n = sceIoRead(fd, buf, sizeof(buf) - 1);
  sceIoClose(fd);
  sceIoRemove(path);
  if(n <= 0)
    return;
  buf[n] = 0;

  if(sscanf(buf, "%d %d", &ui_restart_note.tab, &ui_restart_note.row) != 2)
    return;
  nl = strchr(buf, '\n');
  if(nl)
  {
    char *end;
    nl++;
    end = strchr(nl, '\n');
    if(end)
      *end = 0;
    snprintf(ui_restart_note.game, sizeof(ui_restart_note.game), "%s", nl);
  }
  ui_restart_note.pending = 1;
}

static void note_write(int tab, int row, const char *game)
{
  char path[MAX_PATH], buf[MAX_PATH + 32];
  SceUID fd;
  int n;

  note_path(path, sizeof(path));
  n = snprintf(buf, sizeof(buf), "%d %d\n%s\n", tab, row, game ? game : "");
  fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  if(fd < 0)
    return;
  sceIoWrite(fd, buf, n);
  sceIoClose(fd);
}

void ui_restart_exec(void)
{
  char path[256];
  struct SceKernelLoadExecVSHParam param;
  int apitype;

  path[0] = 0;
  if((kuKernelInitFileName(path) < 0) || (path[0] == 0))
    path_printf(path, sizeof(path), "%s/EBOOT.PBP", main_path);
  apitype = kuKernelInitApitype();

  memset(&param, 0, sizeof(param));
  param.size = sizeof(param);
  param.args = strlen(path) + 1;
  param.argp = path;
  param.key = "game";
  sctrlKernelLoadExecVSHWithApitype(apitype, path, &param);
}

void ui_restart_now(int tab, int row, const char *game)
{
  note_write(tab, row, game);
  quit(3);
}
