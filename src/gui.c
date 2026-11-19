/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licens e as
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

#include "common.h"
#ifdef UI4
#include "ui/ui.h"
#endif

#define STATUS_ROWS 0
#define CURRENT_DIR_ROWS 1
#define FILE_LIST_ROWS 25
#define FILE_LIST_POSITION 10
#define DIR_LIST_POSITION 360
#define PAGE_SCROLL_NUM 5

#define GPSP_CONFIG_FILENAME "gpsp.cfg"

#define GAMEPAD_MENU_WIDTH 15

// gpSP config file header (4 bytes max)
#define GPSP_CONFIG_HEADER     "gpSP"
#define GPSP_CONFIG_HEADER_U32 0x50537067
const u32 gpsp_config_ver = 0x00010000;
gpsp_config_t g_gpsp_config;

// Per-game config file header (4 bytes max)
#define GAME_CONFIG_HEADER     "gcfg"
#define GAME_CONFIG_HEADER_U32 0x67666367
const u32 game_config_ver = 0x00010000;
game_config_t g_game_config;

#define MAKE_MENU(name, init_function, passive_function)                      \
  MENU_TYPE name##_menu =                                                     \
  {                                                                           \
    init_function,                                                            \
    passive_function,                                                         \
    name##_options,                                                           \
    sizeof(name##_options) / sizeof(MENU_OPTION_TYPE)                         \
  }                                                                           \

#define GAMEPAD_CONFIG_OPTION(display_string, number)                         \
{                                                                             \
  NULL,                                                                       \
  menu_fix_gamepad_help,                                                      \
  NULL,                                                                       \
  display_string,                                                             \
  gamepad_config_buttons,                                                     \
  gamepad_config_map + gamepad_config_line_to_button[number],                 \
  sizeof(gamepad_config_buttons) / sizeof(gamepad_config_buttons[0]),         \
  gamepad_help[gamepad_config_map[                                            \
   gamepad_config_line_to_button[number]]],                                   \
  number,                                                                     \
  STRING_SELECTION_TYPE                                                       \
}                                                                             \

#define ANALOG_CONFIG_OPTION(display_string, number)                          \
{                                                                             \
  NULL,                                                                       \
  menu_fix_gamepad_help,                                                      \
  NULL,                                                                       \
  display_string,                                                             \
  gamepad_config_buttons,                                                     \
  gamepad_config_map + number + 12,                                           \
  sizeof(gamepad_config_buttons) / sizeof(gamepad_config_buttons[0]),         \
  gamepad_help[gamepad_config_map[number + 12]],                              \
  number + 2,                                                                 \
  STRING_SELECTION_TYPE                                                     \
}                                                                             \

#define CHEAT_OPTION(number)                                                  \
{                                                                             \
  NULL,                                                                       \
  NULL,                                                                       \
  NULL,                                                                       \
  cheat_format_str[number],                                                   \
  enable_disable_options,                                                     \
  &(g_game_config.cheats_flag[number].cheat_active),                                 \
  2,                                                                          \
  msg[MSG_CHEAT_MENU_HELP_0],                                                 \
  (number) % 10,                                                              \
  STRING_SELECTION_TYPE | VERBATIM_TYPE                                       \
}                                                                             \

#define ACTION_OPTION(action_function, passive_function, display_string,      \
 help_string, line_number)                                                    \
{                                                                             \
  action_function,                                                            \
  passive_function,                                                           \
  NULL,                                                                       \
  display_string,                                                             \
  NULL,                                                                       \
  NULL,                                                                       \
  0,                                                                          \
  help_string,                                                                \
  line_number,                                                                \
  ACTION_TYPE                                                               \
}                                                                             \

#define SUBMENU_OPTION(sub_menu, display_string, help_string, line_number)    \
{                                                                             \
  NULL,                                                                       \
  NULL,                                                                       \
  sub_menu,                                                                   \
  display_string,                                                             \
  NULL,                                                                       \
  NULL,                                                                       \
  sizeof(sub_menu) / sizeof(MENU_OPTION_TYPE),                                \
  help_string,                                                                \
  line_number,                                                                \
  SUBMENU_TYPE                                                              \
}                                                                             \

#define SELECTION_OPTION(passive_function, display_string, options,           \
 option_ptr, num_options, help_string, line_number, type)                     \
{                                                                             \
  NULL,                                                                       \
  passive_function,                                                           \
  NULL,                                                                       \
  display_string,                                                             \
  options,                                                                    \
  option_ptr,                                                                 \
  num_options,                                                                \
  help_string,                                                                \
  line_number,                                                                \
  type                                                                        \
}                                                                             \

#define ACTION_SELECTION_OPTION(action_function, passive_function,            \
 display_string, options, option_ptr, num_options, help_string, line_number,  \
 type)                                                                        \
{                                                                             \
  action_function,                                                            \
  passive_function,                                                           \
  NULL,                                                                       \
  display_string,                                                             \
  options,                                                                    \
  option_ptr,                                                                 \
  num_options,                                                                \
  help_string,                                                                \
  line_number,                                                                \
  type | ACTION_TYPE                                                        \
}                                                                             \

#define STRING_SELECTION_OPTION(passive_function, display_string, options,    \
 option_ptr, num_options, help_string, line_number)                           \
  SELECTION_OPTION(passive_function, display_string, options,                 \
   option_ptr, num_options, help_string, line_number, STRING_SELECTION_TYPE)\

#define NUMERIC_SELECTION_OPTION(passive_function, display_string,            \
 option_ptr, num_options, help_string, line_number)                           \
  SELECTION_OPTION(passive_function, display_string, NULL, option_ptr,        \
   num_options, help_string, line_number, NUMBER_SELECTION_TYPE)            \

#define STRING_SELECTION_ACTION_OPTION(action_function, passive_function,     \
 display_string, options, option_ptr, num_options, help_string, line_number)  \
  ACTION_SELECTION_OPTION(action_function, passive_function,                  \
   display_string,  options, option_ptr, num_options, help_string,            \
   line_number, STRING_SELECTION_TYPE)                                      \

#define NUMERIC_SELECTION_ACTION_OPTION(action_function, passive_function,    \
 display_string, option_ptr, num_options, help_string, line_number)           \
  ACTION_SELECTION_OPTION(action_function, passive_function,                  \
   display_string,  NULL, option_ptr, num_options, help_string,               \
   line_number, NUMBER_SELECTION_TYPE)                                      \

#define NUMERIC_SELECTION_HIDE_OPTION(action_function,                 \
 passive_function, display_string, option_ptr, num_options, help_string,      \
 line_number)                                                                 \
  ACTION_SELECTION_OPTION(action_function, passive_function,                  \
   display_string, NULL, option_ptr, num_options, help_string,                \
   line_number, NUMBER_SELECTION_TYPE)                                      \

typedef enum
{
  NUMBER_SELECTION_TYPE = 0x01,
  STRING_SELECTION_TYPE = 0x02,
  SUBMENU_TYPE          = 0x04,
  ACTION_TYPE           = 0x08,
  // Drawn as is, without lining up a ':' with the other options. Cheat
  // lines use it because a cheat name may contain ':'.
  VERBATIM_TYPE         = 0x10
} MENU_OPTION_TYPE_ENUM;

struct _MENU_OPTION_TYPE
{
  void (* action_function)();
  void (* passive_function)();
  struct _MENU_TYPE *sub_menu;
  char *display_string;
  void *options;
  u32 *current_option;
  u32 num_options;
  char *help_string;
  u32 line_number;
  MENU_OPTION_TYPE_ENUM option_type;
};

struct _MENU_TYPE
{
  void (* init_function)();
  void (* passive_function)();
  struct _MENU_OPTION_TYPE *options;
  u32 num_options;
};

typedef struct _MENU_OPTION_TYPE MENU_OPTION_TYPE;
typedef struct _MENU_TYPE MENU_TYPE;


typedef enum
{
  MAIN_MENU,
  GAMEPAD_MENU,
  SAVESTATE_MENU,
  FRAMESKIP_MENU,
  CHEAT_MENU,
  ADHOC_MENU,
  MISC_MENU
} MENU_ENUM;

char g_default_rom_dir[MAX_PATH];
char g_default_save_dir[MAX_PATH];
char g_default_cfg_dir[MAX_PATH];
char g_default_ss_dir[MAX_PATH];
char g_default_cheat_dir[MAX_PATH];
u32 g_savestate_slot_num = 0;

char *msg[MSG_END];
char *msg_data; // the message strings, sized from the .msg file at load

static char m_font[MAX_PATH];
static char s_font[MAX_PATH];
static u32 menu_cheat_page = 0;
static u32 gamepad_config_line_to_button[] = { 8, 6, 7, 9, 1, 2, 3, 0, 4, 5, 11, 10 };

// Path of the savestate file whose preview get_savestate_snapshot() holds.
// The menu redraws every frame, so the file is read only when this changes.
static char snapshot_path[MAX_PATH];

static void get_savestate_snapshot(char *savestate_filename, u32 slot_num, u32 date_x, u32 date_y);
static void forget_savestate_snapshot(void);
static void get_savestate_filename(u32 slot, char *name_buffer);
static int sort_function(const void *dest_str_ptr, const void *src_str_ptr);
static u32 parse_line(char *current_line, char *current_str);
static void print_status(u32 mode);
static char *skip_padding(char *str);
static int menu_label_length(const char *line);
static int menu_label_width(const char *line, int length);
static void print_centered_240(char *str, u32 y, u16 *dest);
static void print_string_right(char *str, u32 y);
static void print_help(char *help);
static void get_timestamp_string(char *buffer, u16 msg_id, u16 year, u16 mon, u16 day, u16 wday, u16 hour, u16 min, u16 sec, u32 msec);
void save_ss_bmp(u16 *image);
void _flush_cache();
void button_up_wait();
void init_gpsp_config();

static int sort_function(const void *dest_str_ptr, const void *src_str_ptr)
{
  char *dest_str = *((char **)dest_str_ptr);
  char *src_str = *((char **)src_str_ptr);

  if(src_str[0] == '.')
    return 1;

  if(dest_str[0] == '.')
    return -1;

  return strcasecmp(dest_str, src_str);
}

