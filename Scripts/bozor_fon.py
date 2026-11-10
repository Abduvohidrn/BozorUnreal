"""Bozorning zonali fon ovozi: MetaSound (MS_Bozor_Fon) va xaritadagi ABozorFonZonalar aktori.

MS_Bozor_Fon - 7 ta fon qatlami (import_audio.py, /Game/Bozor/Audio/Fon) bitta manbada: har qatlam o'z Wave Player'ida aylanadi,
hammasi bitta On Play'dan bir xil "Boshlanish" joyidan boshlanadi (bir loop'ning bo'laklari - namunagacha sinxron) va Stereo Mixer (7)
ga tushadi. Har qatlam balandligi - grafning float kirishi (olomon, sotuvchi, ... savdo; chiziqli gain): ularni ABozorFonZonalar
har kadr o'yinchi turgan zonaga qarab qo'yadi. Mixer gain'ni blok ichida silliq o'tkazadi - kadrlar orasida chirsillamaydi.

Ishga tushirish (xaritani qayta qurmasdan): UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/fon_yangila.py
build_level.py ham shu modulni chaqiradi.
"""
import unreal

ROOT = "/Game/Bozor"
AUDIO = ROOT + "/Audio"
MS_NAME = "MS_Bozor_Fon"
MIX = AUDIO + "/Fon/bozor_fon_mix_YANGI"
STEMS = ("olomon", "sotuvchi", "qadamlar", "shahar", "bozor", "qushlar", "savdo")   # ABozorFonZonalar::Stems tartibi
OK = unreal.MetaSoundBuilderResult.SUCCEEDED


class FonError(Exception):
    pass


def _ok(r, what):
    if r != OK:
        raise FonError("%s: %s" % (what, r))


def build_metasound(log):
    """MS_Bozor_Fon bor bo'lsa o'shani qaytaradi. build_to_asset mavjud asset ustidan yozmaydi, commandlet'da o'chirib qayta
    yaratib ham bo'lmaydi (nom xotirada band qoladi): grafni o'zgartirish uchun assetni Content Browser'da o'chirib, qayta ishga tushiring."""
    path = "%s/%s" % (AUDIO, MS_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        log("fon: %s mavjud, qayta qurilmadi" % MS_NAME)
        return unreal.load_asset(path)
    bs = unreal.get_engine_subsystem(unreal.MetaSoundBuilderSubsystem)
    es = unreal.get_editor_subsystem(unreal.MetaSoundEditorSubsystem)
    waves = [unreal.load_asset("%s/Fon/bozor_fon_%s_loop" % (AUDIO, s)) for s in STEMS]
    if not all(waves):
        raise FonError("fon qatlamlari yo'q - avval import_audio.py")

    b, on_play, _on_finished, outs, r = bs.create_source_builder(
        "BozorFonBuilder", output_format=unreal.MetaSoundOutputAudioFormat.STEREO, is_one_shot=False)
    _ok(r, "source builder")

    def node(ns, name, variant=""):
        n, r = b.add_node_by_class_name(unreal.MetasoundFrontendClassName(ns, name, variant))
        _ok(r, "tugun %s %s" % (name, variant))
        return n

    def pin_in(n, name):
        h, r = b.find_node_input_by_name(n, name)
        _ok(r, "kirish " + name)
        return h

    def pin_out(n, name):
        h, r = b.find_node_output_by_name(n, name)
        _ok(r, "chiqish " + name)
        return h

    def connect(o, i, what):
        _ok(b.connect_nodes(o, i), "ulash " + what)

    def default(h, literal, what):
        _ok(b.set_node_input_default(h, literal), "qiymat " + what)

    zero, _ = bs.create_float_meta_sound_literal(0.0)
    start, r = b.add_graph_input_node("Boshlanish", "Time", zero)
    _ok(r, "kirish Boshlanish")

    mixer = node("AudioMixer", "Audio Mixer (Stereo, %d)" % len(STEMS))
    for i, (stem, wave) in enumerate(zip(STEMS, waves)):
        wp = node("UE", "Wave Player", "Stereo")
        default(pin_in(wp, "Wave Asset"), bs.create_object_meta_sound_literal(wave), stem)
        default(pin_in(wp, "Loop"), bs.create_bool_meta_sound_literal(True)[0], "Loop")
        default(pin_in(wp, "Maintain Audio Sync"), bs.create_bool_meta_sound_literal(True)[0], "Sync")
        connect(on_play, pin_in(wp, "Play"), stem + " Play")
        connect(start, pin_in(wp, "Start Time"), stem + " Start Time")
        connect(pin_out(wp, "Out Left"), pin_in(mixer, "In %d L" % i), stem + " L")
        connect(pin_out(wp, "Out Right"), pin_in(mixer, "In %d R" % i), stem + " R")
        g, r = b.add_graph_input_node(stem, "Float", zero)
        _ok(r, "kirish " + stem)
        connect(g, pin_in(mixer, "Gain %d" % i), stem + " Gain")
    connect(pin_out(mixer, "Out L"), outs[0], "chiqish L")
    connect(pin_out(mixer, "Out R"), outs[1], "chiqish R")

    # SC_Fon klassi birinchi qatlamdan ko'chadi
    _ms, r = es.build_to_asset(b, "BozorKit", MS_NAME, AUDIO, template_sound_wave=waves[0])
    _ok(r, "asset")
    ms = unreal.load_asset(path)
    unreal.EditorAssetLibrary.save_loaded_asset(ms)
    log("fon: %s %d qatlam, klass %s" % (MS_NAME, len(STEMS), ms.get_editor_property("sound_class_object").get_name()
                                         if ms.get_editor_property("sound_class_object") else None))
    return ms


def area_box(amb):
    """scene.json "ambience" (Unity bozor maydoni, metr, markaz + o'lcham) -> Unreal Box (sm): (x, y, z) -> (z, x, y)."""
    c, s = amb["center"], amb["size"]
    lo = unreal.Vector((c[2] - s[2] / 2) * 100, (c[0] - s[0] / 2) * 100, (c[1] - s[1] / 2) * 100)
    hi = unreal.Vector((c[2] + s[2] / 2) * 100, (c[0] + s[0] / 2) * 100, (c[1] + s[1] / 2) * 100)
    box = unreal.Box()
    box.set_editor_property("min", lo)
    box.set_editor_property("max", hi)
    box.set_editor_property("is_valid", True)
    return box


def place(world, log, ms=None, amb=None):
    """Eski Bozor_Fon (AmbientSound) o'rniga ABozorFonZonalar. Zonalar - C++ dagi standart qiymatlar, maydon - scene.json."""
    ms = ms or unreal.load_asset("%s/%s" % (AUDIO, MS_NAME))
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    removed = 0
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AmbientSound):
        if a.get_actor_label() == "Bozor_Fon":
            actors.destroy_actor(a)
            removed += 1
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BozorFonZonalar):
        actors.destroy_actor(a)
    a = actors.spawn_actor_from_class(unreal.BozorFonZonalar, unreal.Vector(2100, 0, 200), unreal.Rotator(0, 0, 0))
    a.set_actor_label("Bozor_Fon")
    a.set_folder_path("Muhit")
    # asosiy fon - bitta tayyor mix (import_audio.py FON_MIX); bo'lsa zonali qatlamlar (MetaSound) ulanmaydi
    mix = unreal.load_asset(MIX) if unreal.EditorAssetLibrary.does_asset_exist(MIX) else None
    if mix:
        a.set_editor_property("mix", mix)
    else:
        a.set_editor_property("sound", ms)
    if amb:
        a.set_editor_property("area", area_box(amb))
        a.set_editor_property("volume", float(amb.get("volume", 0.8)))
        for k, prop in (("halfDistance", "half_distance"), ("fadeStart", "fade_start"), ("maxDistance", "max_distance")):
            if k in amb:
                a.set_editor_property(prop, float(amb[k]) * 100)
    log("fon: ABozorFonZonalar joylandi (%s, maydon %s), eski AmbientSound o'chirildi: %d" % (
        "bitta mix " + mix.get_name() if mix else "zonali " + MS_NAME, a.get_editor_property("area"), removed))
    return a


