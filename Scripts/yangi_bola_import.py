# Yangi bosh qahramon: Fab "Cartoon Young Boy Rigged" (Epic skeleti) ni import qiladi,
# skeletini bola va Manny skeletlari bilan mos qiladi, o'lchamlarini chiqaradi.
import unreal

FBX = r'C:\ProgramData\Epic\EpicGamesLauncher\VaultCache\FabLibrary\Cartoon_Young_Boy_Rigged-cb88681d\fbx\cartoon_young_boy_fbx_extracted\cartoon Young Boy_Fbx\cartoon_young_boy.fbx'
DEST = '/Game/BoshQahramon/YangiBola'
E = unreal.EditorAssetLibrary

if not E.does_asset_exist(DEST + '/SKM_YangiBola'):
    task = unreal.AssetImportTask()
    task.filename = FBX
    task.destination_path = DEST
    task.destination_name = 'SKM_YangiBola'
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    print('[YB] import:', list(task.imported_object_paths))

for p in E.list_assets(DEST, recursive=True):
    a = unreal.load_asset(p)
    print('[YB] asset:', p, a.get_class().get_name() if a else None)

mesh = next((unreal.load_asset(p) for p in E.list_assets(DEST, recursive=True)
             if isinstance(unreal.load_asset(p), unreal.SkeletalMesh)), None)
if mesh:
    sk = mesh.get_editor_property('skeleton')
    b = mesh.get_bounds()
    print('[YB] mesh:', mesh.get_path_name(), 'skeleton:', sk.get_path_name())
    print('[YB] bounds origin', b.origin, 'extent', b.box_extent)
    mos = list(sk.get_editor_property('compatible_skeletons'))
    for path in ('/Game/Characters/Mannequins/Meshes/SK_Mannequin', '/Game/BoshQahramon/Bola/Rig/SKM_Bola_Skeleton'):
        s = unreal.load_asset(path)
        if s and all((m.get_path_name() if hasattr(m, 'get_path_name') else str(m)) != s.get_path_name() for m in mos):
            mos.append(s)
    sk.set_editor_property('compatible_skeletons', mos)
    E.save_loaded_asset(sk)
    ref = mesh.get_editor_property('skeleton')
    names = [str(sk.get_bone_name(i)) for i in range(sk.get_num_bones())] if hasattr(sk, 'get_num_bones') else []
    print('[YB] suyaklar soni:', len(names), names[:12])
    print('[YB] materiallar:', [str(m.get_editor_property('material_slot_name')) for m in mesh.get_editor_property('materials')])
