"""npc_ssenariy.json -> xaritadagi ABozorNpc'lar: persona, tezlik, boshlanish kechikishi, o'z animatsiyalari, qadamlar.

npc_ssenariy.json - asosiy manba (qo'lda tahrirlanadi; birinchi variantini npc_ssenariy_yarat.js yaratgan).
Muharrirda: exec(open(r"D:/Unreal Projects/BozorUnreal/Scripts/npc_ssenariy.py").read()) ; keyin xaritani saqlash.
"""
import json
import os

import unreal

HERE = r"D:/Unreal Projects/BozorUnreal/Scripts"
ANIM = "/Game/Bozor/MetaAnim/"
# EBozorStepOp qiymatlari Python'da (GoTo -> GOTO yoki GO_TO): nomni pastki chiziqsiz solishtirib topamiz
OPS = {}
for _n in dir(unreal.BozorStepOp):
    if _n.isupper():
        OPS[_n.replace("_", "")] = getattr(unreal.BozorStepOp, _n)
OPS = {k: OPS[k.upper()] for k in ("GoTo", "Anim", "Face", "Greet", "Stall", "Sit", "Wait")}
# sikl (loop) klip: kutish/bo'sh turish kategoriyalari va nomida Loop/Idle bo'lganlar
LOOP_CATS = {"V_Idle", "V_Lean", "C_Wait", "S_Loop"}


def is_loop(cat, clip):
    return cat in LOOP_CATS or "Loop" in clip or "Idle" in clip or "Waiting" in clip or clip in ("AS_Phone_Loop", "AS_Texting_Anim")


def apply(path=os.path.join(HERE, "npc_ssenariy.json")):
    data = json.load(open(path, encoding="utf-8"))["npcs"]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    npcs = {a.get_actor_label(): a for a in actors if isinstance(a, unreal.BozorNpc)}
    cache, missing, stats = {}, set(), {"npc": 0, "qadam": 0, "klip": 0}

    def clip(name):
        if name not in cache:
            p = ANIM + name
            cache[name] = unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else None
            if not cache[name]:
                missing.add(name)
        return cache[name]

    for label, e in data.items():
        a = npcs.get(label)
        if not a:
            missing.add("NPC:" + label)
            continue
        acts = []
        for cat, clips in e.get("actions", {}).items():
            for c in clips:
                an = clip(c)
                if an:
                    acts.append(unreal.BozorNpcAction(category=cat, anim=an, loop=is_loop(cat, c)))
        steps = []
        for s in e.get("steps", []):
            st = unreal.BozorStep()
            st.set_editor_property("op", OPS[s["op"]])
            if "target" in s:
                t = s["target"]
                st.set_editor_property("target", unreal.Vector(t[0], t[1], t[2]))
            if "other" in s:
                o = npcs.get(s["other"])
                if o:
                    st.set_editor_property("other", o)
                else:
                    missing.add("NPC:" + s["other"])
            for k, prop in (("clip", "clip"), ("time", "time"), ("speed", "speed"), ("index", "index")):
                if k in s:
                    st.set_editor_property(prop, s[k])
            if "buy" in s:
                st.set_editor_property("buy", bool(s["buy"]))
            steps.append(st)
        a.modify()
        a.set_editor_property("persona", e.get("persona", ""))
        a.set_editor_property("own_actions", acts)
        a.set_editor_property("script", steps)
        a.set_editor_property("start_delay", float(e.get("start_delay", 0.0)))
        if e.get("speed"):
            a.set_editor_property("walk_speed", float(e["speed"]))
        stats["npc"] += 1
        stats["qadam"] += len(steps)
        stats["klip"] += len(acts)
    print("npc_ssenariy:", stats, "topilmadi:", sorted(missing))
    return stats, missing


if __name__ == "__main__":
    apply()
