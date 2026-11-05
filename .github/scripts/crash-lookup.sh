#!/usr/bin/env bash
# Turn the addresses in a crash screen into function / file:line, using the ELF from the
# matching build's "debug-*" artifact. The ELF must come from the SAME build as the report:
# check the reporter's SHA-256 against sha256sums.txt in that artifact first.
#
# usage: crash-lookup.sh <elf> <crash.txt | hex-address>...
#   crash.txt is the pasted GC/Wii exception screen: LR, SRR0, DAR and the STACK DUMP are used.
#   Wii U / Aroma crash addresses can be given directly as hex addresses.
# env:   CROSS=<prefix> overrides the binutils prefix (default: $DEVKITPPC/bin/powerpc-eabi-)
set -eu

ELF="${1:?usage: crash-lookup.sh <elf> <crash.txt | hex-address>...}"; shift
CROSS="${CROSS:-${DEVKITPPC:-/opt/devkitpro/devkitPPC}/bin/powerpc-eabi-}"

addrs=()
add() { local v="${1,,}"; local x; for x in "${addrs[@]:-}"; do [ "$x" = "$v" ] && return; done; addrs+=("$v"); }

for a in "$@"; do
  if [ -f "$a" ]; then
    # SRR0 is the faulting instruction; LR is its caller. Keep that order, PC first.
    for reg in SRR0 LR CTR; do
      v="$(grep -oiE "^$reg[[:space:]]+[0-9a-f]{8}" "$a" | awk '{print $2}' | head -n 1 || true)"
      [ -n "$v" ] && add "$v"
    done
    # "STACK DUMP:" block: addresses separated by "-->" until the blank line
    while read -r v; do add "$v"; done < <(awk '/STACK DUMP/{f=1;next} /^[[:space:]]*$/{f=0} f' "$a" | grep -oiE '[0-9a-f]{8}')
    dar="$(grep -oiE '^DAR[[:space:]]+[0-9a-f]{8}' "$a" | awk '{print $2}' | head -n 1 || true)"
    [ -n "$dar" ] && echo "DAR (faulting data address): 0x$dar - a bad pointer if outside MEM1/MEM2 (0x80000000-0x817fffff, 0x90000000-0x93ffffff)"
  else
    add "${a#0x}"
  fi
done
[ ${#addrs[@]} -gt 0 ] || { echo "no addresses found" >&2; exit 1; }

echo "== functions (innermost inlined frame first) =="
for a in "${addrs[@]}"; do
  printf '0x%s  ' "$a"
  "${CROSS}addr2line" -f -C -i -p -e "$ELF" "0x$a" | sed '2,$s/^/            /'
done

pc="${addrs[0]}"; start=$((16#$pc - 32)); stop=$((16#$pc + 32))
echo; echo "== disassembly around 0x$pc (compare with the report's CODE DUMP) =="
"${CROSS}objdump" -d -C --start-address="$start" --stop-address="$stop" "$ELF" | tail -n +7
