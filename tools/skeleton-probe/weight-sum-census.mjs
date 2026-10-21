#!/usr/bin/env node
/**
 * Loongbones · Task 5 A5 —— 加权顶点「权重和」分布普查
 *
 * 用途：回答 **D37**（权重和必须有限且 > 0，`sum = 0` / `sum < 0` / NaN 均拒绝）
 * 在真实语料里**到底有没有触发样本**。没有的话，该拒绝路径就只有合成 fixture
 * （`12-mesh-bad-weight-sum-zero` / `13-mesh-bad-weight-sum-negative`）作证据，
 * 不能声称"语料已验证"。
 *
 * 布局来源与 `type-byte-census.mjs` 相同：`importers/spine32/src/binary_reader.cpp`
 * （Task 4 B4 已验证）。本脚本**独立走字节**，不调用旧项目 Reader。
 *
 * 两个已踩过的坑（改这个脚本时不要退回）：
 *   1. 多字节数值是**大端**（`readFloatBE` / `readInt32BE`），写成 LE 会整段错位；
 *   2. `count()` 必须带上界护栏，否则字节错位时会读到巨大长度并空转到超时；
 *   3. **region（类型 0）没有前置 color** —— 布局是 `path + 7×f32 + color`，
 *      而 mesh 族是 `path + color + …`。给 region 多读一个 i32 会让全语料错位
 *      （实测表现为 1104/1113 单位在文件末尾报 STR/COUNT）。
 *
 * 用法：
 *   node tools/skeleton-probe/weight-sum-census.mjs "<语料根>" [--tol 1e-4]
 */
import fs from 'node:fs';
import path from 'node:path';

const K_MAX_ARRAY_COUNT = 1 << 20;
const K_MAX_STRING_BYTES = 4096;
const K_MAX_BONES_PER_VERTEX = 64;

