#!/usr/bin/env node
/**
 * Task 5 A1 补充取证（TBD-F）：附件**类型字节**直接普查。
 *
 * 背景（F1）：`mesh-constraint-probe.mjs` 走的是旧 TS Reader，而该 Reader 解析
 * linked 附件时要**当场**查父附件；父附件在文件里排在后面、且 default skin 名为 null，
 * 查找落空 ⇒ 走回退分支 ⇒ 产出的 `weighted` / `vertices` / `uvs` / `hull` 全是假值。
 * 因此 **linked 附件的继承字段不可信**，但**类型字节是它自己的字段，可信**。
 *
 * 本探针不复用旧 Reader，而是按 `importers/spine32/src/binary_reader.cpp`（B4 已验证的
 * 字节走查结论）独立遍历：header → bones → ik → transform → slots → skins，
 * 逐个附件读出 `type` 字节并按类型专属布局跳过载荷。
 *
 * 边界：
 *   · 只走到 skins 段为止，**不解析 events / animations** ⇒ 不做 EOF 断言；
 *     本工具是普查不是校验器，正确性靠与 A1 已知计数的交叉核对来兜。
 *   · 语料只读，不写任何产品代码，不修改语料。
 *   · 文档中不出现语料绝对路径；路径由命令行传入。
 *
 * 用法：
 *   node tools/skeleton-probe/type-byte-census.mjs "<语料根目录>" [-o <输出目录>]
 */

import fs from 'node:fs';
import path from 'node:path';

const TYPE_NAME = ['region', 'boundingbox', 'mesh', 'weightedMesh', 'linkedMesh', 'weightedLinkedMesh'];

// 与 C++ Reader 一致的上限（binary_reader.hpp §常量 / binary_reader.cpp）：
// 没有这些护栏，一旦字节错位读出一个巨大的 count，循环会空转到超时。
const K_MAX_VARINT_BYTES = 5;
const K_MAX_STRING_BYTES = 4096;
const K_MAX_ARRAY_COUNT = 1 << 20;
const K_MAX_BONES_PER_VERTEX = 64;

class Reader {
  constructor(buf) {
    this.b = buf;
    this.p = 0;
    this.err = null;
    this.errPos = 0;
  }
  need(n) {
    if (this.p + n > this.b.length) { this.err = this.err ?? 'TRUNCATED'; this.errPos = this.p; return false; }
    return true;
  }
  u8() { if (!this.need(1)) return 0; return this.b[this.p++]; }
  // Spine 二进制的多字节数值是**大端**（C++ Reader 的 i32 / f32 均为大端拼接）。
  // 这里若写成 LE，走查会在第一段浮点处错位，全语料必然大面积失败。
  i32() { if (!this.need(4)) return 0; const v = this.b.readInt32BE(this.p); this.p += 4; return v; }
  f32() {
    if (!this.need(4)) return 0;
    const v = this.b.readFloatBE(this.p);
    this.p += 4;
    if (!Number.isFinite(v)) { this.err = this.err ?? 'NON_FINITE_FLOAT'; this.errPos = this.p; return 0; }
    return v;
  }
  varint() {
    let r = 0;
    for (let i = 0; i < K_MAX_VARINT_BYTES; i++) {
      if (!this.need(1)) return 0;
      const byte = this.b[this.p++];
      r |= (byte & 0x7f) << (7 * i);
      if ((byte & 0x80) === 0) return r >>> 0;
    }
    this.err = this.err ?? 'VARINT_OVERFLOW'; this.errPos = this.p;
    return 0;
  }
  /** 长度前缀字符串：0 = null；n = 长度 + 1（空串写作 1）。 */
  string() {
    const n = this.varint();
    if (this.err) return null;
    if (n === 0) return null;
    const len = n - 1;
    if (len > K_MAX_STRING_BYTES) { this.err = this.err ?? 'STRING_TOO_LONG'; this.errPos = this.p; return null; }
    if (!this.need(len)) return null;
    const s = this.b.toString('utf8', this.p, this.p + len);
    this.p += len;
    return s;
  }
  /** 数组长度：与 C++ `count()` 一样带上界检查。 */
  count() {
    const n = this.varint();
    if (this.err) return 0;
    if (n > K_MAX_ARRAY_COUNT) { this.err = this.err ?? 'COUNT_LIMIT'; this.errPos = this.p; return 0; }
    return n;
  }
  shortArray() {
    const n = this.count();
    if (this.err) return;
    if (!this.need(n * 2)) return;
    this.p += n * 2;
  }
}

