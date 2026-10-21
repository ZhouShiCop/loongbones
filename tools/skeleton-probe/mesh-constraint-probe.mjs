#!/usr/bin/env node
/**
 * Loongbones · Task 5 阶段 A1 — 第二批静态数据**语料取证**探针（只读）
 *
 * 任务来源：`Agent_Task/Task5.md` §2（T-C1…T-C5）与 §3 A1。
 *
 * 它只回答「语料里到底是什么样」，不写任何产品代码、不改语料：
 *   · T-C1 加权 mesh 的顶点空间与绑定姿势 → 权重编码、boneCount、boneIndex 越界；
 *   · T-C2 权重归一化 → 每顶点权重和的分布（min/max/离差比例/NaN）；
 *   · T-C3 LinkedMesh 语义 → 逐条记录全部 linked 附件（语料极少，必须逐个看）；
 *   · T-C4 `hull` / `edges` → 出现率、取值与顶点数的关系（可再生性判据）；
 *   · T-C5 BoundingBox → 确认 0 例（3.2.01 附件类型只有 0..5，无 path/clipping）；
 *   以及 IK 链长与目标合法性、transform 约束 4×mix / 6×offset 取值域。
 *
 * 纪律（规范 §8.2 / §9.3；Task 5 §6）：
 *   · **只读**语料；输出**只有统计量与相对路径**，不写盘符/用户名、不写原文；
 *   · Full 与 Primary 是**两个分母**，分别统计，不合并（D12）；
 *   · 本工具是开发期探针，**不是产品读取器**：解析复用旧项目
 *     `converter/src/read/binary32.ts`（证据参照），产品 Reader 在阶段 B 独立实现。
 *
 * 用法见同目录 README.md。
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
// tools/skeleton-probe → 上溯 3 级 = D:/Works/Spine（旧项目根，Reader 所在处）
const OLD_ROOT = path.resolve(HERE, '../../..');
const toUrl = (p) => 'file:///' + p.replace(/\\/g, '/');

const args = process.argv.slice(2);
const flag = (n) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : undefined; };
const POSITIONAL = args.find((a) => !a.startsWith('-'));
const CORPUS_ROOT = path.resolve(flag('-r') ?? POSITIONAL ?? process.env['LOONGBONES_CORPUS_ROOT'] ?? '');
const OUT_DIR = path.resolve(flag('-o') ?? path.join(HERE, 'out'));

if (!CORPUS_ROOT || !fs.existsSync(CORPUS_ROOT)) {
  console.error('用法：node tools/skeleton-probe/mesh-constraint-probe.mjs <语料根目录> [-o 输出目录]');
  console.error('或设置环境变量 LOONGBONES_CORPUS_ROOT');
  process.exit(2);
}

const { readSkeletonBinary32 } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/read/binary32.ts')));
const { IssueLog } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/model.ts')));

// ---------------------------------------------------------------------------
// 收集 / 小工具
// ---------------------------------------------------------------------------

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

const isPrimaryRel = (rel) => /^([^/]+)\/\1\.skel\.bytes$/.test(rel);

/** 数值统计器：count / min / max / sum，忽略非有限值（单独计 nonFinite）。 */
function numStat() {
  return { count: 0, min: Infinity, max: -Infinity, sum: 0, nonFinite: 0, zeroOrOne: 0, other: 0, zero: 0, one: 0, otherIntegral: 0 };
}
function pushNum(s, v) {
  s.count += 1;
  if (!Number.isFinite(v)) { s.nonFinite += 1; return; }
  if (v < s.min) s.min = v;
  if (v > s.max) s.max = v;
  s.sum += v;
}
/**
 * mix / offset 类字段：除数值分布外，还要看清它是「0/1 开关」还是「有量纲的量」
 * ——语料里 transform 的 mix 越出 [0,1]，所以绝不能默认它是 0..1 插值因子。
 */
function pushMix(s, v) {
  pushNum(s, v);
  if (!Number.isFinite(v)) return;
  if (v === 0) { s.zero += 1; s.zeroOrOne += 1; return; }
  if (v === 1) { s.one += 1; s.zeroOrOne += 1; return; }
  s.other += 1;
  // 「其它值」是否为整数：角度/像素类的量通常是整数，插值因子通常不是
  if (Math.abs(v - Math.round(v)) < 1e-6) s.otherIntegral += 1;
}
const fmtNum = (s) => (s.count === 0
  ? '—'
  : `n=${s.count} min=${s.min} max=${s.max} mean=${(s.sum / s.count).toFixed(6)} nonFinite=${s.nonFinite}`);
