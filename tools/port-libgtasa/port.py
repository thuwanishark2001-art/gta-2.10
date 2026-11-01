#!/usr/bin/env python3
"""Port the client's hard-coded libGTASA addresses to another game build.

The client (app/src/main/cpp/samp) patches and calls the engine at fixed
addresses written for GTA: San Andreas Android 2.10, as

    g_libGTASA + (VER_x32 ? 0x<arm32> + 1 : 0x<arm64>)
    g_libGTASA + 0x<addr>          (inside #if VER_x32 / #if !VER_x32)

libGTASA exports its symbols, so each address can be re-expressed as
"symbol + delta" in the old build and looked up again in the new one.
An address inside a function is only moved automatically when the
function's code is byte-identical in both builds (relocated words aside),
so the delta still lands on the same instruction. Everything else is
reported for a human to check.

Usage:
  port.py scan    OLD64.so OLD32.so                  coverage of the 2.10 addresses
  port.py check   OLD64.so OLD32.so                  armv7 and arm64 halves name the same symbol
  port.py structs LIB64.so LIB32.so                  VALIDATE_SIZE values against the engine's DWARF
  port.py port    OLD64.so OLD32.so NEW64.so NEW32.so [--write]
                                                     report, and with --write rewrite the sources

Either NEW library may be "-" to port only one architecture.
The libraries are the player's own copies; never commit them.
"""

import bisect
import os
import re
import struct
import subprocess
import sys
from collections import Counter

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.environ.get("PORT_SRC") or os.path.join(REPO, "app", "src", "main", "cpp", "samp")
def _find_readelf():
    import glob
    for sdk in (os.environ.get("ANDROID_NDK_HOME"), os.environ.get("ANDROID_HOME"), "/opt/android-sdk"):
        if not sdk:
            continue
        hits = sorted(glob.glob(os.path.join(sdk, "toolchains/llvm/prebuilt/*/bin/llvm-readelf")) +
                      glob.glob(os.path.join(sdk, "ndk/*/toolchains/llvm/prebuilt/*/bin/llvm-readelf")))
        if hits:
            return hits[-1]
    return "llvm-readelf"


READELF = os.environ.get("READELF") or _find_readelf()

TERNARY = re.compile(r"VER_x32\s*\?\s*(0x[0-9A-Fa-f]+)(\s*\+\s*1)?\s*:\s*(0x[0-9A-Fa-f]+)")
PLAIN = re.compile(r"g_libGTASA\s*\+\s*\(?\s*(0x[0-9A-Fa-f]+)(\s*\+\s*1\b)?")
# Sections an engine address can point into (not the ELF header at 0, so
# struct sizes and field offsets written the same way are left alone).
ADDRESS_SECTIONS = (".plt", ".text", ".rodata", ".data.rel.ro", ".init_array", ".fini_array",
                    ".got", ".got.plt", ".data", ".bss", ".ARM.extab", ".ARM.exidx")


# ---- ELF ---------------------------------------------------------------------