/** 加权顶点表：每顶点 boneCount(float) + boneCount × (boneIndex, x, y, weight)，全是 float。 */
function skipWeightedVertices(r, n) {
  for (let i = 0; i < n; i++) {
    const bc = r.f32();
    if (r.err) return;
    if (!Number.isFinite(bc) || bc < 0 || bc > K_MAX_BONES_PER_VERTEX || Math.abs(bc - Math.floor(bc)) > 1e-4) {
      r.err = r.err ?? 'VERTEX_BONECOUNT';
      return;
    }
    for (let j = 0; j < bc; j++) { r.f32(); r.f32(); r.f32(); r.f32(); }
    if (r.err) return;
  }
}

function scan(buf) {
  const r = new Reader(buf);
  const atts = [];

  // header
  r.string();                       // hash
  const version = r.string();
  if (r.err) return { err: r.err ?? 'HEADER', pos: r.errPos, size: r.b.length };
  if (version !== '3.2.01') return { err: 'VERSION:' + String(version) };
  r.f32(); r.f32();                 // width / height
  const ne = r.u8() !== 0;
  if (ne) r.string();               // imagesPath
  if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };

  // bones
  const boneCount = r.count();
  for (let i = 0; i < boneCount; i++) {
    r.string();
    if (i > 0) r.varint();          // parentIndex
    for (let k = 0; k < 8; k++) r.f32();
    r.u8(); r.u8();                 // inheritScale / inheritRotation
    if (ne) r.i32();                // 非必要的骨骼颜色（漏掉会让整段错位）
    if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };
  }
  if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };

  // ik：name, boneCount, bones[], target, mix, bendPositive
  const ikCount = r.count();
  for (let i = 0; i < ikCount; i++) {
    r.string();
    const bc = r.count();
    for (let j = 0; j < bc; j++) r.varint();
    r.varint();
    r.f32();
    r.u8();
    if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };
  }
  if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };

  // transform：name, boneIndex, target, 10 × f32
  const trCount = r.count();
  for (let i = 0; i < trCount; i++) {
    r.string();
    r.varint(); r.varint();
    for (let k = 0; k < 10; k++) r.f32();
    if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };
  }

  // slots：name, boneIndex, color, attachment?, blend
  const slotCount = r.count();
  for (let i = 0; i < slotCount; i++) {
    r.string();
    r.varint();
    r.i32();
    r.string();
    const blend = r.u8();
    if (blend > 3) { r.err = r.err ?? 'BLEND'; break; }
    if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };
  }
  if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };

  const readSkinBody = (skinIdx, skinName) => {
    const sc = r.count();
    for (let i = 0; i < sc; i++) {
      if (r.err) return;
      const slotIdx = r.varint();
      const ac = r.count();
      for (let j = 0; j < ac; j++) {
        if (r.err) return;
        const key = r.string();
        const name = r.string();
        const t = r.u8();
        if (r.err) return;
        const rec = { skinIdx, skinName, slotIdx, key, name, type: t };

        if (t === 0) {                       // region：path + 7×f32(rot,x,y,sx,sy,w,h) + color
          r.string();
          for (let k = 0; k < 7; k++) { r.f32(); if (r.err) return; }
          r.i32();
        } else if (t === 1) {                // boundingbox
          const n = r.count();
          for (let k = 0; k < n * 2; k++) { r.f32(); if (r.err) return; }
          if (ne) r.i32();
        } else {
          r.string();                        // path
          r.i32();                           // color
          if (r.err) return;
          if (t === 4 || t === 5) {          // linkedmesh / weightedlinkedmesh
            rec.parentSkin = r.string();
            rec.parent = r.string();
            rec.inheritDeform = r.u8() !== 0;
            if (ne) { r.f32(); r.f32(); }
          } else {                           // mesh / weightedmesh
            const n = r.count();
            for (let k = 0; k < n * 2; k++) { r.f32(); if (r.err) return; }  // uvs
            r.shortArray();                            // triangles
            if (r.err) return;
            if (t === 3) skipWeightedVertices(r, n);
            else for (let k = 0; k < n * 2; k++) { r.f32(); if (r.err) return; }
            if (r.err) return;
            r.varint();                                // hull（只有长度）
            if (r.err) return;
            if (ne) { r.shortArray(); r.f32(); r.f32(); }
          }
        }
        atts.push(rec);
        if (r.err) return;
      }
    }
  };

  readSkinBody(0, null);                     // default skin，无名字
  if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };
  const extra = r.count();
  for (let i = 0; i < extra; i++) {
    if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };
    const sname = r.string();
    if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };
    readSkinBody(i + 1, sname);
    if (r.err) return { err: r.err, pos: r.errPos, size: r.b.length };
  }

  return { ok: true, atts, version, endPos: r.p };
}

