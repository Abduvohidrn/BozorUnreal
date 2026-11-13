# Menyu fonini import qiladi: Saved/Screenshots/MenyuFon dagi tanlangan kadr -> /Game/UI/Fon/T_Menyu_Fon
# Ishga tushirish: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript -script="Scripts/ui_fon_import.py"
import unreal

MANBA = 'D:/Unreal Projects/BozorUnreal/Assets/UI/T_Menyu_Fon.png'

t = unreal.AssetImportTask()
t.filename = MANBA
t.destination_path = '/Game/UI/Fon'
t.destination_name = 'T_Menyu_Fon'
t.automated = True
t.replace_existing = True
t.save = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
tx = unreal.load_asset('/Game/UI/Fon/T_Menyu_Fon')
tx.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
tx.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
tx.set_editor_property('never_stream', True)
unreal.EditorAssetLibrary.save_asset('/Game/UI/Fon/T_Menyu_Fon', only_if_is_dirty=False)
unreal.log('ui_fon_import: %dx%d' % (tx.blueprint_get_size_x(), tx.blueprint_get_size_y()))
