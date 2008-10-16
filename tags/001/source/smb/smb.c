/****************************************************************************
 * TinySMB-GC
 *
 * Nintendo Gamecube SaMBa implementation.
 *
 * Copyright softdev@tehskeen.com
 *
 * Authentication modules, LMhash and DES are 
 *
 * Copyright Christopher R Hertel.
 * http://www.ubiqx.org
 *
 * You WILL find Ethereal, available from http://www.ethereal.com
 * invaluable for debugging each new SAMBA implementation.
 *
 * Recommended Reading
 *	Implementing CIFS - Christopher R Hertel
 *	SNIA CIFS Documentation - http://www.snia.org
 *
 * License:
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************************/

#include <gccore.h>
#include <network.h>
#include "smb.h"
#include "DES.h"
#include "LMhash.h"
#include <ctype.h>

/**
 * A few little bits for libOGC
 */
typedef int SOCKET;
#define recv net_recv
#define send net_send
#define closesocket net_close
#define connect net_connect
#define setsockopt net_setsockopt
#define socket net_socket

/**
 * Client and server SMB 
 */
NBTSMB c;
NBTSMB s;
SMBSESSION session;
SOCKET smbsock;
static struct sockaddr_in smbs;
static int smbcheckbytes = 0;

/**
 * SMB Endian aware supporting functions 
 *
 * SMB always uses Intel Little-Endian values, so htons etc are
 * of little or no use :) ... Thanks M$
 */
/*** get unsigned char ***/
static unsigned char
getUChar (unsigned char *buffer, int offset)
{
  return buffer[offset];
}

/*** set unsigned char ***/
static void
setUChar (unsigned char *buffer, int offset, unsigned char value)
{
  buffer[offset] = value;
}

/*** get unsigned short ***/
static unsigned short
getUShort (unsigned char *buffer, int offset)
{
  unsigned short t;
  t = buffer[offset];
  t |= (buffer[offset + 1] << 8);

  return t;
}

/*** set unsigned short ***/
static void
setUShort (unsigned char *buffer, int offset, unsigned short value)
{
  buffer[offset] = value & 0xff;
  buffer[offset + 1] = (value & 0xff00) >> 8;
}

/*** get unsigned int ***/
static unsigned int
getUInt (unsigned char *buffer, int offset)
{
  unsigned int t;

  t = buffer[offset];
  t |= (buffer[offset + 1] << 8);
  t |= (buffer[offset + 2] << 16);
  t |= (buffer[offset + 3] << 24);

  return t;
}

/*** set unsigned int ***/
static void
setUInt (unsigned char *buffer, int offset, int value)
{
  buffer[offset] = value & 0xff;
  buffer[offset + 1] = (value & 0xff00) >> 8;
  buffer[offset + 2] = (value & 0xff0000) >> 16;
  buffer[offset + 3] = (value & 0xff000000) >> 24;
}

/**
 * MakeSMBHdr
 *
 * Generate the SMB header for each request.
 * Uses 'c' NBTSMB
 */
static void
MakeSMBHdr (unsigned char command)
{
  int pos = 0;

     /*** Clear client packet ***/
  memset (&c, 0, sizeof (c));

     /*** Add protocol SMB ***/
  setUInt (c.smb, pos, SMB_PROTO);
  pos += 4;
  setUChar (c.smb, pos, command);
  pos++;
  pos++;    /*** Error class ***/
  pos++;    /*** Reserved ***/
  pos += 2;  /*** Error Code ***/
  setUChar (c.smb, pos, 0x8);
  pos++;				    /*** Flags == 8 == Case Insensitive ***/
  setUShort (c.smb, pos, 0x1);
  pos += 2;				       /*** Flags2 == 1 == LFN ***/
  pos += 2;  /*** Process ID High ***/
  pos += 8;  /*** Signature ***/
  pos += 2;  /*** Reserved ***/
  setUShort (c.smb, pos, session.TID);
  pos += 2;
  setUShort (c.smb, pos, session.PID);
  pos += 2;
  setUShort (c.smb, pos, session.UID);
  pos += 2;
  setUShort (c.smb, pos, session.MID);
}

