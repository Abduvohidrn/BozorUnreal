// 0-bosqich: miya (LLM) sinovi. llama-server ga bola gaplarini yuborib, JSON tushuncha va vaqtni o'lchaydi.
// Ishlatish: node miya_sinov.mjs [port]  (llama-server oldindan ishga tushirilgan bo'lishi kerak)
import fs from "node:fs";

const PORT = process.argv[2] || "8091";
const URL = process.env.MIYA_URL || `http://127.0.0.1:${PORT}/v1/chat/completions`;

// Bitta rasta holati (o'yindagi FBozorSuhbatHolati ning soddalashgani)
const holat = {
  mollar: ["pomidor", "bodring", "kartoshka"],
  tanlangan: "pomidor", narx: 12000, birlik: "kilo", fel: "Mehribon", bosqich: "NarxAytildi",
};

const TIZIM = `Sen o'zbek bozoridagi sotuvchisan. Xaridor bola gapiradi.
Vazifang: bola gapidan niyatni aniqlab, JSON qaytar va sotuvchi sifatida qisqa (1 gap) o'zbekcha javob yoz.
Narxni o'zing o'ylab topma, faqat berilgan narxni ishlat.
harakat qiymatlari: Salom, NarxAytish, Savdolashish, Tortish, PulQabul, Xayr, MolTanlash, Tushunmadi.
qiymat: Tortish uchun miqdor (kilo), PulQabul uchun so'm, aks holda 0.
odob: 1 odobli, 0 oddiy, -1 qo'pol.
Rasta: ${JSON.stringify(holat)}`;

const SXEMA = {
  type: "object",
  properties: {
    harakat: { type: "string", enum: ["Salom", "NarxAytish", "Savdolashish", "Tortish", "PulQabul", "Xayr", "MolTanlash", "Tushunmadi"] },
    mol: { type: "string", enum: ["", ...holat.mollar] },
    qiymat: { type: "number" },
    odob: { type: "integer", enum: [-1, 0, 1] },
    javob: { type: "string" },
  },
  required: ["harakat", "mol", "qiymat", "odob", "javob"],
};

// [bola gapi, kutilgan harakat]
const GAPLAR = [
  ["Assalomu alaykum, amaki!", "Salom"],
  ["Pomidor necha pul?", "NarxAytish"],
  ["Pomidorning kilosi qancha turadi?", "NarxAytish"],
  ["Qimmat ekan, ozgina arzonroq bering", "Savdolashish"],
  ["Iltimos, o'n ming qilib bering", "Savdolashish"],
  ["Ikki kilo torting", "Tortish"],
  ["Menga uch kilo kartoshka kerak", "Tortish"],
  ["Bir yarim kilo bodring bering", "Tortish"],
  ["Mana, ellik ming", "PulQabul"],
  ["Mana yigirma ming so'm", "PulQabul"],
  ["Rahmat, xayr!", "Xayr"],
  ["Bodring bormi?", "MolTanlash"],
  ["Hoy, tez bo'l, pomidor ber", "Tortish"],
  ["Narxing juda qimmat-ku, insof qil", "Savdolashish"],
  ["Ob-havo qanaqa bugun?", "Tushunmadi"],
  ["salom pomidor qancha", "NarxAytish"],
  ["ikki kilo pamidor", "Tortish"],
  ["kechirasiz arzonroq bo'lmaydimi", "Savdolashish"],
];

async function sora(gap) {
  const t0 = performance.now();
  const r = await fetch(URL, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: process.env.MIYA_MODEL || "x",
      messages: [{ role: "system", content: TIZIM }, { role: "user", content: gap }],
      temperature: 0.2, max_tokens: 120, cache_prompt: true,
      chat_template_kwargs: { enable_thinking: false },
      response_format: { type: "json_schema", json_schema: { name: "tushuncha", schema: SXEMA } },
    }),
  });
  const j = await r.json();
  const ms = performance.now() - t0;
  const matn = j.choices?.[0]?.message?.content ?? JSON.stringify(j);
  let obj = null; try { obj = JSON.parse(matn); } catch {}
  return { ms, obj, matn, t: j.timings };
}

const natija = [];
let togri = 0, jsonOk = 0;
for (const [gap, kutil] of GAPLAR) {
  const { ms, obj, matn, t } = await sora(gap);
  if (obj) jsonOk++;
  const ok = obj?.harakat === kutil;
  if (ok) togri++;
  natija.push({ gap, kutil, ms: Math.round(ms), obj, matn: obj ? undefined : matn, timings: t });
  console.log(`${ok ? "✓" : "✗"} ${Math.round(ms)}ms | ${gap} → ${obj ? `${obj.harakat}/${obj.mol}/${obj.qiymat}/${obj.odob} | "${obj.javob}"` : matn.slice(0, 120)}`
    + (t ? ` [prompt ${t.prompt_n}t ${Math.round(t.prompt_per_second)}t/s, gen ${t.predicted_n}t ${Math.round(t.predicted_per_second)}t/s]` : ""));
}
const vaqtlar = natija.slice(1).map(n => n.ms).sort((a, b) => a - b); // birinchisi kesh isitish
console.log(`\nNiyat to'g'ri: ${togri}/${GAPLAR.length}, JSON to'g'ri: ${jsonOk}/${GAPLAR.length}, median ${vaqtlar[vaqtlar.length >> 1]}ms, eng sekin ${vaqtlar.at(-1)}ms`);
fs.mkdirSync("natija", { recursive: true });
fs.writeFileSync(`natija/miya_${process.argv[3] || "model"}.json`, JSON.stringify(natija, null, 1));