/*--------------------------------------------------------
  Generic file browser / loader
--------------------------------------------------------*/
s32 load_file(char **wildcards, char *result,char *default_dir_name)
{
  DIR *current_dir;
  struct dirent *current_file;
  struct stat file_info;
  char current_dir_name[MAX_PATH];
  char current_dir_short[81];
  u32 current_dir_length;
  u32 total_filenames_allocated;
  u32 total_dirnames_allocated;
  char **file_list;
  char **dir_list;
  u32 num_files;                      // files in the current directory
  u32 num_dirs;                       // directories in the current directory
  char *file_name;
  u32 file_name_length;
  s32 ext_pos = -1;
  s32 return_value = 1;
  u32 current_file_selection;
  u32 current_file_scroll_value;
  u32 current_dir_selection;
  u32 current_dir_scroll_value;
  u32 current_file_in_scroll;
  u32 current_dir_in_scroll;
  u32 current_file_number, current_dir_number;
  u32 current_column = 0;
  u32 repeat;
  u32 i;
  gui_action_type gui_action;
  char utf8[2048];

  if (default_dir_name != NULL)
    chdir(default_dir_name);

  while(return_value == 1)
  {
    current_file_selection = 0;
    current_file_scroll_value = 0;
    current_dir_selection = 0;
    current_dir_scroll_value = 0;
    current_file_in_scroll = 0;
    current_dir_in_scroll = 0;

    total_filenames_allocated = 32;
    total_dirnames_allocated = 32;
    file_list = (char **)malloc(sizeof(char *) * 32);
    dir_list = (char **)malloc(sizeof(char *) * 32);
    memset(file_list, 0, sizeof(char *) * 32);
    memset(dir_list, 0, sizeof(char *) * 32);

    num_files = 0;
    num_dirs = 0;

    getcwd(current_dir_name, MAX_PATH);
    current_dir = opendir(current_dir_name);

    do
    {
      if(current_dir)
        current_file = readdir(current_dir);
      else
        current_file = NULL;

      if(current_file)
      {
        file_name = current_file->d_name;
        file_name_length = strlen(file_name);

        if((stat(file_name, &file_info) >= 0) &&
         ((file_name[0] != '.') || (file_name[1] == '.')))
        {
          if(S_ISDIR(file_info.st_mode))
          {
            dir_list[num_dirs] =
             (char *)malloc(file_name_length + 1);
             sprintf(dir_list[num_dirs], "%s", file_name);

            num_dirs++;
          }
          else
          {
            // Must match one of the wildcards, also ignore the .
            if(file_name_length >= 4)
            {
              if(file_name[file_name_length - 4] == '.')
                ext_pos = file_name_length - 4;
              else

              if(file_name[file_name_length - 3] == '.')
                ext_pos = file_name_length - 3;

              else
                ext_pos = 0;

              for(i = 0; wildcards[i] != NULL; i++)
              {
                if(!strcasecmp((file_name + ext_pos),
                 wildcards[i]))
                {
                  file_list[num_files] =
                   (char *)malloc(file_name_length + 1);

                  sprintf(file_list[num_files], "%s", file_name);

                  num_files++;
                  break;
                }
              }
            }
          }
        }

        if(num_files == total_filenames_allocated)
        {
          file_list = (char **)realloc(file_list, sizeof(char *) *
           total_filenames_allocated * 2);
          memset(file_list + total_filenames_allocated, 0,
           sizeof(u8 *) * total_filenames_allocated);
          total_filenames_allocated *= 2;
        }

        if(num_dirs == total_dirnames_allocated)
        {
          dir_list = (char **)realloc(dir_list, sizeof(char *) *
           total_dirnames_allocated * 2);
          memset(dir_list + total_dirnames_allocated, 0,
           sizeof(char *) * total_dirnames_allocated);
          total_dirnames_allocated *= 2;
        }
      }
    } while(current_file);

    qsort((void *)file_list, num_files, sizeof(u8 *), sort_function);
    qsort((void *)dir_list, num_dirs, sizeof(u8 *), sort_function);

    closedir(current_dir);

    current_dir_length = strlen(current_dir_name);

    if(current_dir_length > 80)
    {
      memcpy(current_dir_short, "...", 3);
      memcpy(current_dir_short + 3,
       current_dir_name + current_dir_length - 77, 77);
      current_dir_short[80] = 0;
    }
    else
    {
      memcpy(current_dir_short, current_dir_name,
       current_dir_length + 1);
    }

    repeat = 1;

    if(num_files == 0)
      current_column = 1;

  clear_screen(COLOR_BG);
  flip_screen();
  clear_screen(COLOR_BG);
  {
    while(repeat)
    {
      gui_action = get_gui_input();

      switch(gui_action)
      {
        case CURSOR_DOWN:
          if(current_column == 0)
          {
            if(current_file_selection < (num_files - 1))
            {
              current_file_selection++;
              if(current_file_in_scroll == (FILE_LIST_ROWS - CURRENT_DIR_ROWS - 1))
              {
                clear_screen(COLOR_BG);
                current_file_scroll_value++;
              }
              else
              {
                current_file_in_scroll++;
              }
            }
          }
          else
          {
            if(current_dir_selection < (num_dirs - 1))
            {
              current_dir_selection++;
              if(current_dir_in_scroll == (FILE_LIST_ROWS - CURRENT_DIR_ROWS - 1))
              {
                clear_screen(COLOR_BG);
                current_dir_scroll_value++;
              }
              else
              {
                current_dir_in_scroll++;
              }
            }
          }

          break;

        case CURSOR_RTRIGGER:
          if(current_column == 0)
          {
            if(num_files > PAGE_SCROLL_NUM)
            {
              if(current_file_selection < (num_files - PAGE_SCROLL_NUM))
              {
                current_file_selection += PAGE_SCROLL_NUM;
                if(current_file_in_scroll >= (FILE_LIST_ROWS - CURRENT_DIR_ROWS - PAGE_SCROLL_NUM))
                {
                  clear_screen(COLOR_BG);
                  current_file_scroll_value += PAGE_SCROLL_NUM;
                }
                else
                {
                  current_file_in_scroll += PAGE_SCROLL_NUM;
                }
              }
            }
          }
          else
          {
            if(num_dirs > PAGE_SCROLL_NUM)
            {
              if(current_dir_selection < (num_dirs - PAGE_SCROLL_NUM))
              {
                current_dir_selection += PAGE_SCROLL_NUM;
                if(current_dir_in_scroll >= (FILE_LIST_ROWS - CURRENT_DIR_ROWS - PAGE_SCROLL_NUM))
                {
                  clear_screen(COLOR_BG);
                  current_dir_scroll_value += PAGE_SCROLL_NUM;
                }
                else
                {
                  current_dir_in_scroll += PAGE_SCROLL_NUM;
                }
              }
            }
          }
          break;

        case CURSOR_UP:
          if(current_column == 0)
          {
            if(current_file_selection)
            {
              current_file_selection--;
              if(current_file_in_scroll == 0)
              {
                clear_screen(COLOR_BG);
                current_file_scroll_value--;
              }
              else
              {
                current_file_in_scroll--;
              }
            }
          }
          else
          {
            if(current_dir_selection)
            {
              current_dir_selection--;
              if(current_dir_in_scroll == 0)
              {
                clear_screen(COLOR_BG);
                current_dir_scroll_value--;
              }
              else
              {
                current_dir_in_scroll--;
              }
            }
          }
          break;

        case CURSOR_LTRIGGER:
          if(current_column == 0)
          {
            if(current_file_selection >= PAGE_SCROLL_NUM)
            {
              current_file_selection -= PAGE_SCROLL_NUM;
              if(current_file_in_scroll < PAGE_SCROLL_NUM)
              {
                clear_screen(COLOR_BG);
                current_file_scroll_value -= PAGE_SCROLL_NUM;
              }
              else
              {
                current_file_in_scroll -= PAGE_SCROLL_NUM;
              }
            }
          }
          else
          {
            if(current_dir_selection >= PAGE_SCROLL_NUM)
            {
              current_dir_selection -= PAGE_SCROLL_NUM;
              if(current_dir_in_scroll < PAGE_SCROLL_NUM)
              {
                clear_screen(COLOR_BG);
                current_dir_scroll_value -= PAGE_SCROLL_NUM;
              }
              else
              {
                current_dir_in_scroll -= PAGE_SCROLL_NUM;
              }
            }
          }
          break;

        case CURSOR_RIGHT:
          if(current_column == 0)
          {
            if(num_dirs != 0)
              current_column = 1;
          }
          break;

        case CURSOR_LEFT:
          if(current_column == 1)
          {
            if(num_files != 0)
              current_column = 0;
          }
          break;

        case CURSOR_SELECT:
          if(current_column == 1)
          {
            repeat = 0;
            chdir(dir_list[current_dir_selection]);
          }
          else
          {
            if(num_files != 0)
            {
              repeat = 0;
              return_value = 0;
              strcpy(result, file_list[current_file_selection]);
              getcwd(rom_path, MAX_PATH);
            }
          }
          break;

        case CURSOR_BACK:

          if(!strcmp(current_dir_name, "ms0:/PSP"))
            break;

          repeat = 0;
          chdir("..");
          break;

        case CURSOR_EXIT:
          return_value = -1;
          repeat = 0;
          break;

        default:
          ;
          break;
      }
      print_status(1);
      PRINT_STRING_BG_SJIS(utf8, current_dir_short, COLOR_ROM_INFO, COLOR_BG, 0, (CURRENT_DIR_ROWS * 10));
      PRINT_STRING_BG(msg[MSG_RETURN_MENU], COLOR_HELP_TEXT, COLOR_BG, 20, 260);

      for(i = 0, current_file_number = i + current_file_scroll_value;
       i < (FILE_LIST_ROWS - CURRENT_DIR_ROWS); i++, current_file_number++)
      {
        if(current_file_number < num_files)
        {
          if((current_file_number == current_file_selection) &&
           (current_column == 0))
          {
            PRINT_STRING_BG_SJIS(utf8, file_list[current_file_number], COLOR_ACTIVE_ITEM,
             COLOR_BG, FILE_LIST_POSITION, ((i + CURRENT_DIR_ROWS + 1) * 10));
          }
          else
          {
            PRINT_STRING_BG_SJIS(utf8, file_list[current_file_number], COLOR_INACTIVE_ITEM,
             COLOR_BG, FILE_LIST_POSITION, ((i + CURRENT_DIR_ROWS + 1) * 10));
          }
        }
      }

      for(i = 0, current_dir_number = i + current_dir_scroll_value;
       i < (FILE_LIST_ROWS - CURRENT_DIR_ROWS); i++, current_dir_number++)
      {
        if(current_dir_number < num_dirs)
        {
          if((current_dir_number == current_dir_selection) &&
           (current_column == 1))
          {
            PRINT_STRING_BG_SJIS(utf8, dir_list[current_dir_number], COLOR_ACTIVE_ITEM,
             COLOR_BG, DIR_LIST_POSITION, ((i + CURRENT_DIR_ROWS + 1) * 10));
          }
          else
          {
            PRINT_STRING_BG_SJIS(utf8, dir_list[current_dir_number], COLOR_INACTIVE_ITEM,
             COLOR_BG, DIR_LIST_POSITION, ((i + CURRENT_DIR_ROWS + 1) * 10));
          }
        }
      }
      scrollbar(2, 22, 7, 257, num_files, FILE_LIST_ROWS, current_file_scroll_value);

      flip_screen();
      clear_screen(COLOR_BG);
    }
  }
    for(i = 0; i < num_files; i++)
    {
      free(file_list[i]);
    }
    free(file_list);

    for(i = 0; i < num_dirs; i++)
    {
      free(dir_list[i]);
    }
    free(dir_list);
  }

  chdir(main_path);

  clear_screen(COLOR_BG);
  flip_screen();
  return return_value;
}

// Default key map
const u32 gamepad_config_map_init[MAX_GAMEPAD_CONFIG_MAP] =
{
    BUTTON_ID_MENU,     /* △ */
    BUTTON_ID_B,        /* × */
    BUTTON_ID_A,        /* ○ */
    BUTTON_ID_FPS,      /* □ */
    BUTTON_ID_L,        /* [L] */
    BUTTON_ID_R,        /* [R] */
    BUTTON_ID_DOWN,     /* ↓ */
    BUTTON_ID_LEFT,     /* ← */
    BUTTON_ID_UP,       /* ↑ */
    BUTTON_ID_RIGHT,    /* → */
    BUTTON_ID_SELECT,   /* [SELECT] */
    BUTTON_ID_START,    /* [START] */
    BUTTON_ID_UP,       /* (↑) */
    BUTTON_ID_DOWN,     /* (↓) */
    BUTTON_ID_LEFT,     /* (←) */
    BUTTON_ID_RIGHT     /* (→) */
};