/**
 * MakeTRANS2Hdr
 */
static void
MakeTRANS2Hdr (unsigned char subcommand)
{
  setUChar (c.smb, T2_WORD_CNT, 15);
  setUShort (c.smb, T2_MAXPRM_CNT, 10);
  setUShort (c.smb, T2_MAXBUFFER, session.MaxBuffer);
  setUChar (c.smb, T2_SSETUP_CNT, 1);
  setUShort (c.smb, T2_SUB_CMD, subcommand);
}

/**
 * SMBCheck
 *
 * Do very basic checking on the return SMB
 */
static int
SMBCheck (unsigned char command, int readlen)
{
  int ret;

  memset (&s, 0, sizeof (s));

  smbcheckbytes = ret = recv (smbsock, (char *) &s, readlen, 0);

  if (ret < 12)
    return 0;

    /*** Do basic SMB Header checks ***/
  ret = getUInt (s.smb, 0);
  if (ret != SMB_PROTO)
    return BAD_PROTOCOL;

  if (getUChar (s.smb, 4) != command)
    return SMB_BAD_COMMAND;

  ret = getUInt (s.smb, SMB_OFFSET_NTSTATUS);

  if (ret)
    return SMB_ERROR;

  return SMB_SUCCESS;

}

/**
 * SMB_NegotiateProtocol
 *
 * The only protocol we admit to is 'DOS LANMAN 2.1'
 */
static int
SMB_NegotiateProtocol ()
{
  int pos;
  int bcpos;
  int ret;
  char proto[] = "\2LM1.2X002";	       /*** Seems to work with Samba and XP Home ***/
  unsigned short bytecount;

    /*** Clear session variables ***/
  memset (&session, 0, sizeof (session));
  session.PID = 0xdead;
  session.MID = 1;

  MakeSMBHdr (SMB_NEG_PROTOCOL);
  pos = SMB_HEADER;

  pos++;   /*** Add word count ***/
  bcpos = pos;
  pos += 2; /*** Byte count - when known ***/

  strcpy (&c.smb[pos], proto);

  pos += strlen (proto) + 1;

    /*** Update byte count ***/
  setUShort (c.smb, bcpos, (pos - bcpos) - 2);

    /*** Set NBT information ***/
  c.msg = SESS_MSG;
  c.length = htons (pos);

  pos += 4;
  ret = send (smbsock, (char *) &c, pos, 0);

    /*** Check response ***/
  if (SMBCheck (SMB_NEG_PROTOCOL, sizeof (s)) == SMB_SUCCESS)
    {
      pos = SMB_HEADER;
	 /*** Collect information ***/
      if (getUChar (s.smb, pos) != 13)
	return SMB_PROTO_FAIL;

      pos++;
      if (getUShort (s.smb, pos))
	return SMB_PROTO_FAIL;

      pos += 2;
      if (getUShort (s.smb, pos) != 3)
	return SMB_NOT_USER;

      pos += 2;
      session.MaxBuffer = getUShort (s.smb, pos);
      pos += 2;
      if (session.MaxBuffer > 2916)
	session.MaxBuffer = 2916;
      session.MaxMpx = getUShort (s.smb, pos);
      pos += 2;
      session.MaxVCS = getUShort (s.smb, pos);
      pos += 2;
      pos += 2;	 /*** Raw Mode ***/
      pos += 4;	 /*** Session Key ***/
      pos += 6;	 /*** Time information ***/
      if (getUShort (s.smb, pos) != 8)
	return SMB_BAD_KEYLEN;

      pos += 2;
      pos += 2;	 /*** Reserved ***/

      bytecount = getUShort (s.smb, pos);
      pos += 2;

	 /*** Copy challenge key ***/
      memcpy (&session.challenge, &s.smb[pos], 8);
      pos += 8;
	 /*** Primary domain ***/
      strcpy (session.p_domain, &s.smb[pos]);

      return SMB_SUCCESS;
    }

  return 0;

}

