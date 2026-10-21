#!/usr/bin/env node
/**
 * Loongbones · Task 5 阶段 A1 — **顶点空间判据的对照实验**（只读探针）
 *
 * 为什么需要它：`mesh-constraint-probe.mjs` 用「顶点到骨骼世界位置的距离 vs 到骨架原点的
 * 距离」判断顶点所在空间，但**该判据本身没有经过验证**。本脚本用三个**已知答案**的样本
 * 检验判据是否成立，避免把判据的失效当成语料的新发现：
 *
 *   · Test A（加权）：同一顶点的多条 (bone,x,y)，变换到骨架空间后是否重合
 *     —— Spine 语义下每条都是「该骨骼局部坐标」，重合 ⇒ 局部空间；不重合 ⇒ 骨架空间。
 *   · Test B（对照）：`region.x/y` 的语义已知是**槽骨骼局部坐标**，用同一判据跑一遍；
 *     若判据把已知局部坐标判成「骨架空间」，判据无效。
 *   · Test C / D（非加权）：同 slot 内同时有 region 与 mesh 时，mesh 质心离
 *     `region.x/y`（局部）更近，还是离 region 的**世界**位置更近 —— 前者近 ⇒ 局部空间。
 *
 * 纪律：只读语料；输出只有统计量与相对路径（规范 §8.2 / §9.3）；解析复用旧项目
 * `converter/src/read/binary32.ts` 作**证据参照**，不用于产品实现。
 *
 * 用法见同目录 README.md。
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
// tools/skeleton-probe → 上溯 3 级 = 旧项目根（Reader 所在处）
const OLD_ROOT = path.resolve(HERE, '../../..');
const toUrl = (p) => 'file:///' + p.replace(/\\/g, '/');

const args = process.argv.slice(2);
const flag = (n) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : undefined; };
const CORPUS_ROOT = path.resolve(flag('-r') ?? args.find((a) => !a.startsWith('-')) ?? process.env['LOONGBONES_CORPUS_ROOT'] ?? '');
if (!CORPUS_ROOT || !fs.existsSync(CORPUS_ROOT)) {
  console.error('用法：node tools/skeleton-probe/tc1-space-control.mjs <语料根目录>');
  process.exit(2);
}

const { readSkeletonBinary32 } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/read/binary32.ts')));
const { IssueLog } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/model.ts')));

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
  return out.map((p) => path.relative(root, p).split(path.sep).join('/')).sort();
}

const rad = (d) => ((d ?? 0) * Math.PI) / 180;
/** 骨骼 setup 世界变换（近似：忽略 3.2 的 inheritScale / inheritRotation 折叠语义）。 */
function boneWorld(bones) {
  const n = bones.length;
  const wx = new Array(n).fill(0), wy = new Array(n).fill(0);
  const A = new Array(n).fill(1), B = new Array(n).fill(0), C = new Array(n).fill(0), D = new Array(n).fill(1);
  for (let i = 0; i < n; i++) {
    const b = bones[i];
    const x = b.x ?? 0, y = b.y ?? 0;
    const rot = rad(b.rotation), sx = b.scaleX ?? 1, sy = b.scaleY ?? 1;
    const ry = rot + Math.PI / 2 + rad(b.shearY);
    const la = Math.cos(rot + rad(b.shearX)) * sx, lb = Math.cos(ry) * sy;
    const lc = Math.sin(rot + rad(b.shearX)) * sx, ld = Math.sin(ry) * sy;
    const pi = b.parentIndex === undefined ? -1 : b.parentIndex;
    if (pi >= 0 && pi < i) {
      const pa = A[pi], pb = B[pi], pc = C[pi], pd = D[pi];
      wx[i] = pa * x + pb * y + wx[pi];
      wy[i] = pc * x + pd * y + wy[pi];
      A[i] = pa * la + pb * lc; B[i] = pa * lb + pb * ld;
      C[i] = pc * la + pd * lc; D[i] = pc * lb + pd * ld;
    } else { wx[i] = x; wy[i] = y; A[i] = la; B[i] = lb; C[i] = lc; D[i] = ld; }
  }
  return { wx, wy, A, B, C, D };
}

