import unreal
m = unreal.load_asset("/Game/Bozor/Characters/Hammol_01")
names = [str(n) for n in unreal.SkeletalMeshEditorSubsystem.get_bone_names(m)] if hasattr(unreal, "SkeletalMeshEditorSubsystem") and hasattr(unreal.SkeletalMeshEditorSubsystem, "get_bone_names") else []
if not names:
    sk = m.get_editor_property("skeleton")
    names = [str(sk.get_bone_name(i)) for i in range(400) if str(sk.get_bone_name(i)) != "None"] if hasattr(sk, "get_bone_name") else ["?"]
open(r"D:\Unreal Projects\BozorUnreal\Saved\probe_bones.txt", "w").write(" ".join(names))
