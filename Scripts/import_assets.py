"""Unity eksportidan (Unreal_Export) assetlarni import qiladi: teksturalar, materiallar, mesh'lar, odamlar, animatsiyalar.

Bosqichlar BOZOR_STAGES muhit o'zgaruvchisida (vergul bilan): tex,mat,mesh,char,anim. Bo'sh bo'lsa - hammasi.
Ishga tushirish: UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/import_assets.py
"""
import collections
import os
import sys
import time

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import unreal

log = bc.log
scene = bc.load_scene()
ROOT = bc.ROOT
mel = unreal.MaterialEditingLibrary
STAGES = [s for s in os.environ.get("BOZOR_STAGES", "tex,mat,mesh,char,anim").split(",") if s]
T0 = time.time()


def asset(path):
    return unreal.load_asset(path) if bc.eal.does_asset_exist(path) else None


def save_dir(path):
    bc.eal.save_directory(path, only_if_is_dirty=True, recursive=True)


# ============================================================ teksturalar
# Tekstura qanday ishlatiladi (materialdagi sampler turi bilan mos bo'lishi shart): color / normal / linear
LINEAR_PROPS = {"_MetallicGlossMap", "_OcclusionMap", "_SpecGlossMap", "_ParallaxMap", "_DetailMask"}


def texture_usage():
    use = {}
    for md in scene["materials"].values():
        for name, v in md["props"].items():
            if isinstance(v, dict) and "tex" in v:
                kind = "normal" if name in ("_BumpMap", "_DetailNormalMap") else "linear" if name in LINEAR_PROPS else "color"
                use.setdefault(v["tex"], kind)
    return use


def white_linear_png():
    """Masks/linear sampler uchun standart oq tekstura (sRGB o'chiq)."""
    import struct
    import zlib
    path = os.path.join(bc.EXP, "Textures", "T_BozorWhiteLinear.png")
    raw = b"".join(b"\x00" + b"\xff\xff\xff\xff" * 4 for _ in range(4))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)
    return path


def import_textures():
    scene["textures"]["T_BozorWhiteLinear"] = {"file": white_linear_png(), "srgb": False, "type": "Default"}
    use = texture_usage()
    use["T_BozorWhiteLinear"] = "linear"
    tasks = []
    for tid, t in scene["textures"].items():
        if asset("%s/Textures/%s" % (ROOT, tid)):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", t["file"])
        task.set_editor_property("destination_path", ROOT + "/Textures")
        task.set_editor_property("destination_name", tid)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    log("tex: import", len(tasks))
    bc.asset_tools.import_asset_tasks(tasks)
    n = 0
    for tid, t in scene["textures"].items():
        tex = asset("%s/Textures/%s" % (ROOT, tid))
        if not tex:
            log("tex: TOPILMADI", tid, t["file"])
            continue
        kind = use.get(tid, "normal" if t.get("type") == "NormalMap" else "color")
        if kind == "normal":
            # Unity normal map'lari OpenGL (Y+), Unreal DirectX (Y-) kutadi
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("flip_green_channel", True)
        else:
            # avtomatik grayscale aniqlash sampler turini buzmasin: siqish turi har doim aniq beriladi
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            tex.set_editor_property("srgb", kind == "color")
            if kind == "color" and not t.get("srgb", True):
                log("tex: sRGB o'chiq edi, rang sifatida ishlatiladi", tid)
        n += 1
    save_dir(ROOT + "/Textures")
    log("tex: tayyor", n, "%.0fs" % (time.time() - T0))


