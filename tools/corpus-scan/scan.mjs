#!/usr/bin/env node
/**
 * Loongbones · Phase 0 — Sdorica Spine 3.2.01 语料只读扫描器
 *
 * 目的：把真实样本做成可复现的清单基线（规范 §5 Phase 0 / §12 交付物 2）。
 *
 * 纪律（规范 §8.2 / §9.3）：
 *   · **只读**语料：不移动、不改写、不删除、不复制任何游戏文件。
 *   · 输出的 manifest **只记相对路径 + hash + 统计量**，不写开发者盘符/用户名；
 *     语料根目录由参数或环境变量 LOONGBONES_CORPUS_ROOT 提供，不进 manifest。
 *   · 游戏原始资源不进入本仓库；manifest 是元数据，不是资产。
 *
 * 用法：
 *   node tools/corpus-scan/scan.mjs [语料根目录] [-o 输出目录] [--limit N]
 *
 * 输入：语料根目录（默认取 $LOONGBONES_CORPUS_ROOT）。
 * 输出：<输出目录>/corpus-manifest.json      机器可读全量清单
 *       <输出目录>/corpus-manifest.md        人读摘要（含 963/1113 口径分析）
 *       <输出目录>/corpus-failures.json      解析失败明细（无失败则空数组）
 *
 * 依赖：复用旧项目已验证的 3.2.01 Reader（converter/src/read/binary32.ts），
 *       它经 963 个真实文件严格 EOF 验证。本工具是**分析工具**，不是产品代码；
 *       产品读取器属 Phase 1/2 范围，届时按 schema 契约重写。
 */
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
// tools/corpus-scan → 上溯 3 级 = D:/Works/Spine（旧项目根，Reader 所在处）
const OLD_ROOT = path.resolve(HERE, '../../..');
const toUrl = (p) => 'file:///' + p.replace(/\\/g, '/');

const args = process.argv.slice(2);
const flag = (n) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : undefined; };
const has = (n) => args.includes(n);
const CORPUS_ROOT = path.resolve(flag('-r') ?? process.env['LOONGBONES_CORPUS_ROOT'] ?? '');
const OUT_DIR = path.resolve(flag('-o') ?? path.join(HERE, 'out'));
const LIMIT = Number(flag('--limit') ?? 0) || Infinity;

if (!CORPUS_ROOT) {
  console.error('用法：node tools/corpus-scan/scan.mjs <语料根目录> [-o 输出目录] [--limit N]');
  console.error('或设置环境变量 LOONGBONES_CORPUS_ROOT');
  process.exit(2);
}
if (!fs.existsSync(CORPUS_ROOT) || !fs.statSync(CORPUS_ROOT).isDirectory()) {
  console.error('语料根目录不存在或不是目录：', CORPUS_ROOT);
  process.exit(2);
}

// ---- Reader：只读解析，strictEof 用来识别截断/损坏 ----
const { readSkeletonBinary32 } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/read/binary32.ts')));
const { IssueLog } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/model.ts')));

/** 递归收集 *.skel.bytes，返回相对路径，排序保证可复现。 */
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
  return out.map((p) => path.relative(root, p).split(path.sep).join('/'));
}