void init_game_config()
{
  u32 i;
  g_game_config.frameskip_type = auto_frameskip;
  g_game_config.frameskip_value = 9;
  g_game_config.random_skip = NO;
  g_game_config.clock_speed_number = 9;
  g_game_config.audio_buffer_size_number = 2;
  g_game_config.update_backup_flag = YES;
  for(i = 0; i < MAX_CHEATS; i++)
  {
    g_game_config.cheats_flag[i].cheat_active = NO;
    g_game_config.cheats_flag[i].cheat_name[0] = '\0';
  }
  memcpy(g_game_config.gamepad_config_map, gamepad_config_map_init, sizeof(gamepad_config_map_init));
  g_game_config.use_default_gamepad_map = YES;
  g_game_config.allocate_sensor = NO;
  g_game_config.rapidfire_speed = RAPIDFIRE_FAST;
  g_game_config.play_seconds = 0;
  g_game_config.last_played = 0;
  g_game_config.last_slot = 0;
  g_game_config.cheat_locks = 0;
}

void init_gpsp_config()
{
  int temp;
  g_gpsp_config.screen_mode = scaled_aspect;
  g_gpsp_config.screen_scale = 100;
  g_gpsp_config.screen_filter = filter_sharp_bilinear;  /* crisp at any scale (#74) */
  g_gpsp_config.screen_ratio = R4_3;
  g_gpsp_config.screen_interlace = PROGRESSIVE;
  g_gpsp_config.enable_audio = YES;
  g_gpsp_config.enable_analog = NO;
  g_gpsp_config.analog_sensitivity_level = 4;
  g_gpsp_config.enable_home = NO;
  memcpy(g_gpsp_config.gamepad_config_map, gamepad_config_map_init, sizeof(gamepad_config_map_init));
  memcpy(gamepad_config_map, g_gpsp_config.gamepad_config_map, sizeof(g_gpsp_config.gamepad_config_map));
  temp = g_gpsp_config.language;
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &temp);
  g_gpsp_config.language = temp;
  g_gpsp_config.emulate_core = ASM_CORE;
  g_gpsp_config.debug_flag = NO;
  g_gpsp_config.fake_fat = NO;
  g_gpsp_config.boot_mode = GAME_CART;
  g_gpsp_config.fast_forward_mode = FF_TOGGLE;
  g_gpsp_config.fast_forward_sound = YES;
  g_gpsp_config.show_fps = NO;
  g_gpsp_config.save_on_leave = YES;
  g_gpsp_config.side_bars = SIDE_BARS_BLACK;
  g_gpsp_config.confirm_button = CONFIRM_SYSTEM;
  g_gpsp_config.ff_speed = FF_SPEED_3X;
  g_gpsp_config.bios_note = YES;
  g_gpsp_config.first_run_done = NO;
#if 0
  g_gpsp_config.solar_level = 0;
#endif
}

/*--------------------------------------------------------
  Load the per-game config file
--------------------------------------------------------*/
void load_game_config_file(void)
{
  char game_config_filename[MAX_FILE];
  char game_config_path[MAX_PATH];
  FILE_ID game_config_file;
  u32 header, ver;

  init_game_config();

  change_ext(gamepak_filename, game_config_filename, ".cfg");

  path_printf(game_config_path, sizeof(game_config_path), "%s/%s", g_default_cfg_dir, game_config_filename);

  FILE_OPEN(game_config_file, game_config_path, READ);

  if(FILE_CHECK_VALID(game_config_file))
  {
    FILE_READ_VARIABLE(game_config_file, header);
    if(header == GAME_CONFIG_HEADER_U32)
    {
      FILE_READ_VARIABLE(game_config_file, ver);
      switch(ver)
      {
        case 0x10000: /* 1.0 */
          FILE_READ_VARIABLE(game_config_file, g_game_config);
          if(g_game_config.use_default_gamepad_map == YES)
            memcpy(gamepad_config_map, g_gpsp_config.gamepad_config_map, sizeof(g_gpsp_config.gamepad_config_map));
          else
            memcpy(gamepad_config_map, g_game_config.gamepad_config_map, sizeof(g_game_config.gamepad_config_map));
          break;
      }
    }
  }
  FILE_CLOSE(game_config_file);
}

/*--------------------------------------------------------
  Load the gpSP config file
--------------------------------------------------------*/
s32 load_config_file()
{
  char gpsp_config_path[MAX_PATH];
  FILE_ID gpsp_config_file;
  u32 header, ver;

  init_gpsp_config();
  path_printf(gpsp_config_path, sizeof(gpsp_config_path), "%s/%s", main_path, GPSP_CONFIG_FILENAME);

  FILE_OPEN(gpsp_config_file, gpsp_config_path, READ);

  if(FILE_CHECK_VALID(gpsp_config_file))
  {
    FILE_READ_VARIABLE(gpsp_config_file, header);
    if(header != GPSP_CONFIG_HEADER_U32)
    {
      FILE_CLOSE(gpsp_config_file);
      return -1;
    }
    FILE_READ_VARIABLE(gpsp_config_file, ver);
    switch(ver)
    {
      case 0x10000: /* 1.0 */
        FILE_READ_VARIABLE(gpsp_config_file, g_gpsp_config);
        break;
    }
    FILE_CLOSE(gpsp_config_file);
    memcpy(gamepad_config_map, g_gpsp_config.gamepad_config_map, sizeof(g_gpsp_config.gamepad_config_map));
    return 0;
  }
  FILE_CLOSE(gpsp_config_file);
  return -1;
}

// Fill in an option's value, giving the line drawn in the menu.
static void format_menu_option(MENU_OPTION_TYPE *option, char *buffer)
{
  if(option->option_type & NUMBER_SELECTION_TYPE)
  {
    sprintf(buffer, option->display_string, *(option->current_option));
  }
  else if(option->option_type & STRING_SELECTION_TYPE)
  {
    sprintf(buffer, option->display_string,
     ((u32 *)option->options)[*(option->current_option)]);
  }
  else
  {
    strcpy(buffer, option->display_string);
  }
}

