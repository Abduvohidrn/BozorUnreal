"""Binary FBX'dagi mesh'lar: Geometry nomi, uchlar soni, chegarasi va unga ulangan Model transformi - tahlil uchun.
Ishlatish: python fbx_meshes.py <fayl.fbx>
"""
import struct
import sys
import zlib

sys.path.insert(0, __file__.replace("\\", "/").rsplit("/", 1)[0])
from fbx_rename_root import read_nodes, parse_props


def arr(node, fmt="d"):
    raw = parse_props(node[2], node[1])[0][1]
    n, enc, clen = struct.unpack_from("<III", raw, 0)
    data = raw[12:12 + clen]
    data = zlib.decompress(data) if enc else data
    return struct.unpack("<%d%s" % (n, fmt), data)


def props70(node):
    out = {}
    for c in node[3]:
        if c[0] == b"Properties70":
            for p in c[3]:
                pp = parse_props(p[2], p[1])
                out[pp[0][1].decode()] = [struct.unpack("<d", v)[0] for t, v in pp[4:] if t == b"D"]
    return out


def load(path):
    data = open(path, "rb").read()
    version = struct.unpack_from("<I", data, 23)[0]
    nodes, _ = read_nodes(data, 27, len(data), version >= 7500)
    objects = next(n for n in nodes if n[0] == b"Objects")
    conns = next(n for n in nodes if n[0] == b"Connections")
    info = {}
    for o in objects[3]:
        pp = parse_props(o[2], o[1])
        oid = struct.unpack("<q", pp[0][1])[0]
        info[oid] = (o[0].decode(), pp[1][1].split(b"\x00\x01")[0].decode(), o)
    links = []
    for c in conns[3]:
        pp = parse_props(c[2], c[1])
        links.append((struct.unpack("<q", pp[1][1])[0], struct.unpack("<q", pp[2][1])[0]))
    return info, links


if __name__ == "__main__":
    info, links = load(sys.argv[1])
    for oid, (kind, name, node) in info.items():
        if kind != "Geometry":
            continue
        v = next((arr(c) for c in node[3] if c[0] == b"Vertices"), ())
        xs, ys, zs = v[0::3], v[1::3], v[2::3]
        model = next((info[d] for s, d in links if s == oid and d in info and info[d][0] == "Model"), None)
        p = props70(model[2]) if model else {}
        print("%-40s n=%5d x[%.1f,%.1f] y[%.1f,%.1f] z[%.1f,%.1f] model=%s T=%s R=%s S=%s" % (
            name, len(v) // 3, min(xs), max(xs), min(ys), max(ys), min(zs), max(zs), model[1] if model else None,
            p.get("Lcl Translation"), p.get("Lcl Rotation"), p.get("Lcl Scaling")))
