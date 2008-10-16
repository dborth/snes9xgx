#ifndef AUTH_DES_H
#define AUTH_DES_H
/* ========================================================================== **
 *
 *                                    DES.h
 *
 * Copyright:
 *  Copyright (C) 2003, 2004 by Christopher R. Hertel
 *
 * Email: crh@ubiqx.mn.org
 *
 * $Id: DES.h,v 0.5 2004/05/30 02:31:47 crh Exp $
 *
 * -------------------------------------------------------------------------- **
 *
 * Description:
 *
 *  Implements DES encryption, but not decryption.
 *  DES is used to create LM password hashes and both LM and NTLM Responses.
 *
 * -------------------------------------------------------------------------- **
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
 *
 * -------------------------------------------------------------------------- **
 *
 * Notes:
 *
 *  This implementation was created by studying many existing examples
 *  found in Open Source, in the public domain, and in various documentation.
 *  The SMB protocol makes minimal use of the DES function, so this is a
 *  minimal implementation.  That which is not required has been removed.
 *
 *  The SMB protocol uses the DES algorithm as a hash function, not an
 *  encryption function.  The auth_DEShash() implemented here is a one-way
 *  function.  The reverse is not implemented in this module.  Also, there
 *  is no attempt at making this either fast or efficient.  There is no
 *  need, as the auth)DEShash() function is used for generating the LM
 *  Response from a 7-byte key and an 8-byte challenge.  It is not intended
 *  for use in encrypting large blocks of data or data streams.
 *
 *  As stated above, this implementation is based on studying existing work
 *  in the public domain or under Open Source (specifically LGPL) license.
 *  The code, however, is written from scratch.  Obviously, I make no claim
 *  with regard to those earlier works (except to claim that I am grateful
 *  to the previous implementors whose work I studied).  See the list of
 *  references below for resources I used.
 *
 *  References:
 *    I read through the libmcrypt code to see how they put the pieces
 *    together.  See: http://mcrypt.hellug.gr/
 *    Libmcrypt is available under the terms of the LGPL.
 *
 *    The libmcrypt implementation includes the following credits:
 *      written 12 Dec 1986 by Phil Karn, KA9Q; large sections adapted
 *      from the 1977 public-domain program by Jim Gillogly
 *      Modified for additional speed - 6 December 1988 Phil Karn
 *      Modified for parameterized key schedules - Jan 1991 Phil Karn
 *      modified in order to use the libmcrypt API by Nikos Mavroyanopoulos
 *      All modifications are placed under the license of libmcrypt.
 *
 *    See also Phil Karn's privacy and security page:
 *      http://www.ka9q.net/privacy.html
 *
 *    I relied heavily upon:
 *      Applied Cryptography, Second Edition:
 *        Protocols, Algorithms, and Source Code in C
 *      by Bruce Schneier. ISBN 0-471-11709-9, John Wiley & Sons, Inc., 1996
 *    Particularly Chapter 12.
 *
 *    Here's one more DES resource, which I found quite helpful (aside from
 *    the Clinton jokes):
 *      http://www.aci.net/kalliste/des.htm
 *
 *    Finally, the use of DES in SMB is covered in:
 *      Implementing CIFS - the Common Internet File System
 *      by your truly.  ISBN 0-13-047116-X, Prentice Hall PTR., August 2003
 *    Section 15.3, in particular.
 *    (Online at: http://ubiqx.org/cifs/SMB.html#SMB.8.3)
 *
 * ========================================================================== **
 */

//#include "auth_common.h"
#include <stdio.h>
typedef unsigned char uchar;
typedef unsigned char uint8_t;

/* -------------------------------------------------------------------------- **
 * Functions:
 */

uchar *auth_DESkey8to7( uchar *dst, const uchar *key );
  /* ------------------------------------------------------------------------ **
   * Compress an 8-byte DES key to its 7-byte form.
   *
   *  Input:  dst - Pointer to a memory location (minimum 7 bytes) to accept
   *                the compressed key.
   *          key - Pointer to an 8-byte DES key.  See the notes below.
   *
   *  Output: A pointer to the compressed key (same as <dst>) or NULL if
   *          either <src> or <dst> were NULL.
   *
   *  Notes:  There are no checks done to ensure that <dst> and <key> point
   *          to sufficient space.  Please be carefull.
   *
   *          The two pointers, <dst> and <key> may point to the same
   *          memory location.  Internally, a temporary buffer is used and
   *          the results are copied back to <dst>.
   *
   *          The DES algorithm uses 8 byte keys by definition.  The first
   *          step in the algorithm, however, involves removing every eigth
   *          bit to produce a 56-bit key (seven bytes).  SMB authentication
   *          skips this step and uses 7-byte keys.  The <auth_DEShash()>
   *          algorithm in this module expects 7-byte keys.  This function
   *          is used to convert an 8-byte DES key into a 7-byte SMB DES key.
   *
   * ------------------------------------------------------------------------ **
   */


uchar *auth_DEShash( uchar *dst, const uchar *key, const uchar *src );
  /* ------------------------------------------------------------------------ **
   * DES encryption of the input data using the input key.
   *
   *  Input:  dst - Destination buffer.  It *must* be at least eight bytes
   *                in length, to receive the encrypted result.
   *          key - Encryption key.  Exactly seven bytes will be used.
   *                If your key is shorter, ensure that you pad it to seven
   *                bytes.
   *          src - Source data to be encrypted.  Exactly eight bytes will
   *                be used.  If your source data is shorter, ensure that
   *                you pad it to eight bytes.
   *
   *  Output: A pointer to the encrpyted data (same as <dst>).
   *
   *  Notes:  In SMB, the DES function is used as a hashing function rather
   *          than an encryption/decryption tool.  When used for generating
   *          the LM hash the <src> input is the known value "KGS!@#$%" and
   *          the key is derived from the password entered by the user.
   *          When used to generate the LM or NTLM response, the <key> is
   *          derived from the LM or NTLM hash, and the challenge is used
   *          as the <src> input.
   *          See: http://ubiqx.org/cifs/SMB.html#SMB.8.3
   *
   *        - This function is called "DEShash" rather than just "DES"
   *          because it is only used for creating LM hashes and the
   *          LM/NTLM responses.  For all practical purposes, however, it
   *          is a full DES encryption implementation.
   *
   *        - This DES implementation does not need to be fast, nor is a
   *          DES decryption function needed.  The goal is to keep the
   *          code small, simple, and well documented.
   *
   *        - The input values are copied and refiddled within the module
   *          and the result is not written to <dst> until the very last
   *          step, so it's okay if <dst> points to the same memory as
   *          <key> or <src>.
   *
   * ------------------------------------------------------------------------ **
   */


/* ========================================================================== */
#endif /* AUTH_DES_H */
