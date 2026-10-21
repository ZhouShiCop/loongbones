// 只读：在给定偏移上试各种「顶点区布局」，看哪一种正好落在目标结束偏移。
//
// 语料根**不写死在源码里**（规范 项目规范.md §8.2）：由 $LOONGBONES_CORPUS_ROOT 提供。
import fs from 'node:fs';
const ROOT = process.env.LOONGBONES_CORPUS_ROOT;
if (!ROOT) {
  console.error('未设置 $LOONGBONES_CORPUS_ROOT；本探针不内置任何机器路径。');
  process.exit(2);
}
const [rel, startArg, nArg, targetArg] = process.argv.slice(2);
if (!rel || startArg === undefined || nArg === undefined || targetArg === undefined) {
  console.error('用法：node tools/skeleton-probe/vert-probe.mjs <相对路径> <起始偏移> <顶点数> <目标结束偏移>');
  process.exit(2);
}
const buf = fs.readFileSync(`${ROOT}/${rel}`);
const START = Number(startArg), N = Number(nArg), TARGET = Number(targetArg);
const vi = (o) => { let r = 0; for (let i = 0; i < 5; i++) { const x = buf[o++]; r |= (x & 0x7f) << (7 * i); if (!(x & 0x80)) return [r, o]; } return [null, o]; };
const f32 = (o) => buf.readFloatBE(o);

console.log('===', rel, 'start', START, 'vertexCount', N, '目标结束', TARGET);

// 1) 纯浮点 k 个/顶点
for (let k = 1; k <= 10; k++) {
  const end = START + N * k * 4;
  const hit = end === TARGET ? '  <<< 命中' : '';
  console.log(`  纯浮点 ${k}/顶点 -> ${end}${hit}`);
}

// 2) 加权：boneCount(varint) + per bone (boneIndex varint + x,y,w)
{
  let p = START, ok = true; const counts = [];
  for (let i = 0; i < N && ok; i++) {
    let bc; [bc, p] = vi(p);
    if (bc === null || bc === 0 || bc > 40) { ok = false; break; }
    counts.push(bc);
    for (let j = 0; j < bc; j++) { let bi; [bi, p] = vi(p); if (bi === null) { ok = false; break; } p += 12; }
  }
  console.log(`  加权(标准) -> ${p}  ok=${ok}  boneCounts=${counts.slice(0, 16).join(',')}${p === TARGET ? '  <<< 命中' : ''}`);
}

// 3) 加权但 boneIndex 用 u8
{
  let p = START, ok = true; const counts = [];
  for (let i = 0; i < N && ok; i++) {
    let bc; [bc, p] = vi(p);
    if (bc === null || bc === 0 || bc > 40) { ok = false; break; }
    counts.push(bc);
    for (let j = 0; j < bc; j++) { p += 1 + 12; }
  }
  console.log(`  加权(u8 骨索引) -> ${p}  ok=${ok}  boneCounts=${counts.slice(0, 16).join(',')}${p === TARGET ? '  <<< 命中' : ''}`);
}

// 4) 前 m 个浮点是顶点，其后逐顶点 boneCount(varint)+骨(varint+x,y,w)
for (let m = 2; m <= 8; m += 2) {
  let p = START + N * m * 4, ok = true; const counts = [];
  for (let i = 0; i < N && ok; i++) {
    let bc; [bc, p] = vi(p);
    if (bc === null || bc === 0 || bc > 40) { ok = false; break; }
    counts.push(bc);
    for (let j = 0; j < bc; j++) { let bi; [bi, p] = vi(p); if (bi === null) { ok = false; break; } p += 9; }
  }
  console.log(`  先 ${m} 浮点/顶点 再加权(骨=varint+x+y, 无 w) -> ${p} ok=${ok} bc=${counts.slice(0, 12).join(',')}${p === TARGET ? '  <<< 命中' : ''}`);
}

console.log('--- 目标附近字节 ---');
for (let o = TARGET - 24; o < TARGET + 16; o += 8) {
  let h = ''; for (let j = o; j < o + 8 && j < buf.length; j++) h += buf.toString('hex', j, j + 1).padStart(2, '0') + ' ';
  let a = ''; for (let j = o; j < o + 8 && j < buf.length; j++) { const c = buf[j]; a += (c >= 32 && c < 127) ? String.fromCharCode(c) : '.'; }
  console.log(String(o).padStart(6), ' ', h, '|' + a + '|', f32(o).toFixed(4));
}
