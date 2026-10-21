// B4 拓展：mesh 附件尾部的独立字节走查。
// 只读真实语料，逐步解码并打点偏移，用于确认/证伪 mesh 尾部布局。
import fs from 'node:fs';

const ROOT = 'D:/Game/Other/sdorica/working/full_export/assets/game/character/spinedata';
const rel = process.argv[2];
const buf = fs.readFileSync(`${ROOT}/${rel}`);
const log = (...a) => console.log(...a);

let p = 0;
const need = (n) => { if (p + n > buf.length) throw new Error(`TRUNCATED at ${p} need ${n} size ${buf.length}`); };
const u8 = () => { need(1); return buf.readUInt8(p++); };
const f32 = () => { need(4); const v = buf.readFloatBE(p); p += 4; return v; };
const i32 = () => { need(4); const v = buf.readInt32BE(p); p += 4; return v; };
const u16 = () => { need(2); const v = buf.readUInt16BE(p); p += 2; return v; };
const varint = () => {
  let result = 0;
  for (let i = 0; i < 5; i++) {
    need(1);
    const b = buf.readUInt8(p++);
    result |= (b & 0x7f) << (7 * i);
    if ((b & 0x80) === 0) return result >>> 0;
  }
  throw new Error(`VARINT_OVERFLOW at ${p}`);
};
const str = () => {
  const n = varint() - 1;
  if (n === -1) return null;
  need(n);
  const s = buf.toString('latin1', p, p + n);
  p += n;
  return s;
};
const at = (label) => `${String(p).padStart(5)}  ${label}`;

log('===', rel, 'size', buf.length);
log(at('hash'), str());
log(at('version'), str());
log(at('width'), f32());
log(at('height'), f32());
const nonessential = u8();
log(at('nonessential'), nonessential);
if (nonessential) log(at('imagesPath'), str());

const boneCount = varint();
log(at('boneCount'), boneCount);
for (let i = 0; i < boneCount; i++) {
  const o = p;
  const name = str();
  const parent = i === 0 ? -1 : varint() - 1;
  for (let k = 0; k < 8; k++) f32();
  const isc = u8(), ir = u8();
  log(at(`bone ${i}`), name, 'parent', parent, 'inherit', isc, ir, '@', o);
}
const ik = varint(); log(at('ikCount'), ik);
const tc = varint(); log(at('transformCount'), tc);
const slotCount = varint();
log(at('slotCount'), slotCount);
for (let i = 0; i < slotCount; i++) {
  const o = p;
  const name = str();
  const boneIndex = varint();
  const color = i32();
  const att = str();
  const blend = u8();
  log(at(`slot ${i}`), name, 'bone', boneIndex, 'color', color.toString(16), 'att', att, 'blend', blend, '@', o);
}

log(at('--- default skin ---'));
const skinSlotCount = varint();
log(at('skinSlotCount'), skinSlotCount);
for (let i = 0; i < skinSlotCount; i++) {
  const slotIndex = varint();
  const attCount = varint();
  log(at(`skin slot ${i}`), 'slotIndex', slotIndex, 'attCount', attCount);
  for (let j = 0; j < attCount; j++) {
    const o = p;
    const key = str();
    const name = str();
    const type = u8();
    log(at(`  att ${j}`), 'key', key, 'name', name, 'type', type, '@', o);
    if (type !== 0) {
      log('>>> 非 region（type=' + type + '），在此停下做人工核对');
      printTail(o);
      process.exit(0);
    }
    log(at('    path'), str());
    for (const f of ['rot', 'x', 'y', 'sx', 'sy', 'w', 'h']) log(at(`    ${f}`), f32());
    log(at('    color'), i32().toString(16));
  }
}

log(at('extraSkinCount'), varint());
log(at('eventCount'), varint());
const animCount = varint();
log(at('animCount'), animCount);
for (let i = 0; i < animCount; i++) log(at(`anim ${i} name`), str());
log('--- 到此偏移', p, '剩余', buf.length - p);

function printTail(from) {
  log('');
  log('=== 附件起点', from, '起的原始字节 ===');
  for (let o = from; o < Math.min(buf.length, from + 400); o += 16) {
    let h = '';
    for (let j = o; j < Math.min(o + 16, buf.length); j++) h += buf.toString('hex', j, j + 1).padStart(2, '0') + ' ';
    let a = '';
    for (let j = o; j < Math.min(o + 16, buf.length); j++) { const c = buf[j]; a += (c >= 32 && c < 127) ? String.fromCharCode(c) : '.'; }
    log(String(o).padStart(5), ' ', h.padEnd(48), '|' + a + '|');
  }
}