/**
 * SMB_SetupAndX
 *
 * Setup the SMB session, including authentication with the 
 * magic 'LM Response'
 */
static int
SMB_SetupAndX (char *user, char *password)
{
  int pos;
  int bcpos;
  char pwd[24], LMh[24], LMr[24];
  int i, ret;

  MakeSMBHdr (SMB_SETUP_ANDX);
  pos = SMB_HEADER;

  setUChar (c.smb, pos, 10);
  pos++;				    /*** Word Count ***/
  setUChar (c.smb, pos, 0xff);
  pos++;				      /*** Next AndX ***/
  pos++;   /*** Reserved ***/
  pos += 2; /*** Next AndX Offset ***/
  setUShort (c.smb, pos, session.MaxBuffer);
  pos += 2;
  setUShort (c.smb, pos, session.MaxMpx);
  pos += 2;
  setUShort (c.smb, pos, session.MaxVCS);
  pos += 2;
  pos += 4; /*** Session key, unknown at this point ***/
  setUShort (c.smb, pos, 24);
  pos += 2;				 /*** Password length ***/
  pos += 4; /*** Reserved ***/
  bcpos = pos;
  pos += 2; /*** Byte count ***/

    /*** The magic 'LM Response' ***/
  strcpy (pwd, password);
  for (i = 0; i < strlen (pwd); i++)
    pwd[i] = toupper (pwd[i]);

  auth_LMhash (LMh, pwd, strlen (pwd));
  auth_LMresponse (LMr, LMh, session.challenge);

    /*** Build information ***/
  memcpy (&c.smb[pos], LMr, 24);
  pos += 24;

    /*** Account ***/
  strcpy (pwd, user);
  for (i = 0; i < strlen (user); i++)
    pwd[i] = toupper (pwd[i]);
  memcpy (&c.smb[pos], pwd, strlen (pwd));
  pos += strlen (pwd) + 1;

    /*** Primary Domain ***/
  memcpy (&c.smb[pos], &session.p_domain, strlen (session.p_domain));
  pos += strlen (session.p_domain) + 1;

    /*** Native OS ***/
  strcpy (pwd, "Unix (libOGC)");
  memcpy (&c.smb[pos], pwd, strlen (pwd));
  pos += strlen (pwd) + 1;

    /*** Native LAN Manager ***/
  strcpy (pwd, "Nintendo GameCube 0.1");
  memcpy (&c.smb[pos], pwd, strlen (pwd));
  pos += strlen (pwd) + 1;

    /*** Update byte count ***/
  setUShort (c.smb, bcpos, (pos - bcpos) - 2);

  c.msg = SESS_MSG;
  c.length = htons (pos);
  pos += 4;

  ret = send (smbsock, (char *) &c, pos, 0);

  if (SMBCheck (SMB_SETUP_ANDX, sizeof (s)) == SMB_SUCCESS)
    {
	 /*** Collect UID ***/
      session.UID = getUShort (s.smb, SMB_OFFSET_UID);

      return SMB_SUCCESS;
    }

  return 0;
}

/**
 * SMB_TreeAndX
 *
 * Finally, connect to the remote share
 */