function collect(root) {
  const out = [];
  const walk = (dir, depth) => {
    if (depth > 12) return;
    let es;
    try { es = fs.readdirSync(dir, { withFileTypes: true }); } catch { return; }
    es.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
    for (const e of es) {
      const p = path.join(dir, e.name);
      if (e.isDirectory()) walk(p, depth + 1);
      else if (e.name.toLowerCase().endsWith('.skel.bytes')) out.push(p);
    }
  };
  walk(root, 0);
  return out.sort();
}

const isPrimaryRel = (rel) => /^([^/]+)\/\1\.skel\.bytes$/.test(rel);

const argv = process.argv.slice(2);
const root = argv.find((a) => !a.startsWith('-'));
let outDir = null;
const oi = argv.indexOf('-o');
if (oi >= 0 && argv[oi + 1]) outDir = argv[oi + 1];

if (!root) {
  console.error('用法: node tools/skeleton-probe/type-byte-census.mjs "<语料根>" [-o <输出目录>]');
  process.exit(2);
}

const lines = [];
const say = (s = '') => { lines.push(s); console.log(s); };

const files = collect(root);
const buckets = {
  Full: { units: 0, ok: 0, fail: 0, byType: [0, 0, 0, 0, 0, 0], other: 0, linked: [] },
  Primary: { units: 0, ok: 0, fail: 0, byType: [0, 0, 0, 0, 0, 0], other: 0, linked: [] },
};
const failures = [];

for (const file of files) {
  const rel = path.relative(root, file).replace(/\\/g, '/');
  let buf;
  try { buf = fs.readFileSync(file); } catch { continue; }
  const res = scan(buf);
  if (!res.ok) {
    if (isPrimaryRel(rel)) { buckets.Primary.units++; buckets.Primary.fail++; }
    buckets.Full.units++; buckets.Full.fail++;
    if (failures.length < 10) failures.push(`\`${rel}\` → ${res.err}（字节位置 ${res.pos} / 文件 ${res.size} 字节）`);
    continue;
  }
  buckets.Full.units++; buckets.Full.ok++;
  const targets = [['Full', res.atts]];
  if (isPrimaryRel(rel)) { buckets.Primary.units++; buckets.Primary.ok++; targets.push(['Primary', res.atts]); }

  for (const [name, atts] of targets) {
    const b = buckets[name];
    for (const a of atts) {
      if (a.type < 6) b.byType[a.type]++; else b.other++;
    }
    // linked 附件：解析父引用，读出**父附件自己的类型字节**
    for (const a of atts) {
      if (a.type !== 4 && a.type !== 5) continue;
      let parentRec = null;
      const wantSkin = a.parentSkin ?? null;
      // 语料 2 条写 `parentSkin = null`（＝同皮肤）；合成 fixture 08 写的是显式 `"default"`。
      // 两条是不同的解析路径，都要能解析，否则会把合法父引用误报成「未找到」。
      const skinMatches = (c) => {
        if (wantSkin === null) return c.skinIdx === a.skinIdx;
        if (c.skinName === wantSkin) return true;
        return wantSkin === 'default' && c.skinIdx === 0;  // default 皮肤没有名字字段
      };
      for (const c of atts) {
        if (c === a) continue;
        if (!skinMatches(c)) continue;
        const cname = c.name ?? c.key;
        if (cname === a.parent) { parentRec = c; break; }
        if (c.key === a.parent) { parentRec = c; break; }
      }
      b.linked.push({
        unit: rel,
        skin: a.skinName ?? '(default)',
        slotIdx: a.slotIdx,
        key: a.key,
        attName: a.name ?? '(null)',
        typeByte: a.type,
        typeName: TYPE_NAME[a.type],
        parentField: a.parent,
        parentSkinField: a.parentSkin ?? '(null)',
        inheritDeform: a.inheritDeform,
        parentFound: parentRec !== null,
        parentTypeByte: parentRec ? parentRec.type : null,
        parentTypeName: parentRec ? TYPE_NAME[parentRec.type] ?? 'other' : null,
      });
    }
  }
}

const fmt = (n) => n.toLocaleString('en-US');

