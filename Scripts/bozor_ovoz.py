"""Sotuvchilar ovozi: xaritaga ABozorSotuvchiOvozi aktori (gaplar /Game/Bozor/Audio/Sotuvchi dan, import_audio.py).

ABozorNpc savdo bosqichlarida o'zi chaqiradi (chaqiriq, salom, savdo, rahmat). Keksa sotuvchiga keksa ovoz berish uchun
NPC'larning WalkStyle'i ham scene.json dan yangilanadi (bozor_npc.py yangi quriladigan NPC'larga o'zi qo'yadi).

Ishga tushirish (xaritani qayta qurmasdan): UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/ovoz_yangila.py
build_level.py ham shu modulni chaqiradi.
"""
import unreal

import os

ROOT = "/Game/Bozor"
CLIPS = ROOT + "/Audio/Sotuvchi"
# diskdagi manba: o'yinga faqat shu yerda bor gaplar olinadi (olib tashlangan eski gaplar Unreal'da asset bo'lib qolsa ham)
CLIPS_SRC = r"D:\Trk\BozorKit_hammasi\BozorKit\Audio\Sotuvchi"
SFX = ROOT + "/Audio/SFX"
QADAM = ROOT + "/Audio/Qadam"

# rasta ovoz nuqtalari: sahnadagi rekvizit mesh nomi -> chalinadigan turlar va ikki ovoz orasidagi vaqt (s)
PROPS = (
    ("Tarozi_Elektron", ["tarozi_pip"], (14, 40)),
    ("Tarozi_Posangili", ["tarozi_tosh"], (16, 45)),
    ("_Qop_", ["qop"], (14, 40)),
    ("Tovoq", ["idish"], (10, 30)),
    ("Choynak_Piyola", ["idish"], (8, 25)),
    ("Patnis", ["idish"], (14, 40)),
)
VENDOR_KINDS, VENDOR_GAP = ["tanga", "pul", "paket"], (20, 50)


def V(v, scale=100.0):
    return unreal.Vector(v[2] * scale, v[0] * scale, v[1] * scale)


def place(world, log, scene):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    on_disk = {os.path.splitext(f)[0] for f in os.listdir(CLIPS_SRC) if f.endswith(".wav")} if os.path.isdir(CLIPS_SRC) else None
    clips = []
    for path in unreal.EditorAssetLibrary.list_assets(CLIPS, recursive=False):
        a = unreal.load_asset(path)
        if isinstance(a, unreal.SoundBase) and a.get_name().startswith("sot_") and (on_disk is None or a.get_name() in on_disk):
            clips.append(a)
    if not clips:
        log("ovoz: sotuvchi kliplari yo'q - avval import_audio.py")
        return None

    # mavjud NPC'larga walkStyle (Unity nomi = aktor label'i)
    style = {n["name"]: int(n.get("walkStyle", -1)) for n in scene["npc"]["npcs"]}
    n = 0
    for npc in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorNpc):
        s = style.get(npc.get_actor_label())
        if s is not None:
            npc.set_editor_property("walk_style", s)
            n += 1

    for old in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorSotuvchiOvozi):
        actors.destroy_actor(old)
    a = actors.spawn_actor_from_class(unreal.BozorSotuvchiOvozi, unreal.Vector(0, 0, 200), unreal.Rotator(0, 0, 0))
    a.set_actor_label("Sotuvchi_Ovozlari")
    a.set_folder_path("Muhit")
    a.set_editor_property("clips", clips)
    log("ovoz: Sotuvchi_Ovozlari %d klip, walkStyle %d NPC" % (len(clips), n))
    return a