static int
SMB_TreeAndX (char *server, char *share)
{
  int pos, bcpos, ret;
  char path[256];

  MakeSMBHdr (SMB_TREEC_ANDX);
  pos = SMB_HEADER;

  setUChar (c.smb, pos, 4);
  pos++;				    /*** Word Count ***/
  setUChar (c.smb, pos, 0xff);
  pos++;				    /*** Next AndX ***/
  pos++;   /*** Reserved ***/
  pos += 2; /*** Next AndX Offset ***/
  pos += 2; /*** Flags ***/
  setUShort (c.smb, pos, 1);
  pos += 2;				    /*** Password Length ***/
  bcpos = pos;
  pos += 2;
  pos++;    /*** NULL Password ***/

    /*** Build server share path ***/
  strcpy (path, "\\\\");
  strcat (path, server);
  strcat (path, "\\");
  strcat (path, share);
  for (ret = 0; ret < strlen (path); ret++)
    path[ret] = toupper (path[ret]);

  memcpy (&c.smb[pos], path, strlen (path));
  pos += strlen (path) + 1;

    /*** Service ***/
  strcpy (path, "?????");
  memcpy (&c.smb[pos], path, strlen (path));
  pos += strlen (path) + 1;

    /*** Update byte count ***/
  setUShort (c.smb, bcpos, (pos - bcpos) - 2);

  c.msg = SESS_MSG;
  c.length = htons (pos);
  pos += 4;

  ret = send (smbsock, (char *) &c, pos, 0);

  if (SMBCheck (SMB_TREEC_ANDX, sizeof (s)) == SMB_SUCCESS)
    {
	 /*** Collect Tree ID ***/
      session.TID = getUShort (s.smb, SMB_OFFSET_TID);
      return SMB_SUCCESS;
    }

  return 0;
}

/**
 * SMB_FindFirst
 *
 * Uses TRANS2 to support long filenames
 */
int
SMB_FindFirst (char *filename, unsigned short flags, SMBDIRENTRY * sdir)
{
  int pos;
  int ret;
  int bpos;

  MakeSMBHdr (SMB_TRANS2);
  MakeTRANS2Hdr (SMB_FIND_FIRST2);
  pos = T2_BYTE_CNT + 2;
  bpos = pos;
  pos += 3;	     /*** Padding ***/

  setUShort (c.smb, pos, flags);
  pos += 2;					  /*** Flags ***/
  setUShort (c.smb, pos, 1);
  pos += 2;					  /*** Count ***/
  setUShort (c.smb, pos, 6);
  pos += 2;					  /*** Internal Flags ***/
  setUShort (c.smb, pos, 260);
  pos += 2;					  /*** Level of Interest ***/
  pos += 4;    /*** Storage Type == 0 ***/
  memcpy (&c.smb[pos], filename, strlen (filename));
  pos += strlen (filename) + 1;			   /*** Include padding ***/

    /*** Update counts ***/
  setUShort (c.smb, T2_PRM_CNT, 13 + strlen (filename));
  setUShort (c.smb, T2_SPRM_CNT, 13 + strlen (filename));
  setUShort (c.smb, T2_SPRM_OFS, 68);
  setUShort (c.smb, T2_SDATA_OFS, 81 + strlen (filename));
  setUShort (c.smb, T2_BYTE_CNT, pos - bpos);

  c.msg = SESS_MSG;
  c.length = htons (pos);

  pos += 4;

  ret = send (smbsock, (char *) &c, pos, 0);
  session.sid = 0;
  session.count = 0;
  session.eos = 1;

  if (SMBCheck (SMB_TRANS2, sizeof (s)) == SMB_SUCCESS)
    {
	 /*** Get parameter offset ***/
      pos = getUShort (s.smb, SMB_HEADER + 9);
      session.sid = getUShort (s.smb, pos);
      pos += 2;
      session.count = getUShort (s.smb, pos);
      pos += 2;
      session.eos = getUShort (s.smb, pos);
      pos += 2;
      pos += 46;

      if (session.count)
	{
	  sdir->size_low = getUInt (s.smb, pos);
	  pos += 4;
	  sdir->size_high = getUInt (s.smb, pos);
	  pos += 4;
	  pos += 8;
	  sdir->attributes = getUInt (s.smb, pos);
	  pos += 38;
	  strcpy (sdir->name, &s.smb[pos]);

	  return SMB_SUCCESS;
	}
    }

  return 0;

}