# ============================================================ materiallar
def build_master():
    path = ROOT + "/Materials/M_BozorLit"
    m = asset(path)
    if m:
        # MI'lar unga bog'langan: o'chirmasdan tugunlarini qayta quramiz
        mel.delete_all_material_expressions(m)
    else:
        m = bc.asset_tools.create_asset("M_BozorLit", ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    tlm = [n for n in dir(unreal.TranslucencyLightingMode) if "FORWARD" in n]
    log("mat: TLM", tlm)
    if tlm:
        m.set_editor_property("translucency_lighting_mode", getattr(unreal.TranslucencyLightingMode, tlm[0]))
    E = unreal.MaterialProperty
    x = [-1600]

    def node(cls, px, py, **props):
        n = mel.create_material_expression(m, cls, px, py)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(a, aout, b, bin_):
        mel.connect_material_expressions(a, aout, b, bin_)

    def vec(name, val, py):
        return node(unreal.MaterialExpressionVectorParameter, -1400, py, parameter_name=name, default_value=unreal.LinearColor(*val))

    def scal(name, val, py):
        return node(unreal.MaterialExpressionScalarParameter, -1400, py, parameter_name=name, default_value=val)

    def tex(name, default, py, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        t = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, py, parameter_name=name, texture=unreal.load_asset(default), sampler_type=sampler)
        link(uv, "", t, "UVs")
        return t

    def op(cls, py, a, aout, b, bout, px=-500):
        n = node(cls, px, py)
        link(a, aout, n, "A")
        link(b, bout, n, "B")
        return n

    def lerp(py, a, aout, b, bout, alpha, alphaout, px=-300):
        n = node(unreal.MaterialExpressionLinearInterpolate, px, py)
        link(a, aout, n, "A")
        link(b, bout, n, "B")
        link(alpha, alphaout, n, "Alpha")
        return n

    def const(v, py, px=-700):
        return node(unreal.MaterialExpressionConstant, px, py, r=v)

    # UV = TexCoord * UVTransform.xy + UVTransform.zw
    tc = node(unreal.MaterialExpressionTextureCoordinate, -1900, 0)
    uvt = vec("UVTransform", (1, 1, 0, 0), 100)
    mxy = node(unreal.MaterialExpressionComponentMask, -1650, 60, r=True, g=True, b=False, a=False)
    link(uvt, "RGBA", mxy, "")
    mzw = node(unreal.MaterialExpressionComponentMask, -1650, 140, r=False, g=False, b=True, a=True)
    link(uvt, "RGBA", mzw, "")
    mul = op(unreal.MaterialExpressionMultiply, 0, tc, "", mxy, "", px=-1450)
    uv = op(unreal.MaterialExpressionAdd, 0, mul, "", mzw, "", px=-1250)

    white = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"
    white_lin = ROOT + "/Textures/T_BozorWhiteLinear"
    base_t = tex("BaseMap", white, 0)
    base_c = vec("BaseColor", (1, 1, 1, 1), 300)
    # vertex color: glTF importi chiziqli rangni sRGB baytga yozadi, Unity esa xom qiymatni ishlatadi -> ^2.2
    vc = node(unreal.MaterialExpressionVertexColor, -1400, 500)
    vcp = node(unreal.MaterialExpressionPower, -1150, 500)
    link(vc, "", vcp, "Base")
    vcp_e = const(2.2, 560, px=-1300)
    link(vcp_e, "", vcp, "Exp")
    vcs = scal("VertexColorStrength", 0.0, 620)
    vcl = lerp(500, const(1.0, 460, px=-1000), "", vcp, "", vcs, "", px=-850)
    c1 = op(unreal.MaterialExpressionMultiply, 200, base_t, "RGB", base_c, "RGB", px=-600)
    c2 = op(unreal.MaterialExpressionMultiply, 260, c1, "", vcl, "", px=-400)
    mel.connect_material_property(c2, "", E.MP_BASE_COLOR)
    alpha = op(unreal.MaterialExpressionMultiply, 360, base_t, "A", base_c, "A", px=-600)
    mel.connect_material_property(alpha, "", E.MP_OPACITY)
    mel.connect_material_property(alpha, "", E.MP_OPACITY_MASK)

    # normal: lerp((0,0,1), N, NormalScale)
    nrm_t = tex("NormalMap", "/Engine/EngineMaterials/FlatNormal.FlatNormal", 800, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    flat = node(unreal.MaterialExpressionConstant3Vector, -700, 760, constant=unreal.LinearColor(0, 0, 1, 1))
    nsc = scal("NormalScale", 1.0, 900)
    nl = lerp(800, flat, "", nrm_t, "RGB", nsc, "")
    mel.connect_material_property(nl, "", E.MP_NORMAL)

    # metallic / smoothness (URP: metallic = map.r, smoothness = map.a * _Smoothness; xaritasiz - slider qiymatlari)
    ms_t = tex("MetallicGlossMap", white_lin, 1100, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    use_ms = scal("UseMSMap", 0.0, 1200)
    met = scal("Metallic", 0.0, 1260)
    mlerp = lerp(1100, met, "", ms_t, "R", use_ms, "")
    mel.connect_material_property(mlerp, "", E.MP_METALLIC)
    one = const(1.0, 1340)
    sm_map = lerp(1300, one, "", ms_t, "A", use_ms, "", px=-500)
    alb = scal("SmoothFromAlbedo", 0.0, 1420)
    sm_src = lerp(1380, sm_map, "", base_t, "A", alb, "", px=-350)
    smooth = scal("Smoothness", 0.5, 1480)
    sm = op(unreal.MaterialExpressionMultiply, 1450, sm_src, "", smooth, "", px=-200)
    rough = node(unreal.MaterialExpressionOneMinus, -50, 1450)
    link(sm, "", rough, "")
    mel.connect_material_property(rough, "", E.MP_ROUGHNESS)

    # AO
    occ_t = tex("OcclusionMap", white_lin, 1650, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    occ_s = scal("OcclusionStrength", 1.0, 1750)
    ao = lerp(1650, const(1.0, 1620), "", occ_t, "G", occ_s, "")
    mel.connect_material_property(ao, "", E.MP_AMBIENT_OCCLUSION)

    # emission
    em_t = tex("EmissionMap", white, 1900)
    em_c = vec("EmissionColor", (0, 0, 0, 1), 2000)
    em = op(unreal.MaterialExpressionMultiply, 1900, em_t, "RGB", em_c, "RGB")
    mel.connect_material_property(em, "", E.MP_EMISSIVE_COLOR)

    # -game rejimida foydalanish bayroqlarini o'zi qo'sha olmaydi: oldindan yoqiladi
    for usage in ("used_with_skeletal_mesh", "used_with_instanced_static_meshes", "used_with_nanite", "used_with_morph_targets"):
        m.set_editor_property(usage, True)
    mel.recompile_material(m)
    bc.eal.save_loaded_asset(m)
    return m


def build_materials():
    master = build_master()
    log("mat: master tayyor")
    ok = 0
    for mid, md in scene["materials"].items():
        if md["shader"].startswith("Skybox"):
            continue
        path = "%s/Materials/%s" % (ROOT, mid)
        mi = asset(path)
        if not mi:
            mi = bc.asset_tools.create_asset(mid, ROOT + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        p = md["props"]
        kw = set(md.get("keywords", []))

        def color(name, linear=False):
            c = p.get(name)
            if not c:
                return None
            if linear:
                return unreal.LinearColor(*c)
            return unreal.LinearColor(bc.srgb_to_linear(c[0]), bc.srgb_to_linear(c[1]), bc.srgb_to_linear(c[2]), c[3])

        def texture(name):
            t = p.get(name)
            return asset("%s/Textures/%s" % (ROOT, t["tex"])) if isinstance(t, dict) else None

        bcol = color("_BaseColor") or color("_Color") or unreal.LinearColor(1, 1, 1, 1)
        mel.set_material_instance_vector_parameter_value(mi, "BaseColor", bcol)
        base = texture("_BaseMap") or texture("_MainTex")
        if base:
            mel.set_material_instance_texture_parameter_value(mi, "BaseMap", base)
        st = p.get("_BaseMap") if isinstance(p.get("_BaseMap"), dict) else p.get("_MainTex")
        if isinstance(st, dict):
            sx, sy = st["scale"]
            ox, oy = st["offset"]
            # Unity V yuqoriga, Unreal pastga: v' = 1 - v
            mel.set_material_instance_vector_parameter_value(mi, "UVTransform", unreal.LinearColor(sx, sy, ox, 1.0 - sy - oy))
        nrm = texture("_BumpMap")
        vertex_shader = md["shader"] == "BozorKit/LitVertexColor"
        if nrm and (not vertex_shader or p.get("_UseNormalMap", 0) > 0.5):
            mel.set_material_instance_texture_parameter_value(mi, "NormalMap", nrm)
            mel.set_material_instance_scalar_parameter_value(mi, "NormalScale", float(p.get("_BumpScale", 1.0)))
        ms = texture("_MetallicGlossMap")
        if ms and (not vertex_shader or p.get("_UseMSMap", 0) > 0.5):
            mel.set_material_instance_texture_parameter_value(mi, "MetallicGlossMap", ms)
            mel.set_material_instance_scalar_parameter_value(mi, "UseMSMap", 1.0)
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", float(p.get("_Metallic", 0.0)))
        mel.set_material_instance_scalar_parameter_value(mi, "Smoothness", float(p.get("_Smoothness", p.get("_Glossiness", 0.5))))
        if p.get("_SmoothnessTextureChannel", 0) > 0.5:
            mel.set_material_instance_scalar_parameter_value(mi, "SmoothFromAlbedo", 1.0)
        occ = texture("_OcclusionMap")
        if occ:
            mel.set_material_instance_texture_parameter_value(mi, "OcclusionMap", occ)
            mel.set_material_instance_scalar_parameter_value(mi, "OcclusionStrength", float(p.get("_OcclusionStrength", 1.0)))
        if "_EMISSION" in kw:
            ec = color("_EmissionColor")
            if ec:
                mel.set_material_instance_vector_parameter_value(mi, "EmissionColor", ec)
            em = texture("_EmissionMap")
            if em:
                mel.set_material_instance_texture_parameter_value(mi, "EmissionMap", em)
        if vertex_shader:
            mel.set_material_instance_scalar_parameter_value(mi, "VertexColorStrength", float(p.get("_VertexColorStrength", 1.0)))

        # shaffoflik, alpha clip, ikki tomonlama
        bpo = mi.get_editor_property("base_property_overrides")
        transparent = p.get("_Surface", 0) > 0.5
        clip = p.get("_AlphaClip", 0) > 0.5 or "_ALPHATEST_ON" in kw
        if transparent or clip:
            bpo.set_editor_property("override_blend_mode", True)
            bpo.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT if transparent and not clip else unreal.BlendMode.BLEND_MASKED)
            if clip:
                bpo.set_editor_property("override_opacity_mask_clip_value", True)
                bpo.set_editor_property("opacity_mask_clip_value", float(p.get("_Cutoff", 0.5)))
        if p.get("_Cull", 2) < 0.5:
            bpo.set_editor_property("override_two_sided", True)
            bpo.set_editor_property("two_sided", True)
        mi.set_editor_property("base_property_overrides", bpo)
        mel.update_material_instance(mi)
        ok += 1
    save_dir(ROOT + "/Materials")
    log("mat: tayyor", ok, "%.0fs" % (time.time() - T0))


def is_transparent(mid):
    md = scene["materials"].get(mid) if mid else None
    return bool(md) and md["props"].get("_Surface", 0) > 0.5 and not md["props"].get("_AlphaClip", 0) > 0.5


# ============================================================ mesh'lar
def import_meshes():
    pipe = bc.make_pipeline("P_Static", static=True)
    # har mesh uchun eng ko'p uchraydigan material to'plami - mesh'ning o'z materiallari bo'ladi
    combos = collections.defaultdict(collections.Counter)
    for o in scene["objects"]:
        combos[o["mesh"]][tuple(o["mats"])] += 1
    for pr in scene["props"].values():
        for part in (pr or {}).get("parts", []):
            combos[part["mesh"]][tuple(part["mats"])] += 1
    done = 0
    for mid, md in scene["meshes"].items():
        path = "%s/Meshes/%s" % (ROOT, mid)
        sm = asset(path)
        if not sm:
            res = bc.interchange_import(md["file"], ROOT + "/Meshes", pipe)
            sm = next((r for r in res if isinstance(r, unreal.StaticMesh)), None)
            if not sm:
                log("mesh: XATO", mid, res)
                continue
        mats = combos[mid].most_common(1)[0][0] if combos[mid] else ()
        for i, m in enumerate(mats):
            mi = asset("%s/Materials/%s" % (ROOT, m)) if m else None
            if mi and i < len(sm.get_editor_property("static_materials")):
                sm.set_material(i, mi)
        # Nanite shaffof materialni ko'rsatmaydi
        if any(is_transparent(m) for c in combos[mid] for m in c):
            ns = sm.get_editor_property("nanite_settings")
            ns.set_editor_property("enabled", False)
            sm.set_editor_property("nanite_settings", ns)
        done += 1
        if done % 50 == 0:
            log("mesh:", done, "%.0fs" % (time.time() - T0))
            save_dir(ROOT + "/Meshes")
    save_dir(ROOT + "/Meshes")
    log("mesh: tayyor", done, "%.0fs" % (time.time() - T0))


# ============================================================ odamlar
def import_characters():
    chars = scene["npc"]["characters"]
    order = ["Yigit_01"] + sorted(c for c in chars if c != "Yigit_01")
    # BOZOR_REIMPORT_CHARS: FBX'i o'zgargan odamlar (vergul bilan) joyida qayta import qilinadi, havolalar saqlanadi
    again = set(os.environ.get("BOZOR_REIMPORT_CHARS", "").split(",")) - {""}
    skeleton = None
    for ch in order:
        path = "%s/Characters/%s" % (ROOT, ch)
        sk = asset(path)
        if not sk or ch in again:
            pipe = bc.make_pipeline("P_Char", static=False, skeleton=skeleton)
            res = bc.interchange_import(bc.EXP + "\\Characters\\%s.fbx" % ch, ROOT + "/Characters", pipe)
            sk = next((r for r in res if isinstance(r, unreal.SkeletalMesh)), None)
            if not sk:
                log("char: XATO", ch, res)
                continue
        skeleton = skeleton or sk.get_editor_property("skeleton")
        # slot nomi = FBX material nomi ('.' -> '_'); Unity'dagi mos material
        by_name = {k.replace(".", "_").replace(" ", "_"): v for k, v in chars[ch]["materialsByName"].items()}
        mats = sk.get_editor_property("materials")
        miss = []
        for i, m in enumerate(mats):
            slot = str(m.get_editor_property("material_slot_name"))
            mid = by_name.get(slot)
            if not mid:
                ren = chars[ch]["renderers"][0]
                names = [n for n in ren["matNames"]]
                mid = ren["mats"][names.index(slot)] if slot in names else (ren["mats"][i] if i < len(ren["mats"]) else None)
            mi = asset("%s/Materials/%s" % (ROOT, mid)) if mid else None
            if mi:
                m.set_editor_property("material_interface", mi)
                mats[i] = m                      # massiv elementi nusxa: qaytarib yoziladi
            else:
                miss.append(slot)
        sk.set_editor_property("materials", mats)
        log("char:", ch, "skeleton", sk.get_editor_property("skeleton").get_name(), "slots", len(mats), "topilmadi", miss)
    save_dir(ROOT + "/Characters")
    log("char: tayyor", "%.0fs" % (time.time() - T0))


# ============================================================ animatsiyalar (IK Retarget)
SOURCES = {
    "Vendors": "/Game/Vendors_And_Customers/Demo/Mannequin/Character/Mesh/SK_Mannequin",
    "WW100": "/Game/WW100IdleAnimPack/Demo/Characters/Mannequin_UE4/Meshes/SK_Mannequin",
    "Dialog": "/Game/DialogAnimations/Character/Mesh/SK_Mannequin",
    "City": "/Game/CityAnimations_pack2/Mannequin/Character/Mesh/SK_Mannequin",
    "Civilian": "/Game/CivilianNPCsAI/TPS_Example/Characters/Mannequins/Meshes/SKM_Manny",
}
EXTRA = ["AS_Walking__7__Anim", "AS_Walking__4__Anim", "MM_Idle", "MF_Idle"]


def make_rig(name, mesh):
    path = "%s/Retarget/%s" % (ROOT, name)
    if bc.eal.does_asset_exist(path):
        bc.eal.delete_asset(path)
    r = bc.asset_tools.create_asset(name, ROOT + "/Retarget", unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(r)
    c.set_skeletal_mesh(mesh)
    ok = c.apply_auto_generated_retarget_definition() and c.apply_auto_fbik()
    bc.eal.save_loaded_asset(r)
    log("anim: rig", name, ok, len(c.get_retarget_chains()))
    return r


def make_retargeter(name, src_rig, tgt_rig, smesh, tmesh):
    S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
    path = "%s/Retarget/%s" % (ROOT, name)
    if bc.eal.does_asset_exist(path):
        bc.eal.delete_asset(path)
    r = bc.asset_tools.create_asset(name, ROOT + "/Retarget", unreal.IKRetargeter, unreal.IKRetargetFactory())
    c = unreal.IKRetargeterController.get_controller(r)
    c.set_ik_rig(S, src_rig)
    c.set_ik_rig(T, tgt_rig)
    c.set_preview_mesh(S, smesh)
    c.set_preview_mesh(T, tmesh)
    if c.get_num_retarget_ops() == 0:
        c.add_default_ops()
    c.assign_ik_rig_to_all_ops(S, src_rig)
    c.assign_ik_rig_to_all_ops(T, tgt_rig)
    c.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    c.auto_align_all_bones(T)
    bc.eal.save_loaded_asset(r)
    return r


def retarget_animations():
    need = sorted({a["state"] for a in scene["npc"]["actions"]} | set(EXTRA))
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    by_src = collections.defaultdict(list)
    src_skel = {k: unreal.load_asset(v).get_editor_property("skeleton").get_path_name() for k, v in SOURCES.items()}
    found = set()
    for ad in reg.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Engine", "AnimSequence"), True):
        name = str(ad.asset_name)
        pkg = str(ad.package_name)
        if name not in need or name in found or pkg.startswith(ROOT) or pkg.startswith("/Game/Characters/") or pkg.startswith("/Game/Test"):
            continue
        sk = ad.get_asset().get_editor_property("skeleton").get_path_name()
        src = next((k for k, v in src_skel.items() if v == sk), None)
        if src:
            by_src[src].append(ad)
            found.add(name)
    log("anim: topildi", len(found), "/", len(need), "yo'q:", sorted(set(need) - found))
    tgt = unreal.load_asset(ROOT + "/Characters/Yigit_01")
    tgt_rig = make_rig("IK_BozorOdam", tgt)
    out = []
    for src, ads in by_src.items():
        smesh = unreal.load_asset(SOURCES[src])
        srig = make_rig("IK_Src_" + src, smesh)
        rtg = make_retargeter("RTG_%s_BozorOdam" % src, srig, tgt_rig, smesh, tgt)
        res = unreal.IKRetargetBatchOperation.duplicate_and_retarget(ads, smesh, tgt, rtg, target_path=ROOT + "/Animations",
                                                                    include_referenced_assets=False, overwrite_existing_files=True)
        out += list(res)
        log("anim:", src, len(ads), "->", len(res), "%.0fs" % (time.time() - T0))
    save_dir(ROOT + "/Animations")
    save_dir(ROOT + "/Retarget")
    log("anim: tayyor", len(out))


if "tex" in STAGES:
    import_textures()
if "mat" in STAGES:
    build_materials()
if "mesh" in STAGES:
    import_meshes()
if "char" in STAGES:
    import_characters()
if "anim" in STAGES:
    retarget_animations()
log("HAMMASI TAYYOR", STAGES, "%.0fs" % (time.time() - T0))