/** 统计一份 IR 的能力特征，用于样本打标签。 */
function profile(ir) {
  let meshes = 0, weighted = 0, regions = 0, other = 0;
  let deform = 0, ikTl = 0, events = 0, slotTl = 0, boneTl = 0, drawOrder = 0;
  let maxVerts = 0;
  const animNames = [];
  for (const s of ir.skins)
    for (const k of Object.keys(s.attachments))
      for (const n of Object.keys(s.attachments[k])) {
        const a = s.attachments[k][n];
        if (a.type === 'mesh') {
          meshes++;
          if (a.weighted) weighted++;
          maxVerts = Math.max(maxVerts, (a.uvs?.length ?? 0) / 2);
        } else if (a.type === 'region') regions++;
        else other++;
      }
  for (const an of ir.animations ?? []) {
    animNames.push(an.name);
    for (const tl of an.timelines ?? []) {
      if (tl.kind === 'deform') deform++;
      else if (tl.kind === 'ik') ikTl++;
      else if (tl.kind === 'event') events++;
      else if (tl.kind === 'drawOrder') drawOrder++;
      else if (tl.kind === 'rotate' || tl.kind === 'translate' || tl.kind === 'scale') boneTl++;
      else if (tl.kind === 'attachment' || tl.kind === 'color') slotTl++;
    }
  }
  return {
    version: ir.skeleton?.version ?? null,
    skeletonHash: ir.skeleton?.hash ?? null,
    bones: ir.bones?.length ?? 0,
    slots: ir.slots?.length ?? 0,
    skins: ir.skins?.length ?? 0,
    animations: (ir.animations ?? []).length,
    animNames,
    timelines: (ir.animations ?? []).reduce((a, x) => a + (x.timelines?.length ?? 0), 0),
    ik: (ir.ik ?? []).length,
    transform: (ir.transform ?? []).length,
    pathConstraints: (ir.path ?? []).length,
    events: (ir.events ?? []).length,
    regions, meshes, weightedMeshes: weighted, otherAttachments: other,
    deformTimelines: deform, ikTimelines: ikTl, eventTimelines: events,
    boneTimelines: boneTl, slotTimelines: slotTl, drawOrderTimelines: drawOrder,
    maxMeshVerts: maxVerts,
  };
}

const files = collect(CORPUS_ROOT).slice(0, LIMIT);
console.log(`语料根：${CORPUS_ROOT}`);
console.log(`发现 ${files.length} 个 *.skel.bytes，开始只读扫描（含 SHA-256 与解析统计）…`);
const t0 = Date.now();

const entries = [];
const failures = [];
for (let i = 0; i < files.length; i++) {
  const rel = files[i];
  const abs = path.join(CORPUS_ROOT, rel);
  const size = fs.statSync(abs).size;
  const sha256 = crypto.createHash('sha256').update(fs.readFileSync(abs)).digest('hex');

  const e = { path: rel, size, sha256, dir: path.posix.dirname(rel), base: path.posix.basename(rel) };
  // 旧 manifest 的 963 口径：只数「顶层目录内、文件名与目录名相同」的单位
  // （见 converter/src/corpus.ts:57-66 findSkelBytesFiles）。打上标记，便于两种口径对照。
  e.isPrimary = /^([^/]+)\/\1\.skel\.bytes$/.test(rel);

  // 文件头：sdorica 3.2.01 私有分支是「hash 字符串 + version 字符串」开头
  // （见 binary32.ts:90-91）。这里只做原始字节留档，版本号以解析结果为准。
  const buf = fs.readFileSync(abs);
  const head = buf.slice(0, 16).toString('latin1').replace(/[^\x20-\x7e]/g, '');
  e.header = head;

  // 伴随文件：3.x 用 <base>.atlas.txt 与 <base>.png
  const baseNoExt = abs.replace(/\.skel\.bytes$/i, '');
  e.hasAtlas = fs.existsSync(baseNoExt + '.atlas.txt');
  e.hasAtlasAlt = !e.hasAtlas && fs.existsSync(baseNoExt + '.atlas');
  e.hasPng = fs.existsSync(baseNoExt + '.png');

  try {
    const log = new IssueLog();
    const ir = readSkeletonBinary32(buf, log, { strictEof: true });
    Object.assign(e, profile(ir));
    e.parse = 'ok';
    e.readerWarnings = (log.warnings ?? []).length;
  } catch (err) {
    e.parse = 'fail';
    e.parseError = String(err.message ?? err).slice(0, 200);
    failures.push({ path: rel, sha256, size, error: e.parseError });
  }
  entries.push(e);
  if ((i + 1) % 100 === 0) console.log(`  … ${i + 1}/${files.length}`);
}