/**
 * SMB_FindNext
 */
int
SMB_FindNext (SMBDIRENTRY * sdir)
{
  int pos;
  int ret;
  int bpos;

  if (session.eos)
    return 0;

  if (session.sid == 0)
    return 0;

  MakeSMBHdr (SMB_TRANS2);
  MakeTRANS2Hdr (SMB_FIND_NEXT2);
  pos = T2_BYTE_CNT + 2;
  bpos = pos;
  pos += 3;	     /*** Padding ***/

  setUShort (c.smb, pos, session.sid);
  pos += 2;						    /*** Search ID ***/
  setUShort (c.smb, pos, 1);
  pos += 2;					  /*** Count ***/
  setUShort (c.smb, pos, 260);
  pos += 2;					  /*** Level of Interest ***/
  pos += 4;    /*** Storage Type == 0 ***/
  setUShort (c.smb, pos, 12);
  pos += 2;			     /*** Search flags ***/
  pos++;

    /*** Update counts ***/
  setUShort (c.smb, T2_PRM_CNT, 13);
  setUShort (c.smb, T2_SPRM_CNT, 13);
  setUShort (c.smb, T2_SPRM_OFS, 68);
  setUShort (c.smb, T2_SDATA_OFS, 81);
  setUShort (c.smb, T2_BYTE_CNT, pos - bpos);

  c.msg = SESS_MSG;
  c.length = htons (pos);

  pos += 4;

  ret = send (smbsock, (char *) &c, pos, 0);

  if (SMBCheck (SMB_TRANS2, sizeof (s)) == SMB_SUCCESS)
    {
	 /*** Get parameter offset ***/
      pos = getUShort (s.smb, SMB_HEADER + 9);
      session.count = getUShort (s.smb, pos);
      pos += 2;
      session.eos = getUShort (s.smb, pos);
      pos += 2;
      pos += 44;

      if (session.count)
	{
	  sdir->size_low = getUInt (s.smb, pos);
	  pos += 4;
	  sdir->size_high = getUInt (s.smb, pos);
	  pos += 4;
	  pos += 8;
	  sdir->attributes = getUInt (s.smb, pos);
	  pos += 38;
	  strcpy (sdir->name, &s.smb[pos]);

	  return SMB_SUCCESS;
	}
    }

  return 0;

}

/**
 * SMB_FindClose
 */
int
SMB_FindClose ()
{
  int pos = SMB_HEADER;
  int ret;

  if (session.sid == 0)
    return 0;

  MakeSMBHdr (SMB_FIND_CLOSE2);

  setUChar (c.smb, pos, 1);
  pos++;					   /*** Word Count ***/
  setUShort (c.smb, pos, session.sid);
  pos += 2;
  pos += 2;  /*** Byte Count ***/

  c.msg = SESS_MSG;
  c.length = htons (pos);
  pos += 4;
  ret = send (smbsock, (char *) &c, pos, 0);

  return SMBCheck (SMB_FIND_CLOSE2, sizeof (s));

}

/**
 * SMB_Open
 */
