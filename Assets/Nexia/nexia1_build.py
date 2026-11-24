# Nexia 1 (Daewoo Nexia N100) — blender-car-modeling skill testi.
# Ishga tushirish:  blender -b --python nexia1_build.py -- <out_dir>
# Usul: kuzov ko'ndalang kesimlardan (stansiyalar) quriladi, har birida bir xil
# vertex soni -> faqat quad setka, Mirror Y + Bevel(weight) + Subdivision.
import bpy, bmesh, math, sys, os
from mathutils import Vector

OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else os.path.dirname(__file__)

# ---------------------------------------------------------------- tozalash
bpy.ops.wm.read_factory_settings(use_empty=True)
scn = bpy.context.scene

def mat(name, rgb, metal=0.0, rough=0.4, alpha=1.0, emit=None):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if alpha < 1:
        b.inputs["Alpha"].default_value = alpha
        b.inputs["Transmission Weight"].default_value = 0.6
    if emit:
        b.inputs["Emission Color"].default_value = (*emit, 1)
        b.inputs["Emission Strength"].default_value = 0.6
    m.diffuse_color = (*rgb, 1)
    return m

M_PAINT = mat("Bo'yoq | oq", (0.78, 0.79, 0.80), metal=0.3, rough=0.25)
M_GLASS = mat("Oyna", (0.04, 0.05, 0.06), metal=0.2, rough=0.05)
M_BLACK = mat("Qora plastik", (0.025, 0.025, 0.025), rough=0.6)
M_CHROME = mat("Xrom", (0.9, 0.9, 0.9), metal=1.0, rough=0.1)
M_HEAD = mat("Fara oynasi", (0.85, 0.88, 0.9), metal=0.5, rough=0.05)
M_RED = mat("Stop qizil", (0.6, 0.02, 0.02), rough=0.2, emit=(0.4, 0, 0))
M_AMBER = mat("Burilish sariq", (0.9, 0.45, 0.02), rough=0.2)
M_TIRE = mat("Shina", (0.03, 0.03, 0.03), rough=0.8)
M_RIM = mat("Disk | po'lat", (0.55, 0.56, 0.58), metal=0.8, rough=0.3)

def link(obj):
    scn.collection.objects.link(obj)
    return obj

def new_obj(name, bm, mats):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me); bm.free()
    for m in mats:
        me.materials.append(m)
    o = bpy.data.objects.new(name, me)
    link(o)
    for p in me.polygons:
        p.use_smooth = True
    return o

def car_stack(obj, bevel=0.01, subsurf=2, mirror=True, solidify=None):
    """Skilldagi standart stek: Mirror(Y) -> Bevel(weight) -> [Solidify] -> Subsurf."""
    if mirror:
        m = obj.modifiers.new("Mirror", 'MIRROR')
        m.use_axis = (False, True, False); m.use_clip = True; m.use_mirror_merge = True
    b = obj.modifiers.new("Bevel", 'BEVEL')
    b.width = bevel; b.segments = 2; b.limit_method = 'WEIGHT'; b.profile = 1.0
    if solidify:
        s = obj.modifiers.new("Solidify", 'SOLIDIFY'); s.thickness = solidify; s.offset = -0.8
    if subsurf:
        sd = obj.modifiers.new("Subdivision", 'SUBSURF'); sd.levels = subsurf; sd.render_levels = subsurf

def lerp(a, b, t):
    return a + (b - a) * t

# ---------------------------------------------------------------- o'lchamlar (m)
L, HALF_L = 4.482, 2.241
AX_F, AX_R = -1.37, 1.15          # baza 2.52
WHEEL_R, WHEEL_Y = 0.29, 0.715
ARCH_R = 0.355
ROCKER = 0.235