say('# TBD-F：附件类型字节直接普查');
say();
say(`- 语料根：\`${path.basename(root)}\`（命令行传入，报告不落绝对路径）`);
say(`- 分母：**Full ${fmt(buckets.Full.units)}** / **Primary ${fmt(buckets.Primary.units)}** —— 两口径分别统计，不合并（D12）`);
say(`- 走查：按 \`binary_reader.cpp\`（B4 已验证布局）独立遍历到 skins 段为止；解析失败 Full ${buckets.Full.fail} / Primary ${buckets.Primary.fail}`);
say();
say('## 1. 类型字节计数（直接读字节，不经旧 Reader 的继承回退）');
say();
say('| 类型字节 | 名称 | Full | Primary |');
say('|---|---|---:|---:|');
for (let t = 0; t < 6; t++) {
  say(`| ${t} | ${TYPE_NAME[t]} | ${fmt(buckets.Full.byType[t])} | ${fmt(buckets.Primary.byType[t])} |`);
}
say(`| ≥6 | 其他/未知 | ${fmt(buckets.Full.other)} | ${fmt(buckets.Primary.other)} |`);
say();

const f = buckets.Full.byType, p = buckets.Primary.byType;
say('## 2. 与 A1 已知计数的交叉核对');
say();
say('| 项 | A1 报告值 | 本次普查 | 一致 |');
say('|---|---|---:|---|');
const chk = [
  ['region（类型 0）', '27,459 / 24,605', `${fmt(f[0])} / ${fmt(p[0])}`, f[0] === 27459 && p[0] === 24605],
  ['mesh（类型 2）', '2,979 / 2,572', `${fmt(f[2])} / ${fmt(p[2])}`, f[2] === 2979 && p[2] === 2572],
  ['weightedMesh（类型 3）', '17,271 / ?', `${fmt(f[3])} / ${fmt(p[3])}`, f[3] === 17271],
  ['linked 合计（4+5）', '2 / 2', `${fmt(f[4] + f[5])} / ${fmt(p[4] + p[5])}`, (f[4] + f[5]) === 2 && (p[4] + p[5]) === 2],
  ['boundingbox（类型 1）', '0 / 0', `${fmt(f[1])} / ${fmt(p[1])}`, f[1] === 0 && p[1] === 0],
];
for (const [k, a, b, ok] of chk) say(`| ${k} | ${a} | ${b} | ${ok ? '✅' : '❌'} |`);
say();

say('## 3. Linked 附件逐条明细（F1 的核心问题）');
say();
if (buckets.Full.linked.length === 0) {
  say('无 linked 附件。');
} else {
  say('| 单位 | skin | slotIdx | key | 附件名 | **类型字节** | 类型名 | parent 字段 | parentSkin | inheritDeform | 父附件类型字节 | 父类型名 |');
  say('|---|---|---:|---|---|---:|---|---|---|---:|---|');
  for (const l of buckets.Full.linked) {
    say(`| \`${l.unit}\` | ${l.skin} | ${l.slotIdx} | \`${l.key}\` | ${l.attName} | **${l.typeByte}** | ${l.typeName} | \`${l.parentField}\` | ${l.parentSkinField} | ${l.inheritDeform} | ${l.parentFound ? l.parentTypeByte : '未找到'} | ${l.parentTypeName ?? '—'} |`);
  }
  say();
  const wlm = buckets.Full.linked.filter((l) => l.typeByte === 5).length;
  const lm = buckets.Full.linked.filter((l) => l.typeByte === 4).length;
  say(`- 类型 4（linkedMesh）**${lm}** 条；类型 5（weightedLinkedMesh）**${wlm}** 条。`);
  say(`- 父附件是否加权：${buckets.Full.linked.map((l) => `${l.key}→父类型 ${l.parentTypeName}(${l.parentTypeByte})`).join('；')}`);
  say('- 由此判定：A1 报告「两条都不加权」是**旧 Reader 回退分支的假值**（继承字段不可信），以本普查的类型字节为准。');
}
say();
if (failures.length) {
  say('## 4. 走查失败样本（最多 10 条）');
  say();
  for (const x of failures) say(`- ${x}`);
  say();
}

if (outDir) {
  fs.mkdirSync(outDir, { recursive: true });
  const dst = path.join(outDir, 'type-byte-census.md');
  fs.writeFileSync(dst, lines.join('\n') + '\n', 'utf8');
  console.log(`\n已写出：${dst}`);
}
