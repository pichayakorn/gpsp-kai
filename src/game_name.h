/* game_name.h - a game's name from its file name (game_name.c) */

#ifndef GAME_NAME_H
#define GAME_NAME_H

/* The first n bytes of a file name, cleaned up: bracketed tags, a leading
   release number and underscores go. out: 256 bytes. Returns its length. */
int game_name_clean(const char *name, int n, char *out);
/* The same for a whole file name, its extension left out */
void game_name_of_file(const char *file, char *out);

#endif
