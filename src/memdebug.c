/* Debug build for issue #36 (ROM buffer is only 16 MB on a PSP-3000).
 *
 * Records how much user memory (partition 2) the EBOOT really has and where
 * it ends, so we can tell whether the CFW gave us the extra memory that
 * MEMSIZE=1 asks for. Built only with MEMDEBUG = 1 in Makefile.set.
 */

#include "common.h"
#include <stdarg.h>
#include "memdebug.h"

/* From <systemctrl.h>, which clashes with PSP_1000/PSP_2000 in main.h */
int sctrlHENGetVersion(void);
int sctrlHENGetMinorVersion(void);
int sctrlGetInitPARAM(const char *paramName, u16 *paramType, u32 *paramLength,
 void *paramBuffer);

#ifdef MEMDEBUG

#define KB 1024
#define MB (1024 * 1024)
#define PROBE_STEP (64 * KB)
#define PROBE_MAX  (64 * MB)

/* Defined by libcglue: the partition 2 block that holds the newlib heap.
 * libcglue allocates it on the first malloc, sized to
 * sceKernelMaxFreeMemSize() minus a 512 KB threshold. That first malloc
 * happens before main(): crt0 -> __libcglue_init -> __timezone_update ->
 * setenv("TZ"). */
extern SceUID __psp_heap_blockid;
/* Linker symbols: start of the EBOOT and end of its .bss */
extern char __executable_start[];
extern char _end[];

typedef struct
{
  u32 total_free;   /* sceKernelTotalFreeMemSize() */
  u32 max_free;     /* sceKernelMaxFreeMemSize() */
  u32 brk;          /* sbrk(0): current end of the used heap */
  u32 largest;      /* largest malloc() that succeeds, 64 KB steps */
  u32 largest_addr; /* where that block starts */
} mem_snapshot;

static mem_snapshot snap_start;
static mem_snapshot snap_before;
static mem_snapshot snap_after;
static u32 gamepak_addr;
static u32 gamepak_size;

static u32 largest_malloc(u32 *addr)
{
  u32 lo = 0;
  u32 hi = PROBE_MAX / PROBE_STEP;
  void *p;

  while(lo < hi)
  {
    u32 mid = (lo + hi + 1) / 2;
    p = malloc(mid * PROBE_STEP);
    if(p != NULL)
    {
      free(p);
      lo = mid;
    }
    else
      hi = mid - 1;
  }

  *addr = 0;
  if(lo > 0)
  {
    p = malloc(lo * PROBE_STEP);
    if(p != NULL)
    {
      *addr = (u32)p;
      free(p);
    }
  }
  return lo * PROBE_STEP;
}

static void take_snapshot(mem_snapshot *s)
{
  /* The heap already exists at main start, so the kernel numbers are the
   * memory outside the heap; "largest malloc" is about the heap size. */
  s->total_free = sceKernelTotalFreeMemSize();
  s->max_free = sceKernelMaxFreeMemSize();
  s->largest = largest_malloc(&s->largest_addr);
  s->brk = (u32)sbrk(0);
}

void memdebug_main_start(void)
{
  take_snapshot(&snap_start);
}

void memdebug_before_gamepak(void)
{
  take_snapshot(&snap_before);
}

void memdebug_after_gamepak(void)
{
  gamepak_addr = (u32)gamepak_rom;
  gamepak_size = gamepak_ram_buffer_size;
  take_snapshot(&snap_after);
}

#define LINES 24
#define LINE_LEN 100

static char lines[LINES][LINE_LEN];
static int line_count;

static void add_line(const char *fmt, ...)
{
  va_list ap;

  if(line_count >= LINES)
    return;
  va_start(ap, fmt);
  vsnprintf(lines[line_count], LINE_LEN, fmt, ap);
  va_end(ap);
  line_count++;
}

static void add_snapshot(const char *name, mem_snapshot *s)
{
  add_line("%s free %u KB, max %u KB, brk 0x%08X", name,
   s->total_free / KB, s->max_free / KB, s->brk);
  add_line("   largest malloc %u KB at 0x%08X, ends 0x%08X", s->largest / KB,
   s->largest_addr, s->largest_addr ? s->largest_addr + s->largest : 0);
}