u32 menu(u16 *original_screen)
{
  gui_action_type gui_action;
  u32 i;
  u32 repeat = 1;
  u32 return_value = 0;
  u32 first_load = 0;
  char current_savestate_filename[MAX_FILE];
  char line_buffer[256];
  int value_x;
  char cheat_format_str[MAX_CHEATS][41*4];

  MENU_TYPE *current_menu;
  MENU_OPTION_TYPE *current_option;
  MENU_OPTION_TYPE *display_option;
  u32 current_option_num;

  auto void choose_menu(MENU_TYPE *new_menu);
  auto void menu_exit();
  auto void menu_quit();
  auto void menu_load();
  auto void menu_restart();
  auto void menu_save_ss();
  auto void menu_change_state();
  auto void menu_save_state();
  auto void menu_load_state();
  auto void menu_load_state_file();
  auto void menu_delete_state();
  auto void menu_load_cheat_file();
  auto void menu_fix_gamepad_help();
  auto void submenu_graphics_sound();
  auto void submenu_cheats();
  auto void submenu_misc();
  auto void submenu_gamepad();
  auto void submenu_analog();
  auto void submenu_savestate();
  auto void submenu_main();
  auto void reload_cheats_page();
  auto void home_mode();
  auto void set_gamepad();
#ifdef USE_ADHOC
  auto void adhoc_connect_menu();
  auto void adhoc_disconnect_menu();
#endif

  char *gamepad_help[] =
  {
    msg[MSG_PAD_MENU_CFG_HELP_0],
    msg[MSG_PAD_MENU_CFG_HELP_1],
    msg[MSG_PAD_MENU_CFG_HELP_2],
    msg[MSG_PAD_MENU_CFG_HELP_3],
    msg[MSG_PAD_MENU_CFG_HELP_4],
    msg[MSG_PAD_MENU_CFG_HELP_5],
    msg[MSG_PAD_MENU_CFG_HELP_6],
    msg[MSG_PAD_MENU_CFG_HELP_7],
    msg[MSG_PAD_MENU_CFG_HELP_8],
    msg[MSG_PAD_MENU_CFG_HELP_9],
    msg[MSG_PAD_MENU_CFG_HELP_10],
    msg[MSG_PAD_MENU_CFG_HELP_11],
    msg[MSG_PAD_MENU_CFG_HELP_12],
    msg[MSG_PAD_MENU_CFG_HELP_13],
    msg[MSG_PAD_MENU_CFG_HELP_14],
    msg[MSG_PAD_MENU_CFG_HELP_15],
    msg[MSG_PAD_MENU_CFG_HELP_16],
    msg[MSG_PAD_MENU_CFG_HELP_17],
    msg[MSG_PAD_MENU_CFG_HELP_18],
    msg[MSG_PAD_MENU_CFG_HELP_19],
    msg[MSG_PAD_MENU_CFG_HELP_20],
    msg[MSG_PAD_MENU_CFG_HELP_21]
  };

  void menu_exit()
  {
    if(!first_load)
      repeat = 0;
  }

  void menu_quit()
  {
    if(!first_load && g_gpsp_config.save_on_leave)
      save_last_state(original_screen);
    quit(0);
  }

  void menu_load()
  {
    char *file_ext[] = { ".gba", ".bin", ".zip", NULL };
    char load_filename[MAX_FILE];

    save_config_file();
    save_game_config_file();

//    if(!g_gpsp_config.update_backup_flag) // TODO: reconsider when to do this
      update_backup_force();

    while(load_file(file_ext, load_filename, g_default_rom_dir) != -1)
    {
      // Save the game being left into slot 10 before the new ROM replaces
      // it. After a failed load first_load is set, so this runs once.
      if(!first_load)
      {
        note_game_left();
        save_game_config_file();
        if(g_gpsp_config.save_on_leave)
          save_last_state(original_screen);
      }

      if(load_gamepak(load_filename) != -1)
      {
        reset_gba();
        return_value = 1;
        repeat = 0;
        reg[CHANGED_PC_STATUS] = 1;
        return;
      }

      // The failed load may have overwritten the old ROM, so continue as
      // if no game were loaded; the file list opens again.
      gamepak_filename[0] = 0;
      first_load = 1;
      memset(original_screen, 0x00, 240 * 160 * 2);
      print_centered_240(msg[MSG_NON_LOAD_GAME], 75, original_screen);
      set_cpu_clock(10);
    }

    choose_menu(current_menu);
  }

  void menu_restart()
  {
    if(!first_load)
    {
      reset_gba();
      reg[CHANGED_PC_STATUS] = 1;
      return_value = 1;
      repeat = 0;
    }
  }

  void menu_save_ss()
  {
    if(!first_load)
      save_ss_bmp(original_screen);
  }

  void menu_change_state()
  {
    get_savestate_filename(g_savestate_slot_num, current_savestate_filename);
  }

  void menu_save_state()
  {
    menu_change_state();
    if(!first_load)
    {
      get_savestate_filename_noshot(g_savestate_slot_num, current_savestate_filename);
      save_state(current_savestate_filename, original_screen, g_savestate_slot_num);
      // Show the new preview and date at once.
      forget_savestate_snapshot();
      pause_sound(1);
      clear_screen(COLOR_BG);
      blit_to_screen(original_screen, 240, 160, 230, 40);
    }
  }

  void menu_load_state()
  {
    menu_change_state();
    if(!first_load)
    {
      if (load_state(current_savestate_filename, g_savestate_slot_num) == 1)
      {
        return_value = 1;
        repeat = 0;
      }
      else
      {
        clear_screen(COLOR_BG);
        blit_to_screen(original_screen, 240, 160, 230, 40);
      }
    }
  }

  void menu_delete_state()
  {
    char buf[256];
    char savestate_path[MAX_PATH];
    SceIoStat stat;

    menu_change_state();
    if(first_load)
      return;

    if(g_savestate_slot_num == MEM_STATE_NUM)
    {
      // Slot 10 is in memory, with a copy in its file
      if(mem_save_flag == 0)
        return;
    }
    else
    {
      get_savestate_filename_noshot(g_savestate_slot_num, current_savestate_filename);
      path_printf(savestate_path, sizeof(savestate_path), "%s/%s", g_default_save_dir, current_savestate_filename);
      if(sceIoGetstat(savestate_path, &stat) < 0)
        return;  // empty slot
    }

    // yesno_dialog() returns 0 for yes
    sprintf(buf, msg[MSG_DELETE_STATE_NUM], (int)g_savestate_slot_num);
    if(yesno_dialog(buf) == 0)
    {
      if(g_savestate_slot_num == MEM_STATE_NUM)
        delete_last_state();
      else
        delete_state_files(savestate_path);
      forget_savestate_snapshot();
    }

    clear_screen(COLOR_BG);
    blit_to_screen(original_screen, 240, 160, 230, 40);
  }

  void menu_load_state_file()
  {
    char *file_ext[] = { ".svs", NULL };
    char load_filename[512];

    // A savestate only applies to the game it was made with.
    if(first_load)
      return;

    if(load_file(file_ext, load_filename, g_default_save_dir) != -1)
    {
      if (load_state(load_filename, g_savestate_slot_num) == 1)
      {
        return_value = 1;
        repeat = 0;
      }
      else
      {
        clear_screen(COLOR_BG);
        choose_menu(current_menu);
      }
    }
    else
    {
      choose_menu(current_menu);
    }
  }

  void menu_load_cheat_file()
  {
    char *file_ext[] = { ".cht", NULL };
    char load_filename[MAX_FILE];
    u32 i;

    if(load_file(file_ext, load_filename, g_default_cheat_dir) != -1)
    {
      add_cheats(load_filename);
      for(i = 0; i < MAX_CHEATS; i++)
      {
        if(i >= g_cheats_num)
        {
          sprintf(cheat_format_str[i], msg[MSG_CHEAT_MENU_NON_LOAD], i);
        }
        else
        {
          sprintf(cheat_format_str[i], msg[MSG_CHEAT_MENU_0], i, g_game_config.cheats_flag[i].cheat_name);
        }
      }
      choose_menu(current_menu);

    }
    else
    {
      choose_menu(current_menu);
    }
  }

  void menu_fix_gamepad_help()
  {
    current_option->help_string =
     gamepad_help[gamepad_config_map[gamepad_config_line_to_button[current_option_num]]];
  }

  void submenu_graphics_sound()
  {

  }

  void submenu_cheats()
  {

  }

  void submenu_misc()
  {

  }

  void submenu_gamepad()
  {

  }

#ifdef USE_ADHOC
  void submenu_adhoc()
  {

  }

  void adhoc_connect_menu()
  {
    set_cpu_clock(g_game_config.clock_speed_number);
    adhoc_init(gamepak_title);
    adhoc_select();
  }

  void adhoc_disconnect_menu()
  {
    adhoc_exit();
  }

#endif

  void submenu_analog()
  {

  }

  void submenu_savestate()
  {
    PRINT_STRING_BG(msg[MSG_STATE_MENU_TITLE], COLOR_HELP_TEXT, COLOR_BG, 10, 70);
    menu_change_state();
  }

  void submenu_main()
  {
    get_savestate_filename_noshot(g_savestate_slot_num, current_savestate_filename);

    // On the load and save rows, the preview shows the selected slot (the
    // only main menu rows that change the slot), with its date below it.
    if(!first_load && (current_option->current_option == &g_savestate_slot_num))
      get_savestate_snapshot(current_savestate_filename, g_savestate_slot_num, 230, 200);
  }

  char *yes_no_options[] =
  {
    msg[MSG_NO],
    msg[MSG_YES]
  };

  char *fast_forward_options[] =
  {
    msg[MSG_FF_TOGGLE],
    msg[MSG_FF_HOLD]
  };

  char *rapidfire_speed_options[] =
  {
    msg[MSG_RAPIDFIRE_SLOW],
    msg[MSG_RAPIDFIRE_MEDIUM],
    msg[MSG_RAPIDFIRE_FAST]
  };

  char *filter_options[] =
  {
    msg[MSG_FILTER_OFF],
    msg[MSG_FILTER_BILINEAR],
    msg[MSG_FILTER_SHARP_BILINEAR]
  };

  char *enable_disable_options[] =
  {
    msg[MSG_DISABLED],
    msg[MSG_ENABLED]
  };

  char *scale_options[] =
  {
    msg[MSG_SCALE_UNSCALED],
    msg[MSG_SCALE_SCALED],
    msg[MSG_SCALE_FULL],
    msg[MSG_SCN_OPT1],
    msg[MSG_SCN_OPT2]
  };

  char *frameskip_options[] = { msg[MSG_FS_AUTO], msg[MSG_FS_MANUAL], msg[MSG_FS_OFF] };
  char *frameskip_variation_options[] = { msg[MSG_FS_UNIFORM], msg[MSG_FS_RANDOM] };

  char *audio_buffer_options[] =
  {
    msg[MSG_SB_2048], msg[MSG_SB_3072], msg[MSG_SB_4096], msg[MSG_SB_5120], msg[MSG_SB_6144],
    msg[MSG_SB_7168], msg[MSG_SB_8192], msg[MSG_SB_9216], msg[MSG_SB_10240], msg[MSG_SB_11264], msg[MSG_SB_12288]
  };

  char *update_backup_options[] = { msg[MSG_BK_EXITONLY], msg[MSG_BK_AUTO] };

  char *clock_speed_options[] =
  {
    msg[MSG_CLK_33], msg[MSG_CLK_66], msg[MSG_CLK_100], msg[MSG_CLK_133], msg[MSG_CLK_166],
    msg[MSG_CLK_200], msg[MSG_CLK_233], msg[MSG_CLK_266], msg[MSG_CLK_300], msg[MSG_CLK_333]
  };

  char *gamepad_config_buttons[] =
  {
    msg[MSG_PAD_MENU_CFG_0],
    msg[MSG_PAD_MENU_CFG_1],
    msg[MSG_PAD_MENU_CFG_2],
    msg[MSG_PAD_MENU_CFG_3],
    msg[MSG_PAD_MENU_CFG_4],
    msg[MSG_PAD_MENU_CFG_5],
    msg[MSG_PAD_MENU_CFG_6],
    msg[MSG_PAD_MENU_CFG_7],
    msg[MSG_PAD_MENU_CFG_8],
    msg[MSG_PAD_MENU_CFG_9],
    msg[MSG_PAD_MENU_CFG_10],
    msg[MSG_PAD_MENU_CFG_11],
    msg[MSG_PAD_MENU_CFG_12],
    msg[MSG_PAD_MENU_CFG_13],
    msg[MSG_PAD_MENU_CFG_14],
    msg[MSG_PAD_MENU_CFG_15],
    msg[MSG_PAD_MENU_CFG_16],
    msg[MSG_PAD_MENU_CFG_17],
    msg[MSG_PAD_MENU_CFG_18],
    msg[MSG_PAD_MENU_CFG_19],
    msg[MSG_PAD_MENU_CFG_20],
    msg[MSG_PAD_MENU_CFG_21],
  };

  char *ratio_options[] = { msg[MSG_R_4_3], msg[MSG_R_16_9] };

  char *interlace_options[] = { msg[MSG_I_NON], msg[MSG_I_ON] };

  // Display names, in the order of lang[] (the message file names). They are
  // in English because the menu font may not have the native scripts.
  char *language_options[] =
  {
    "Japanese",
    "English",
    "French",
    "Spanish",
    "German",
    "Italian",
    "Dutch",
    "Portuguese",
    "Russian",
    "Korean",
    "Chinese (Traditional)",
    "Chinese (Simplified)"
  };

  char *boot_mode_options[] =
  {
      "GAME CART",
      "GBA BIOS"
  };

#ifdef USE_C_CORE
  char *emulate_core_options[] =
  {
    "ASM CORE",
    "C CORE"
  };
#endif

  MENU_OPTION_TYPE graphics_sound_options[] =
  {
    /* 00 */ STRING_SELECTION_OPTION(NULL, msg[MSG_SCALE], scale_options, &g_gpsp_config.screen_mode, 5, msg[MSG_G_S_MENU_HELP_0], 0),
    /* 01 */ STRING_SELECTION_OPTION(NULL, msg[MSG_FILTER], filter_options, &g_gpsp_config.screen_filter, 3, msg[MSG_G_S_MENU_HELP_1], 1),
    /* 02 */ STRING_SELECTION_OPTION(NULL, msg[MSG_RATIO], ratio_options, &g_gpsp_config.screen_ratio, 2, msg[MSG_G_S_MENU_HELP_9], 2),
    /* 03 */ STRING_SELECTION_OPTION(NULL, msg[MSG_INTERLACE], interlace_options, &g_gpsp_config.screen_interlace, 2, msg[MSG_G_S_MENU_HELP_11], 3),
    /* 04 */
    /* 05 */ STRING_SELECTION_OPTION(NULL, msg[MSG_SKIP_TYPE], frameskip_options, &g_game_config.frameskip_type, 3, msg[MSG_G_S_MENU_HELP_2], 5),
    /* 06 */ NUMERIC_SELECTION_OPTION(NULL, msg[MSG_SKIP_VALUE], &g_game_config.frameskip_value, 100, msg[MSG_G_S_MENU_HELP_3], 6),
    /* 07 */ STRING_SELECTION_OPTION(NULL, msg[MSG_SKIP_RANDOM], frameskip_variation_options, &g_game_config.random_skip, 2, msg[MSG_G_S_MENU_HELP_4], 7),
    /* 08 */ STRING_SELECTION_OPTION(NULL, msg[MSG_FF_MODE], fast_forward_options, &g_gpsp_config.fast_forward_mode, 2, msg[MSG_G_S_MENU_HELP_FF_MODE], 8),
    /* 09 */ STRING_SELECTION_OPTION(NULL, msg[MSG_AUDIO_ENABLE], yes_no_options, &g_gpsp_config.enable_audio, 2, msg[MSG_G_S_MENU_HELP_5], 9),
    /* 10 */ STRING_SELECTION_OPTION(NULL, msg[MSG_AUDIO_BUFFER], audio_buffer_options, &g_game_config.audio_buffer_size_number, 11, msg[MSG_G_S_MENU_HELP_6], 10),
    /* 11 */ STRING_SELECTION_OPTION(NULL, msg[MSG_FF_SOUND], yes_no_options, &g_gpsp_config.fast_forward_sound, 2, msg[MSG_G_S_MENU_HELP_FF_SOUND], 11),
    /* 12 */ ACTION_OPTION(menu_save_ss, NULL, msg[MSG_SCREEN_SHOT], msg[MSG_G_S_MENU_HELP_7], 12),
    /* 13 */ STRING_SELECTION_OPTION(NULL, msg[MSG_SHOW_FPS], yes_no_options, &g_gpsp_config.show_fps, 2, msg[MSG_G_S_MENU_HELP_SHOW_FPS], 13),
    /* 14 */NUMERIC_SELECTION_OPTION(NULL, msg[MSG_DISPLAY_SCALE], &g_gpsp_config.screen_scale, 201, msg[MSG_G_S_MENU_HELP_10], 14),
    /* 15 */
    /* 16 */ SUBMENU_OPTION(NULL, msg[MSG_MENU_RETURN_MAIN], msg[MSG_G_S_MENU_HELP_8], 16)
  };

  MAKE_MENU(graphics_sound, submenu_graphics_sound, NULL);

  MENU_OPTION_TYPE cheats_options[] =
  {
    /* 00 */ CHEAT_OPTION((10 * menu_cheat_page) + 0),
    /* 01 */ CHEAT_OPTION((10 * menu_cheat_page) + 1),
    /* 02 */ CHEAT_OPTION((10 * menu_cheat_page) + 2),
    /* 03 */ CHEAT_OPTION((10 * menu_cheat_page) + 3),
    /* 04 */ CHEAT_OPTION((10 * menu_cheat_page) + 4),
    /* 05 */ CHEAT_OPTION((10 * menu_cheat_page) + 5),
    /* 06 */ CHEAT_OPTION((10 * menu_cheat_page) + 6),
    /* 07 */ CHEAT_OPTION((10 * menu_cheat_page) + 7),
    /* 08 */ CHEAT_OPTION((10 * menu_cheat_page) + 8),
    /* 09 */ CHEAT_OPTION((10 * menu_cheat_page) + 9),
    /* 10 */ NUMERIC_SELECTION_OPTION(reload_cheats_page, msg[MSG_CHEAT_MENU_5], &menu_cheat_page, MAX_CHEATS_PAGE, msg[MSG_CHEAT_MENU_HELP_5], 11),
    /* 11 */ ACTION_OPTION(menu_load_cheat_file, NULL, msg[MSG_CHEAT_MENU_1], msg[MSG_CHEAT_MENU_HELP_1], 13),
    /* 12 */
    /* 13 */
    /* 14 */
    /* 15 */
    /* 16 */ SUBMENU_OPTION(NULL, msg[MSG_CHEAT_MENU_4], msg[MSG_CHEAT_MENU_HELP_4], 16)
  };

  MAKE_MENU(cheats, submenu_cheats, NULL);

  MENU_OPTION_TYPE misc_options[] =
  {
    STRING_SELECTION_OPTION(NULL, msg[MSG_CHEAT_MENU_2], clock_speed_options, &g_game_config.clock_speed_number, 10, msg[MSG_CHEAT_MENU_HELP_2], 0),
    STRING_SELECTION_OPTION(NULL, msg[MSG_CHEAT_MENU_3], update_backup_options, &g_game_config.update_backup_flag, 2, msg[MSG_CHEAT_MENU_HELP_3], 1),
    STRING_SELECTION_OPTION(home_mode, msg[MSG_CHEAT_MENU_6], yes_no_options, &g_gpsp_config.enable_home, 2, msg[MSG_CHEAT_MENU_HELP_6], 2),

#ifdef USE_C_CORE
    STRING_SELECTION_OPTION(NULL, msg[MSG_MISC_MENU_0], emulate_core_options, &g_gpsp_config.emulate_core , 2, msg[MSG_MISC_MENU_HELP_0], 3),
#endif

#ifdef USE_DEBUG
    STRING_SELECTION_OPTION(NULL, msg[MSG_MISC_MENU_1], yes_no_options, &g_gpsp_config.debug_flag, 2, msg[MSG_MISC_MENU_HELP_1], 4),
#endif

    STRING_SELECTION_OPTION(NULL, msg[MSG_MISC_MENU_2], language_options, &g_gpsp_config.language, 12, msg[MSG_MISC_MENU_HELP_2], 4),
    STRING_SELECTION_OPTION(NULL, msg[MSG_MISC_MENU_3], yes_no_options, &g_gpsp_config.fake_fat, 2, msg[MSG_MISC_MENU_HELP_3], 6),
    STRING_SELECTION_OPTION(NULL, msg[MSG_MISC_MENU_4], boot_mode_options, &g_gpsp_config.boot_mode, 2, msg[MSG_MISC_MENU_HELP_4], 7),
#if 0
    NUMERIC_SELECTION_OPTION(NULL, msg[MSG_MISC_MENU_5], &g_gpsp_config.solar_level, 25, msg[MSG_MISC_MENU_HELP_5], 8),
#endif
    SUBMENU_OPTION(NULL, msg[MSG_CHEAT_MENU_4], msg[MSG_CHEAT_MENU_HELP_4], 16)
  };

  MAKE_MENU(misc, submenu_misc, NULL);

  MENU_OPTION_TYPE savestate_options[] =
  {
    NUMERIC_SELECTION_HIDE_OPTION(menu_load_state, menu_change_state, msg[MSG_STATE_MENU_0], &g_savestate_slot_num, 11, msg[MSG_STATE_MENU_HELP_0], 6),
    NUMERIC_SELECTION_HIDE_OPTION(menu_save_state, menu_change_state, msg[MSG_STATE_MENU_1], &g_savestate_slot_num, 11, msg[MSG_STATE_MENU_HELP_1], 7),

    NUMERIC_SELECTION_HIDE_OPTION(menu_load_state_file, menu_change_state, msg[MSG_STATE_MENU_2], &g_savestate_slot_num, 11, msg[MSG_STATE_MENU_HELP_2], 9),
    NUMERIC_SELECTION_HIDE_OPTION(menu_delete_state, menu_change_state, msg[MSG_STATE_MENU_5], &g_savestate_slot_num, 11, msg[MSG_STATE_MENU_HELP_5], 10),

    NUMERIC_SELECTION_OPTION(menu_change_state, msg[MSG_STATE_MENU_3], &g_savestate_slot_num, 11, msg[MSG_STATE_MENU_HELP_3], 12),
    STRING_SELECTION_OPTION(NULL, msg[MSG_SAVE_ON_LEAVE], yes_no_options, &g_gpsp_config.save_on_leave, 2, msg[MSG_STATE_MENU_HELP_SAVE_ON_LEAVE], 13),



    SUBMENU_OPTION(NULL, msg[MSG_STATE_MENU_4], msg[MSG_STATE_MENU_HELP_4], 15)
  };

  MAKE_MENU(savestate, submenu_savestate, NULL);

  MENU_OPTION_TYPE gamepad_config_options[] =
  {
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_0], 0),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_1], 1),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_2], 2),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_3], 3),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_4], 4),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_5], 5),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_6], 6),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_7], 7),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_8], 8),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_9], 9),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_10], 10),
    GAMEPAD_CONFIG_OPTION(msg[MSG_PAD_MENU_11], 11),

    STRING_SELECTION_OPTION(set_gamepad, msg[MSG_PAD_MENU_12], yes_no_options, &g_game_config.use_default_gamepad_map, 2, msg[MSG_PAD_MENU_HELP_12], 13),
    STRING_SELECTION_OPTION(NULL, msg[MSG_RAPIDFIRE_SPEED], rapidfire_speed_options, &g_game_config.rapidfire_speed, 3, msg[MSG_PAD_MENU_HELP_RAPIDFIRE], 14),

    SUBMENU_OPTION(NULL, msg[MSG_PAD_MENU_13], msg[MSG_PAD_MENU_HELP_13], 15)
  };

  MAKE_MENU(gamepad_config, submenu_gamepad, NULL);

  MENU_OPTION_TYPE analog_config_options[] =
  {
    ANALOG_CONFIG_OPTION(msg[MSG_A_PAD_MENU_0], 0),
    ANALOG_CONFIG_OPTION(msg[MSG_A_PAD_MENU_1], 1),
    ANALOG_CONFIG_OPTION(msg[MSG_A_PAD_MENU_2], 2),
    ANALOG_CONFIG_OPTION(msg[MSG_A_PAD_MENU_3], 3),



    STRING_SELECTION_OPTION(NULL, msg[MSG_A_PAD_MENU_4], yes_no_options, &g_gpsp_config.enable_analog, 2, msg[MSG_A_PAD_MENU_HELP_0], 7),
    NUMERIC_SELECTION_OPTION(NULL, msg[MSG_A_PAD_MENU_5], &g_gpsp_config.analog_sensitivity_level, 10, msg[MSG_A_PAD_MENU_HELP_1], 8),
    STRING_SELECTION_OPTION(NULL, msg[MSG_A_PAD_MENU_6], yes_no_options, &g_game_config.allocate_sensor, 2, msg[MSG_A_PAD_MENU_HELP_2], 9),
    STRING_SELECTION_OPTION(set_gamepad, msg[MSG_PAD_MENU_12], yes_no_options, &g_game_config.use_default_gamepad_map, 2, msg[MSG_PAD_MENU_HELP_12], 11),



    SUBMENU_OPTION(NULL, msg[MSG_A_PAD_MENU_7], msg[MSG_A_PAD_MENU_HELP_3], 15)
  };

  MAKE_MENU(analog_config, submenu_analog, NULL);

