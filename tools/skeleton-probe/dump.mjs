// 只读：按给定偏移 dump 语料字节，用于人工核对。
import fs from 'node:fs';
const ROOT = 'D:/Game/Other/sdorica/working/full_export/assets/game/character/spinedata';
const [rel, fromArg, lenArg] = process.argv.slice(2);
const buf = fs.readFileSync(`${ROOT}/${rel}`);
const from = Number(fromArg);
const len = Number(lenArg ?? 256);
console.log('===', rel, 'size', buf.length, 'from', from);
for (let o = from; o < Math.min(buf.length, from + len); o += 16) {
  let h = '';
  for (let j = o; j < Math.min(o + 16, buf.length); j++) h += buf.toString('hex', j, j + 1).padStart(2, '0') + ' ';
  let a = '';
  for (let j = o; j < Math.min(o + 16, buf.length); j++) { const c = buf[j]; a += (c >= 32 && c < 127) ? String.fromCharCode(c) : '.'; }
  console.log(String(o).padStart(6), ' ', h.padEnd(48), '|' + a + '|');
}
