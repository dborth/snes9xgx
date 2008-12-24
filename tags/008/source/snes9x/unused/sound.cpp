/**********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  (c) Copyright 1996 - 2002  Gary Henderson (gary.henderson@ntlworld.com),
                             Jerremy Koot (jkoot@snes9x.com)

  (c) Copyright 2002 - 2004  Matthew Kendora

  (c) Copyright 2002 - 2005  Peter Bortas (peter@bortas.org)

  (c) Copyright 2004 - 2005  Joel Yliluoma (http://iki.fi/bisqwit/)

  (c) Copyright 2001 - 2006  John Weidman (jweidman@slip.net)

  (c) Copyright 2002 - 2006  funkyass (funkyass@spam.shaw.ca),
                             Kris Bleakley (codeviolation@hotmail.com)

  (c) Copyright 2002 - 2007  Brad Jorsch (anomie@users.sourceforge.net),
                             Nach (n-a-c-h@users.sourceforge.net),
                             zones (kasumitokoduck@yahoo.com)

  (c) Copyright 2006 - 2007  nitsuja


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

  HQ2x, HQ3x, HQ4x filters
  (c) Copyright 2003         Maxim Stepin (maxim@hiend3d.com)

  Win32 GUI code
  (c) Copyright 2003 - 2006  blip,
                             funkyass,
                             Matthew Kendora,
                             Nach,
                             nitsuja

  Mac OS GUI code
  (c) Copyright 1998 - 2001  John Stiles
  (c) Copyright 2001 - 2007  zones


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



#ifndef __WIN32__

#include <devices/ahi.h>
#include <exec/exec.h>
#include <proto/ahi.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <clib/ahippc_protos.h>
#include <stdio.h>

#define EQ ==
#define MINBUFFLEN 10000

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "snes9x.h"
#include "soundux.h"

extern SoundStatus so;

extern int AudioOpen(unsigned long freq, unsigned long bufsize, unsigned long bitrate, unsigned long stereo);
extern void AudioClose(void);

extern int OpenPrelude(ULONG Type, ULONG DefaultFreq, ULONG MinBuffSize);
extern void ClosePrelude(void);

extern int SoundSignal;
unsigned long DoubleBuffer;
//extern struct AHISampleInfo Sample0;
//extern struct AHISampleInfo Sample1;
//extern unsigned long BufferSize;

struct Library    *AHIPPCBase;
struct Library    *AHIBase;
struct MsgPort    *AHImp=NULL;
struct AHIRequest *AHIio=NULL;
BYTE               AHIDevice=-1;

struct AHIData *AHIData;

unsigned long Frequency = 0;
//unsigned long BufferSize = 0;
unsigned long BitRate = 0;
unsigned long Stereo = 0;
//unsigned long AHIError = 9;

BYTE InternSignal=-1;

int mixsamples;
extern int prelude;

#define REALSIZE (BitRate*Stereo)

struct AHIAudioModeRequester *req=NULL;
struct AHIAudioCtrl *actrl=NULL;

ULONG BufferLen=NULL;


/* this really should be dynamically allocated... */
#undef  MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 65536
#define MIN_BUFFER_SIZE 65536

#define MODE_MONO       0
#define MODE_STEREO     1

#define QUAL_8BIT       8
#define QUAL_16BIT      16


int test=0;
int test2=0;

int AudioOpen(unsigned long freq, unsigned long minbufsize, unsigned long bitrate, unsigned long stereo)
{
        ULONG Type;

        Frequency = freq;

    so.playback_rate = Frequency;

    if(stereo) so.stereo = TRUE;
    else so.stereo = FALSE;

        switch(bitrate)
        {
                case 8:
            so.sixteen_bit = FALSE;
                        BitRate=1;
                        if(stereo)
                        {
                                Stereo=2;
                                Type = AHIST_S8S;
                        }
                        else
                        {
                                Stereo=1;
                                Type = AHIST_M8S;
                        }

                break;

                default:        //defaulting to 16bit, because it means it won't crash atleast
                case QUAL_16BIT:
            so.sixteen_bit = TRUE;
                        BitRate=2;
                        if(stereo)
                        {
                                Stereo=2;
                                Type = AHIST_S16S;
                        }
                        else
                        {
                                Stereo=1;
                                Type = AHIST_M16S;
                        }
                break;
        }

    if(prelude) prelude = OpenPrelude(Type, freq, minbufsize);


    if(prelude) return 1; else printf("Defaulting to AHI...\n");

    /* only 1 channel right? */
    /* NOTE: The buffersize will not always be what you requested
     * it finds the minimun AHI requires and then rounds it up to
     * nearest 32 bytes.  Check AHIData->BufferSize or Samples[n].something_Length
     */
    if(AHIData = OpenAHI(1, Type, AHI_INVALID_ID, AHI_DEFAULT_FREQ, 0, minbufsize))
    {
        printf("AHI opened\n");
        printf("BuffSize %d\n", AHIData->BufferSize);
    }
    else
    {
        printf("AHI failed to open: %d\n", AHIData);
        return 0;
    }

    so.buffer_size = AHIData->BufferSize; // in bytes
        if (so.buffer_size > MAX_BUFFER_SIZE) so.buffer_size = MAX_BUFFER_SIZE;

    /* Lots of useful fields in the AHIData struct, have a look */
    AHIBase = AHIData->AHIBase;
    actrl = AHIData->AudioCtrl;
    Frequency = AHIData->MixingFreq;

        printf("signal %ld\n", AHIData->SoundFuncSignal);

        Wait(AHIData->SoundFuncSignal);

        /* I don't think it should start playing until there is something
         * In the buffer, however to set off the SoundFunc it should
         * probably go through the buffer at least once, just silently.
         */
        AHI_SetFreq(0, Frequency, actrl, AHISF_IMM);

        Wait(AHIData->SoundFuncSignal);

        AHI_SetVol(0, 0x10000, 0x8000, actrl, AHISF_IMM);

        mixsamples=AHIData->BufferSamples;

        SoundSignal = AHIData->SoundFuncSignal;

    return 1;
}

void AudioClose( void )
{
    if(prelude) ClosePrelude();
        else ;//CloseAHI(AHIData);
}


#include <wbstartup.h>

extern int main(int argc, char **argv);

void wbmain(struct WBStartup * argmsg)
{
 char argv[1][]={"WarpSNES"};
 int argc=1;
 main(argc,(char **)argv);
}


#endif