# Kalit stansiyalar: x, zt(tepa, o'rta chiziq), zs(belbog' chizig'i), w(yarim eni), wtop(tepa yuza qirrasi)
KEYS = [
    (-2.241, 0.60, 0.56, 0.74, 0.66),
    (-2.200, 0.70, 0.66, 0.79, 0.72),
    (-2.100, 0.745, 0.715, 0.815, 0.75),
    (-1.400, 0.805, 0.775, 0.828, 0.77),
    (-0.800, 0.875, 0.845, 0.830, 0.77),
    (-0.730, 0.900, 0.860, 0.830, 0.745),
    (-0.050, 1.375, 0.885, 0.830, 0.600),
    (0.950, 1.385, 0.915, 0.830, 0.600),
    (1.520, 1.000, 0.940, 0.828, 0.715),
    (2.120, 0.975, 0.945, 0.815, 0.735),
    (2.200, 0.940, 0.910, 0.795, 0.715),
    (2.241, 0.880, 0.850, 0.760, 0.680),
]

def key_at(x):
    for a, b in zip(KEYS, KEYS[1:]):
        if a[0] <= x <= b[0]:
            t = (x - a[0]) / (b[0] - a[0])
            return [lerp(a[i], b[i], t) for i in range(1, 5)]
    return list(KEYS[-1][1:])

def z_bottom(x):
    for ax in (AX_F, AX_R):
        dx = x - ax
        if abs(dx) < ARCH_R:
            return max(ROCKER, WHEEL_R + math.sqrt(ARCH_R**2 - dx**2))
    # bamperlar oldi/orqasi biroz yuqori
    if x < -2.0:
        return lerp(ROCKER, 0.30, (-2.0 - x) / 0.241)
    if x > 1.95:
        return lerp(ROCKER, 0.33, (x - 1.95) / 0.291)
    return ROCKER

def profile(x):
    """Yarim kesim: tashqi pastdan -> o'rta chiziq tepasigacha, DOIM 11 nuqta."""
    zt, zs, w, wtop = key_at(x)
    zb = z_bottom(x)
    cabin = max(0.0, min(1.0, (zt - zs - 0.08) / 0.25))
    crown = 0.018 * (1 - cabin) + 0.012 * cabin
    zA = min(zs + 0.025, zt)
    zB = max(zA, zt - 0.045 * cabin - 0.006)
    A = (w - 0.045, zA)
    B = (wtop, zB)
    pts = [
        (w * 0.965, zb),
        (w, zb + (zs - zb) * 0.30),
        (w, zb + (zs - zb) * 0.70),
        (w * 0.997, zs),
        A,
        (lerp(A[0], B[0], 0.5), lerp(A[1], B[1], 0.5)),
        B,
        (wtop - 0.05, zt - 0.004),
        (wtop * 0.66, zt + crown * 0.6),
        (wtop * 0.33, zt + crown * 0.9),
        (0.0, zt + crown),
    ]
    return pts

# Stansiyalar: g'ildirak arkalari atrofida zichroq
CAP = 0.04                      # uchlardagi quad qopqoq chuqurligi
xs = []
x = -HALF_L + CAP
while x < HALF_L - CAP - 1e-6:
    xs.append(x)
    near = min(abs(x - AX_F), abs(x - AX_R)) < ARCH_R + 0.05
    x += 0.04 if near else 0.075
xs.append(HALF_L - CAP)

# ---------------------------------------------------------------- kuzov
bm = bmesh.new()
grid = []
for x in xs:
    grid.append([bm.verts.new((x, y, z)) for (y, z) in profile(x)])
NP = len(grid[0])
faces = []
for i in range(len(grid) - 1):
    for j in range(NP - 1):
        faces.append(bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1])))

# Old/orqa qopqoq: ngon o'rniga 2 ta qo'shimcha halqa (y*0.55, keyin y=0) -> faqat quad.
# Oxirgi halqa mirror tekisligida yotadi, Mirror(clip) uni ikkinchi yarmi bilan tikadi.
def cap(row, sign):
    loops = [row]
    for ys, dx in ((0.82, 0.55), (0.45, 0.88), (0.0, 1.0)):
        loops.append([bm.verts.new((v.co.x + sign * CAP * dx, v.co.y * ys, v.co.z))
                      for v in row])
    out = []
    for a, b in zip(loops, loops[1:]):
        for j in range(NP - 1):
            quad = (a[j], a[j + 1], b[j + 1], b[j]) if sign > 0 else (a[j], b[j], b[j + 1], a[j + 1])
            out.append(bm.faces.new(quad))
    return out