class Lib:
    def __init__(self, path):
        self.path = path
        self.data = open(path, "rb").read()
        self.segments = self._segments()
        self.word = 8 if "AARCH64" in self._run("-h") else 4
        self.syms = self._symbols()
        self._relocs()
        self.starts = [s[0] for s in self.syms]
        self.by_name = {}
        for s in self.syms:
            self.by_name.setdefault(s[3], s)

    def _run(self, *args):
        return subprocess.run([READELF, "-W", *args, self.path], capture_output=True, text=True, check=True).stdout

    def _segments(self):
        segs = []
        for line in self._run("-l").splitlines():
            parts = line.split()
            if parts and parts[0] == "LOAD":
                off, vaddr, _, filesz = (int(x, 16) for x in parts[1:5])
                segs.append((vaddr, vaddr + filesz, off))
        self.mapped, self.sections = [], {}
        for m in re.finditer(r"\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)", self._run("-S")):
            if m.group(1) in ADDRESS_SECTIONS:
                start = int(m.group(2), 16)
                self.sections[m.group(1)] = (start, start + int(m.group(3), 16))
                self.mapped.append((start, start + int(m.group(3), 16)))
        return segs

    def _symbols(self):
        """Function and data symbols, from .symtab when the build still has
        it (local statics included), else from the exported .dynsym."""
        found = {}
        for table in ("--syms", "--dyn-syms"):
            for line in self._run(table).splitlines():
                parts = line.split()
                if len(parts) < 8 or parts[3] not in ("FUNC", "OBJECT") or parts[6] == "UND":
                    continue
                value, size = int(parts[1], 16), int(parts[2], 0)
                if parts[3] == "FUNC":
                    value &= ~1  # thumb bit
                found.setdefault((value, parts[7]), (value, size, parts[3], parts[7]))
        return sorted(found.values())

    def _relocs(self):
        """GOT and pointer slots: offset -> (type, symbol, addend). RELATIVE
        slots carry no symbol; their addend is the target address (read from
        the slot itself on ARM32, which uses REL)."""
        self.relocs, self.slots = {}, {}
        word = self.word
        for line in self._run("-r").splitlines():
            parts = line.split()
            if len(parts) < 3 or not parts[2].startswith("R_"):
                continue
            offset = int(parts[0], 16)
            kind = parts[2].split("_", 2)[-1]
            rest = parts[3:]
            if kind == "RELATIVE":
                name = ""
                if rest:
                    addend = int(rest[0], 16)
                else:
                    raw = self.read(offset, word)
                    addend = struct.unpack("<I" if word == 4 else "<Q", raw)[0] if raw else 0
            else:
                name = rest[1] if len(rest) >= 2 else ""
                addend = int(rest[3], 16) * (-1 if rest[2] == "-" else 1) if len(rest) >= 4 else 0
            self.relocs[offset] = (kind, name, addend)
            self.slots.setdefault((kind, name, addend), []).append(offset)

    def string_at(self, addr):
        """The C string starting at addr in .rodata, if it is one."""
        start, end = self.sections.get(".rodata", (0, 0))
        if not start <= addr < end:
            return None
        raw = self.read(addr, min(256, end - addr)) or b""
        text = raw.split(b"\0", 1)[0]
        if len(text) >= 4 and all(32 <= c < 127 for c in text):
            return text
        return None

    def find_string(self, text):
        start, end = self.sections.get(".rodata", (0, 0))
        blob = self.read(start, end - start) or b""
        hits, i = [], blob.find(text + b"\0")
        while i >= 0:
            hits.append(start + i)
            i = blob.find(text + b"\0", i + 1)
        return hits

    def thumb_bit(self, addr):
        """1 for an odd ARM32 code address (a thumb entry point), else 0.
        Odd data addresses (a byte variable, a string) keep their low bit."""
        if self.word != 4 or not addr & 1:
            return 0
        return int(any(a <= addr < b for name, (a, b) in self.sections.items() if name in (".text", ".plt")))

    def is_address(self, addr):
        return any(a <= addr < b for a, b in self.mapped)

    def read(self, vaddr, size):
        for start, end, off in self.segments:
            if start <= vaddr and vaddr + size <= end:
                return self.data[off + vaddr - start: off + vaddr - start + size]
        return None

    def containing(self, addr):
        """(symbol, delta) for the symbol whose range holds addr, else None."""
        i = bisect.bisect_right(self.starts, addr) - 1
        while i >= 0 and self.syms[i][0] + max(self.syms[i][1], 1) > addr - 0x100000:
            start, size, kind, name = self.syms[i]
            if start <= addr < start + max(size, 1):
                return self.syms[i], addr - start
            i -= 1
        return None


