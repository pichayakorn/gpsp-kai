/* cheat_engine.h - kai 4.0's cheat engine (#166, part 1)
 *
 * A game's cheats from the .cht files people already have, every cheat
 * checked when it loads, the ones that are on remembered by name. See
 * cheat_engine.c for the formats and the rules.
 */

#ifndef CHEAT_ENGINE_H
#define CHEAT_ENGINE_H

#define CE_MAX_CHEATS   512
#define CE_NAME_LENGTH  48

/* Why a cheat can't run (0: it runs) */
enum
{
  CE_OK = 0,
  CE_CANT_KIND,     /* a kind of code kai doesn't run */
  CE_CANT_SAVE,     /* it would write to the game's save or outside the RAM */
  CE_ASK            /* bare 16-digit codes: GameShark or Action Replay? */
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
const char *cheat_name(u32 i);
u32 cheat_status(u32 i);           /* CE_* */
const char *cheat_why(u32 i);      /* the reason in plain words, or NULL */
u32 cheat_is_on(u32 i);
u32 cheat_is_locked(u32 i);
void cheat_set_on(u32 i, u32 on);  /* ROM patches go in or come out now */
void cheat_set_locked(u32 i, u32 locked);
/* Write which cheats are on and locked (GBA/CFG/<game>.on). */
void cheat_engine_save(void);

#endif