cap(grid[0], -1)
cap(grid[-1], 1)

bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])

# Material: oynalar (yon, old, orqa), B ustuni bo'yoqda qoladi
def assign(f):
    c = f.calc_center_median()
    js = [grid_index[v] for v in f.verts if v in grid_index]
    if not js:
        return 0
    jmin, jmax = min(j for _, j in js), max(j for _, j in js)
    x = c.x
    # yon oynalar: 4..6 qatorlar
    if 4 <= jmin and jmax <= 6 and -0.66 < x < 1.45 and not (0.36 < x < 0.50):
        if abs(x - (-0.66)) < 0.02:
            return 0
        return 1
    # old oyna
    if jmin >= 6 and -0.70 < x < -0.10:
        return 1
    # orqa oyna
    if jmin >= 6 and 1.02 < x < 1.48:
        return 1
    return 0

grid_index = {v: (i, j) for i, row in enumerate(grid) for j, v in enumerate(row)}
for f in bm.faces:
    f.material_index = assign(f)

# Bevel weight: belbog' chizig'i va kapot/yukxona qirralari
bw = bm.edges.layers.float.new("bevel_weight_edge")
for i in range(len(grid) - 1):
    for j in (3, 6):
        e = bm.edges.get((grid[i][j], grid[i + 1][j]))
        if e:
            e[bw] = 0.6 if j == 3 else 0.4

# ---- Panel tirqishlari (skill 4-bo'lim): V (rip) -> G G bilan ochish -> E + Alt+S (soxta qalinlik)
def col_index(xv):
    return min(range(len(xs)), key=lambda i: abs(xs[i] - xv))

seam_edges = set()
def vseam(xv, j0, j1):                       # stansiya bo'ylab vertikal chok
    i = col_index(xv)
    for j in range(j0, j1):
        seam_edges.add(bm.edges.get((grid[i][j], grid[i][j + 1])))
def hseam(x0, x1, j):                        # qator bo'ylab gorizontal chok
    for i in range(col_index(x0), col_index(x1)):
        seam_edges.add(bm.edges.get((grid[i][j], grid[i + 1][j])))

vseam(-0.70, 0, 3); vseam(0.43, 0, 3); vseam(1.30, 0, 3)   # eshiklar
hseam(-0.70, 1.30, 3)                                       # eshik yuqori qirrasi
hseam(-2.15, -0.80, 5); vseam(-0.80, 5, 10)                 # kapot
hseam(1.56, 2.10, 5); vseam(1.56, 5, 10)                    # yukxona qopqog'i
seam_edges.discard(None)

res = bmesh.ops.split_edges(bm, edges=list(seam_edges))
GAP, DEPTH = 0.0025, 0.014
seam_verts = {v for e in res["edges"] for v in e.verts}
for v in seam_verts:                          # har bir nusxani o'z yuzalari tomon tortish
    if not v.link_faces:
        continue
    c = sum((f.calc_center_median() for f in v.link_faces), Vector()) / len(v.link_faces)
    d = c - v.co
    if v.co.y < 1e-4:
        d.y = 0
    if d.length > 1e-6:
        v.co += d.normalized() * GAP
bm.normal_update()
boundary = [e for e in res["edges"] if e.is_boundary]
ext = bmesh.ops.extrude_edge_only(bm, edges=boundary)
for v in (g for g in ext["geom"] if isinstance(g, bmesh.types.BMVert)):
    n = v.normal.copy()
    if v.co.y < 1e-4:
        n.y = 0
    v.co -= n.normalized() * DEPTH
for e in boundary:
    e[bw] = 1.0
bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
print("Choklar: ajratilgan qirralar =", len(seam_edges), ", soxta qalinlik qirralari =", len(boundary))

body = new_obj("Kuzov", bm, [M_PAINT, M_GLASS])
car_stack(body, bevel=0.012, subsurf=2)

# ---------------------------------------------------------------- g'ildiraklar (profil + Spin)
def spun(name, prof, steps, matl, axis_y=True):
    bm = bmesh.new()
    vs = [bm.verts.new((r, y, 0)) for (r, y) in prof]   # x=radius, y=o'q bo'yicha
    es = [bm.edges.new((a, b)) for a, b in zip(vs, vs[1:])]
    bmesh.ops.spin(bm, geom=vs + es, cent=(0, 0, 0), axis=(0, 1, 0),
                   angle=math.tau, steps=steps, use_merge=True)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = new_obj(name, bm, [matl])
    return o