#ifdef USE_ADHOC
  MENU_OPTION_TYPE adhoc_options[] =
  {
    ACTION_OPTION(adhoc_connect_menu, NULL, "adhoc connect", "adhoc connect", 0),
    ACTION_OPTION(adhoc_disconnect_menu, NULL, "adhoc disconnect", "adhoc disconnect", 1),














    SUBMENU_OPTION(NULL, "exit", "exit", 15)
  };

  MAKE_MENU(adhoc, submenu_adhoc, NULL);
#endif

  MENU_OPTION_TYPE main_options[] =
  {
    /* Same order and lines as b225/b230, except that the first two are
       swapped on purpose (#44): the menu opens on Return to game */
    ACTION_OPTION(menu_exit, NULL, msg[MSG_MAIN_MENU_9], msg[MSG_MAIN_MENU_HELP_9], 0),
    ACTION_OPTION(menu_load, NULL, msg[MSG_MAIN_MENU_7], msg[MSG_MAIN_MENU_HELP_7], 1),
    ACTION_OPTION(menu_restart, NULL, msg[MSG_MAIN_MENU_8], msg[MSG_MAIN_MENU_HELP_8], 2),

    NUMERIC_SELECTION_ACTION_OPTION(menu_load_state, NULL, msg[MSG_MAIN_MENU_1], &g_savestate_slot_num, 11, msg[MSG_MAIN_MENU_HELP_1], 4),
    NUMERIC_SELECTION_ACTION_OPTION(menu_save_state, NULL, msg[MSG_MAIN_MENU_2], &g_savestate_slot_num, 11, msg[MSG_MAIN_MENU_HELP_2], 5),
    SUBMENU_OPTION(&savestate_menu, msg[MSG_MAIN_MENU_3], msg[MSG_MAIN_MENU_HELP_3], 6),

    SUBMENU_OPTION(&cheats_menu, msg[MSG_MAIN_MENU_6], msg[MSG_MAIN_MENU_HELP_6], 8),
#ifdef USE_ADHOC
    SUBMENU_OPTION(&adhoc_menu, "adhoc", "adhoc", 9),
#endif

    SUBMENU_OPTION(&gamepad_config_menu, msg[MSG_MAIN_MENU_4], msg[MSG_MAIN_MENU_HELP_4], 10),
    SUBMENU_OPTION(&analog_config_menu, msg[MSG_MAIN_MENU_5], msg[MSG_MAIN_MENU_HELP_5], 11),

    SUBMENU_OPTION(&graphics_sound_menu, msg[MSG_MAIN_MENU_0], msg[MSG_MAIN_MENU_HELP_0], 13),
    SUBMENU_OPTION(&misc_menu, msg[MSG_MAIN_MENU_11], msg[MSG_MAIN_MENU_HELP_11], 14),

    ACTION_OPTION(menu_quit, NULL, msg[MSG_MAIN_MENU_10], msg[MSG_MAIN_MENU_HELP_10], 16)
  };

  MAKE_MENU(main, submenu_main, NULL);

  void choose_menu(MENU_TYPE *new_menu)
  {
    if(new_menu == NULL)
      new_menu = &main_menu;

    clear_screen(COLOR_BG);
    blit_to_screen(original_screen, 240, 160, 230, 40);

    current_menu = new_menu;

    current_option = new_menu->options;
    current_option_num = 0;

    // With no game loaded, Return to game does nothing: start on Load new game.
    if((new_menu == &main_menu) && first_load)
    {
      u32 option_num;

      for(option_num = 0; option_num < new_menu->num_options; option_num++)
      {
        if(new_menu->options[option_num].action_function == menu_load)
        {
          current_option_num = option_num;
          current_option = new_menu->options + option_num;
          break;
        }
      }
    }

    if(current_menu->init_function)
     current_menu->init_function();
  }

  void reload_cheats_page()
  {
    for(i = 0; i<10; i++)
    {
      cheats_options[i].display_string = cheat_format_str[(10 * menu_cheat_page) + i];
      cheats_options[i].current_option = &(g_game_config.cheats_flag[(10 * menu_cheat_page) + i].cheat_active);
    }
  }

  void home_mode()
  {
    sceImposeSetHomePopup(g_gpsp_config.enable_home);
  }

  void set_gamepad()
  {
    if(g_game_config.use_default_gamepad_map == 1)
    {
      memcpy(g_game_config.gamepad_config_map, gamepad_config_map, sizeof(g_game_config.gamepad_config_map));
      memcpy(gamepad_config_map, g_gpsp_config.gamepad_config_map, sizeof(g_gpsp_config.gamepad_config_map));
    }
    else
    {
      memcpy(g_gpsp_config.gamepad_config_map, gamepad_config_map, sizeof(g_gpsp_config.gamepad_config_map));
      memcpy(gamepad_config_map, g_game_config.gamepad_config_map, sizeof(g_game_config.gamepad_config_map));
    }
  }

  pause_sound(1);

  button_up_wait();

  video_resolution(FRAME_MENU);

  clear_screen(COLOR_BG);

  // The menu runs at 222 MHz
  set_cpu_clock(10);

  // A savestate may have been saved with the in-game buttons since the
  // menu was last open.
  forget_savestate_snapshot();

  if(gamepak_filename[0] == 0)
  {
    first_load = 1;
    memset(original_screen, 0x00, 240 * 160 * 2);
    print_centered_240(msg[MSG_NON_LOAD_GAME], 75, original_screen);
  }

  choose_menu(&main_menu);
  current_menu = &main_menu;

  for(i = 0; i < MAX_CHEATS; i++)
  {
    if(i >= g_cheats_num)
    {
      sprintf(cheat_format_str[i], msg[MSG_CHEAT_MENU_NON_LOAD], i);
    }
    else
    {
      sprintf(cheat_format_str[i], msg[MSG_CHEAT_MENU_0], i, g_game_config.cheats_flag[i].cheat_name);
    }
  }

  reload_cheats_page();

  current_menu->init_function();

  while(repeat)
  {

    print_status(0);
    blit_to_screen(original_screen, 240, 160, 230, 40);

    // Every option's ':' and value start at the same column, right after
    // the widest label of this menu.
    value_x = 0;
    display_option = current_menu->options;

    for(i = 0; i < current_menu->num_options; i++, display_option++)
    {
      int label_length;

      format_menu_option(display_option, line_buffer);
      label_length = (display_option->option_type & VERBATIM_TYPE) ? -1 :
       menu_label_length(line_buffer);

      if(label_length >= 0)
      {
        int label_width = menu_label_width(line_buffer, label_length);

        if(label_width > value_x)
          value_x = label_width;
      }
    }
    value_x += 10;

    display_option = current_menu->options;

    for(i = 0; i < current_menu->num_options; i++, display_option++)
    {
      u32 item_color;
      u32 item_y;
      int label_length;

      format_menu_option(display_option, line_buffer);

      item_color = (display_option == current_option) ?
       COLOR_ACTIVE_ITEM : COLOR_INACTIVE_ITEM;
      item_y = (display_option->line_number * 10) + 40;
      label_length = (display_option->option_type & VERBATIM_TYPE) ? -1 :
       menu_label_length(line_buffer);

      if(label_length >= 0)
      {
        char *colon = strchr(line_buffer, ':');

        PRINT_STRING_BG(colon, item_color, COLOR_BG, value_x, item_y);
        line_buffer[label_length] = 0;
        PRINT_STRING_BG(line_buffer, item_color, COLOR_BG, 10, item_y);
      }
      else
      {
        PRINT_STRING_BG(line_buffer, item_color, COLOR_BG, 10, item_y);
      }
    }

    print_help(current_option->help_string);

    gui_action = get_gui_input();

//    if (quit_flag == 1)
//      menu_quit();

    switch(gui_action)
    {
      case CURSOR_DOWN:
        current_option_num = (current_option_num + 1) %
          current_menu->num_options;

        current_option = current_menu->options + current_option_num;
//        clear_help();
        break;

      case CURSOR_UP:
        if(current_option_num)
          current_option_num--;
        else
          current_option_num = current_menu->num_options - 1;

        current_option = current_menu->options + current_option_num;
//        clear_help();
        break;

      case CURSOR_RIGHT:
        if(current_option->option_type & (NUMBER_SELECTION_TYPE |
         STRING_SELECTION_TYPE))
        {
          *(current_option->current_option) =
           (*current_option->current_option + 1) %
           current_option->num_options;

          if(current_option->passive_function)
            current_option->passive_function();
        }
        break;

      case CURSOR_LEFT:
        if(current_option->option_type & (NUMBER_SELECTION_TYPE |
         STRING_SELECTION_TYPE))
        {
          u32 current_option_val = *(current_option->current_option);

          if(current_option_val)
            current_option_val--;
          else
            current_option_val = current_option->num_options - 1;

          *(current_option->current_option) = current_option_val;

          if(current_option->passive_function)
            current_option->passive_function();
        }
        break;

      case CURSOR_EXIT:
        if(current_menu == &main_menu)
          menu_exit();

        choose_menu(&main_menu);
        break;

      case CURSOR_SELECT:
        if(current_option->option_type & ACTION_TYPE)
          current_option->action_function();

        else if(current_option->option_type & SUBMENU_TYPE)
          choose_menu(current_option->sub_menu);
        break;

      case KEY_SELECT:
        break;

      default:
        ;
        break;
    }  // end switch
    if(current_menu->init_function)
     current_menu->init_function();
    flip_screen();
    clear_screen(COLOR_BG);
  }  // end while

  button_up_wait();

  clear_screen(0);
  flip_screen();
  clear_screen(0);
  flip_screen();

  video_resolution(FRAME_GAME);

  set_cpu_clock(g_game_config.clock_speed_number);

  pause_sound(0);
  real_frame_count = 0;
  virtual_frame_count = 0;
  return return_value;
}

