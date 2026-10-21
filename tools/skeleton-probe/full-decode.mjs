// 只读：完整解码一份 .skel.bytes 并逐字段打点偏移，用于字节走查。
// deform 段的帧布局可切换假设（DEF=n），用「是否精确落在 EOF」判定。
import fs from 'node:fs';
const ROOT = 'D:/Game/Other/sdorica/working/full_export/assets/game/character/spinedata';
const rel = process.argv[2];
const DEF = Number(process.argv[3] ?? 0); // 0=现实现 time+vc+verts
const buf = fs.readFileSync(`${ROOT}/${rel}`);
const log = (...a) => console.log(...a);

let p = 0;
const need = (n) => { if (p + n > buf.length) throw new Error(`TRUNCATED at ${p} need ${n} size ${buf.length}`); };
const u8 = () => { need(1); return buf.readUInt8(p++); };
const f32 = () => { need(4); const v = buf.readFloatBE(p); p += 4; return v; };
const i32 = () => { need(4); const v = buf.readInt32BE(p); p += 4; return v; };
const varint = () => {
  let r = 0;
  for (let i = 0; i < 5; i++) {
    need(1);
    const b = buf.readUInt8(p++);
    r |= (b & 0x7f) << (7 * i);
    if ((b & 0x80) === 0) return r >>> 0;
  }
  throw new Error(`VARINT_OVERFLOW at ${p}`);
};
const str = () => { const n = varint() - 1; if (n === -1) return null; need(n); const s = buf.toString('latin1', p, p + n); p += n; return s; };
const at = (l) => `${String(p).padStart(5)}  ${l}`;

const hasCurve = (k) => [0, 1, 2, 3, 5, 6, 7, 9].includes(k);
const curve = () => {
  const o = p, t = u8();
  if (t === 2) { f32(); f32(); f32(); f32(); }
  else if (t !== 0 && t !== 1) throw new Error(`BAD_CURVE ${t} at ${o}`);
  return t;
};

function timeline(label) {
  const o = p;
  const kind = u8();
  const fc = varint();
  log(`${String(o).padStart(5)}  ${label} type=${kind} frames=${fc}`);
  for (let f = 0; f < fc; f++) {
    const fo = p;
    const time = f32();
    if (kind === 0) f32();
    else if (kind === 1 || kind === 2 || kind === 3) { f32(); f32(); }
    else if (kind === 4) str();
    else if (kind === 5) i32();
    else if (kind === 6) {
      if (DEF === 0) { const vc = varint(); for (let k = 0; k < vc; k++) f32(); }
      else if (DEF === 1) { const end = varint(); const start = f === 0 ? 0 : varint(); for (let k = 0; k < end - start; k++) f32(); }
      else if (DEF === 2) { /* 无顶点 */ }
    } else if (kind === 7) { f32(); u8(); }
    else if (kind === 9) { f32(); f32(); f32(); f32(); }
    else throw new Error(`BAD_TIMELINE_TYPE ${kind} at ${o}`);
    let c = '-';
    if (hasCurve(kind) && f + 1 < fc) c = curve();
    log(`${String(fo).padStart(7)}    f${f} t=${time.toFixed(4)} curve=${c}`);
  }
}
function group(label) {
  const o = p;
  const n = varint();
  log(`${String(o).padStart(5)}  ${label} = ${n}`);
  for (let i = 0; i < n; i++) {
    const io = p;
    const idx = varint();
    const tlc = varint();
    log(`${String(io).padStart(5)}  ${label}[${i}] idx=${idx} tl=${tlc}`);
    for (let t = 0; t < tlc; t++) timeline(`${label}[${i}].tl${t}`);
  }
}