TIRE = [(0.185, -0.088), (0.235, -0.094), (0.272, -0.088), (0.288, -0.060), (0.291, 0.0),
        (0.288, 0.060), (0.272, 0.088), (0.235, 0.094), (0.185, 0.088)]
RIM = [(0.186, 0.085), (0.178, 0.070), (0.170, 0.040), (0.150, 0.045), (0.120, 0.058),
       (0.085, 0.066), (0.040, 0.074), (0.0, 0.078)]
HUB = [(0.135, 0.070), (0.130, 0.082), (0.110, 0.090), (0.070, 0.096), (0.0, 0.098)]

wheels = []
for xw in (AX_F, AX_R):
    for side in (-1, 1):
        t = spun(f"Shina {'old' if xw < 0 else 'orqa'} {'chap' if side < 0 else 'ong'}", TIRE, 36, M_TIRE)
        r = spun(f"Disk {'old' if xw < 0 else 'orqa'} {'chap' if side < 0 else 'ong'}", RIM, 36, M_RIM)
        h = spun(f"Kolpak {'old' if xw < 0 else 'orqa'} {'chap' if side < 0 else 'ong'}", HUB, 18, M_CHROME)
        for o in (t, r, h):
            o.location = (xw, side * WHEEL_Y, WHEEL_R)
            if side < 0:
                o.scale.y = -1
            sd = o.modifiers.new("Subdivision", 'SUBSURF'); sd.levels = 1; sd.render_levels = 2
        wheels += [t, r, h]

# ---------------------------------------------------------------- detallar
def box(name, center, size, matl, bevel=0.01, sub=2, rot=(0, 0, 0)):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    o = new_obj(name, bm, [matl])
    o.location = center; o.rotation_euler = rot
    b = o.modifiers.new("Bevel", 'BEVEL'); b.width = bevel; b.segments = 3
    if sub:
        s = o.modifiers.new("Subdivision", 'SUBSURF'); s.levels = sub; s.render_levels = sub
    return o

def surface_x_front(y, z):
    return -HALF_L

# Old bamper (kuzov rangida, pastki qismi qora) — Nexia N100
box("Old bamper", (-2.205, 0, 0.40), (0.14, 1.64, 0.20), M_PAINT, bevel=0.03)
box("Old bamper | qora molding", (-2.275, 0, 0.42), (0.02, 1.50, 0.05), M_BLACK, bevel=0.008)
box("Old bamper | pastki tirqish", (-2.272, 0, 0.33), (0.02, 0.80, 0.05), M_BLACK, bevel=0.01)
# Orqa bamper
box("Orqa bamper", (2.205, 0, 0.44), (0.14, 1.62, 0.20), M_PAINT, bevel=0.03)
box("Orqa bamper | qora molding", (2.275, 0, 0.47), (0.02, 1.48, 0.05), M_BLACK, bevel=0.008)

# Radiator panjarasi: gorizontal planka (Array modifier, skilldagi usul)
g = box("Panjara | planka", (-2.248, 0, 0.665), (0.02, 0.62, 0.012), M_BLACK, bevel=0.003, sub=0)
arr = g.modifiers.new("Array", 'ARRAY'); arr.count = 4
arr.use_relative_offset = False; arr.use_constant_offset = True; arr.constant_offset_displace = (0, 0, -0.026)
g.location.z = 0.70
box("Panjara | ramka", (-2.236, 0, 0.66), (0.03, 0.66, 0.12), M_CHROME, bevel=0.012, sub=1)
box("Daewoo belgisi", (-2.262, 0, 0.665), (0.01, 0.08, 0.06), M_CHROME, bevel=0.02)

