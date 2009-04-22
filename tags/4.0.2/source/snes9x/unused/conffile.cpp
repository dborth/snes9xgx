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

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <string>

#include "conffile.h"

using namespace std;

ConfigFile::ConfigFile(void) {
    data.clear();
}

void ConfigFile::Clear(void){
    data.clear();
}
    
bool ConfigFile::LoadFile(const char *filename){
    STREAM s;
    bool ret=false;
    const char *n, *n2;

    if((s=OPEN_STREAM(filename, "r"))){
        n=filename;
        n2=strrchr(n, '/'); if(n2!=NULL) n=n2+1;
        n2=strrchr(n, '\\'); if(n2!=NULL) n=n2+1;
        LoadFile(new fReader(s), n);
        CLOSE_STREAM(s);
        ret = true;
    } else {
        fprintf(stderr, "Couldn't open conffile ");
        perror(filename);
    }
    return ret;
}


void ConfigFile::LoadFile(Reader *r, const char *name){
    string l, key, val;
    string section;
    int i, line, line2;
    bool eof;

    line=line2=0;
    section.clear();
    do {
        line=line2++;
        l=r->getline(eof);
        ConfigEntry::trim(l);
        if(l.size()==0) continue;
        
        if(l[0]=='#' || l[0]==';'){
            // comment
            continue;
        }

        if(l[0]=='['){
            if(*l.rbegin()!=']'){
                if(name) fprintf(stderr, "%s:", name);
                fprintf(stderr, "[%d]: Ignoring invalid section header\n", line);
                continue;
            }
            section.assign(l, 1, l.size()-2);
            continue;
        }

        while(*l.rbegin()=='\\'){
            l.erase(l.size()-1);
            line2++;
            val=r->getline(eof);
            if(eof){
                fprintf(stderr, "Unexpected EOF reading config file");
                if(name) fprintf(stderr, " '%s'", name);
                fprintf(stderr, "\n");
                return;
            }
            ConfigEntry::trim(val);
            l+=val;
        }
        i=l.find('=');
        if(i<0){
            if(name) fprintf(stderr, "%s:", name);
            fprintf(stderr, "[%d]: Ignoring invalid entry\n", line);
            continue;
        }
        key=l.substr(0,i); ConfigEntry::trim(key);
        val=l.substr(i+1); ConfigEntry::trim(val);
        if(val[0]=='"' && *val.rbegin()=='"') val=val.substr(1, val.size()-2);

        ConfigEntry e(line, section, key, val);
        data.erase(e);
        data.insert(e);
    } while(!eof);
}

bool ConfigFile::SaveTo(const char *filename){
    string section;
    FILE *fp;

    if((fp=fopen(filename, "w"))==NULL){
        fprintf(stderr, "Couldn't write conffile ");
        perror(filename);
        return false;
    }

    section.clear();
    set<ConfigEntry, ConfigEntry::line_less> tmp;
    fprintf(fp, "# Config file output by snes9x\n");
    time_t t=time(NULL);
    fprintf(fp, "# %s", ctime(&t));
    for(set<ConfigEntry, ConfigEntry::key_less>::iterator j=data.begin(); ; j++){
        if(j==data.end() || j->section!=section){
            if(!tmp.empty()){
                fprintf(fp, "\n[%s]\n", section.c_str());
				for(set<ConfigEntry, ConfigEntry::line_less>::iterator i=tmp.begin(); i!=tmp.end(); i++){
                    string o=i->val; ConfigEntry::trim(o);
                    if(o!=i->val) o="\""+i->val+"\"";
                    fprintf(fp, "%s = %s\n", i->key.c_str(), o.c_str());
                }
            }
            if(j==data.end()) break;
            section=j->section;
            tmp.clear();
        }
        tmp.insert(*j);
    }

    fclose(fp);
    return true;
}


/***********************************************/

string ConfigFile::Get(const char *key){
	set<ConfigEntry, ConfigEntry::key_less>::iterator i;
    i=data.find(ConfigEntry(key));
    return i->val;
}

bool ConfigFile::Exists(const char *key){
    return data.find(ConfigEntry(key))!=data.end();
}


string ConfigFile::GetString(const char *key, string def){
    if(!Exists(key)) return def;
    return Get(key);
}