# zona fonlari: bozorda yozilgan joyga xos olomon (import_audio.py, BozorKit/Audio/tools/bozor_yozuv_bolish.py --fon).
# Asosiy fon (Bozor_Fon, hamma joyda) ustiga qo'shiladi: quti ichida to'liq, chetidan Falloff sm da so'nadi.
# Unity metr (x0, x1, z0, z1) - ABozorFonZonalar zonalari bilan bir xil
ZONA_FONLAR = (
    ("Fon_Zona_Kirish", "fon_zona_kirish", (-16.0, 16.0, -8.0, 3.0), 0.8, 600.0),      # darvoza: ko'cha, mashinalar
    ("Fon_Zona_Ovqat", "fon_zona_ovqat", (8.0, 16.0, 33.8, 44.0), 0.8, 400.0),        # go'sht-sut / ovqat pavilyoni
)


def place_zonalar(world, log):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.AmbientSound):
        if a.get_actor_label().startswith("Fon_Zona_"):
            actors.destroy_actor(a)
    made = []
    for label, asset, (x0, x1, z0, z1), vol, falloff in ZONA_FONLAR:
        snd = unreal.load_asset("%s/Fon/%s" % (AUDIO, asset)) if unreal.EditorAssetLibrary.does_asset_exist("%s/Fon/%s" % (AUDIO, asset)) else None
        if not snd:
            log("fon: zona ovozi yo'q - avval import_audio.py:", asset)
            continue
        center = unreal.Vector((z0 + z1) * 50, (x0 + x1) * 50, 200)
        a = actors.spawn_actor_from_class(unreal.AmbientSound, center, unreal.Rotator(0, 0, 0))
        a.set_actor_label(label)
        a.set_folder_path("Muhit/Fon_Zonalar")
        c = a.get_component_by_class(unreal.AudioComponent)
        c.set_editor_property("sound", snd)
        c.set_editor_property("volume_multiplier", vol)
        c.set_editor_property("override_attenuation", True)
        att = c.get_editor_property("attenuation_overrides")
        att.set_editor_property("attenuate", True)
        att.set_editor_property("spatialize", False)          # olomon har tomondan - 2D, faqat masofa bo'yicha so'nadi
        att.set_editor_property("attenuation_shape", unreal.AttenuationShape.BOX)
        att.set_editor_property("attenuation_shape_extents", unreal.Vector((z1 - z0) * 50, (x1 - x0) * 50, 600))
        att.set_editor_property("falloff_distance", falloff)
        c.set_editor_property("attenuation_overrides", att)
        made.append(label)
    log("fon: zona fonlari %s" % made)
    return made
