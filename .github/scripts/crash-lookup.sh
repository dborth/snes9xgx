#!/usr/bin/env bash
# Turn the addresses in a crash screen into function / file:line, using the ELF from the
# matching build's "debug-*" artifact. The ELF must come from the SAME build as the report:
# the tool compares the report's CODE DUMP with the ELF and refuses a different build.
#
# usage: crash-lookup.sh <elf> <crash.txt | hex-address>...
#   crash.txt is the pasted GC/Wii exception screen: registers, LR/SRR0/DAR, STACK DUMP and CODE DUMP.
#   Wii U / Aroma crash addresses can be given directly as hex addresses.
# env:   CROSS=<prefix> overrides the binutils prefix (default: $DEVKITPPC/bin/powerpc-eabi-)
#        FORCE=1        symbolize even when the CODE DUMP does not match the ELF
exec python3 "$(dirname "$0")/crash-lookup.py" "$@"
