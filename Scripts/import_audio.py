"""Bozor ovozlarini import qiladi va ovoz tuzilmasini quradi: /Game/Bozor/Audio.

  Fon/       - 7 ta fon qatlami (BozorKit/Audio/Ambience, generate_ambience.py): bir xil uzunlikdagi choksiz loop'lar.
               Asl balandlikda olinadi (Unity'dagi -1 dBFS'ga ko'tarilgan nusxalar emas): Unreal'da balandlik 1 dan oshishi mumkin,
               zona gain'lari generate_zonalar.py dagi dB'lar bilan to'g'ridan-to'g'ri qo'yiladi.
  Sotuvchi/  - sotuvchilarning o'zbekcha gaplari (generate_sotuvchi.py): sot_<tur>_<bolim>_<persona>_<nn>, mono.
  SFX/       - rastalardagi nuqtaviy ovozlar (generate_sfx.py): sfx_<tur>_<nn> - tarozi, qop, idish, tanga, pul, paket; mono.
  Qadam/     - o'yinchi qadamlari sirt bo'yicha (generate_sfx.py): qadam_<sirt>_<nn>; Qadam/Sirt - PM_<Sirt> Physical Material'lar,
               yer materiallariga biriktiriladi, Surface Type nomlari DefaultEngine.ini PhysicsSettings'da.
  Tuzilma/   - Sound Class'lar, Sound Mix'lar, attenuation va concurrency.

Sound Class daraxti (sozlamalardagi slayderlar shu klasslarga Sound Mix override bilan ulanadi):
  SC_Master
    SC_Fon      - fon qatlamlari
    SC_Ovoz     - sotuvchi va NPC gaplari; gapirayotganda fon biroz pasayadi (passiv Mix_Ovoz_Ducking)
    SC_SFX      - nuqtaviy ovozlar (tarozi, tanga, qop...), qadamlar
    SC_UI       - savdo menyusi
    SC_Musiqa
Mix_Asosiy - loyihaning base sound mix'i (AudioSettings), hamma klass 1.0: slayderlar shu mix'ga override qo'yadi.

Qayta ishga tushirsa bo'ladi: mavjud assetlar ustidan yoziladi.
Ishga tushirish: UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/import_audio.py
"""
import glob
import os
import re
import sys
import time

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\import_audio.txt"
log = bc.log
SRC = r"D:\Trk\BozorKit_hammasi\BozorKit\Audio"
AUDIO = bc.ROOT + "/Audio"
TUZ = AUDIO + "/Tuzilma"
STEMS = ("olomon", "sotuvchi", "qadamlar", "shahar", "bozor", "qushlar", "savdo")   # BozorFonZonalar.Stems tartibi
# asosiy fon: bozorda yozilgan olomon yozuvlaridan stereo loop (BozorKit/Audio/tools/bozor_yozuv_bolish.py --fon).
# Asset nomi eskicha qoladi (bozor_fon_mix_YANGI) - xaritadagi Bozor_Fon aktori shunga ulangan
FON_MIX = os.path.join(SRC, "Ambience", "bozor_fon_haqiqiy_loop.wav")
FON_MIX_ASSET = "bozor_fon_mix_YANGI"
# zona fonlari (bozor_fon.ZONA_FONLAR): fayl -> asset nomi
FON_ZONALAR = (("fon_zona_kirish_loop.wav", "fon_zona_kirish"), ("fon_zona_ovqat_loop.wav", "fon_zona_ovqat"))
T0 = time.time()


def load(path):
    return unreal.load_asset(path) if bc.eal.does_asset_exist(path) else None


def create(name, folder, cls, factory):
    """Bor bo'lsa o'shani qaytaradi (unga bog'langan havolalar uzilmasin), yo'q bo'lsa yaratadi."""
    return load("%s/%s" % (folder, name)) or bc.asset_tools.create_asset(name, folder, cls, factory)


