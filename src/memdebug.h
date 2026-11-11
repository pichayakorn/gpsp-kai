/* Debug build for issue #36 (ROM buffer is only 16 MB on a PSP-3000).
 * Built only with MEMDEBUG = 1 in Makefile.set; drop this file and its
 * hooks once the cause is known.
 */

#ifndef MEMDEBUG_H
#define MEMDEBUG_H

#ifdef MEMDEBUG
void memdebug_main_start(void);
void memdebug_before_gamepak(void);
void memdebug_after_gamepak(void);
/* Show the numbers on screen, write them to memdebug.txt, wait for O/X. */
void memdebug_show(void);
#endif

#endif
