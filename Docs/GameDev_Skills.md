# Game Dev uchun Claude Code skill'lari

BozorUnreal loyihasida (Unreal Engine 5 + Blender) ishlatiladigan Claude Code skill'lari ro'yxati: har biri nima qiladi va qayerdan o'rnatiladi.

## O'rnatish

Skill'lar [`skills`](https://github.com/vercel-labs/skills) CLI orqali o'rnatiladi (Node.js kerak):

```bash
# bitta skill
npx skills add <owner>/<repo>@<skill-nomi> -g -y

# repodagi hamma skill'lar
npx skills add <owner>/<repo> -g
```

- `-g`: global, ya'ni `~/.claude/skills/` papkasiga o'rnatadi (hamma loyihada ishlaydi)
- `-y`: tasdiq so'ramaydi

O'rnatilgandan keyin Claude Code'ni qayta oching. Skill'lar mos vazifada avtomatik ishga tushadi yoki ularni `/skill-nomi` deb chaqirasiz.

---

## 1. Unreal Engine: quodsoler/unreal-engine-skills

Manba: https://github.com/quodsoler/unreal-engine-skills

UE 5.8 header'lari bo'yicha tekshirilgan, chuqur C++ skill'lar.

| Skill | Nima uchun | O'rnatish |
|---|---|---|
| `ue-animation-system` | AnimInstance, Montage, AnimNotify, Blend Space, State Machine, root motion, Motion Matching | `npx skills add quodsoler/unreal-engine-skills@ue-animation-system -g -y` |
| `ue-audio-system` | MetaSound, SoundCue, attenuation, submix, reverb, Quartz | `npx skills add quodsoler/unreal-engine-skills@ue-audio-system -g -y` |
| `ue-input-system` | Enhanced Input (C++), trigger/modifier, key rebinding | `npx skills add quodsoler/unreal-engine-skills@ue-input-system -g -y` |
| `ue-ui-umg-slate` | UMG, Slate, CommonUI, MVVM, gamepad focus | `npx skills add quodsoler/unreal-engine-skills@ue-ui-umg-slate -g -y` |

Repoda boshqa skill'lar ham bor (masalan `ue-actor-component-architecture`, `ue-character-movement`, `ue-gameplay-abilities`). Hammasini o'rnatish:

```bash
npx skills add quodsoler/unreal-engine-skills -g
```

## 2. Unreal Engine: gamedev-skills/awesome-gamedev-agent-skills

Manba: https://github.com/gamedev-skills/awesome-gamedev-agent-skills (papka: `skills/unreal/`)

Unreal bo'yicha yengilroq, amaliy skill'lar.

| Skill | Nima uchun | O'rnatish |
|---|---|---|
| `unreal-cpp-gameplay` | UCLASS/UPROPERTY/UFUNCTION, GameMode, Character, Build.cs | `npx skills add gamedev-skills/awesome-gamedev-agent-skills@unreal-cpp-gameplay -g -y` |
| `unreal-blueprints` | Blueprint, Event Graph, Interface, Event Dispatcher | `npx skills add gamedev-skills/awesome-gamedev-agent-skills@unreal-blueprints -g -y` |
| `unreal-behavior-trees` | NPC AI: Behavior Tree, Blackboard, AIController, BTTask/Service | `npx skills add gamedev-skills/awesome-gamedev-agent-skills@unreal-behavior-trees -g -y` |
| `unreal-enhanced-input` | IA_/IMC_ asset'lar, ETriggerEvent bilan bog'lash | `npx skills add gamedev-skills/awesome-gamedev-agent-skills@unreal-enhanced-input -g -y` |
| `unreal-niagara` | Niagara VFX, emitter/module, User parametrlar | `npx skills add gamedev-skills/awesome-gamedev-agent-skills@unreal-niagara -g -y` |
| `unreal-packaging` | Package Project, Shipping build, cook, RunUAT BuildCookRun | `npx skills add gamedev-skills/awesome-gamedev-agent-skills@unreal-packaging -g -y` |

Bu repoda Unreal'dan tashqari quyidagi skill'lar ham bor:
- `skills/disciplines/`: game-ai, level-design, game-feel, dialogue-systems, save-systems, procedural-gen, shader-programming, performance-optimization...
- `skills/genres/`: rpg, fps-shooter, platformer, roguelike, survival-crafting...
- `skills/unity/`, `skills/godot/`, `skills/other-engines/` (Bevy, Roblox, Love2D, Pygame...)

