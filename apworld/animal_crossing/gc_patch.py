# Animal Crossing GameCube patcher (0.7.0): turns the player's own USA disc (GAFE01, rev 0) into the AP build.
# Only two disc files change: sys/main.dol and files/foresta.rel.szs. The apworld carries bsdiff4 deltas of those
# (data/gc/, made at release time by `make`), so no Nintendo code ships. The output is a new ISO with the files
# packed one after another (about 33 MB instead of the 1.4 GB of a full GameCube disc; Dolphin boots it).
#
# Input: .iso (full or trimmed) or .ciso. Other formats (.rvz, .gcz, .wia): convert to ISO in Dolphin first.
# Only the header, apploader and the files are read, so the input's padding and junk data don't matter.
# Every part that is read is checked against the vanilla disc's sha1s (manifest "disc"), and the written ISO is
# read back and checked too, so a bad dump or a failed write is reported here instead of as "doesn't boot".
#
# CLI (needs bsdiff4; Archipelago ships it):
#   python gc_patch.py patch <input disc> <output.iso>
#   python gc_patch.py make <vanilla disc> <static.dol> <foresta.rel.szs> <version> <out dir>   (release step)
# No imports from Archipelago here, so it also runs on its own (uv run --with bsdiff4 python gc_patch.py ...).
import hashlib
import json
import os
import pkgutil
import struct
import sys

import bsdiff4

GAME_ID = b"GAFE01"
DOL = "main.dol"  # sys/main.dol: not in the file table, found through the disc header
FILES = [DOL, "foresta.rel.szs"]
ALIGN = 0x8000  # where files start, as on Nintendo's discs
DATA_DIR = "data/gc"


class PatchError(Exception):
    pass


def _sha1(data):
    return hashlib.sha1(data).hexdigest()


def _align(n, a=ALIGN):
    return (n + a - 1) // a * a


class Disc:
    # Read-only view of a GameCube disc image: plain ISO or CISO
    def __init__(self, path):
        self.f = open(path, "rb")
        head = self.f.read(0x8000)
        self.ciso_blocks = None
        if head[:4] == b"CISO":
            self.block = struct.unpack("<I", head[4:8])[0]
            self.ciso_blocks = {}
            for i, used in enumerate(head[8:0x8000]):
                if used:
                    self.ciso_blocks[i] = len(self.ciso_blocks)
        elif head[:4] in (b"RVZ\x01", b"WIA\x01") or head[:4] == b"\x01\xc0\x0b\xb1":
            raise PatchError("This disc image is compressed (RVZ / WIA / GCZ). In Dolphin, right-click the game, "
                             "Convert File..., format ISO, then patch the .iso.")
        game = self.read(0, 8)
        if game[:6] != GAME_ID:
            raise PatchError("Not the USA Animal Crossing disc (game id %r, want GAFE01)." % game[:6].decode("ascii", "replace"))
        if game[7] != 0:
            raise PatchError("Disc revision %d, only revision 0 is supported." % game[7])

    def close(self):
        self.f.close()

    def read(self, off, size):
        if self.ciso_blocks is None:
            self.f.seek(off)
            data = self.f.read(size)
            if len(data) != size:
                raise PatchError("The disc image is cut short (read past its end).")
            return data
        out = bytearray()
        while size:
            blk, o = divmod(off, self.block)
            take = min(size, self.block - o)
            if blk in self.ciso_blocks:
                self.f.seek(0x8000 + self.ciso_blocks[blk] * self.block + o)
                out += self.f.read(take)
            else:
                out += bytes(take)
            off += take
            size -= take
        return bytes(out)


def _dol_size(dol_head):
    # End of the furthest of the 7 text + 11 data sections
    offs = struct.unpack(">18I", dol_head[0:0x48])
    sizes = struct.unpack(">18I", dol_head[0x90:0xD8])
    return max(o + s for o, s in zip(offs, sizes) if s)


