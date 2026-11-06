#!/usr/bin/env python3
"""Turn a GC/Wii exception screen (or bare hex addresses) into function / file:line.

Used through crash-lookup.sh. The ELF must come from the SAME build as the report. This tool
checks that itself: it compares the report's CODE DUMP with the bytes the ELF has at that
address, and refuses to symbolize against a different build (set FORCE=1 to override).

  crash-lookup.sh <elf> <crash.txt | hex-address>...
  env: CROSS=<prefix>  binutils prefix (default $DEVKITPPC/bin/powerpc-eabi-)
       FORCE=1         symbolize even when the CODE DUMP does not match the ELF
"""
import bisect, os, re, struct, subprocess, sys

HEX8 = r'[0-9A-Fa-f]{8}'


def die(msg, code=1):
    print(msg, file=sys.stderr)
    sys.exit(code)


class Elf:
    """Just enough of a 32-bit big-endian ELF reader to answer: what bytes are at this address?"""
    SHT_NOBITS, SHF_WRITE, SHF_ALLOC, SHF_EXEC = 8, 1, 2, 4

    def __init__(self, path):
        d = self.data = open(path, 'rb').read()
        if d[:4] != b'\x7fELF' or d[4] != 1 or d[5] != 2:
            die('%s is not a 32-bit big-endian ELF' % path)
        shoff, = struct.unpack('>I', d[32:36])
        shentsize, shnum, shstrndx = struct.unpack('>HHH', d[46:52])
        raw = [struct.unpack('>10I', d[shoff + i * shentsize: shoff + i * shentsize + 40]) for i in range(shnum)]
        stroff = raw[shstrndx][4]
        self.sections = []
        for name, typ, flags, addr, off, size, *_ in raw:
            n = d[stroff + name: d.index(b'\0', stroff + name)].decode()
            if flags & self.SHF_ALLOC and size:
                self.sections.append(dict(name=n, type=typ, flags=flags, addr=addr, off=off, size=size))
        self.sections.sort(key=lambda s: s['addr'])

    def section_of(self, addr):
        for s in self.sections:
            if s['addr'] <= addr < s['addr'] + s['size']:
                return s
        return None

    def read(self, addr, n):
        """n bytes at addr, or None if any of them is not backed by file contents."""
        out = b''
        while len(out) < n:
            s = self.section_of(addr + len(out))
            if not s or s['type'] == self.SHT_NOBITS:
                return None
            o = addr + len(out) - s['addr']
            take = min(n - len(out), s['size'] - o)
            out += self.data[s['off'] + o: s['off'] + o + take]
        return out

    def cstring(self, addr, limit=64):
        b = self.read(addr, 1)
        if b is None:
            return None
        s = bytearray()
        while len(s) < limit:
            c = self.read(addr + len(s), 1)
            if not c or c[0] == 0:
                break
            s += c
        return bytes(s)


class Tools:
    def __init__(self, elf):
        self.elf = elf
        self.cross = os.environ.get('CROSS') or os.path.join(os.environ.get('DEVKITPPC', '/opt/devkitpro/devkitPPC'), 'bin', 'powerpc-eabi-')
        self._syms = None

    def run(self, tool, *args):
        try:
            return subprocess.run([self.cross + tool, *args], capture_output=True, text=True, check=False).stdout
        except FileNotFoundError:
            die('%s%s not found: set CROSS=<binutils prefix> or DEVKITPPC' % (self.cross, tool))

    def symbols(self):
        if self._syms is None:
            rows = []
            for line in self.run('nm', '-nSC', '--defined-only', self.elf).splitlines():
                m = re.match(r'([0-9a-fA-F]{8})\s+(?:([0-9a-fA-F]{8})\s+)?(\w)\s+(.*)', line)
                if m:
                    rows.append((int(m.group(1), 16), int(m.group(2) or '0', 16), m.group(4)))
            self._syms = (rows, [r[0] for r in rows])
        return self._syms

    def symbol_at(self, addr):
        """name+off of the sized symbol that contains addr, else None (the caller falls back to section+off)."""
        rows, keys = self.symbols()
        i = bisect.bisect_right(keys, addr) - 1
        while i >= 0:           # aliases and nested symbols: the nearest one that actually covers addr
            a, size, name = rows[i]
            if size and addr < a + size:
                return '%s+0x%x' % (name, addr - a)
            if a < addr - 0x10000:
                break
            i -= 1
        return None


