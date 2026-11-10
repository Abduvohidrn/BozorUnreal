"""Ro'mol (veil) ostidagi sochni ro'mol ichiga suradi: binary FBX'da sochning ro'moldan chiqib turgan uchlari ichkariga ko'chadi.

MakeHuman sochi (masalan ponytail01) ro'mol bilan birga kiydirilganda dumi ro'mol yuzasidan teshib chiqadi va orqadan
"ko'k soch"dek ko'rinadi. Har bir soch uchi uchun eng yaqin ro'mol uchi va uning normali olinadi; uch ro'mol yuzasidan
MARGIN dan kam ichkarida (yoki tashqarida) bo'lsa, normal bo'yicha ichkariga suriladi. Faqat Geometry "Vertices" o'zgaradi:
topologiya, UV, normal va skin og'irliklari o'sha-o'sha qoladi. Ro'mol chetiga yaqin soch (yuz atrofidagi soch chizig'i)
tegilmaydi: u yerda chet normali sochni bosh ichiga kiritib yuborardi va peshona taqir ko'rinardi.
fbx_to_cm.py buni santimetrga o'tkazgandan keyin chaqiradi.
Ishlatish (faqat hisobot): python fbx_tuck_hair.py <fayl.fbx>
"""
import math
import re
import struct
import sys
import zlib

sys.path.insert(0, __file__.replace("\\", "/").rsplit("/", 1)[0])
from fbx_rename_root import HEADER, read_nodes, parse_props

HAIR = re.compile(r"(ponytail|bob|long|short|braid|afro)\d")   # MakeHuman soch nomlari (skirt_full_long emas)
VEIL = ("veil", "romol", "scarf", "hijab")
MARGIN = 1.5        # sm: soch ro'mol yuzasidan shuncha ichkarida turadi (animatsiyada ham chiqmasligi uchun)
REACH = 5.0         # sm: bundan uzoqdagi ro'mol uchlari hisobga olinmaydi
TANGENT = 2.5       # sm: ro'mol yuzasi ustida bo'lmagan soch uchlari tegilmaydi
BORDER = 3.0        # sm: ro'mol chetidan shuncha masofadagi ro'mol uchlari ostidagi soch tegilmaydi


def get_array(node):
    t, raw = parse_props(node[2], node[1])[0]
    n, enc, clen = struct.unpack_from("<III", raw, 0)
    data = raw[12:12 + clen]
    data = zlib.decompress(data) if enc else data
    return list(struct.unpack("<%d%s" % (n, t.decode()), data))


def set_doubles(node, vals):
    node[2] = b"d" + struct.pack("<III", len(vals), 0, len(vals) * 8) + struct.pack("<%dd" % len(vals), *vals)


def child(node, name):
    return next((c for c in node[3] if c[0] == name), None)


def ident(node):
    pp = parse_props(node[2], node[1])
    return struct.unpack("<q", pp[0][1])[0], pp[1][1].split(b"\x00\x01")[0].decode()


def materials_of(nodes, model_id):
    """Model'ga ulangan materiallar nomi, ulanish tartibida (bu Materials indeksining tartibi)."""
    objects = next(n for n in nodes if n[0] == b"Objects")
    names = {ident(o)[0]: ident(o)[1] for o in objects[3] if o[0] == b"Material"}
    conns = next(n for n in nodes if n[0] == b"Connections")
    out = []
    for c in conns[3]:
        pp = parse_props(c[2], c[1])
        src, dst = struct.unpack("<q", pp[1][1])[0], struct.unpack("<q", pp[2][1])[0]
        if dst == model_id and src in names:
            out.append(names[src])
    return out


def polygons(pvi):
    poly, cur = [], []
    for i in pvi:
        if i < 0:
            cur.append(~i)
            poly.append(cur)
            cur = []
        else:
            cur.append(i)
    return poly


