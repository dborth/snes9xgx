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

#ifndef CONFIG_H
#define CONFIG_H

#include <set>
#include <vector>
#include <string>

#include "port.h"
#include "reader.h"

class ConfigFile {
  public:
    ConfigFile(void);

    void Clear(void);
    
    // return false on failure
    bool LoadFile(const char *filename);
    void LoadFile(Reader *r, const char *name=NULL);

    // return false if key does not exist
    bool Exists(const char *key);

    // return the value / default
    std::string GetString(const char *key, std::string def);
    char *GetString(const char *key, char *out, uint32 outlen); // return NULL if it doesn't exist, out not affected
    const char *GetString(const char *key, const char *def=NULL); // NOTE: returned pointer becomes invalid when key is deleted/modified, or the ConfigFile is Clear()ed or deleted.
    char *GetStringDup(const char *key, const char *def=NULL); // Much like "strdup(GetString(key, def))"
    int32 GetInt(const char *key, int32 def=-1, bool *bad=NULL);
    uint32 GetUInt(const char *key, uint32 def=0, int base=0, bool *bad=NULL); // base = 0, 8, 10, or 16
    bool GetBool(const char *key, bool def=false, bool *bad=NULL);

    // return true if the key existed prior to setting
    bool SetString(const char *key, std::string val);
    bool SetInt(const char *key, int32 val);
    bool SetUInt(const char *key, uint32 val, int base=10); // base = 8, 10, or 16
    bool SetBool(const char *key, bool val, char *true_val="TRUE", char *false_val="FALSE");
    bool DeleteKey(const char *key);

    // Operation on entire sections
    bool DeleteSection(const char *section);
    typedef std::vector<std::pair<std::string,std::string> > secvec_t;
    secvec_t GetSection(const char *section);

    bool SaveTo(const char *filename);

  private:
    std::string Get(const char *key);

    class ConfigEntry {
      protected:
        int line;
        std::string section;
        std::string key;
        std::string val;

        struct key_less {
            bool operator()(const ConfigEntry &a, const ConfigEntry &b) const{
                if(a.section!=b.section) return a.section<b.section;
                return a.key<b.key;
            }
        };

        struct line_less {
            bool operator()(const ConfigEntry &a, const ConfigEntry &b){
                if(a.line==b.line) return a.key<b.key;
                if(b.line<0) return true;
                return a.line<b.line;
            }
        };

        static void trim(std::string &s){
            int i;
            i=s.find_first_not_of(" \f\n\r\t\v");
            if(i==-1){
                s.clear();
                return;
            }
            if(i>0) s.erase(0, i);
            i=s.find_last_not_of(" \f\n\r\t\v");
            if(i!=-1) s.erase(i+1);
        }

      public:
        ConfigEntry(int l, std::string &s, std::string &k, std::string &v) :
            line(l), section(s), key(k), val(v) {
            trim(section);
            trim(key);
        }

        void parse_key(std::string &k){
            int i=k.find("::");
            if(i==-1){
                section=""; key=k;
            } else {
                section=k.substr(0,i); key=k.substr(i+2);
            }
            trim(section);
            trim(key);
        }
        
        ConfigEntry(std::string k){
            parse_key(k);
        }

        ConfigEntry(std::string k, std::string &v) : line(-1), val(v) {
            parse_key(k);
        }

        friend class ConfigFile;
        friend struct key_less;
        friend struct line_less;
    };
    std::set<ConfigEntry, ConfigEntry::key_less> data;
};

/* Config file format:
 * 
 * Comments are any lines whose first non-whitespace character is ';' or '#'.
 * Note that comments must be on lines by themselves, they cannot "end" a
 * normal line.
 * 
 * All parameters fall into sections. To name a section, the first
 * non-whitespace character on the line will be '[', and the last will be ']'.
 *
 * Parameters are simple key=value pairs. Whitespace around the '=', and at the
 * beginning or end of the line is ignored. Key names may not contain '=' nor
 * begin with '[', however values can. If the last character of the value is
 * '\', the next line (sans leading/trailing whitespace) is considered part of
 * the value as well. Programmatically, the key "K" in section "S" is referred
 * to as "S::K", much like C++ namespaces. For example:
 *   [Section1]
 *   # this is a comment
 *   foo = bar \
 *      baz\    
 *      quux \
 *   # this is not a comment!
 * means the value of "Section1::foo" is "bar bazquux # this is not a comment!" 
 *
 * Parameters may be of several types:
 *  String - Bare characters. If the first and last characters are both '"',
 *           they are removed (so just double them if you really want quotes
 *           there)
 *  Int - A decimal number from -2147483648 to 2147483647
 *  UInt - A number in decimal, hex, or octal from 0 to 4294967295 (or
 *         0xffffffff, or 037777777777)
 *  Bool - true/false, 0/1, on/off, yes/no
 *
 * Of course, the actual accepted values for a parameter may be further
 * restricted ;)
 */


/* You must write this for your port */
void S9xParsePortConfig(ConfigFile &, int pass);

/* This may or may not be useful to you */
char *S9xParseDisplayConfig(ConfigFile &, int pass);

#endif
