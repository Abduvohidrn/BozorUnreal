// Har bir NPC'ning qo'lda belgilangan ssenariysini yaratadi: npc_sahna.json -> npc_ssenariy.json
// (bir martalik vosita; keyin npc_ssenariy.json qo'lda tahrirlanadi va npc_ssenariy.py uni xaritaga yozadi).
// Qoidalar: D:\Trk\BozorKit_hammasi\npc_ssenariy_rejasi.md (tadqiqot 1-bo'lim, animatsiya xaritasi 2-bo'lim).
// Ishga tushirish: node npc_ssenariy_yarat.js
const fs = require("fs");
const path = require("path");
const S = JSON.parse(fs.readFileSync(path.join(__dirname, "npc_sahna.json"), "utf8"));

// ---- deterministik tasodif (har ishga tushirishda bir xil natija)
let seed = 20261109;
const rnd = () => ((seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
const pick = (a) => a[Math.floor(rnd() * a.length)];
const range = (lo, hi) => Math.round((lo + rnd() * (hi - lo)) * 10) / 10;

// ---- persona: tanaga qarab (MetaHuman tanasi = yosh/jins)
const PERSONA_BY_BODY = {
  SKM_MH_KeksaOtaxon_BodyMesh: "keksa_ota", SKM_MH_DehqonOta_BodyMesh: "keksa_ota", SKM_MH_KeksaMomo_BodyMesh: "keksa_momo",
  SKM_MH_Onaxon_BodyMesh: "ona", SKM_MH_XaridorAyol_BodyMesh: "ona", SKM_MH_YoshSotuvchiQiz_BodyMesh: "qiz", SKM_MH_Bosh_02_BodyMesh: "qiz",
  SKM_MH_TalabaYigit_BodyMesh: "yigit", SKM_MH_YoshDehqon_BodyMesh: "yigit", SKM_MH_AravakashYigit_BodyMesh: "hammol",
  SKM_MH_ZamonaviyXaridor_BodyMesh: "erkak", SKM_MH_OrtaYoshliSotuvchi_BodyMesh: "erkak", SKM_MH_KochaSavdogari_BodyMesh: "erkak",
};
// tezlik sm/s (tadqiqot 1.4: xaridor 70-100, yo'lovchi 110-140, hammol 130-150, keksa sekin)
const SPEED = { keksa_ota: [62, 75], keksa_momo: [58, 70], ona: [80, 92], qiz: [88, 100], yigit: [95, 110], erkak: [88, 102], hammol: [130, 145] };

// ---- animatsiyalar (2-bo'lim). AS = AnimationSequence_ prefiksli Vendors_And_Customers kliplari
const A = (n) => "AnimationSequence_" + n;
const CLIPS = {
  // xaridor - umumiy
  C_Greet: [A("Cashier_Greeting_Bow")],
  C_Thanks: [A("Cashier_Greeting_Bow")],
  C_Ask: ["Dialog_Calm_Short_01", "Dialog_Calm_Short_02", "Dialog_Calm_Short_03"],
  C_Think: ["AS_Thinking_Anim"],
  C_Reject: ["AS_Disappointed_Anim"],
  C_Browse: [A("Customer_Standing_BrowsingItems01"), A("Customer_Standing_BrowsingItems02")],
  C_Pick: ["Collect_in_basket_1", "Collect_in_basket_2", "Collect_in_basket_3", "Collect_in_basket_4", "Collect_in_basket_5"],
};
// persona bo'yicha farqlanadiganlar
const PERSONA_CLIPS = {
  keksa_ota: {
    C_Haggle: ["Dialog_Calm_Medium_01", "Dialog_Calm_Long_01", "Dialog_Calm_Long_03"],
    C_Wait: [A("Customer_Standing_WaitingInLine01"), "Waiting_1", "AS_Idle__2__Anim"],
    P_Pause: ["AS_Thinking_Anim", "AS_Idle__2__Anim"],
  },
  keksa_momo: {
    C_Haggle: ["Dialog_Calm_Medium_01", "Dialog_Calm_Short_02", "Dialog_Calm_Long_01"],
    C_Wait: [A("Customer_Standing_WaitingInLine02"), "Waiting_2"],
    P_Pause: ["AS_Idle__2__Anim", "Waiting_2"],
  },
  ona: {
    C_Haggle: ["Dialog_Intense_Short_01", "Dialog_Intense_Short_02", "Dialog_Intense_Medium_01", "Dialog_Calm_Medium_01"],
    C_Wait: [A("Customer_Standing_WaitingInLine02"), A("Customer_Standing_WaitingInLine03")],
    P_Pause: ["AS_Phone_Loop", "AS_Thinking_Anim"],
  },
  qiz: {
    C_Haggle: ["Dialog_Calm_Short_01", "Dialog_Calm_Short_03", "Dialog_Intense_Short_01"],
    C_Wait: ["AS_Texting_Anim", A("Customer_Standing_WaitingInLine03")],
    P_Pause: ["AS_Texting_Anim", "AS_Phone_Loop"],
  },
  yigit: {
    C_Haggle: ["Dialog_Intense_Short_01", "Dialog_Intense_Short_03", "Dialog_Calm_Short_02"],
    C_Wait: ["AS_Texting_Anim", "Waiting_3"],
    P_Pause: ["AS_Phone_Loop", "AS_Texting_Anim", "AS_Arm_Stretching_Anim"],
  },
  erkak: {
    C_Haggle: ["Dialog_Intense_Short_01", "Dialog_Intense_Medium_02", "Dialog_Calm_Medium_01"],
    C_Wait: [A("Customer_Standing_WaitingInLine01"), "Waiting_1"],
    P_Pause: ["AS_Phone_Loop", "AS_Arm_Stretching_Anim", "AS_Thinking_Anim"],
  },
  hammol: { P_Pause: ["AS_Arm_Stretching_Anim", "Waiting_3"] },
};
// sotuvchi: bo'sh payt (1.3: suyanadi, mol teradi, telefon, chaqiradi), savdo javoblari
const VENDOR_BASE = {
  V_Greet: [A("Cashier_Greeting_Bow")],
  V_Thanks: [A("Cashier_Greeting_Bow")],
  V_Show: [A("Cashier_Show_LeftSide"), A("Cashier_Show_RightSide")],
  V_Yes: [A("Cashier_Talk_HeadNod_Yes")],
  V_No: [A("Cashier_Talk_HeadNod_No")],
  V_Offer: [A("Cashier_Handout")],
  V_Call: [A("Salesman_HandGesture_WaveOver"), A("Cashier_Greeting_WaveHello"), A("Salesman_HandGesture_OpenArms")],
};
const VENDOR_IDLE = {
  keksa: {
    V_Idle: [A("Bartender_Idle_Standing_HandsOnTable_Loop"), "AS_StandUpright_Normal01_Loop01", "AS_Idle__2__Anim"],
    V_Lean: [A("Bartender_Idle_Standing_HandsOnTable_ElbowLean_Loop"), A("Bartender_Idle_Standing_HandsOnTable_Loop")],
    V_Busy: [A("Chef_WorkStation_Inspect"), A("Server_PlaceFoodOnTable_LeftSide"), "AS_Thinking_Anim"],
  },
  ayol: {
    V_Idle: [A("Cashier_Idle_Standing"), "AS_StandUpright_Normal02_Loop01", A("Bartender_Idle_Standing_HandsOnTable_Loop")],
    V_Lean: [A("Bartender_Idle_Standing_HandsOnTable_Loop")],
    V_Busy: [A("Server_PlaceFoodOnTable_LeftSide"), A("Server_PlaceFoodOnTable_RightSide"), A("Bartender_Idle_Standing_HandsOnTable_WipeCounter_Loop"), "AS_Phone_Loop"],
  },
  erkak: {
    V_Idle: ["AS_StandUpright_Normal01_Loop01", "AS_StandUpright_Normal03_Loop01", A("Cashier_Idle_Standing")],
    V_Lean: [A("Bartender_Idle_Standing_HandsOnTable_ElbowLean_Loop"), A("Bartender_Idle_Standing_HandsOnTable_Loop")],
    V_Busy: [A("Chef_WorkStation_Inspect"), A("Server_PlaceFoodOnTable_RightSide"), "AS_Texting_Anim", "AS_Arm_Stretching_Anim"],
  },
};

// ---- joylar
const Q = S.queues;
const npcByLabel = Object.fromEntries(S.npcs.map((n) => [n.label, n]));
const aisleOf = (y) => (y < -600 ? "janub" : y > 600 ? "shimol" : y < 0 ? "markaz_j" : "markaz_sh");
const queuesIn = (aisle) => Q.filter((q) => aisleOf(q.points[0][1]) === aisle || (aisle === "markaz" && aisleOf(q.points[0][1]).startsWith("markaz")));
const usage = new Map(Q.map((q) => [q.i, 0]));
const GATE = { g: S.gate[0], s: S.gate[1], j: S.gate[2], sh: S.gate[3] };   // g'arb, sharq, janub, shimol
const CROSS = [1650, 0];
const AISLE_Y = { janub: -1060, markaz: 0, shimol: 1060 };

// rastalar: odam ko'p bormagan, o'z yo'lagidagi va x bo'yicha tartiblangan (yo'lak bo'ylab bir tomonga yuradi)
function chooseStalls(aisles, count, fromX) {
  let pool = aisles.flatMap((a) => queuesIn(a));
  pool.sort((a, b) => usage.get(a.i) - usage.get(b.i) || rnd() - 0.5);
  const picked = [];
  const vendors = new Set();
  for (const q of pool) {
    if (picked.length >= count) break;
    if (vendors.has(q.vendor)) continue;          // bitta sotuvchiga ikki marta emas
    picked.push(q);
    vendors.add(q.vendor);
    usage.set(q.i, usage.get(q.i) + 1);
  }
  const dir = fromX < 1650 ? 1 : -1;
  picked.sort((a, b) => dir * (a.points[0][0] - b.points[0][0]));
  return picked;
}
const P = (p) => [Math.round(p[0]), Math.round(p[1]), Math.round(p[2] || 2.5)];

// ---- qadam quruvchilar
const go = (p, speed) => ({ op: "GoTo", target: P(p), ...(speed ? { speed } : {}) });
const stall = (q, buy, browse) => ({ op: "Stall", index: q.i, buy, time: browse, note: q.vendor });
const anim = (clip, time) => ({ op: "Anim", clip, time });
const wait = (time, clip) => ({ op: "Wait", time, ...(clip ? { clip } : {}) });
const greet = (vendor) => ({ op: "Greet", other: vendor });
const sit = (i, time) => ({ op: "Sit", index: i, time });

// ---- xaridorlar (yolg'iz va guruh boshlig'i)
function customerScript(n, persona) {
  const steps = [];
  const home = aisleOf(n.pos[1]).startsWith("markaz") ? "markaz" : aisleOf(n.pos[1]);
  const other = home === "markaz" ? pick(["janub", "shimol"]) : "markaz";
  const elder = persona.startsWith("keksa");
  const nStalls = elder ? 3 : persona === "yigit" ? 2 : persona === "ona" ? 4 : 3;
  const first = chooseStalls([home], Math.ceil(nStalls / 2) + 1, n.pos[0]);
  const second = chooseStalls([other], Math.floor(nStalls / 2) + 1, CROSS[0]);
  // tajribali xaridor (ona, keksa) avval aylanib chiqadi: birinchi rastada faqat ko'radi, keyin qaytib sotib oladi
  const visit = (q, i) => {
    const buy = persona === "yigit" ? i === 0 : rnd() < (elder ? 0.7 : 0.6);
    if (rnd() < 0.3) steps.push(greet(q.vendor));             // tanish sotuvchi: savdodan oldin salom-alik
    steps.push(stall(q, buy, elder ? range(6, 10) : range(4, 8)));
    if (rnd() < 0.25) steps.push(anim("P_Pause", range(3, 6)));
  };
  first.forEach(visit);
  steps.push(go([CROSS[0] + range(-60, 60), AISLE_Y[home] * 0.5 + AISLE_Y[other] * 0.5]));   // ko'ndalang yo'lakdan o'tadi
  second.forEach(visit);
  if (elder && rnd() < 0.6) steps.push(sit(pick([1, 2, 3, 4]), range(30, 60)));            // choyxonada dam oladi
  if (persona === "ona" && first[0]) steps.push(stall(first[0], true, range(3, 5)));       // aylanib chiqib, birinchisidan oladi
  // bozordan chiqib, bir ozdan keyin qaytadi (ssenariy boshiga)
  const gate = pick([GATE.g, GATE.s, GATE.sh, GATE.j]);
  steps.push(go(gate));
  steps.push(wait(range(8, 20)));
  return steps;
}

// ---- yo'lovchi: bozorni kesib o'tadi, ba'zan bir rastaga qaraydi yoki choyxonada o'tiradi
function walkerScript(n, persona, k) {
  const steps = [];
  const routes = [
    [GATE.g, [900, 60], [1650, 40], [3400, 60], GATE.s],
    [GATE.j, [1650, -1060], [1650, 0], [1650, 1060], GATE.sh],
    [GATE.sh, [1650, 1060], [700, 1060], [300, 600], GATE.g],
    [GATE.s, [3900, -40], [2400, -40], [1650, -600], GATE.j],
    [GATE.g, [400, -1080], [1900, -1080], [3080, -1066], [3840, -1050], GATE.s],
  ];
  const elder = persona.startsWith("keksa");
  // keksalar choyxona (shimoli-g'arb, o'rindiq 1-4) yonidan o'tadigan yo'lda yuradi va u yerda choy ichib o'tiradi
  const r = elder ? routes[2] : routes[k % routes.length];
  const fast = persona === "yigit" || persona === "qiz" ? range(115, 135) : elder ? 0 : range(105, 120);
  r.forEach((p, i) => {
    steps.push(go(p, fast || undefined));
    if (elder && i === 3) steps.push(sit(pick([1, 2, 3, 4]), range(25, 45)));
    if (i === 1 && rnd() < 0.5) {
      // yo'lda bitta rastaga ko'z tashlaydi (ko'pincha sotib olmaydi)
      const q = chooseStalls([aisleOf(p[1]).startsWith("markaz") ? "markaz" : aisleOf(p[1])], 1, p[0])[0];
      if (q) steps.push(stall(q, rnd() < 0.3, range(3, 5)));
    }
    if (i === 2 && rnd() < 0.4) steps.push(anim("P_Pause", range(4, 7)));
  });
  steps.push(wait(range(5, 15)));
  return steps;
}

// ---- hammol: aravani bir yo'nalishda aylana yo'ldan olib yuradi (orqaga burilish kam)
function hammolScript(k) {
  const loops = [
    [[300, 0], [1650, 0], [1650, 1060], [3080, 1080], [1650, 1060], [1650, 0], [4150, -100], GATE.s, [3400, 60], [1650, 40]],
    // sharqiy qism ichidan emas (53-navbat va poldagi mol yonida arava 57 s tiqilgan), x~3160 dagi keng o'tishdan
    [[400, -1080], [1650, -1060], [3080, -1066], [3160, -900], [3160, -300], [1650, -40], [1650, -1060], [700, -1050], GATE.g, [150, -110]],
  ];
  const steps = [];
  loops[k % 2].forEach((p, i) => {
    steps.push(go(p));
    if (i % 4 === 3) steps.push(wait(range(4, 8), "P_Pause"));   // yuk tushiradi / dam oladi
  });
  return steps;
}

// ---- yig'ish
const out = { izoh: "Har NPC: persona, tezlik, boshlanish kechikishi, o'z animatsiyalari va qadamlar. Qadam turlari: GoTo, Anim, Face, Greet, Stall, Sit, Wait (npc_ssenariy_rejasi.md 3-bo'lim). Koordinatalar sm (Unreal).", npcs: {} };
let wk = 0, hk = 0, ck = 0;
for (const n of S.npcs) {
  const persona = PERSONA_BY_BODY[n.body] || (n.female ? "ona" : "erkak");
  const e = { persona, actions: {} };
  if (n.role === "SOTUVCHI") {
    const kind = persona.startsWith("keksa") ? "keksa" : n.female ? "ayol" : "erkak";
    e.persona = "sotuvchi_" + kind;
    e.actions = { ...VENDOR_BASE, ...VENDOR_IDLE[kind] };
  } else {
    e.actions = { ...CLIPS, ...(PERSONA_CLIPS[persona] || {}) };
  }
  const sp = SPEED[persona];
  if (sp && n.role !== "SOTUVCHI" && n.role !== "SUHBATDOSH") e.speed = range(sp[0], sp[1]);
  if (n.role === "XARIDOR") {
    e.steps = customerScript(n, persona);
    e.start_delay = range(0, 12) + (ck++ % 4) * 3;
  } else if (n.role === "YOLOVCHI") {
    e.steps = walkerScript(n, persona, wk++);
    e.start_delay = range(0, 20);
  } else if (n.role === "HAMMOL") {
    e.steps = hammolScript(hk++);
    e.start_delay = range(0, 5);
  }
  out.npcs[n.label] = e;
}
// guruh: boshliq eng sekin a'zoga moslashadi (1.4), hamroh boshliqqa ergashadi (o'z animatsiyalari bilan)
for (const n of S.npcs) {
  if (n.role === "HAMROH" && n.leader && out.npcs[n.leader]) {
    const L = out.npcs[n.leader], H = out.npcs[n.label];
    if (H.speed && L.speed) L.speed = Math.min(L.speed, H.speed);
  }
}
fs.writeFileSync(path.join(__dirname, "npc_ssenariy.json"), JSON.stringify(out, null, 1));
const c = {};
for (const e of Object.values(out.npcs)) c[e.persona] = (c[e.persona] || 0) + 1;
console.log("yozildi:", Object.keys(out.npcs).length, "NPC", c);
console.log("rasta ishlatilishi (0 = hech kim bormaydi):", [...usage.values()].filter((x) => x === 0).length, "ta bo'sh, max", Math.max(...usage.values()));