const stat = () => ({ n: 0, min: Infinity, max: -Infinity, sum: 0, over1: 0, over10: 0, over100: 0, over300: 0 });
function push(s, v) {
  if (!Number.isFinite(v)) return;
  s.n += 1;
  if (v < s.min) s.min = v;
  if (v > s.max) s.max = v;
  s.sum += v;
  if (v > 1) s.over1 += 1;
  if (v > 10) s.over10 += 1;
  if (v > 100) s.over100 += 1;
  if (v > 300) s.over300 += 1;
}
const fmt = (s) => (s.n === 0
  ? '—'
  : `n=${s.n} min=${s.min.toFixed(3)} max=${s.max.toFixed(3)} mean=${(s.sum / s.n).toFixed(3)}；>1:${s.over1} >10:${s.over10} >100:${s.over100} >300:${s.over300}`);

const spreadRaw = stat();      // A1：原始 (x,y) 各条目的最大两两距离
const spreadXform = stat();    // A2：变换到骨架空间后的最大两两距离
const identical = { same: 0, diff: 0 };
const regToOrigin = stat();    // B1
const regToSlot = stat();      // B2
const centroidToRegion = stat();   // C1
const centroidToOrigin = stat();   // C2
const centroidToRegionWorld = stat(); // D1
const centroidToBoneWorld = stat();   // D2

let units = 0, bonesTotal = 0, inheritFalse = 0, slotsWithBoth = 0;
for (const rel of collect(CORPUS_ROOT)) {
  const buf = fs.readFileSync(path.join(CORPUS_ROOT, rel));
  let data;
  try { data = readSkeletonBinary32(buf, new IssueLog(), { strictEof: true, validate: true }); } catch { continue; }
  units += 1;
  const bones = data.bones ?? [];
  bonesTotal += bones.length;
  for (const b of bones) if (b.inheritScale === false || b.inheritRotation === false) inheritFalse += 1;
  const W = boneWorld(bones);
  const n = bones.length;
  const slotToBone = new Map((data.slots ?? []).map((s) => [s.name, s.boneIndex ?? -1]));

  for (const skin of data.skins ?? []) {
    const bySlot = skin.attachments ?? {};
    for (const slotName of Object.keys(bySlot)) {
      const byKey = bySlot[slotName] ?? {};
      const sbi = slotToBone.get(slotName) ?? -1;
      const hasSlot = sbi >= 0 && sbi < n;
      const regions = Object.values(byKey).filter((a) => a && a.type === 'region');
      const meshes = Object.values(byKey).filter((a) => a && a.type === 'mesh' && a.weighted !== true && a.parent === undefined);

      // ---- Test C / D：同 slot 内 region 与 mesh 的对照 ----
      if (regions.length && meshes.length && hasSlot) {
        slotsWithBoth += 1;
        for (const m of meshes) {
          const vs = m.vertices ?? [];
          let sx = 0, sy = 0, c = 0;
          for (let i = 0; i + 1 < vs.length; i += 2) { sx += vs[i]; sy += vs[i + 1]; c += 1; }
          if (!c) continue;
          const cx = sx / c, cy = sy / c;
          let bestLocal = Infinity, bestWorld = Infinity;
          for (const r of regions) {
            const rx = r.x ?? 0, ry = r.y ?? 0;
            bestLocal = Math.min(bestLocal, Math.hypot(cx - rx, cy - ry));
            const wxx = W.A[sbi] * rx + W.B[sbi] * ry + W.wx[sbi];
            const wyy = W.C[sbi] * rx + W.D[sbi] * ry + W.wy[sbi];
            bestWorld = Math.min(bestWorld, Math.hypot(cx - wxx, cy - wyy));
          }
          push(centroidToRegion, bestLocal);
          push(centroidToRegionWorld, bestWorld);
          push(centroidToOrigin, Math.hypot(cx, cy));
          push(centroidToBoneWorld, Math.hypot(cx - W.wx[sbi], cy - W.wy[sbi]));
        }
      }

      for (const key of Object.keys(byKey)) {
        const a = byKey[key];
        if (!a) continue;
        // ---- Test B：region.x/y（已知：槽骨骼局部坐标）----
        if (a.type === 'region') {
          const x = a.x ?? 0, y = a.y ?? 0;
          push(regToOrigin, Math.hypot(x, y));
          if (hasSlot) push(regToSlot, Math.hypot(x - W.wx[sbi], y - W.wy[sbi]));
          continue;
        }
        if (a.type !== 'mesh' || a.weighted !== true || a.parent !== undefined) continue;
        // ---- Test A：加权顶点的多骨骼条目 ----
        const vs = a.vertices ?? [];
        let p = 0;
        while (p < vs.length) {
          const bcRaw = vs[p++];
          if (!Number.isFinite(bcRaw)) break;
          const bc = Math.round(bcRaw);
          const raw = [], xf = [];
          for (let k = 0; k < bc; k++) {
            const bi = Math.round(vs[p++]); const x = vs[p++], y = vs[p++]; vs[p++];
            if (!Number.isFinite(bi) || !Number.isFinite(x) || !Number.isFinite(y)) continue;
            raw.push([x, y]);
            if (bi >= 0 && bi < n) xf.push([W.A[bi] * x + W.B[bi] * y + W.wx[bi], W.C[bi] * x + W.D[bi] * y + W.wy[bi]]);
          }
          if (bc >= 2) {
            let sr = 0, sx = 0;
            for (let i = 0; i < raw.length; i++) for (let j = i + 1; j < raw.length; j++) {
              sr = Math.max(sr, Math.hypot(raw[i][0] - raw[j][0], raw[i][1] - raw[j][1]));
            }
            for (let i = 0; i < xf.length; i++) for (let j = i + 1; j < xf.length; j++) {
              sx = Math.max(sx, Math.hypot(xf[i][0] - xf[j][0], xf[i][1] - xf[j][1]));
            }
            push(spreadRaw, sr);
            push(spreadXform, sx);
            if (sr < 1e-6) identical.same += 1; else identical.diff += 1;
          }
        }
      }
    }
  }
}

