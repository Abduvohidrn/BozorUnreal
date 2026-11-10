"""Paketlarning AnimBP, montaj va kliplarini bozor odamlari skeletiga retarget qiladi va paket Blueprint'larini ularga ulaydi.

  Civilian NPCs AI : AB_NPC (+ blendspace/kliplar) va BP_NPC o'ynaydigan AM_* montajlar -> /Game/Bozor/Pack/Civilian
  NPC Queue system : ABP_Manny (xaridor)                                               -> /Game/Bozor/Pack/Queue
  100 Idle          : ABP_UE4_RandomAnimation va StandUpright aktori ishlatadigan 9 klip -> /Game/Bozor/Pack/WW100
Keyin BP_NPC (montajlar) va BP_UE4_RandomAnimation_StandUpright (kliplar) dagi havolalar yangi nusxalarga almashadi,
Civilian manekenining soketlari (Phone ...) bozor skeletiga ko'chiriladi.
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import importlib
import bozor_common as bc
importlib.reload(bc)
import unreal

bc.LOG = r"D:\Unreal Projects\BozorUnreal\Saved\retarget_packs.txt"
log = bc.log
lib = unreal.BozorLevelTools
reg = unreal.AssetRegistryHelpers.get_asset_registry()
reg.search_all_assets(True)
ROOT = bc.ROOT
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET

target = unreal.load_asset(ROOT + "/Characters/Yigit_01")
tgt_rig = unreal.load_asset(ROOT + "/Retarget/IK_BozorOdam")


def rig_and_retargeter(name, mesh_path):
    """Manba manekeni uchun IK Rig va Retargeter (bor bo'lsa qayta ishlatiladi)."""
    rtg_path = "%s/Retarget/RTG_%s_BozorOdam" % (ROOT, name)
    mesh = unreal.load_asset(mesh_path)
    if bc.eal.does_asset_exist(rtg_path):
        return mesh, unreal.load_asset(rtg_path)
    rig = bc.asset_tools.create_asset("IK_Src_" + name, ROOT + "/Retarget", unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig)
    c.set_skeletal_mesh(mesh)
    c.apply_auto_generated_retarget_definition()
    c.apply_auto_fbik()
    rtg = bc.asset_tools.create_asset("RTG_%s_BozorOdam" % name, ROOT + "/Retarget", unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(S, rig)
    rc.set_ik_rig(T, tgt_rig)
    rc.set_preview_mesh(S, mesh)
    rc.set_preview_mesh(T, target)
    if rc.get_num_retarget_ops() == 0:
        rc.add_default_ops()
    rc.assign_ik_rig_to_all_ops(S, rig)
    rc.assign_ik_rig_to_all_ops(T, tgt_rig)
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    rc.auto_align_all_bones(T)
    bc.eal.save_loaded_asset(rig)
    bc.eal.save_loaded_asset(rtg)
    return mesh, rtg


def retarget(pack, mesh_path, asset_paths, dest):
    mesh, rtg = rig_and_retargeter(pack, mesh_path)
    ads = [bc.eal.find_asset_data(p) for p in asset_paths]
    ads = [a for a in ads if a and a.is_valid()]
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget(ads, mesh, target, rtg, target_path=dest,
                                                                include_referenced_assets=True, overwrite_existing_files=True)
    log("retarget", pack, len(ads), "->", len(res))
    return {str(r.asset_name): str(r.package_name) for r in res}


def assets_in(path, cls):
    return [str(a.package_name) for a in reg.get_assets_by_path(path, True) if str(a.asset_class_path.asset_name) == cls]


# ---- Civilian
civ_montages = assets_in("/Game/CivilianNPCsAI/AI_BP/AnimationBlueprint/AnimationMontage", "AnimMontage")
civ = retarget("Civilian", "/Game/CivilianNPCsAI/TPS_Example/Characters/Mannequins/Meshes/SKM_Manny",
               ["/Game/CivilianNPCsAI/AI_BP/AnimationBlueprint/AB_NPC"] + civ_montages, ROOT + "/Pack/Civilian")
# ---- Queue
que = retarget("Queue", "/Game/NPCQueueSystem/Characters/Mannequins/Meshes/SKM_Manny",
               ["/Game/NPCQueueSystem/Characters/Mannequins/Animations/ABP_Manny"], ROOT + "/Pack/Queue")
# ---- 100 Idle
ww_actor = "/Game/WW100IdleAnimPack/Blueprints/PlaceActor/Default/BP_UE4_RandomAnimation_StandUpright"
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
# aktor endi bozor nusxalariga havola qiladi (oldingi ishga tushirish): manba kliplar nomi bo'yicha olinadi
ww_anims = [p for p in assets_in("/Game/WW100IdleAnimPack/Animations/UE4", "AnimSequence") if p.split("/")[-1].startswith("AS_StandUpright_")]
ww = retarget("WW100", "/Game/WW100IdleAnimPack/Demo/Characters/Mannequin_UE4/Meshes/SK_Mannequin",
              ["/Game/WW100IdleAnimPack/Blueprints/ABP/Default/ABP_UE4_RandomAnimation"] + ww_anims, ROOT + "/Pack/WW100")

# ---- paket Blueprint'laridagi havolalarni yangi nusxalarga
bp_npc = unreal.load_asset("/Game/CivilianNPCsAI/AI_BP/BP_NPC")
bp_ww = unreal.load_asset(ww_actor)
old, new = [], []
for p in civ_montages + ww_anims:
    name = p.split("/")[-1]
    src = civ if p in civ_montages else ww
    if name in src:
        old.append(unreal.load_asset(p))
        new.append(unreal.load_asset(src[name]))
n = lib.replace_references(old, new)
log("havola almashtirildi:", n, "/", len(old))
for bp in (bp_npc, bp_ww):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    bc.eal.save_loaded_asset(bp)

# ---- soketlar (telefon, sigaret, ichimlik qo'lga biriktiriladi)
skel = unreal.load_asset(ROOT + "/Characters/Yigit_01_Skeleton")
k = lib.copy_sockets(unreal.load_asset("/Game/CivilianNPCsAI/TPS_Example/Characters/Mannequins/Meshes/SKM_Manny"), skel)
bc.eal.save_loaded_asset(skel)
log("soketlar:", k)
for d in ("/Pack/Civilian", "/Pack/Queue", "/Pack/WW100", "/Retarget"):
    bc.eal.save_directory(ROOT + d, only_if_is_dirty=True, recursive=True)
log("natija Civilian:", sorted(civ)[:60])
log("natija Queue:", sorted(que))
log("natija WW100:", sorted(ww))
log("TAYYOR")
