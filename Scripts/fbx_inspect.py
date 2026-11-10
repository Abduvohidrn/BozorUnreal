"""Binary FBX'dagi Model tugunlari (nom, tur, Lcl Translation/Rotation/Scaling) va matritsalar ro'yxati - tahlil uchun."""
import struct
import sys

sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
from fbx_rename_root import read_nodes, parse_props

data = open(sys.argv[1], "rb").read()
version = struct.unpack_from("<I", data, 23)[0]
nodes, _ = read_nodes(data, 27, len(data), version >= 7500)


def find(nodes, name):
    for n in nodes:
        if n[0] == name:
            yield n


def props70(node):
    out = {}
    for c in node[3]:
        if c[0] == b"Properties70":
            for p in c[3]:
                pp = parse_props(p[2], p[1])
                key = pp[0][1].decode()
                vals = [struct.unpack("<d", v)[0] if t == b"D" else v for t, v in pp[4:]]
                out[key] = vals
    return out


objects = next(find(nodes, b"Objects"))
for m in objects[3]:
    if m[0] == b"Model":
        pp = parse_props(m[2], m[1])
        oid = struct.unpack("<q", pp[0][1])[0]
        name = pp[1][1].split(b"\x00\x01")[0].decode()
        kind = pp[2][1].decode()
        p = props70(m)
        print(oid, name, kind, "T", p.get("Lcl Translation"), "R", p.get("Lcl Rotation"), "S", p.get("Lcl Scaling"))
    if m[0] == b"Deformer":
        pp = parse_props(m[2], m[1])
        name = pp[1][1].split(b"\x00\x01")[0].decode()
        for c in m[3]:
            if c[0] == b"TransformLink":
                arr = parse_props(c[2], c[1])[0][1]
                n, enc, clen = struct.unpack_from("<III", arr, 0)
                import zlib
                raw = arr[12:12 + clen]
                raw = zlib.decompress(raw) if enc else raw
                vals = struct.unpack("<16d", raw)
                print("  cluster", name, "TransformLink col0 norm=%.3f t=(%.3f,%.3f,%.3f)" % ((vals[0] ** 2 + vals[1] ** 2 + vals[2] ** 2) ** 0.5, vals[12], vals[13], vals[14]))
                break
    if m[0] == b"Pose":
        for pn in m[3]:
            if pn[0] == b"PoseNode":
                node_id = None
                for c in pn[3]:
                    if c[0] == b"Node":
                        node_id = struct.unpack("<q", parse_props(c[2], c[1])[0][1])[0]
                    if c[0] == b"Matrix":
                        arr = parse_props(c[2], c[1])[0][1]
                        n, enc, clen = struct.unpack_from("<III", arr, 0)
                        import zlib
                        raw = arr[12:12 + clen]
                        raw = zlib.decompress(raw) if enc else raw
                        vals = struct.unpack("<16d", raw)
                        print("  posenode", node_id, "col0 norm=%.3f t=(%.3f,%.3f,%.3f)" % ((vals[0] ** 2 + vals[1] ** 2 + vals[2] ** 2) ** 0.5, vals[12], vals[13], vals[14]))
gs = props70(next(find(nodes, b"GlobalSettings")))
print("GlobalSettings", {k: v for k, v in gs.items() if "Scale" in k or "Axis" in k})