const fmtMix = (s) => (s.count === 0
  ? '—'
  : `${fmtNum(s)}；取值：0×${s.zero}、1×${s.one}、其它×${s.other}（其中整数 ${s.otherIntegral}）`);

/** 直方图：key → count。 */
function hist() { return new Map(); }
function bump(h, k) { h.set(k, (h.get(k) ?? 0) + 1); }
function topN(h, n) {
  return [...h.entries()].sort((a, b) => (b[1] - a[1]) || (String(a[0]) < String(b[0]) ? -1 : 1)).slice(0, n);
}

// ---------------------------------------------------------------------------
// 聚合容器（Full / Primary 双口径）
// ---------------------------------------------------------------------------

function newBucket() {
  return {
    units: 0, parseOk: 0, parseFail: 0,
    // 附件分类（按旧 Reader 解出的形态；与 B4 的原始 typeByte 计数口径对照见报告）
    att: { mesh: 0, weightedMesh: 0, linked: 0, boundingBox: 0, region: 0, other: 0 },
    mesh: {
      vertexCount: numStat(), triangleCount: numStat(),
      vertexAbsXY: numStat(),
      distToSlotBoneWorld: numStat(), distToRootBoneWorld: numStat(), distToOrigin: numStat(),
      uvOutOfRange: 0, uvNonFinite: 0, triIndexOutOfRange: 0,
      hullCount: 0, hullEqualsVertexCount: 0, hullZero: 0, hullOther: 0,
      hullValues: hist(), edgesPresent: 0, edgesLength: numStat(),
      widthHeightPresent: 0,
    },
    weighted: {
      attachments: 0, vertices: 0,
      bonesPerVertex: numStat(), bonesPerVertexHist: hist(),
      boneIndexOutOfRange: 0, boneIndexNonInteger: 0,
      distToMainBoneWorld: numStat(), distToSlotBoneWorld: numStat(),
      distToRootBoneWorld: numStat(), distToOrigin: numStat(),
      weightSum: numStat(), weightSumAbsErr: numStat(), weightSumZero: 0,
      weightNegative: 0, weightNonFinite: 0, weightValues: numStat(),
      vertexXYNonFinite: 0, vertexXY: numStat(),
    },
    linked: [],           // T-C3：逐条记录
    boundingBox: [],      // T-C5：0 例复核（非空则逐条列出）
    ik: {
      count: 0, chainLength: numStat(), targetMissing: 0, boneMissing: 0,
      mix: numStat(), bendPositiveTrue: 0, bendPositiveFalse: 0, mixOutOf01: 0,
    },
    transform: {
      count: 0, boneMissing: 0, targetMissing: 0,
      rotateMix: numStat(), translateMix: numStat(), scaleMix: numStat(), shearMix: numStat(),
      offsetRotation: numStat(), offsetX: numStat(), offsetY: numStat(),
      offsetScaleX: numStat(), offsetScaleY: numStat(), offsetShearY: numStat(),
    },
    pathConstraint: 0, clippingAttachment: 0,
  };
}

const full = newBucket();
const primary = newBucket();
const parseFailures = [];

// ---------------------------------------------------------------------------
// 骨骼 setup 世界位置（T-C1 的判据基础）
// ---------------------------------------------------------------------------

/**
 * 按 setup 姿态递推每根骨骼的**世界位置**（只要平移分量，但 2×2 仍需完整递推，
 * 因为父骨的旋转/缩放/剪切会改变子骨位置）。
 *
 * 近似说明：忽略 3.2 的 `inheritScale` / `inheritRotation` 折叠语义（语料里这两个布尔
 * 绝大多数为真），因此这是**判据级近似**，不是求值器实现；结论只用于判断顶点所在空间。
 */
