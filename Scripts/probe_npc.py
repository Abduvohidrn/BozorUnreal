"""ABozorNpc uchun: ABP_Manny slotlari, BP_Customer mesh joylashuvi, arava/paket o'lchamlari."""
import unreal
OUT = r"D:\Unreal Projects\BozorUnreal\Saved\probe_npc.txt"
L = []
def p(*a): L.append(" ".join(str(x) for x in a))
for path in ("/Game/NPCQueueSystem/Blueprints/BP_Customer", "/Game/CivilianNPCsAI/AI_BP/BP_NPC"):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    cdo = unreal.get_default_object(cls)
    m = cdo.get_editor_property("mesh")
    cap = cdo.get_editor_property("capsule_component")
    p(path, "mesh loc", m.get_editor_property("relative_location"), "rot", m.get_editor_property("relative_rotation"),
      "cap", cap.get_unscaled_capsule_radius(), cap.get_unscaled_capsule_half_height())
abp = unreal.load_asset("/Game/Bozor/Pack/Queue/ABP_Manny")
p("abp skeleton", abp.get_editor_property("target_skeleton"))
try:
    p("slots", unreal.AnimationLibrary.get_animation_graphs(abp))
except Exception as e:
    p("graphs err", e)
sk = unreal.load_asset("/Game/Bozor/Characters/Yigit_01_Skeleton")
p("skeleton", sk)
for n in ("SM_SM_Aravakash_Arava_LOD0", "SM_SM_Paket_Qora_LOD0"):
    sm = unreal.load_asset("/Game/Bozor/Meshes/" + n)
    b = sm.get_bounding_box()
    p(n, b.min, b.max)
ch = unreal.load_asset("/Game/Bozor/Characters/Yigit_01")
p("char bounds", ch.get_bounds())
p("char skel", ch.get_editor_property("skeleton"))
open(OUT, "w", encoding="utf-8").write("\n".join(L))