def place_rasta(world, log, scene):
    """Rasta nuqtaviy ovozlari: ABozorRastaOvozlari, nuqtalar - tarozi, qop, idish rekvizitlari va sotuvchilar kassasi."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    clips = [unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(SFX, recursive=False)]
    clips = [c for c in clips if isinstance(c, unreal.SoundBase) and c.get_name().startswith("sfx_")]
    if not clips:
        log("ovoz: rasta SFX yo'q - avval import_audio.py")
        return None

    def nuqta(loc, kinds, gap):
        return unreal.BozorRastaNuqta(location=loc, kinds=kinds, gap=unreal.Vector2D(gap[0], gap[1]))

    points, stats = [], {}
    for o in scene["objects"]:
        for key, kinds, gap in PROPS:
            if key in o["mesh"]:
                p = V(o["pos"])
                points.append(nuqta(unreal.Vector(p.x, p.y, p.z + 15), kinds, gap))
                stats[key] = stats.get(key, 0) + 1
                break
    # sotuvchi kassasi: peshtaxta tomonda, qo'l balandligida
    for n in scene["npc"]["npcs"]:
        if n["role"] == "Sotuvchi":
            p = V(n["pos"])
            f = unreal.Quat(n["rot"][2], n["rot"][0], n["rot"][1], n["rot"][3]).rotator().get_forward_vector()
            points.append(nuqta(unreal.Vector(p.x + f.x * 50, p.y + f.y * 50, p.z + 100), VENDOR_KINDS, VENDOR_GAP))
            stats["sotuvchi"] = stats.get("sotuvchi", 0) + 1

    for old in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorRastaOvozlari):
        actors.destroy_actor(old)
    a = actors.spawn_actor_from_class(unreal.BozorRastaOvozlari, unreal.Vector(0, 0, 200), unreal.Rotator(0, 0, 0))
    a.set_actor_label("Rasta_Ovozlari")
    a.set_folder_path("Muhit")
    a.set_editor_property("clips", clips)
    a.set_editor_property("points", points)
    log("ovoz: Rasta_Ovozlari %d klip, %d nuqta %s" % (len(clips), len(points), stats))
    return a


def place_qadam(world, log):
    """O'yinchi qadamlari: ABozorQadamOvozlari (kliplar /Game/Bozor/Audio/Qadam, sirtlar import_audio.py SIRTLAR)."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    clips = [unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets(QADAM, recursive=False)]
    clips = [c for c in clips if isinstance(c, unreal.SoundBase) and c.get_name().startswith("qadam_")]
    if not clips:
        log("ovoz: qadam kliplari yo'q - avval import_audio.py")
        return None
    for old in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorQadamOvozlari):
        actors.destroy_actor(old)
    a = actors.spawn_actor_from_class(unreal.BozorQadamOvozlari, unreal.Vector(0, 0, 200), unreal.Rotator(0, 0, 0))
    a.set_actor_label("Qadam_Ovozlari")
    a.set_folder_path("Muhit")
    a.set_editor_property("clips", clips)
    log("ovoz: Qadam_Ovozlari %d klip" % len(clips))
    return a


# yopiq hududlar (Unity metr: x0, x1, z0, z1, tom balandligi) - tom plitalaridan hisoblangan (profnastil + brezent);
# reverb, ichkarida tashqi ovoz, tashqarida ichki ovoz (balandlik, LPF)
PAVILYONLAR = (
    ("Pavilyon_Old", -12.0, 9.0, 2.5, 15.5, 3.0, "Rev_Pavilyon", 0.45, (0.6, 5000), (0.85, 8000)),
    ("Pavilyon_Orqa", -12.0, 9.0, 17.5, 30.5, 3.0, "Rev_Pavilyon", 0.45, (0.6, 5000), (0.85, 8000)),
    ("Pavilyon_Gosht_Sut", 7.5, 16.0, 33.5, 44.0, 3.5, "Rev_Gosht", 0.6, (0.4, 2500), (0.6, 3000)),
)


def place_pavilyonlar(world, log):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AudioVolume):
        if a.get_actor_label().startswith("Pavilyon_"):
            actors.destroy_actor(a)
    made = []
    for name, x0, x1, z0, z1, h, rev, wet, ext, inn in PAVILYONLAR:
        h += 1.5     # tinglovchi - Third Person kamerasi, o'yinchidan baland va orqada: tomdan biroz yuqorigacha
        center = unreal.Vector((z0 + z1) * 50, (x0 + x1) * 50, h * 50)
        v = actors.spawn_actor_from_class(unreal.AudioVolume, center, unreal.Rotator(0, 0, 0))
        v.set_actor_label(name)
        v.set_folder_path("Muhit/Akustika")
        unreal.BozorLevelTools.make_box_brush(v, unreal.Vector((z1 - z0) * 100, (x1 - x0) * 100, h * 100))
        r = v.get_editor_property("settings")
        r.set_editor_property("apply_reverb", True)
        r.set_editor_property("reverb_effect", unreal.load_asset("%s/Audio/Tuzilma/%s" % (ROOT, rev)))
        r.set_editor_property("volume", wet)
        r.set_editor_property("fade_time", 1.0)
        v.set_editor_property("settings", r)
        z = v.get_editor_property("ambient_zone_settings")
        for k, val in (("exterior_volume", ext[0]), ("exterior_lpf", ext[1]), ("exterior_time", 0.8), ("exterior_lpf_time", 0.8),
                       ("interior_volume", inn[0]), ("interior_lpf", inn[1]), ("interior_time", 0.8), ("interior_lpf_time", 0.8)):
            z.set_editor_property(k, float(val))
        v.set_editor_property("ambient_zone_settings", z)
        made.append(name)
    log("ovoz: pavilyon AudioVolume'lari", made)
    return made


def cleanup_clips(log):
    """Diskdan olib tashlangan sotuvchi gaplarining Unreal assetlarini o'chiradi (xarita saqlangandan keyin chaqiriladi:
    aktor ularga endi havola qilmaydi)."""
    if not os.path.isdir(CLIPS_SRC):
        return 0
    on_disk = {os.path.splitext(f)[0] for f in os.listdir(CLIPS_SRC) if f.endswith(".wav")}
    n = 0
    for path in unreal.EditorAssetLibrary.list_assets(CLIPS, recursive=False):
        name = path.rsplit("/", 1)[-1].split(".")[0]
        if name.startswith("sot_") and name not in on_disk and unreal.EditorAssetLibrary.delete_asset(path):
            n += 1
    log("ovoz: eski sotuvchi gaplari o'chirildi: %d" % n)
    return n