const L = [];
const say = (s = '') => L.push(s);
say('# Task 5 A1 · 顶点空间判据的对照实验');
say();
say(`- 单位 ${units}；骨骼总数 ${bonesTotal}；inheritScale / inheritRotation 为 false 的骨骼 ${inheritFalse}（世界变换是判据级近似，会贡献残差）`);
say(`- 同时含 region 与非加权 mesh 的 slot：${slotsWithBoth}`);
say();
say('## Test A · 加权顶点（≥2 骨）：同一顶点各骨骼条目的一致性');
say();
say(`- A1 原始 (x,y) 条目间最大距离 ${fmt(spreadRaw)}`);
say(`- A2 变换到骨架空间后最大距离 ${fmt(spreadXform)}`);
say(`- A3 各条目 (x,y) 完全相同的顶点：${identical.same} / 不同：${identical.diff}`);
say();
say('判读：A2 ≪ A1 ⇒ 每条 (x,y) 是**该骨骼的局部坐标**（Spine 语义）；若 A1 ≈ 0 且 A2 很大，才是骨架空间。');
say();
say('## Test B · 对照：region.x/y（已知是槽骨骼局部坐标）用同一判据');
say();
say(`- B1 到骨架原点的距离 ${fmt(regToOrigin)}`);
say(`- B2 到槽骨骼世界位置的距离 ${fmt(regToSlot)}`);
say();
say('判读：B2 ≫ B1 ⇒ 「离原点更近 ⇒ 骨架空间」这一判据会把**已知局部坐标**误判为骨架空间 ⇒ 判据无效。');
say();
say('## Test C / D · 同 slot 内非加权 mesh 质心 vs region');
say();
say(`- C1 质心 → region.x/y（局部） ${fmt(centroidToRegion)}`);
say(`- C2 质心 → 骨架原点 ${fmt(centroidToOrigin)}`);
say(`- D1 质心 → region 的**世界**位置 ${fmt(centroidToRegionWorld)}`);
say(`- D2 质心 → 槽骨骼世界原点 ${fmt(centroidToBoneWorld)}`);
say();
say('判读：C1 ≪ C2 且 D1 与 D2 同量级 ⇒ mesh 顶点与 region 同处**槽骨骼局部空间**。');
console.log(L.join('\n'));
