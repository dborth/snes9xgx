/**********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com) and
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  Brad Jorsch (anomie@users.sourceforge.net),
                             funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com),
                             Nach (n-a-c-h@users.sourceforge.net), and
                             zones (kasumitokoduck@yahoo.com)

  BS-X C emulator code
  (c) Copyright 2005 - 2006  Dreamer Nom,
                             zones

  C4 x86 assembler and some C emulation code
  (c) Copyright 2000 - 2003  _Demo_ (_demo_@zsnes.com),
                             Nach,
                             zsKnight (zsknight@zsnes.com)

  C4 C++ code
  (c) Copyright 2003 - 2006  Brad Jorsch,
                             Nach

  DSP-1 emulator code
  (c) Copyright 1998 - 2006  _Demo_,
                             Andreas Naive (andreasnaive@gmail.com)
                             Gary Henderson,
                             Ivar (ivar@snes9x.com),
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora,
                             Nach,
                             neviksti (neviksti@hotmail.com)

  DSP-2 emulator code
  (c) Copyright 2003         John Weidman,
                             Kris Bleakley,
                             Lord Nightmare (lord_nightmare@users.sourceforge.net),
                             Matthew Kendora,
                             neviksti


  DSP-3 emulator code
  (c) Copyright 2003 - 2006  John Weidman,
                             Kris Bleakley,
                             Lancer,
                             z80 gaiden

  DSP-4 emulator code
  (c) Copyright 2004 - 2006  Dreamer Nom,
                             John Weidman,
                             Kris Bleakley,
                             Nach,
                             z80 gaiden

  OBC1 emulator code
  (c) Copyright 2001 - 2004  zsKnight,
                             pagefault (pagefault@zsnes.com),
                             Kris Bleakley,
                             Ported from x86 assembler to C by sanmaiwashi

  SPC7110 and RTC C++ emulator code
  (c) Copyright 2002         Matthew Kendora with research by
                             zsKnight,
                             John Weidman,
                             Dark Force

  S-DD1 C emulator code
  (c) Copyright 2003         Brad Jorsch with research by
                             Andreas Naive,
                             John Weidman

  S-RTC C emulator code
  (c) Copyright 2001-2006    byuu,
                             John Weidman

  ST010 C++ emulator code
  (c) Copyright 2003         Feather,
                             John Weidman,
                             Kris Bleakley,
                             Matthew Kendora

  Super FX x86 assembler emulator code
  (c) Copyright 1998 - 2003  _Demo_,
                             pagefault,
                             zsKnight,

  Super FX C emulator code
  (c) Copyright 1997 - 1999  Ivar,
                             Gary Henderson,
                             John Weidman

  Sound DSP emulator code is derived from SNEeSe and OpenSPC:
  (c) Copyright 1998 - 2003  Brad Martin
  (c) Copyright 1998 - 2006  Charles Bilyue'

  SH assembler code partly based on x86 assembler code
  (c) Copyright 2002 - 2004  Marcus Comstedt (marcus@mc.pp.se)

  2xSaI filter
  (c) Copyright 1999 - 2001  Derek Liauw Kie Fa

  HQ2x filter
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Specific ports contains the works of other authors. See headers in
  individual files.

  Snes9x homepage: http://www.snes9x.com

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without 
  fee, providing that this license information and copyright notice appear 
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
**********************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "snes9x.h"
#include "memmap.h"
#include "display.h"

#ifndef NGC
#include "cheats.h"
#endif

#include "controls.h"

#ifndef NGC
#include "netplay.h"
#endif

#ifdef DEBUGGER
extern FILE *trace;
#endif

static char *rom_filename=NULL;

void S9xUsage ()
{
    S9xMessage (S9X_INFO, S9X_USAGE, "snes9x: S9xUsage: snes9x <options> <rom image filename>\n");
    S9xMessage (S9X_INFO, S9X_USAGE, "Where <options> can be:");
    S9xMessage(S9X_INFO, S9X_USAGE, "");

    /* SOUND OPTIONS */
    S9xMessage(S9X_INFO, S9X_USAGE, "-sound or -so                   Enable digital sound output");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nosound or -ns                 Disable digital sound output");
    S9xMessage(S9X_INFO, S9X_USAGE, "-soundskip or -sk <0-3>         Sound CPU skip-waiting method");
#ifdef SHOW_OBSOLETE_OPTIONS
    S9xMessage(S9X_INFO, S9X_USAGE, "-ratio or -ra <num>             Ratio of 65c816 to SPC700 instructions (ignored)");
#endif
    S9xMessage(S9X_INFO, S9X_USAGE, "-soundquality, -sq, or -r <num> Set sound playback quality");
#ifdef __sgi
/* BS: changed the sample rate values to match the IRIX options */
    S9xMessage (S9X_INFO, S9X_USAGE, "\
                                  0 - off, 1 - 8192, 2 - 11025, 3 - 16000,\n\
                                  4 - 22050 (default), 5 - 32000, 6 - 44100,\n\
                                  7 - 48000");
#else
    S9xMessage (S9X_INFO, S9X_USAGE, "\
                                  0 - off, 1 - 8192, 2 - 11025, 3 - 16500,\n\
                                  4 - 22050 (default), 5 - 29300, 6 - 36600,\n\
                                  7 - 44000");
#endif
    S9xMessage(S9X_INFO, S9X_USAGE, "-altsampledecode or -alt        Use alternate sample decoder");
    S9xMessage(S9X_INFO, S9X_USAGE, "-stereo or -st                  Enable stereo sound output (implies -sound)");
    S9xMessage(S9X_INFO, S9X_USAGE, "-mono                           Enable monaural sound output (implies -sound)");
    S9xMessage(S9X_INFO, S9X_USAGE, "-soundsync or -sy               Enable sound sync to CPU at startup");
    S9xMessage(S9X_INFO, S9X_USAGE, "-soundsync2 or -sy2             Alternate method to sync sound");
#ifdef USE_THREADS
    S9xMessage(S9X_INFO, S9X_USAGE, "-threadsound or -ts             Use a separate thread to output sound");