// ---- 口径分析：963 vs 1113 ----
const byHash = new Map();
for (const e of entries) {
  if (e.parse !== 'ok') continue;
  const k = e.sha256;
  if (!byHash.has(k)) byHash.set(k, []);
  byHash.get(k).push(e);
}
const dupGroups = [...byHash.entries()].filter(([, v]) => v.length > 1);
// 逻辑资产 = 顶层目录名（变体在子目录里，如 <name>/a2/<name>_a2.skel.bytes）
const byTopDir = new Map();
for (const e of entries) {
  const top = e.path.split('/')[0];
  byTopDir.set(top, (byTopDir.get(top) ?? 0) + 1);
}
const topDirs = [...byTopDir.keys()].sort();

const totals = {
  files: entries.length,
  parsedOk: entries.filter((e) => e.parse === 'ok').length,
  parseFail: entries.filter((e) => e.parse === 'fail').length,
  primaryUnits: entries.filter((e) => e.isPrimary).length,
  variantFiles: entries.filter((e) => !e.isPrimary).length,
  withAtlas: entries.filter((e) => e.hasAtlas).length,
  withAtlasAlt: entries.filter((e) => e.hasAtlasAlt).length,
  withPng: entries.filter((e) => e.hasPng).length,
  completeTrio: entries.filter((e) => e.hasAtlas && e.hasPng).length,
  uniqueSha256: byHash.size,
  duplicateFiles: entries.length - byHash.size,
  duplicateGroups: dupGroups.length,
  topLevelDirs: topDirs.length,
  versions: [...new Set(entries.map((e) => e.version))].sort(),
};
for (const k of ['bones', 'slots', 'skins', 'animations', 'ik', 'transform', 'pathConstraints', 'events',
  'timelines', 'regions', 'meshes', 'weightedMeshes', 'otherAttachments',
  'deformTimelines', 'ikTimelines', 'eventTimelines', 'boneTimelines',
  'slotTimelines', 'drawOrderTimelines']) {
  totals[k] = entries.reduce((a, e) => a + (e[k] ?? 0), 0);
}

// 「primary 口径」= 只数 <dir>/<dir>.skel.bytes（旧 manifest 的 963 枚举规则），
// 用于和 converter/test/fixtures/v32/corpus-manifest.json 的 963 直接对照。
const primaryEntries = entries.filter((e) => e.isPrimary && e.parse === 'ok');
const primaryTotals = { files: primaryEntries.length };
for (const k of ['bones', 'slots', 'skins', 'animations', 'ik', 'transform', 'pathConstraints', 'events',
  'timelines', 'regions', 'meshes', 'weightedMeshes', 'otherAttachments',
  'deformTimelines', 'ikTimelines', 'eventTimelines', 'boneTimelines',
  'slotTimelines', 'drawOrderTimelines']) {
  primaryTotals[k] = primaryEntries.reduce((a, e) => a + (e[k] ?? 0), 0);
}
const LEGACY_MANIFEST = {
  files: 963, bones: 65171, slots: 38821, skins: 963, animations: 15067,
  ik: 2481, transform: 160, timelines: 4676323, meshes: 18325, weightedMeshes: 15751,
};
// 旧 manifest 用的键名 → 本次扫描的键名（仅 weighted 一项不同）
const LEGACY_KEY_MAP = { weighted: 'weightedMeshes' };
const legacyCompare = {};
for (const [legacyKey, legacyVal] of Object.entries(LEGACY_MANIFEST)) {
  const mine = primaryTotals[LEGACY_KEY_MAP[legacyKey] ?? legacyKey];
  legacyCompare[legacyKey] = {
    legacy: legacyVal, primaryScan: mine, delta: mine - legacyVal,
  };
}
console.log('\n=== primary 口径 vs 旧 manifest（963）===');
for (const [k, v] of Object.entries(legacyCompare))
  console.log(`  ${k.padEnd(12)} legacy=${String(v.legacy).padStart(9)}  primaryScan=${String(v.primaryScan).padStart(9)}  Δ=${v.delta}`);

