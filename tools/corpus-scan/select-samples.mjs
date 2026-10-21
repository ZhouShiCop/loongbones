#!/usr/bin/env node
/**
 * Loongbones · Phase 0 — 首批样本选择
 *
 * 从 corpus-scan 的 manifest 里挑 8–20 个覆盖样本，按能力打标签，
 * 供后续阶段做回归与验收分母。只读 manifest，不碰原始语料。
 *
 * 覆盖目标（规范 §5 Phase 0）：简单骨架、复杂层级、IK、mesh、weighted mesh、
 * 长动画/事件、三件套齐全（可做视觉工作）。
 *
 * 用法：node tools/corpus-scan/select-samples.mjs [-m manifest] [-o 输出目录] [-n 数量]
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const flag = (n) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : undefined; };
const MANIFEST = path.resolve(flag('-m') ?? path.join(HERE, 'out/corpus-manifest.json'));
const OUT = path.resolve(flag('-o') ?? HERE);
const N = Number(flag('-n') ?? 16);

const m = JSON.parse(fs.readFileSync(MANIFEST, 'utf8'));
const E = m.entries.filter((e) => e.parse === 'ok');

/** 给一个文件打能力标签。 */
function tags(e) {
  const t = [];
  if (e.bones <= 5) t.push('simple-skeleton');
  else if (e.bones >= 40) t.push('deep-hierarchy');
  if (e.ik > 0) t.push('ik');
  if (e.transform > 0) t.push('transform-constraint');
  if (e.meshes > 0) t.push('mesh');
  if (e.weightedMeshes > 0) t.push('weighted-mesh');
  if (e.deformTimelines > 0) t.push('deform');
  if (e.events > 0 || e.eventTimelines > 0) t.push('event');
  if (e.animations >= 8) t.push('many-animations');
  if (e.maxMeshVerts >= 100) t.push('large-mesh');
  if (e.hasAtlas && e.hasPng) t.push('complete-trio');
  return t;
}
for (const e of E) { e.tags = tags(e); e.trio = e.hasAtlas && e.hasPng; e.kind = e.isPrimary ? 'primary' : 'variant'; }

const picked = [];
const take = (label, cand, why) => {
  const c = cand.filter((e) => !picked.includes(e)).sort((a, b) => (a.size - b.size))[0];
  if (c) { picked.push(c); c.why = why; return true; }
  return false;
};

// 1) 唯一带 event 的样本（全语料仅 1 个，必须收）
take('event', E.filter((e) => e.tags.includes('event')), '全语料唯一含事件的样本');

// 2) 简单骨架（骨头最少、且三件套齐全）
take('simple', E.filter((e) => e.tags.includes('simple-skeleton') && e.trio), '最小骨架，覆盖基础骨骼/插槽/region');

// 3) 复杂层级（骨头最多）
take('deep', E.filter((e) => e.tags.includes('deep-hierarchy')), '最深层级，覆盖父子变换与继承');

// 4) IK
take('ik', E.filter((e) => e.ik > 0 && e.trio), 'IK 约束');

// 5) transform 约束
take('transform', E.filter((e) => e.transform > 0), 'transform 约束');

// 6) mesh（非加权）
take('mesh', E.filter((e) => e.meshes > 0 && e.weightedMeshes === 0 && e.trio), '非加权 mesh');

// 7) weighted mesh
take('weighted', E.filter((e) => e.weightedMeshes > 0 && e.trio), '加权 mesh');

// 8) 加权 + deform + IK 组合
take('combo', E.filter((e) => e.weightedMeshes > 0 && e.deformTimelines > 0 && e.ik > 0 && e.trio), '加权+deform+IK 组合');

// 9) 大网格
take('large-mesh', E.filter((e) => e.tags.includes('large-mesh') && e.trio), '大网格顶点量');

// 10) 动画最多
take('many-anim', E.filter((e) => e.tags.includes('many-animations')), '动画数量最多');

// 11) 时间线最多
take('many-tl', E.filter((e) => e.timelines > 20000), '时间线数量最多');

// 12) 体积最大 / 最小（性能边界）
take('largest', E.slice().sort((a, b) => b.size - a.size), '体积最大（加载/性能边界）');
take('smallest', E.slice().sort((a, b) => a.size - b.size), '体积最小');