SMBFILE
SMB_Open (char *filename, unsigned short access, unsigned short creation)
{
  int pos = SMB_HEADER;
  int bpos, ret;
  char realfile[256];
  unsigned short fid;

  MakeSMBHdr (SMB_OPEN_ANDX);

  setUChar (c.smb, pos, 15);
  pos++;			       /*** Word Count ***/
  setUChar (c.smb, pos, 0xff);
  pos++;				 /*** Next AndX ***/
  pos += 3;  /*** Next AndX Offset ***/

  pos += 2;  /*** Flags ***/
  setUShort (c.smb, pos, access);
  pos += 2;					 /*** Access mode ***/
  setUShort (c.smb, pos, 0x6);
  pos += 2;				       /*** Type of file ***/
  pos += 2;  /*** Attributes ***/
  pos += 4;  /*** File time - don't care - let server decide ***/
  setUShort (c.smb, pos, creation);
  pos += 2;				       /*** Creation flags ***/
  pos += 4;  /*** Allocation size ***/
  pos += 8;  /*** Reserved ***/
  pos += 2;  /*** Byte Count ***/
  bpos = pos;

  if (filename[0] != '\\')
    {
      strcpy (realfile, "\\");
      strcat (realfile, filename);
    }
  else
    strcpy (realfile, filename);

  memcpy (&c.smb[pos], realfile, strlen (realfile));
  pos += strlen (realfile) + 1;

  setUShort (c.smb, bpos - 2, (pos - bpos));

  c.msg = SESS_MSG;
  c.length = htons (pos);

  pos += 4;
  ret = send (smbsock, (char *) &c, pos, 0);

  if (SMBCheck (SMB_OPEN_ANDX, sizeof (s)) == SMB_SUCCESS)
    {
	/*** Check file handle ***/
      fid = getUShort (s.smb, SMB_HEADER + 5);

      if (fid)
	return fid;
    }

  return 0;
}

/**
 * SMB_Close
 */
void
SMB_Close (SMBFILE sfid)
{
  int pos, ret;

  MakeSMBHdr (SMB_CLOSE);
  pos = SMB_HEADER;

  setUChar (c.smb, pos, 3);
  pos++;			      /** Word Count **/
  setUShort (c.smb, pos, sfid);
  pos += 2;
  setUInt (c.smb, pos, 0xffffffff);
  pos += 4;					/*** Last Write ***/
  pos += 2;  /*** Byte Count ***/

  c.msg = SESS_MSG;
  c.length = htons (pos);
  pos += 4;
  ret = send (smbsock, (char *) &c, pos, 0);

  SMBCheck (SMB_CLOSE, sizeof (s));
}

/**
 * SMB_Read
 */
int
SMB_Read (char *buffer, int size, int offset, SMBFILE sfile)
{
  int pos, ret, ofs;
  unsigned short length = 0;

  MakeSMBHdr (SMB_READ_ANDX);
  pos = SMB_HEADER;

	/*** Don't let the size exceed! ***/
  if (size > 62 * 1024)
    return 0;

  setUChar (c.smb, pos, 10);
  pos++;				      /*** Word count ***/
  setUChar (c.smb, pos, 0xff);
  pos++;
  pos += 3;	    /*** Reserved, Next AndX Offset ***/
  setUShort (c.smb, pos, sfile);
  pos += 2;					    /*** FID ***/
  setUInt (c.smb, pos, offset);
  pos += 4;						 /*** Offset ***/

  setUShort (c.smb, pos, size & 0xffff);
  pos += 2;
  setUShort (c.smb, pos, size & 0xffff);
  pos += 2;
  setUInt (c.smb, pos, 0);
  pos += 4;
  pos += 2;	    /*** Remaining ***/
  pos += 2;	    /*** Byte count ***/

  c.msg = SESS_MSG;
  c.length = htons (pos);

  pos += 4;
  ret = send (smbsock, (char *) &c, pos, 0);

	/*** SMBCheck should now only read up to the end of a standard header ***/
  if (SMBCheck (SMB_READ_ANDX, SMB_HEADER + 27 + 4) == SMB_SUCCESS)
    {
		/*** Retrieve data length for this packet ***/
      length = getUShort (s.smb, SMB_HEADER + 11);
		/*** Retrieve offset to data ***/
      ofs = getUShort (s.smb, SMB_HEADER + 13);

		/*** Default offset, with no padding is 59, so grab any outstanding padding ***/
      if (ofs > 59)
	{
	  char pad[1024];
	  ret = recv (smbsock, pad, ofs - 59, 0);
	}

		/*** Finally, go grab the data ***/
      ofs = 0;

      if (length)
	{
	  while (((ret = recv (smbsock, buffer + ofs, length, 0)) > 0))
	    {
	      ofs += ret;
	      if (ofs == length)
		break;
	    }
	}

      return ofs;

    }

  return 0;
    
}

