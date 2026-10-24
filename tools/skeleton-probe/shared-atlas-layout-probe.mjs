// Task 6 A1：共享图集布局的**只读**取证探针。
//
// 回答四个问题（Task6_Implementation_Plan.md §5 A1）：
//   1. 枚举语料里所有「骨架 ↔ 图集不同名」的单位，按布局形态分类；
//   2. 每种形态的数量（Full / Primary 双口径分别报告，D12 不混分母）；
//   3. 共享图集页引用 PNG 的位置与命名（确认共用时资源是否也共用）；
//   4. Q6-2（目录内多候选）与 Q6-3（图集在上级目录等其它布局）到底**有没有**语料样本。
//
// 只读语料：只输出统计量与**相对路径**，不复制语料、不写机器绝对路径（规范 §8.2 / §9.3）。
//
// 用法：node tools/skeleton-probe/shared-atlas-layout-probe.mjs "<语料根>" [-o out 目录]

import { readdirSync, readFileSync, statSync, mkdirSync, writeFileSync } from 'node:fs';
import { join, relative, dirname, basename, sep } from 'node:path';

const args = process.argv.slice(2);
const root = args.find((a) => !a.startsWith('-'));
if (!root) {
  console.error('用法：node shared-atlas-layout-probe.mjs "<语料根>" [-o out 目录]');
  process.exit(2);
}

function walk(dir, files, dirs) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    const st = statSync(p);
    if (st.isDirectory()) {
      dirs.push(p);
      walk(p, files, dirs);
    } else {
      files.push(p);
    }
  }
}

const allFiles = [];
const allDirs = [];
walk(root, allFiles, allDirs);

const SKEL = '.skel.bytes';
const ATLAS = '.atlas.txt';

const skels = allFiles.filter((f) => f.endsWith(SKEL));
const atlasesByDir = new Map();      // dir -> [abs atlas paths]
for (const f of allFiles.filter((f) => f.endsWith(ATLAS))) {
  const d = dirname(f);
  if (!atlasesByDir.has(d)) atlasesByDir.set(d, []);
  atlasesByDir.get(d).push(f);
}
for (const list of atlasesByDir.values()) list.sort();

const rel = (p) => relative(root, p).split(sep).join('/');
const stemOf = (p) => basename(p).slice(0, -SKEL.length);

/// Primary 口径：与既有探针 `mesh-constraint-probe.mjs` **逐字一致** ——
/// `/^([^/]+)\/\1\.skel\.bytes$/`，即**只认第一级** `<dir>/<dir>.skel.bytes`（963）。
/// （不限层级的写法会得到 993，属**不同分母**，D12 禁止混用 ⇒ 沿用既有定义。）
function isPrimary(p) {
  return /^([^/]+)\/\1\.skel\.bytes$/.test(rel(p));
}

function classify(skelPath) {
  const dir = dirname(skelPath);
  const stem = stemOf(skelPath);
  const own = join(dir, stem + ATLAS);
  const inDir = (atlasesByDir.get(dir) ?? []).slice();

  if (inDir.includes(own)) {
    return { kind: '骨架同名命中', atlas: own, candidates: inDir.length };
  }
  if (inDir.length === 0) {
    // 目录内没有 ⇒ 看上级目录
    const parent = dirname(dir);
    const inParent = (atlasesByDir.get(parent) ?? []).slice();
    const parentOwn = join(parent, stem + ATLAS);
    if (inParent.includes(parentOwn)) {
      return { kind: '上级目录同名', atlas: parentOwn, candidates: inParent.length };
    }
    if (inParent.length === 1) {
      return { kind: '上级目录唯一候选', atlas: inParent[0], candidates: 1 };
    }
    if (inParent.length > 1) {
      return { kind: '上级目录多候选（歧义）', atlas: null, candidates: inParent.length };
    }
    return { kind: '无图集', atlas: null, candidates: 0 };
  }
  if (inDir.length === 1) {
    const only = inDir[0];
    const dirName = basename(dir);
    const onlyStem = basename(only).slice(0, -ATLAS.length);
    const sub = onlyStem === dirName ? '与目录同名'
        : onlyStem.endsWith('_' + dirName) ? '前缀_目录名'
        : '与其它骨架同名';
    return { kind: '目录内唯一候选：' + sub, atlas: only, candidates: 1 };
  }
  const dirName = basename(dir);
  const dirNamed = inDir.filter((a) => {
    const s = basename(a).slice(0, -ATLAS.length);
    return s === dirName || s.endsWith('_' + dirName);
  });
  if (dirNamed.length === 1) {
    return { kind: '目录内多候选中唯一与目录同名', atlas: dirNamed[0], candidates: inDir.length };
  }
  return { kind: '目录内多候选（歧义）', atlas: null, candidates: inDir.length };
}

function scan(list, label) {
  const byKind = new Map();
  const samples = [];
  for (const s of list) {
    const c = classify(s);
    byKind.set(c.kind, (byKind.get(c.kind) ?? 0) + 1);
    if (c.kind !== '骨架同名命中' && samples.length < 40) {
      samples.push({
        skeleton: rel(s),
        kind: c.kind,
        atlas: c.atlas ? rel(c.atlas) : null,
        candidatesInDir: c.candidates,
      });
    }
  }
  const kinds = {};
  for (const [k, v] of [...byKind.entries()].sort()) kinds[k] = v;
  return { 口径: label, 单位数: list.length, 分类: kinds, 非同名样本: samples };
}

