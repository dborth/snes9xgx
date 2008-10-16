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
|0O×øo·                        SNES9XGX v2.0.1b8                      ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

*******************************************************************************
PLEASE NOTE: THIS DOCUMENT IS A WORK IN PROGRESS - IT IS INCOMPLETE AND SOME OF
THE INFORMATION IS OUT OF DATE
*******************************************************************************

Welcome to the revolution in GameCube emulators! SNES9X is by far the most
complete and accurate Super Nintendo Entertainment System emulator to date.
Taking full power of the ATi-GX chipset and packed full of features that are
to die for after playing your games you'll never want to come back to
reality.

SNES9X is a very popular open source emulator mainly for the PC platform, but
has seen many ports to other consoles such as the Nintendo DS, Microsoft XBOX
and now thanks to SoftDev the Nintendo GameCube! This is a straight port and
is not based on any previous SNES emulators that have existed for the
GameCube. You can get more information on SNES9X here from the below URL.
http://snes9x.ipherswipsite.com/

[What's New 002]
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
|0O×øo·                         SETUP & INSTALLATION                  ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

Unzip the archive into a folder. You'll find 6 variations of the dol, along
with an "inject" tool for (optionally) injecting a rom into a dol. The six
variants auto save and load preferences and sram to/from different locations
as follows (you can also manually save and load SRAM to any location):

  filename                      preferences/sram autoloading location
  -------------------------     -------------------------------------
  snes9xGx201b8-mcslota.dol     Memory card in slot A
  snes9xGx201b8-mcslotb.dol     Memory card in slot B
  snes9xGx201b8-sdslota.dol     SD card in SD adapter in slot A
  snes9xGx201b8-sdslotb.dol     SD card in SD adapter in slot B
  snes9xGx201b8-smb.dol         SMB share (see SMB section below)
  snes9xGx201b8-noload.dol      none - doesn't load prefs nor autosave SRAM
  

Your SNES rom images must be in the Super Magicom (SMC) or FIG format. Generally,
all images you find will be in this format, but if you run across one that isn't
please download RTOOL which will allow you to convert the image into SMC format.

You can load roms from DVD, SD card or SMB share. If you wish to use an SD card
or SMB share, you must create an SNESROMS and an SNESSAVE folder at the top
level (root) of the card or share. Put your roms in the SNESROMS folder. On DVD
you can either place your roms at the top level, or optionally you may have an
SNESROMS folder at the top level of the DVD, in which case the game selector
will default to showing that folder when first entered. If you create a bootable
DVD of Snes9xGx you can put roms on the same DVD.

Now that you have that set up all you need to do is load the dol of your choice.
You can load it via sdload and an SD card in slot A, or by streaming it to your
cube, or by booting a bootable DVD with it on it. This document doesn't cover
how to do any of that. A good source for information on these topics is the
tehskeen forums:

    http://www.tehskeen.com/forums/
×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                   DEFAULT CONTROLLER MAPPING                  ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

Wiimote		SNES
---------------------
  1		  Y
  2		  B
  A		  A
  B		  X
  -		SELECT
  +		START
HOME	    Emulator menu
		 LT
		 RT

This configuration allows you to play with the wiimote held sideways.

Nunchuk		SNES
---------------------
  Z		  Y
  B		  B
  A		  A
  C		  X
  2		SELECT
  1		START
HOME	    Emulator menu
  -		 LT
  +		 RT

Classic		SNES
---------------------
  X		  Y
  B		  B
  A		  A
  Y		  X
  -		SELECT
  +		START
HOME	    Emulator menu
 LT		 LT
 RT		 RT

GC PAD		SNES
---------------------
  Y		  Y
  B		  B
  A		  A
  X		  X
  Z		SELECT
 START		START
  LT		 LT
  RT		 RT


×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                        PARTIAL PKZIP SUPPORT                  ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

The Zip support in the emulator is courtesy of the zlib library. Currently,
it supports only the Inflate method.

The good news is that this is by far the most common zip method!

You should note also that this implementation assumes that the first file
in each archive is the required rom in smc/fig format.

In an attempt to save you time, we recommend using 7-Zip as the compressor,
as it gives a gain of 5-25% better compression over standard zips.

To use 7-Zip compression on either linux or windows, use the following command:

  7za a -tzip -mx=9 myrom.zip myrom.smc

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                              MAIN MENU                        ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

Once the DOL file is loaded you will be presented with main menu where you can
load a rom from DVD, SD or SMB, set options for the emulator, set the joypad
configuration (on a per-game basis), save or load freezes, manage SRAM, etc.
After loading a game the game will start running immediately. If you have the
auto-load SRAM option enabled it will automatically load SRAM (if it exists)
before starting play. You can return to the main menu at any time by pressing
the c-stick (the yellow control stick) to the left, or by pressing L+R+X+Y.
Return to the game by selecting "Resume Game" or by pressing the B button until
play resumes.

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

Snes9xGx 2.0.1 now includes the ability to load SRAM from Snes9x on other
platforms (Mac/PC/Linux/Etc) and to save back to those platforms. To use this
feature simply save or load SRAM to/from SD card or an SMB share.

The only thing to be aware of is that Snes9xGx requires that the SRM file have a
name that is the rom name (not necessarily the same as the file name!) with .SRM
at the end. For example, the game "Super Mario All-Stars + World" has a rom name
of "ALL_STARS + WORLD", so the SRAM file should have the name "ALL_STARS +
WORLD.srm". You can see the rom name for a game by loading the game - the rom
name is shown in the information that is briefly shown at the bottom of the
screen. A perhaps easier way to find the correct name is simply to load the game
and then save SRAM to SD or SMB and look at the filename that Snes9xGx uses.
That's the name you should use when importing an SRAM from another platform.

To use an Snes9xGx SRAM on another platform just do the opposite: save SRAM to
SD or SMB, and then copy that saved SRAM file to the other platform. You may
have to rename the file to be what that version of snes9x expects it to be.

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                         INJECTING A ROM                       ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

Injecting a rom is not required to use snes9xGx, but if you primarily use
snes9xGx by streaming it to your cube you may wish to inject a rom into the dol.
In that case the game will start playing immediately once Snes9xGx loads. If you
have the auto-load SRAM option enabled it will load SRAM before starting the
game as well.

To inject a rom you use the "inject.exe" tool on Windows or the "inject" tool on
Mac OS X. This tool works at the command line and has syntax as follows:

     inject original.dol gamerom.smc output.dol
   
On the Mac you will either need to copy the inject tool into a location that is
in your "PATH" or just go into the directory that has the inject tool in it and
proceed the command with "./" like this:

   ./inject original.dol gamerom.smc output.dol
   
Once you run the tool on your dol, just stream the outputted dol to your cube,
or otherwise load it and Snes9xGx will load and the game will start running.

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                  WAVEBIRD WIRELESS CONTROLLER                 ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

This note applies to all homebrew GC software. The Wavebird wireless controller
CAN be used with homebrew, including Snes9xGx, but to enable it you must press a
button on the controller when you power up the GC and see the Gamecube logo.
This will initialize the controller and allow it to function in Snes9xGx and
other homebrew software.

You must do this each time you power up your Gamecube. Also, if you unplug the
wireless receiver while Snes9xGx is running and plug it back in, it will not
work - you will then have to plug in a wired controller to continue play. Until
someone figures out how to properly handle the Wavebird, this will continue to
be the case. My ears are open!

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                               SMB                             ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

TBD

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                               CREDITS                         ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

                             Technical Credits

                       Snes9x v1.5.0/1.4.3 - Snes9x Team
                 GameCube Port 2.0 WIP6 and earlier - SoftDev
                 Additional improvements to 2.0 WIP6 - eke-eke
                   GameCube 2.0.1bx enhancements - crunchy2
                         v001 updates - michniewski
                        GX - http://www.gc-linux.org
                             Font - Qoob Team
                             libogc - Shagkur


                                 Testing
                     crunchy2 / tehskeen users / others


                              Documentation
                 original by brakken/updated by crunchy2


×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                        SNES9XGX v2.0.1b8                      ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'