function computeBoneWorldPositions(bones) {
  const n = bones.length;
  const worldX = new Array(n).fill(0), worldY = new Array(n).fill(0);
  const A = new Array(n).fill(1), B = new Array(n).fill(0);
  const C = new Array(n).fill(0), D = new Array(n).fill(1);
  const rad = (d) => ((d ?? 0) * Math.PI) / 180;
  for (let i = 0; i < n; i++) {
    const b = bones[i];
    const x = b.x ?? 0, y = b.y ?? 0;
    const rot = rad(b.rotation), sx = b.scaleX ?? 1, sy = b.scaleY ?? 1;
    const shx = rad(b.shearX), shy = rad(b.shearY);
    const rotationY = rot + Math.PI / 2 + shy;
    const la = Math.cos(rot + shx) * sx, lb = Math.cos(rotationY) * sy;
    const lc = Math.sin(rot + shx) * sx, ld = Math.sin(rotationY) * sy;
    const pi = b.parentIndex === undefined ? -1 : b.parentIndex;
    if (pi >= 0 && pi < i) {
      const pa = A[pi], pb = B[pi], pc = C[pi], pd = D[pi];
      worldX[i] = pa * x + pb * y + worldX[pi];
      worldY[i] = pc * x + pd * y + worldY[pi];
      A[i] = pa * la + pb * lc;
      B[i] = pa * lb + pb * ld;
      C[i] = pc * la + pd * lc;
      D[i] = pc * lb + pd * ld;
    } else {
      worldX[i] = x; worldY[i] = y;
      A[i] = la; B[i] = lb; C[i] = lc; D[i] = ld;
    }
  }
  return { worldX, worldY };
}

// ---------------------------------------------------------------------------
// 单个单位的分析
// ---------------------------------------------------------------------------