void button_up_wait()
{
  SceCtrlData ctrl_data;
  sceKernelDelayThread(10);

  while(sceCtrlPeekBufferPositive(&ctrl_data, 1), (((g_use_home == 1) ? (ctrl_data.Buttons | readHomeButton()) : (ctrl_data.Buttons)) & 0x1F3F9) != 0)
  {
    sceKernelDelayThread(10);
  }
}


u32 load_dircfg(char *file_name)  // TODO: rewrite more cleanly
{
  int loop;
  char current_line[256];
  char current_str[256];
  FILE *msg_file;
  char msg_path[MAX_PATH];

  path_printf(msg_path, sizeof(msg_path), "%s/%s", main_path, file_name);

  msg_file = fopen(msg_path, "r");

  if(msg_file)
  {
    loop = 0;
    while(fgets(current_line, 256, msg_file))
    {
      if(parse_line(current_line, current_str) != -1)
      {
        switch(loop)
        {
          case 0:
            if(opendir(current_str) != NULL)
              strcpy(g_default_rom_dir,current_str);
            else
            {
              g_default_rom_dir[0] = '\0';
              pspDebugScreenInit();
              printf("not open rom dir : %s\n",current_str);
              sceKernelDelayThread(500000*2);
              init_video();
              video_resolution(FRAME_MENU);
            }
            loop++;
            break;

          case 1:
            if(opendir(current_str) != NULL)
              strcpy(g_default_save_dir,current_str);
            else
            {
              g_default_save_dir[0] = '\0';
              pspDebugScreenInit();
              printf("not open save dir : %s\n",current_str);
              sceKernelDelayThread(500000*2);
              init_video();
              video_resolution(FRAME_MENU);
            }
            loop++;
            break;

          case 2:
            if(opendir(current_str) != NULL)
              strcpy(g_default_cfg_dir,current_str);
            else
            {
              g_default_cfg_dir[0] = '\0';
              pspDebugScreenInit();
              printf("not open cfg dir : %s\n",current_str);
              sceKernelDelayThread(500000*2);
              init_video();
              video_resolution(FRAME_MENU);
            }
            loop++;
            break;

          case 3:
            if(opendir(current_str) != NULL)
              strcpy(g_default_ss_dir,current_str);
            else
            {
              g_default_ss_dir[0] = '\0';
              pspDebugScreenInit();
              printf("not open screen shot dir : %s\n",current_str);
              sceKernelDelayThread(500000*2);
              init_video();
              video_resolution(FRAME_MENU);
            }
            loop++;
            break;

          case 4:
            if(opendir(current_str) != NULL)
              strcpy(g_default_cheat_dir,current_str);
            else
            {
              g_default_cheat_dir[0] = '\0';
              pspDebugScreenInit();
              printf("not open cheat dir : %s\n",current_str);
              sceKernelDelayThread(500000*2);
              init_video();
              video_resolution(FRAME_MENU);
            }
            loop++;
            break;
        }
      }
    }

    fclose(msg_file);
    if (loop == 5)
    {
      return 0;
    }
    else
    {
      return -1;
    }
  }
  fclose(msg_file);
  return -1;
}

