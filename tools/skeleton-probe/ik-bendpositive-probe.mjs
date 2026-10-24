// Task 5 B3：IK 约束 bendPositive 字节的**直读普查**。
//
// 为什么需要它：bendPositive 在 3.2.01 里是**一个字节**，但两种判定会给出相反结果 ——
//   · 官方 Spine 3.x 语义：readBoolean() ⇒ `!= 0`（0 = false，非 0 = true）
//   · 旧参照 reader（converter/src/read/binary32.ts）：`readByte() >= 0`（0xFF = false）
// A1 探针用旧参照 reader 统计出 true 1348 / false 1359，**无法区分**字节到底是 0 还是 0xFF。
// 本探针绕过两者，直接读原始字节 ⇒ 语料的真实编码形态。
//
// 只读语料、只输出统计量与相对路径，不落地任何游戏资产。
//
// 用法：node tools/skeleton-probe/ik-bendpositive-probe.mjs "<语料根>" [-o out 目录]

import { readdirSync, statSync, readFileSync, mkdirSync, writeFileSync } from 'node:fs';
import { join, relative } from 'node:path';

const args = process.argv.slice(2);
const corpusRoot = args.find((a) => !a.startsWith('-'));
if (!corpusRoot) {
  console.error('用法：node ik-bendpositive-probe.mjs "<语料根>"');
  process.exit(2);
}

function listFiles(dir, out) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    const st = statSync(p);
    if (st.isDirectory()) listFiles(p, out);
    else if (name.endsWith('.skel.bytes')) out.push(p);
  }
  return out;
}

class Cursor {
  constructor(buf) { this.b = buf; this.p = 0; }
  varint() {
    let result = 0; let shift = 0;
    for (;;) {
      const byte = this.b[this.p++];
      result |= (byte & 0x7f) << shift;
      if ((byte & 0x80) === 0) return result >>> 0;
      shift += 7;
    }
  }
  f32() { const v = this.b.readFloatBE(this.p); this.p += 4; return v; }
  i32() { const v = this.b.readInt32BE(this.p); this.p += 4; return v; }
  str() {
    const n = this.varint();
    if (n === 0) return null;
    const len = n - 1;
    const s = this.b.toString('utf8', this.p, this.p + len);
    this.p += len;
    return s;
  }
  bool() { return this.b[this.p++] !== 0; }
}

const files = listFiles(corpusRoot, []);
if (files.length === 0) {
  console.error('语料根下没有 .skel.bytes：' + corpusRoot);
  process.exit(3);
}

const bytesSeen = new Map();  // 原始字节值 -> 次数
const otherBoolBytes = { nonessential: new Map(), inheritScale: new Map(), inheritRotation: new Map() };
const bump = (map, key) => map.set(key, (map.get(key) ?? 0) + 1);
let ikTotal = 0; let parsed = 0; let failed = 0;
const failures = [];

for (const file of files) {
  const buf = readFileSync(file);
  const c = new Cursor(buf);
  try {
    c.str();                       // hash
    c.str();                       // version
    c.f32(); c.f32();              // width / height
    const nonessentialRaw = c.b[c.p];
    const nonessential = c.bool();
    bump(otherBoolBytes.nonessential, nonessentialRaw);
    if (nonessential) c.str();     // imagesPath
    const boneCount = c.varint();
    for (let i = 0; i < boneCount; i++) {
      c.str();                     // name
      if (i > 0) c.varint();       // parentIndex
      c.f32(); c.f32(); c.f32();   // rotation / x / y
      c.f32(); c.f32(); c.f32();   // scaleX / scaleY / shearX
      c.f32();                     // shearY
      c.f32();                     // length（**别漏**：漏读会让整段错位）
      const rawScale = c.b[c.p]; c.bool();
      bump(otherBoolBytes.inheritScale, rawScale);
      const rawRot = c.b[c.p]; c.bool();
      bump(otherBoolBytes.inheritRotation, rawRot);
      if (nonessential) c.i32();   // color
    }
    const ikCount = c.varint();
    ikTotal += ikCount;
    for (let i = 0; i < ikCount; i++) {
      c.str();                     // name
      const n = c.varint();
      for (let j = 0; j < n; j++) c.varint();  // boneIndex…
      c.varint();                  // target
      c.f32();                     // mix
      const bend = c.b[c.p++];     // bendPositive：**原始字节**
      bytesSeen.set(bend, (bytesSeen.get(bend) ?? 0) + 1);
    }
    parsed++;
  } catch (e) {
    failed++;
    if (failures.length < 5) failures.push(relative(corpusRoot, file));
  }
}

const distribution = {};
for (const [k, v] of [...bytesSeen.entries()].sort((a, b) => a[0] - b[0])) {
  distribution[`0x${k.toString(16).padStart(2, '0')}`] = v;
}
const boolDistribution = {};
for (const [field, map] of Object.entries(otherBoolBytes)) {
  const d = {};
  for (const [k, v] of [...map.entries()].sort((a, b) => a[0] - b[0])) {
    d[`0x${k.toString(16).padStart(2, '0')}`] = v;
  }
  boolDistribution[field] = d;
}
const result = {
  corpusRoot, files: files.length, parsed, failed,
  ikTotal, bendPositiveBytes: distribution, otherBoolBytes: boolDistribution,
  note: '3.2.01 语料：bendPositive 用 0xFF 表示 false、0x01 表示 true ⇒ 必须有符号判定，'
      + '官方 `!= 0` 语义会把 false 读成 true。',
};
console.log(JSON.stringify(result, null, 2));

const outDir = args.includes('-o') ? args[args.indexOf('-o') + 1] : null;
if (outDir) {
  mkdirSync(outDir, { recursive: true });
  writeFileSync(join(outDir, 'ik-bendpositive-probe.json'), JSON.stringify(result, null, 2), 'utf8');
}
process.exit(failed > 0 && parsed === 0 ? 4 : 0);