function analyze(rel, buf, bucket) {
  bucket.units += 1;
  const log = new IssueLog();
  let data;
  try {
    data = readSkeletonBinary32(buf, log, { strictEof: true, validate: true });
  } catch (err) {
    bucket.parseFail += 1;
    if (parseFailures.length < 20) {
      parseFailures.push({ path: rel, error: String(err.message ?? err).slice(0, 200) });
    }
    return;
  }
  bucket.parseOk += 1;

  const boneCount = data.bones?.length ?? 0;
  const world = computeBoneWorldPositions(data.bones ?? []);

  // ---- 附件 ----
  for (const skin of data.skins ?? []) {
    const attachmentsBySlot = skin.attachments ?? {};
    for (const slotName of Object.keys(attachmentsBySlot)) {
      const byKey = attachmentsBySlot[slotName] ?? {};
      for (const key of Object.keys(byKey)) {
        const a = byKey[key];
        if (!a) continue;
        if (a.type === 'boundingbox') {
          bucket.att.boundingBox += 1;
          if (bucket.boundingBox.length < 20) {
            bucket.boundingBox.push({ path: rel, skin: skin.name ?? 'default', slot: slotName, name: a.name, vertexCount: (a.vertices?.length ?? 0) / 2 });
          }
          continue;
        }
        if (a.type === 'region') { bucket.att.region += 1; continue; }
        if (a.type !== 'mesh') { bucket.att.other += 1; continue; }

        const isLinked = a.parent !== undefined;
        const isWeighted = a.weighted === true;
        // 槽 → 骨骼：判据要用骨骼的**世界**位置（父链变换后），不能用骨骼的局部 x/y
        const slotRec = (data.slots ?? []).find((s) => s.name === slotName);
        const slotBoneIdx = slotRec?.boneIndex ?? -1;
        const hasSlotBone = slotBoneIdx >= 0 && slotBoneIdx < boneCount;
        if (isLinked) {
          bucket.att.linked += 1;
          if (bucket.linked.length < 40) {
            bucket.linked.push({
              path: rel, skin: skin.name ?? 'default', slot: slotName, name: a.name,
              parent: a.parent, parentSkin: a.parentSkin ?? 'default',
              inheritDeform: a.deform === true, weighted: isWeighted,
              vertexCount: (a.uvs?.length ?? 0) / 2,
            });
          }
          // linked 附件**继承父附件**的 uvs/triangles/vertices/hull/edges：
          // 重复计入会污染「顶点规模 / 权重和 / hull 出现率」的统计，这里只逐条记录。
          continue;
        }
        if (isWeighted) {
          bucket.att.weightedMesh += 1;
        } else {
          bucket.att.mesh += 1;
        }

        // ---- 通用 mesh 统计 ----
        const uvs = a.uvs ?? [];
        const n = uvs.length / 2;
        const m = bucket.mesh;
        if (!isLinked) {
          // linked 附件复用父附件的顶点，不重复计入顶点规模统计
          pushNum(m.vertexCount, n);
          pushNum(m.triangleCount, (a.triangles?.length ?? 0) / 3);
        }
        for (let i = 0; i < uvs.length; i++) {
          const v = uvs[i];
          if (!Number.isFinite(v)) { m.uvNonFinite += 1; continue; }
          if (v < 0 || v > 1) m.uvOutOfRange += 1;
        }
        const tris = a.triangles ?? [];
        for (const t of tris) if (!(t >= 0 && t < n)) m.triIndexOutOfRange += 1;
        if (typeof a.hull === 'number') {
          m.hullCount += 1;
          bump(m.hullValues, a.hull);
          if (a.hull === n) m.hullEqualsVertexCount += 1;
          else if (a.hull === 0) m.hullZero += 1;
          else m.hullOther += 1;
        }
        if (Array.isArray(a.edges)) {
          m.edgesPresent += 1;
          pushNum(m.edgesLength, a.edges.length);
        }
        if (typeof a.width === 'number' && typeof a.height === 'number') m.widthHeightPresent += 1;

        // ---- T-C1：非加权 mesh 的顶点空间判据 ----
        // DECISIONS.md N2 只记录「非加权 mesh 顶点是槽骨骼局部坐标」，且**不得沿用**到加权 mesh。
        // 这里给出可复核的数值：顶点到**槽骨骼 setup 位置**的距离 vs 到**骨架原点**的距离。
        // 前者显著更小 → 支持局部坐标；两者接近 → 说明可能是别的空间，不能直接套 N2。
        if (!isWeighted) {
          const vs = a.vertices ?? [];
          for (let i = 0; i + 1 < vs.length; i += 2) {
            const x = vs[i], y = vs[i + 1];
            if (!Number.isFinite(x) || !Number.isFinite(y)) continue;
            pushNum(m.vertexAbsXY, Math.abs(x));
            pushNum(m.vertexAbsXY, Math.abs(y));
            pushNum(m.distToOrigin, Math.hypot(x, y));
            if (hasSlotBone) {
              pushNum(m.distToSlotBoneWorld, Math.hypot(x - world.worldX[slotBoneIdx], y - world.worldY[slotBoneIdx]));
            }
            if (boneCount > 0) {
              pushNum(m.distToRootBoneWorld, Math.hypot(x - world.worldX[0], y - world.worldY[0]));
            }
          }
        }

        // ---- 加权顶点（T-C1 / T-C2）----
        if (isWeighted) {
          const w = bucket.weighted;
          w.attachments += 1;
          const vs = a.vertices ?? [];
          let p = 0;
          while (p < vs.length) {
            const bcRaw = vs[p++];
            if (!Number.isFinite(bcRaw)) break;
            const bc = Math.round(bcRaw);
            if (Math.abs(bcRaw - bc) > 1e-6) w.boneIndexNonInteger += 1;
            pushNum(w.bonesPerVertex, bc);
            bump(w.bonesPerVertexHist, bc);
            w.vertices += 1;
            let sum = 0;
            let maxW = -Infinity, maxBi = -1, maxX = 0, maxY = 0;
            for (let k = 0; k < bc; k++) {
              const biRaw = vs[p++];
              const x = vs[p++], y = vs[p++], wt = vs[p++];
              if (!Number.isFinite(biRaw) || !Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(wt)) {
                w.weightNonFinite += 1;
                continue;
              }
              const bi = Math.round(biRaw);
              if (Math.abs(biRaw - bi) > 1e-6) w.boneIndexNonInteger += 1;
              if (bi < 0 || bi >= boneCount) w.boneIndexOutOfRange += 1;
              if (!Number.isFinite(x) || !Number.isFinite(y)) w.vertexXYNonFinite += 1;
              else { pushNum(w.vertexXY, Math.abs(x)); pushNum(w.vertexXY, Math.abs(y)); }
              pushNum(w.weightValues, wt);
              if (wt < 0) w.weightNegative += 1;
              if (wt > maxW) { maxW = wt; maxBi = bi; maxX = x; maxY = y; }
              sum += wt;
            }
            pushNum(w.weightSum, sum);
            // T-C1 加权侧判据：顶点相对「主权重骨骼 setup 位置」的距离 vs 到骨架原点的距离
            if (maxBi >= 0 && maxBi < boneCount && Number.isFinite(maxX) && Number.isFinite(maxY)) {
              pushNum(w.distToMainBoneWorld, Math.hypot(maxX - world.worldX[maxBi], maxY - world.worldY[maxBi]));
              if (hasSlotBone) {
                pushNum(w.distToSlotBoneWorld, Math.hypot(maxX - world.worldX[slotBoneIdx], maxY - world.worldY[slotBoneIdx]));
              }
              if (boneCount > 0) {
                pushNum(w.distToRootBoneWorld, Math.hypot(maxX - world.worldX[0], maxY - world.worldY[0]));
              }
              pushNum(w.distToOrigin, Math.hypot(maxX, maxY));
            }
            pushNum(w.weightSumAbsErr, Math.abs(sum - 1));
            if (sum === 0) w.weightSumZero += 1;
          }
        }
      }
    }
  }

  // ---- IK 约束 ----
  for (const ik of data.ik ?? []) {
    const c = bucket.ik;
    c.count += 1;
    const bones = ik.bones ?? [];
    pushNum(c.chainLength, bones.length);
    for (const b of bones) if (b === '?' || b === undefined) c.boneMissing += 1;
    if (ik.target === '?' || ik.target === undefined) c.targetMissing += 1;
    pushMix(c.mix, ik.mix);
    if (Number.isFinite(ik.mix) && (ik.mix < 0 || ik.mix > 1)) c.mixOutOf01 += 1;
    if (ik.bendPositive === true) c.bendPositiveTrue += 1; else c.bendPositiveFalse += 1;
  }

  // ---- Transform 约束 ----
  for (const tc of data.transform ?? []) {
    const c = bucket.transform;
    c.count += 1;
    const bones = tc.bones ?? [];
    for (const b of bones) if (b === '?' || b === undefined) c.boneMissing += 1;
    if (tc.target === '?' || tc.target === undefined) c.targetMissing += 1;
    pushMix(c.rotateMix, tc.rotateMix);
    pushMix(c.translateMix, tc.translateMix);
    pushMix(c.scaleMix, tc.scaleMix);
    pushMix(c.shearMix, tc.shearMix);
    pushMix(c.offsetRotation, tc.offsetRotation);
    pushMix(c.offsetX, tc.offsetX);
    pushMix(c.offsetY, tc.offsetY);
    pushMix(c.offsetScaleX, tc.offsetScaleX);
    pushMix(c.offsetScaleY, tc.offsetScaleY);
    pushMix(c.offsetShearY, tc.offsetShearY);
  }
}

