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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "snes9x.h"

#include "memmap.h"
#include "ppu.h"
#include "cpuexec.h"
#include "missing.h"
#include "dma.h"
#include "apu.h"
#include "gfx.h"
#include "sa1.h"
#include "spc7110.h"

#ifdef SDD1_DECOMP
#include "sdd1emu.h"
#endif

#ifdef SDD1_DECOMP
uint8 buffer[0x10000];
#endif

extern int HDMA_ModeByteCounts [8];
extern uint8 *HDMAMemPointers [8];

#if defined(__linux__) || defined(__WIN32__) || defined(__MACOSX__)
static int S9xCompareSDD1IndexEntries (const void *p1, const void *p2)
{
    return (*(uint32 *) p1 - *(uint32 *) p2);
}
#endif

static inline void S9xTrySyncAPUInDMA (void)
{
#ifdef SPC700_C
	S9xUpdateAPUTimer();
	APU_EXECUTE();
#endif
}

/**********************************************************************************************/
/* S9xDoDMA()                                                                                   */
/* This function preforms the general dma transfer                                            */
/**********************************************************************************************/

void S9xDoDMA (uint8 Channel)
{
    uint8 Work;

    if (Channel > 7 || CPU.InDMA)
        return;

    CPU.InDMA = TRUE;
    bool8 in_sa1_dma = FALSE;
    uint8 *in_sdd1_dma = NULL;
    uint8 *spc7110_dma=NULL;
    bool s7_wrap=false;
    SDMA *d = &DMA[Channel];


    int32 count = d->TransferBytes;

    if (count == 0)
        count = 0x10000;

    int inc = d->AAddressFixed ? 0 : (!d->AAddressDecrement ? 1 : -1);

    if((d->ABank==0x7E || d->ABank==0x7F) && d->BAddress==0x80 && !d->TransferDirection)
    {
        d->AAddress+= d->TransferBytes;
        //does an invalid DMA actually take time?
        // I'd say yes, since 'invalid' is probably just the WRAM chip
        // not being able to read and write itself at the same time
        // And no, PPU.WRAM should not be updated.
        CPU.Cycles+=(d->TransferBytes+1)*SLOW_ONE_CYCLE;
		S9xTrySyncAPUInDMA();

#ifdef DEBUGGER
        if (Settings.TraceDMA)
        {
            sprintf (String, "DMA[%d]: %s Mode: %d 0x%02X%04X->0x21%02X Bytes: %d (%s) V-Line:%ld -- ABORT WRAM Bank%02X->$2180",
                     Channel, d->TransferDirection ? "read" : "write",
                     d->TransferMode, d->ABank, d->AAddress,
                     d->BAddress, d->TransferBytes,
                     d->AAddressFixed ? "fixed" :
                     (d->AAddressDecrement ? "dec" : "inc"),
                     CPU.V_Counter, d->ABank);
            S9xMessage (S9X_TRACE, S9X_DMA_TRACE, String);
        }
#endif
        goto update_address;
    }
    switch (d->BAddress)
    {
      case 0x18:
      case 0x19:
        if (IPPU.RenderThisFrame)
            FLUSH_REDRAW ();
        break;
    }
    if (Settings.SDD1)
    {
        if (d->AAddressFixed && Memory.FillRAM [0x4801] > 0)
        {
            // XXX: Should probably verify that we're DMAing from ROM? And
            // somewhere we should make sure we're not running across a mapping
            // boundary too.
            // Hacky support for pre-decompressed S-DD1 data
            inc = !d->AAddressDecrement ? 1 : -1;
            uint32 address = (((d->ABank << 16) | d->AAddress) & 0xfffff) << 4;

            address |= Memory.FillRAM [0x4804 + ((d->ABank - 0xc0) >> 4)];

#ifdef SDD1_DECOMP
            if(Settings.SDD1Pack)
            {
                uint8* in_ptr=GetBasePointer(((d->ABank << 16) | d->AAddress));
                if(in_ptr){
                    in_ptr+=d->AAddress;
                    SDD1_decompress(buffer,in_ptr,d->TransferBytes);
                } else {
                    sprintf(String, "S-DD1 DMA from non-block address $%02X:%04X", d->ABank, d->AAddress);
                    S9xMessage(S9X_WARNING, S9X_DMA_TRACE, String);
                }
                in_sdd1_dma=buffer;
#ifdef SDD1_VERIFY
                void *ptr = bsearch (&address, Memory.SDD1Index, 
                                     Memory.SDD1Entries, 12, S9xCompareSDD1IndexEntries);
                if(memcmp(buffer, ptr, d->TransferBytes))
                {
                    uint8 *p = Memory.SDD1LoggedData;
                    bool8 found = FALSE;
                    uint8 SDD1Bank = Memory.FillRAM [0x4804 + ((d->ABank - 0xc0) >> 4)] | 0xf0;

                    for (uint32 i = 0; i < Memory.SDD1LoggedDataCount; i++, p += 8)
                    {
                        if (*p == d->ABank ||
                            *(p + 1) == (d->AAddress >> 8) &&
                            *(p + 2) == (d->AAddress & 0xff) &&
                            *(p + 3) == (count >> 8) &&
                            *(p + 4) == (count & 0xff) &&
                            *(p + 7) == SDD1Bank)
                        {
                            found = TRUE;
                        }
                    }
                    if (!found && Memory.SDD1LoggedDataCount < MEMMAP_MAX_SDD1_LOGGED_ENTRIES)
                    {
                        int j=0;
                        while(ptr[j]==buffer[j])
                            j++;

                        *p = d->ABank;
                        *(p + 1) = d->AAddress >> 8;
                        *(p + 2) = d->AAddress & 0xff;
                        *(p + 3) = j&0xFF;
                        *(p + 4) = (j>>8)&0xFF;
                        *(p + 7) = SDD1Bank;
                        Memory.SDD1LoggedDataCount += 1;
                    }
                }
#endif
            }

            else
            {
#endif
#if defined(__linux__) || defined(__WIN32__) || defined(__MACOSX__)
                void *ptr = bsearch (&address, Memory.SDD1Index, 
                                     Memory.SDD1Entries, 12, S9xCompareSDD1IndexEntries);
                if (ptr)
                    in_sdd1_dma = *(uint32 *) ((uint8 *) ptr + 4) + Memory.SDD1Data;
#else
                uint8 *ptr = Memory.SDD1Index;

                for (uint32 e = 0; e < Memory.SDD1Entries; e++, ptr += 12)
                {
                    if (address == *(uint32 *) ptr)
                    {
                        in_sdd1_dma = *(uint32 *) (ptr + 4) + Memory.SDD1Data;
                        break;
                    }
                }
#endif

                if (!in_sdd1_dma)
                {
                    // No matching decompressed data found. Must be some new 
                    // graphics not encountered before. Log it if it hasn't been
                    // already.
                    uint8 *p = Memory.SDD1LoggedData;
                    bool8 found = FALSE;
                    uint8 SDD1Bank = Memory.FillRAM [0x4804 + ((d->ABank - 0xc0) >> 4)] | 0xf0;

                    for (uint32 i = 0; i < Memory.SDD1LoggedDataCount; i++, p += 8)
                    {
                        if (*p == d->ABank ||
                            *(p + 1) == (d->AAddress >> 8) &&
                            *(p + 2) == (d->AAddress & 0xff) &&
                            *(p + 3) == (count >> 8) &&
                            *(p + 4) == (count & 0xff) &&
                            *(p + 7) == SDD1Bank)
                        {
                            found = TRUE;
                            break;
                        }
                    }
                    if (!found && Memory.SDD1LoggedDataCount < MEMMAP_MAX_SDD1_LOGGED_ENTRIES)
                    {
                        *p = d->ABank;
                        *(p + 1) = d->AAddress >> 8;
                        *(p + 2) = d->AAddress & 0xff;
                        *(p + 3) = count >> 8;
                        *(p + 4) = count & 0xff;
                        *(p + 7) = SDD1Bank;
                        Memory.SDD1LoggedDataCount += 1;
                    }
                }
            }
#ifdef SDD1_DECOMP
        }
#endif

        Memory.FillRAM [0x4801] = 0;
    }
    if(Settings.SPC7110&&(d->AAddress==0x4800||d->ABank==0x50))
    {
        uint32 i,j;
        i=(s7r.reg4805|(s7r.reg4806<<8));
#ifdef SPC7110_DEBUG
        printf("DMA Transfer of %04X bytes from %02X%02X%02X:%02X, offset of %04X, internal bank of %04X, multiplier %02X\n",d->TransferBytes,s7r.reg4803,s7r.reg4802,s7r.reg4801, s7r.reg4804,i,  s7r.bank50Internal, s7r.AlignBy);
#endif
        i*=s7r.AlignBy;
        i+=s7r.bank50Internal;
        i%=DECOMP_BUFFER_SIZE;
        j=0;
        if((i+d->TransferBytes)<DECOMP_BUFFER_SIZE)
        {
            spc7110_dma=&s7r.bank50[i];
        }
        else
        {
            spc7110_dma=new uint8[d->TransferBytes];
            j=DECOMP_BUFFER_SIZE-i;
            memcpy(spc7110_dma, &s7r.bank50[i], j);
            memcpy(&spc7110_dma[j],s7r.bank50,d->TransferBytes-j);
            s7_wrap=true;
        }
        int icount=s7r.reg4809|(s7r.reg480A<<8);
        icount-=d->TransferBytes;
        s7r.reg4809=0x00ff&icount;
        s7r.reg480A=(0xff00&icount)>>8;

        s7r.bank50Internal+=d->TransferBytes;
        s7r.bank50Internal%=DECOMP_BUFFER_SIZE;
        inc=1;
        d->AAddress-=count;
    }
    if (d->BAddress == 0x18 && SA1.in_char_dma && (d->ABank & 0xf0) == 0x40)
    {
        // Perform packed bitmap to PPU character format conversion on the
        // data before transmitting it to V-RAM via-DMA.
        int num_chars = 1 << ((Memory.FillRAM [0x2231] >> 2) & 7);
        int depth = (Memory.FillRAM [0x2231] & 3) == 0 ? 8 :
            (Memory.FillRAM [0x2231] & 3) == 1 ? 4 : 2;

        int bytes_per_char = 8 * depth;
        int bytes_per_line = depth * num_chars;
        int char_line_bytes = bytes_per_char * num_chars;
        uint32 addr = (d->AAddress / char_line_bytes) * char_line_bytes;
        uint8 *base = GetBasePointer ((d->ABank << 16) + addr);
        if(!base){
            sprintf(String, "SA-1 DMA from non-block address $%02X:%04X", d->ABank, addr);
            S9xMessage(S9X_WARNING, S9X_DMA_TRACE, String);
            base=Memory.ROM;
        }
        base+=addr;
        uint8 *buffer = &Memory.ROM [CMemory::MAX_ROM_SIZE - 0x10000];
        uint8 *p = buffer;
        uint32 inc = char_line_bytes - (d->AAddress % char_line_bytes);
        uint32 char_count = inc / bytes_per_char;

        in_sa1_dma = TRUE;

        //printf ("%08x,", base); fflush (stdout);
        //printf ("depth = %d, count = %d, bytes_per_char = %d, bytes_per_line = %d, num_chars = %d, char_line_bytes = %d\n",
        //depth, count, bytes_per_char, bytes_per_line, num_chars, char_line_bytes);
        int i;

        switch (depth)
        {
          case 2:
            for (i = 0; i < count; i += inc, base += char_line_bytes, 
                 inc = char_line_bytes, char_count = num_chars)
            {
                uint8 *line = base + (num_chars - char_count) * 2;
                for (uint32 j = 0; j < char_count && p - buffer < count; 
                     j++, line += 2)
                {
                    uint8 *q = line;
                    for (int l = 0; l < 8; l++, q += bytes_per_line)
                    {
                        for (int b = 0; b < 2; b++)
                        {
                            uint8 r = *(q + b);
                            *(p + 0) = (*(p + 0) << 1) | ((r >> 0) & 1);
                            *(p + 1) = (*(p + 1) << 1) | ((r >> 1) & 1);
                            *(p + 0) = (*(p + 0) << 1) | ((r >> 2) & 1);
                            *(p + 1) = (*(p + 1) << 1) | ((r >> 3) & 1);
                            *(p + 0) = (*(p + 0) << 1) | ((r >> 4) & 1);
                            *(p + 1) = (*(p + 1) << 1) | ((r >> 5) & 1);
                            *(p + 0) = (*(p + 0) << 1) | ((r >> 6) & 1);
                            *(p + 1) = (*(p + 1) << 1) | ((r >> 7) & 1);
                        }
                        p += 2;
                    }
                }
            }
            break;
          case 4:
            for (i = 0; i < count; i += inc, base += char_line_bytes, 
                 inc = char_line_bytes, char_count = num_chars)
            {
                uint8 *line = base + (num_chars - char_count) * 4;
                for (uint32 j = 0; j < char_count && p - buffer < count; 
                     j++, line += 4)
                {
                    uint8 *q = line;
                    for (int l = 0; l < 8; l++, q += bytes_per_line)
                    {
                        for (int b = 0; b < 4; b++)
                        {
                            uint8 r = *(q + b);
                            *(p +  0) = (*(p +  0) << 1) | ((r >> 0) & 1);
                            *(p +  1) = (*(p +  1) << 1) | ((r >> 1) & 1);
                            *(p + 16) = (*(p + 16) << 1) | ((r >> 2) & 1);
                            *(p + 17) = (*(p + 17) << 1) | ((r >> 3) & 1);
                            *(p +  0) = (*(p +  0) << 1) | ((r >> 4) & 1);
                            *(p +  1) = (*(p +  1) << 1) | ((r >> 5) & 1);
                            *(p + 16) = (*(p + 16) << 1) | ((r >> 6) & 1);
                            *(p + 17) = (*(p + 17) << 1) | ((r >> 7) & 1);
                        }
                        p += 2;
                    }
                    p += 32 - 16;
                }
            }
            break;
          case 8:
            for (i = 0; i < count; i += inc, base += char_line_bytes, 
                 inc = char_line_bytes, char_count = num_chars)
            {
                uint8 *line = base + (num_chars - char_count) * 8;
                for (uint32 j = 0; j < char_count && p - buffer < count; 
                     j++, line += 8)
                {
                    uint8 *q = line;
                    for (int l = 0; l < 8; l++, q += bytes_per_line)
                    {
                        for (int b = 0; b < 8; b++)
                        {
                            uint8 r = *(q + b);
                            *(p +  0) = (*(p +  0) << 1) | ((r >> 0) & 1);
                            *(p +  1) = (*(p +  1) << 1) | ((r >> 1) & 1);
                            *(p + 16) = (*(p + 16) << 1) | ((r >> 2) & 1);
                            *(p + 17) = (*(p + 17) << 1) | ((r >> 3) & 1);
                            *(p + 32) = (*(p + 32) << 1) | ((r >> 4) & 1);
                            *(p + 33) = (*(p + 33) << 1) | ((r >> 5) & 1);
                            *(p + 48) = (*(p + 48) << 1) | ((r >> 6) & 1);
                            *(p + 49) = (*(p + 49) << 1) | ((r >> 7) & 1);
                        }
                        p += 2;
                    }
                    p += 64 - 16;
                }
            }
            break;
        }
    }

#ifdef DEBUGGER
    if (Settings.TraceDMA)
    {
        sprintf (String, "DMA[%d]: %s Mode: %d 0x%02X%04X->0x21%02X Bytes: %d (%s) V-Line:%ld",
                 Channel, d->TransferDirection ? "read" : "write",
                 d->TransferMode, d->ABank, d->AAddress,
                 d->BAddress, d->TransferBytes,
                 d->AAddressFixed ? "fixed" :
                 (d->AAddressDecrement ? "dec" : "inc"),
                 CPU.V_Counter);
        if (d->BAddress == 0x18 || d->BAddress == 0x19 || d->BAddress == 0x39 || d->BAddress == 0x3a)
            sprintf (String, "%s VRAM: %04X (%d,%d) %s", String,
                     PPU.VMA.Address,
                     PPU.VMA.Increment, PPU.VMA.FullGraphicCount,
                     PPU.VMA.High ? "word" : "byte");

        else
            if (d->BAddress == 0x22 || d->BAddress == 0x3b)

                sprintf (String, "%s CGRAM: %02X (%x)", String, PPU.CGADD,
                         PPU.CGFLIP);			
            else
                if (d->BAddress == 0x04 || d->BAddress == 0x38)
                    sprintf (String, "%s OBJADDR: %04X", String, PPU.OAMAddr);
        S9xMessage (S9X_TRACE, S9X_DMA_TRACE, String);
    }
#endif

    if (!d->TransferDirection)
    {
        //reflects extra cycle used by DMA
        CPU.Cycles += SLOW_ONE_CYCLE * (count+1);
		S9xTrySyncAPUInDMA();

        uint8 *base = GetBasePointer((d->ABank << 16) + d->AAddress);
        uint16 p = d->AAddress;
        int rem = count;
        int b = 0;
        count = d->AAddressFixed ? rem : (d->AAddressDecrement ? ((p&MEMMAP_MASK)+1) : (MEMMAP_BLOCK_SIZE-(p&MEMMAP_MASK)));
		bool8 inWRAM_DMA = FALSE;

        if (in_sa1_dma)
        {
            base = &Memory.ROM [CMemory::MAX_ROM_SIZE - 0x10000];
            p = 0;
            count = rem;
        }

        if (in_sdd1_dma)
        {
            base = in_sdd1_dma;
            p = 0;
            count = rem;
        }

        if (spc7110_dma)
        {
            base = spc7110_dma;
            p = 0;
            count = rem;
        }
		
		inWRAM_DMA = ((!in_sa1_dma && !in_sdd1_dma && !spc7110_dma) &&
				 (d->ABank == 0x7e || d->ABank == 0x7f || (!(d->ABank & 0x40) && d->AAddress < 0x2000)));

        while(1){
            if(count>rem) count=rem;
            rem -= count;
            if (inc > 0)
                d->AAddress += count;
            else if (inc < 0)
                d->AAddress -= count;
            //CPU.InWRAM_DMA = (base>=Memory.RAM && base<Memory.RAM+0x20000);
			CPU.InWRAM_DMA = inWRAM_DMA;
			
            if(!base){
                // DMA SLOW PATH
                if (d->TransferMode == 0 || d->TransferMode == 2 || d->TransferMode == 6)
                {
                    do
                    {
                        Work = S9xGetByte((d->ABank << 16) + p);
                        S9xSetPPU (Work, 0x2100 + d->BAddress);
                        p += inc;
                        CHECK_SOUND();
                    } while (--count > 0);
                }
                else if (d->TransferMode == 1 || d->TransferMode == 5)
                {
                    // This is a variation on Duff's Device. It is legal C/C++,
                    // see http://www.lysator.liu.se/c/duffs-device.html
                    switch(b){
                      default:
                        while (count > 1)
                        {
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2100 + d->BAddress);
                            p += inc;
                            count--;

                      case 1:
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2101 + d->BAddress);
                            p += inc;
                            CHECK_SOUND();
                            count --;
                        }
                    }
                    if (count == 1)
                    {
                        Work = S9xGetByte((d->ABank << 16) + p);
                        S9xSetPPU (Work, 0x2100 + d->BAddress);
                        p += inc;
                        b = 1;
                    } else {
                        b = 0;
                    }
                }
                else if (d->TransferMode == 3 || d->TransferMode == 7)
                {
                    switch(b){
                      default:
                        do
                        {
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2100 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 1;
                                break;
                            }

                      case 1:
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2100 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 2;
                                break;
                            }

                      case 2:
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2101 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 3;
                                break;
                            }

                      case 3:
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2101 + d->BAddress);
                            p += inc;
                            CHECK_SOUND();
                            if(--count<=0){
                                b = 0;
                                break;
                            }
                        } while (1);
                    }
                }
                else if (d->TransferMode == 4)
                {
                    switch(b){
                      default:
                        do
                        {
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2100 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 1;
                                break;
                            }

                      case 1:
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2101 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 2;
                                break;
                            }

                      case 2:
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2102 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 3;
                                break;
                            }

                      case 3:
                            Work = S9xGetByte((d->ABank << 16) + p);
                            S9xSetPPU (Work, 0x2103 + d->BAddress);
                            p += inc;
                            CHECK_SOUND();
                            if (--count <= 0){
                                b = 0;
                                break;
                            }
                        } while (1);
                    }
                }
                else
                {
#ifdef DEBUGGER
                    //	    if (Settings.TraceDMA)
                    {
                        sprintf (String, "Unknown DMA transfer mode: %d on channel %d\n",
                                 d->TransferMode, Channel);
                        S9xMessage (S9X_TRACE, S9X_DMA_TRACE, String);
                    }
#endif
                }
            } else {
                // DMA FAST PATH
                if (d->TransferMode == 0 || d->TransferMode == 2 || d->TransferMode == 6)
                {
                    switch (d->BAddress)
                    {
                      case 0x04:
                        do
                        {
                            Work = *(base + p);
                            REGISTER_2104(Work);
                            p += inc;
                            CHECK_SOUND();
                        } while (--count > 0);
                        break;
                      case 0x18:
#ifndef CORRECT_VRAM_READS
                        IPPU.FirstVRAMRead = TRUE;
#endif
                        if (!PPU.VMA.FullGraphicCount)
                        {
                            do
                            {
                                Work = *(base + p);
                                REGISTER_2118_linear(Work);
                                p += inc;
                                CHECK_SOUND();
                            } while (--count > 0);
                        }
                        else
                        {
                            do
                            {
                                Work = *(base + p);
                                REGISTER_2118_tile(Work);
                                p += inc;
                                CHECK_SOUND();
                            } while (--count > 0);
                        }
                        break;
                      case 0x19:
#ifndef CORRECT_VRAM_READS
                        IPPU.FirstVRAMRead = TRUE;
#endif
                        if (!PPU.VMA.FullGraphicCount)
                        {
                            do
                            {
                                Work = *(base + p);
                                REGISTER_2119_linear(Work);
                                p += inc;
                                CHECK_SOUND();
                            } while (--count > 0);
                        }
                        else
                        {
                            do
                            {
                                Work = *(base + p);
                                REGISTER_2119_tile(Work);
                                p += inc;
                                CHECK_SOUND();
                            } while (--count > 0);
                        }
                        break;
                      case 0x22:
                        do
                        {
                            Work = *(base + p);
                            REGISTER_2122(Work);
                            p += inc;
                            CHECK_SOUND();
                        } while (--count > 0);
                        break;
                      case 0x80:
                        if(!CPU.InWRAM_DMA){
                            do
                            {
                                Work = *(base + p);
                                REGISTER_2180(Work);
                                p += inc;
                                CHECK_SOUND();
                            } while (--count > 0);
                        } else {
                            count=0;
                        }
                        break;
                      default:
                        do
                        {
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2100 + d->BAddress);
                            p += inc;
                            CHECK_SOUND();
                        } while (--count > 0);
                        break;
                    }
                }
                else if (d->TransferMode == 1 || d->TransferMode == 5)
                {
                    if (d->BAddress == 0x18)
                    {
                        // Write to V-RAM
#ifndef CORRECT_VRAM_READS
                        IPPU.FirstVRAMRead = TRUE;
#endif
                        if (!PPU.VMA.FullGraphicCount)
                        {
                            switch(b){
                              default:
                                while (count > 1)
                                {
                                    Work = *(base + p);
                                    REGISTER_2118_linear(Work);
                                    p += inc;
                                    count--;

                              case 1:
                                    Work = *(base + p);
                                    REGISTER_2119_linear(Work);
                                    p += inc;
                                    CHECK_SOUND();
                                    count--;
                                }
                            }
                            if (count == 1)
                            {
                                Work = *(base + p);
                                REGISTER_2118_linear(Work);
                                p += inc;
                                b = 1;
                            } else {
                                b = 0;
                            }
                        }
                        else
                        {
                            switch(b){
                              default:
                                while (count > 1)
                                {
                                    Work = *(base + p);
                                    REGISTER_2118_tile(Work);
                                    p += inc;
                                    count--;

                              case 1:
                                    Work = *(base + p);
                                    REGISTER_2119_tile(Work);
                                    p += inc;
                                    CHECK_SOUND();
                                    count--;
                                }
                            }
                            if (count == 1)
                            {
                                Work = *(base + p);
                                REGISTER_2118_tile(Work);
                                p += inc;
                                b = 1;
                            } else {
                                b = 0;
                            }
                        }
                    }
                    else
                    {
                        // DMA mode 1 general case
                        switch(b){
                          default:
                            while (count > 1)
                            {
                                Work = *(base + p);
                                S9xSetPPU (Work, 0x2100 + d->BAddress);
                                p += inc;
                                count--;

                          case 1:
                                Work = *(base + p);
                                S9xSetPPU (Work, 0x2101 + d->BAddress);
                                p += inc;
                                CHECK_SOUND();
                                count --;
                            }
                        }
                        if (count == 1)
                        {
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2100 + d->BAddress);
                            p += inc;
                            b = 1;
                        } else {
                            b = 0;
                        }
                    }
                }
                else if (d->TransferMode == 3 || d->TransferMode == 7)
                {
                    switch(b){
                      default:
                        do
                        {
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2100 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 1;
                                break;
                            }

                      case 1:
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2100 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 2;
                                break;
                            }

                      case 2:
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2101 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 3;
                                break;
                            }

                      case 3:
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2101 + d->BAddress);
                            p += inc;
                            CHECK_SOUND();
                            if(--count<=0){
                                b = 0;
                                break;
                            }
                        } while (1);
                    }
                }
                else if (d->TransferMode == 4)
                {
                    switch(b){
                      default:
                        do
                        {
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2100 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 1;
                                break;
                            }

                      case 1:
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2101 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 2;
                                break;
                            }

                      case 2:
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2102 + d->BAddress);
                            p += inc;
                            if (--count <= 0){
                                b = 3;
                                break;
                            }

                      case 3:
                            Work = *(base + p);
                            S9xSetPPU (Work, 0x2103 + d->BAddress);
                            p += inc;
                            CHECK_SOUND();
                            if (--count <= 0){
                                b = 0;
                                break;
                            }
                        } while (1);
                    }
                }
                else
                {
#ifdef DEBUGGER
                    //	    if (Settings.TraceDMA)
                    {
                        sprintf (String, "Unknown DMA transfer mode: %d on channel %d\n",
                                 d->TransferMode, Channel);
                        S9xMessage (S9X_TRACE, S9X_DMA_TRACE, String);
                    }
#endif
                }
            }
            if(rem<=0) break;
            base = GetBasePointer((d->ABank << 16) + d->AAddress);
            count=MEMMAP_BLOCK_SIZE;
			inWRAM_DMA = ((!in_sa1_dma && !in_sdd1_dma && !spc7110_dma) &&
					 (d->ABank == 0x7e || d->ABank == 0x7f || (!(d->ABank & 0x40) && d->AAddress < 0x2000)));
        }
    }
    else
    {
        if(d->BAddress>0x80-4 && d->BAddress<=0x83 && !(d->ABank&0x40)){
            // REVERSE-DMA REALLY-SLOW PATH
            do
            {
                switch (d->TransferMode)
                {
                  case 0:
                  case 2:
                  case 6:
                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    --count;
                    break;

                  case 1:
                  case 5:
                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2101 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    count--;
                    break;

                  case 3:
                  case 7:
                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2101 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2101 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    count--;
                    break;

                  case 4:
                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2101 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2102 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    CPU.InWRAM_DMA = (d->AAddress<0x2000);
                    Work = S9xGetPPU (0x2103 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    count--;
                    break;

                  default:
#ifdef DEBUGGER
                    if (1) //Settings.TraceDMA)
                    {
                        sprintf (String, "Unknown DMA transfer mode: %d on channel %d\n",
                                 d->TransferMode, Channel);
                        S9xMessage (S9X_TRACE, S9X_DMA_TRACE, String);
                    }
#endif
                    count = 0;
                    break;
                }
                CHECK_SOUND();
            } while (count);
        } else {
            // REVERSE-DMA FASTER PATH
            CPU.InWRAM_DMA = (d->ABank==0x7e || d->ABank==0x7f);
            do
            {
                switch (d->TransferMode)
                {
                  case 0:
                  case 2:
                  case 6:
                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    --count;
                    break;

                  case 1:
                  case 5:
                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    Work = S9xGetPPU (0x2101 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    count--;
                    break;

                  case 3:
                  case 7:
                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    Work = S9xGetPPU (0x2101 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    Work = S9xGetPPU (0x2101 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    count--;
                    break;

                  case 4:
                    Work = S9xGetPPU (0x2100 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    Work = S9xGetPPU (0x2101 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    Work = S9xGetPPU (0x2102 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    if (!--count)
                        break;

                    Work = S9xGetPPU (0x2103 + d->BAddress);
                    S9xSetByte (Work, (d->ABank << 16) + d->AAddress);
                    d->AAddress += inc;
                    count--;
                    break;

                  default:
#ifdef DEBUGGER
                    if (1) //Settings.TraceDMA)
                    {
                        sprintf (String, "Unknown DMA transfer mode: %d on channel %d\n",
                                 d->TransferMode, Channel);
                        S9xMessage (S9X_TRACE, S9X_DMA_TRACE, String);
                    }
#endif
                    count = 0;
                    break;
                }
                CHECK_SOUND();
            } while(count);
        }
    }

    while (CPU.Cycles >= CPU.NextEvent)
        S9xDoHEventProcessing ();	
	if ((CPU.Cycles >= Timings.WRAMRefreshPos) && (CPU.Cycles < (Timings.WRAMRefreshPos + SNES_WRAM_REFRESH_CYCLES)))
	{
		int32	mc = Timings.WRAMRefreshPos + SNES_WRAM_REFRESH_CYCLES - CPU.Cycles;
		S9xCheckMissingHTimerPositionRange(CPU.Cycles, mc); 
		CPU.Cycles += mc;
	}
	
	S9xTrySyncAPUInDMA();
	
    if(Settings.SPC7110&&spc7110_dma)
    {
        if(spc7110_dma&&s7_wrap)
            delete [] spc7110_dma;
    }

update_address:
    d->TransferBytes = 0;

    CPU.InDMA = CPU.InWRAM_DMA = FALSE;
}

static inline bool8 HDMAReadLineCount(int d){
    //remember, InDMA is set.
    //Get/Set incur no charges!
    uint8 line = S9xGetByte ((DMA[d].ABank << 16) + DMA[d].Address);
    CPU.Cycles+=SLOW_ONE_CYCLE;
    if(!line){
        DMA[d].Repeat = FALSE;
        DMA[d].LineCount = 128;
        if(DMA[d].HDMAIndirectAddressing){
            CPU.Cycles+=SLOW_ONE_CYCLE;
            DMA[d].IndirectAddress = S9xGetWord((DMA[d].ABank << 16) + DMA[d].Address);
            DMA[d].Address++;
        }
        DMA[d].Address++;
        HDMAMemPointers[d] = NULL;
        return FALSE;
    } else if (line == 0x80) {
        DMA[d].Repeat = TRUE;
        DMA[d].LineCount = 128;
    } else {
        DMA[d].Repeat = !(line & 0x80);
        DMA[d].LineCount = line & 0x7f;
    }

    DMA[d].Address++;
    DMA[d].DoTransfer = TRUE;
    if (DMA[d].HDMAIndirectAddressing) {
        //again, no cycle charges while InDMA is set!
        CPU.Cycles+=SLOW_ONE_CYCLE<<1;
        DMA[d].IndirectAddress = S9xGetWord ((DMA[d].ABank << 16) + DMA[d].Address);
        DMA[d].Address += 2;
        HDMAMemPointers [d] = S9xGetMemPointer ((DMA[d].IndirectBank << 16) + DMA[d].IndirectAddress);
    } else {
        HDMAMemPointers [d] = S9xGetMemPointer ((DMA[d].ABank << 16) + DMA[d].Address);
    }
    return TRUE;
}

void S9xStartHDMA () {
    if (Settings.DisableHDMA)
        IPPU.HDMA = 0;
    else
        missing.hdma_this_frame = IPPU.HDMA = Memory.FillRAM [0x420c];

	IPPU.HDMAEnded = 0;
    CPU.InDMA = TRUE;
    CPU.InWRAM_DMA = FALSE;

    // XXX: Not quite right...
    if(IPPU.HDMA!=0) CPU.Cycles+=18;

    for (uint8 i = 0; i < 8; i++)
    {
        if (IPPU.HDMA & (1 << i))
        {
            DMA [i].Address = DMA[i].AAddress;
            // Disable H-DMA'ing into V-RAM (register 2118) for Hook
            /* XXX: instead of DMA[i].BAddress == 0x18, make S9xSetPPU fail
             * XXX: writes to $2118/9 when appropriate (leave the
             * !HDMAReadLineCount(i) test though)
             */
            if (!HDMAReadLineCount(i) || DMA[i].BAddress == 0x18) {
                IPPU.HDMA &= ~(1<<i);
            }
			
			S9xTrySyncAPUInDMA();
        } else {
			DMA[i].DoTransfer = FALSE;
        }
    }

    CPU.InDMA = FALSE;
}

#ifdef DEBUGGER
void S9xTraceSoundDSP (const char *s, int i1 = 0, int i2 = 0, int i3 = 0,
					   int i4 = 0, int i5 = 0, int i6 = 0, int i7 = 0);
#endif

uint8 S9xDoHDMA (uint8 byte)
{
    struct SDMA *p = &DMA [0];
    uint32 ShiftedIBank;
    uint16 IAddr;

    int d = 0;

    CPU.InDMA = TRUE;

    // XXX: Not quite right...
    CPU.Cycles+=18;

    for (uint8 mask = 1; mask; mask <<= 1, p++, d++)
    {
        if (byte & mask)
        {
            CPU.InWRAM_DMA = FALSE;
            if (p->HDMAIndirectAddressing) {
                ShiftedIBank = (p->IndirectBank << 16);
                IAddr = p->IndirectAddress;
            } else {
                ShiftedIBank = (p->ABank << 16);
                IAddr = p->Address;
            }
            if (!HDMAMemPointers [d]) {
                HDMAMemPointers [d] = S9xGetMemPointer (ShiftedIBank + IAddr);
            }

            if (p->DoTransfer) {
                // XXX: Hack for Uniracers, because we don't understand
                // OAM Address Invalidation
                if (p->BAddress == 0x04){
                    if(SNESGameFixes.Uniracers){
                        PPU.OAMAddr = 0x10c;
                        PPU.OAMFlip=0;
                    }
                }

#ifdef DEBUGGER
                if (Settings.TraceSoundDSP && p->DoTransfer && 
                    p->BAddress >= 0x40 && p->BAddress <= 0x43)
                    S9xTraceSoundDSP ("Spooling data!!!\n");
                if (Settings.TraceHDMA && p->DoTransfer)
                {
                    sprintf (String, "H-DMA[%d] %s (%d) 0x%06X->0x21%02X %s, Count: %3d, Rep: %s, V-LINE: %3ld %02X%04X",
                             p-DMA, p->TransferDirection? "read" : "write",
                             p->TransferMode, ShiftedIBank+IAddr, p->BAddress,
                             p->HDMAIndirectAddressing ? "ind" : "abs",
                             p->LineCount,
                             p->Repeat ? "yes" : "no ", CPU.V_Counter,
                             p->ABank, p->Address);
                    S9xMessage (S9X_TRACE, S9X_HDMA_TRACE, String);
                }
#endif

                if (!p->TransferDirection) {
                    if((IAddr&MEMMAP_MASK)+HDMA_ModeByteCounts[p->TransferMode]>=MEMMAP_BLOCK_SIZE){
                        // HDMA REALLY-SLOW PATH
                        HDMAMemPointers[d]=NULL;
#define DOBYTE(Addr, RegOff) \
                        CPU.InWRAM_DMA = (ShiftedIBank==0x7e0000 || ShiftedIBank==0x7f0000 || (!(ShiftedIBank&0x400000) && ((uint16)(Addr))<0x2000)); \
                        S9xSetPPU (S9xGetByte(ShiftedIBank + ((uint16)(Addr))), 0x2100 + p->BAddress + RegOff);
                        switch (p->TransferMode) {
                          case 0:
                            CPU.Cycles += SLOW_ONE_CYCLE;
                            DOBYTE(IAddr, 0);
                            break;
                          case 5:
                            CPU.Cycles += 4*SLOW_ONE_CYCLE;
                            DOBYTE(IAddr+0, 0);
                            DOBYTE(IAddr+1, 1);
                            DOBYTE(IAddr+2, 0);
                            DOBYTE(IAddr+3, 1);
                            break;
                          case 1:
                            CPU.Cycles += 2*SLOW_ONE_CYCLE;
                            DOBYTE(IAddr+0, 0);
                            DOBYTE(IAddr+1, 1);
                            break;
                          case 2:
                          case 6:
                            CPU.Cycles += 2*SLOW_ONE_CYCLE;
                            DOBYTE(IAddr+0, 0);
                            DOBYTE(IAddr+1, 0);
                            break;
                          case 3:
                          case 7:
                            CPU.Cycles += 4*SLOW_ONE_CYCLE;
                            DOBYTE(IAddr+0, 0);
                            DOBYTE(IAddr+1, 0);
                            DOBYTE(IAddr+2, 1);
                            DOBYTE(IAddr+3, 1);
                            break;
                          case 4:
                            CPU.Cycles += 4*SLOW_ONE_CYCLE;
                            DOBYTE(IAddr+0, 0);
                            DOBYTE(IAddr+1, 1);
                            DOBYTE(IAddr+2, 2);
                            DOBYTE(IAddr+3, 3);
                            break;
                        }
#undef DOBYTE
                    } else {
                        CPU.InWRAM_DMA = (ShiftedIBank==0x7e0000 || ShiftedIBank==0x7f0000 || (!(ShiftedIBank&0x400000) && IAddr<0x2000));
                        if(!HDMAMemPointers[d]){
                            // HDMA SLOW PATH
                            uint32 Addr = ShiftedIBank + IAddr;
                            switch (p->TransferMode) {
                              case 0:
                                CPU.Cycles += SLOW_ONE_CYCLE;
                                S9xSetPPU (S9xGetByte(Addr), 0x2100 + p->BAddress);
                                break;
                              case 5:
                                CPU.Cycles += 2*SLOW_ONE_CYCLE;
                                S9xSetPPU (S9xGetByte(Addr+0), 0x2100 + p->BAddress);
                                S9xSetPPU (S9xGetByte(Addr+1), 0x2101 + p->BAddress);
                                Addr+=2;
                                /* fall through */
                              case 1:
                                CPU.Cycles += 2*SLOW_ONE_CYCLE;
                                S9xSetPPU (S9xGetByte(Addr+0), 0x2100 + p->BAddress);
                                S9xSetPPU (S9xGetByte(Addr+1), 0x2101 + p->BAddress);
                                break;
                              case 2:
                              case 6:
                                CPU.Cycles += 2*SLOW_ONE_CYCLE;
                                S9xSetPPU (S9xGetByte(Addr+0), 0x2100 + p->BAddress);
                                S9xSetPPU (S9xGetByte(Addr+1), 0x2100 + p->BAddress);
                                break;
                              case 3:
                              case 7:
                                CPU.Cycles += 4*SLOW_ONE_CYCLE;
                                S9xSetPPU (S9xGetByte(Addr+0), 0x2100 + p->BAddress);
                                S9xSetPPU (S9xGetByte(Addr+1), 0x2100 + p->BAddress);
                                S9xSetPPU (S9xGetByte(Addr+2), 0x2101 + p->BAddress);
                                S9xSetPPU (S9xGetByte(Addr+3), 0x2101 + p->BAddress);
                                break;
                              case 4:
                                CPU.Cycles += 4*SLOW_ONE_CYCLE;
                                S9xSetPPU (S9xGetByte(Addr+0), 0x2100 + p->BAddress);
                                S9xSetPPU (S9xGetByte(Addr+1), 0x2101 + p->BAddress);
                                S9xSetPPU (S9xGetByte(Addr+2), 0x2102 + p->BAddress);
                                S9xSetPPU (S9xGetByte(Addr+3), 0x2103 + p->BAddress);
                                break;
                            }
                        } else {
                            // HDMA FAST PATH
                            switch (p->TransferMode) {
                              case 0:
                                CPU.Cycles += SLOW_ONE_CYCLE;
                                S9xSetPPU (*HDMAMemPointers [d]++, 0x2100 + p->BAddress);
                                break;
                              case 5:
                                CPU.Cycles += 2*SLOW_ONE_CYCLE;
                                S9xSetPPU (*(HDMAMemPointers [d] + 0), 0x2100 + p->BAddress);
                                S9xSetPPU (*(HDMAMemPointers [d] + 1), 0x2101 + p->BAddress);
                                HDMAMemPointers [d] += 2;
                                /* fall through */
                              case 1:
                                CPU.Cycles += 2*SLOW_ONE_CYCLE;
                                S9xSetPPU (*(HDMAMemPointers [d] + 0), 0x2100 + p->BAddress);
                                S9xSetPPU (*(HDMAMemPointers [d] + 1), 0x2101 + p->BAddress);
                                HDMAMemPointers [d] += 2;
                                break;
                              case 2:
                              case 6:
                                CPU.Cycles += 2*SLOW_ONE_CYCLE;
                                S9xSetPPU (*(HDMAMemPointers [d] + 0), 0x2100 + p->BAddress);
                                S9xSetPPU (*(HDMAMemPointers [d] + 1), 0x2100 + p->BAddress);
                                HDMAMemPointers [d] += 2;
                                break;
                              case 3:
                              case 7:
                                CPU.Cycles += 4*SLOW_ONE_CYCLE;
                                S9xSetPPU (*(HDMAMemPointers [d] + 0), 0x2100 + p->BAddress);
                                S9xSetPPU (*(HDMAMemPointers [d] + 1), 0x2100 + p->BAddress);
                                S9xSetPPU (*(HDMAMemPointers [d] + 2), 0x2101 + p->BAddress);
                                S9xSetPPU (*(HDMAMemPointers [d] + 3), 0x2101 + p->BAddress);
                                HDMAMemPointers [d] += 4;
                                break;
                              case 4:
                                CPU.Cycles += 4*SLOW_ONE_CYCLE;
                                S9xSetPPU (*(HDMAMemPointers [d] + 0), 0x2100 + p->BAddress);
                                S9xSetPPU (*(HDMAMemPointers [d] + 1), 0x2101 + p->BAddress);
                                S9xSetPPU (*(HDMAMemPointers [d] + 2), 0x2102 + p->BAddress);
                                S9xSetPPU (*(HDMAMemPointers [d] + 3), 0x2103 + p->BAddress);
                                HDMAMemPointers [d] += 4;
                                break;
                            }
                        }
                    }
                } else {
                    // REVERSE HDMA REALLY-SLOW PATH
                    // anomie says: Since this is apparently never used
                    // (otherwise we would have noticed before now), let's not
                    // bother with faster paths.
                    HDMAMemPointers[d]=NULL;
#define DOBYTE(Addr, RegOff) \
                    CPU.InWRAM_DMA = (ShiftedIBank==0x7e0000 || ShiftedIBank==0x7f0000 || (!(ShiftedIBank&0x400000) && ((uint16)(Addr))<0x2000)); \
                    S9xSetByte (S9xGetPPU(0x2100 + p->BAddress + RegOff), ShiftedIBank + ((uint16)(Addr)));
                    switch (p->TransferMode) {
                      case 0:
                        CPU.Cycles += SLOW_ONE_CYCLE;
                        DOBYTE(IAddr, 0);
                        break;
                      case 5:
                        CPU.Cycles += 4*SLOW_ONE_CYCLE;
                        DOBYTE(IAddr+0, 0);
                        DOBYTE(IAddr+1, 1);
                        DOBYTE(IAddr+2, 0);
                        DOBYTE(IAddr+3, 1);
                        break;
                      case 1:
                        CPU.Cycles += 2*SLOW_ONE_CYCLE;
                        DOBYTE(IAddr+0, 0);
                        DOBYTE(IAddr+1, 1);
                        break;
                      case 2:
                      case 6:
                        CPU.Cycles += 2*SLOW_ONE_CYCLE;
                        DOBYTE(IAddr+0, 0);
                        DOBYTE(IAddr+1, 0);
                        break;
                      case 3:
                      case 7:
                        CPU.Cycles += 4*SLOW_ONE_CYCLE;
                        DOBYTE(IAddr+0, 0);
                        DOBYTE(IAddr+1, 0);
                        DOBYTE(IAddr+2, 1);
                        DOBYTE(IAddr+3, 1);
                        break;
                      case 4:
                        CPU.Cycles += 4*SLOW_ONE_CYCLE;
                        DOBYTE(IAddr+0, 0);
                        DOBYTE(IAddr+1, 1);
                        DOBYTE(IAddr+2, 2);
                        DOBYTE(IAddr+3, 3);
                        break;
                    }
#undef DOBYTE
                }
                if (p->HDMAIndirectAddressing){
                    p->IndirectAddress += HDMA_ModeByteCounts [p->TransferMode];
                } else {
                    p->Address += HDMA_ModeByteCounts [p->TransferMode];
                }
            }

            p->DoTransfer = !p->Repeat;
            if (!--p->LineCount) {
                if (!HDMAReadLineCount(d)) {
                    byte &= ~mask;
                    IPPU.HDMAEnded |= mask;
                    p->DoTransfer = FALSE;
                    continue;
                }
            } else {
                CPU.Cycles += SLOW_ONE_CYCLE;
            }
			
			S9xTrySyncAPUInDMA();
        }
    }
    CPU.InDMA=CPU.InWRAM_DMA=FALSE;
	
    return (byte);
}

void S9xResetDMA () {
    int d;
    for (d = 0; d < 8; d++) {
        DMA[d].TransferDirection = FALSE;
        DMA[d].HDMAIndirectAddressing = FALSE;
        DMA[d].AAddressFixed = TRUE;
        DMA[d].AAddressDecrement = FALSE;
        DMA[d].TransferMode = 7;
        DMA[d].BAddress = 0xff;
        DMA[d].AAddress = 0xffff;
        DMA[d].ABank = 0xff;
        DMA[d].DMACount_Or_HDMAIndirectAddress = 0xffff;
        DMA[d].IndirectBank = 0xff;
        DMA[d].Address = 0xffff;
        DMA[d].Repeat = FALSE;
        DMA[d].LineCount = 0x7f;
        DMA[d].UnknownByte = 0xff;
        DMA[d].DoTransfer = FALSE;
    }
}
