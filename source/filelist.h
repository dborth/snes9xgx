/****************************************************************************
 * Snes9x Nintendo Wii/Gamecube Port
 *
 * Tantric 2008-2023
 *
 * filelist.h
 *
 * Contains a list of all of the files stored in the images/, fonts/, and
 * sounds/ folders
 ***************************************************************************/

#ifndef _FILELIST_H_
#define _FILELIST_H_

#include <gccore.h>

// Fonts
#include "font_ttf.h"

// Languages
#include "jp_lang.h"
#include "en_lang.h"
#include "de_lang.h"
#include "fr_lang.h"
#include "es_lang.h"
#include "it_lang.h"
#include "nl_lang.h"
#include "zh_lang.h"
#include "ko_lang.h"
#include "pt_lang.h"
#include "pt_br_lang.h"
#include "ca_lang.h"
#include "tr_lang.h"

// Sounds
#ifdef HW_RVL
// background music is Wii-only
#include "bg_music_ogg.h"
#include "enter_ogg.h"
#include "exit_ogg.h"
#endif
#include "button_over_pcm.h"
#include "button_click_pcm.h"

// Graphics
#include "logo_png.h"
#include "logo_over_png.h"
#include "bg_top_png.h"
#include "bg_bottom_png.h"
#include "icon_settings_png.h"
#include "icon_home_png.h"
#include "icon_game_settings_png.h"
#include "icon_game_cheats_png.h"
#include "icon_game_controllers_png.h"
#include "icon_game_load_png.h"
#include "icon_game_save_png.h"
#include "icon_game_delete_png.h"
#include "icon_game_reset_png.h"
#include "icon_settings_wiimote_png.h"
#include "icon_settings_classic_png.h"
#include "icon_settings_gamecube_png.h"
#include "icon_settings_nunchuk_png.h"
#include "icon_settings_wiiupro_png.h"
#include "icon_settings_drc_png.h"
#include "icon_settings_snescontroller_png.h"
#include "icon_settings_superscope_png.h"
#include "icon_settings_justifier_png.h"
#include "icon_settings_mouse_png.h"
#include "icon_settings_file_png.h"
#include "icon_settings_mappings_png.h"
#include "icon_settings_menu_png.h"
#include "icon_settings_network_png.h"
#include "icon_settings_video_png.h"
#include "icon_settings_audio_png.h"
#include "icon_settings_screenshot_png.h"
#include "button_png.h"
#include "button_over_png.h"
#include "button_prompt_png.h"
#include "button_prompt_over_png.h"
#include "button_long_png.h"
#include "button_long_over_png.h"
#include "button_short_png.h"
#include "button_short_over_png.h"
#include "button_small_png.h"
#include "button_small_over_png.h"
#include "button_large_png.h"
#include "button_large_over_png.h"
#include "button_arrow_left_png.h"
#include "button_arrow_right_png.h"
#include "button_arrow_up_png.h"
#include "button_arrow_down_png.h"
#include "button_arrow_left_over_png.h"
#include "button_arrow_right_over_png.h"
#include "button_arrow_up_over_png.h"
#include "button_arrow_down_over_png.h"
#include "button_gamesave_png.h"
#include "button_gamesave_over_png.h"
#include "button_gamesave_blank_png.h"
#include "screen_position_png.h"
#include "dialogue_box_png.h"
#include "credits_box_png.h"
#include "progressbar_png.h"
#include "progressbar_empty_png.h"
#include "progressbar_outline_png.h"
#include "throbber_png.h"
#include "icon_folder_png.h"
#include "icon_sd_png.h"
#include "icon_usb_png.h"
#include "icon_dvd_png.h"
#include "icon_smb_png.h"
#include "battery_png.h"
#include "battery_red_png.h"
#include "battery_bar_png.h"
#include "bg_options_png.h"
#include "bg_options_entry_png.h"
#include "bg_game_selection_png.h"
#include "bg_game_selection_entry_png.h"
#include "bg_preview_png.h"
#include "scrollbar_png.h"
#include "scrollbar_arrowup_png.h"
#include "scrollbar_arrowup_over_png.h"
#include "scrollbar_arrowdown_png.h"
#include "scrollbar_arrowdown_over_png.h"
#include "scrollbar_box_png.h"
#include "scrollbar_box_over_png.h"
#include "keyboard_textbox_png.h"
#include "keyboard_key_png.h"
#include "keyboard_key_over_png.h"
#include "keyboard_mediumkey_png.h"
#include "keyboard_mediumkey_over_png.h"
#include "keyboard_largekey_png.h"
#include "keyboard_largekey_over_png.h"
#include "player1_point_png.h"
#include "player2_point_png.h"
#include "player3_point_png.h"
#include "player4_point_png.h"
#include "player1_grab_png.h"
#include "player2_grab_png.h"
#include "player3_grab_png.h"
#include "player4_grab_png.h"

#endif
