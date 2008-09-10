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




#include "snes9x.h"
#include "65c816.h"
#include "memmap.h"
#include "ppu.h"
#include "apu.h"
#include "cpuexec.h"
#include "sa1.h"

#ifndef S9xSTREAM
#define S9xSTREAM stdout
#endif

#define OFFSET(N,F) \
fprintf (S9xSTREAM, "#define " #N " CPU + %d\n", (int) &((struct SCPUState *) 0)->F);
#define OFFSET2(N,F) \
fprintf (S9xSTREAM, "#define " #N " Registers + %d\n", (int) &((struct SRegisters *) 0)->F);
#define OFFSET3(F) \
fprintf (S9xSTREAM, "#define " #F " Memory + %d\n", (int) &((class CMemory *) 0)->F);
#define OFFSET4(N,F) \
fprintf (S9xSTREAM, "#define " #N " APU + %d\n", (int) &((struct SAPU *) 0)->F);
#define OFFSET5(N,F) \
fprintf (S9xSTREAM, "#define " #N " IAPU + %d\n", (int) &((struct SIAPU *) 0)->F);
#define OFFSET6(N,F) \
fprintf (S9xSTREAM, "#define " #N " ICPU + %d\n", (int) &((struct SICPU *) 0)->F);
#define OFFSET7(N,F) \
fprintf (S9xSTREAM, "#define " #N " Settings + %d\n", (int) &((struct SSettings *) 0)->F);
#define OFFSET8(N, F) \
fprintf (S9xSTREAM, "#define " #N " APURegisters + %d\n", (int) &((struct SAPURegisters *) 0)->F);

#define OFFSET9(N, F) \
fprintf (S9xSTREAM, "#define " #N " PPU + %d\n", (int) &((struct SPPU *) 0)->F);
#define OFFSET10(N, F) \
fprintf (S9xSTREAM, "#define " #N " IPPU + %d\n", (int) &((struct InternalPPU *) 0)->F);
#define OFFSET11(N, F) \
fprintf (S9xSTREAM, "#define " #N " SA1 + %d\n", (int) &((struct SSA1 *) 0)->F);
#define OFFSET12(N, F) \
fprintf (S9xSTREAM, "#define " #N " SA1Registers + %d\n", (int) &((struct SSA1Registers *) 0)->F);

int main (int /*argc*/, char ** /*argv*/)
{
    OFFSET(Flags,Flags)
    OFFSET(BranchSkip,BranchSkip)
    OFFSET(NMIActive,NMIActive)
    OFFSET(IRQActive,IRQActive)
    OFFSET(WaitingForInterrupt,WaitingForInterrupt)
    OFFSET(InDMA,InDMA)
	OFFSET(InWRAM_DMA,InWRAM_DMA)
    OFFSET(WhichEvent,WhichEvent)
    OFFSET(PCBase,PCBase)
    OFFSET(PBPCAtOpcodeStart,PBPCAtOpcodeStart)
    OFFSET(WaitAddress,WaitAddress)
    OFFSET(WaitCounter,WaitCounter)
    OFFSET(Cycles,Cycles)
    OFFSET(NextEvent,NextEvent)
    OFFSET(V_Counter,V_Counter)
    OFFSET(MemSpeed,MemSpeed)
    OFFSET(MemSpeedx2,MemSpeedx2)
    OFFSET(FastROMSpeed,FastROMSpeed)
    OFFSET(AutoSaveTimer,AutoSaveTimer)
    OFFSET(SRAMModified,SRAMModified)
    OFFSET(BRKTriggered,BRKTriggered)
    OFFSET(TriedInterleavedMode2,TriedInterleavedMode2)

    OFFSET2(DB,DB)
    OFFSET2(PP,P.W)
    OFFSET2(PL,P.W)
    fprintf (S9xSTREAM, "#define PH PL + 1\n");
    OFFSET2(AA,A.W)
    OFFSET2(AL,A.W)
    fprintf (S9xSTREAM, "#define AH AL + 1\n");
    OFFSET2(DD,D.W)
    OFFSET2(DL,D.W)
    fprintf (S9xSTREAM, "#define DH DL + 1\n");
    OFFSET2(SS,S.W)
    OFFSET2(SL,S.W)
    fprintf (S9xSTREAM, "#define SH SL + 1\n");
    OFFSET2(XX,X.W)
    OFFSET2(XL,X.W)
    fprintf (S9xSTREAM, "#define XH XL + 1\n");
    OFFSET2(YY,Y.W)
    OFFSET2(YL,Y.W)
    fprintf (S9xSTREAM, "#define YH YL + 1\n");
    OFFSET2(PCR,PC.W.xPC)
    OFFSET2(PB,PC.B.xPB)

    OFFSET3(RAM)
    OFFSET3(ROM)
    OFFSET3(VRAM)
    OFFSET3(SRAM)
    OFFSET3(BWRAM)
    OFFSET3(FillRAM)
    OFFSET3(C4RAM)
    OFFSET3(HiROM)
    OFFSET3(LoROM)
    OFFSET3(SRAMMask)
    OFFSET3(SRAMSize)
    OFFSET3(Map)
    OFFSET3(WriteMap)
    OFFSET3(MemorySpeed)
    OFFSET3(BlockIsRAM)
    OFFSET3(BlockIsROM)
    OFFSET3(ROMFilename)

    OFFSET5(APUPCS,PC)
    OFFSET5(APURAM,RAM)
    OFFSET5(APUExecuting,APUExecuting)
    OFFSET5(APUDirectPage,DirectPage)
    OFFSET5(APUBit,Bit)
    OFFSET5(APUAddress,Address)
    OFFSET5(APUWaitAddress1,WaitAddress1)
    OFFSET5(APUWaitAddress2,WaitAddress2)
    OFFSET5(APUWaitCounter,WaitCounter)
    OFFSET5(APUShadowRAM,ShadowRAM)
    OFFSET5(APUCachedSamples,CachedSamples)
    OFFSET5(APU_Carry,_Carry)
    OFFSET5(APU_Zero,_Zero)
    OFFSET5(APU_Overflow,_Overflow)
    OFFSET5(APUTimerErrorCounter,TimerErrorCounter)
    OFFSET5(NextAPUTimerPos,NextAPUTimerPos)

    OFFSET4(APUCycles,Cycles)
    OFFSET4(APUShowROM,ShowROM)
    OFFSET4(APUFlags,Flags)
    OFFSET4(APUKeyedChannels,KeyedChannels)
    OFFSET4(APUOutPorts,OutPorts)
    OFFSET4(APUDSP,DSP)
    OFFSET4(APUExtraRAM,ExtraRAM)
    OFFSET4(APUTimer,Timer)
    OFFSET4(APUTimerTarget,TimerTarget)
    OFFSET4(APUTimerEnabled,TimerEnabled)
    OFFSET4(TimerValueWritten,TimerValueWritten)

    OFFSET6(CPUSpeed,Speed)
    OFFSET6(CPUOpcodes,S9xOpcodes)
    OFFSET6(_Carry,_Carry)
    OFFSET6(_Zero,_Zero)
    OFFSET6(_Negative,_Negative)
    OFFSET6(_Overflow,_Overflow)
    OFFSET6(ShiftedDB,ShiftedDB)
    OFFSET6(ShiftedPB,ShiftedPB)
    OFFSET6(CPUExecuting,CPUExecuting)
    OFFSET6(Scanline,Scanline)
    OFFSET6(Frame,Frame)

    OFFSET7(APUEnabled,APUEnabled)
    OFFSET7(Shutdown,Shutdown)
    OFFSET7(SoundSkipMethod,SoundSkipMethod)
    OFFSET7(HDMATimingHack,HDMATimingHack)
    OFFSET7(DisableIRQ,DisableIRQ)
    OFFSET7(Paused,Paused)
    OFFSET7(PAL,PAL)
    OFFSET7(SoundSync,SoundSync)
    OFFSET7(SA1Enabled,SA1)
    OFFSET7(SuperFXEnabled,SuperFX)

    OFFSET8(ApuP,P)
    OFFSET8(ApuYA,YA.W)
    OFFSET8(ApuA,YA.B.A)
    OFFSET8(ApuY,YA.B.Y)
    OFFSET8(ApuX,X)
    OFFSET8(ApuS,S)
    OFFSET8(ApuPC,PC)
    OFFSET8(APUPCR,PC)

    OFFSET9(BGMode,BGMode)
    OFFSET9(BG3Priority,BG3Priority)
    OFFSET9(Brightness,Brightness)
    OFFSET9(GHight,VMA.High)
    OFFSET9(GInc,VMA.Increment)
    OFFSET9(GAddress,VMA.Address)
    OFFSET9(GMask1,VMA.Mask1)
    OFFSET9(GFullGraphicCount,VMA.FullGraphicCount)
    OFFSET9(GShift,VMA.Shift)
    OFFSET9(CGFLIP,CGFLIP)
    OFFSET9(CGDATA,CGDATA)
    OFFSET9(FirstSprite,FirstSprite)
    OFFSET9(LastSprite,LastSprite)
    OFFSET9(OBJ,OBJ)
    OFFSET9(OAMPriorityRotation,OAMPriorityRotation)
    OFFSET9(OAMAddr,OAMAddr)
    OFFSET9(OAMFlip,OAMFlip)
    OFFSET9(OAMTileAddress,OAMTileAddress)
    OFFSET9(IRQVBeamPos,IRQVBeamPos)
    OFFSET9(IRQHBeamPos,IRQHBeamPos)
    OFFSET9(VBeamPosLatched,VBeamPosLatched)
    OFFSET9(HBeamPosLatched,HBeamPosLatched)
    OFFSET9(HBeamFlip,HBeamFlip)
    OFFSET9(VBeamFlip,VBeamFlip)
    OFFSET9(HVBeamCounterLatched,HVBeamCounterLatched)
    OFFSET9(MatrixA,MatrixA)
    OFFSET9(MatrixB,MatrixB)
    OFFSET9(MatrixC,MatrixC)
    OFFSET9(MatrixD,MatrixD)
    OFFSET9(CentreX,CentreX)
    OFFSET9(CentreY,CentreY)
    OFFSET9(CGADD,CGADD)
    OFFSET9(FixedColourGreen,FixedColourGreen)
    OFFSET9(FixedColourRed,FixedColourRed)
    OFFSET9(FixedColourBlue,FixedColourBlue)
    OFFSET9(SavedOAMAddr,SavedOAMAddr)
    OFFSET9(ScreenHeight,ScreenHeight)
    OFFSET9(WRAM,WRAM)
    OFFSET9(BG_Forced,BG_Forced)
    OFFSET9(ForcedBlanking,ForcedBlanking)
    OFFSET9(OBJThroughMain,OBJThroughMain)
    OFFSET9(OBJThroughSub,OBJThroughSub)
    OFFSET9(OBJSizeSelect,OBJSizeSelect)
    OFFSET9(OBJNameBase,OBJNameBase)
    OFFSET9(OAMReadFlip,OAMReadFlip)
    OFFSET9(OAMData,OAMData)
    OFFSET9(VTimerEnabled,VTimerEnabled)
    OFFSET9(HTimerEnabled,HTimerEnabled)
    OFFSET9(HTimerPosition,HTimerPosition)
    OFFSET9(Mosaic,Mosaic)
    OFFSET9(BGMosaic,BGMosaic)
    OFFSET9(Mode7HFlip,Mode7HFlip)
    OFFSET9(Mode7VFlip,Mode7VFlip)
    OFFSET9(Mode7Repeat,Mode7Repeat)
    OFFSET9(Window1Left,Window1Left)
    OFFSET9(Window1Right,Window1Right)
    OFFSET9(Window2Left,Window2Left)
    OFFSET9(Window2Right,Window2Right)
    OFFSET9(ClipWindowOverlapLogic,ClipWindowOverlapLogic)
    OFFSET9(ClipWindow1Enable,ClipWindow1Enable)
    OFFSET9(ClipWindow2Enable,ClipWindow2Enable)
    OFFSET9(ClipWindow1Inside,ClipWindow1Inside)
    OFFSET9(ClipWindow2Inside,ClipWindow2Inside)
    OFFSET9(RecomputeClipWindows,RecomputeClipWindows)
    OFFSET9(CGFLIPRead,CGFLIPRead)
    OFFSET9(OBJNameSelect,OBJNameSelect)
    OFFSET9(Need16x8Mulitply,Need16x8Mulitply)
    OFFSET9(RangeTimeOver,RangeTimeOver)

    OFFSET10(ColorsChanged,ColorsChanged)
    OFFSET10(HDMA,HDMA)
    OFFSET10(HDMAStarted,HDMAStarted)
    OFFSET10(MaxBrightness,MaxBrightness)
    OFFSET10(LatchedBlanking,LatchedBlanking)
    OFFSET10(OBJChanged,OBJChanged)
    OFFSET10(RenderThisFrame,RenderThisFrame)
    OFFSET10(SkippedFrames,SkippedFrames)
    OFFSET10(FrameSkip,FrameSkip)
    OFFSET10(TileCache,TileCache)
    OFFSET10(TileCached,TileCached)
#ifdef CORRECT_VRAM_READS
    OFFSET10(VRAMReadBuffer,VRAMReadBuffer)
#else
    OFFSET10(FirstVRAMRead,FirstVRAMRead)
#endif
    OFFSET10(Interlace,Interlace)
    OFFSET10(DoubleWidthPixels,DoubleWidthPixels)
    OFFSET10(RenderedScreenHeight,RenderedScreenHeight)
    OFFSET10(RenderedScreenWidth,RenderedScreenWidth)
    OFFSET10(Red,Red)
    OFFSET10(Green,Green)
    OFFSET10(Blue,Blue)
    OFFSET10(XB,XB)
    OFFSET10(ScreenColors,ScreenColors)
    OFFSET10(PreviousLine,PreviousLine)
    OFFSET10(CurrentLine,CurrentLine)
    OFFSET10(Clip,Clip)

    OFFSET11(SA1Opcodes,S9xOpcodes)
    OFFSET11(SA1_Carry,_Carry)
    OFFSET11(SA1_Zero,_Zero)
    OFFSET11(SA1_Negative,_Negative)
    OFFSET11(SA1_Overflow,_Overflow)
    OFFSET11(SA1CPUExecuting,CPUExecuting)
    OFFSET11(SA1ShiftedPB,ShiftedPB)
    OFFSET11(SA1ShiftedDB,ShiftedDB)
    OFFSET11(SA1Flags,Flags)
    OFFSET11(SA1Executing,Executing)
    OFFSET11(SA1NMIActive,NMIActive)
    OFFSET11(SA1IRQActive,IRQActive)
    OFFSET11(SA1WaitingForInterrupt,WaitingForInterrupt)
    OFFSET11(SA1PCBase,PCBase)
    OFFSET11(SA1PBPCAtOpcodeStart,PBPCAtOpcodeStart)
    OFFSET11(SA1WaitAddress,WaitAddress)
    OFFSET11(SA1WaitCounter,WaitCounter)
    OFFSET11(SA1WaitByteAddress1,WaitByteAddress1)
    OFFSET11(SA1WaitByteAddress2,WaitByteAddress2)
    OFFSET11(SA1BWRAM,BWRAM)
    OFFSET11(SA1Map,Map)
    OFFSET11(SA1WriteMap,WriteMap)
    OFFSET11(SA1op1,op1)
    OFFSET11(SA1op2,op2)
    OFFSET11(SA1arithmetic_op,arithmetic_op)
    OFFSET11(SA1sum,sum)
    OFFSET11(SA1overflow,overflow)
    OFFSET11(VirtualBitmapFormat,VirtualBitmapFormat)
    OFFSET11(SA1_in_char_dma,in_char_dma)
    OFFSET11(SA1variable_bit_pos,variable_bit_pos)

    OFFSET12(SA1DB,DB)
    OFFSET12(SA1PP,P.W)
    OFFSET12(SA1PL,P.W)
    fprintf (S9xSTREAM, "#define SA1PH SA1PL + 1\n");
    OFFSET12(SA1AA,A.W)
    OFFSET12(SA1AL,A.W)
    fprintf (S9xSTREAM, "#define SA1AH SA1AL + 1\n");
    OFFSET12(SA1DD,D.W)
    OFFSET12(SA1DL,D.W)
    fprintf (S9xSTREAM, "#define SA1DH SA1DL + 1\n");
    OFFSET12(SA1SS,S.W)
    OFFSET12(SA1SL,S.W)
    fprintf (S9xSTREAM, "#define SA1SH SA1SL + 1\n");
    OFFSET12(SA1XX,X.W)
    OFFSET12(SA1XL,X.W)
    fprintf (S9xSTREAM, "#define SA1XH SA1XL + 1\n");
    OFFSET12(SA1YY,Y.W)
    OFFSET12(SA1YL,Y.W)
    fprintf (S9xSTREAM, "#define SA1YH SA1YL + 1\n");
    OFFSET12(SA1PB,PC.B.xPB)
    OFFSET12(SA1PCR,PC.W.xPC)

    return (0);
}

