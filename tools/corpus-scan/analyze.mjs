#!/usr/bin/env node
/**
 * Loongbones · Phase 0 — 语料口径分析（963 vs 1113）
 *
 * 读 corpus-scan 产出的 manifest，按多种口径解释数量差异，并输出候选样本集。
 * 只读 manifest，不碰原始语料。
 *
 * 用法：node tools/corpus-scan/analyze.mjs [manifest 路径] [-o 输出目录]
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const flag = (n) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : undefined; };
const MANIFEST = path.resolve(flag('-m') ?? path.join(HERE, 'out/corpus-manifest.json'));
const OUT = path.resolve(flag('-o') ?? HERE);

const m = JSON.parse(fs.readFileSync(MANIFEST, 'utf8'));
const E = m.entries.filter((e) => e.parse === 'ok');
const T = m.totals;
const out = [];
const say = (s) => { out.push(s); console.log(s); };

say('# 963 vs 1113 口径分析');
say('');
say(`磁盘 \`*.skel.bytes\`：**${T.files}**｜manifest 记录 \`files\`：**963**`);
say('');

// --- 口径 1：唯一 SHA-256 ---
const byHash = new Map();
for (const e of E) (byHash.get(e.sha256) ?? byHash.set(e.sha256, []).get(e.sha256)).push(e);
const dupGroups = [...byHash.entries()].filter(([, v]) => v.length > 1);
say(`## 口径 A：唯一 SHA-256 = ${byHash.size}`);
say(`- 完全相同的重复文件：${T.files - byHash.size} 个，分布在 ${dupGroups.length} 组`);
say(`- 解释力：963 ≈ ${byHash.size}？${byHash.size === 963 ? '**完全吻合**' : `不吻合（差 ${byHash.size - 963}）`}`);
say('');

// --- 口径 2：顶层目录 ---
const byTop = new Map();
for (const e of E) {
  const top = e.path.split('/')[0];
  (byTop.get(top) ?? byTop.set(top, []).get(top)).push(e);
}
say(`## 口径 B：顶层目录数 = ${byTop.size}`);
say(`- 解释力：963 ≈ ${byTop.size}？${byTop.size === 963 ? '**完全吻合**' : `不吻合（差 ${byTop.size - 963}）`}`);
say('');

// --- 口径 3：变体子目录 ---
const multi = [...byTop.entries()].filter(([, v]) => v.length > 1);
say(`## 口径 C：含多个文件的顶层目录 = ${multi.length} 个`);
say('这些目录贡献了超出"一目录一文件"的差额：');
for (const [top, v] of multi.slice(0, 25)) {
  say(`- \`${top}\` × ${v.length}：${v.map((x) => x.base).join(', ')}`);
}
if (multi.length > 25) say(`- …其余 ${multi.length - 25} 个目录同理`);
say('');

// --- 口径 4：逻辑名（去掉变体后缀）---
const logical = new Set();
for (const e of E) logical.add(e.path.replace(/_a\d+\.skel\.bytes$/i, '.skel.bytes'));
say(`## 口径 D：按"去掉 _aN 变体后缀"归并的逻辑名 = ${logical.size}`);
say(`- 解释力：963 ≈ ${logical.size}？${logical.size === 963 ? '**完全吻合**' : `不吻合（差 ${logical.size - 963}）`}`);
say('');

// --- 口径 5：根目录散落文件 ---
const rootFiles = E.filter((e) => !e.path.includes('/'));
say(`## 口径 E：直接放在语料根下的文件 = ${rootFiles.length}`);
if (rootFiles.length) say(rootFiles.slice(0, 20).map((e) => `- \`${e.path}\``).join('\n'));
say('');

// --- 伴随文件 ---
say('## 伴随文件');
say(`- \`.atlas.txt\`：${T.withAtlas}/${T.files}`);
say(`- 仅 \`.atlas\`（旧命名）：${T.withAtlasAlt}`);
say(`- \`.png\`：${T.withPng}/${T.files}`);
say(`- 三件套齐全：${T.completeTrio}/${T.files}`);
say('');

// --- 版本头 ---
const verCount = {};
for (const e of E) verCount[e.version ?? '(无)'] = (verCount[e.version ?? '(无)'] ?? 0) + 1;
say('## 版本头分布');
for (const [v, n] of Object.entries(verCount).sort()) say(`- \`${v}\`：${n}`);
say('');

// --- 汇总表 ---
say('## 汇总：哪个口径等于 963');
say('');
say('| 口径 | 数值 | 是否 = 963 |');
say('|---|---|---|');
say(`| 磁盘文件数 | ${T.files} | ${T.files === 963 ? '是' : '否'} |`);
say(`| 唯一 SHA-256 | ${byHash.size} | ${byHash.size === 963 ? '**是**' : '否'} |`);
say(`| 顶层目录数 | ${byTop.size} | ${byTop.size === 963 ? '**是**' : '否'} |`);
say(`| 逻辑名（去 _aN） | ${logical.size} | ${logical.size === 963 ? '**是**' : '否'} |`);

fs.writeFileSync(path.join(OUT, 'corpus-reconciliation.md'), out.join('\n') + '\n');
console.log(`\n写出：${path.join(OUT, 'corpus-reconciliation.md')}`);