def setp(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as e:
        log("audio: %s.%s o'rnatilmadi: %s" % (obj.get_name(), prop, e))
        return False


def import_waves(files, dest):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", False)
        tasks.append(t)
    bc.asset_tools.import_asset_tasks(tasks)
    waves = []
    for f in files:
        w = load("%s/%s" % (dest, os.path.splitext(os.path.basename(f))[0]))
        if isinstance(w, unreal.SoundWave):
            waves.append(w)
        else:
            log("audio: import bo'lmadi", f)
    return waves


# ============================================================ Sound Class'lar
def build_classes():
    sc = {n: create(n, TUZ, unreal.SoundClass, unreal.SoundClassFactory())
          for n in ("SC_Master", "SC_Fon", "SC_Ovoz", "SC_SFX", "SC_UI", "SC_Musiqa")}
    master = sc["SC_Master"]
    children = [sc[n] for n in ("SC_Fon", "SC_Ovoz", "SC_SFX", "SC_UI", "SC_Musiqa")]
    setp(master, "child_classes", children)
    for c in children:
        setp(c, "parent_class", master)
    return sc


# ============================================================ Sound Mix'lar
def adjuster(cls, volume):
    a = unreal.SoundClassAdjuster()
    a.set_editor_property("sound_class_object", cls)
    a.set_editor_property("volume_adjuster", volume)
    a.set_editor_property("pitch_adjuster", 1.0)
    a.set_editor_property("apply_to_children", True)
    return a


def build_mixes(sc):
    base = create("Mix_Asosiy", TUZ, unreal.SoundMix, unreal.SoundMixFactory())
    setp(base, "sound_class_effects", [adjuster(sc[n], 1.0) for n in ("SC_Fon", "SC_Ovoz", "SC_SFX", "SC_UI", "SC_Musiqa")])

    # sotuvchi eshitiladigan darajada gapirsa fon -3 dB ga tushadi: tez pasayadi, sekin qaytadi
    duck = create("Mix_Ovoz_Ducking", TUZ, unreal.SoundMix, unreal.SoundMixFactory())
    setp(duck, "sound_class_effects", [adjuster(sc["SC_Fon"], 0.7)])
    setp(duck, "fade_in_time", 0.3)
    setp(duck, "fade_out_time", 1.2)
    pm = unreal.PassiveSoundMixModifier()
    pm.set_editor_property("sound_mix", duck)
    pm.set_editor_property("min_volume_threshold", 0.15)   # uzoqdagi, past eshitiladigan gap fonni bosmasin
    pm.set_editor_property("max_volume_threshold", 10.0)
    setp(sc["SC_Ovoz"], "passive_sound_mix_modifiers", [pm])
    return base


# ============================================================ attenuation va concurrency
def build_voice_rules():
    # Unity BozorSotuvchiOvozi: minDistance 1.5 m, maxDistance 35 m (logarifmik), uzoqdagisi bo'g'iq
    att = create("Att_Sotuvchi", TUZ, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    s = att.get_editor_property("attenuation")
    for k, v in (("attenuate", True), ("spatialize", True),
                 ("attenuation_shape", unreal.AttenuationShape.SPHERE),
                 ("attenuation_shape_extents", unreal.Vector(150, 0, 0)),
                 ("falloff_distance", 3350.0),
                 ("distance_algorithm", unreal.AttenuationDistanceModel.NATURAL_SOUND),
                 ("d_b_attenuation_at_max", -60.0),
                 ("non_spatialized_radius_start", 50.0), ("non_spatialized_radius_end", 100.0),
                 ("attenuate_with_lpf", True),
                 ("lpf_radius_min", 500.0), ("lpf_radius_max", 3500.0),
                 ("lpf_frequency_at_min", 20000.0), ("lpf_frequency_at_max", 3500.0)):
        setp_struct(s, k, v)
    att.set_editor_property("attenuation", s)

    # bir vaqtda eng ko'pi bilan 4 ta sotuvchi gapi; ortig'ida eng past eshitilayotgani to'xtaydi
    con = create("Con_Sotuvchi", TUZ, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
    c = con.get_editor_property("concurrency")
    setp_struct(c, "max_count", 4)
    setp_struct(c, "resolution_rule", unreal.MaxConcurrentResolutionRule.STOP_QUIETEST)
    setp_struct(c, "volume_scale", 1.0)
    con.set_editor_property("concurrency", c)
    return att, con


def build_sfx_rules():
    # rasta ovozlari kichik va mayda: 1 m gacha to'liq, ~18 m da so'nadi; uzoqdagisi tezroq bo'g'iqlashadi
    att = create("Att_SFX", TUZ, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    s = att.get_editor_property("attenuation")
    for k, v in (("attenuate", True), ("spatialize", True),
                 ("attenuation_shape", unreal.AttenuationShape.SPHERE),
                 ("attenuation_shape_extents", unreal.Vector(100, 0, 0)),
                 ("falloff_distance", 1700.0),
                 ("distance_algorithm", unreal.AttenuationDistanceModel.NATURAL_SOUND),
                 ("d_b_attenuation_at_max", -60.0),
                 ("attenuate_with_lpf", True),
                 ("lpf_radius_min", 300.0), ("lpf_radius_max", 1800.0),
                 ("lpf_frequency_at_min", 20000.0), ("lpf_frequency_at_max", 3000.0)):
        setp_struct(s, k, v)
    att.set_editor_property("attenuation", s)

    # bir vaqtda eng ko'pi bilan 6 ta rasta ovozi; ortig'ida eng pasti to'xtaydi
    con = create("Con_SFX", TUZ, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
    c = con.get_editor_property("concurrency")
    setp_struct(c, "max_count", 6)
    setp_struct(c, "resolution_rule", unreal.MaxConcurrentResolutionRule.STOP_QUIETEST)
    setp_struct(c, "voice_steal_release_time", 0.08)    # to'xtatilgan ovoz chirsillamasin
    con.set_editor_property("concurrency", c)
    return att, con


def setp_struct(s, prop, value):
    try:
        s.set_editor_property(prop, value)
    except Exception as e:
        log("audio: %s.%s o'rnatilmadi: %s" % (type(s).__name__, prop, e))


# ============================================================ to'lqinlar
def import_fon(sc):
    files = [os.path.join(SRC, "Ambience", "bozor_fon_%s_loop.wav" % n) for n in STEMS]
    waves = import_waves(files, AUDIO + "/Fon")
    # asosiy fon (ABozorFonZonalar.Mix) - zonali qatlamlar o'rniga; fayl nomi asset nomidan farq qiladi
    named = [(FON_MIX, FON_MIX_ASSET)] + [(os.path.join(SRC, "Ambience", f), n) for f, n in FON_ZONALAR]
    for path, name in named:
        if not os.path.exists(path):
            continue
        t = unreal.AssetImportTask()
        for k, v in (("filename", path), ("destination_path", AUDIO + "/Fon"), ("destination_name", name),
                     ("automated", True), ("replace_existing", True), ("save", False)):
            t.set_editor_property(k, v)
        bc.asset_tools.import_asset_tasks([t])
        w = load("%s/Fon/%s" % (AUDIO, name))
        if isinstance(w, unreal.SoundWave):
            waves.append(w)
    for w in waves:
        setp(w, "looping", True)
        setp(w, "sound_class_object", sc["SC_Fon"])
        # 60 s stereo: xotirada to'liq ushlab turilmaydi, oqim bilan o'qiladi
        setp(w, "loading_behavior", unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
    # xaritadagi eski bitta fon ham shu klassga
    old = load(AUDIO + "/Bozor_Fon")
    if old:
        setp(old, "sound_class_object", sc["SC_Fon"])
    log("audio: fon qatlamlari", len(waves), "%.0fs" % (time.time() - T0))
    return waves


def import_sotuvchi(sc, att, con):
    files = sorted(glob.glob(os.path.join(SRC, "Sotuvchi", "sot_*.wav")))
    waves = import_waves(files, AUDIO + "/Sotuvchi")
    for w in waves:
        setp(w, "looping", False)
        setp(w, "sound_class_object", sc["SC_Ovoz"])
        setp(w, "attenuation_settings", att)
        cs = unreal.Set(unreal.SoundConcurrency)
        cs.add(con)
        setp(w, "concurrency_set", cs)
    kinds = {}
    for w in waves:
        m = re.match(r"sot_([a-z]+)_", w.get_name())
        if m:
            kinds[m.group(1)] = kinds.get(m.group(1), 0) + 1
    log("audio: sotuvchi gaplari", len(waves), "/", len(files), kinds, "%.0fs" % (time.time() - T0))
    return waves


def import_sfx(sc, att, con):
    files = sorted(glob.glob(os.path.join(SRC, "SFX", "sfx_*.wav")))
    waves = import_waves(files, AUDIO + "/SFX")
    for w in waves:
        setp(w, "looping", False)
        setp(w, "sound_class_object", sc["SC_SFX"])
        setp(w, "attenuation_settings", att)
        setp(w, "loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE)   # qisqa: xotirada tursin, darhol o'ynasin
        cs = unreal.Set(unreal.SoundConcurrency)
        cs.add(con)
        setp(w, "concurrency_set", cs)
    log("audio: rasta SFX", len(waves), "/", len(files), "%.0fs" % (time.time() - T0))
    return waves


def import_qadam(sc):
    # o'yinchining o'z qadami: yaqindan eshitiladi, uzoqqa ketmaydi. Manba oyoq ostida (kameradan ~1.2 m pastda):
    # 200 sm dan yaqinda 2D'ga aralasha boshlaydi, 60 sm da to'liq 2D - qadamlar quloqda chap-o'ng sakramaydi
    att = create("Att_Qadam", TUZ, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    s = att.get_editor_property("attenuation")
    for k, v in (("attenuate", True), ("spatialize", True),
                 ("attenuation_shape", unreal.AttenuationShape.SPHERE),
                 ("attenuation_shape_extents", unreal.Vector(200, 0, 0)),
                 ("falloff_distance", 1300.0),
                 ("distance_algorithm", unreal.AttenuationDistanceModel.NATURAL_SOUND),
                 ("d_b_attenuation_at_max", -60.0),
                 ("non_spatialized_radius_start", 200.0),
                 ("non_spatialized_radius_end", 60.0)):
        setp_struct(s, k, v)
    att.set_editor_property("attenuation", s)
    con = create("Con_Qadam", TUZ, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
    c = con.get_editor_property("concurrency")
    setp_struct(c, "max_count", 4)
    setp_struct(c, "resolution_rule", unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
    setp_struct(c, "voice_steal_release_time", 0.05)
    con.set_editor_property("concurrency", c)

    files = sorted(glob.glob(os.path.join(SRC, "Qadam", "qadam_*.wav")))
    waves = import_waves(files, AUDIO + "/Qadam")
    for w in waves:
        setp(w, "looping", False)
        setp(w, "sound_class_object", sc["SC_SFX"])
        setp(w, "attenuation_settings", att)
        setp(w, "loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
        cs = unreal.Set(unreal.SoundConcurrency)
        cs.add(con)
        setp(w, "concurrency_set", cs)
    log("audio: qadamlar", len(waves), "/", len(files), "%.0fs" % (time.time() - T0))
    return waves


# ============================================================ pavilyon akustikasi
# Reverb effektlari - yopiq hududlardagi AAudioVolume'lar uchun (bozor_ovoz.place_pavilyonlar). Tashqi ovozni bo'g'ish
# (AudioVolume interior settings) faqat bApplyAmbientVolumes yoqilgan klasslarga ta'sir qiladi: gaplar va rasta/qadam ovozlari.
# SC_Fon'da o'chiq: fon manbai o'yinchi bilan yuradi, uning tashqi qatlamlarini ABozorFonZonalar o'zi pasaytiradi.
REVERBS = (
    # nom            decay  density diffusion gain  gain_hf refl_gain refl_delay late_gain late_delay decay_hf
    ("Rev_Pavilyon", 0.9, 0.8, 0.7, 0.32, 0.85, 0.6, 0.008, 1.2, 0.012, 0.7),    # profnastil tom, yonlari ochiq
    ("Rev_Gosht", 1.4, 1.0, 0.85, 0.35, 0.8, 0.8, 0.012, 1.3, 0.02, 0.6),        # devorli zal, kafel
)


def build_acoustics(sc):
    for n in ("SC_Ovoz", "SC_SFX"):
        p = sc[n].get_editor_property("properties")
        p.set_editor_property("apply_ambient_volumes", True)
        sc[n].set_editor_property("properties", p)
    keys = ("decay_time", "density", "diffusion", "gain", "gain_hf", "reflections_gain", "reflections_delay",
            "late_gain", "late_delay", "decay_hf_ratio")
    for row in REVERBS:
        r = create(row[0], TUZ, unreal.ReverbEffect, unreal.ReverbEffectFactory())
        for k, v in zip(keys, row[1:]):
            setp(r, k, float(v))
    log("audio: reverb", [r[0] for r in REVERBS], "ambient volumes: SC_Ovoz, SC_SFX")


# ============================================================ sirtlar (qadam ovozi uchun)
# Surface Type -> Physical Material -> yer materiallari. ABozorQadamOvozlari qadam ostidagi sirt nomini
# (PhysicsSettings.PhysicalSurfaces) oladi va qadam_<nom> kliplaridan chaladi.
SIRTLAR = (
    ("Beton", ("MI_M_Beton", "MI_M_Beton_Dogli")),
    ("Asfalt", ("MI_M_Asfalt", "MI_M_Asfalt_Yamoq", "MI_M_StripedAsphalt_Markaz", "MI_M_StripedAsphalt_Toza",
                "MI_M_StripedAsphalt_Toza_Oyna", "MI_M_StripedAsphalt_Yol")),
    ("Maysa", ("MI_M_Yer_Maysa",)),
    ("Tuproq", ("MI_M_Yer_Tuproq", "MI_M_Uzoq_Yer")),
    ("Yogoch", ("MI_M_Yogoch",)),
    ("Metall", ("MI_M_Metall_Boyalgan",)),
)


def build_surfaces():
    folder = AUDIO + "/Qadam/Sirt"
    n = 0
    for i, (name, mats) in enumerate(SIRTLAR, 1):
        pm = create("PM_" + name, folder, unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew())
        # nomlanmagan SurfaceType1.. Python enum'ida ko'rinmaydi: C++ yordamchi raqam bilan qo'yadi
        unreal.BozorLevelTools.set_surface_type(pm, i)
        for m in mats:
            mi = load("%s/Materials/%s" % (bc.ROOT, m))
            if not mi:
                log("audio: material yo'q", m)
                continue
            setp(mi, "override_phys_material", True)
            setp(mi, "phys_material", pm)
            n += 1
    bc.eal.save_directory(bc.ROOT + "/Materials", only_if_is_dirty=True, recursive=False)
    ini_section("[/Script/Engine.PhysicsSettings]", {}, "+PhysicalSurfaces",
                ['(Type=SurfaceType%d,Name="%s")' % (i, name) for i, (name, _) in enumerate(SIRTLAR, 1)])
    log("audio: sirtlar", [s[0] for s in SIRTLAR], "material", n)


# ============================================================ loyiha sozlamalari
def ini_section(section, keys, list_key=None, list_values=()):
    """DefaultEngine.ini bo'limida keys'ni qo'yadi/almashtiradi; list_key ("+Nom") qatorlari list_values bilan almashadi."""
    path = r"D:\Unreal Projects\BozorUnreal\Config\DefaultEngine.ini"
    text = open(path, encoding="utf-8").read()
    if section not in text:
        text = text.rstrip("\n") + "\n\n" + section + "\n"
    head, _, rest = text.partition(section)
    body, sep, tail = rest.partition("\n[")
    for k, v in keys.items():
        if re.search(r"^%s=" % re.escape(k), body, flags=re.M):
            body = re.sub(r"^%s=.*$" % re.escape(k), "%s=%s" % (k, v), body, flags=re.M)
        else:
            body = body.rstrip("\n") + "\n%s=%s\n" % (k, v)
    if list_key:
        body = re.sub(r"^%s=.*\n?" % re.escape(list_key), "", body, flags=re.M)
        body = body.rstrip("\n") + "\n" + "".join("%s=%s\n" % (list_key, v) for v in list_values)
    text = head + section + body + (sep + tail if sep else "\n")
    open(path, "w", encoding="utf-8").write(text)


def set_project_audio(sc, base):
    """DefaultEngine.ini: klassi berilmagan ovozlar SC_Master'ga, base mix - Mix_Asosiy."""
    lines = {"DefaultSoundClassName": sc["SC_Master"].get_path_name(),
             "DefaultBaseSoundMix": base.get_path_name()}
    ini_section("[/Script/Engine.AudioSettings]", lines)
    log("audio: AudioSettings", lines)


# ============================================================
sc = build_classes()
base = build_mixes(sc)
att, con = build_voice_rules()
import_fon(sc)
import_sotuvchi(sc, att, con)
import_sfx(sc, *build_sfx_rules())
import_qadam(sc)
build_surfaces()
build_acoustics(sc)
bc.eal.save_directory(AUDIO, only_if_is_dirty=False, recursive=True)
set_project_audio(sc, base)
log("audio: TAYYOR %.0fs" % (time.time() - T0))