const manifest = {
  _note: 'Loongbones Phase 0 · Sdorica Spine 3.2.01 语料只读清单。仅含相对路径、hash 与解析统计，不含任何游戏资源本体。',
  generatedBy: 'tools/corpus-scan/scan.mjs',
  generator: 'loongbones-phase0-corpus-scan',
  corpusRootRef: 'LOONGBONES_CORPUS_ROOT（本地配置/环境变量，不写入本文件）',
  pathsAreRelativeTo: 'corpusRootRef',
  readerNote: '解析用旧项目 converter/src/read/binary32.ts（经 963 真实文件验证）；产品读取器属 Phase 1/2。',
  primaryTotals,
  legacyManifestCompare: legacyCompare,
  totals,
  entries,
};

fs.mkdirSync(OUT_DIR, { recursive: true });
fs.writeFileSync(path.join(OUT_DIR, 'corpus-manifest.json'), JSON.stringify(manifest, null, 1) + '\n');
fs.writeFileSync(path.join(OUT_DIR, 'corpus-failures.json'), JSON.stringify(failures, null, 1) + '\n');

// ---- 人读摘要 ----
const md = [];
md.push('# Sdorica Spine 3.2.01 语料清单（Loongbones Phase 0）');
md.push('');
md.push(`- 生成工具：\`tools/corpus-scan/scan.mjs\`（用法见同目录 \`README.md\`）`);
md.push(`- 语料根：由 \`LOONGBONES_CORPUS_ROOT\` / 命令行参数提供，**不写入本文件**`);
md.push(`- 扫描耗时：${((Date.now() - t0) / 1000).toFixed(1)} s`);
md.push('');
md.push('## 总量');
md.push('');
md.push('| 指标 | 值 |');
md.push('|---|---|');
for (const [k, v] of Object.entries(totals)) md.push(`| ${k} | ${v} |`);
md.push('');
md.push('## 963 vs 1113 口径差异');
md.push('');
md.push(`- 磁盘 \`*.skel.bytes\` 文件数：**${totals.files}**`);
md.push(`- 去重后唯一 SHA-256：**${totals.uniqueSha256}**（重复文件 ${totals.duplicateFiles} 个，涉及 ${totals.duplicateGroups} 组）`);
md.push(`- 顶层目录数（按目录名归并变体）：**${totals.topLevelDirs}**`);
md.push(`- manifest 记录的 \`files\`：963`);
md.push('');
md.push('判据见报告正文；本文件只列可复现数字。');
md.push('');
md.push('## 版本头分布');
md.push('');
for (const v of totals.versions) md.push(`- \`${v}\``);
md.push('');
md.push('## 伴随文件');
md.push('');
md.push(`- \`.atlas.txt\` 齐全：${totals.withAtlas} / ${totals.files}`);
md.push(`- 仅 \`.atlas\`（旧命名）：${totals.withAtlasAlt}`);
md.push(`- \`.png\` 齐全：${totals.withPng} / ${totals.files}`);
md.push(`- 三件套齐全：${totals.completeTrio} / ${totals.files}`);
md.push('');
md.push('## 解析失败');
md.push('');
md.push(failures.length === 0 ? '无（strictEof 全部通过，未发现截断/损坏）。' : `${failures.length} 个，明细见 \`corpus-failures.json\`。`);
md.push('');
fs.writeFileSync(path.join(OUT_DIR, 'corpus-manifest.md'), md.join('\n'));

console.log(`\n完成：${totals.files} 个文件，解析成功 ${totals.parsedOk}，失败 ${totals.parseFail}`);
console.log(`唯一 SHA-256：${totals.uniqueSha256}｜顶层目录：${totals.topLevelDirs}｜版本：${totals.versions.join(', ')}`);
console.log(`输出目录：${OUT_DIR}`);