// ---------------------------------------------------------------------------
// 主流程
// ---------------------------------------------------------------------------

const rels = collect(CORPUS_ROOT);
if (rels.length === 0) {
  // 守门：语料根给错时不能输出一份「全 0」的报告当作证据（与 smoke 的退出码 3 同款纪律）
  console.error(`语料根下没有任何 .skel.bytes：${CORPUS_ROOT}（很可能是根目录给错了）`);
  process.exit(3);
}
const t0 = Date.now();
for (const rel of rels) {
  const buf = fs.readFileSync(path.join(CORPUS_ROOT, rel));
  analyze(rel, buf, full);
  if (isPrimaryRel(rel)) analyze(rel, buf, primary);
}
const elapsed = ((Date.now() - t0) / 1000).toFixed(1);

// ---------------------------------------------------------------------------
// 输出
// ---------------------------------------------------------------------------

const summary = {
  tool: 'mesh-constraint-probe',
  purpose: 'Task 5 阶段 A1 第二批静态数据取证（T-C1…T-C5、IK、transform）',
  denominators: {
    full: { files: full.units, parseOk: full.parseOk, parseFail: full.parseFail },
    primary: { files: primary.units, parseOk: primary.parseOk, parseFail: primary.parseFail },
  },
  elapsedSeconds: Number(elapsed),
  full, primary,
  parseFailureSamples: parseFailures,
};