class Layout:
    # What the patcher needs from a disc: the area before the DOL (boot.bin, bi2.bin, apploader), the DOL,
    # the file table, and each file's place in it
    def __init__(self, disc):
        self.header = bytearray(disc.read(0, 0x440))
        self.dol_off, fst_off, fst_size = struct.unpack(">III", self.header[0x420:0x42C])
        self.boot_area = bytearray(disc.read(0, self.dol_off))
        apploader_end = 0x2440 + 0x20 + sum(struct.unpack(">II", self.boot_area[0x2454:0x245C]))
        if apploader_end > self.dol_off:
            raise PatchError("Unexpected disc layout (apploader runs into the DOL).")
        self.dol = disc.read(self.dol_off, _dol_size(disc.read(self.dol_off, 0x100)))
        self.fst = bytearray(disc.read(fst_off, fst_size))
        count = struct.unpack(">I", self.fst[8:12])[0]
        names = self.fst[count * 12:]
        self.entries = []  # (fst index, name, disc offset, size), files only, in disc order
        for i in range(1, count):
            word, off, size = struct.unpack(">III", self.fst[i * 12:i * 12 + 12])
            if not word >> 24:
                name = names[word & 0xFFFFFF:].split(b"\0")[0].decode("ascii")
                self.entries.append((i, name, off, size))
        self.entries.sort(key=lambda e: e[2])

    def boot_sha1(self):
        # Boot area without the FST offset / size fields (0x424-0x42F): those differ on trimmed or rebuilt discs
        boot = bytearray(self.boot_area)
        boot[0x424:0x430] = bytes(12)
        return _sha1(boot)

    def hashes(self, disc):
        # sha1 of each part: "boot area", main.dol and every file
        out = {"boot area": self.boot_sha1(), DOL: _sha1(self.dol)}
        for _, name, off, size in self.entries:
            out[name] = _sha1(disc.read(off, size))
        return out

    def file(self, disc, name):
        if name == DOL:
            return self.dol
        for _, n, off, size in self.entries:
            if n == name:
                return disc.read(off, size)
        raise PatchError("File %s is missing from the disc." % name)


def write_iso(disc, layout, replace, out_path):
    # New ISO: boot area as is, then the DOL, the file table and the files, each aligned; `replace` = name -> bytes
    dol = replace.get(DOL, layout.dol)
    fst_off = _align(layout.dol_off + len(dol))
    pos = _align(fst_off + len(layout.fst))
    fst = bytearray(layout.fst)
    plan = []
    for index, name, off, size in layout.entries:
        new_size = len(replace[name]) if name in replace else size
        struct.pack_into(">II", fst, index * 12 + 4, pos, new_size)
        plan.append((pos, name, off, size))
        pos = _align(pos + new_size)
    boot = bytearray(layout.boot_area)
    struct.pack_into(">III", boot, 0x424, fst_off, len(fst), len(fst))  # FST offset, size, max size
    tmp = out_path + ".tmp"
    with open(tmp, "wb") as out:
        out.write(boot)
        out.seek(layout.dol_off)
        out.write(dol)
        out.seek(fst_off)
        out.write(fst)
        for new_off, name, off, size in plan:
            out.seek(new_off)
            out.write(replace[name] if name in replace else disc.read(off, size))
        out.truncate(out.tell())
    os.replace(tmp, out_path)


def _load_data(name):
    data = pkgutil.get_data(__package__, DATA_DIR + "/" + name) if __package__ else \
        open(os.path.join(os.path.dirname(os.path.abspath(__file__)), DATA_DIR, name), "rb").read()
    if data is None:
        raise FileNotFoundError(name)
    return data


def load_manifest():
    try:
        return json.loads(_load_data("manifest.json"))
    except (FileNotFoundError, OSError):
        raise PatchError("This apworld has no GameCube patch (it is only included in release builds).")


def _mismatches(have, want):
    # Names whose sha1 differs from `want` (missing or extra files count too)
    return sorted(n for n in set(have) | set(want) if have.get(n) != want.get(n))