u32 load_fontcfg(char *file_name)  // TODO: rewrite more cleanly
{
  int loop;
  char current_line[256];
  char current_str[256];
  FILE *msg_file;
  char msg_path[MAX_PATH];

  path_printf(msg_path, sizeof(msg_path), "%s/%s", main_path, file_name);

  m_font[0] = '\0';
  s_font[0] = '\0';

  msg_file = fopen(msg_path, "rb");

  if(msg_file)
  {
    loop = 0;
    while(fgets(current_line, 256, msg_file))
    {
      if(parse_line(current_line, current_str) != -1)
      {
        switch(loop)
        {
          case 0:
            strcpy(m_font, current_str);
            loop++;
            break;
          case 1:
            strcpy(s_font, current_str);
            loop++;
            break;
        }
      }
    }

    fclose(msg_file);
    if (loop > 0)
      return 0;
    else
      return -1;
  }
  fclose(msg_file);
  return -1;
}

u32 load_msgcfg(char *file_name)
{
  int loop;
  int next_line;
  char current_line[256];
  char current_str[256];
  FILE *msg_file;
  char msg_path[MAX_PATH];
  u32 offset;
  long file_size;

  path_printf(msg_path, sizeof(msg_path), "%s/%s", main_path, file_name);

  msg_file = fopen(msg_path, "rb");
  if(msg_file == NULL)
    return -1;

  /* The parsed strings never take more bytes than the file: each '!' line
     loses its '!' and line end, and every other line adds one NUL. */
  fseek(msg_file, 0, SEEK_END);
  file_size = ftell(msg_file);
  fseek(msg_file, 0, SEEK_SET);
  free(msg_data);
  msg_data = (file_size > 0) ? malloc(file_size + 1) : NULL;
  if(msg_data == NULL)
  {
    fclose(msg_file);
    return -1;
  }

  next_line = 0;
  offset = 0;
  loop = 0;
  while(fgets(current_line, 256, msg_file))
  {
    if(parse_line(current_line, current_str) != -1)
    {
      if(next_line == 0)
      {
        if(loop == MSG_END)
        {
          /* more messages than message.h knows */
          loop++;
          break;
        }
        msg[loop] = &msg_data[offset];
        next_line = 1;
        loop++;
      }
      strcpy(&msg_data[offset], current_str);
      offset = offset + strlen(current_str);  // offset points at the terminating NUL
    }
    else
    {
      next_line = 0;
      offset++;
    }
  }

  fclose(msg_file);
  return (loop == MSG_END) ? 0 : -1;
}

u32 load_font()
{
    return fbm_init(m_font,s_font);
}

void get_savestate_filename_noshot(u32 slot, char *name_buffer)
{
  char savestate_ext[16];

  sprintf(savestate_ext, "_%d.svs", (int)slot);
  change_ext(gamepak_filename, name_buffer, savestate_ext);
}

/*--------------------------------------------------------
  Recent games
  <cfg dir>/recent.txt lists the games played, newest first, one full
  path per line (UTF-8), at most RECENT_MAX. It is written when a game
  is left; the 4.0 library reads it.
--------------------------------------------------------*/
#define RECENT_MAX 30
#define RECENT_FILENAME "recent.txt"

static void update_recent_list(const char *game_path)
{
  static char lines[RECENT_MAX][MAX_PATH];
  char list_path[MAX_PATH];
  FILE *list_file;
  u32 count = 0, i;

  path_printf(list_path, sizeof(list_path), "%s/%s", g_default_cfg_dir, RECENT_FILENAME);

  list_file = fopen(list_path, "r");
  if(list_file)
  {
    while((count < RECENT_MAX) && fgets(lines[count], MAX_PATH, list_file))
    {
      lines[count][strcspn(lines[count], "\r\n")] = 0;
      if((lines[count][0] != 0) && strcasecmp(lines[count], game_path))
        count++;
    }
    fclose(list_file);
  }
  if(count == RECENT_MAX)
    count--;  // the oldest drops off

  list_file = fopen(list_path, "w");
  if(list_file == NULL)
    return;
  fprintf(list_file, "%s\n", game_path);
  for(i = 0; i < count; i++)
    fprintf(list_file, "%s\n", lines[i]);
  fclose(list_file);
}

// The player is leaving the running game: stamp it and move it to the
// top of the Recent list. The caller saves the game config afterwards.
void note_game_left(void)
{
  ScePspDateTime now;
  time_t now_t;

  if((gamepak_filename[0] == 0) || (gamepak_path[0] == 0))
    return;

  if((sceRtcGetCurrentClock(&now, 0) >= 0) && (sceRtcGetTime_t(&now, &now_t) >= 0))
    g_game_config.last_played = (u32)now_t;
  update_recent_list(gamepak_path);
}

/*--------------------------------------------------------
  Low battery
  Once a second while a game runs: on battery at BATTERY_SAVE_PERCENT or
  less, save the game into slot 10 once and say so. Charging or a level
  above it arms the save again.
--------------------------------------------------------*/
#define BATTERY_SAVE_PERCENT 4

void check_battery_save(void)
{
  static u32 frame = 0;
  static u32 armed = 1;
  int percent;
  u16 *screen;
  u32 saved;

  if(++frame < 60)
    return;
  frame = 0;

  if((gamepak_filename[0] == 0) || (scePowerIsBatteryExist() <= 0))
    return;
  percent = scePowerGetBatteryLifePercent();
  if(percent < 0)
    return;
#ifdef UI4
  ui_game_battery(percent, scePowerIsBatteryCharging() <= 0);
#endif
  if((scePowerIsBatteryCharging() > 0) || (percent > BATTERY_SAVE_PERCENT))
  {
    armed = 1;
    return;
  }
  if(!armed)
    return;
  armed = 0;

  screen = copy_screen();  // the savestate's picture, before the menu mode
  if(screen == NULL)
    return;

  video_resolution(FRAME_MENU);
  clear_screen(COLOR_BG);
  saved = save_last_state(screen);
  free(screen);
  message_dialog(msg[saved ? MSG_BATTERY_LOW_SAVED : MSG_BATTERY_LOW_NOT_SAVED],
   msg[MSG_BATTERY_LOW_CONTINUE]);
  button_up_wait();  // the button that closed the message stays out of the game

  clear_screen(0);
  flip_screen();
  clear_screen(0);
  flip_screen();
  video_resolution(FRAME_GAME);
  real_frame_count = 0;
  virtual_frame_count = 0;
  pause_sound(0);
}

/*--------------------------------------------------------
  Save the per-game config file
--------------------------------------------------------*/
s32 save_game_config_file()
{
  char game_config_filename[MAX_FILE];
  char game_config_path[MAX_PATH];
  FILE_ID game_config_file;

  if(gamepak_filename[0] == 0) return -1;

  change_ext(gamepak_filename, game_config_filename, ".cfg");

  path_printf(game_config_path, sizeof(game_config_path), "%s/%s", g_default_cfg_dir, game_config_filename);

  if(g_game_config.use_default_gamepad_map == 0)
    memcpy(g_game_config.gamepad_config_map, gamepad_config_map, sizeof(g_game_config.gamepad_config_map));

  FILE_OPEN(game_config_file, game_config_path, WRITE);
  if(FILE_CHECK_VALID(game_config_file))
  {
    FILE_WRITE(game_config_file, (int *)GAME_CONFIG_HEADER, sizeof(u32));
    FILE_WRITE_VARIABLE(game_config_file, game_config_ver);
    FILE_WRITE_VARIABLE(game_config_file, g_game_config);
    FILE_CLOSE(game_config_file);
    return 0;
  }
  FILE_CLOSE(game_config_file);
  return -1;
}

/*--------------------------------------------------------
  Save the gpSP config file
--------------------------------------------------------*/
s32 save_config_file()
{
  char gpsp_config_path[MAX_PATH];
  FILE_ID gpsp_config_file;

  path_printf(gpsp_config_path, sizeof(gpsp_config_path), "%s/%s", main_path, GPSP_CONFIG_FILENAME);

  save_game_config_file();

  if(g_game_config.use_default_gamepad_map == 1)
    memcpy(g_gpsp_config.gamepad_config_map, gamepad_config_map, sizeof(g_gpsp_config.gamepad_config_map));

  FILE_OPEN(gpsp_config_file, gpsp_config_path, WRITE);
  if(FILE_CHECK_VALID(gpsp_config_file))
  {
    FILE_WRITE(gpsp_config_file, (int *)GPSP_CONFIG_HEADER, sizeof(u32));
    FILE_WRITE_VARIABLE(gpsp_config_file, gpsp_config_ver);
    FILE_WRITE_VARIABLE(gpsp_config_file, g_gpsp_config);
    FILE_CLOSE(gpsp_config_file);
    return 0;
  }
  FILE_CLOSE(gpsp_config_file);
  return -1;
}

// Show a slot's savestate picture in the preview area, and its date at
// date_x, date_y.
static void get_savestate_snapshot(char *savestate_filename, u32 slot_num, u32 date_x, u32 date_y)
  {
    static u16 snapshot_buffer[240 * 160];
    char savestate_timestamp_string[80];
    char savestate_path[MAX_PATH];
    FILE_ID savestate_file;
    static u64 savestate_time_flat;
    u64 local_time;
    int wday;
    ScePspDateTime current_time;
    static u32 valid_flag;

    path_printf(savestate_path, sizeof(savestate_path), "%s/%s", g_default_save_dir, savestate_filename);

    if (slot_num != MEM_STATE_NUM)
    {
      if(strcmp(savestate_path, snapshot_path) != 0)
      {
        FILE_OPEN(savestate_file, savestate_path, READ);
        strcpy(snapshot_path, savestate_path);
        if(FILE_CHECK_VALID(savestate_file))
        {
          char str[4];
          FILE_READ_ARRAY(savestate_file, str);
          if(memcmp(str, g_state_str, 4) == 0)
            FILE_SEEK(savestate_file, 8, SEEK_SET);
          else
            FILE_SEEK(savestate_file, 0, SEEK_SET);
          FILE_READ_ARRAY(savestate_file, snapshot_buffer);
          FILE_READ_VARIABLE(savestate_file, savestate_time_flat);
          FILE_CLOSE(savestate_file);
          valid_flag = 1;
        }
        else
          valid_flag = 0;
      }
    }
    else
    {
      // Slot 10 lives in memory and replaces the buffered preview, so the
      // next file slot must be read again.
      forget_savestate_snapshot();
      valid_flag = 0;

      if (mem_save_flag == 1)
      {
        g_state_buffer_ptr = savestate_write_buffer;
        if(memcmp(g_state_buffer_ptr, g_state_str, 4) == 0)
        {
          g_state_buffer_ptr += 8;
          FILE_READ_MEM_ARRAY(g_state_buffer_ptr, snapshot_buffer);
          FILE_READ_MEM_VARIABLE(g_state_buffer_ptr, savestate_time_flat);
          valid_flag = 1;
        }
      }
    }

    if (valid_flag == 1)
      {
        sceRtcConvertUtcToLocalTime(&savestate_time_flat, &local_time);

        sceRtcSetTick(&current_time, &local_time);
        wday = sceRtcGetDayOfWeek(current_time.year, current_time.month, current_time.day);
        get_timestamp_string(savestate_timestamp_string, MSG_STATE_MENU_DATE_FMT_0, current_time.year, current_time.month, current_time.day,
            wday, current_time.hour, current_time.minute, current_time.second, 0);

        savestate_timestamp_string[40] = 0;

        PRINT_STRING_BG(savestate_timestamp_string, COLOR_HELP_TEXT, COLOR_BG, date_x, date_y);
      }
    else
      {
        memset(snapshot_buffer, 0, 240 * 160 * 2);
        print_centered_240(msg[(slot_num == MEM_STATE_NUM) ?
         MSG_STATE_MENU_MEMORY_NONE : MSG_STATE_MENU_STATE_NONE], 65, snapshot_buffer);
        get_timestamp_string(savestate_timestamp_string, MSG_STATE_MENU_DATE_NONE_0, 0, 0, 0, 0, 0, 0, 0, 0);
        PRINT_STRING_BG(savestate_timestamp_string, COLOR_HELP_TEXT, COLOR_BG, date_x, date_y);
      }
    blit_to_screen(snapshot_buffer, 240, 160, 230, 40);

    // Slot 10 is the last state saved or loaded from any slot, kept in
    // memory only (takka's quick save); label its picture.
    if ((slot_num == MEM_STATE_NUM) && (valid_flag == 1))
      PRINT_STRING_BG(msg[MSG_STATE_MENU_MEMORY_LABEL], COLOR_HELP_TEXT, COLOR_BG, 232, 42);
  }

