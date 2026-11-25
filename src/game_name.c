/* unofficial gameplaySP kai
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

/* The companion tools build this file with the cheat engine, so it
 * depends on nothing but the C library. */

#include <string.h>
#include "game_name.h"

/* The cover is drawn from this name, so these rules must not change once
 * released. */
int game_name_clean(const char *name, int n, char *out)
{
  int i = 0, o = 0, depth = 0;

  if(n > 255)
    n = 255;
  while((i < n) && (name[i] >= '0') && (name[i] <= '9'))
    i++;
  if(i > 0)
  {
    int j = i;
    while((j < n) && ((name[j] == ' ') || (name[j] == '_')))
      j++;
    if((j < n) && (name[j] == '-'))
    {
      j++;
      while((j < n) && ((name[j] == ' ') || (name[j] == '_')))
        j++;
      i = j;
    }
    else
      i = 0;
  }
  for(; i < n; i++)
  {
    char c = name[i];
    if((c == '(') || (c == '['))
      depth++;
    else if(((c == ')') || (c == ']')) && depth)
      depth--;
    else if(depth == 0)
    {
      if(c == '_')
        c = ' ';
      if((c == ' ') && ((o == 0) || (out[o - 1] == ' ')))
        continue;
      out[o++] = c;
    }
  }
  while((o > 0) && ((out[o - 1] == ' ') || (out[o - 1] == '-')))
    o--;
  if(o == 0)
  {
    memcpy(out, name, n);
    o = n;
  }
  out[o] = 0;
  return o;
}

void game_name_of_file(const char *file, char *out)
{
  const char *dot = strrchr(file, '.');
  game_name_clean(file, dot ? (int)(dot - file) : (int)strlen(file), out);
}
