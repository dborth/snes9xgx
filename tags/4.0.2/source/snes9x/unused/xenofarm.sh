#! /bin/sh

# This script is part of the automated Xenofarm testing testing. For
# more information about Xenofarm see http://www.lysator.liu.se/xenofarm/

# $Id: xenofarm.sh,v 1.6 2003/11/09 14:02:39 pbortas Exp $
# This file scripts the xenofarm actions and creates a result package
# to send back.

#FIXME: snes9x doesn't really need MAKE_FLAGS. Clean out later.

BUILDDIR=.
#OS=`uname -s -r -m|sed \"s/ /-/g\"|tr \"[A-Z]\" \"[a-z]\"|tr \"/()\" \"___\"`
#BUILDDIR=build/$(OS)
MAKE=${MAKE-make}
PATH=$PATH:/usr/ccs/bin
export PATH

log() {
  echo $1 | tee -a build/xenofarm/mainlog.txt
}

log_start() {
  log "BEGIN $1"
  TZ=GMT date >> build/xenofarm/mainlog.txt
}

log_end() {
  LASTERR=$1
  if [ "$1" = "0" ] ; then
    log "PASS"
  else
    log "FAIL"
  fi
  TZ=GMT date >> build/xenofarm/mainlog.txt
}

xenofarm_build() {
  log_start compile
 "`pwd`/configure" >> build/xenofarm/compilelog.txt 2>&1 &&
  $MAKE $MAKE_FLAGS >> build/xenofarm/compilelog.txt 2>&1
  log_end $?
  [ $LASTERR = 0 ] || return 1
}

xenofarm_post_build() {
  log_start verify
  $MAKE $MAKE_FLAGS verify > build/xenofarm/verifylog.txt 2>&1
  log_end $?
  [ $LASTERR = 0 ] || return 1
  
  log_start binrelease
  $MAKE $MAKE_FLAGS bin-release > build/xenofarm/binreleaselog.txt 2>&1
  log_end $?
  [ $LASTERR = 0 ] || return 1
}

fail_builddir() {
  echo "FATAL: Failed to create build directory!"
  exit 1
}

# main code

test -d build || mkdir build 2>&1 &&
rm -rf build/xenofarm 2>&1 &&
mkdir build/xenofarm 2>&1 || fail_builddir 

LC_CTYPE=C
export LC_CTYPE
log "FORMAT 2"

log_start build
xenofarm_build
log_end $?

if [ $LASTERR = 0 ]; then
  log_start post_build
  xenofarm_post_build
  log_end $?
  else :
fi

log_start response_assembly
  # Basic stuff
  cp ../buildid.txt build/xenofarm/
  # Configuration
  cp "$BUILDDIR/config.info" build/xenofarm/configinfo.txt 2>/dev/null || /bin/true
  (
    cd "$BUILDDIR"
    test -f config.log && cat config.log
    for f in `find . -name config.log -type f`; do
      echo
      echo '###################################################'
      echo '##' `dirname "$f"`
      echo
      cat "$f"
    done
  ) > build/xenofarm/configlogs.txt
  cp "$BUILDDIR/config.cache" build/xenofarm/configcache.txt 2>/dev/null || /bin/true;
  # Core files
  find . -name "core" -exec \
    gdb --batch --nx --command=bin/xenofarm_gdb_cmd "$BUILDDIR/pike" {} >> \
       build/xenofarm/_core.txt ";"
  find . -name "*.core" -exec \
    gdb --batch --nx --command=bin/xenofarm_gdb_cmd "$BUILDDIR/pike" {} >> \
      build/xenofarm/_core.txt ";"
  find . -name "core.*" -exec \
    gdb --batch --nx --command=bin/xenofarm_gdb_cmd "$BUILDDIR/pike" {} >> \
      build/xenofarm/_core.txt ";"
log_end $?

log "END"
