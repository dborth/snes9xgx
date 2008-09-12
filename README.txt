­———————————————————————————————————————————————————————————————————————————­
:::::::::::::::×::::::::::::   .______   :::::::::::::::::::   _   ::::::::::
|        _________            /   ___°/           -------.    (_)'\ /     `°|
×       /______ °   ---__---./   /___ _________  /  ---  /    __| / \      °²
×      _______\ \ /  ___  //  /____//\_____ °  /---/   / ___    ---         ×
|     °________/ /  / /  //  /__    _______\ \    /   /  \  \  / /        .||
::::::::::::::::/   /::--/_______\::.________/::::/   /:­::\   _  \::::::×:::
:::::::°:::::::/___\:::::::::::::::::::::::::::::/   /::::/__/   \--::­::::::
°:::::::::::::::::×:::::::::::::::°::::×:::::::::\--/::::::::::::::::::×:::::
­———————————————————————————————————————————————————————————————————————•ßrK•

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                          SNES9X GX                            ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

SNES9x GX is a Super Nintendo emulator for the Wii based on the PC emulator 
SNES9x 1.51 (http://snes9x.ipherswipsite.com/). SoftDev is responsible for 
the original SNES9x 1.50 GameCube port, whose work was continued by crunchy2. 
It was updated for the Wii by michniewski and is currently being maintained 
by michniewski and Tantric.

[What's New 005]

michniewski
- added: Superscope/mouse/justifier support, with Wii remote

Tantric
- added: now uses SNES 1.51 core (thanks to eke-eke for help with this)
- added: cheats menu! Loads .CHT file from /snes9x/cheats folder, 
         .CHT file name must match file name of ROM
- added: load/save preference selector. ROM, SRAM, Freeze, and preferences 
         are saved/loaded according to these
- added: preliminary Windows file share loading/saving (SMB) support on Wii:
		 You can input your network settings into snes9xGX.xml, or edit 
		 s9xconfig.cpp from the source code and compile.
- added: 'Auto' settings for save/load - attempts to automatically determine
         your load/save device(s) - SD, USB, Memory Card, DVD, SMB
- added: ROM Information page
- added: Game Menu - all game-specific options are here now: 
         SRAM save/load, Snapshot save/load, game reload, etc
- added: Credits page
- fixed: sd gecko works now
- fixed: full USB support
- changed: menu structure
- changed: preferences are now loaded and saved in XML format. You can open
		   snes9xGX.xml edit all settings, including some not available within
		   the program
- changed: if Home button is pressed when a game is running, Game Menu pops up
- changed: if preferences can't be loaded at the start and/or are reset, 
           preferences menu pops up - remove to save your preferences!
- changed: SRAM load - game reloaded automatically after loading SRAM

[What Was New 004]

- added: option to disable AA filtering 
         (snes graphics 'crisper', AA now default OFF)
- added: mapped zooming and turbo mode to classic controller
- added: preliminary usb support (loading)
- changed: sram and freezes now saved by filename, not internal romname. 
           If you have multiple versions of the same game, you can now have 
           srams and freezes for each version. A prompt to convert to the 
           new naming is provided for sram only.
- changed: by default, autoload/save sram and freeze enabled

[What Was New 003]
- added: alphabetical file sorting
- added: background logo/backdrop + nicer menus
- added: scrolling in ROM selector
- fixed: switching between pal/ntsc ROMS doesn't mess up timings
- fixed: GC controller config works now
- fixed: freeze autoloading on ROM load
- fixed: zipped ROMS should now load in a reasonable time
- fixed: precompiled dols for autosaving to various locations (see readme)
- changed: GC default quickload slot (to sd) (thanks kerframil)
- changed: default load/save dirs are now "/snes9x/roms" and 
           "/snes9x/saves/"  (thanks kerframil)
- changed: Classic X and Y defaults aren't switched
- changed: if autosave is enabled, it doesn't ask to save SRAM 
           anymore. It is saved in the background.
- updated README

[Whats Was New 002]
- added: classic and nunchuk support
- added: all controllers can now be configured
- added: GC version (untested)
- changed: mappings are no longer stored in SRAM, but in config file. 
           This means no per-game configurations, but one global 
           config per controller.
- one makefile to make all versions. (thanks to snes9x143 SVN)


[What Was New 001]
- compiles with latest devkitppc (r15)
- now uses libfat (can use front sd slot on wii)
- updated menu items a bit
- wiimote support
- fixed: autoload sram/freeze
- fixed: rom plays immediately after loading

[older update history at the bottom]


×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                            FEATURES                           ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'
- Based on Snes9x 1.5 - superior ROM compatibility
- Wiimote, Nunchuk, Classic, and Gamecube controller support
- SNES Superscope, Mouse, Justifier support
- Cheat support
- Auto Load/Save Game Snapshots and SRAM
- Custom controller configurations
- SD, USB, DVD, SMB, GC Memory Card, and Zip support
- Autodetect PAL/NTSC, 16:9 widescreen support
- Open Source!

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                         SETUP & INSTALLATION                  ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

Unzip the archive. You will find the following folders inside:

apps			Contains Homebrew Channel ready files
				(see Homebrew Channel instructions below)
				
executables		Contains Gamecube / Wii DOL files
				(for loading from other methods)
				
snes9x			Contains the directory structure required for storing
				roms, saves, and cheats (see below)

----------------------------
Directory Structure Setup
----------------------------

By default, roms are loaded from "snes9x/roms/", saves and preferences are 
stored in "snes9x/saves/", and cheats are loaded from "/snes9x/cheats/".
Therefore you should have the following folder structure at the root
of your load device (SD/USB/SMB):

  snes9x/
       roms/
       saves/
       cheats/
       
----------------------------
ROMS, Preferences, Saves, and Cheats:
----------------------------
Your SNES rom images must be in the Super Magicom (SMC) or FIG format. Generally,
all images you find will be in this format, but if you run across one that isn't
please download RTOOL which will allow you to convert the image into SMC format.
Cheats must be placed in the cheats folder and named identically to the ROM name,
except with a CHT extension.

    Wii
----------
On the Wii, you can load roms from SD card (Front SD or SD Gecko), USB, DVD,
or SMB share. Note that if you are using the Homebrew Channel, to load from 
USB, DVD, or SMB you will first have to load Snes9xGx from SD, and then set 
your load method preference. To load roms from a Windows network share (SMB) 
you will have to edit snes9xGX.xml on your SD card with your network settings, 
or edit s9xconfig.cpp from the source code and compile. If you edit and compile 
the source, you can use wiiload and the Homebrew Channel to load and play 
Snes9xGx completely over the network, without needing an SD card.


  Gamecube
------------
You can load roms from DVD or SD card. If you create a bootable 
DVD of Snes9xGx you can put roms on the same DVD. You may save preferences and
game data to SD or Memory Card.

------------------------------
Loading / Running the Emulator:
------------------------------

Wii - Via Homebrew Channel:
--------------------
The most popular method of running homebrew on the Wii is through the Homebrew
Channel. If you already have the channel installed, just copy over the apps folder
included in the archive into the root of your SD card.

Remember to also create the snes9x directory structure required. See above.

If you haven't installed the Homebrew Channel yet, read about how to here:
http://hbc.hackmii.com/

Gamecube:
---------
You can load Snes9xGX via sdload and an SD card in slot A, or by streaming 
it to your Gamecube, or by booting a bootable DVD with Snes9xGX on it. 
This document doesn't cover how to do any of that. A good source for information 
on these topics is the tehskeen forums: http://www.tehskeen.com/forums/

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                         ABOUT SNES9X                          ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

Welcome to the revolution in Wii/GameCube emulators! SNES9X is by far the most
complete and accurate Super Nintendo Entertainment System emulator to date.
Taking full power of the ATi-GX chipset and packed full of features that are
to die for after playing your games you'll never want to come back to
reality.

SNES9X is a very popular open source emulator mainly for the PC platform, but
has seen many ports to other consoles such as the Nintendo DS, Microsoft XBOX
,
and, thanks to SoftDev, the Nintendo GameCube. SoftDev's 1.50 port is not based 
on any previous SNES emulators that have existed for the GameCube. 

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                   DEFAULT CONTROLLER MAPPING                  ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

NOTE: You can change the controller configurations to your liking in the menu
under the Config Controllers option.

Below are the default button mappings for supported controllers. The wiimote
configuration allows you to play with the wiimote held sideways.

Wiimote		SNES					Nunchuk		SNES
---------------------				---------------------
  1			  Y						  Z			  Y
  2			  B						  B			  B
  A			  A						  A			  A
  B			  X						  C			  X
  -			SELECT					  2			SELECT
  +			START					  1			START
HOME	    Emulator menu			HOME		Emulator menu
			 LT						  -			 LT
			 RT						  +			 RT




Classic		SNES					GC PAD		SNES
---------------------				---------------------
  X			  X						  Y			  Y
  B			  B						  B			  B
  A			  A						  A			  A
  Y			  Y						  X			  X
  -			SELECT					  Z			SELECT
  +			START					 START		START
HOME	    Emulator menu			
 LT			 LT						  LT		 LT
 RT			 RT						  RT		 RT



×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                             ZIP SUPPORT                       ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

The Zip support in the emulator is courtesy of the zlib library. Currently,
it supports only the Inflate method.

The good news is that this is by far the most common zip method!

You should note also that this implementation assumes that the first file
in each archive is the required rom in smc/fig format.

In an attempt to save you time, we recommend using 7-Zip as the compressor,
as it gives a gain of 5-25% better compression over standard zips. This being
said, you can very well use WinZip or any other zip utility to create your zipped
ROMs.

To use 7-Zip compression on either linux or windows, use the following command:

  7za a -tzip -mx=9 myrom.zip myrom.smc

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                              MAIN MENU                        ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

Once the DOL file is loaded you will be presented with main menu where you can
load a ROM, set options for the emulator, set the joypad configuration, etc.
After loading a game the game will start running immediately. If you have the
auto-load SRAM option enabled it will automatically load SRAM (if it exists)
before starting play. 

You can return to the menu at any time by pressing the Home button 
(Wiimote and Classic Controller) or on the c-stick (the yellow control stick) 
to the left, or by pressing L+R+X+Y (GameCube Controller).
Return to the game by selecting "Resume Game".

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                             TURBO MODE                        ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

TurboMode increases the playback speed of the game by about 2x. To use TurboMode
simply press the c-stick (yellow control stick) to the right and hold it right
as long as you want playback to be double-speed. Release the c-stick when you
want normal playback speed to resume.

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                   IMPORTING AND EXPORTING SRAM                ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

Snes9xGx now includes the ability to load SRAM from Snes9x on other
platforms (Mac/PC/Linux/Etc) and to save back to those platforms. 

To load a SRAM file on the Wii or Gamecube from another platform, ensure the
name of the SRM file matches the filename of the ROM (except with an SRM 
extension).

To use a Wii/GameCube SRAM file on another platform just do the opposite: 
copy the saved SRAM file to the other platform. You may have to rename the 
file to be what that version of snes9x expects it to be.

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                      UPDATE HISTORY (old)                     ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

[What Was New 2.0.1b8]
* Added: SD slot B options for freezes, sram and loading of roms
* Changed: SMB options no longer displayed in menus when run on a Wii
* Changed: Game auto resumes running after resetting when choosing the "Reset
    Game" menu option
* Fixed (maybe): Reading of DVDs past the 1.36 GB barrier (Wii only) please
    test! - svpe

[What Was New 2.0.1b7]
* Fixed: Zip compressed freezes to memory card could become corrupted as the
    file size changed - fixed this by deleting the existing file before writing
    a new one if the file size increased. If the file got smaller or stayed the
    same the original file is updated, using less of the existing file if the
    actual file size is smaller. A check is made before deleting the existing
    freeze file to ensure that there is enough space available for the new
    file. Note that additional space equivalent to the new file size must be
    available. If not enough space is available the original freeze is retained
    and the user is informed of the lack of space.
* Fixed: If option to auto-load freeze was selected, joypad config would not
    be restored since that is stored in SRAM. Resolved this for now by first
    loading SRAM if any and then loading the freeze. Obviously having to have
    both SRAM and freeze is not ideal, but this gets the job done if you have
    enough space on your memory card, SD card, etc.
* Added prompt when returning to the menu with autosave enabled allowing the
    user choose whether or not to perform the save. Press A to save or B if you
    don't wish to save.
* Added optional verification of Gamecube memory card saves. After writing
    the file it reads it back in and verifies that the written file matches
    what was to be saved. If it doesn't or if there was a problem opening the
    file it reports the problem to the user. Set this option in the preferences
    if desired.
* Added Reset Gamecube/Wii menu item
* Experimental DVD support for reading beyond 1.36 GB barrier on Wii. I have
    no way to test this, so please report on whether or not it works! Based on
    svpe's code.
    
NOTE: due to changes in the settings, this version will reset your emulator
options settings, so if you had saved preferences you will need to make your
changes to the emulator settings again and save them.

[What Was New 2.0.1b6a]
* Fixed: Going up a directory when selecting a rom on a DVD wasn't working
    
[What's Was New 2.0.1b6]
* PAL Wii support - no separate version necessary! - eke-eke
* PAL roms now play at correct speed via internal timer, ntsc roms still use
    more accurate VSYNC timing - eke-eke
* Zipped freezes to memory card - take 9-12 blocks or so - based on denman's
    code
* Added option for auto save and load of freezes. For saving, can do both SRAM
    and Freeze if desired
* Memory card saving and loading shows a progress bar
* More miscellaneous ease-of-use improvements and cleanup
* Fixed: pressing B to get out of a rom file selection screen no longer drops
    you all the way back to the main menu. Now goes back to choice of where to
    load ROM (the "Load from DVD", "Load from SMB"... screen)
* Fixed: loading of joypad configuration in SRAM works again - no longer gets
    messed up

[ What Was New in 2.0.1b5]
* B button implemented in all menus (returns to previous menu)
* Fixed bug when freezing state to SD card - would crash if SD support was not
    previously initialized
* Fixed double A button press needed after manual prefs/sram save to memory card
* Fixed delay after pressing A button after saving freeze to SD card
* Fixed problem of ".srm" SRAM file being created when SRAM was saved with no
    ROM loaded
* Fixed version number in SRAM and preferences
* Minor other code revisions

[ What Was New 2.0.1b1 through 2.0.1b4]
* SRAM saving and loading from snes9x on other platforms via SD card or SMB
* Games now autostart once loaded
* After manually loading SRAM the emulator is automatically reset
* Optional auto-loading of SRAM from memory card, SD or SMB after game loads
* Optional auto-saving of SRAM to memory card, SD or SMB when returning to menu
* TurboMode
* Global emulator preferences
* Menus redesigned (hopefully for the better!)
* Comes in 6 variants, each auto-loading/saving preferences/sram to a different
  location: mcslota, mcslotb, sdslota, sdslotb, smb, and noload
* ROM injector works again
* A number of small improvements and bug fixes
  
[ What Was New in 2.0 WIP6 ]
* Port of SNES9X 1.50
* SMB support
* SD support
* Greatly improved emulation and timing for NTSC roms
* Save states (freezes) to SD and SMB
* Screen zoom
* Improved font and display
* ROM injector
* ... and more ...

[ Features - OLD 1.43 LIST! ]
* Port of SNES9X v1.43
* Fullscreen Graphics
* Sound Emulation
* SRAM Manager
* DVD Loading
* 1-4 Player Support
* Mode 7 Supported
* Super FX Supported
* SDD1, SRTC, SA-1 Supported
* DSP1 & DSP2 Supported
* Partial DSP4 Support
* Supports Hi-Res 512x224 screens
* Joliet Browser
* PAD Configuration saved with SRAM
* Memcard save/load time extended
* Partial Zip support
* Crude Timer
* Sound Sync Option
* Analog Clip
* XenoGC Support (GC-Linux Homebrew DVD Compatibility)


×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                               CREDITS                         ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

                             Technical Credits

                       Snes9x v1.5.0/1.4.3 - Snes9x Team
                 GameCube Port 2.0 WIP6 and earlier - SoftDev
                 Additional improvements to 2.0 WIP6 - eke-eke
                   GameCube 2.0.1bx enhancements - crunchy2
                         v00x updates - michniewski & Tantric
                        GX - http://www.gc-linux.org
                        libogc - Shagkur & wintermute


                                 Testing
                     crunchy2 / tehskeen users / others


                              Documentation
                      brakken, crunchy2, michniewski, Tantric

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                                                               ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'