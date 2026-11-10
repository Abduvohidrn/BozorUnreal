"""Avtobus va bekat yozuv/reklama teksturalarini PNG'ga chiqaradi (Saved/Yozuvlar) va qaysi material qaysi teksturani ishlatishini yozadi.
UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script=Scripts/probe_yozuvlar.py
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Yozuvlar")
os.makedirs(OUT, exist_ok=True)
lines = []

TEX = [
    "/Game/City_Bus/Textures/T_Label_BaseColor",
    "/Game/City_Bus/Textures/Decals/T_Decal_Zero_Emission",
    "/Game/City_Bus/Textures/Decals/T_Decal_Advertisement",
    "/Game/City_Bus/Textures/Decals/T_Decal_Fully_Electric",
    "/Game/City_Bus/Textures/Decals/T_Decal_STAR_Transit",
    "/Game/City_Bus/Textures/Decals/T_Decal_STAR_Transit_2",
    "/Game/City_Bus/Textures/T_Lights_BaseColor",
    "/Game/City_Bus/Textures/T_Reg_Plate",
    "/Game/Bekat/T_bus_shelter02_albedo",
]
for p in TEX:
    t = unreal.load_asset(p)
    if not t:
        lines.append("YOQ " + p)
        continue
    task = unreal.AssetExportTask()
    task.object = t
    task.filename = os.path.join(OUT, t.get_name() + ".png")
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.TextureExporterPNG()
    ok = unreal.Exporter.run_asset_export_task(task)
    lines.append("%s %s %dx%d srgb=%s comp=%s" % (p, ok, t.blueprint_get_size_x(), t.blueprint_get_size_y(), t.srgb, t.compression_settings))

# qaysi material T_Label / Decal ishlatadi
for p in ("/Game/City_Bus/Materials/M_Reg_Plate", "/Game/City_Bus/Materials/M_Labels","/Game/City_Bus/Materials/Decals/MI_Decal_Zero_Emission", "/Game/City_Bus/Materials/Decals/M_Decal"):
    m = unreal.load_asset(p)
    if not m:
        continue
    lines.append("MAT %s %s" % (p, m.get_class().get_name()))
    if isinstance(m, unreal.MaterialInstance):
        for tp in m.texture_parameter_values:
            lines.append("   tex %s = %s" % (tp.parameter_info.name, tp.parameter_value.get_path_name() if tp.parameter_value else None))
        for sp in m.scalar_parameter_values:
            lines.append("   scl %s = %s" % (sp.parameter_info.name, sp.parameter_value))
    else:
        lines.append("   textures %s" % [x.get_path_name() for x in unreal.MaterialEditingLibrary.get_used_textures(m)])

# M_Labels qaysi mesh'larda
for p in ("/Game/City_Bus/Meshes/SM_Bus", "/Game/City_Bus/Meshes/Interior/SM_Interior", "/Game/City_Bus/Meshes/SM_Electric_Text",
          "/Game/City_Bus/Meshes/Interior/SM_Interior_SectionF", "/Game/City_Bus/Meshes/SM_Bus_SectionR"):
    sm = unreal.load_asset(p)
    if sm:
        lines.append("MESH %s %s" % (p, [(str(s.material_slot_name), s.material_interface.get_name() if s.material_interface else None) for s in sm.static_materials]))

with open(os.path.join(OUT, "probe.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
