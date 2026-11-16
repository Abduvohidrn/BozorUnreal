"""Citizen bosh mesh'lariga gapirish uchun "Ogiz_Ochiq" morph target'ini yozadi (UBozorOgizMorph) va saqlaydi.

Muharrir yopiq holda:
  UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=<to'liq yo'l>/Scripts/ogiz_morph.py -unattended -nullrhi
Natija: Saved/ogiz_morph.txt
"""
import unreal

BOSHLAR = [
    "SK_ma_head_05_asian", "SK_ma_head_05_basian", "SK_ma_head_01", "SK_ma_head_02",
    "SK_fe_head_02_asian", "SK_fe_head_02_b", "SK_fe_head_01",
]
PAPKA = "/Game/CitizenNPC/CharacterParts/Meshes/"
BURCHAK = 10.0

natija = []
for nom in BOSHLAR:
    yol = PAPKA + nom
    m = unreal.load_asset(yol)
    if not m:
        natija.append(f"{nom}: topilmadi")
        continue
    natija.append(unreal.BozorOgizMorph.ogiz_qosh(m, BURCHAK))
    natija.append(f"  morph nomlari: {list(m.get_all_morph_target_names())}")
    unreal.EditorAssetLibrary.save_loaded_asset(m, only_if_is_dirty=False)

matn = "\n".join(natija)
unreal.log(matn)
with open(unreal.Paths.project_saved_dir() + "ogiz_morph.txt", "w", encoding="utf-8") as f:
    f.write(matn)