#endif
    S9xMessage(S9X_INFO, S9X_USAGE, "-interpolatedsound or -is <0-3> Select sound interpolation method\n\
                                  0=none, 1=linear, 2=cubic, 3=gaussian");
    S9xMessage(S9X_INFO, S9X_USAGE, "-echo or -e                     Enable DSP echo effects at startup");
    S9xMessage(S9X_INFO, S9X_USAGE, "-noecho or -ne                  Disable DSP echo effects at startup");
    S9xMessage(S9X_INFO, S9X_USAGE, "-envx or -ex                    Enable volume envelope height reading");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nosamplecaching, -nsc, or -nc  Disable sample caching");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nomastervolume or -nmv         Disable master volume setting");
    S9xMessage(S9X_INFO, S9X_USAGE, "-fix                            'Fix' sound frequencies");
    S9xMessage(S9X_INFO, S9X_USAGE, "");
    
    /* FEATURE OPTIONS */
    S9xMessage(S9X_INFO, S9X_USAGE, "-conf <filename>                Use specified conf file (after standard files)");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nostdconf                      Do not load the standard config files");
    S9xMessage(S9X_INFO, S9X_USAGE, "-hdma or -ha                    Enable HDMA emulation at startup");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nohdma or -nh                  Disable HDMA emulation at startup");
    S9xMessage(S9X_INFO, S9X_USAGE, "-transparency or -tr            Enable transparency effects");
    S9xMessage(S9X_INFO, S9X_USAGE, "-notransparency or -nt          Disable transparency effects at start");
    S9xMessage(S9X_INFO, S9X_USAGE, "-windows                        Enable graphic window effects");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nowindows or -nw               Disable graphic window effects");
#ifdef SHOW_OBSOLETE_OPTIONS
    S9xMessage(S9X_INFO, S9X_USAGE, "-layering or -L                 Enable BG Layering Hack at startup (ignored)");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nolayering or -nl              Disable BG Layering Hack at startup (ignored)");
#endif
    S9xMessage(S9X_INFO, S9X_USAGE, "-im7                            Enable Mode 7 interpolation effects");
    S9xMessage(S9X_INFO, S9X_USAGE, "-displayframerate or -dfr       Display the frame rate counter");
    S9xMessage(S9X_INFO, S9X_USAGE, "-aidoshm <shmid>                Run in AIDO mode, with specified SHM ID");
    S9xMessage(S9X_INFO, S9X_USAGE, "");

    /* DISPLAY OPTIONS */
    S9xMessage(S9X_INFO, S9X_USAGE, "-hires or -hi                   Enable support for hi-res and interlace modes");
#ifdef SHOW_OBSOLETE_OPTIONS
    S9xMessage(S9X_INFO, S9X_USAGE, "-16 or -sixteen                 Enable 16-bit rendering");
#endif
    S9xMessage(S9X_INFO, S9X_USAGE, "-frameskip or -f <num>          Screen update frame skip rate");
    S9xMessage(S9X_INFO, S9X_USAGE, "-frametime or -ft <float>       Milliseconds per frame for frameskip auto-adjust");
    S9xMessage(S9X_INFO, S9X_USAGE, "");

    /* ROM OPTIONS */
    S9xMessage(S9X_INFO, S9X_USAGE, "-hirom, -hr, or -fh             Force Hi-ROM memory map");
    S9xMessage(S9X_INFO, S9X_USAGE, "-lorom, -lr, or -fl             Force Lo-ROM memory map");
    S9xMessage(S9X_INFO, S9X_USAGE, "-bs                             Use BS Satellite System ROM mapping");
    S9xMessage(S9X_INFO, S9X_USAGE, "-bsxbootup                      Boot up BS games from BS-X");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nointerleave or -ni            ROM image is not in interleaved format");
    S9xMessage(S9X_INFO, S9X_USAGE, "-interleaved or -i              ROM image is in interleaved format");
    S9xMessage(S9X_INFO, S9X_USAGE, "-interleaved2 or -i2            ROM image is in interleaved 2 format");
    S9xMessage(S9X_INFO, S9X_USAGE, "-interleavedgd24 or -gd24       ROM image is in interleaved gd24 format");
    S9xMessage(S9X_INFO, S9X_USAGE, "-header, -he, or -hd            Force the detection of a ROM image header");
    S9xMessage(S9X_INFO, S9X_USAGE, "-noheader or -nhd               Force the detection of no ROM image header");
    S9xMessage(S9X_INFO, S9X_USAGE, "-ntsc or -n                     Force NTSC timing (60 frames/sec)");
    S9xMessage(S9X_INFO, S9X_USAGE, "-pal or -p                      Force PAL timing (50 frames/sec)");
    S9xMessage(S9X_INFO, S9X_USAGE, "-superfx or -sfx                Force detection of the SuperFX chip");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nosuperfx or -nosfx            Force detection of no SuperFX chip");
    S9xMessage(S9X_INFO, S9X_USAGE, "-dsp1                           Force detection of the DSP-1 chip");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nodsp1                         Force detection of no DSP-1 chip");
    S9xMessage(S9X_INFO, S9X_USAGE, "");

#ifdef DEBUGGER
    /* OPTIONS FOR DEBUGGING USE */
    S9xMessage(S9X_INFO, S9X_USAGE, "-cycles or -h <1-199>           Percentage of CPU cycles to execute per scanline");
    S9xMessage(S9X_INFO, S9X_USAGE, "-speedhacks or -sh              Enable speed hacks");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nospeedhacks or -nsh           Disable speed hacks");
    S9xMessage(S9X_INFO, S9X_USAGE, "-debug or -d                    Set the Debugger flag at startup");
    S9xMessage(S9X_INFO, S9X_USAGE, "-trace or -t                    Begin CPU instruction tracing at startup");
    S9xMessage(S9X_INFO, S9X_USAGE, "-noirq                          Disable processor IRQ (for debugging)");
    S9xMessage(S9X_INFO, S9X_USAGE, "--selftest                      Run self tests and exit");
#ifdef DEBUG_MAXCOUNT
    S9xMessage(S9X_INFO, S9X_USAGE, "-maxcount <N>                   Only run through N cycles of the main loop");
#endif
    S9xMessage(S9X_INFO, S9X_USAGE, "");