/**
 * SMB_Write
 */
int
SMB_Write (char *buffer, int size, int offset, SMBFILE sfile)
{
  int pos, ret;
  int blocks64;

  MakeSMBHdr (SMB_WRITE_ANDX);
  pos = SMB_HEADER;

  setUChar (c.smb, pos, 12);
  pos++;				  /*** Word Count ***/
  setUChar (c.smb, pos, 0xff);
  pos += 2;				   /*** Next AndX ***/
  pos += 2; /*** Next AndX Offset ***/

  setUShort (c.smb, pos, sfile);
  pos += 2;
  setUInt (c.smb, pos, offset);
  pos += 4;
  pos += 4; /*** Reserved ***/
  pos += 2; /*** Write Mode ***/
  pos += 2; /*** Remaining ***/

  blocks64 = size >> 16;

  setUShort (c.smb, pos, blocks64);
  pos += 2;				       /*** Length High ***/
  setUShort (c.smb, pos, size & 0xffff);
  pos += 2;					    /*** Length Low ***/
  setUShort (c.smb, pos, 59);
  pos += 2;				 /*** Data Offset ***/
  setUShort (c.smb, pos, size & 0xffff);
  pos += 2;					    /*** Data Byte Count ***/

  c.msg = SESS_MSG;
  c.length = htons (pos + size);

  /*** Will this fit in a single send? ***/
  if (size <= 2916)
    {
      memcpy (&c.smb[pos], buffer, size);
      pos += size;
    }
  else
    {
      memcpy (&c.smb[pos], buffer, 2916);
      pos += 2916;
    }

  pos += 4;

    /*** Send Header Information ***/
  ret = send (smbsock, (char *) &c, pos, 0);

  if (size > 2916)
    {
	/*** Send the data ***/
      ret = send (smbsock, buffer + 2916, size - 2916, 0);
    }

  if (SMBCheck (SMB_WRITE_ANDX, sizeof (s)) == SMB_SUCCESS)
    {
	return (int) getUShort (s.smb, SMB_HEADER + 5);
    }

  return 0;

}

/****************************************************************************
 * Primary setup, logon and connection all in one :)
 ****************************************************************************/
int
SMB_Init (char *user, char *password, char *client,
	  char *server, char *share, char *IP)
{
  int ret;
  int nodelay;

    /*** Create the global socket ***/
  smbsock = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);

    /*** Switch off Nagle, ON TCP_NODELAY ***/
  nodelay = 1;
  ret = setsockopt (smbsock, IPPROTO_TCP, TCP_NODELAY,
		    (char *) &nodelay, sizeof (char));

    /*** Attempt to connect to the server IP ***/
  memset (&smbs, 0, sizeof (client));
  smbs.sin_family = AF_INET;
  smbs.sin_port = htons (445);
  smbs.sin_addr.s_addr = inet_addr (IP);

  ret = connect (smbsock, (struct sockaddr *) &smbs, sizeof (smbs));

  if (ret)
    {
      closesocket (smbsock);
      return 0;
    }

  if (SMB_NegotiateProtocol () == SMB_SUCCESS)
    {
      if (SMB_SetupAndX (user, password) == SMB_SUCCESS)
	{
	  if (SMB_TreeAndX (server, share) == SMB_SUCCESS)
	    return SMB_SUCCESS;
	}
    }

  return 0;

}

/****************************************************************************
 * SMB_Destroy
 *
 * Probably NEVER called on GameCube, but here for completeness
 ****************************************************************************/
void
SMB_Destroy ()
{
  if (smbsock)
    closesocket (smbsock);
}