Hammasini o'rnatish:

```bash
npx skills add gamedev-skills/awesome-gamedev-agent-skills -g
```

Yoki repo'ni klon qilib, kerakli papkalarni `~/.claude/skills/` ga nusxalash mumkin:

```bash
git clone https://github.com/gamedev-skills/awesome-gamedev-agent-skills.git
```

## 3. Blender: scenario-labs/skills

Manba: https://github.com/scenario-labs/skills (papka: `skills/dcc/blender/`)

Blender'da Python yoki MCP orqali professional darajadagi 3D ish. Blender MCP bridge kerak bo'ladi.

| Skill | Nima uchun | O'rnatish |
|---|---|---|
| `scenario-blender-expert` | Modeling, sculpt, retopo, export'dan oldin tekshiruv (asosiy skill, qolganlari unga tayanadi) | `npx skills add scenario-labs/skills@scenario-blender-expert -g -y` |
| `scenario-blender-rigging` | Rigify, weight paint, IK/FK, ARKit 52 blend shape, viseme | `npx skills add scenario-labs/skills@scenario-blender-rigging -g -y` |
| `scenario-blender-uv-baking` | UV unwrap, seam, texel density, normal/AO bake | `npx skills add scenario-labs/skills@scenario-blender-uv-baking -g -y` |
| `scenario-blender-texturing-shading` | PBR material, stylized teri, ko'z, edge wear | `npx skills add scenario-labs/skills@scenario-blender-texturing-shading -g -y` |
| `scenario-blender-geometry-nodes` | Procedural modeling, scatter, simulation zone | `npx skills add scenario-labs/skills@scenario-blender-geometry-nodes -g -y` |

Bu skill'lar bir-biriga bog'liq, shuning uchun beshalasini birga o'rnatgan ma'qul.

## 4. Blender (personaj): arjun988/blender-skills

Manba: https://github.com/arjun988/blender-skills (papka: `.claude/skills/`), MIT litsenziya

| Skill | Nima uchun | O'rnatish |
|---|---|---|
| `character-artist` | O'yin personajlari: anatomiya, yuz topologiyasi, kiyim, soch, animatsiyaga tayyor mesh | `npx skills add arjun988/blender-skills@character-artist -g -y` |
| `cloth-sim` | Kiyim blockout, cloth simulyatsiya, pin group, shape key'ga bake | `npx skills add arjun988/blender-skills@cloth-sim -g -y` |

## 5. Yordamchi skill'lar

| Skill | Nima uchun | Manba | O'rnatish |
|---|---|---|---|
| `graphify` | Kod bazasi, hujjat va rasmlardan knowledge graph quradi, katta loyihani tushunishga yordam beradi | https://github.com/graphify-labs/graphify | `npx skills add graphify-labs/graphify@graphify -g -y` |
| `find-skills` | Yangi skill qidirib o'rnatadi ("find a skill for X") | https://github.com/vercel-labs/skills | `npx skills add vercel-labs/skills@find-skills -g -y` |

## 6. Plugin: text-to-cad

Bu skill emas, Claude Code plugin'i. Ichida CAD, STEP, STL, DXF, URDF va 3D print skill'lari hamda 3D modelni chatda ko'rsatadigan viewer bor.

Manba: https://github.com/earthtojake/text-to-cad

Claude Code ichida:

```
/plugin marketplace add earthtojake/text-to-cad
/plugin install text-to-cad@earthtojake
```

---

## Hammasini bir yo'la o'rnatish

```bash
npx skills add quodsoler/unreal-engine-skills -g -y
npx skills add gamedev-skills/awesome-gamedev-agent-skills -g -y
npx skills add scenario-labs/skills -g -y
npx skills add arjun988/blender-skills -g -y
npx skills add graphify-labs/graphify@graphify -g -y
npx skills add vercel-labs/skills@find-skills -g -y
```

Eslatma: butun repo o'rnatilsa, bu ro'yxatdagidan ko'proq skill tushadi. Faqat keraklilari kerak bo'lsa, jadvallardagi `@skill-nomi` buyruqlaridan foydalaning.

Boshqa skill'larni qidirish uchun: https://skills.sh
