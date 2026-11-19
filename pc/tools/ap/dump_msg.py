# Dump AC (US GC) messages with decoded control codes.
# usage: dump_msg.py [msg|a-b ...] [--select] ; --select dumps select_data (choice strings) instead
# ROM: the first build/windows32/bin/rom/*.ciso (read only). Also imported by dump_villagers.py.
import struct, sys, re, os, glob
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
ROM = glob.glob(REPO + "/build/windows32/bin/rom/*.ciso")[0]
f = open(ROM, "rb"); hdr = f.read(0x8000)
assert hdr[:4] == b"CISO"
bs = struct.unpack("<I", hdr[4:8])[0]
phys = {}; n = 0
for i, b in enumerate(hdr[8:0x8000]):
    if b: phys[i] = n; n += 1
def read(off, size):
    out = bytearray()
    while size:
        blk, o = divmod(off, bs); take = min(size, bs - o)
        if blk in phys: f.seek(0x8000 + phys[blk] * bs + o); out += f.read(take)
        else: out += bytes(take)
        off += take; size -= take
    return bytes(out)
def gc_file(want):
    fo, fs = struct.unpack(">II", read(0x424, 8)); fst = read(fo, fs)
    cnt = struct.unpack(">I", fst[8:12])[0]; strs = fst[cnt*12:]
    for i in range(1, cnt):
        w, a, b = struct.unpack(">III", fst[i*12:i*12+12])
        if not (w >> 24) and strs[w & 0xFFFFFF:].split(b"\0")[0].decode() == want: return read(a, b)
    raise KeyError(want)
def yaz0(src):
    if src[:4] != b"Yaz0": return src
    dsz = struct.unpack(">I", src[4:8])[0]; out = bytearray(); p = 16
    while len(out) < dsz:
        code = src[p]; p += 1
        for bit in range(8):
            if len(out) >= dsz: break
            if code & (0x80 >> bit): out.append(src[p]); p += 1
            else:
                b1, b2 = src[p], src[p+1]; p += 2
                d = ((b1 & 0xF) << 8 | b2) + 1; ln = b1 >> 4
                if ln == 0: ln = src[p] + 0x12; p += 1
                else: ln += 2
                s = len(out) - d
                for k in range(ln): out.append(out[s + k])
    return bytes(out)
def rarc_files(arc):
    arc = yaz0(arc); assert arc[:4] == b"RARC", arc[:4]
    data_off = struct.unpack(">I", arc[0xC:0x10])[0] + 0x20
    info = 0x20
    nfiles, foff = struct.unpack(">II", arc[info+8:info+16])
    stroff = struct.unpack(">I", arc[info+0x14:info+0x18])[0]
    fe = info + foff; st = info + stroff; out = {}
    for i in range(struct.unpack(">I", arc[info+8:info+12])[0]):
        e = fe + i*20
        _, _, attr_noff, doff, dsz = struct.unpack(">HHIII", arc[e:e+16])
        attr = attr_noff >> 24; noff = attr_noff & 0xFFFFFF
        name = arc[st+noff:].split(b"\0")[0].decode()
        if attr & 2: continue  # dir
        out[name] = yaz0(arc[data_off+doff:data_off+doff+dsz])
    return out
cache = os.path.join(REPO, "build", "msgcache")  # extracted ROM archives, regenerated on demand
def res(arcname, name):
    p = os.path.join(cache, name)
    if not os.path.exists(p):
        os.makedirs(cache, exist_ok=True)
        for k, v in rarc_files(gc_file(arcname)).items(): open(os.path.join(cache, k), "wb").write(v)
    return open(p, "rb").read()
