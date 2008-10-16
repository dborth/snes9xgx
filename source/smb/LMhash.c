/* ========================================================================== **
 *
 *                                  LMhash.c
 *
 * Copyright:
 *  Copyright (C) 2004 by Christopher R. Hertel
 *
 * Email: crh@ubiqx.mn.org
 *
 * $Id: LMhash.c,v 0.1 2004/05/30 02:26:31 crh Exp $
 *
 * -------------------------------------------------------------------------- **
 *
 * Description:
 *
 *  Implemention of the LAN Manager hash (LM hash) and LM response
 *  algorithms.
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
 *  This module implements the LM hash.  The NT hash is simply the MD4() of
 *  the password, so we don't need a separate implementation of that.  This
 *  module also implements the LM response, which can be combined with the
 *  NT hash to produce the NTLM response.
 *
 *  This implementation was created based on the description in my own book.
 *  The book description was, in turn, written after studying many existing
 *  examples in various documentation.  Jeremy Allison and Andrew Tridgell
 *  deserve lots of credit for having figured out the secrets of Lan Manager
 *  authentication many years ago.
 *
 *  See:
 *    Implementing CIFS - the Common Internet File System
 *      by your truly.  ISBN 0-13-047116-X, Prentice Hall PTR., August 2003
 *    Section 15.3, in particular.
 *    (Online at: http://ubiqx.org/cifs/SMB.html#SMB.8.3)
 *
 * ========================================================================== **
 */

#include "DES.h"
#include "LMhash.h"


/* -------------------------------------------------------------------------- **
 * Static Constants:
 *
 *  SMB_LMhash_Magic  - This is the plaintext "magic string" that is used to
 *                      generate the LM Hash from the user's password.  This
 *                      value was a Microsoft "secret" for many years.
 */

static const uchar SMB_LMhash_Magic[8] =
  { 'K', 'G', 'S', '!', '@', '#', '$', '%' };


/* -------------------------------------------------------------------------- **
 * Functions:
 */

uchar *auth_LMhash( uchar *dst, const uchar *pwd, const int pwdlen )
  /* ------------------------------------------------------------------------ **
   * Generate an LM Hash from the input password.
   *
   *  Input:  dst     - Pointer to a location to which to write the LM Hash.
   *                    Requires 16 bytes minimum.
   *          pwd     - Source password.  Should be in OEM charset (extended
   *                    ASCII) format in all upper-case, but this
   *                    implementation doesn't really care.  See the notes
   *                    below.
   *          pwdlen  - Length, in bytes, of the password.  Normally, this
   *                    will be strlen( pwd ).
   *
   *  Output: Pointer to the resulting LM hash (same as <dst>).
   *
   *  Notes:  This function does not convert the input password to upper
   *          case.  The upper-case conversion should be done before the
   *          password gets this far.  DOS codepage handling and such
   *          should be taken into consideration.  Rather than attempt to
   *          work out all those details here, the function assumes that
   *          the password is in the correct form before it reaches this
   *          point.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  int     i,
          max14;
  uint8_t tmp_pwd[14] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

  /* Copy at most 14 bytes of <pwd> into <tmp_pwd>.
   * If the password is less than 14 bytes long
   * the rest will be nul padded.
   */
  max14 = pwdlen > 14 ? 14 : pwdlen;
  for( i = 0; i < max14; i++ )
    tmp_pwd[i] = pwd[i];

  /* The password is split into two 7-byte keys, each of which
   * are used to DES-encrypt the magic string.  The results are
   * concatonated to produce the 16-byte LM Hash.
   */
  (void)auth_DEShash(  dst,     tmp_pwd,    SMB_LMhash_Magic );
  (void)auth_DEShash( &dst[8], &tmp_pwd[7], SMB_LMhash_Magic );

  /* Return a pointer to the result.
   */
  return( dst );
  } /* auth_LMhash */


uchar *auth_LMresponse( uchar *dst, const uchar *hash, const uchar *challenge )
  /* ------------------------------------------------------------------------ **
   * Generate the LM (or NTLM) response from the password hash and challenge.
   *
   *  Input:  dst       - Pointer to memory into which to write the response.
   *                      Must have 24 bytes available.
   *          hash      - Pointer to the 16-byte password hash.
   *          challenge - Pointer to the 8-byte challenge.
   *
   *  Output: A pointer to the 24-byte response (same as <dst>).
   *
   *  Notes:  The function does not check the lengths of the input or output
   *          parameters.  The byte sizes given above must be respected by
   *          calling function.
   *
   * ------------------------------------------------------------------------ **
   */
  {
  uchar tmp[7] =
    { hash[14], hash[15], 0,0,0,0,0 };  /* 3rd key is nul-padded. */

  /* It's painfully simple...
   * The challenge is DES encrypted three times.
   * The first time, the first 7 bytes of the hash are used.
   * The second time, the second 7 bytes of the hash are used.
   * The third time, the two remaining hash bytes plus five nuls are used.
   * The three 8-byte results are concatonated to form the 24-byte response.
   */
  (void)auth_DEShash(  dst,     hash,    challenge );
  (void)auth_DEShash( &dst[8], &hash[7], challenge );
  (void)auth_DEShash( &dst[16], tmp,     challenge );

  /* Return the result.
   */
  return( dst );
  } /* auth_LMresponse */

/* ========================================================================== */