# Faralar (to'rtburchak) + burilish chiroqlari burchakda
for side in (-1, 1):
    box(f"Fara {'chap' if side < 0 else 'ong'}", (-2.215, side * 0.53, 0.655), (0.06, 0.40, 0.14), M_HEAD, bevel=0.015)
    box(f"Burilish {'chap' if side < 0 else 'ong'}", (-2.150, side * 0.775, 0.65), (0.16, 0.07, 0.12), M_AMBER, bevel=0.015)
    # Stop chiroqlar: orqa burchaklarni o'rab oluvchi gorizontal blok
    box(f"Stop chiroq {'chap' if side < 0 else 'ong'}", (2.215, side * 0.55, 0.80), (0.06, 0.48, 0.15), M_RED, bevel=0.015)
    box(f"Stop chiroq yon {'chap' if side < 0 else 'ong'}", (2.13, side * 0.79, 0.80), (0.16, 0.06, 0.15), M_RED, bevel=0.015)
    # Ko'zgular
    box(f"Ko'zgu {'chap' if side < 0 else 'ong'}", (-0.60, side * 0.895, 0.975), (0.09, 0.14, 0.10), M_BLACK, bevel=0.03)
    box(f"Ko'zgu oyog'i {'chap' if side < 0 else 'ong'}", (-0.63, side * 0.825, 0.925), (0.10, 0.04, 0.05), M_BLACK, bevel=0.01, sub=1)
    # Eshik tutqichlari
    for xh in (-0.08, 0.83):
        box(f"Tutqich {xh} {side}", (xh, side * 0.838, 0.83), (0.11, 0.015, 0.025), M_BLACK, bevel=0.006, sub=1)
    # Yon molding
    box(f"Yon molding {'chap' if side < 0 else 'ong'}", (-0.11, side * 0.836, 0.55), (1.72, 0.012, 0.04), M_BLACK, bevel=0.006, sub=1)
box("Raqam joyi | orqa", (2.272, 0, 0.62), (0.01, 0.52, 0.12), M_BLACK, bevel=0.01, sub=1)
box("Raqam joyi | old", (-2.285, 0, 0.36), (0.01, 0.52, 0.11), M_BLACK, bevel=0.01, sub=1)
# Stop chiroqlar orasidagi qizil-qora panel (N100 yukxona qopqog'i)
box("Orqa panel | qaytargich", (2.232, 0, 0.80), (0.03, 0.62, 0.06), M_RED, bevel=0.01, sub=1)
box("Orqa panel | qora tasma", (2.236, 0, 0.755), (0.02, 0.62, 0.025), M_BLACK, bevel=0.005, sub=1)

# Arka ichki qoplamasi: arka orqali kuzov ichi ko'rinmasin (qora, ochiq silindr bo'lagi)
def arch_liner(name, xa, side):
    bm = bmesh.new()
    prof = [(ARCH_R - 0.004, 0.50), (ARCH_R - 0.004, 0.835)]
    vs = [bm.verts.new((r, y, 0)) for r, y in prof]
    e = bm.edges.new(vs)
    bmesh.ops.spin(bm, geom=vs + [e], cent=(0, 0, 0), axis=(0, 1, 0),
                   angle=math.radians(-200), steps=24)   # Y o'qi atrofida manfiy = +X dan tepaga
    o = new_obj(name, bm, [M_BLACK])
    o.location = (xa, 0, WHEEL_R)
    o.rotation_euler = (0, math.radians(10), 0)
    if side < 0:
        o.scale.y = -1
    return o
for xa in (AX_F, AX_R):
    for side in (-1, 1):
        arch_liner(f"Arka qoplamasi {xa} {side}", xa, side)

for side in (-1, 1):
    s = 'chap' if side < 0 else 'ong'
    # B ustuni qora qoplamasi (oyna oralig'ida)
    box(f"B ustun {s}", (0.43, side * 0.697, 1.12), (0.075, 0.01, 0.38), M_BLACK, bevel=0.005, sub=1,
        rot=(side * math.radians(23), 0, 0))   # R_x(+θ): tepa -Y ga og'adi (tumblehome)
    # Old artgichlar
    box(f"Artgich {s}", (-0.70, side * 0.20 - 0.10, 0.905), (0.012, 0.50, 0.012), M_BLACK, bevel=0.003, sub=0,
        rot=(0, 0, math.radians(8 * side)))
    # Loy to'sqichlar ostidagi qora poroglar
    box(f"Porog {s}", (-0.11, side * 0.815, 0.25), (1.75, 0.04, 0.04), M_BLACK, bevel=0.01, sub=1)