#endif

    /* PATCH/CHEAT OPTIONS */
    S9xMessage(S9X_INFO, S9X_USAGE, "-nopatch                        Do not apply any available IPS patches");
    S9xMessage(S9X_INFO, S9X_USAGE, "-cheat                          Apply saved cheats");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nocheat                        Do not apply saved cheats");
    S9xMessage(S9X_INFO, S9X_USAGE, "-gamegenie or -gg <code>        Supply a Game Genie code");
    S9xMessage(S9X_INFO, S9X_USAGE, "-actionreplay or -ar <code>     Supply a Pro-Action Reply code");
    S9xMessage(S9X_INFO, S9X_USAGE, "-goldfinger or -gf <code>       Supply a Gold Finger code");
    S9xMessage(S9X_INFO, S9X_USAGE, "");

    /* CONTROLLER OPTIONS */
    S9xMessage(S9X_INFO, S9X_USAGE, "-nomp5                          Disable emulation of the Multiplayer 5 adapter");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nomouse                        Disable emulation of the SNES mouse");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nosuperscope                   Disable emulation of the Superscope");
    S9xMessage(S9X_INFO, S9X_USAGE, "-nojustifier                    Disable emulation of the Konami Justifier");
    S9xMessage(S9X_INFO, S9X_USAGE, "-port# <control>                Specify which controller to emulate in port 1/2");
    S9xMessage(S9X_INFO, S9X_USAGE, "      Controllers: none            No controller");
    S9xMessage(S9X_INFO, S9X_USAGE, "                   pad#            Joypad number 1-8");
    S9xMessage(S9X_INFO, S9X_USAGE, "                   mouse#          Mouse number 1-2");
    S9xMessage(S9X_INFO, S9X_USAGE, "                   superscope      Superscope (not useful with -port1)");
    S9xMessage(S9X_INFO, S9X_USAGE, "                   justifier       Blue Justifier (not useful with -port1)");
    S9xMessage(S9X_INFO, S9X_USAGE, "                   one-justifier   ditto");
    S9xMessage(S9X_INFO, S9X_USAGE, "                   two-justifiers  Blue & Pink Justifiers");
    S9xMessage(S9X_INFO, S9X_USAGE, "                   mp5:####        MP5 with the 4 named pads (1-8 or n)");
    
    S9xMessage(S9X_INFO, S9X_USAGE, "");

#ifdef NETPLAY_SUPPORT
    /* NETPLAY OPTIONS */
    S9xMessage(S9X_INFO, S9X_USAGE, "-net                            Enable netplay");
    S9xMessage(S9X_INFO, S9X_USAGE, "-port or -po <num>              Use port <num> for netplay");
    S9xMessage(S9X_INFO, S9X_USAGE, "-server or -srv <string>        Use the specified server for netplay");
    S9xMessage(S9X_INFO, S9X_USAGE, "");
#endif

#ifdef STORM
    /* "STORM" OPTIONS */
    S9xMessage(S9X_INFO, S9X_USAGE, "-nosecondjoy                    Set secondjoy=0");
    S9xMessage(S9X_INFO, S9X_USAGE, "-showfps                        Set dofps=1");
    S9xMessage(S9X_INFO, S9X_USAGE, "-hicolor                        Set hicolor=1");
    S9xMessage(S9X_INFO, S9X_USAGE, "-minimal                        Turn off "Keyboard with exception of ESC"");
    S9xMessage(S9X_INFO, S9X_USAGE, "-ahiunit <num>                  Set AHI Unit to <num>");
    S9xMessage(S9X_INFO, S9X_USAGE, "");
#endif

#ifndef NGC
    S9xExtraUsage();
#endif

    S9xMessage (S9X_INFO, S9X_USAGE, "\
\nROM image needs to be in Super MagiCom (*.smc), Super FamiCom (*.sfc),\n\
*.fig, or split (*.1, *.2, or sf32527a, sf32527b, etc) format and can be\n\
compressed with zip, gzip, or compress.\n");

    exit (1);
}

#ifdef STORM
extern int dofps;
extern int hicolor;
extern int secondjoy;
extern int minimal;
int prelude=0;
extern int unit;
#endif

#ifndef NGC
static bool parse_controller_spec(int port, const char *arg){
    if(!strcasecmp(arg, "none")) {
        S9xSetController(port, CTL_NONE, 0, 0, 0, 0);
    } else if(!strncasecmp(arg, "pad", 3) &&
              arg[3]>='1' && arg[3]<='8' &&
              arg[4]=='\0'){
        S9xSetController(port, CTL_JOYPAD, arg[3]-'1', 0, 0, 0);
    } else if(!strncasecmp(arg, "mouse", 5) &&
              arg[5]>='1' && arg[5]<='2' &&
              arg[6]=='\0'){
        S9xSetController(port, CTL_MOUSE, arg[5]-'1', 0, 0, 0);
    } else if(!strcasecmp(arg, "superscope")) {
        S9xSetController(port, CTL_SUPERSCOPE, 0, 0, 0, 0);
    } else if(!strcasecmp(arg, "justifier") ||
              !strcasecmp(arg, "one-justifier")) {
        S9xSetController(port, CTL_JUSTIFIER, 0, 0, 0, 0);
    } else if(!strcasecmp(arg, "two-justifiers")) {
        S9xSetController(port, CTL_JUSTIFIER, 1, 0, 0, 0);
    } else if(!strncasecmp(arg, "mp5:", 4) &&
              ((arg[4]>='1' && arg[4]<='8') || arg[4]=='n') &&
              ((arg[5]>='1' && arg[5]<='8') || arg[5]=='n') &&
              ((arg[6]>='1' && arg[6]<='8') || arg[6]=='n') &&
              ((arg[7]>='1' && arg[7]<='8') || arg[7]=='n') &&
              arg[8]=='\0'){
        S9xSetController(port, CTL_MP5,
                         (arg[4]=='n')?-1:arg[4]-'1',
                         (arg[5]=='n')?-1:arg[5]-'1',
                         (arg[6]=='n')?-1:arg[6]-'1',
                         (arg[7]=='n')?-1:arg[7]-'1');
    } else {
        return false;
    }
    return true;
}

#endif

