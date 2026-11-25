#ifndef CHEAT_ENGINE_H
#define CHEAT_ENGINE_H

#define CE_MAX_CHEATS   1024
#define CE_MAX_GROUPS   64
#define CE_NAME_LENGTH  48

/* Why a cheat can't run (0: it runs) */
enum
{
  CE_OK = 0,
  CE_CANT_KIND,     /* a kind of code kai doesn't run */
  CE_CANT_SAVE,     /* it would write to the game's save or outside the RAM */
  CE_ASK            /* only while a file is read: bare codes, kind not known yet */
};

/* Why a cheat can't run, finer than CE_* (cheat_detail) */
typedef enum
{
  CE_E_NONE = 0,
  CE_E_SYNTAX,       /* not a code kai reads */
  CE_E_GAMESHARK,    /* bare 16-digit codes that make sense no way */
  CE_E_REGION,       /* writes outside the work RAM (save, I/O, ROM...) */
  CE_E_ALIGN,        /* a 16- or 32-bit write to an odd address */
  CE_E_MIXED,        /* kinds of code mixed in one cheat */
  CE_E_UNSUPPORTED,  /* a code type kai doesn't run */
  CE_E_ENCRYPTED,    /* an encrypted CodeBreaker code */
  CE_E_TOOBIG,       /* more code than kai keeps for one cheat, or a line cut */
  CE_E_WORK          /* more reads and writes a frame than kai allows */
} ce_error;

/* A group's rule (a "#kai: ..." line in its file) */
enum
{
  CE_RULE_NONE = 0,
  CE_RULE_ONE,      /* one at a time: switching one on switches the others off */
  CE_RULE_ONCE,     /* runs once: "on" means ready; it runs, then switches off */
  CE_RULE_RESTART   /* works from the game's start */
};


/* Load the cheats for the game just loaded (its file name, as in
 * gamepak_filename) and apply the ROM patches of those remembered on. */
void cheat_engine_load(const char *rom_name);
/* Forget every cheat (no game, or before another loads). */
void cheat_engine_clear(void);
/* Once a frame, from update_gba: run the cheats that are on. */
void cheat_engine_frame(void);
/* A ROM page was just read from the Memory Stick (a game too big for
 * memory): put the active ROM patches inside it back. */
void cheat_engine_page_loaded(u32 physical_index, u8 *page);

u32 cheat_count(void);
/* Every cheat the files hold, past CE_MAX_CHEATS too (master codes aside) */
u32 cheat_total(void);
/* A master code by its name ("(M)", "must be on"): never loaded */
u32 cheat_name_is_master(const char *name);
const char *cheat_name(u32 i);
u32 cheat_status(u32 i);           /* CE_* */
const char *cheat_why(u32 i);      /* the reason in plain words, or NULL */
u32 cheat_is_on(u32 i);
u32 cheat_is_locked(u32 i);
void cheat_set_on(u32 i, u32 on);  /* ROM patches go in or come out now */
void cheat_set_locked(u32 i, u32 locked);
/* Write which cheats are on and locked (GBA/CFG/<game>.on). */
void cheat_engine_save(void);

u32 cheat_group_count(void);
const char *cheat_group_name(u32 g);
u32 cheat_group_rule(u32 g);       /* CE_RULE_* */
u32 cheat_group_first(u32 g);      /* its cheats: first .. first + size - 1 */
u32 cheat_group_size(u32 g);
u32 cheat_group_of(u32 i);
/* On since the game started, in a group that needs a restart */
u32 cheat_waits_restart(u32 i);
/* The game was just restarted */
void cheat_engine_restarted(void);
/* Runs-once cheats that ran since the last call, and the first one's name */
u32 cheat_engine_applied(char *name, u32 size);
/* Any game's cheats without loading them: how many, and how many it
   remembers on */
u32 cheat_engine_peek(const char *rom_name, u32 *on);
u32 cheat_detail(u32 i);           /* ce_error */
/* Where the game's cheat files go: GBA/CHT/<game>/ */
void cheat_engine_folder(char *out, u32 size);

#endif
