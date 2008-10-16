TinySMB-GC 0.1
May 2006

Developer Documentation

What is TinySMB-GC?

TinySMB-GC is a minimal implementation of the SMB protocol for the Nintendo Gamecube.

What does it do?

Essentially, TinySMB-GC enables network share support. This means that it will allow the
GameCube to connect to a shared folder on a Windows XP or linux samba box, and perform
basic file functions.

Known Restrictions

TinySMB-GC only supports the LM1.2X002 protocol. THIS IS NOT THE MOST SECURE!
However, it does NOT transmit your password over the wire, so it's reasonably safe.

How do I use it?
  
TinySMB-GC is developed with devkitPPC and libOGC, therefore you should be using this
development environment. Put simply, it won't work with anything else!

TinySMB-GC uses the TCP-raw form of SMB on port 445.

The Functions.

int SMB_Init (char *user,     /*** The logon user - MUST have share access rights ***/
              char *password, /*** PLEASE USE PASSWORD SECURITY! ***/     
              char *client,   /*** Machine ID, whatever you want to call the GC ***/
              char *server,   /*** Machine ID of share server ***/
              char *share,    /*** Share ID ***/
              char *IP);      /*** IP of share server ***/

SMB_Init is used to establish the connection, authenticate and attach to the share.
Obviously, this must be called successfully before any other function.

void SMB_Destroy ();

SMB_Destroy takes care of releasing the internal socket of TinySMB-GC and should be
called when the SMB functions are no longer required.

int SMB_FindFirst (char *filename,        /*** The file mask to search for ***/
                   unsigned short flags,  /*** Search criteria flags ***/
                   SMBDIRENTRY * sdir);   /*** An SMBDIRENTRY to hold directory information ***/

Similar to MS-Windows, to search for a file or directory, use this function to determine if the
file already exists. The SMBDIRENTRY simply holds basic information on each entry retrieved.

int SMB_FindNext (SMBDIRENTRY * sdir);

Called to continue a search started with SMB_FindFirst.

int SMB_FindClose ();

When all searches have completed, call SMB_FindClose to dispense with the search handle.

SMBFILE SMB_Open (char *filename,           /*** The filename to open ***/
                  unsigned short access,    /*** Access method ***/
                  unsigned short creation); /*** Creation flags ***/

This call will open a file on the share. Both reading and writing are supported.
Look at smb.h for information on the access and creation flags.

void SMB_Close (SMBFILE sfid);

Close a file previously opened with SMB_Open

int SMB_Read (char *buffer, int size, int offset, SMBFILE sfile);

Read from the file opened with SMB_Open.

int SMB_Write (char *buffer, int size, int offset, SMBFILE sfile);                                    

Write to the file opened with SMB_Open.

NOTE: The offset value provides the missing seek function. However, the onus
is on the developer to maintain the offset value when reading / writing 
sequentially.

You should also be aware that these functions should only be used to read/write
blocks up to 62Kbytes. Although it allows reading and writing of 4Gb files, it
does not support blocks larger then 16-bit - go figure!

Credits

TinySMB-GC      Copyright softdev@tehskeen.com
                Please respect this copyright!
                NOTE WELL: This software is released under GPL 2.1
                           YOU MAY NOT STEAL IT AND HIDE IT IN YOUR
                           CLOSED SOURCE PRODUCT.
                                
libOGC          shagkur

devkitPPC       wntrmute

CIFS Info       Christopher R Hertel
                http://www.ubiqx.com
                Storage Networking Industry Association 
                http://www.snia.com
                Ethereal - Packet Capture
                http://www.ethereal.com

Thanks

Cedy_NL, for testing and helping get this off the ground.
brakken, web hosting and promotion.

Everyone who has participated in the Genesis Plus Project - keep up the good work !
                