fs.mkdirSync(OUT_DIR, { recursive: true });
const jsonPath = path.join(OUT_DIR, 'mesh-constraint-probe.json');
// JSON 里的直方图是 Map，需显式转成对象，否则序列化成 {}
fs.writeFileSync(jsonPath, JSON.stringify(summary, (_k, v) => (v instanceof Map ? Object.fromEntries(v) : v), 2));

const L = [];
const say = (s = '') => L.push(s);
say('# Task 5 A1 第二批静态数据取证（只读探针）');
say();
say(`- 分母：**Full ${full.units}**（全部 \`.skel.bytes\`）／**Primary ${primary.units}**（\`<dir>/<dir>.skel.bytes\`）—— 两口径分别统计，不合并`);
say(`- 解析：Full ok ${full.parseOk} / fail ${full.parseFail}；Primary ok ${primary.parseOk} / fail ${primary.parseFail}`);
say(`- 耗时 ${elapsed}s；机器可读结果：\`tools/skeleton-probe/out/mesh-constraint-probe.json\``);
say();

const attTable = (b) => `| mesh ${b.att.mesh} | weightedMesh ${b.att.weightedMesh} | linked ${b.att.linked} | boundingBox ${b.att.boundingBox} | region ${b.att.region} | other ${b.att.other} |`;
say('## 1. 附件分类');
say();
say('| 口径 | 分类计数 |');
say('|---|---|');
say(`| Full | ${attTable(full)} |`);
say(`| Primary | ${attTable(primary)} |`);
say();

say('## 2. Mesh 通用统计');
say();
for (const [name, b] of [['Full', full], ['Primary', primary]]) {
  const m = b.mesh;
  say(`### ${name}`);
  say();
  say(`- 顶点数/附件：${fmtNum(m.vertexCount)}`);
  say(`- 三角形数/附件：${fmtNum(m.triangleCount)}`);
  say(`- UV 越出 [0,1]：${m.uvOutOfRange}；UV 非有限：${m.uvNonFinite}`);
  say(`- 三角索引越界：${m.triIndexOutOfRange}`);
  say(`- hull：出现 ${m.hullCount}；= 顶点数 ${m.hullEqualsVertexCount}；=0 ${m.hullZero}；其它 ${m.hullOther}`);
  say(`- hull 取值 top10：${topN(m.hullValues, 10).map(([k, v]) => `${k}×${v}`).join(', ') || '—'}`);
  say(`- **非加权**顶点 |x|,|y|：${fmtNum(m.vertexAbsXY)}`);
  say(`- **非加权**顶点到槽骨骼**世界**位置的距离：${fmtNum(m.distToSlotBoneWorld)}`);
  say(`- **非加权**顶点到根骨骼**世界**位置的距离：${fmtNum(m.distToRootBoneWorld)}`);
  say(`- **非加权**顶点到骨架原点的距离：${fmtNum(m.distToOrigin)}`);
  say(`- edges：出现 ${m.edgesPresent}；长度 ${fmtNum(m.edgesLength)}`);
  say(`- width/height（nonessential）：${m.widthHeightPresent}`);
  say();
}

say('## 3. 加权顶点（T-C1 / T-C2）');
say();
for (const [name, b] of [['Full', full], ['Primary', primary]]) {
  const w = b.weighted;
  say(`### ${name}`);
  say();
  say(`- 加权附件 ${w.attachments}；加权顶点 ${w.vertices}`);
  say(`- 每顶点骨骼数：${fmtNum(w.bonesPerVertex)}`);
  say(`  分布 top10：${topN(w.bonesPerVertexHist, 10).map(([k, v]) => `${k}骨×${v}`).join(', ') || '—'}`);
  say(`- boneIndex 越界：${w.boneIndexOutOfRange}；非整数编码：${w.boneIndexNonInteger}`);
  say(`- 每顶点权重和：${fmtNum(w.weightSum)}`);
  say(`- |sum−1|：${fmtNum(w.weightSumAbsErr)}`);
  say(`- 权重和 = 0 的顶点：${w.weightSumZero}；负权重：${w.weightNegative}；非有限权重：${w.weightNonFinite}`);
  say(`- 权重取值：${fmtNum(w.weightValues)}`);
  say(`- 绑定姿势 |x|,|y|：${fmtNum(w.vertexXY)}；非有限顶点坐标：${w.vertexXYNonFinite}`);
  say(`- **加权**顶点到主权重骨骼**世界**位置的距离：${fmtNum(w.distToMainBoneWorld)}`);
  say(`- **加权**顶点到槽骨骼**世界**位置的距离：${fmtNum(w.distToSlotBoneWorld)}`);
  say(`- **加权**顶点到根骨骼**世界**位置的距离：${fmtNum(w.distToRootBoneWorld)}`);
  say(`- **加权**顶点到骨架原点的距离：${fmtNum(w.distToOrigin)}`);
  say();
}