// Make the next get_savestate_snapshot() read the file again.
static void forget_savestate_snapshot(void)
{
  snapshot_path[0] = 0;
}

static void get_savestate_filename(u32 slot, char *name_buffer)
{
  char savestate_ext[16];

  sprintf(savestate_ext, "_%d.svs", (int)slot);
  change_ext(gamepak_filename, name_buffer, savestate_ext);

  get_savestate_snapshot(name_buffer, g_savestate_slot_num, 10, 40);
}

static u32 parse_line(char *current_line, char *current_str)
{
  char *line_ptr;
  char *line_ptr_new;

  line_ptr = current_line;
  /* NULL or comment or other */
  if((current_line[0] == 0) || (current_line[0] == '#') || (current_line[0] != '!'))
    return -1;

  line_ptr++;

  line_ptr_new = strchr(line_ptr, '\r');
  while (line_ptr_new != NULL)
  {
    *line_ptr_new = '\n';
    line_ptr_new = strchr(line_ptr, '\r');
  }

  line_ptr_new = strchr(line_ptr, '\n');
  if (line_ptr_new == NULL)
    return -1;

  *line_ptr_new = 0;

  // "\n" to '\n'
  line_ptr_new = strstr(line_ptr, "\\n");
  while (line_ptr_new != NULL)
  {
    *line_ptr_new = '\n';
    memmove((line_ptr_new + 1), (line_ptr_new + 2), (strlen(line_ptr_new + 2) + 1));
    line_ptr_new = strstr(line_ptr_new, "\\n");
  }

  strcpy(current_str, line_ptr);
  return 0;
}

/*------------------------------------------------------
  Menu text alignment

  The menu font used to be proportional, so the message files padded text
  with spaces and '$' (a 1 px blank in gpsp10.fbm) to line up the ':' of
  each option or to centre a line. Text is now drawn in fixed-width cells,
  so that padding is ignored and the alignment is done here instead.
------------------------------------------------------*/

// Skip the leading padding of a message.
static char *skip_padding(char *str)
{
  while ((*str == ' ') || (*str == '$'))
    str++;

  return str;
}

// Length of an option's label: the text before the first ':', without
// trailing padding. -1 if the line has no ':'.
static int menu_label_length(const char *line)
{
  const char *colon = strchr(line, ':');
  int length;

  if (colon == NULL)
    return -1;

  length = colon - line;
  while ((length > 0) && ((line[length - 1] == ' ') || (line[length - 1] == '$')))
    length--;

  return length;
}

static int menu_label_width(const char *line, int length)
{
  char label[256];

  if (length >= (int)sizeof(label))
    length = sizeof(label) - 1;

  memcpy(label, line, length);
  label[length] = 0;

  return fbm_getwidth(label);
}

// Centre a message in a 240 px wide image, such as the game preview.
static void print_centered_240(char *str, u32 y, u16 *dest)
{
  int x;

  str = skip_padding(str);
  x = (240 - fbm_getwidth(str)) / 2;
  if (x < 0)
    x = 0;

  PRINT_STRING_EXT_BG(str, 0xFFFF, 0x0000, x, y, dest, 240);
}

// Draw an option's help text at the bottom of the 5-line help area. The
// message files used to pad help texts with leading blank lines for this;
// leading and trailing blank lines are now ignored.
#define HELP_Y      210
#define HELP_LINES  5

static void print_help(char *help)
{
  char buffer[1024];
  int length;
  int lines;
  int i;

  while (*help == '\n')
    help++;

  snprintf(buffer, sizeof(buffer), "%s", help);
  length = strlen(buffer);
  while ((length > 0) && (buffer[length - 1] == '\n'))
    buffer[--length] = 0;

  if (length == 0)
    return;

  lines = 1;
  for (i = 0; i < length; i++)
  {
    if (buffer[i] == '\n')
      lines++;
  }
  if (lines > HELP_LINES)
    lines = HELP_LINES;

  PRINT_STRING_BG(buffer, COLOR_HELP_TEXT, COLOR_BG, 30,
   HELP_Y + (HELP_LINES - lines) * 10);
}

// Draw header text so that it ends at the right edge of the screen.
static void print_string_right(char *str, u32 y)
{
  int x = 470 - fbm_getwidth(str);

  if (x < 0)
    x = 0;

  PRINT_STRING_BG(str, COLOR_HELP_TEXT, COLOR_BG, x, y);
}

static void print_status(u32 mode)
{
  char print_buffer_1[256];
  char print_buffer_2[256];
  char utf8[2048];
  ScePspDateTime current_time;

  sceRtcGetCurrentClockLocalTime(&current_time);
  int wday = sceRtcGetDayOfWeek(current_time.year, current_time.month , current_time.day);

  get_timestamp_string(print_buffer_1, MSG_MENU_DATE_FMT_0, current_time.year, current_time.month , current_time.day, wday,
    current_time.hour, current_time.minute, current_time.second, 0);
  sprintf(print_buffer_2,"%s%s", msg[MSG_MENU_DATE], print_buffer_1);
  PRINT_STRING_BG(print_buffer_2, COLOR_HELP_TEXT, COLOR_BG, 10, 0);

  // Right of row 1: time left (or "Charging") then the battery level, so
  // the battery stays at the right edge.
  if (scePowerIsBatteryExist() > 0)
  {
    int minutes = scePowerGetBatteryLifeTime();

    print_buffer_1[0] = 0;
    if (scePowerIsBatteryCharging() > 0)
      sprintf(print_buffer_1, "%s", msg[MSG_MENU_BATTERY_CHARGING]);
    else if (minutes >= 0)
      sprintf(print_buffer_1, msg[MSG_MENU_BATTERY_TIME], minutes / 60, minutes % 60);
    if (print_buffer_1[0] != 0)
      strcat(print_buffer_1, "  ");

    sprintf(print_buffer_2, msg[MSG_MENU_BATTERY], scePowerGetBatteryLifePercent(), minutes);
    strcat(print_buffer_1, print_buffer_2);
    print_string_right(print_buffer_1, 0);
  }

  // Right of row 2: the version.
  print_string_right(VERSION_STRING, 10);

  if (mode == 0)
  {
    snprintf(print_buffer_1, sizeof(print_buffer_1), "%.80s", gamepak_filename);  /* show at most 80 characters */
    PRINT_STRING_BG_SJIS(utf8, print_buffer_1, COLOR_ROM_INFO, COLOR_BG, 10, 20);
    sprintf(print_buffer_1, "%s  %s  %s  %0X", gamepak_title, gamepak_code, gamepak_maker, (unsigned int)gamepak_crc32);
    PRINT_STRING_BG(print_buffer_1, COLOR_ROM_INFO, COLOR_BG, 10, 10);
  }
}

static void get_timestamp_string(char *buffer, u16 msg_id, u16 year, u16 mon, u16 day, u16 wday, u16 hour, u16 min, u16 sec, u32 msec)
{
  char *weekday_strings[] =
  {
    msg[MSG_WDAY_0], msg[MSG_WDAY_1], msg[MSG_WDAY_2], msg[MSG_WDAY_3],
    msg[MSG_WDAY_4], msg[MSG_WDAY_5], msg[MSG_WDAY_6], ""
  };

  // The b225 message files put the weekday after the date in the menu and
  // savestate formats, but keep it first in the screenshot file name formats
  // (MSG_SS_FMT_1/2). The argument order must match, or %s reads an integer.
  u32 weekday_first = (msg_id == MSG_SS_FMT_0);

  switch(date_format)
  {
    case PSP_SYSTEMPARAM_DATE_FORMAT_YYYYMMDD:
      sprintf(buffer, msg[msg_id    ], year, mon, day, weekday_strings[wday], hour, min, sec, msec / 1000);
      break;
    case PSP_SYSTEMPARAM_DATE_FORMAT_MMDDYYYY:
      if(weekday_first)
        sprintf(buffer, msg[msg_id + 1], weekday_strings[wday], mon, day, year, hour, min, sec, msec / 1000);
      else
        sprintf(buffer, msg[msg_id + 1], mon, day, year, weekday_strings[wday], hour, min, sec, msec / 1000);
      break;
    case PSP_SYSTEMPARAM_DATE_FORMAT_DDMMYYYY:
      if(weekday_first)
        sprintf(buffer, msg[msg_id + 2], weekday_strings[wday], day, mon, year, hour, min, sec, msec / 1000);
      else
        sprintf(buffer, msg[msg_id + 2], day, mon, year, weekday_strings[wday], hour, min, sec, msec / 1000);
      break;
  }
}

void save_ss_bmp(u16 *image)
{
  static unsigned char header[] ={ 'B',  'M',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
                                   0x00, 0x00,  240, 0x00, 0x00, 0x00,  160, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  char ss_filename[MAX_FILE];
  char timestamp[MAX_FILE];
  char save_ss_path[MAX_PATH];
  ScePspDateTime current_time;
  u8 *rgb_data = (u8 *)UNIVERSAL_VRAM_ADDR;
  u8 x,y;
  u16 col;
  u8 r,g,b;

  sceRtcGetCurrentClockLocalTime(&current_time);

  change_ext(gamepak_filename, ss_filename, "_");

  get_timestamp_string(timestamp, MSG_SS_FMT_0, current_time.year, current_time.month , current_time.day, 7,
    current_time.hour, current_time.minute, current_time.second, current_time.microsecond);

  path_printf(save_ss_path, sizeof(save_ss_path), "%s/%s%s.bmp", g_default_ss_dir, ss_filename, timestamp);

  for(y = 0; y < 160; y++)
  {
    for(x = 0; x < 240; x++)
    {
      col = image[x + y * 240];
      r = (col >> 10) & 0x1F;
      g = (col >> 5) & 0x1F;
      b = (col) & 0x1F;

      rgb_data[(159-y)*240*3+x*3+2] = b * 255 / 31;
      rgb_data[(159-y)*240*3+x*3+1] = g * 255 / 31;
      rgb_data[(159-y)*240*3+x*3+0] = r * 255 / 31;
    }
  }

    FILE *ss = fopen( save_ss_path, "wb" );
    if( ss == NULL ) return;

    fwrite( header, sizeof(header), 1, ss );
    fwrite( rgb_data, 240*160*3, 1, ss);
    fclose( ss );
}

void _flush_cache()
{
//    sceKernelDcacheWritebackAll();
    invalidate_all_cache();
}
