/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2008-2026
 *
 * filelist.h
 *
 * Contains a list of all of the files stored in the images/, fonts/, and
 * sounds/ folders
 ***************************************************************************/

#ifndef _FILELIST_H_
#define _FILELIST_H_

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
#include "sv_lang.h"

// Sounds
#ifndef HW_DOL
// exclude background music on GameCube
#include "bg_music_ogg.h"
#include "enter_ogg.h"
#include "exit_ogg.h"
#endif
#include "button_over_pcm.h"
#include "button_click_pcm.h"

// Graphics
#include "logo_png.h"
#include "logo_over_png.h"
#include "icon_settings_png.h"
#include "icon_home_png.h"
#include "icon_credits_png.h"
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
#include "icon_settings_ds4_png.h"
#include "icon_settings_snescontroller_png.h"
#include "icon_settings_superscope_png.h"
#include "icon_settings_justifier_png.h"
#include "icon_settings_mouse_png.h"
#include "icon_settings_file_png.h"
#include "icon_settings_mappings_png.h"
#include "icon_settings_menu_png.h"
#include "icon_settings_network_png.h"
#include "icon_settings_video_png.h"
#include "icon_settings_emulation_png.h"
#include "icon_settings_screenshot_png.h"
#include "icon_folder_png.h"
#include "icon_sd_png.h"
#include "icon_usb_png.h"
#include "icon_dvd_png.h"
#include "icon_smb_png.h"
#include "controller_gamepad_png.h"
#include "controller_wiimote_png.h"
#include "controller_nunchuk_png.h"
#include "controller_classic_png.h"
#include "controller_wiiupro_png.h"
#include "controller_gamecube_png.h"
#include "controller_ds4_png.h"
#include "controller_usb_png.h"
#include "controller_xbox360_png.h"
#include "controller_charging_png.h"
#include "default_preview_png.h"
#include "player1_point_png.h"
#include "player2_point_png.h"
#include "player3_point_png.h"
#include "player4_point_png.h"

#endif
