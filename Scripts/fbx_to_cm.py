"""Blender FBX'ini (metr, UnitScaleFactor=100) santimetrga o'tkazadi va armatura nomini almashtiradi.

Unreal Interchange metrli FBX'ni birinchi suyakka (Root) x100 masshtab qo'yib import qiladi; IK Retargeter esa suyak
masshtabini yozmaydi, natijada retarget qilingan animatsiyada tana buziladi. Bu skript masofalarni o'zi x100 qiladi:
Model "Lcl Translation", Geometry "Vertices", Cluster Transform/TransformLink va BindPose matritsalarining ko'chish qismi;
GlobalSettings UnitScaleFactor = 1. Natijada Unreal hech qanday suyakka masshtab qo'ymaydi.
Ro'mol kiygan odamlarda soch ro'mol ichiga suriladi (fbx_tuck_hair.py).
Ishlatish: python fbx_to_cm.py <kirish.fbx> <chiqish.fbx> <eski_armatura_nomi> <yangi_nom>
"""
import struct
import sys
import zlib

sys.path.insert(0, __file__.replace("\\", "/").rsplit("/", 1)[0])
from fbx_rename_root import HEADER, FOOT_MAGIC, read_nodes, parse_props, build_props, write_nodes, rename
from fbx_tuck_hair import tuck_nodes

K = 100.0


def get_array(raw):
    n, enc, clen = struct.unpack_from("<III", raw, 0)
    data = raw[12:12 + clen]
    return n, (zlib.decompress(data) if enc else data)


def put_array(n, data):
    return struct.pack("<III", n, 0, len(data)) + data


def scale_doubles(node, scale_all=False, translation_of_matrix=False):
    """node xususiyatlaridagi 'd' massivni o'zgartiradi."""
    pp = parse_props(node[2], node[1])
    t, raw = pp[0]
    if t != b"d":
        return False
    n, data = get_array(raw)
    vals = list(struct.unpack("<%dd" % n, data))
    if scale_all:
        vals = [v * K for v in vals]
    if translation_of_matrix and n == 16:
        vals[12] *= K
        vals[13] *= K
        vals[14] *= K
    pp[0] = (t, put_array(n, struct.pack("<%dd" % n, *vals)))
    node[2] = build_props(pp)
    return True


def fix_properties70(node, keys, value=None, mul=None):
    for c in node[3]:
        if c[0] != b"Properties70":
            continue
        for p in c[3]:
            pp = parse_props(p[2], p[1])
            if pp[0][1].decode() not in keys:
                continue
            for i in range(4, len(pp)):
                if pp[i][0] == b"D":
                    v = struct.unpack("<d", pp[i][1])[0]
                    v = value if value is not None else v * mul
                    pp[i] = (b"D", struct.pack("<d", v))
            p[2] = build_props(pp)


def convert(nodes):
    counts = {"model": 0, "vertices": 0, "matrix": 0}
    for top in nodes:
        if top[0] == b"GlobalSettings":
            fix_properties70(top, {"UnitScaleFactor", "OriginalUnitScaleFactor"}, value=1.0)
        if top[0] != b"Objects":
            continue
        for o in top[3]:
            if o[0] == b"Model":
                fix_properties70(o, {"Lcl Translation"}, mul=K)
                counts["model"] += 1
            elif o[0] == b"Geometry":
                for c in o[3]:
                    if c[0] == b"Vertices" and scale_doubles(c, scale_all=True):
                        counts["vertices"] += 1
            elif o[0] == b"Deformer":
                for c in o[3]:
                    if c[0] in (b"Transform", b"TransformLink") and scale_doubles(c, translation_of_matrix=True):
                        counts["matrix"] += 1
            elif o[0] == b"Pose":
                for pn in o[3]:
                    if pn[0] == b"PoseNode":
                        for c in pn[3]:
                            if c[0] == b"Matrix" and scale_doubles(c, translation_of_matrix=True):
                                counts["matrix"] += 1
    return counts


def main(src, dst, old, new):
    data = open(src, "rb").read()
    assert data[:23] == HEADER, "binary FBX emas"
    version = struct.unpack_from("<I", data, 23)[0]
    wide = version >= 7500
    nodes, _ = read_nodes(data, 27, len(data), wide)
    renamed = rename(nodes, old.encode(), new.encode())
    counts = convert(nodes)
    counts["soch_surildi"] = sum(r[3][0] for r in tuck_nodes(nodes))   # santimetrda ishlaydi: convert'dan keyin
    out = bytearray(data[:27])
    write_nodes(out, nodes, wide)
    out += b"\x00" * (25 if wide else 13)
    out += b"\xfa\xbc\xab\x09\xd0\xc8\xd4\x66\xb1\x76\xfb\x83\x1c\xf7\x26\x7e"
    out += b"\x00" * 4
    pad = ((len(out) + 15) & ~15) - len(out)
    out += b"\x00" * (pad or 16)
    out += struct.pack("<I", version) + b"\x00" * 120 + FOOT_MAGIC
    open(dst, "wb").write(out)
    print("%s: nom=%d %s" % (dst, renamed, counts))


if __name__ == "__main__":
    main(*sys.argv[1:5])
