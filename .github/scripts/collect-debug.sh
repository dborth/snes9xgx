#!/usr/bin/env bash
# Collects everything needed to diagnose a crash report against this exact build:
# the ELF (with DWARF), the link map, symbol/section tables, the shipped binary,
# SHA-256 sums, and the toolchain / dependency versions that produced it.
#
# usage: collect-debug.sh <Wii|GameCube|WiiU> [output-dir]
# Run from the repo root, after the build. Never fails the job: every step is best effort.
set -u

PLATFORM="${1:?usage: collect-debug.sh <Wii|GameCube|WiiU> [output-dir]}"
OUT="${2:-debug}"

case "$PLATFORM" in
  Wii)      SUFFIX=wii;  BUILD=build_wii  ;;
  GameCube) SUFFIX=gc;   BUILD=build_gc   ;;
  WiiU)     SUFFIX=wiiu; BUILD=build_wiiu ;;
  *) echo "unknown platform: $PLATFORM" >&2; exit 1 ;;
esac

git config --global --add safe.directory '*' 2>/dev/null || true
mkdir -p "$OUT"

CROSS="${DEVKITPPC:-/opt/devkitpro/devkitPPC}/bin/powerpc-eabi-"
ELF="$(ls executables/*-"$SUFFIX".elf 2>/dev/null | head -1)"
MAP="$(find "$BUILD" -name '*.elf.map' 2>/dev/null | head -1)"
if [ "$SUFFIX" = wiiu ]; then
  BINS="$(ls executables/*-wiiu.rpx executables/*-wiiu.wuhb 2>/dev/null)"
else
  BINS="$(ls executables/*-"$SUFFIX".dol 2>/dev/null)"
fi

# ---- versions.txt: what produced this build -------------------------------------------
{
  echo "== build =="
  echo "platform      $PLATFORM"
  echo "built (UTC)   $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "repository    ${GITHUB_REPOSITORY:-?}"
  echo "commit        ${GITHUB_SHA:-$(git rev-parse HEAD 2>/dev/null)}"
  echo "ref           ${GITHUB_REF:-?}  (event: ${GITHUB_EVENT_NAME:-?})"
  echo "run           ${GITHUB_SERVER_URL:-https://github.com}/${GITHUB_REPOSITORY:-?}/actions/runs/${GITHUB_RUN_ID:-?} (attempt ${GITHUB_RUN_ATTEMPT:-?})"
  echo "commit info   $(git log -1 --format='%h %ad %s' --date=iso 2>/dev/null)"
  echo "git describe  $(git describe --always --dirty --tags 2>/dev/null)"
  echo
  echo "== compiler =="
  "${CROSS}gcc" -v 2>&1 | tail -n 2
  "${CROSS}ld" --version 2>&1 | head -n 1
  echo
  echo "== environment =="
  env | grep -E '^(DEVKITPRO|DEVKITPPC|LIBOGC_|PORTLIBS|ImageOS|ImageVersion|RUNNER_)' | sort
  grep -E '^(PRETTY_NAME|VERSION_ID)=' /etc/os-release 2>/dev/null
  echo
  echo "== packages (toolchain, libogc2/wut and portlibs) =="
  (dkp-pacman -Q || pacman -Q) 2>&1 | sort
  echo
  echo "== dependencies built from source (not pinned: HEAD at build time) =="
  for d in /tmp/libsmb2 /tmp/libmocha /tmp/libdvm; do
    [ -d "$d/.git" ] || continue
    echo "$(basename "$d") $(git -C "$d" rev-parse HEAD) $(git -C "$d" log -1 --format='%ad %s' --date=short)"
    git -C "$d" submodule status 2>/dev/null | sed 's/^/    submodule /'
  done
} > "$OUT/versions.txt" 2>&1

# ---- ELF + link map + the shipped binary ----------------------------------------------
[ -n "$ELF" ] && cp "$ELF" "$OUT/"
[ -n "$MAP" ] && cp "$MAP" "$OUT/"
for f in $BINS; do
  case "$f" in *.wuhb) ;; *) cp "$f" "$OUT/" ;; esac   # the .wuhb is just the .rpx plus assets
done

# ---- tables that are quick to grep without a toolchain --------------------------------
if [ -n "$ELF" ]; then
  "${CROSS}size" -A "$ELF"                    > "$OUT/sections.txt" 2>&1
  "${CROSS}readelf" -lSW "$ELF"               > "$OUT/segments.txt" 2>&1
  "${CROSS}nm" -nSC --defined-only "$ELF"     > "$OUT/symbols.txt"  2>&1
fi

# ---- sha256sums.txt: tie the report's hash to a build ---------------------------------
(
  cd "$OUT" && : > sha256sums.txt
  for f in *; do
    [ -f "$f" ] && [ "$f" != sha256sums.txt ] && sha256sum "$f" >> sha256sums.txt
  done
)
# the .wuhb is not copied (see above) but its hash is what a Wii U user has on disk
for f in $BINS; do
  case "$f" in *.wuhb) (cd "$(dirname "$f")" && sha256sum "$(basename "$f")") >> "$OUT/sha256sums.txt" ;; esac
done

echo "debug info collected in $OUT:"; ls -l "$OUT"
[ -n "$ELF" ] || echo "warning: no ELF found for $PLATFORM (did the build fail?)"
exit 0