# Glushitel trubasi
bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.025, depth=0.20,
                                    location=(2.17, 0.45, 0.22), rotation=(0, math.radians(90), 0))
ex = bpy.context.object; ex.name = "Glushitel"; ex.data.materials.append(M_CHROME)
# Antenna (tom orqasida)
bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.003, depth=0.55,
                                    location=(0.98, 0.0, 1.60), rotation=(0, math.radians(-25), 0))
an = bpy.context.object; an.name = "Antenna"; an.data.materials.append(M_BLACK)

# ---------------------------------------------------------------- tekshiruv
me = body.data
nonquad = sum(1 for p in me.polygons if len(p.vertices) != 4)
print(f"KUZOV: stansiyalar={len(xs)}, kesim nuqtalari={NP}, poligon={len(me.polygons)}, quad emas={nonquad}")
print("O'lchamlar (Mirror bilan):", [round(v, 3) for v in body.evaluated_get(bpy.context.evaluated_depsgraph_get()).dimensions])

# ---------------------------------------------------------------- kamera, yorug'lik, render
world = bpy.data.worlds.new("Muhit"); scn.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.37, 0.40, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.8

def light(name, loc, energy, size=4):
    ld = bpy.data.lights.new(name, 'AREA'); ld.energy = energy; ld.size = size
    o = bpy.data.objects.new(name, ld); link(o); o.location = loc
    o.rotation_euler = (Vector((0, 0, 0.6)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
light("Asosiy", (-4, -4, 5), 1500, 5)
light("Tepa", (0, 0, 6), 800, 6)
light("Orqa", (5, 3, 4), 700, 4)

bpy.ops.mesh.primitive_plane_add(size=40)
floor = bpy.context.object; floor.name = "Pol"
floor.data.materials.append(mat("Pol", (0.3, 0.31, 0.33), rough=0.7))

cam_d = bpy.data.cameras.new("Kamera"); cam = bpy.data.objects.new("Kamera", cam_d); link(cam)
scn.camera = cam

def shot(fname, loc, ortho=None, lens=50, engine='BLENDER_EEVEE'):
    cam.location = loc
    cam.rotation_euler = (Vector((0, 0, 0.62)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    if ortho:
        cam_d.type = 'ORTHO'; cam_d.ortho_scale = ortho
    else:
        cam_d.type = 'PERSP'; cam_d.lens = lens
    scn.render.engine = engine
    scn.render.filepath = os.path.join(OUT, fname)
    bpy.ops.render.render(write_still=True)

scn.render.resolution_x, scn.render.resolution_y = 1400, 800
try:
    scn.eevee.taa_render_samples = 32
except Exception:
    pass

shot("Nexia1_Render.png", (-6.2, -5.6, 2.4), lens=50)
shot("Nexia1_Yon.png", (0, -9, 0.7), ortho=5.2)
shot("Nexia1_Old.png", (-9, 0, 0.75), ortho=2.4)
shot("Nexia1_Orqa.png", (9, 0, 0.75), ortho=2.4)
shot("Nexia1_Tepa.png", (0, 0, 9), ortho=5.2)

# Matcap tekshiruvi (Workbench)
scn.render.engine = 'BLENDER_WORKBENCH'
sh = scn.display.shading
sh.light = 'MATCAP'
for name in ("metal_carpaint.exr", "check_rim_light.exr", "basic_1.exr"):
    try:
        sh.studio_light = name; break
    except Exception:
        continue
sh.color_type = 'SINGLE'; sh.single_color = (0.8, 0.8, 0.8)
shot("Nexia1_Matcap.png", (-6.2, -5.6, 2.4), lens=50, engine='BLENDER_WORKBENCH')
shot("Nexia1_Matcap_Orqa.png", (5.6, -4.4, 2.2), lens=50, engine='BLENDER_WORKBENCH')

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "Nexia1.blend"))
print("SAQLANDI", os.path.join(OUT, "Nexia1.blend"))