log('===', rel, 'size', buf.length, 'DEF=' + DEF);
try {
  log(at('hash'), str());
  log(at('version'), str());
  log(at('w'), f32()); log(at('h'), f32());
  log(at('nonessential'), u8());
  const bn = varint(); log(at('boneCount'), bn);
  for (let i = 0; i < bn; i++) { str(); if (i) varint(); for (let k = 0; k < 8; k++) f32(); u8(); u8(); }
  log(at('boneDataEnd'));
  log(at('ikCount'), varint());
  log(at('transformCount'), varint());
  const sn = varint(); log(at('slotCount'), sn);
  for (let i = 0; i < sn; i++) { str(); varint(); i32(); str(); u8(); }
  const ss = varint(); log(at('skinSlotCount'), ss);
  for (let i = 0; i < ss; i++) {
    const si = varint(), ac = varint();
    log(at(`skinSlot ${si} att=${ac}`));
    for (let j = 0; j < ac; j++) {
      const o = p;
      const key = str(); const name = str(); const type = u8();
      log(`${String(o).padStart(5)}  att key=${key} name=${name} type=${type}`);
      if (type === 0) { str(); for (let k = 0; k < 7; k++) f32(); i32(); }
      else {
        str(); i32();
        const vc = varint();
        for (let k = 0; k < vc; k++) { f32(); f32(); }
        const tc = varint();
        p += tc * 2;
        for (let k = 0; k < vc; k++) { f32(); f32(); }
        log(at('  meshTailCount'), varint());
      }
    }
  }
  log(at('extraSkinCount'), varint());
  log(at('eventCount'), varint());
  const an = varint();
  log(at('animCount'), an);
  for (let i = 0; i < an; i++) {
    log(at(`anim ${i} name`), str());
    group('slots');
    group('bones');
    group('ik');
    group('transform');
    // deform
    const dof = p;
    const dc = varint();
    log(`${String(dof).padStart(5)}  deform = ${dc}`);
    for (let s = 0; s < dc; s++) {
      const six = varint();
      log(`      deform[${s}].skinIndex=${six}`);
      const sc = varint();
      log(`      deform[${s}].slotCount=${sc}`);
      for (let j = 0; j < sc; j++) {
        const idx = varint();
        log(`      deform[${s}].slot${j}.slotIndex=${idx}`);
        const ac = varint();
        log(`      deform[${s}].slot${j}.attCount=${ac}`);
        for (let a = 0; a < ac; a++) {
          const nm = str();
          const fc = varint();
          log(`      deform[${s}].slot${j}.att${a} name=${nm} frames=${fc}`);
          for (let f = 0; f < fc; f++) {
            const fo = p;
            const time = f32();
            const vc = varint();
            let info = `vc=${vc}`;
            if (DEF === 3) {           // vc 是「顶点对数」，浮点数是 2×vc
              for (let k = 0; k < vc * 2; k++) f32();
              info += ` doubles=${vc * 2}`;
            } else {
              for (let k = 0; k < vc; k++) f32();
            }
            let c = '-';
            if (f + 1 < fc) c = curve();
            log(`${String(fo).padStart(7)}      f${f} t=${time.toFixed(4)} ${info} curve=${c}`);
          }
        }
      }
    }
    const dro = p, drc = varint();
    log(`${String(dro).padStart(5)}  drawOrder = ${drc}`);
    for (let k = 0; k < drc; k++) {
      const fc = varint();
      log(at(` do${k}.frames`), fc);
      for (let f = 0; f < fc; f++) {
        const fo = p; const time = f32();
        const oc = varint();
        for (let q = 0; q < oc; q++) { varint(); varint(); }
        log(`${String(fo).padStart(7)}    f${f} t=${time.toFixed(4)} offsets=${oc}`);
      }
    }
    const eo = p, ec = varint();
    log(`${String(eo).padStart(5)}  event = ${ec}`);
    for (let k = 0; k < ec; k++) {
      const fc = varint();
      for (let f = 0; f < fc; f++) {
        const fo = p; const time = f32();
        str(); varint(); f32(); str();
        log(`${String(fo).padStart(7)}    ev${k}.f${f} t=${time.toFixed(4)}`);
      }
    }
  }
  log('--- 结束偏移', p, p === buf.length ? '*** 精确 EOF ***' : `剩余 ${buf.length - p}`);
} catch (e) {
  log('!!! 失败', e.message, '已读到', p, '/', buf.length);
}