def _shape_a64(code):
    """AArch64 instructions with PC-relative fields masked out: branch and
    literal offsets, ADRP pages, and the low 12 bits added to an ADRP base.
    Everything else (registers, struct offsets, constants) must match."""
    out, paged = [], set()
    for (insn,) in struct.iter_unpack("<I", code[:len(code) // 4 * 4]):
        rd, rn = insn & 0x1F, (insn >> 5) & 0x1F
        if insn & 0x9F000000 in (0x90000000, 0x10000000):      # ADRP / ADR
            insn &= 0x9F00001F; paged.add(rd)
        elif insn & 0x7C000000 == 0x14000000:                  # B / BL
            insn &= 0xFC000000
        elif insn & 0xFF000010 == 0x54000000 or insn & 0x7E000000 == 0x34000000:  # B.cond, CBZ/CBNZ
            insn &= 0xFF00001F
        elif insn & 0x7E000000 == 0x36000000:                  # TBZ/TBNZ
            insn &= 0xFFF8001F
        elif insn & 0x3B000000 == 0x18000000:                  # LDR literal
            insn &= 0xFF00001F
        elif rn in paged and (insn & 0x1F000000 == 0x11000000 or insn & 0x3B000000 == 0x39000000):
            insn &= 0xFFC003FF                                 # ADD/LDR/STR #:lo12: after ADRP
            if rd != rn:
                paged.discard(rd)
        else:
            paged.discard(rd)
        out.append(insn)
    return out


def _shape_thumb(code):
    """Thumb-2 halfwords with branch offsets masked and literal pools (the
    words PC-relative loads read, often address offsets) skipped."""
    half = [h for (h,) in struct.iter_unpack("<H", code[:len(code) // 2 * 2])]
    pool, i = set(), 0
    while i < len(half):
        h = half[i]
        if h & 0xF800 == 0x4800:                                # LDR Rt, [PC, #imm8*4]
            t = ((i * 2 + 4) & ~3) + (h & 0xFF) * 4
            pool.update((t, t + 2))
        elif h == 0xF8DF and i + 1 < len(half):                 # LDR.W Rt, [PC, #imm12]
            t = ((i * 2 + 4) & ~3) + (half[i + 1] & 0xFFF)
            pool.update((t, t + 2))
        elif h == 0xED9F and i + 1 < len(half):                 # VLDR Dd, [PC, #imm8*4]
            t = ((i * 2 + 4) & ~3) + (half[i + 1] & 0xFF) * 4
            pool.update((t, t + 2, t + 4, t + 6))
        i += 2 if h >> 11 in (0b11101, 0b11110, 0b11111) else 1
    out, i = [], 0
    while i < len(half):
        if i * 2 in pool:
            i += 1; continue
        h = half[i]
        if h >> 11 in (0b11101, 0b11110, 0b11111) and i + 1 < len(half):
            lo = half[i + 1]
            if h >> 11 == 0b11110 and lo & 0x8000:              # B.W / BL / BLX / B<c>.W
                out.append((0xF000, lo & 0xD000))
            elif h == 0xF8DF or h == 0xED9F:                    # literal loads
                out.append((h, lo & 0xF000))
            else:
                out.append((h, lo))
            i += 2
        else:
            if h & 0xF800 == 0x4800:
                h &= 0xFF00                                     # LDR literal
            elif h & 0xF000 == 0xD000 and h & 0x0F00 not in (0x0E00, 0x0F00):
                h &= 0xFF00                                     # B<c>
            elif h & 0xF800 == 0xE000:
                h &= 0xF800                                     # B
            elif h & 0xF500 == 0xB100:
                h &= 0xFD07                                     # CBZ/CBNZ
            out.append(h)
            i += 1
    return out


def same_code(old, new, osym, nsym, word):
    """True if a function is the same code in both builds: same size and the
    same instructions once PC-relative fields, which change whenever
    anything around the function moves, are masked out."""
    if osym[1] != nsym[1]:
        return False
    a, b = old.read(osym[0], osym[1]), new.read(nsym[0], nsym[1])
    if a is None or b is None:
        return False
    if a == b:
        return True
    shape = _shape_thumb if word == 4 else _shape_a64
    return shape(a) == shape(b)


# ---- source scan ---------------------------------------------------------------

def sites():
    """Yield (path, line_no, arch, value, span) for every address literal.
    arch is 32 or 64; span is the (start, end) of the hex literal in the line."""
    for root, dirs, files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d != "vendor"]
        for f in files:
            if not f.endswith((".cpp", ".h")):
                continue
            path = os.path.join(root, f)
            stack = []  # None, 32 or 64 per #if level
            for n, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
                s = line.strip()
                if re.match(r"#\s*if\s+VER_x32\b", s):
                    stack.append(32); continue
                if re.match(r"#\s*if\s+!\s*VER_x32\b", s):
                    stack.append(64); continue
                if re.match(r"#\s*if", s):
                    stack.append(None); continue
                if re.match(r"#\s*else", s) and stack:
                    stack[-1] = {32: 64, 64: 32}.get(stack[-1]); continue
                if re.match(r"#\s*endif", s) and stack:
                    stack.pop(); continue
                if s.startswith("//"):
                    continue
                taken = []
                for m in TERNARY.finditer(line):
                    yield path, n, 32, int(m.group(1), 16), m.span(1)
                    yield path, n, 64, int(m.group(3), 16), m.span(3)
                    taken.append(m.span())
                for m in PLAIN.finditer(line):
                    if any(a <= m.start(1) < b for a, b in taken):
                        continue
                    arch = next((x for x in reversed(stack) if x), None)
                    if m.group(2) or int(m.group(1), 16) & 1:
                        arch = arch or 32  # "+ 1" or an odd address: thumb code
                    yield path, n, arch, int(m.group(1), 16), m.span(1)


# ---- anchors -----------------------------------------------------------------
# An anchor says where an address is in build-independent terms.

def anchor(lib, addr):
    """("slot", kind, symbol, addend) for a GOT/pointer slot,
    ("sym", symbol, delta) for an address inside a symbol,
    None when nothing names it, or "not-address" for sizes and offsets."""
    if not lib.is_address(addr):
        return "not-address"
    hit = lib.containing(addr)
    if hit:
        return ("sym", hit[0][3], hit[1])
    if addr in lib.relocs:
        kind, name, addend = lib.relocs[addr]
        if not name:  # RELATIVE: describe the target instead
            target = lib.containing(addend & ~1 if lib.word == 4 else addend)
            if not target:
                return None
            name, addend = "@" + target[0][3], addend - target[0][0]
        return ("slot", kind, name, addend)
    text = lib.string_at(addr)
    if text:
        return ("str", text)
    return None


def resolve(old, new, anc, word):
    """New address for an anchor, or (None, reason)."""
    if anc[0] == "str":
        hits = new.find_string(anc[1])
        if len(hits) != 1:
            return None, f"string {anc[1]!r} found {len(hits)} times"
        return hits[0], None
    if anc[0] == "slot":
        _, kind, name, addend = anc
        if name.startswith("@"):
            sym = new.by_name.get(name[1:])
            if not sym:
                return None, f"slot points into {name[1:]}, which is gone"
            target = sym[0] + addend
            hits = [o for (k, n, a), offs in new.slots.items() if k == kind and not n
                    and a in (target, target | 1) for o in offs]
        else:
            hits = new.slots.get((kind, name, addend), [])
        if len(hits) != 1:
            return None, f"{len(hits)} {kind} slots for {name}+{addend:#x}"
        return hits[0], None
    _, name, delta = anc
    osym = old.by_name.get(name)
    nsym = new.by_name.get(name)
    if not nsym:
        return None, f"{name} is not in the new build"
    if delta and osym[2] == "FUNC" and not same_code(old, new, osym, nsym, word):
        return None, f"{name}+{delta:#x}: function body changed"
    if delta and osym[2] == "OBJECT" and delta >= nsym[1]:
        return None, f"{name}+{delta:#x}: object is smaller now"
    moved = nsym[0] + delta
    # A pointer slot inside a table (a vtable entry, say) must still hold
    # the same symbol.
    was = old.relocs.get(osym[0] + delta)
    if was and was[1]:
        now = new.relocs.get(moved)
        if not now or now[1] != was[1]:
            return None, f"{name}+{delta:#x} held {was[1]}, now {now[1] if now else 'nothing'}"
    return moved, None


def site_arch(libs, arch, value):
    if arch:
        return arch
    hits = [a for a in (64, 32) if libs[a].is_address(value)]
    return hits[0] if len(hits) == 1 else None


# ---- commands ------------------------------------------------------------------

def scan(old64, old32):
    libs = {64: Lib(old64), 32: Lib(old32)}
    counts, unresolved = Counter(), []
    for path, n, arch, value, _ in sites():
        arch = site_arch(libs, arch, value) or 64
        anc = anchor(libs[arch], value - libs[arch].thumb_bit(value))
        if anc == "not-address":
            counts["not an address"] += 1
        elif anc is None:
            counts["unnamed"] += 1
            unresolved.append((path, n, arch, value))
        elif anc[0] == "slot":
            counts["GOT/pointer slot"] += 1
        elif anc[0] == "str":
            counts["string"] += 1
        else:
            counts["symbol" if anc[2] == 0 else "symbol + delta"] += 1
    total = sum(counts.values())
    print(f"{total} literals")
    for k, v in counts.most_common():
        print(f"  {k:18} {v:5}  {100 * v / total:5.1f}%")
    print("\nNot named by any symbol or relocation (need manual porting):")
    for path, n, arch, value in unresolved:
        print(f"  {os.path.relpath(path, REPO)}:{n} arm{arch} 0x{value:x}")


def check(lib64, lib32):
    """Both halves of each VER_x32 ternary should name the same symbol.
    A mismatch means one of the two addresses is wrong in the source."""
    libs = {64: Lib(lib64), 32: Lib(lib32)}

    def name(arch, value):
        anc = anchor(libs[arch], value - libs[arch].thumb_bit(value))
        if anc in (None, "not-address") or anc[0] == "str":
            return None, anc
        sym = anc[1] if anc[0] == "sym" else anc[2].lstrip("@")
        # size_t and 32-bit ints mangle differently on the two ABIs.
        return re.sub(r"(?<=[^A-Za-z])[jmy](?=[^a-z]|$)|(?<=E)[jmy]", "?", sym), anc

    pairs, bad = 0, 0
    previous = None
    for path, n, arch, value, span in sites():
        if arch == 32:
            previous = (path, n, value)
            continue
        if arch != 64 or not previous or previous[:2] != (path, n):
            continue
        k32, a32 = name(32, previous[2])
        k64, a64 = name(64, value)
        previous = None
        if k32 is None or k64 is None:
            continue
        pairs += 1
        if k32 != k64:
            bad += 1
            print(f"{os.path.relpath(path, REPO)}:{n}\n  arm32 {a32[1:]}\n  arm64 {a64[1:]}")
    print(f"{pairs} address pairs, {bad} disagree")
    return 1 if bad else 0


NAMED = re.compile(r'"(_Z[A-Za-z0-9_]+)"')


def named_symbols():
    """Mangled names the client hooks or looks up by name ("_Z..." literals)."""
    names = {}
    for root, dirs, files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d != "vendor"]
        for f in files:
            if f.endswith((".cpp", ".h")):
                path = os.path.join(root, f)
                for n, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
                    if line.lstrip().startswith("//"):
                        continue
                    for m in NAMED.finditer(line):
                        names.setdefault(m.group(1), (path, n))
    return names


def check_names(old, new):
    """Names hooked by symbol that the old build has and the new one lacks."""
    missing = []
    for name, (path, n) in sorted(named_symbols().items()):
        for arch, lib in new.items():
            if lib and name in old[arch].by_name and name not in lib.by_name:
                missing.append((path, n, arch, name))
    return missing


SIZE_CHECK = re.compile(r"VALIDATE_SIZE\(\s*([A-Za-z_][\w:]*)\s*,\s*(.+?)\)\s*;?\s*(?://.*)?$")


def _size_of(expr, arch):
    """Value of a VALIDATE_SIZE size for one ABI, or None if it isn't a
    plain number or VER_x32 ternary of numbers."""
    expr = expr.strip()
    while expr.startswith("(") and expr.endswith(")"):
        expr = expr[1:-1].strip()
    m = re.fullmatch(r"VER_x32\s*\?\s*(\w+)\s*:\s*(\w+)", expr)
    if m:
        expr = m.group(1) if arch == 32 else m.group(2)
    try:
        return int(expr, 0)
    except ValueError:
        return None


def dwarf_sizes(path, names):
    """{name: set of byte sizes} from the library's DWARF, for complete
    struct/class/union definitions with those names."""
    dump = READELF.replace("llvm-readelf", "llvm-dwarfdump")
    args = [dump, "--show-children=false"]
    for n in names:
        args.append(f"--name={n}")
    out = subprocess.run(args + [path], capture_output=True, text=True).stdout
    sizes = {}
    for block in out.split("\n\n"):
        if not re.search(r"DW_TAG_(class|structure|union)_type", block):
            continue
        name = re.search(r'DW_AT_name\s+\("([^"]+)"\)', block)
        size = re.search(r"DW_AT_byte_size\s+\((0x[0-9a-f]+)\)", block)
        if name and size:
            sizes.setdefault(name.group(1), set()).add(int(size.group(1), 16))
    return sizes


# Our names for engine classes, where they differ.
ALIASES = {
    "CWidgetGta": "CWidget", "CObjectGta": "CObject", "CPedGTA": "CPed",
    "CPlayerPedGta": "CPlayerPed", "CPlayerInfoGta": "CPlayerInfo", "CVehicleGTA": "CVehicle",
    "CColSurface": "CColSurface", "tColLighting": "tColLighting",
}
# Size differences checked by hand and harmless.
KNOWN = {
    "CBoundingBox": "mobile's CBoundingBox also holds the bounding sphere; ours keeps the "
                    "sphere beside it in CColModel, whose size and layout match",
}


def structs(lib64, lib32):
    """Compare each VALIDATE_SIZE with the engine's own debug info."""
    checks = []
    for root, dirs, files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d != "vendor"]
        for f in files:
            if not f.endswith((".cpp", ".h")):
                continue
            path = os.path.join(root, f)
            for n, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
                m = SIZE_CHECK.search(line.strip())
                if m and not line.lstrip().startswith(("//", "#define")):
                    checks.append((path, n, m.group(1).split("::")[-1], m.group(2)))
    checks = [(p, n, ALIASES.get(name, name), e) for p, n, name, e in checks]
    names = sorted({c[2] for c in checks})
    found = {64: dwarf_sizes(lib64, names), 32: dwarf_sizes(lib32, names)}
    counts, bad = Counter(), 0
    for path, n, name, expr in checks:
        for arch in (32, 64):
            want = _size_of(expr, arch)
            have = found[arch].get(name)
            where = f"{os.path.relpath(path, REPO)}:{n} {name} arm{arch}"
            if want is None:
                counts["not a number"] += 1
            elif not have:
                counts["no debug info"] += 1
            elif want in have:
                counts["matches"] += 1
            elif name in KNOWN:
                counts["known, harmless"] += 1
            else:
                counts["DIFFERS"] += 1
                bad += 1
                print(f"  {where}: we say {want:#x}, the engine says {', '.join(hex(x) for x in sorted(have))}")
    print(f"{len(checks)} size checks")
    for k, v in counts.most_common():
        print(f"  {k:14} {v:5}")
    return 1 if bad else 0


def port(old64, old32, new64, new32, write):
    old = {64: Lib(old64), 32: Lib(old32)}
    new = {a: (Lib(p) if p != "-" else None) for a, p in ((64, new64), (32, new32))}
    edits, status, review = {}, Counter(), []
    for path, n, arch, value, span in sites():
        arch = site_arch(old, arch, value)
        if arch is None or new[arch] is None:
            continue
        thumb = old[arch].thumb_bit(value)
        anc = anchor(old[arch], value - thumb)
        if anc == "not-address":
            continue
        if anc is None:
            status["unnamed"] += 1
            review.append((path, n, arch, value, "not named by any symbol or relocation"))
            continue
        moved, why = resolve(old[arch], new[arch], anc, 4 if arch == 32 else 8)
        if moved is None:
            status["changed"] += 1
            review.append((path, n, arch, value, why))
            continue
        moved += thumb
        status["same" if moved == value else "moved"] += 1
        if moved != value:
            width = span[1] - span[0] - 2
            edits.setdefault(path, []).append((n, span, f"0x{moved:0{width}X}"))
    for path, n, arch, name in check_names(old, new):
        status["name gone"] += 1
        review.append((path, n, arch, 0, f"hooked by name, but {name} is not in the new build"))
    total = sum(status.values())
    print(f"{total} addresses and names checked")
    for k, v in status.most_common():
        print(f"  {k:10} {v:5}")
    if review:
        print("\nNeed a human (left unchanged):")
        for path, n, a, value, why in review:
            print(f"  {os.path.relpath(path, REPO)}:{n} arm{a} 0x{value:x}  {why}")
    if write:
        for path, changes in edits.items():
            lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
            for n, span, text in sorted(changes, key=lambda c: (c[0], -c[1][0])):
                line = lines[n - 1]
                lines[n - 1] = line[:span[0]] + text + line[span[1]:]
            open(path, "w", encoding="utf-8").write("\n".join(lines))
        print(f"\nRewrote {sum(len(c) for c in edits.values())} addresses in {len(edits)} files.")
        import hashlib
        print("\nThen, in app/src/main/java/com/samp/mobile/game/GameFiles.java:")
        for arch, lib in new.items():
            if lib is None:
                continue
            abi = "arm64-v8a" if arch == 64 else "armeabi-v7a"
            const = "REQUIRED_VERSION_ARM64" if arch == 64 else "REQUIRED_VERSION_ARMV7"
            print(f"  {const} = the new game version (versionName from the Play Store)")
            print(f'  KNOWN_GOOD {{ "{abi}/libGTASA.so", "{hashlib.sha256(lib.data).hexdigest()}" }}')
            print(f"  (and {abi}/libSCAnd.so from the same APK)")
    return 0 if not review else 1


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) == 3 and args[0] == "scan":
        scan(args[1], args[2])
    elif len(args) == 3 and args[0] == "structs":
        sys.exit(structs(args[1], args[2]))
    elif len(args) == 3 and args[0] == "check":
        sys.exit(check(args[1], args[2]))
    elif len(args) == 5 and args[0] == "port":
        sys.exit(port(*args[1:], "--write" in sys.argv))
    else:
        print(__doc__)
        sys.exit(2)