class Reader {
  constructor(b) { this.b = b; this.p = 0; this.err = null; }
  need(n) { if (this.p + n > this.b.length) { this.err = this.err ?? 'TRUNCATED'; return false; } return true; }
  u8() { if (!this.need(1)) return 0; return this.b[this.p++]; }
  i32() { if (!this.need(4)) return 0; const v = this.b.readInt32BE(this.p); this.p += 4; return v; }
  f32() { if (!this.need(4)) return 0; const v = this.b.readFloatBE(this.p); this.p += 4; return v; }
  varint() {
    let r = 0;
    for (let i = 0; i < 5; i++) {
      if (!this.need(1)) return 0;
      const byte = this.b[this.p++];
      r |= (byte & 0x7f) << (7 * i);
      if ((byte & 0x80) === 0) return r >>> 0;
    }
    this.err = this.err ?? 'VARINT_OVERFLOW';
    return 0;
  }
  string() {
    const n = this.varint();
    if (this.err) return null;
    if (n === 0) return null;
    const len = n - 1;
    if (len > K_MAX_STRING_BYTES) { this.err = this.err ?? 'STRING_TOO_LONG'; return null; }
    if (!this.need(len)) return null;
    const s = this.b.toString('utf8', this.p, this.p + len);
    this.p += len;
    return s;
  }
  count() {
    const n = this.varint();
    if (this.err) return 0;
    if (n > K_MAX_ARRAY_COUNT) { this.err = this.err ?? 'COUNT_LIMIT'; return 0; }
    return n;
  }
  shortArray() {
    const n = this.count();
    if (this.err) return;
    if (!this.need(n * 2)) return;
    this.p += n * 2;
  }
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

const root = process.argv[2];
if (!root) { console.error('用法：node weight-sum-census.mjs "<语料根>"'); process.exit(2); }
const tolArg = process.argv.indexOf('--tol');
const TOL = tolArg >= 0 ? Number(process.argv[tolArg + 1]) : 1e-4;

const files = collect(root);
let units = 0, verts = 0, zero = 0, neg = 0, nonFinite = 0, overTol = 0, badBoneCount = 0;
let minSum = Infinity, maxSum = -Infinity, negWeightBindings = 0;
const samples = [];
for (const f of files) {
  const r = new Reader(fs.readFileSync(f));
  r.string(); r.string(); r.f32(); r.f32();
  const ne = r.u8() !== 0;
  if (ne) r.string();

  const boneCount = r.count();
  for (let i = 0; i < boneCount; i++) {
    r.string();
    if (i > 0) r.varint();
    for (let k = 0; k < 8; k++) r.f32();
    r.u8(); r.u8();
    if (ne) r.i32();
  }
  const ikCount = r.count();
  for (let i = 0; i < ikCount; i++) {
    r.string();
    const bc = r.count();
    for (let j = 0; j < bc; j++) r.varint();
    r.varint(); r.f32(); r.u8();
  }
  const trCount = r.count();
  for (let i = 0; i < trCount; i++) {
    r.string(); r.varint(); r.varint();
    for (let k = 0; k < 10; k++) r.f32();
  }
  const slotCount = r.count();
  for (let i = 0; i < slotCount; i++) { r.string(); r.varint(); r.i32(); r.string(); r.u8(); }

  const readSkinBody = () => {
    const sc = r.count();
    for (let i = 0; i < sc; i++) {
      r.varint();
      const ac = r.count();
      for (let j = 0; j < ac; j++) {
        r.string(); r.string();
        const t = r.u8();
        if (t === 0) { r.string(); for (let k = 0; k < 7; k++) r.f32(); r.i32(); continue; }
        if (t === 1) { const n = r.count(); for (let k = 0; k < n * 2; k++) r.f32(); if (ne) r.i32(); continue; }
        r.string(); r.i32();                       // path + color（**仅 mesh 族有**）
        if (t === 4 || t === 5) { r.string(); r.string(); r.u8(); if (ne) { r.f32(); r.f32(); } continue; }
        const n = r.count();
        for (let k = 0; k < n * 2; k++) r.f32();   // uvs
        r.shortArray();                            // triangles
        if (t === 3) {
          for (let v = 0; v < n; v++) {
            const bc = r.f32();
            if (!(bc >= 1 && bc <= K_MAX_BONES_PER_VERTEX && Math.abs(bc - Math.round(bc)) < 1e-4)) { badBoneCount++; break; }
            let sum = 0;
            for (let k = 0; k < bc; k++) {
              r.f32(); r.f32(); r.f32();
              const wt = r.f32();
              if (wt < 0) negWeightBindings++;
              sum += wt;
            }
            verts++;
            if (!Number.isFinite(sum)) { nonFinite++; continue; }
            if (sum === 0) zero++;
            else if (sum < 0) {
              neg++;
              if (samples.length < 5) samples.push(`${path.relative(root, f).replace(/\\/g, '/')} v${v} sum=${sum}`);
            }
            if (sum < minSum) minSum = sum;
            if (sum > maxSum) maxSum = sum;
            if (Math.abs(sum - 1) > TOL) overTol++;
          }
        } else {
          for (let k = 0; k < n * 2; k++) r.f32();
        }
        r.varint();                                // hull
        if (ne) { r.shortArray(); r.f32(); r.f32(); }
      }
    }
  };
  readSkinBody();
  const extra = r.count();
  for (let i = 0; i < extra; i++) { r.string(); readSkinBody(); }
  if (!r.err) units++;
}

const pct = (n) => (verts ? ((100 * n) / verts).toFixed(4) + '%' : '—');
console.log('# 加权顶点「权重和」分布普查（D37 证据）');
console.log();
console.log(`- 语料根：\`${path.basename(root)}\`（命令行传入，报告不落绝对路径）`);
console.log(`- 走查单位 **${units} / ${files.length}**；加权顶点 **${verts}**；容差 tol = ${TOL}`);
console.log();
console.log('| 项 | 数量 | 占加权顶点 |');
console.log('|---|---:|---:|');
console.log(`| \`sum = 0\`（D37 拒绝） | ${zero} | ${pct(zero)} |`);
console.log(`| \`sum < 0\`（D37 拒绝） | ${neg} | ${pct(neg)} |`);
console.log(`| \`sum\` 非有限（NaN / ±Inf） | ${nonFinite} | ${pct(nonFinite)} |`);
console.log(`| \`|sum − 1| > tol\` | ${overTol} | ${pct(overTol)} |`);
console.log(`| 单条**负权重**绑定（D34 合法） | ${negWeightBindings} | — |`);
console.log(`| boneCount 非法（走查中断） | ${badBoneCount} | — |`);
console.log();
console.log(`- 权重和区间：**min ${minSum}**，**max ${maxSum}**`);
if (samples.length) { console.log('- `sum < 0` 样本：'); for (const s of samples) console.log(`  - \`${s}\``); }
console.log();
console.log('> 若 `sum = 0` / `sum < 0` / 非有限**全为 0**，说明 D37 的拒绝路径在真实语料里');
console.log('> **没有触发样本**，只能由合成 fixture 覆盖（不得表述为"语料已验证"）。');