say('## 4. LinkedMesh 逐条（T-C3）');
say();
say(`Full 共 ${full.att.linked} 条，Primary 共 ${primary.att.linked} 条。`);
say();
say('| 单位相对路径 | skin | slot | 附件名 | parent | parentSkin | inheritDeform | weighted | 顶点数 |');
say('|---|---|---|---|---|---|---|---|---|');
for (const r of full.linked) {
  say(`| \`${r.path}\` | ${r.skin} | ${r.slot} | ${r.name} | ${r.parent} | ${r.parentSkin} | ${r.inheritDeform} | ${r.weighted} | ${r.vertexCount} |`);
}
say();

say('## 5. BoundingBox（T-C5）与其它');
say();
say(`- boundingbox 附件：Full ${full.att.boundingBox} / Primary ${primary.att.boundingBox}`);
if (full.boundingBox.length) {
  for (const r of full.boundingBox) say(`  - \`${r.path}\` ${r.skin}/${r.slot}/${r.name} 顶点 ${r.vertexCount}`);
}
say('- 3.2.01 附件类型字节只有 0..5（region/boundingbox/mesh/weightedmesh/linkedmesh/weightedlinkedmesh）；');
say('  **path 约束与 clipping 附件在该版本格式中不存在**（3.5+ 才引入），故「0 例」是格式层面的，不是语料抽样结果。');
say();

say('## 6. IK 约束');
say();
for (const [name, b] of [['Full', full], ['Primary', primary]]) {
  const c = b.ik;
  say(`### ${name}`);
  say();
  say(`- 数量 ${c.count}；链长 ${fmtNum(c.chainLength)}`);
  say(`- 链内骨骼缺失（索引越界）：${c.boneMissing}；目标骨缺失：${c.targetMissing}`);
  say(`- mix：${fmtMix(c.mix)}；越出 [0,1]：${c.mixOutOf01}`);
  say(`- bendPositive：true ${c.bendPositiveTrue} / false ${c.bendPositiveFalse}`);
  say();
}

say('## 7. Transform 约束');
say();
for (const [name, b] of [['Full', full], ['Primary', primary]]) {
  const c = b.transform;
  say(`### ${name}`);
  say();
  say(`- 数量 ${c.count}；骨骼缺失 ${c.boneMissing}；目标骨缺失 ${c.targetMissing}`);
  say(`- rotateMix ${fmtMix(c.rotateMix)}`);
  say(`- translateMix ${fmtMix(c.translateMix)}`);
  say(`- scaleMix ${fmtMix(c.scaleMix)}`);
  say(`- shearMix ${fmtMix(c.shearMix)}`);
  say(`- offsetRotation ${fmtMix(c.offsetRotation)}`);
  say(`- offsetX ${fmtMix(c.offsetX)}；offsetY ${fmtMix(c.offsetY)}`);
  say(`- offsetScaleX ${fmtMix(c.offsetScaleX)}；offsetScaleY ${fmtMix(c.offsetScaleY)}`);
  say(`- offsetShearY ${fmtMix(c.offsetShearY)}`);
  say();
}

if (parseFailures.length) {
  say('## 8. 解析失败样本（前 20）');
  say();
  for (const f of parseFailures) say(`- \`${f.path}\` — ${f.error}`);
  say();
}

const mdPath = path.join(OUT_DIR, 'mesh-constraint-probe.md');
fs.writeFileSync(mdPath, L.join('\n'));

console.log(L.join('\n'));
console.log(`\n[written] ${jsonPath}`);
console.log(`[written] ${mdPath}`);