def tuck(verts, polys, poly_mats, hair_ids, veil_ids):
    """verts (x,y,z ketma-ket) ni joyida o'zgartiradi; (surilgan, soch uchlari, ro'mol uchlari, eng katta siljish) qaytaradi."""
    hair = sorted({v for p, m in zip(polys, poly_mats) if m in hair_ids for v in p})
    veil = sorted({v for p, m in zip(polys, poly_mats) if m in veil_ids for v in p})
    P = lambda i: (verts[3 * i], verts[3 * i + 1], verts[3 * i + 2])
    # ro'mol uchlari normali (yuzalar normalining yig'indisi, yelpig'ich uchburchaklar)
    nrm = {v: [0.0, 0.0, 0.0] for v in veil}
    for p, m in zip(polys, poly_mats):
        if m not in veil_ids:
            continue
        a = P(p[0])
        for k in range(1, len(p) - 1):
            b, c = P(p[k]), P(p[k + 1])
            u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
            w = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
            f = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
            for v in (p[0], p[k], p[k + 1]):
                n = nrm[v]
                n[0] += f[0]; n[1] += f[1]; n[2] += f[2]
    for v, n in nrm.items():
        ln = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1.0
        nrm[v] = (n[0] / ln, n[1] / ln, n[2] / ln)
    # normal tashqariga qarashi kerak: ro'mol markazidan uzoqlashuvchi yo'nalish bilan solishtiriladi
    cx = sum(P(v)[0] for v in veil) / len(veil)
    cy = sum(P(v)[1] for v in veil) / len(veil)
    cz = sum(P(v)[2] for v in veil) / len(veil)
    out = sum((P(v)[0] - cx) * nrm[v][0] + (P(v)[1] - cy) * nrm[v][1] + (P(v)[2] - cz) * nrm[v][2] for v in veil)
    sgn = 1.0 if out >= 0 else -1.0
    # ro'mol cheti: faqat bitta ko'pburchakka tegishli qirralar
    edges = {}
    for p, m in zip(polys, poly_mats):
        if m in veil_ids:
            for a, b in zip(p, p[1:] + p[:1]):
                e = (min(a, b), max(a, b))
                edges[e] = edges.get(e, 0) + 1
    rim = {v for e, k in edges.items() if k == 1 for v in e}
    rgrid = {}
    for v in rim:
        x, y, z = P(v)
        rgrid.setdefault((int(x // BORDER), int(y // BORDER), int(z // BORDER)), []).append(P(v))
    near_rim = set()
    for v in veil:
        x, y, z = P(v)
        gx, gy, gz = int(x // BORDER), int(y // BORDER), int(z // BORDER)
        if any((x - q[0]) ** 2 + (y - q[1]) ** 2 + (z - q[2]) ** 2 < BORDER * BORDER
               for dx in (-1, 0, 1) for dy in (-1, 0, 1) for dz in (-1, 0, 1) for q in rgrid.get((gx + dx, gy + dy, gz + dz), ())):
            near_rim.add(v)
    # tez qidirish uchun katakchalar
    cell = REACH
    grid = {}
    for v in veil:
        x, y, z = P(v)
        grid.setdefault((int(x // cell), int(y // cell), int(z // cell)), []).append(v)
    moved, worst = 0, 0.0
    for h in hair:
        x, y, z = P(h)
        gx, gy, gz = int(x // cell), int(y // cell), int(z // cell)
        near = []
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    for v in grid.get((gx + dx, gy + dy, gz + dz), ()):
                        q = P(v)
                        d2 = (x - q[0]) ** 2 + (y - q[1]) ** 2 + (z - q[2]) ** 2
                        if d2 < REACH * REACH:
                            near.append((d2, v))
        if not near:
            continue
        # eng yaqin 4 ta ro'mol uchi bo'yicha silliq masofa va normal (bitta uchda takrorlashda tebranib qoladi)
        near = sorted(near)[:4]
        if any(v in near_rim for _, v in near):
            continue
        sw, d, n = 0.0, 0.0, [0.0, 0.0, 0.0]
        for d2, v in near:
            w = 1.0 / (d2 + 0.01)
            q, m = P(v), nrm[v]
            d += w * sgn * ((x - q[0]) * m[0] + (y - q[1]) * m[1] + (z - q[2]) * m[2])   # musbat = ro'moldan tashqarida
            n[0] += w * m[0]; n[1] += w * m[1]; n[2] += w * m[2]
            sw += w
        d /= sw
        ln = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1.0
        n = (n[0] / ln, n[1] / ln, n[2] / ln)
        tang = math.sqrt(max(0.0, near[0][0] - d * d))
        if d <= -MARGIN + 0.05 or tang > TANGENT:
            continue
        s = (d + MARGIN) * sgn
        verts[3 * h] -= s * n[0]
        verts[3 * h + 1] -= s * n[1]
        verts[3 * h + 2] -= s * n[2]
        moved += 1
        worst = max(worst, d + MARGIN)
    return moved, len(hair), len(veil), worst


def tuck_nodes(nodes):
    """FBX daraxtidagi har bir mesh uchun: soch va ro'mol materiallari bo'lsa, sochni ichkariga suradi. Hisobot qaytaradi."""
    objects = next(n for n in nodes if n[0] == b"Objects")
    conns = next(n for n in nodes if n[0] == b"Connections")
    geo_model = {}
    for c in conns[3]:
        pp = parse_props(c[2], c[1])
        geo_model[struct.unpack("<q", pp[1][1])[0]] = struct.unpack("<q", pp[2][1])[0]
    report = []
    for g in objects[3]:
        if g[0] != b"Geometry" or child(g, b"Vertices") is None:
            continue
        gid, gname = ident(g)
        mats = [m.lower() for m in materials_of(nodes, geo_model.get(gid))]
        hair_ids = {i for i, m in enumerate(mats) if HAIR.search(m)}
        veil_ids = {i for i, m in enumerate(mats) if any(k in m for k in VEIL)}
        if not hair_ids or not veil_ids:
            continue
        lem = child(g, b"LayerElementMaterial")
        mapping = parse_props(child(lem, b"MappingInformationType")[2], 1)[0][1]
        polys = polygons(get_array(child(g, b"PolygonVertexIndex")))
        pm = get_array(child(lem, b"Materials"))
        if mapping != b"ByPolygon":
            pm = pm[:1] * len(polys)
        vnode = child(g, b"Vertices")
        verts = get_array(vnode)
        # surilgan uchning eng yaqin ro'mol uchi o'zgarishi mumkin: bir necha marta takrorlanadi
        res = tuck(verts, polys, pm, hair_ids, veil_ids)
        for _ in range(5):
            again = tuck(verts, polys, pm, hair_ids, veil_ids)
            if again[0] == 0:
                break
        set_doubles(vnode, verts)
        report.append((gname, [mats[i] for i in sorted(hair_ids)], [mats[i] for i in sorted(veil_ids)], res))
    return report


if __name__ == "__main__":
    data = open(sys.argv[1], "rb").read()
    assert data[:23] == HEADER, "binary FBX emas"
    version = struct.unpack_from("<I", data, 23)[0]
    nodes, _ = read_nodes(data, 27, len(data), version >= 7500)
    for gname, hair, veil, (moved, nh, nv, worst) in tuck_nodes(nodes):
        print("%s: soch %s ro'mol %s -> %d/%d soch uchi surildi (ro'mol uchlari %d, eng ko'pi %.2f sm)" % (
            gname, hair, veil, moved, nh, nv, worst))