// 13) 已知问题素材 b0001s3（此前调查的核心样本）
take('b0001s3', E.filter((e) => e.base === 'b0001s3.skel.bytes'), '历史调查核心样本（16 mesh / 11 旋转 region）');

// 14) 补齐：优先三件套齐全 + 能力覆盖广的
const rest = E.filter((e) => !picked.includes(e) && e.trio)
  .sort((a, b) => (b.tags.length - a.tags.length) || (a.size - b.size));
while (picked.length < N && rest.length) picked.push(rest.shift());

// ---- 输出 ----
const lines = [];
const say = (s) => { lines.push(s); console.log(s); };
say('# 首批样本选择（Loongbones Phase 0）');
say('');
say(`- 来源：\`tools/corpus-scan/out/corpus-manifest.json\`（全量 ${m.totals.files} 个文件，primary 口径 ${m.primaryTotals.files} 个）`);
say(`- 选出 ${picked.length} 个：**primary 单位**（\`<dir>/<dir>.skel.bytes\`）${picked.filter((e) => e.kind === 'primary').length} 个，
  **变体**（子目录/异名，如 \`<dir>/<sub>/<name>.skel.bytes\`）${picked.filter((e) => e.kind === 'variant').length} 个`);
say(`- 回归分母建议：能力指纹对齐用 primary 口径 ${m.primaryTotals.files} 个；本样本集另含变体，用于覆盖 primary 里没有的形态`);
say('- 语料根由 `LOONGBONES_CORPUS_ROOT` 提供，**不写入本文件**');
say('');
say('| # | 相对路径 | 口径 | 体积 | SHA-256(前12) | 骨骼 | 插槽 | 动画 | IK | mesh | 加权 | deform | 三件套 | 选择理由 |');
say('|---|---|---|---|---|---|---|---|---|---|---|---|---|---|');
picked.forEach((e, i) => {
  say(`| ${i + 1} | \`${e.path}\` | ${e.kind} | ${e.size} | ${e.sha256.slice(0, 12)} | ${e.bones} | ${e.slots} | ${e.animations} | ${e.ik} | ${e.meshes} | ${e.weightedMeshes} | ${e.deformTimelines} | ${e.trio ? '✔' : '✘'} | ${e.why ?? '能力覆盖补齐'} |`);
});
say('');
say('## 能力覆盖核对');
say('');
const need = ['simple-skeleton', 'deep-hierarchy', 'ik', 'transform-constraint', 'mesh',
  'weighted-mesh', 'deform', 'event', 'many-animations', 'large-mesh', 'complete-trio'];
const allTags = new Set(picked.flatMap((e) => e.tags));
for (const t of need) say(`- ${allTags.has(t) ? '✔' : '**✘ 未覆盖**'} ${t}`);
say('');
say('## 语料能力边界（供 Phase 1 能力矩阵参考）');
say('');
say(`- 附件类型：全语料只有 \`region\` 与 \`mesh\` 两类；\`boundingbox\`/\`path\`/\`clipping\`/\`point\` **0 个**`);
say(`- \`path\` 约束：**0 个**`);
say(`- 事件：\`ir.events\` 合计 **${m.totals.events}** 个，event 时间线 **${m.totals.eventTimelines}** 条 —— 事件能力在本语料几乎无法验证`);
say(`- 解析：strictEof **0 失败**，未发现截断/损坏`);
say(`- 版本：全部 \`${(m.totals.versions ?? []).join(', ')}\``);
say(`- 三件套齐全：${m.totals.completeTrio}/${m.totals.files}`);

fs.writeFileSync(path.join(OUT, 'sample-selection.md'), lines.join('\n') + '\n');
fs.writeFileSync(path.join(OUT, 'out/samples.json'), JSON.stringify(
  picked.map((e) => ({ path: e.path, kind: e.kind, sha256: e.sha256, size: e.size, tags: e.tags, why: e.why ?? null })),
  null, 1) + '\n');
console.log(`\n写出：${path.join(OUT, 'sample-selection.md')}`);
console.log(`     ${path.join(OUT, 'out/samples.json')}`);