const full = scan(skels, 'Full');
const primary = scan(skels.filter(isPrimary), 'Primary');

/// 候选存在性口径（2026-10-24 复核 P1 补）。
///
/// `classify()` 先判「骨架同名」并立即返回 ⇒ 多图集目录里的骨架**永远进不了**「多候选」分支，
/// 上级目录也只在「目录内 0 张」时才查。所以上面的分类只统计**实际解析路径**，
/// **不能**用来回答「语料里有没有这类候选」。这里单独统计**候选存在性**：
///   - `dir` 候选数分布（每个骨架所在目录有几张图集）
///   - 多图集目录里的骨架：多少靠同名消解、多少是真歧义
///   - 父目录含图集的骨架：同名可解 / 唯一候选 / 多候选
function candidateCensus(list) {
  const dirAtlasCount = new Map();   // dir(骨架) -> 该目录图集数
  const multi = { 骨架数: 0, 同名可解: 0, 真歧义: 0, 明细: [] };
  const parent = { 父目录含图集_目录数: 0, 骨架数: 0, 同名可解: 0, 唯一候选: 0, 多候选: 0, 明细: [] };
  const parentDirs = new Set();

  for (const s of list) {
    const dir = dirname(s);
    const stem = stemOf(s);
    const inDir = atlasesByDir.get(dir) ?? [];
    const n = inDir.length;
    dirAtlasCount.set(n, (dirAtlasCount.get(n) ?? 0) + 1);

    if (n > 1) {
      // 多图集目录：同名命中才算唯一可解；否则是真歧义（"唯一候选"规则在这里不适用）
      multi.骨架数 += 1;
      const own = join(dir, stem + ATLAS);
      if (inDir.includes(own)) multi.同名可解 += 1;
      else {
        multi.真歧义 += 1;
        if (multi.明细.length < 20) {
          multi.明细.push({ 骨架: rel(s), 目录图集数: n, 候选: inDir.map(rel) });
        }
      }
    }

    const p = dirname(dir);
    const inParent = atlasesByDir.get(p) ?? [];
    if (inParent.length > 0) {
      parentDirs.add(p);
      parent.骨架数 += 1;
      const parentOwn = join(p, stem + ATLAS);
      if (inParent.includes(parentOwn)) parent.同名可解 += 1;
      else if (inParent.length === 1) parent.唯一候选 += 1;
      else parent.多候选 += 1;
      if (parent.明细.length < 20) {
        parent.明细.push({ 骨架: rel(s), 父目录图集数: inParent.length });
      }
    }
  }

  const dirDist = {};
  for (const [k, v] of [...dirAtlasCount.entries()].sort((a, b) => a[0] - b[0])) {
    dirDist[`目录内图集数=${k}`] = v;
  }
  parent.父目录含图集_目录数 = parentDirs.size;
  return {
    说明: '候选存在性口径：回答"语料里有没有这类候选"，与上面"解析路径"口径**不同**，不得混读',
    骨架所在目录的图集数分布: dirDist,
    多图集目录: {
      骨架数: multi.骨架数,
      同名可解: multi.同名可解,
      真歧义: multi.真歧义,
      明细: multi.明细,
    },
    父目录候选: {
      父目录含图集_目录数: parent.父目录含图集_目录数,
      骨架数: parent.骨架数,
      同名可解: parent.同名可解,
      唯一候选: parent.唯一候选,
      多候选: parent.多候选,
      明细: parent.明细,
    },
  };
}

// 共享图集的 PNG 引用（只取「非骨架同名命中」的那些图集，读其页名）
const sharedAtlases = new Set();
for (const s of skels) {
  const c = classify(s);
  if (c.kind !== '骨架同名命中' && c.atlas) sharedAtlases.add(c.atlas);
}
const atlasPages = [];
for (const a of [...sharedAtlases].sort()) {
  let text = '';
  try {
    text = readFileSync(a, 'utf8');
  } catch {
    continue;
  }
  const pages = [];
  // .atlas.txt 的页文件行：紧随其后的第一个非空行就是 PNG 文件名
  const lines = text.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    const t = lines[i].trim();
    if (t.endsWith('.png')) {
      pages.push({ 页文件: t, 存在: allFiles.includes(join(dirname(a), t)) });
      i += 1;  // 跳过尺寸/格式/过滤等后续行，直到下一个页名
    }
  }
  const consumers = skels.filter((s) => {
    const c = classify(s);
    return c.atlas === a;
  }).map(rel);
  atlasPages.push({
    图集: rel(a),
    页数: pages.length,
    页: pages.slice(0, 8),
    被引用的骨架: consumers,
    共享单位数: consumers.length,
  });
}

const result = {
  语料根: '（不写入绝对路径，规范 §8.2）',
  文件: { skel: skels.length, atlas: allFiles.filter((f) => f.endsWith(ATLAS)).length },
  口径: [full, primary],
  候选存在性: { Full: candidateCensus(skels), Primary: candidateCensus(skels.filter(isPrimary)) },
  共享图集: atlasPages,
};

console.log(JSON.stringify(result, null, 2));

const outDir = args.includes('-o') ? args[args.indexOf('-o') + 1] : null;
if (outDir) {
  mkdirSync(outDir, { recursive: true });
  writeFileSync(join(outDir, 'shared-atlas-layout-probe.json'), JSON.stringify(result, null, 2), 'utf8');
}
process.exit(0);
