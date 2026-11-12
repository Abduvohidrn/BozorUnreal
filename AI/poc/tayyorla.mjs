// Mikrofon yozuvini STT uchun tayyorlash tajribasi: jimlikni kesish + normallashtirish (o'yindagi C++ bilan bir xil algoritm)
import fs from "node:fs";
const [inp, out] = process.argv.slice(2);
const b = fs.readFileSync(inp); const n = (b.length - 44) / 2;
const s = new Float32Array(n); for (let i = 0; i < n; i++) s[i] = b.readInt16LE(44 + i * 2) / 32768;
const W = 480; // 30 ms
const rms = []; for (let i = 0; i < n; i += W) { let e = 0; for (let j = i; j < Math.min(n, i + W); j++) e += s[j] * s[j]; rms.push(Math.sqrt(e / W)); }
const sorted = [...rms].sort((a, b) => a - b); const floor = sorted[Math.floor(sorted.length * 0.2)]; const top = sorted[sorted.length - 1];
const thr = Math.max(floor * 4, top * 0.03, 0.002);
let a = rms.findIndex(r => r > thr), z = rms.length - 1 - [...rms].reverse().findIndex(r => r > thr);
const pad = Math.round(0.3 * 16000 / W); a = Math.max(0, a - pad); z = Math.min(rms.length - 1, z + pad);
const seg = s.slice(a * W, (z + 1) * W);
let pk = 0; for (const v of seg) pk = Math.max(pk, Math.abs(v)); const g = Math.min(0.9 / Math.max(pk, 1e-4), 30);
const o = Buffer.alloc(44 + seg.length * 2); b.copy(o, 0, 0, 44); o.writeUInt32LE(36 + seg.length * 2, 4); o.writeUInt32LE(seg.length * 2, 40);
for (let i = 0; i < seg.length; i++) o.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(seg[i] * g * 32767))), 44 + i * 2);
fs.writeFileSync(out, o);
console.log(`${inp}: ${(n / 16000).toFixed(2)}s -> ${(seg.length / 16000).toFixed(2)}s, kuch x${g.toFixed(1)}, chegara ${thr.toFixed(4)} (shovqin ${floor.toFixed(4)})`);
