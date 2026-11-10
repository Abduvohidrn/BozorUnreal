# Yangi bola teksturalari: FBX'da tekstura yo'q, GLB'da bor. GLB alohida papkaga import qilinadi
# (glTF PBR materiallari teksturalari bilan), so'ng materiallar nomi bo'yicha SKM_YangiBola slotlariga ulanadi.
import unreal

GLB = r'C:\ProgramData\Epic\EpicGamesLauncher\VaultCache\FabLibrary\Cartoon_Young_Boy_Rigged-cb88681d\glb\cartoon_young_boy_glb_extracted\cartoon Young Boy_Glb\cartoon Young Boy.glb'
DEST = '/Game/BoshQahramon/YangiBola/GLB'
E = unreal.EditorAssetLibrary

if not E.does_directory_exist(DEST) or not E.list_assets(DEST):
    task = unreal.AssetImportTask()
    task.filename = GLB
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    print('[TX] import:', len(task.imported_object_paths))

mats = {}
for p in E.list_assets(DEST, recursive=True):
    a = unreal.load_asset(p)
    if isinstance(a, unreal.MaterialInterface):
        mats[a.get_name()] = a
print('[TX] GLB materiallari:', sorted(mats))

mesh = unreal.load_asset('/Game/BoshQahramon/YangiBola/SKM_YangiBola')
slots = list(mesh.get_editor_property('materials'))
for s in slots:
    nom = str(s.get_editor_property('material_slot_name'))
    m = mats.get(nom) or mats.get('MI_' + nom) or next((v for k, v in mats.items() if k.endswith(nom)), None)
    if m:
        s.set_editor_property('material_interface', m)
    print('[TX] slot', nom, '->', m.get_name() if m else 'TOPILMADI')
mesh.set_editor_property('materials', slots)
print('[TX] saqlandi', E.save_loaded_asset(mesh))