def classify(v):
    if v < 0x1000:
        return 'null-pointer access (offset 0x%x)' % v
    if 0x80000000 <= v <= 0x817fffff:
        return 'MEM1 (cached)'
    if 0xc0000000 <= v <= 0xc17fffff:
        return 'MEM1 (uncached)'
    if 0x90000000 <= v <= 0x93ffffff:
        return 'MEM2 (cached)'
    if 0xd0000000 <= v <= 0xd3ffffff:
        return 'MEM2 (uncached)'
    if 0xcc000000 <= v <= 0xcdffffff:
        return 'hardware registers'
    return 'NOT MAPPED on GC/Wii'


# Reports are often typed in from a photo. In a field that must be 8 hex digits, these letters are
# almost always a misread digit (O/o -> 0, Z -> 2, I/l -> 1, S -> 5); read them that way and say so.
OCR = str.maketrans('OoZIlS', '002115')
TOKEN = r'[0-9A-Za-z]{8}'


def fix_hex(tok, fixes):
    if re.fullmatch(HEX8, tok):
        return tok
    fixed = tok.translate(OCR)
    if re.fullmatch(HEX8, fixed):
        fixes.append((tok, fixed))
        return fixed
    return None


def parse_report(text):
    r = {}
    fixes = r['fixes'] = []
    # LR, SRR0, SRR1 and MSR share a line on some screens, so do not anchor to the start of a line
    for key in ('SRR0', 'LR', 'DAR', 'DSISR'):
        m = re.search(r'\b%s[ \t]+(%s)' % (key, TOKEN), text)
        if m:
            v = fix_hex(m.group(1), fixes)
            if v:
                r[key] = int(v, 16)
    r['gpr'] = {}
    for n, v in re.findall(r'GPR(\d\d)[ \t]+(%s)' % TOKEN, text):
        v = fix_hex(v, fixes)
        if v:
            r['gpr'][int(n)] = int(v, 16)
    m = re.search(r'STACK DUMP:(.*?)(?:\n\s*\n|CODE DUMP|\Z)', text, re.S)
    stack = []
    for tok in re.findall(TOKEN, m.group(1)) if m else []:
        v = fix_hex(tok, fixes)
        if v:
            stack.append(int(v, 16))
    r['stack'] = stack
    code = []
    for a, words in re.findall(r'^[ \t]*(%s):((?:[ \t]+%s)+)' % (TOKEN, TOKEN), text, re.M):
        a = fix_hex(a, fixes)
        if not a:
            continue
        for i, w in enumerate(words.split()):
            w = fix_hex(w, fixes)
            if w:
                code.append((int(a, 16) + 4 * i, int(w, 16)))
    r['code'] = code
    return r


