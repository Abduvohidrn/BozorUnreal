"""Xarita ishlatadigan barcha assetlar (rekursiv bog'liqliklar) ro'yxati: Saved/xarita_bogliqlik.txt - har qatorda Content ichidagi
fayl yo'li. Git'ga hammasi kirganini tekshirish uchun (faqat o'qiydi). Xarita: -Xarita=/Game/... (bo'lmasa Bozor).
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script="Scripts/xarita_bogliqlik.py"
"""
import os
import unreal

LOYIHA = os.path.abspath(unreal.Paths.project_dir())
XARITA = "/Game/Bozor/Maps/Bozor"
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.wait_for_completion()
opt = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
korildi, navbat = set(), [XARITA]
while navbat:
    p = navbat.pop()
    if p in korildi:
        continue
    korildi.add(p)
    for d in ar.get_dependencies(p, opt) or []:
        d = str(d)
        if d.startswith("/Game/") and d not in korildi:
            navbat.append(d)
qator = []
for p in sorted(korildi):
    rel = "Content/" + p[len("/Game/"):]
    for ext in (".uasset", ".umap"):
        if os.path.exists(os.path.join(LOYIHA, rel + ext)):
            qator.append(rel + ext)
            break
    else:
        qator.append("DISKDA_YOQ " + rel)
open(os.path.join(LOYIHA, "Saved", "xarita_bogliqlik.txt"), "w", encoding="utf-8").write("\n".join(qator))
unreal.log_warning("XARITA_BOGLIQLIK %d paket" % len(qator))
