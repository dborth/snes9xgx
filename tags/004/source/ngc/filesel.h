/****************************************************************************
 * Snes9x 1.50 
 *
 * Nintendo Gamecube Filesel - borrowed from GPP
 *
 * softdev July 2006
 * crunchy2 May 2007
 ****************************************************************************/

#ifndef _NGCFILESEL_
#define _NGCFILESEL_

int OpenDVD ();
int OpenSMB ();
int OpenSD ();
int OpenUSB ();

#define LOAD_DVD 1
#define LOAD_SMB 2
#define LOAD_SDC 4
#define LOAD_USB 8
#define ROOTSDDIR "fat3:/"
#define SNESROMDIR "snes9x/roms"
#define SNESSAVEDIR "snes9x/saves"

#endif