char *ConfigFile::GetString(const char *key, char *out, uint32 outlen){
    if(!Exists(key)) return NULL;
    ZeroMemory(out, outlen);
    string o=Get(key);
    if(outlen>0){
        outlen--;
        if(o.size()<outlen) outlen=o.size();
        memcpy(out, o.data(), outlen);
    }
    return out;
}

const char *ConfigFile::GetString(const char *key, const char *def){
    set<ConfigEntry, ConfigEntry::key_less>::iterator i;
    i=data.find(ConfigEntry(key));
    if(i==data.end()) return def;
    // This should be OK, until this key gets removed
    return i->val.c_str();
}

char *ConfigFile::GetStringDup(const char *key, const char *def){
    const char *c=GetString(key, def);
    if(c==NULL) return NULL;
    return strdup(c);
}

bool ConfigFile::SetString(const char *key, string val){
    set<ConfigEntry, ConfigEntry::key_less>::iterator i;
    bool ret=false;

    ConfigEntry e(key, val);

    i=data.find(e);
    if(i!=data.end()){
        e.line=i->line;
        data.erase(e);
        ret=true;
    }
    data.insert(e);
    return ret;
}

int32 ConfigFile::GetInt(const char *key, int32 def, bool *bad){
    if(bad) *bad=false;
    if(!Exists(key)) return def;
    char *c;
    int32 i;
    string o=Get(key);
    i=strtol(o.c_str(), &c, 10);
    if(c!=NULL && *c!='\0'){
        i=def;
        if(bad) *bad=true;
    }
    return i;
}

bool ConfigFile::SetInt(const char *key, int32 val){
    char buf[20];
    snprintf(buf, sizeof(buf), "%d", val);
    return SetString(key, buf);
}

uint32 ConfigFile::GetUInt(const char *key, uint32 def, int base, bool *bad){
    if(bad) *bad=false;
    if(!Exists(key)) return def;
    if(base!=8 && base!=10 && base!=16) base=0;
    char *c;
    uint32 i;
    string o=Get(key);
    i=strtol(o.c_str(), &c, base);
    if(c!=NULL && *c!='\0'){
        i=def;
        if(bad) *bad=true;
    }
    return i;
}

bool ConfigFile::SetUInt(const char *key, uint32 val, int base){
    char buf[20];
    switch(base){
      case 10:
      default:
        snprintf(buf, sizeof(buf), "%u", val);
        break;
      case 8:
        snprintf(buf, sizeof(buf), "%#o", val);
        break;
      case 16:
        snprintf(buf, sizeof(buf), "%#x", val);
        break;
    }
    return SetString(key, buf);
}

bool ConfigFile::GetBool(const char *key, bool def, bool *bad){
    if(bad) *bad=false;
    if(!Exists(key)) return def;
    string o=Get(key);
    const char *c=o.c_str();
    if(!strcasecmp(c, "true") || !strcasecmp(c, "1") || !strcasecmp(c, "yes") || !strcasecmp(c, "on")) return true;
    if(!strcasecmp(c, "false") || !strcasecmp(c, "0") || !strcasecmp(c, "no") || !strcasecmp(c, "off")) return false;
    if(bad) *bad=true;
    return def;
}

bool ConfigFile::SetBool(const char *key, bool val, char *true_val, char *false_val){
    return SetString(key, val?true_val:false_val);
}

bool ConfigFile::DeleteKey(const char *key){
    return (data.erase(ConfigEntry(key))>0);
}

/***********************************************/

bool ConfigFile::DeleteSection(const char *section){
    set<ConfigEntry, ConfigEntry::key_less>::iterator s, e;
    
    for(s=data.begin(); s!=data.end() && s->section!=section; s++);
    if(s==data.end()) return false;
    for(e=s; e!=data.end() && e->section==section; e++);
    data.erase(s, e);
    return true;
}

ConfigFile::secvec_t ConfigFile::GetSection(const char *section){
    secvec_t v;
    set<ConfigEntry, ConfigEntry::key_less>::iterator i;

    v.clear();
    for(i=data.begin(); i!=data.end(); i++){
        if(i->section!=section) continue;
        v.push_back(std::pair<string,string>(i->key, i->val));
    }
    return v;
}