def patch(in_path, out_path):
    # Returns the manifest's version string
    manifest = load_manifest()
    if os.path.abspath(in_path) == os.path.abspath(out_path):
        raise PatchError("Pick a different output file than the input disc.")
    vanilla = manifest["disc"]
    disc = Disc(in_path)
    try:
        layout = Layout(disc)
        have = layout.hashes(disc)
        if all(have[n] == manifest["files"][n]["target_sha1"] for n in FILES):
            raise PatchError("This disc is already patched for this version. Play it as it is.")
        bad = _mismatches(have, vanilla)
        if bad:
            raise PatchError("This disc doesn't match the vanilla Animal Crossing (USA) disc: %s differ%s. It is a bad "
                             "or modified dump, or already patched by another version: patch an unmodified dump of "
                             "your disc." % (", ".join(bad), "s" if len(bad) == 1 else ""))
        replace = {}
        for name in FILES:
            info = manifest["files"][name]
            new = bsdiff4.patch(layout.file(disc, name), _load_data(info["delta"]))
            if _sha1(new) != info["target_sha1"]:
                raise PatchError("Patching %s gave the wrong result (damaged apworld?)." % name)
            replace[name] = new
        write_iso(disc, layout, replace, out_path)
    finally:
        disc.close()
    verify(out_path, manifest)
    return manifest["version"]


def verify(path, manifest):
    # Read the written ISO back: every part must be vanilla, except the patched files, which must be the build's
    want = dict(manifest["disc"])
    for name in FILES:
        want[name] = manifest["files"][name]["target_sha1"]
    disc = Disc(path)
    try:
        layout = Layout(disc)
        bad = _mismatches(layout.hashes(disc), want)
    except PatchError:
        bad = ["file cut short"]
    finally:
        disc.close()
    if bad:
        raise PatchError("The patched disc was written wrong (%s); disk full or blocked by antivirus? Try again, "
                         "maybe to another folder." % ", ".join(bad))


def make(vanilla_path, dol_path, rel_path, version, out_dir):
    # Release step: deltas of the built files against the vanilla disc + manifest.json in out_dir
    disc = Disc(vanilla_path)
    try:
        layout = Layout(disc)
        built = {DOL: open(dol_path, "rb").read(), "foresta.rel.szs": open(rel_path, "rb").read()}
        os.makedirs(out_dir, exist_ok=True)
        manifest = {"version": version, "game_id": GAME_ID.decode(), "disc": layout.hashes(disc), "files": {}}
        for name in FILES:
            old = layout.file(disc, name)
            delta = name + ".bsdiff4"
            with open(os.path.join(out_dir, delta), "wb") as f:
                f.write(bsdiff4.diff(old, built[name]))
            manifest["files"][name] = {"source_sha1": _sha1(old), "target_sha1": _sha1(built[name]), "delta": delta}
        with open(os.path.join(out_dir, "manifest.json"), "w") as f:
            json.dump(manifest, f, indent=2)
    finally:
        disc.close()


def run_gui():
    # Archipelago launcher component: pick the disc, pick where to save, patch
    import Utils
    try:
        version = load_manifest()["version"]
        src = Utils.open_filename("Animal Crossing (USA) disc image", [("GameCube disc", [".iso", ".ciso", ".gcm"])])
        if not src:
            return
        suggest = os.path.join(os.path.dirname(src), "Animal Crossing AP %s.iso" % version)
        dst = Utils.save_filename("Save the patched disc", [("GameCube disc", [".iso"])], suggest)
        if not dst:
            return
        patch(src, dst)
        Utils.messagebox("Animal Crossing GameCube Patcher", "Patched disc (version %s) saved to:\n%s" % (version, dst))
    except (PatchError, OSError) as e:
        Utils.messagebox("Animal Crossing GameCube Patcher", str(e), error=True)


def main(argv):
    if len(argv) == 3 and argv[0] == "patch":
        try:
            print("Patched (version %s): %s" % (patch(argv[1], argv[2]), argv[2]))
        except (PatchError, OSError) as e:
            sys.exit("Error: %s" % e)
    elif len(argv) == 6 and argv[0] == "make":
        make(*argv[1:])
    else:
        sys.exit("usage: gc_patch.py patch <in> <out.iso> | make <vanilla> <dol> <rel> <version> <out dir>")


if __name__ == "__main__":
    main(sys.argv[1:])
