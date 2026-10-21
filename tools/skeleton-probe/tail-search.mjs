// 只读：从已知锚点（bones 组结束 = 1008）向前穷举尾段布局组合，
// 唯一判据：是否**精确落在 EOF**。用于确定 deform / drawOrder / event 三段的结构。
import fs from 'node:fs';
const ROOT = 'D:/Game/Other/sdorica/working/full_export/assets/game/character/spinedata';
const [rel, startArg] = process.argv.slice(2);
const buf = fs.readFileSync(`${ROOT}/${rel}`);
const START = Number(startArg);

// 帧格式：返回 [新位置, 是否可解析]；null 表示失败
function deformFrame(p, mode) {
  if (p + 4 > buf.length) return null;
  const time = buf.readFloatBE(p);
  if (!isFinite(time) || time < 0 || time > 1000) return null;
  p += 4;
  if (mode === 'vc') {
    const vc = buf.readUInt8(p);
    if (vc & 0x80) return null;
    p += 1;
    if (p + vc * 4 > buf.length) return null;
    for (let k = 0; k < vc; k++) { if (!isFinite(buf.readFloatBE(p + k * 4))) return null; }
    p += vc * 4;
  }
  return { p, time };
}
function curveByte(p) {
  const t = buf.readUInt8(p);
  if (t === 2) return p + 1 + 16;
  if (t === 0 || t === 1) return p + 1;
  return null;
}

const results = [];
// deform 段形状
for (const shape of ['nameSkin', 'indexSkin']) {
  for (const fmode of ['vc', 'none']) {
    let p = START;
    const dc = buf.readUInt8(p);
    if (dc & 0x80) continue;
    p += 1;
    let ok = true;
    const trace = [];
    for (let s = 0; s < dc && ok; s++) {
      if (shape === 'nameSkin') {
        const n = buf.readUInt8(p); p += 1;
        if (n === 0) { p += 0; } else { p += n - 1; }
        trace.push(`skinName@${p}`);
      } else {
        const idx = buf.readUInt8(p); p += 1;
        trace.push(`skinIndex=${idx}`);
      }
      const sc = buf.readUInt8(p); p += 1;
      if (sc & 0x80) { ok = false; break; }
      for (let j = 0; j < sc && ok; j++) {
        if (shape === 'indexSkin') { const si = buf.readUInt8(p); p += 1; }
        const ac = buf.readUInt8(p); p += 1;
        if (ac & 0x80) { ok = false; break; }
        for (let a = 0; a < ac && ok; a++) {
          const n = buf.readUInt8(p); p += 1;
          if (n === 0) { ok = false; break; }
          const name = buf.toString('latin1', p, p + n - 1); p += n - 1;
          const fc = buf.readUInt8(p); p += 1;
          if (fc & 0x80) { ok = false; break; }
          for (let f = 0; f < fc && ok; f++) {
            const fr = deformFrame(p, fmode);
            if (!fr) { ok = false; break; }
            p = fr.p;
            if (f + 1 < fc) { const c = curveByte(p); if (c === null) { ok = false; break; } p = c; }
          }
        }
      }
    }
    if (!ok) continue;
    // 之后：drawOrder / event
    if (p >= buf.length) continue;
    for (const doMode of ['fc', 'direct']) {
     try {
      let q = p;
      const drc = buf.readUInt8(q); q += 1;
      if (drc & 0x80) continue;
      let ok2 = true;
      for (let k = 0; k < drc && ok2; k++) {
        let fc = 1;
        if (doMode === 'fc') { fc = buf.readUInt8(q); q += 1; if (fc & 0x80) { ok2 = false; break; } }
        for (let f = 0; f < fc && ok2; f++) {
          if (q + 4 > buf.length) { ok2 = false; break; }
          const t = buf.readFloatBE(q);
          if (!isFinite(t) || t < 0 || t > 1000) { ok2 = false; break; }
          q += 4;
          const oc = buf.readUInt8(q); q += 1;
          if (oc & 0x80) { ok2 = false; break; }
          for (let x = 0; x < oc; x++) { const a = buf.readUInt8(q); q += 1; if (a & 0x80) { ok2 = false; break; } const b = buf.readUInt8(q); q += 1; if (b & 0x80) { ok2 = false; break; } }
        }
      }
      if (!ok2) continue;
      // event
      let r = q;
      if (r >= buf.length) { results.push(`    越界：shape=${shape} frame=${fmode} drawOrder=${doMode} q=${q}`); continue; }
      const ec = buf.readUInt8(r); r += 1;
      if (ec & 0x80) continue;
      if (ec !== 0) continue;
      if (r === buf.length) {
        results.push(`*** 精确 EOF：shape=${shape} frame=${fmode} drawOrder=${doMode} deformEnd=${p} doEnd=${q} final=${r} ***`);
      } else {
        results.push(`    差 ${buf.length - r} 字节：shape=${shape} frame=${fmode} drawOrder=${doMode} final=${r}`);
      }
     } catch (e) { /* 越界/不可解析，跳过该组合 */ }
    }
  }
}
console.log('===', rel, 'size', buf.length, 'start', START);
for (const r of results) console.log(r);
if (!results.length) console.log('无可行组合');
