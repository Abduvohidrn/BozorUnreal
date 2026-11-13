// LLM sotuvchi javobini NPC fe'li bilan qayta yozadi: sifat va tezlik sinovi.
// Ishlatish: node javob_sinov.mjs <port> <nom>   (llama-server oldindan ishga tushirilgan bo'lishi kerak)
// Qoida: narx/summa/qaytimni o'yin hisoblaydi. LLM faqat tayyor gapning ma'nosini o'z uslubida aytadi.
// Tayyor gapdagi har bir son LLM javobida aynan bo'lmasa - javob rad etiladi (o'yinda tayyor gap ishlatiladi).
import fs from "node:fs";

const PORT = process.argv[2] || "8091";
const NOM = process.argv[3] || "model";
const URL = `http://127.0.0.1:${PORT}/v1/chat/completions`;

// Fe'l tavsifi (sotuv.json fe'llariga mos)
// Fe'l: tavsif + namuna (ma'no -> shu fe'ldagi gap)
const FEL = {
  Mehribon: ["mehribon, xushmuomala, bolalarni yaxshi ko'radi, 'bolam', 'o'g'lim', 'aylanay' deydi", [
    ["Bir kilo bodring 9 000 so'm.", "Bodringning kilosi 9 000 so'm, bolam. Hozirgina uzilgan, yangi-yangi."],
    ["Narxi shu.", "Aylanay, bundan tushirolmayman, o'zimga ham qimmatga tushgan."],
  ]],
  Qattiq: ["qattiqqo'l, kam gapiradi, qisqa va keskin javob beradi, ortiqcha gapni yoqtirmaydi", [
    ["Bir kilo bodring 9 000 so'm.", "9 000. Kilosi."],
    ["Narxi shu.", "Narxi shu. Olsangiz oling."],
  ]],
  Shoshqaloq: ["shoshqaloq, doim ishi ko'p, tez gapiradi, 'qani', 'tezroq', 'xo'p-xo'p' deydi", [
    ["Bir kilo bodring 9 000 so'm.", "Bodring 9 000 so'm, qani, nechta kilo? Tezroq ayting!"],
    ["Narxi shu.", "Xo'p-xo'p, narxi shu, vaqtim yo'q, olasizmi?"],
  ]],
  Ayyor: ["ayyor, molini haddan ortiq maqtaydi, shirin so'z, 'asal mol', 'butun bozorda yo'q' deydi", [
    ["Bir kilo bodring 9 000 so'm.", "Bunaqa bodring butun bozorda yo'q, asal! Kilosi atigi 9 000 so'm."],
    ["Narxi shu.", "Shu sifatga shu narx - tekin degan gap, bolam!"],
  ]],
};

export function tizim(npc) {
  const [tavsif, namuna] = FEL[npc.fel];
  return `Sen o'zbek bozoridagi sotuvchisan. Isming ${npc.ism}, ${npc.yosh} yoshdasan, ${npc.jins}.
Fe'ling: ${tavsif}.
Xaridor - 10-13 yoshli bola. Senga "ma'no" beriladi: shu gapning BUTUN ma'nosini o'z fe'lingga mos, jonli o'zbek tilida (lotin yozuvida) qayta ayt. So'zma-so'z takrorlama.
Qoidalar:
- Faqat 1-2 qisqa gap. Faqat sotuvchining gapi, izohsiz, qo'shtirnoqsiz. O'z ismingni aytma.
- Ma'nodagi barcha sonlarni AYNAN shunday yoz (masalan "12 000 so'm"), yangi son, narx yoki chegirma qo'shma.
- Ma'nodagi hech bir qismni tashlab ketma, ma'noda yo'q va'da berma.
Namunalar (sening uslubing):
${namuna.map(([m, j]) => `Ma'no: ${m}\nSen: ${j}`).join("\n")}`;
}

export function savol(v) {
  return `Kayfiyating (bolaga munosabat): ${v.munosabat >= 70 ? "iliq" : v.munosabat < 30 ? "sovuq, jahling chiqqan" : "oddiy"}.
Bola: "${v.bola}"
Ma'no: ${v.mano}`;
}

const NPClar = {
  a: { ism: "Malika opa", yosh: 45, jins: "ayol", fel: "Mehribon" },
  b: { ism: "Rustam aka", yosh: 50, jins: "erkak", fel: "Qattiq" },
  c: { ism: "Dilshod aka", yosh: 32, jins: "erkak", fel: "Shoshqaloq" },
  d: { ism: "Gulnora opa", yosh: 40, jins: "ayol", fel: "Ayyor" },
};