char *S9xParseArgs (char **argv, int argc)
{

#ifndef NGC
	// Nintendo Gamecube has NO COMMAND LINE ARGUMENTS -;) ***/
    for (int i = 1; i < argc; i++)
    {
	if (*argv[i] == '-')
	{
            if (strcasecmp (argv [i], "--selftest") == 0)
	    {
              // FIXME: Probable missuse of S9X_USAGE
              // FIXME: Actual tests. But at least this checks for coredumps.
              S9xMessage (S9X_INFO, S9X_USAGE, "Running selftest ...");
              S9xMessage (S9X_INFO, S9X_USAGE, "snes9x started:\t[OK]");
              S9xMessage (S9X_INFO, S9X_USAGE, "All tests ok.");
              exit(0);
	    }
	    if (strcasecmp (argv [i], "-so") == 0 ||
		strcasecmp (argv [i], "-sound") == 0)
	    {
		Settings.NextAPUEnabled = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-ns") == 0 ||
		     strcasecmp (argv [i], "-nosound") == 0)
	    {
		Settings.NextAPUEnabled = FALSE;
	    }
	    else if (strcasecmp (argv [i], "-soundskip") == 0 ||
		     strcasecmp (argv [i], "-sk") == 0)
	    {
		if (i + 1 < argc)
		    Settings.SoundSkipMethod = atoi (argv [++i]);
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-ra") == 0 ||
		     strcasecmp (argv [i], "-ratio") == 0)
	    {
		if ((i + 1) < argc)
		{
		}
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-h") == 0 ||
		     strcasecmp (argv [i], "-cycles") == 0)
	    {
		if (i + 1 < argc)
		{
		    int p = atoi (argv [++i]);
		    if (p > 0 && p < 200)
			Settings.CyclesPercentage = p;
		}
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-nh") == 0 ||
		     strcasecmp (argv [i], "-nohdma") == 0)
	    {
		Settings.DisableHDMA = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-ha") == 0 ||
		     strcasecmp (argv [i], "-hdma") == 0)
	    {
		Settings.DisableHDMA = FALSE;
	    }
	    else if (strcasecmp (argv [i], "-nsh") == 0 ||
		     strcasecmp (argv [i], "-nospeedhacks") == 0)
	    {
		Settings.ShutdownMaster = FALSE;
	    }
	    else if (strcasecmp (argv [i], "-sh") == 0 ||
		     strcasecmp (argv [i], "-speedhacks") == 0)
	    {
		Settings.ShutdownMaster = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-p") == 0 ||
		     strcasecmp (argv [i], "-pal") == 0)
	    {
		Settings.ForcePAL = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-n") == 0 ||
		     strcasecmp (argv [i], "-ntsc") == 0)
	    {
		Settings.ForceNTSC = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-f") == 0 ||
		     strcasecmp (argv [i], "-frameskip") == 0)
	    {
		if (i + 1 < argc)
		    Settings.SkipFrames = atoi (argv [++i]) + 1;
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-fh") == 0 ||
		     strcasecmp (argv [i], "-hr") == 0 ||
		     strcasecmp (argv [i], "-hirom") == 0)
		Settings.ForceHiROM = TRUE;
	    else if (strcasecmp (argv [i], "-fl") == 0 ||
		     strcasecmp (argv [i], "-lr") == 0 ||
		     strcasecmp (argv [i], "-lorom") == 0)
		Settings.ForceLoROM = TRUE;
	    else if (strcasecmp (argv [i], "-hd") == 0 ||
		     strcasecmp (argv [i], "-header") == 0 ||
		     strcasecmp (argv [i], "-he") == 0)
	    {
		Settings.ForceHeader = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-nhd") == 0 ||
		     strcasecmp (argv [i], "-noheader") == 0)
	    {
		Settings.ForceNoHeader = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-bs") == 0)
	    {
		Settings.BS = TRUE;
	    }
		else if (strcasecmp (argv [i], "-bsxbootup") == 0)
		{
		Settings.BSXBootup = TRUE;
		}
#ifdef DEBUGGER
	    else if (strcasecmp (argv [i], "-d") == 0 ||
		     strcasecmp (argv [i], "-debug") == 0)
	    {
		CPU.Flags |= DEBUG_MODE_FLAG;
	    }
	    else if (strcasecmp (argv [i], "-t") == 0 ||
		     strcasecmp (argv [i], "-trace") == 0)
	    {
		if(!trace) trace = fopen ("trace.log", "wb");
		CPU.Flags |= TRACE_FLAG;
	    }
#endif
	    else if (strcasecmp (argv [i], "-L") == 0 ||
		     strcasecmp (argv [i], "-layering") == 0)
		Settings.BGLayering = TRUE;
	    else if (strcasecmp (argv [i], "-nl") == 0 ||
		     strcasecmp (argv [i], "-nolayering") == 0)
		Settings.BGLayering = FALSE;
	    else if (strcasecmp (argv [i], "-tr") == 0 ||
		     strcasecmp (argv [i], "-transparency") == 0)
	    {
		Settings.ForceTransparency = TRUE;
		Settings.ForceNoTransparency = FALSE;
	    }
	    else if (strcasecmp (argv [i], "-nt") == 0 ||
		     strcasecmp (argv [i], "-notransparency") == 0)
	    {
		Settings.ForceNoTransparency = TRUE;
		Settings.ForceTransparency = FALSE;
	    }
	    else if (strcasecmp (argv [i], "-hi") == 0 ||   
		     strcasecmp (argv [i], "-hires") == 0)
	    {
		Settings.SupportHiRes = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-16") == 0 ||
		     strcasecmp (argv [i], "-sixteen") == 0)
	    {
	    }
	    else if (strcasecmp (argv [i], "-displayframerate") == 0 ||
		     strcasecmp (argv [i], "-dfr") == 0)
	    {
		Settings.DisplayFrameRate = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-i") == 0 ||
		     strcasecmp (argv [i], "-interleaved") == 0)
		Settings.ForceInterleaved = TRUE;
	    else if (strcasecmp (argv [i], "-i2") == 0 ||
		     strcasecmp (argv [i], "-interleaved2") == 0)
		Settings.ForceInterleaved2=TRUE;
	    else if (strcasecmp (argv [i], "-gd24") == 0 ||
		     strcasecmp (argv [i], "-interleavedgd24") == 0)
		Settings.ForceInterleaveGD24 = TRUE;
	    else if (strcasecmp (argv [i], "-ni") == 0 ||
		     strcasecmp (argv [i], "-nointerleave") == 0)
		Settings.ForceNotInterleaved = TRUE;
	    else if (strcasecmp (argv [i], "-noirq") == 0)
		Settings.DisableIRQ = TRUE;
	    else if (strcasecmp (argv [i], "-nw") == 0 ||
		     strcasecmp (argv [i], "-nowindows") == 0)
	    {
		Settings.DisableGraphicWindows = TRUE;
	    }
		else if (strcasecmp (argv [i], "-nopatch") == 0)
		{
			Settings.NoPatch=TRUE;
		}
		else if (strcasecmp (argv [i], "-nocheat") == 0)
		{
			Settings.ApplyCheats=FALSE;
		}
		else if (strcasecmp (argv [i], "-cheat") == 0)
		{
			Settings.ApplyCheats=TRUE;
		}
	    else if (strcasecmp (argv [i], "-windows") == 0)
	    {
		Settings.DisableGraphicWindows = FALSE;
	    }
            else if (strcasecmp (argv [i], "-aidoshm") == 0)
            {
                if (i + 1 < argc)
                {
                    Settings.AIDOShmId = atoi (argv [++i]);
                    fprintf(stderr, "Snes9X running in AIDO mode. shmid: %d\n",
                            Settings.AIDOShmId);
                } else
                    S9xUsage ();
            }
#ifdef DEBUG_MAXCOUNT
            else if (strcasecmp (argv [i], "-maxcount") == 0)
            {
                if (i + 1 < argc)
                {
                    Settings.MaxCount = atol (argv [++i]);
                    fprintf(stderr, "Running for a maximum of %d loops.\n",
                            Settings.MaxCount);
                } else
                    S9xUsage ();
            }
#endif
	    else if (strcasecmp (argv [i], "-im7") == 0)
	    {
		Settings.Mode7Interpolate = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-gg") == 0 ||
		     strcasecmp (argv [i], "-gamegenie") == 0)
	    {
		if (i + 1 < argc)
		{
		    uint32 address;
		    uint8 byte;
		    const char *error;
		    if ((error = S9xGameGenieToRaw (argv [++i], address, byte)) == NULL)
			S9xAddCheat (TRUE, FALSE, address, byte);
		    else
			S9xMessage (S9X_ERROR, S9X_GAME_GENIE_CODE_ERROR,
				    error);
		}
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-ar") == 0 ||
		     strcasecmp (argv [i], "-actionreplay") == 0)
	    {
		if (i + 1 < argc)
		{
		    uint32 address;
		    uint8 byte;
		    const char *error;
		    if ((error = S9xProActionReplayToRaw (argv [++i], address, byte)) == NULL)
			S9xAddCheat (TRUE, FALSE, address, byte);
		    else
			S9xMessage (S9X_ERROR, S9X_ACTION_REPLY_CODE_ERROR,
				    error);
		}
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-gf") == 0 ||
		     strcasecmp (argv [i], "-goldfinger") == 0)
	    {
		if (i + 1 < argc)
		{
		    uint32 address;
		    uint8 bytes [3];
		    bool8 sram;
		    uint8 num_bytes;
		    const char *error;
		    if ((error = S9xGoldFingerToRaw (argv [++i], address, sram,
						     num_bytes, bytes)) == NULL)
		    {
			for (int c = 0; c < num_bytes; c++)
			    S9xAddCheat (TRUE, FALSE, address + c, bytes [c]);
		    }
		    else
			S9xMessage (S9X_ERROR, S9X_GOLD_FINGER_CODE_ERROR,
				    error);
		}
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv[i], "-ft") == 0 ||
		     strcasecmp (argv [i], "-frametime") == 0)
	    {
		if (i + 1 < argc)
		{
		    double ft;
		    if (sscanf (argv [++i], "%lf", &ft) == 1)
		    {
#ifdef __WIN32__
			Settings.FrameTimePAL = (int32) (ft * 1000);
			Settings.FrameTimeNTSC = (int32) (ft * 1000);
#else
			Settings.FrameTimePAL = (int32) ft;
			Settings.FrameTimeNTSC = (int32) ft;
#endif

		    }
		}
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-e") == 0 ||
		     strcasecmp (argv [i], "-echo") == 0)
		Settings.DisableSoundEcho = FALSE;
	    else if (strcasecmp (argv [i], "-ne") == 0 ||
		     strcasecmp (argv [i], "-noecho") == 0)
		Settings.DisableSoundEcho = TRUE;
	    else if (strcasecmp (argv [i], "-r") == 0 ||
		     strcasecmp (argv [i], "-soundquality") == 0 ||
		     strcasecmp (argv [i], "-sq") == 0)
	    {
		if (i + 1 < argc)
		    Settings.SoundPlaybackRate = atoi (argv [++i]) & 7;
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-stereo") == 0 ||
		     strcasecmp (argv [i], "-st") == 0)
	    {
		Settings.Stereo = TRUE;
		Settings.APUEnabled = TRUE;
		Settings.NextAPUEnabled = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-mono") == 0)
	    {
		Settings.Stereo = FALSE;
		Settings.NextAPUEnabled = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-envx") == 0 ||
		     strcasecmp (argv [i], "-ex") == 0)
	    {
		Settings.SoundEnvelopeHeightReading = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-nosamplecaching") == 0 ||
		     strcasecmp (argv [i], "-nsc") == 0 ||
		     strcasecmp (argv [i], "-nc") == 0)
	    {
		Settings.DisableSampleCaching = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-nomastervolume") == 0 ||
		     strcasecmp (argv [i], "-nmv") == 0)
	    {
		Settings.DisableMasterVolume = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-soundsync") == 0 ||
		     strcasecmp (argv [i], "-sy") == 0)
	    {
		Settings.SoundSync = TRUE;
		Settings.SoundEnvelopeHeightReading = TRUE;
		Settings.InterpolatedSound = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-soundsync2") == 0 ||
		     strcasecmp (argv [i], "-sy2") == 0)
	    {
		Settings.SoundSync = 2;
		Settings.SoundEnvelopeHeightReading = TRUE;
		Settings.InterpolatedSound = TRUE;
	    }
	    else if (strcasecmp (argv [i], "-interpolatedsound") == 0 ||
		     strcasecmp (argv [i], "-is") == 0)
	    {
		Settings.InterpolatedSound = TRUE;
	    }
#ifdef USE_THREADS
	    else if (strcasecmp (argv [i], "-threadsound") == 0 ||
		     strcasecmp (argv [i], "-ts") == 0)
	    {
		Settings.ThreadSound = TRUE;
	    }
#endif
	    else if (strcasecmp (argv [i], "-alt") == 0 ||
		     strcasecmp (argv [i], "-altsampledecode") == 0)
	    {
		Settings.AltSampleDecode = 1;
	    }
	    else if (strcasecmp (argv [i], "-fix") == 0)
	    {
		Settings.FixFrequency = 1;
	    }
	    else if (strcasecmp (argv [i], "-nosuperfx") == 0 ||
		     strcasecmp (argv [i], "-nosfx") == 0)
		Settings.ForceNoSuperFX = TRUE;
	    else if (strcasecmp (argv [i], "-superfx") == 0 ||
		     strcasecmp (argv [i], "-sfx") == 0)
		Settings.ForceSuperFX = TRUE;
	    else if (strcasecmp (argv [i], "-dsp1") == 0)
		Settings.ForceDSP1 = TRUE;
	    else if (strcasecmp (argv [i], "-nodsp1") == 0)
		Settings.ForceNoDSP1 = TRUE;
	    else if (strcasecmp (argv [i], "-nomp5") == 0)
		Settings.MultiPlayer5Master = FALSE;
	    else if (strcasecmp (argv [i], "-nomouse") == 0)
		Settings.MouseMaster = FALSE;
	    else if (strcasecmp (argv [i], "-nosuperscope") == 0)
		Settings.SuperScopeMaster = FALSE;
	    else if (strcasecmp (argv [i], "-nojustifier") == 0)
		Settings.JustifierMaster = FALSE;
            else if (strcasecmp (argv[i], "-port1") == 0 ||
                     strcasecmp (argv[i], "-port2") == 0)
            {
		if (i + 1 < argc)
		{
                    i++;
                    if(!parse_controller_spec(argv[i-1][5]-'1', argv[i]))
                        S9xUsage ();
                }
		else
		    S9xUsage ();
            }
#ifdef NETPLAY_SUPPORT
	    else if (strcasecmp (argv [i], "-port") == 0 ||
		     strcasecmp (argv [i], "-po") == 0)
	    {
		if (i + 1 < argc)
		{
		    Settings.NetPlay = TRUE;
		    Settings.Port = -atoi (argv [++i]);
		}
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-server") == 0 ||
		     strcasecmp (argv [i], "-srv") == 0)
	    {
		if (i + 1 < argc)
		{
		    Settings.NetPlay = TRUE;
		    strncpy (Settings.ServerName, argv [++i], 127);
		    Settings.ServerName [127] = 0;
		}
		else
		    S9xUsage ();
	    }
	    else if (strcasecmp (argv [i], "-net") == 0)
	    {
		Settings.NetPlay = TRUE;
	    }
#endif
#ifdef STORM
            else if (strcasecmp(argv[i],"-nosecondjoy")==0){secondjoy=0;}
            else if (strcasecmp(argv[i],"-showfps")==0){dofps=1;}
            else if (strcasecmp(argv[i],"-hicolor")==0){hicolor=1;}
            else if (strcasecmp(argv[i],"-minimal")==0){minimal=1;printf("Keyboard with exception of ESC switched off!\n");}
            else if (strcasecmp(argv[i],"-ahiunit")==0)
            {
             if (i+1<argc)
             {
              fprintf(stderr,"AHI Unit set to: Unit %i\n",atoi(argv[++i]));
              unit=atoi(argv[++i]);
             }
            }
#endif
	    else if (strcasecmp (argv [i], "-conf") == 0)
	    {
                if (++i>=argc) S9xUsage();
                // Else do nothing, S9xLoadConfigFiles() handled it
            }
	    else if (strcasecmp (argv [i], "-nostdconf") == 0)
            {
                // Do nothing, S9xLoadConfigFiles() handled it
            }

	    else
		S9xParseArg (argv, i, argc);
	}
	else
	    rom_filename = argv [i];
    }

    S9xVerifyControllers();

#endif

    return (rom_filename);

}

#ifndef NGC
void S9xParseCheatsFile (const char *rom_filename)
{
    FILE *f;
    char dir [_MAX_DIR];
    char drive [_MAX_DRIVE];
    char name [_MAX_FNAME];
    char ext [_MAX_EXT];
    char fname [_MAX_PATH];
    char buf [80];
    uint32 address;
    uint8 byte;
    uint8 bytes [3];
    bool8 sram;
    uint8 num_bytes;
    const char *error;
    char *p;

    _splitpath (rom_filename, drive, dir, name, ext);
    _makepath (fname, drive, dir, name, "pat");

    if ((f = fopen(fname, "r")) != NULL)
    {
        while(fgets(buf, 80, f) != NULL)
        {
	    if ((p = strrchr (buf, '\n')) != NULL) 
		*p = '\0';
	    if (((error = S9xGameGenieToRaw (buf, address, byte)) == NULL) ||
		((error = S9xProActionReplayToRaw (buf, address, byte)) == NULL))
	    {
		S9xAddCheat (TRUE, FALSE, address, byte);
	    }
	    else
	    if ((error = S9xGoldFingerToRaw (buf, address, sram,
					     num_bytes, bytes)) == NULL)
	    {
		for (int c = 0; c < num_bytes; c++)
		    S9xAddCheat (TRUE, FALSE, address + c, bytes [c]);
	    }
	    else
		S9xMessage (S9X_ERROR, S9X_GAME_GENIE_CODE_ERROR, error);
        }
        fclose(f);
    }
}

#endif

#ifndef NGC
#include "conffile.h"
#include "crosshairs.h"

static void parse_crosshair_spec(enum crosscontrols ctl, const char *spec){
    int idx=-1, i;
    const char *fg=NULL, *bg=NULL, *s=spec;

    if(s[0]=='"'){
        s++;
        for(i=0; s[i]!='\0'; i++){
            if(s[i]=='"' && s[i-1]!='\\') break;
        }
        idx=31-ctl;
        std::string fname(s, i);
        if(!S9xLoadCrosshairFile(idx, fname.c_str())) return;
        s+=i+1;
    } else {
        if(isdigit(*s)){ idx=*s-'0'; s++; }
        if(isdigit(*s)){ idx=idx*10+*s-'0'; s++; }
        if(idx>31){
            fprintf(stderr, "Invalid crosshair spec '%s'\n", spec);
            return;
        }
    }

    while(*s!='\0' && isspace(*s)){ s++; }
    if(*s!='\0'){
        fg=s;
        while(isalnum(*s)){ s++; }
        if(*s!='/' || !isalnum(s[1])){
            fprintf(stderr, "Invalid crosshair spec '%s'\n", spec);
            return;
        }
        bg=++s;
        while(isalnum(*s)){ s++; }
        if(*s!='\0'){
            fprintf(stderr, "Invalid crosshair spec '%s'\n", spec);
            return;
        }
    }

    S9xSetControllerCrosshair(ctl, idx, fg, bg);
}

static bool try_load(const char *fname, ConfigFile &conf){
    STREAM fp;
    if((fp=OPEN_STREAM(fname, "r"))!=NULL){
        fprintf(stderr, "Reading config file %s\n", fname);
        conf.LoadFile(new fReader(fp));
        CLOSE_STREAM(fp);
        return true;
    }
    return false;
}

void S9xLoadConfigFiles(char **argv, int argc){
    int i;
    bool skip=false;

    for(i=0; i<argc; i++){
        if(!strcasecmp(argv[i], "-nostdconf")){
            skip=true;
            break;
        }
    }

    ConfigFile conf;
    conf.Clear();

    if(!skip){
#ifdef SYS_CONFIG_FILE
        try_load(SYS_CONFIG_FILE, conf);
        S9xParsePortConfig(conf, 0);
#endif

        std::string fname;
        fname=S9xGetDirectory(DEFAULT_DIR);
        fname+=SLASH_STR "snes9x.conf";
        if(!try_load(fname.c_str(), conf)){
            fname=S9xGetDirectory(DEFAULT_DIR);
            fname+=SLASH_STR "snes9x.cfg";
            try_load(fname.c_str(), conf);
        }

        fname=S9xGetDirectory(ROM_DIR);
        fname+=SLASH_STR "snes9x.conf";
        if(!try_load(fname.c_str(), conf)){
            fname=S9xGetDirectory(ROM_DIR);
            fname+=SLASH_STR "snes9x.cfg";
            try_load(fname.c_str(), conf);
        }
    } else {
        fprintf(stderr, "Skipping standard config files\n");
    }

    for(i=0; i<argc-1; i++){
        if(!strcasecmp(argv[i], "-conf")) try_load(argv[++i], conf);
    }

    /* Parse config file here */
    Settings.NextAPUEnabled=conf.GetBool("Sound::APUEnabled", Settings.APUEnabled);
    Settings.SoundSkipMethod=conf.GetInt("Sound::SoundSkip", 0);
    i=conf.GetInt("CPU::Cycles", 100);
    if(i>0 && i<200) Settings.CyclesPercentage = i;
    Settings.DisableHDMA=conf.GetBool("Settings::DisableHDMA", false);
    Settings.ShutdownMaster=conf.GetBool("Settings::SpeedHacks", true);
    Settings.ForcePAL=conf.GetBool("ROM::PAL", false);
    Settings.ForceNTSC=conf.GetBool("ROM::NTSC", false);
    if(!strcasecmp(conf.GetString("Settings::FrameSkip", "Auto"),"Auto")){
        Settings.SkipFrames=AUTO_FRAMERATE;
    } else {
        Settings.SkipFrames=conf.GetUInt("Settings::FrameSkip", 0)+1;
    }
    Settings.TurboSkipFrames=conf.GetUInt("Settings::TurboFrameSkip", 15);
    Settings.TurboMode=conf.GetBool("Settings::TurboMode",false);
    Settings.StretchScreenshots=conf.GetInt("Settings::StretchScreenshots",1);
    Settings.AutoSaveDelay=conf.GetUInt("Settings::AutoSaveDelay", 30);
    Settings.ForceHiROM=conf.GetBool("ROM::HiROM", false);
    Settings.ForceLoROM=conf.GetBool("ROM::LoROM", false);
    if(conf.Exists("ROM::Header")){
        Settings.ForceHeader=conf.GetBool("ROM::Header");
        Settings.ForceNoHeader=!Settings.ForceHeader;
    }
    Settings.BS=conf.GetBool("ROM::BS", false);
#ifdef DEBUGGER
    if(conf.GetBool("DEBUG::Debugger", false)) CPU.Flags |= DEBUG_MODE_FLAG;
    if(conf.GetBool("DEBUG::Trace", false)){
        if(!trace) trace = fopen ("trace.log", "wb");
        CPU.Flags |= TRACE_FLAG;
    }
#endif
    Settings.BGLayering=conf.GetBool("Settings::BGLayeringHack", false);
    if(conf.Exists("Display::Transparency")){
        Settings.ForceTransparency=conf.GetBool("Display::Transparency", true);
        Settings.ForceNoTransparency=!Settings.ForceTransparency;
    }
    Settings.SupportHiRes=conf.GetBool("Display::HiRes", true);
    Settings.DisplayFrameRate=conf.GetBool("Display::FrameRate", false);
    if(conf.Exists("ROM::Interleaved")){
        Settings.ForceInterleaved=conf.GetBool("ROM::Interleaved", false);
        Settings.ForceNotInterleaved=!Settings.ForceInterleaved;
    }
    Settings.ForceInterleaved2=conf.GetBool("ROM::Interleaved2", false);
    Settings.ForceInterleaveGD24=conf.GetBool("ROM::InterleaveGD24", false);
    Settings.DisableIRQ=conf.GetBool("CPU::DisableIRQ", false);
    Settings.DisableGraphicWindows=!conf.GetBool("Display::GraphicWindows", true);
    Settings.NoPatch=!conf.GetBool("ROM::Patch", true);
    Settings.ApplyCheats=conf.GetBool("ROM::Cheat", true);
#ifdef DEBUG_MAXCOUNT
    if(conf.Exists("DEBUG::MaxCount")){
        Settings.MaxCount = conf.GetUInt("DEBUG::MaxCount", 1);
        fprintf(stderr, "Running for a maximum of %d loops.\n", Settings.MaxCount);
    }
#endif
    Settings.Mode7Interpolate=conf.GetBool("Display::Mode7Interpolate", false);
    Settings.FrameTimePAL=conf.GetUInt("Settings::PALFrameTime", 20000);
    Settings.FrameTimeNTSC=conf.GetUInt("Settings::NTSCFrameTime", 16667);
    if(conf.Exists("Settings::FrameTime")){
        double ft;
        if (sscanf(conf.GetString("Settings::FrameTime"), "%lf", &ft) == 1)
        {
#ifdef __WIN32__
            Settings.FrameTimePAL = (int32) (ft * 1000);
            Settings.FrameTimeNTSC = (int32) (ft * 1000);
#else
            Settings.FrameTimePAL = (int32) ft;
            Settings.FrameTimeNTSC = (int32) ft;
#endif

        }
    }
    Settings.FrameTime = Settings.FrameTimeNTSC;
    Settings.DisableSoundEcho=!conf.GetBool("Sound::Echo", true);
    Settings.SoundPlaybackRate=conf.GetUInt("Sound::Rate", Settings.SoundPlaybackRate) & 7;
    Settings.SoundBufferSize=conf.GetUInt("Sound::BufferSize", Settings.SoundBufferSize);
    if(conf.Exists("Sound::Stereo")){
        Settings.Stereo = conf.GetBool("Sound::Stereo");
        Settings.APUEnabled = TRUE;
        Settings.NextAPUEnabled = TRUE;
    }
    if(conf.Exists("Sound::Mono")){
        Settings.Stereo = !conf.GetBool("Sound::Mono");
        Settings.NextAPUEnabled = TRUE;
    }
    Settings.SoundEnvelopeHeightReading=conf.GetBool("Sound::EnvelopeHeightReading");
    Settings.DisableSampleCaching=!conf.GetBool("Sound::SampleCaching");
    Settings.DisableMasterVolume=!conf.GetBool("Sound::MasterVolume");
    Settings.InterpolatedSound=conf.GetBool("Sound::Interpolate", false);
    if(conf.Exists("Sound::Sync")){
        Settings.SoundSync=conf.GetInt("Sound::Sync", 1);
        if(Settings.SoundSync>2) Settings.SoundSync=1;
        Settings.SoundEnvelopeHeightReading = TRUE;
        Settings.InterpolatedSound = TRUE;
    }
#ifdef USE_THREADS
    Settings.ThreadSound=conf.GetBool("Sound::ThreadSound", false);
#endif
    if(conf.Exists("Sound::AltDecode")){
        Settings.AltSampleDecode=conf.GetInt("Sound::AltDecode", 1);
    }
    if(conf.Exists("Sound::FixFrequency")){
        Settings.FixFrequency=conf.GetInt("Sound::FixFrequency", 1);
    }
    if(conf.Exists("ROM::SuperFX")){
        Settings.ForceSuperFX=conf.GetBool("ROM::SuperFX");
        Settings.ForceNoSuperFX=!Settings.ForceSuperFX;
    }
    if(conf.Exists("ROM::DSP1")){
        Settings.ForceDSP1=conf.GetBool("ROM::DSP1");
        Settings.ForceNoDSP1=!Settings.ForceDSP1;
    }
    Settings.MultiPlayer5Master=conf.GetBool("Controls::MP5Master", true);
    Settings.MouseMaster=conf.GetBool("Controls::MouseMaster", true);
    Settings.SuperScopeMaster=conf.GetBool("Controls::SuperscopeMaster", true);
    Settings.JustifierMaster=conf.GetBool("Controls::JustifierMaster", true);
    if(conf.Exists("Controls::Port1")){
        parse_controller_spec(0, conf.GetString("Controls::Port1"));
    }
    if(conf.Exists("Controls::Port2")){
        parse_controller_spec(1, conf.GetString("Controls::Port2"));
    }
    if(conf.Exists("Controls::Mouse1Crosshair")){
        parse_crosshair_spec(X_MOUSE1, conf.GetString("Controls::Mouse1Crosshair"));
    }
    if(conf.Exists("Controls::Mouse2Crosshair")){
        parse_crosshair_spec(X_MOUSE2, conf.GetString("Controls::Mouse2Crosshair"));
    }
    if(conf.Exists("Controls::SuperscopeCrosshair")){
        parse_crosshair_spec(X_SUPERSCOPE, conf.GetString("Controls::SuperscopeCrosshair"));
    }
    if(conf.Exists("Controls::Justifier1Crosshair")){
        parse_crosshair_spec(X_JUSTIFIER1, conf.GetString("Controls::Justifier1Crosshair"));
    }
    if(conf.Exists("Controls::Justifier2Crosshair")){
        parse_crosshair_spec(X_JUSTIFIER2, conf.GetString("Controls::Justifier2Crosshair"));
    }
#ifdef NETPLAY_SUPPORT
    Settings.Port = NP_DEFAULT_PORT;
    if(conf.Exists("Netplay::Port")){
        Settings.NetPlay = TRUE;
        Settings.Port = -conf.GetUInt("Netplay::Port");
    }
    Settings.ServerName[0]='\0';
    if(conf.Exists("Netplay::Server")){
        Settings.NetPlay = TRUE;
        conf.GetString("Netplay::Server", Settings.ServerName, 128);
    }
    Settings.NetPlay=conf.GetBool("Netplay::Enable");
#endif
#ifdef STORM
    secondjoy=conf.GetBool("STORM::EnableSecondJoy",true)?1:0;
    dofps=conf.GetBool("STORM::ShowFPS",false)?1:0;
    hicolor=conf.GetBool("STORM::HiColor",false)?1:0;
    if((minimal=conf.GetBool("STORM::Minimal")?1:0)){
        printf("Keyboard with exception of ESC switched off!\n");
    }
    if(conf.Exists("STORM::AHIunit")){
        unit=conf.GetInt("STORM::AHIunit",0);
        fprintf(stderr,"AHI Unit set to: Unit %i\n",unit);
    }
#endif
    Settings.JoystickEnabled=conf.GetBool("Controls::Joystick", Settings.JoystickEnabled);
    rom_filename=conf.GetStringDup("ROM::Filename", NULL);

    S9xParsePortConfig(conf, 1);
    S9xVerifyControllers();
}

#endif /*** Nintendo Gamecube Exclusion ***/
