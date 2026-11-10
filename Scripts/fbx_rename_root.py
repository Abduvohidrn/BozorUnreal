"""Binary FBX'dagi armatura (skelet ildizi) nomini almashtiradi: SK_Char_<Ism> -> Armature.

Blender har bir odamning armaturasini o'z nomi bilan yozgan, Unreal esa ildizi boshqa nomdagi skeletlarni
birlashtira olmaydi. Fayl daraxti o'qiladi, Model nomi o'zgartiriladi va offsetlar qayta hisoblanib yoziladi.
Ishlatish: python fbx_rename_root.py <kirish.fbx> <chiqish.fbx> <eski_nom> <yangi_nom>
"""
import struct
import sys

HEADER = b"Kaydara FBX Binary  \x00\x1a\x00"
FOOT_MAGIC = b"\xf8\x5a\x8c\x6a\xde\xf5\xd9\x7e\xec\xe9\x0c\xe3\x75\x8f\x29\x0b"


def read_nodes(data, pos, end, wide):
    nodes = []
    hdr = 25 if wide else 13
    while pos < end:
        if wide:
            end_off, nprops, plen = struct.unpack_from("<QQQ", data, pos)
            pos += 24
        else:
            end_off, nprops, plen = struct.unpack_from("<III", data, pos)
            pos += 12
        nlen = data[pos]
        pos += 1
        if end_off == 0:          # null record: ro'yxat tugadi
            return nodes, pos
        name = data[pos:pos + nlen]
        pos += nlen
        props = data[pos:pos + plen]   # xom holida (faqat kerakli string o'zgaradi)
        pos += plen
        children = []
        if pos < end_off:
            children, pos = read_nodes(data, pos, end_off, wide)
        pos = end_off
        nodes.append([name, nprops, props, children])
    return nodes, pos


def parse_props(raw, n):
    out, p = [], 0
    for _ in range(n):
        t = raw[p:p + 1]
        p += 1
        if t in b"SR":
            ln = struct.unpack_from("<I", raw, p)[0]
            out.append((t, raw[p + 4:p + 4 + ln]))
            p += 4 + ln
        elif t in b"fdlib":
            _, _, clen = struct.unpack_from("<III", raw, p)
            out.append((t, raw[p:p + 12 + clen]))
            p += 12 + clen
        else:
            size = {b"Y": 2, b"C": 1, b"I": 4, b"F": 4, b"D": 8, b"L": 8}[t]
            out.append((t, raw[p:p + size]))
            p += size
    return out


def build_props(props):
    b = bytearray()
    for t, v in props:
        b += t
        if t in b"SR":
            b += struct.pack("<I", len(v)) + v
        else:
            b += v
    return bytes(b)


def write_nodes(out, nodes, wide):
    hdr = 25 if wide else 13
    for name, nprops, props, children in nodes:
        start = len(out)
        out += b"\x00" * (hdr - 1)
        out += bytes([len(name)]) + name + props
        # FBX SDK: ichki ro'yxat (yoki xususiyatsiz tugun) null yozuv bilan tugaydi
        if children or nprops == 0:
            write_nodes(out, children, wide)
            out += b"\x00" * hdr
        end = len(out)
        fmt = "<QQQ" if wide else "<III"
        struct.pack_into(fmt, out, start, end, nprops, len(props))


def rename(nodes, old, new):
    count = 0
    for node in nodes:
        name, nprops, props, children = node
        if name == b"Model" or name == b"NodeAttribute":
            pp = parse_props(props, nprops)
            changed = False
            for i, (t, v) in enumerate(pp):
                if t == b"S" and v.split(b"\x00\x01")[0] == old:
                    pp[i] = (t, new + v[len(old):])
                    changed = True
            if changed:
                node[2] = build_props(pp)
                count += 1
        count += rename(children, old, new)
    return count


def main(src, dst, old, new):
    data = open(src, "rb").read()
    assert data[:23] == HEADER, "binary FBX emas"
    version = struct.unpack_from("<I", data, 23)[0]
    wide = version >= 7500
    nodes, _ = read_nodes(data, 27, len(data), wide)
    n = rename(nodes, old.encode(), new.encode())
    out = bytearray(data[:27])
    write_nodes(out, nodes, wide)
    out += b"\x00" * (25 if wide else 13)
    # footer (Blender encode_bin bilan bir xil)
    out += b"\xfa\xbc\xab\x09\xd0\xc8\xd4\x66\xb1\x76\xfb\x83\x1c\xf7\x26\x7e"
    out += b"\x00" * 4
    pad = ((len(out) + 15) & ~15) - len(out)
    out += b"\x00" * (pad or 16)
    out += struct.pack("<I", version) + b"\x00" * 120 + FOOT_MAGIC
    open(dst, "wb").write(out)
    print("%s: %d ta yozuv o'zgardi (FBX %d)" % (dst, n, version))
    return n


if __name__ == "__main__":
    sys.exit(0 if main(*sys.argv[1:5]) else 1)