// [npc, bola gapi, tayyor gap (ma'no), munosabat]
const HOLATLAR = [
  ["a", "Assalomu alaykum!", "Va alaykum assalom, bolam! Keling, nima kerak? Bizda pomidor, bodring bor.", 60],
  ["b", "Assalomu alaykum, amaki", "Va alaykum assalom. Nima kerak? Bizda kartoshka, piyoz bor.", 50],
  ["c", "pomidor necha pul", "Bir kilo pomidor 12 000 so'm.", 50],
  ["d", "pomidor necha pul", "Bir kilo pomidor 12 000 so'm.", 50],
  ["a", "iltimos ozgina arzonroq bering", "Mayli, siz uchun 11 000 so'm qilaman.", 75],
  ["b", "arzonroq bering", "Narxi shu, juda yaxshi mol. 7 500 so'm.", 45],
  ["d", "qimmat ekan, o'n mingdan bering", "Bundan arzoni yo'q. 11 500 so'm.", 50],
  ["c", "ikki kilo torting", "Mana, 2 kilo pomidor. Jami 24 000 so'm bo'ldi.", 50],
  ["a", "mana ellik ming", "Rahmat! Mana qaytimingiz: 20 000, 5 000 va 1 000 so'm.", 60],
  ["b", "mana yigirma ming", "Bu yetmaydi. 24 000 so'm kerak, yana 4 000 so'm bering.", 40],
  ["c", "qaytim kam berdingiz", "Kechirasiz, adashibman! Mana yana 2 000 so'm.", 50],
  ["d", "men bir kilo so'ragan edim", "Rostdan ham, uzr. To'g'riladim: 1 kilo - 11 000 so'm.", 50],
  ["b", "hoy tez bo'l pomidor ber", "Kattalarga odob bilan gapirish kerak. Narxi shu.", 20],
  ["c", "hoy chol tez bo'l", "Bo'ldi, men sizga sotmayman. Boshqa rastaga boring.", 5],
  ["a", "rahmat xayr", "Rahmat, bolam! Ota-onangizga salom ayting!", 80],
  ["d", "ob-havo qanaqa bugun", "Tushunmadim. Pomidor olasizmi?", 50],
];

const sonlar = (s) => (s.match(/\d[\d ]*\d|\d/g) || []).map((x) => x.replace(/ /g, ""));

async function sora(npc, v) {
  const t0 = performance.now();
  const r = await fetch(URL, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      messages: [{ role: "system", content: tizim(npc) }, { role: "user", content: savol(v) }],
      temperature: 0.7, max_tokens: 80, cache_prompt: true,
      chat_template_kwargs: { enable_thinking: false },
    }),
  });
  const j = await r.json();
  return { ms: performance.now() - t0, matn: (j.choices?.[0]?.message?.content ?? JSON.stringify(j)).trim(), t: j.timings };
}

const natija = [];
let ok = 0;
for (const [k, bola, mano, munosabat] of HOLATLAR) {
  const npc = NPClar[k];
  const { ms, matn, t } = await sora(npc, { bola, mano, munosabat });
  const kerak = sonlar(mano), bor = sonlar(matn);
  const sonOk = kerak.every((s) => bor.includes(s)) && bor.every((s) => kerak.includes(s));
  if (sonOk) ok++;
  natija.push({ npc: npc.ism, fel: npc.fel, bola, mano, matn, sonOk, ms: Math.round(ms) });
  console.log(`${sonOk ? "✓" : "✗"} ${Math.round(ms)}ms [${npc.fel}] ${bola}\n   ma'no: ${mano}\n   LLM:   ${matn}` +
    (t ? `  [gen ${t.predicted_n}t ${Math.round(t.predicted_per_second)}t/s]` : ""));
}
const v = natija.slice(1).map((n) => n.ms).sort((a, b) => a - b);
console.log(`\nSonlar to'g'ri: ${ok}/${HOLATLAR.length}, median ${v[v.length >> 1]}ms, eng sekin ${v.at(-1)}ms`);
fs.mkdirSync("natija", { recursive: true });
fs.writeFileSync(`natija/javob_${NOM}.json`, JSON.stringify(natija, null, 1));