# control code names/sizes from the decomp
hdrtxt = open(REPO + "/include/m_font.h").read()
names = re.findall(r"mFont_CONT_CODE_(\w+)", hdrtxt[hdrtxt.index("mFont_CONT_CODE_BEGIN"):hdrtxt.index("mFont_CONT_CODE_NUM")])
names = [x for x in names if x not in ("BEGIN",)]  # LAST = BEGIN = 0
tbl = open(REPO + "/src/game/m_font_main.c_inc").read()
tbl = tbl[tbl.index("mFont_cont_info_tbl"):]; tbl = tbl[:tbl.index("};")]
sizes = [int(x) for x in re.findall(r"\{\s*(\d+),", tbl)]
assert len(names) == len(sizes), (len(names), len(sizes))
# charset: ASCII for 0x20..0x7E except a few; others from CHAR_ defines
chars = {}
for m in re.finditer(r"#define CHAR_(\w+) (\d+)", hdrtxt): chars[int(m.group(2))] = m.group(1)
ascii_ok = set(range(0x41, 0x5B)) | set(range(0x61, 0x7B)) | set(range(0x30, 0x3A)) | set(b" !\"%&'(),-.:<=>?@_")
special = {205: "\n", 213: "“", 214: "”", 215: "‘", 216: "’", 144: "-", 208: ";", 209: "#", 42: "~"}
def body(table_name, data_name, arcname, idx):
    t = res(arcname, table_name); d = res(arcname, data_name)
    ofs = [struct.unpack(">I", t[i:i+4])[0] for i in range(0, len(t), 4)]
    start = 0 if idx == 0 else ofs[idx-1]; end = ofs[idx]
    return d[start:end]
def sel(i): return decode(body("select_data_table.bin", "select_data.bin", "forest_1st.arc", i), False)
def decode(b, expand=True):
    out = []; i = 0
    while i < len(b):
        c = b[i]
        if c == 0x7F:
            code = b[i+1]; sz = sizes[code] if code < len(sizes) else 2
            nm = names[code] if code < len(names) else "?%d" % code
            args = b[i+2:i+sz]
            if nm.startswith("SET_DEMO_ORDER"):
                s = "<%s idx=%d val=%d>" % (nm, args[0], args[1] << 8 | args[2])
            elif nm.startswith("SET_NEXT_MESSAGE") and "RANDOM" not in nm:
                s = "<%s -> %d>" % (nm, args[0] << 8 | args[1])
            elif nm.startswith("SET_NEXT_MESSAGE_RANDOM") and nm != "SET_NEXT_MESSAGE_RANDOM_SECTION":
                s = "<%s -> %s>" % (nm, [args[k] << 8 | args[k+1] for k in range(0, len(args), 2)])
            elif nm.startswith("SET_SELECT_STRING"):
                ids = [args[k] << 8 | args[k+1] for k in range(0, len(args), 2)]
                s = "<%s %s>" % (nm, ", ".join("%d=%r" % (x, sel(x)) for x in ids) if expand else ids)
            elif nm in ("LAST", "CONTINUE", "CLEAR", "BUTTON", "SET_SELECT_WINDOW", "SET_FORCE_NEXT"):
                s = "<%s>" % nm + ("\n" if nm in ("CLEAR", "LAST") else "")
            elif nm == "CURSOR_SET_TIME":
                s = "<PAUSE %d>" % args[0]
            else:
                s = "<%s%s>" % (nm, (" " + args.hex()) if args else "")
            out.append(s); i += sz
        elif c == 0x80:
            out.append("<TAG %02x>" % b[i+1]); i += 2
        elif c in special: out.append(special[c]); i += 1
        elif c in ascii_ok: out.append(chr(c)); i += 1
        else: out.append("{%s}" % chars.get(c, "%02x" % c)); i += 1
    return "".join(out)
if __name__ == "__main__":
    args = sys.argv[1:]
    fn = (lambda i: sel(i)) if "--select" in args else \
         (lambda i: decode(body("message_data_table.bin", "message_data.bin", "forest_2nd.arc", i)))
    for a in args:
        if a.startswith("--"): continue
        lo, hi = (a.split("-") + [a])[:2]
        for i in range(int(lo), int(hi) + 1):
            print("=== %d ===" % i); print(fn(i)); print()
