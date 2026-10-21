// 只读：把一段字节按 uint16 大端打印，用于找出三角索引数组的边界。
import fs from 'node:fs';
const ROOT = 'D:/Game/Other/sdorica/working/full_export/assets/game/character/spinedata';
const [rel, fromArg, toArg] = process.argv.slice(2);
const buf = fs.readFileSync(`${ROOT}/${rel}`);
const from = Number(fromArg), to = Number(toArg);
let line = '';
const flush = () => { if (line) { console.log(line); line = ''; } };
for (let o = from; o + 1 < Math.min(buf.length, to); o += 2) {
  const v = buf.readUInt16BE(o);
  const mark = v <= 39 ? '' : '<-- 越界';
  if (mark) { flush(); }
  line += String(o).padStart(6) + ':' + String(v).padStart(4) + ' ' + mark + '   ';
  if (line.length > 90) flush();
}
flush();