static void collect(void)
{
  int model = kuKernelGetModel();
  int memsize = -1;
  u32 memsize_len = sizeof(memsize);
  int memsize_ret;
  SceUID uid;
  u32 heap_base = 0;
  u32 p2_top = 0;
  int p2_32mb;

  memsize_ret = sctrlGetInitPARAM("MEMSIZE", NULL, &memsize_len, &memsize);

  if(__psp_heap_blockid > 0)
    heap_base = (u32)sceKernelGetBlockHeadAddr(__psp_heap_blockid);

  /* A small block from the high end shows where partition 2 ends:
   * about 0x0A000000 for the normal 24 MB; with the extra memory
   * unlocked about 0x0BB00000 (ARK-4 with partition 11) to 0x0BF00000. */
  uid = sceKernelAllocPartitionMemory(2, "memdbg_hi", PSP_SMEM_High, 16 * KB, NULL);
  if(uid > 0)
  {
    p2_top = (u32)sceKernelGetBlockHeadAddr(uid) + 16 * KB;
    sceKernelFreePartitionMemory(uid);
  }

  /* The heap normally holds nearly all of partition 2, so this is expected
   * to fail; a success would mean memory outside the heap. */
  p2_32mb = sceKernelAllocPartitionMemory(2, "memdbg_32", PSP_SMEM_Low, 32 * MB, NULL);
  if(p2_32mb > 0)
    sceKernelFreePartitionMemory(p2_32mb);

  line_count = 0;
  add_line("gpSP kai MEMDEBUG #36  %s", VERSION_STRING);
  add_line("model %d (%02dg)  devkit 0x%08X  apitype 0x%X", model, model + 1,
   sceKernelDevkitVersion(), kuKernelInitApitype());
  add_line("CFW ver 0x%X.%X  MEMSIZE %d (ret 0x%08X)", sctrlHENGetVersion(),
   sctrlHENGetMinorVersion(), memsize, memsize_ret);
  add_line("EBOOT 0x%08X-0x%08X (%u KB)", (u32)__executable_start,
   (u32)_end, ((u32)_end - (u32)__executable_start) / KB);
  add_line("heap block id 0x%X base 0x%08X", __psp_heap_blockid, heap_base);
  add_snapshot("A main start: ", &snap_start);
  /* The heap is nearly empty at main start, so the end of the largest
   * malloc is about the end of the heap block. Heap size plus A max is
   * about the largest free block partition 2 had before the heap. */
  if(heap_base != 0 && snap_start.largest_addr != 0)
  {
    u32 heap_end = snap_start.largest_addr + snap_start.largest;
    add_line("   heap ~%u KB (base to A largest end), pre-heap max ~%u KB",
     (heap_end - heap_base) / KB, (heap_end - heap_base + snap_start.max_free) / KB);
  }
  add_snapshot("B pre gamepak:", &snap_before);
  add_line("C ROM buffer %u KB (%u MB) at 0x%08X", gamepak_size / KB,
   gamepak_size / MB, gamepak_addr);
  add_snapshot("D after:      ", &snap_after);
  add_line("p2 top 0x%08X  p2 32MB alloc 0x%08X", p2_top, p2_32mb);
  add_line("Saved to memdebug.txt. Press O or X to continue.");
}

static void write_file(void)
{
  FILE *f = fopen("memdebug.txt", "w");
  int i;

  if(f == NULL)
    return;
  for(i = 0; i < line_count; i++)
    fprintf(f, "%s\n", lines[i]);
  fclose(f);
}

static void wait_button(void)
{
  SceCtrlData pad;

  do
  {
    sceKernelDelayThread(20000);
    sceCtrlPeekBufferPositive(&pad, 1);
  } while(pad.Buttons & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS));

  do
  {
    sceKernelDelayThread(20000);
    sceCtrlPeekBufferPositive(&pad, 1);
  } while(!(pad.Buttons & (PSP_CTRL_CIRCLE | PSP_CTRL_CROSS)));
}

void memdebug_show(void)
{
  int i;

  collect();
  write_file();

  clear_screen(0);
  for(i = 0; i < line_count; i++)
    PRINT_STRING_BG(lines[i], 0xFFFF, 0x0000, 4, 4 + i * (FONT_HEIGHT + 2));
  flip_screen();

  wait_button();

  clear_screen(COLOR_BG);
  flip_screen();
  clear_screen(COLOR_BG);
}

#endif