def main():
    if len(sys.argv) < 3:
        die(__doc__.strip(), 2)
    elfpath, args = sys.argv[1], sys.argv[2:]
    elf, tools = Elf(elfpath), Tools(elfpath)
    force = os.environ.get('FORCE') == '1'

    frames = []          # (address as reported, address to look up, label)
    seen = set()

    def add(addr, lookup, label):
        if addr not in seen:
            seen.add(addr)
            frames.append((addr, lookup, label))

    reports = []
    raw = []
    for a in args:
        if os.path.isfile(a):
            reports.append(parse_report(open(a, errors='replace').read()))
        else:
            raw.append(int(a, 16))

    for rep in reports:
        if 'SRR0' in rep:
            add(rep['SRR0'], rep['SRR0'], 'faulting instruction')
        # LR and the stack dump hold RETURN addresses: the call is the instruction before them,
        # and looking up the return address itself can land in the next line, or the next inlined function
        if 'LR' in rep:
            add(rep['LR'], rep['LR'] - 4, 'return address (LR)')
        for a in rep['stack']:
            add(a, a - 4, 'return address')
    for a in raw:
        add(a, a, 'address')

    for rep in reports:
        if rep['fixes']:
            print('note: %d value(s) in the report are not hex and were read as a misread digit: %s\n' % (
                len(rep['fixes']), ', '.join('%s->%s' % f for f in rep['fixes'])))

    # ---- is this the reporter's build? ---------------------------------------------------------
    for rep in reports:
        code = rep['code']
        if not code:
            print('note: no CODE DUMP in the report, so nothing confirms this ELF is the reporter\'s build.')
            print('      Check the report\'s SHA-256 against sha256sums.txt in the same debug artifact first.\n')
            continue
        first_bad = None
        checked = 0
        for addr, word in code:
            have = elf.read(addr, 4)
            if have is None or struct.unpack('>I', have)[0] != word:
                first_bad = (addr, word, None if have is None else struct.unpack('>I', have)[0])
                break
            checked += 1
        if first_bad is None:
            print('CODE DUMP: all %d words match this ELF at 0x%08x. This is the reporter\'s build.\n' % (checked, code[0][0]))
        else:
            addr, word, have = first_bad
            print('=' * 78)
            print('CODE DUMP DOES NOT MATCH THIS ELF - it is not the build that crashed.')
            print('  0x%08x: the report has %08X, this ELF has %s' % (addr, word, 'nothing (address not in its code)' if have is None else '%08X' % have))
            print('  Every function name below would be about a different program. Get the ELF whose')
            print('  sha256sums.txt lists the reporter\'s hash (or rebuild at the commit they built).')
            print('=' * 78)
            if not force:
                print('Not symbolizing. FORCE=1 shows it anyway.')
                sys.exit(2)
            print('FORCE=1: symbolizing anyway; do not trust the result.\n')

    # ---- what the faulting access was ----------------------------------------------------------
    for rep in reports:
        if 'DAR' in rep:
            v = rep['DAR']
            note = classify(v)
            if v & 3 and 'MEM' not in note:
                note += '; also not 4-byte aligned, so not a real pointer (bytes %s)' % ' '.join('%02x' % b for b in v.to_bytes(4, 'big'))
            elif v & 3:
                note += '; not 4-byte aligned'
            print('DAR (faulting data address): 0x%08x - %s' % (v, note))
            rv = [n for n, g in rep['gpr'].items() if g == v]
            if rv:
                print('  that value is in %s: the faulting access went through that register' % ', '.join('r%d' % n for n in rv))

    # ---- functions -----------------------------------------------------------------------------
    print('\n== functions (innermost inlined frame first) ==')
    for addr, lookup, label in frames:
        out = tools.run('addr2line', '-f', '-C', '-i', '-p', '-e', elfpath, '0x%x' % lookup).rstrip().splitlines()
        tag = '0x%08x' % addr + ('' if addr == lookup else ' (call at 0x%08x)' % lookup)
        print('%s  %s' % (tag, out[0] if out else '??'))
        for l in out[1:]:
            print(' ' * 12 + l)

    # ---- registers: what do they point at in this build? ---------------------------------------
    for rep in reports:
        rows = []
        for n, v in sorted(rep['gpr'].items()):
            s = elf.section_of(v)
            if not s:
                continue
            where = '%s+0x%x' % (s['name'], v - s['addr'])
            sym = tools.symbol_at(v)
            extra = ''
            if not (s['flags'] & Elf.SHF_EXEC) and not (s['flags'] & Elf.SHF_WRITE):
                b = elf.cstring(v)
                if b and len(b) >= 3 and all(32 <= c < 127 for c in b):
                    extra = '  "%s"' % b.decode()
            rows.append('  r%-2d = 0x%08x  %s%s%s' % (n, v, sym or where, '' if not sym else '  [%s]' % s['name'], extra))
        if rows:
            print('\n== registers that point into this binary ==')
            print('\n'.join(rows))

    # ---- disassembly ---------------------------------------------------------------------------
    pc = frames[0][0] if frames else None
    if pc is not None:
        print('\n== disassembly around 0x%08x ==' % pc)
        out = tools.run('objdump', '-d', '-C', '--start-address=0x%x' % (pc - 32), '--stop-address=0x%x' % (pc + 32), elfpath)
        for line in out.splitlines()[6:]:
            m = re.match(r'\s*([0-9a-f]+):', line)
            print(line + ('    <<<<<< faulting instruction' if m and int(m.group(1), 16) == pc and reports else ''))


if __name__ == '__main__':
    main()
