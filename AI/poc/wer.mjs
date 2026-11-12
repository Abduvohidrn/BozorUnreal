// So'z xatosi (WER): ikkala matn kichik harf, tinish belgilarsiz, apostroflar bir xil qilinadi
import fs from "node:fs";
const norm = s => s.toLowerCase().replace(/[ʻʼ‘’`´]/g, "'").replace(/[^\p{L}\p{N}' ]/gu, " ").replace(/\s+/g, " ").trim().split(" ");
function ed(a, b) { const d = Array.from({ length: a.length + 1 }, (_, i) => [i, ...Array(b.length).fill(0)]); for (let j = 1; j <= b.length; j++) d[0][j] = j;
  for (let i = 1; i <= a.length; i++) for (let j = 1; j <= b.length; j++) d[i][j] = Math.min(d[i-1][j] + 1, d[i][j-1] + 1, d[i-1][j-1] + (a[i-1] === b[j-1] ? 0 : 1)); return d[a.length][b.length]; }
const r = JSON.parse(fs.readFileSync(process.argv[2], "utf8").replace(/^﻿/, ""));
let e = 0, n = 0, aud = 0, stt = 0;
for (const x of r) { const a = norm(x.matn), b = norm(x.chiqdi); const k = ed(a, b); e += k; n += a.length; aud += x.sekund; stt += +x.stt_s;
  console.log(`${k ? "✗" : "✓"} ${x.chiqdi}${k ? `   [asli: ${x.matn}]` : ""}`); }
console.log(`\nWER ${(100 * e / n).toFixed(1)}% (${e}/${n} so'z) | ${aud.toFixed(1)}s audio -> ${stt.toFixed(2)}s STT (${(aud / stt).toFixed(0)}x realtime)`);
