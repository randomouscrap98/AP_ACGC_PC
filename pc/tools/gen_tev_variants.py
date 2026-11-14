#!/usr/bin/env python3
"""Build-time TEV shader variants for the Vulkan renderer (pc_gx_sdl.c).

GL compiles a specialized fragment shader per TEV config at runtime (pc_gx_tev.c). Vulkan needs SPIR-V and the
game ships no GLSL compiler, so this script makes SPIR-V for the configs in pc/include/pc_shader_seed.h at build
time; the game uses the uber shader for any other config.

    gen_tev_variants.py <pc_shader_seed.h> <default.frag> <glslangValidator> <out.bin> <work dir>
    gen_tev_variants.py --print-consts <pc_shader_seed.h>     (test: the const block of every key)

The const block must match pc_gx_tev_const_block() in pc/src/pc_gx_tev_source.c character for character
(ignore/tests checks it). Output file, little endian:
    "ACTV", u32 version (1), u32 key size, u32 count,
    count x { key bytes, u32 offset, u32 size } (offset from the start of the file),
    SPIR-V blobs.
"""
import os
import re
import struct
import subprocess
import sys

KEY_SIZE = 122  # sizeof(PCGXShaderKey)
STAGES = 3      # PC_GX_MAX_TEV_STAGES
STAGE_SIZE = 31


def read_seed(path):
    text = open(path).read()
    size = int(re.search(r"#define PC_SHADER_SEED_KEY_SIZE (\d+)", text).group(1))
    if size != KEY_SIZE:
        sys.exit("gen_tev_variants.py: seed key size %d, expected %d (PCGXShaderKey changed?)" % (size, KEY_SIZE))
    return [bytes(int(b) for b in row.split(",")) for row in re.findall(r"^    \{([\d,]+)\},$", text, re.M)]


def parse_key(k):
    # Same field order as PCGXShaderKey (pc_gx_internal.h)
    key = {
        "num_stages": k[0], "num_ind": k[1], "fog_enable": k[2],
        "alpha": k[3:6], "light": k[6:13], "swap_tbl": k[13:29], "st": [],
    }
    for s in range(STAGES):
        b = k[29 + s * STAGE_SIZE:29 + (s + 1) * STAGE_SIZE]
        key["st"].append({
            "cin": b[0:4], "ain": b[4:8], "cop": b[8], "aop": b[9], "bsc": b[10:14], "outc": b[14:18],
            "swap": b[18:20], "ksel": b[20:22], "tc_src": b[22], "use_tex": b[23], "ind": b[24:31],
        })
    return key


def ivec(values):
    return "ivec%d(%s)" % (len(values), ",".join(str(v) for v in values))


def array(type_, values):
    return "%s[%d](%s);\n" % (type_, len(values), ",".join(values))


def const_block(k):
    st = k["st"]
    out = "const int u_num_tev_stages = %d;\n" % k["num_stages"]
    out += "const ivec4 u_tev_color_in[3] = " + array("ivec4", [ivec(s["cin"]) for s in st])
    out += "const ivec4 u_tev_alpha_in[3] = " + array("ivec4", [ivec(s["ain"]) for s in st])
    out += "const int u_tev_color_op[3] = " + array("int", [str(s["cop"]) for s in st])
    out += "const int u_tev_alpha_op[3] = " + array("int", [str(s["aop"]) for s in st])
    out += "const int u_tev_tc_src[3] = " + array("int", [str(s["tc_src"]) for s in st])
    out += "const ivec4 u_tev_bsc[3] = " + array("ivec4", [ivec(s["bsc"]) for s in st])
    out += "const ivec4 u_tev_out[3] = " + array("ivec4", [ivec(s["outc"]) for s in st])
    out += "const ivec2 u_tev_swap[3] = " + array("ivec2", [ivec(s["swap"]) for s in st])
    out += "const ivec3 u_tev_ksel[3] = " + array("ivec3", [ivec([s["ksel"][0], s["ksel"][1], i])
                                                            for i, s in enumerate(st)])
    out += "const ivec4 u_tev_ind_cfg[3] = " + array("ivec4", [ivec(s["ind"][0:4]) for s in st])
    out += "const ivec3 u_tev_ind_wrap[3] = " + array("ivec3", [ivec(s["ind"][4:7]) for s in st])
    out += "const int u_use_texture[3] = " + array("int", [str(s["use_tex"]) for s in st])
    light = k["light"]
    out += "const ivec4 u_lighting_cfg0 = %s;\n" % ivec(light[0:4])
    out += "const ivec4 u_lighting_cfg1 = %s;\n" % ivec([light[4], light[5], light[6], 0])
    out += "const ivec4 u_swap_table[4] = " + array("ivec4", [ivec(k["swap_tbl"][t * 4:t * 4 + 4]) for t in range(4)])
    out += "const ivec3 u_alpha_ctrl = %s;\n" % ivec(k["alpha"])
    out += "const int u_num_ind_stages = %d;\n" % k["num_ind"]
    out += "const int u_fog_enable = %d;\n" % k["fog_enable"]
    return out


def variant_source(key, frag):
    # Like build_specialized_source in pc_gx_tev.c: #version, PC_SPECIALIZED, consts, the rest of default.frag
    body = frag.split("\n", 1)[1] if frag.startswith("#version") else frag
    return "#version 330 core\n#define PC_SPECIALIZED\n" + const_block(parse_key(key)) + body


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--print-consts":
        for key in read_seed(sys.argv[2]):
            sys.stdout.write(const_block(parse_key(key)) + "----\n")
        return
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    seed, frag_path, glslang, out_path, work = sys.argv[1:]
    keys = read_seed(seed)
    frag = open(frag_path).read()
    os.makedirs(work, exist_ok=True)

    def compile_one(i):
        src = os.path.join(work, "variant%03d.frag" % i)
        spv = os.path.join(work, "variant%03d.spv" % i)
        open(src, "w").write(variant_source(keys[i], frag))
        r = subprocess.run([glslang, "-V", "--target-env", "vulkan1.0", "-o", spv, src],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if r.returncode != 0:
            sys.exit("gen_tev_variants.py: %s failed:\n%s" % (src, r.stdout))
        return open(spv, "rb").read()

    blobs = [compile_one(i) for i in range(len(keys))]

    header = b"ACTV" + struct.pack("<III", 1, KEY_SIZE, len(keys))
    offset = len(header) + len(keys) * (KEY_SIZE + 8)
    table = b""
    for key, blob in zip(keys, blobs):
        table += key + struct.pack("<II", offset, len(blob))
        offset += len(blob)
    with open(out_path, "wb") as f:
        f.write(header + table + b"".join(blobs))
    print("gen_tev_variants.py: %d variants, %d bytes" % (len(keys), offset))


if __name__ == "__main__":
    main()
