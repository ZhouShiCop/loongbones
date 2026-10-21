#!/usr/bin/env node
/**
 * Loongbones · Task 4 阶段 A — 图集页「坐标空间」判定探针
 *
 * 要回答的问题（DECISIONS.md V2 / U12 的证据缺口）：
 *   `.atlas.txt` 的页头写了 `size: W,H`，而伴随 PNG 的 IHDR 尺寸**普遍更大**
 *   （探针实测 1107/1111 不一致，且 PNG 恒为 2 的幂）。那么 region 的
 *   `xy`/`size` 到底是哪个空间的坐标？
 *
 * 两个互斥假设：
 *   H1「补齐」：PNG 是打包内容 + 右侧/下方 padding 补到 2 的幂
 *       → 非透明像素的外接矩形 ≈ 页声明 size；region 坐标无需缩放。
 *   H2「缩放」：PNG 是更高分辨率的同一张图，图集按缩小后的尺寸打包
 *       → 非透明像素铺满整个 PNG；region 坐标需乘 pngW/pageW。
 *
 * 判据（只用 alpha，不解释图像内容、不输出图像）：
 *   计算 PNG 中**非透明像素的外接矩形** (bboxW, bboxH)。
 *   bbox ≈ 页声明 size（误差 < 1%）→ 支持 H1；
 *   bbox ≈ PNG 尺寸（误差 < 1%）      → 支持 H2；
 *   都不符 → 记为 `inconclusive`，不猜测。
 *
 * 纪律：只读；不解码为图像文件、不写出任何像素、不复制游戏资产。
 *       输出只有路径（相对）+ 尺寸数值 + 判定类别。
 *
 * 用法：
 *   node tools/skeleton-probe/png-page-probe.mjs <语料根目录> [--limit N] [-o out]
 */
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const OLD_ROOT = path.resolve(HERE, '../../..');
const toUrl = (p) => 'file:///' + p.replace(/\\/g, '/');

const args = process.argv.slice(2);
const flag = (n) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : undefined; };
const POSITIONAL = args.find((a) => !a.startsWith('-'));
const CORPUS_ROOT = path.resolve(flag('-r') ?? POSITIONAL ?? process.env['LOONGBONES_CORPUS_ROOT'] ?? '');
const OUT_DIR = path.resolve(flag('-o') ?? path.join(HERE, 'out'));
const LIMIT = Number(flag('--limit') ?? 40) || 40;

if (!CORPUS_ROOT || !fs.existsSync(CORPUS_ROOT)) {
  console.error('用法：node tools/skeleton-probe/png-page-probe.mjs <语料根目录> [--limit N] [-o out]');
  process.exit(2);
}
const { parseAtlas } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/atlas.ts')));

// ---------------------------------------------------------------------------
// 极简 PNG 解码：只支持 8 位、非隔行（够用于本判据；其它一律记 unsupported）
// ---------------------------------------------------------------------------

/**
 * @param boxW/boxH 若给出，额外统计「落在左上角 boxW×boxH 内」的不透明像素数。
 *   H1（padding）下该比例应≈1；H2（缩放）下应≈(box/png)²。
 */
function decodePngAlpha(abs, boxW = 0, boxH = 0) {
  const buf = fs.readFileSync(abs);
  if (buf.length < 8 || buf.subarray(0, 8).toString('latin1') !== '\x89PNG\r\n\x1a\n') {
    return { ok: false, reason: 'not-png' };
  }
  let off = 8;
  let ihdr = null;
  const idat = [];
  while (off + 8 <= buf.length) {
    const len = buf.readUInt32BE(off);
    const type = buf.toString('latin1', off + 4, off + 8);
    const data = buf.subarray(off + 8, off + 8 + len);
    if (type === 'IHDR') {
      ihdr = {
        width: data.readUInt32BE(0), height: data.readUInt32BE(4),
        bitDepth: data[8], colorType: data[9], interlace: data[12],
      };
    } else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    off += 12 + len;
  }
  if (!ihdr) return { ok: false, reason: 'no-ihdr' };
  if (ihdr.interlace !== 0) return { ok: false, reason: 'interlaced' };
  if (ihdr.bitDepth !== 8) return { ok: false, reason: `bitDepth=${ihdr.bitDepth}` };
  // 每像素字节数：0=灰 2=RGB 3=调色 4=灰+α 6=RGBA
  const channels = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[ihdr.colorType];
  if (!channels) return { ok: false, reason: `colorType=${ihdr.colorType}` };

  const raw = zlib.inflateSync(Buffer.concat(idat));
  const { width: w, height: h } = ihdr;
  const bpp = channels;
  const stride = w * bpp;
  if (raw.length !== (stride + 1) * h) return { ok: false, reason: 'idat-size-mismatch' };

  // 反滤波（PNG 规范 5 种），只保留 alpha 通道用于后续统计
  const cur = Buffer.alloc(stride);
  const prev = Buffer.alloc(stride);
  let minX = Infinity, minY = Infinity, maxX = -1, maxY = -1;
  let opaque = 0;
  let insideBox = 0;
  const hasAlpha = ihdr.colorType === 6 || ihdr.colorType === 4 || ihdr.colorType === 3;
  if (ihdr.colorType === 3) return { ok: false, reason: 'palette-unsupported' };

  for (let y = 0; y < h; y++) {
    const ft = raw[y * (stride + 1)];
    raw.copy(cur, 0, y * (stride + 1) + 1, y * (stride + 1) + 1 + stride);
    for (let i = 0; i < stride; i++) {
      const a = i >= bpp ? cur[i - bpp] : 0;
      const b = prev[i];
      const c = i >= bpp ? prev[i - bpp] : 0;
      let v = cur[i];
      if (ft === 1) v += a;
      else if (ft === 2) v += b;
      else if (ft === 3) v += (a + b) >> 1;
      else if (ft === 4) {
        const p = a + b - c;
        const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
        v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
      }
      cur[i] = v & 0xff;
    }
    cur.copy(prev);
    // alpha 采样：每 2 像素取一次（足够判定外接矩形与框内占比，省时）
    for (let x = 0; x < w; x += 2) {
      const alpha = hasAlpha ? cur[x * bpp + bpp - 1] : 255;
      if (alpha > 8) {
        opaque++;
        if (boxW > 0 && x < boxW && y < boxH) insideBox++;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
    }
  }
  if (maxX < 0) return { ok: true, width: w, height: h, bbox: null, opaque: 0, insideBox: 0 };
  return {
    ok: true, width: w, height: h, opaque, insideBox,
    bbox: { x: minX, y: minY, w: maxX - minX + 1, h: maxY - minY + 1 },
  };
}

// ---------------------------------------------------------------------------
// 主流程：挑「声明尺寸与 PNG 尺寸差异最大」的页来判定（差异越大越能区分 H1/H2）
// ---------------------------------------------------------------------------

/** 收集 (skel 相对路径, atlas 绝对路径)。 */
function collectAtlases(root) {
  const out = [];
  const walk = (dir, depth) => {
    if (depth > 12) return;
    let es;
    try { es = fs.readdirSync(dir, { withFileTypes: true }); } catch { return; }
    es.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
    for (const e of es) {
      const p = path.join(dir, e.name);
      if (e.isDirectory()) walk(p, depth + 1);
      else if (e.name.toLowerCase().endsWith('.atlas.txt')) out.push(p);
    }
  };
  walk(root, 0);
  return out;
}

const atlasFiles = collectAtlases(CORPUS_ROOT);
console.log(`发现 ${atlasFiles.length} 个 .atlas.txt，先挑差异最大的 ${LIMIT} 个做像素判据…`);

const cands = [];
for (const abs of atlasFiles) {
  const rel = path.relative(CORPUS_ROOT, abs).split(path.sep).join('/');
  let atlas;
  try { atlas = parseAtlas(fs.readFileSync(abs, 'utf8')); } catch { continue; }
  const p0 = atlas.pages?.[0];
  if (!p0?.size) continue;
  const pngAbs = path.join(path.dirname(abs), p0.file ?? '');
  if (!fs.existsSync(pngAbs)) continue;
  const gzip = fs.readFileSync(pngAbs);
  if (gzip.length < 24) continue;
  const pngW = gzip.readUInt32BE(16);
  const pngH = gzip.readUInt32BE(20);
  const ratio = pngW / p0.size[0];
  cands.push({ rel, pngAbs, page: p0.size, png: [pngW, pngH], ratio, regionCount: p0.regions?.length ?? 0 });
}
// 差异大的优先：能最干净地区分 H1 / H2
cands.sort((a, b) => b.ratio - a.ratio);
const picked = cands.slice(0, LIMIT);

const rows = [];
for (const c of picked) {
  const d = decodePngAlpha(c.pngAbs, c.page[0], c.page[1]);
  if (!d.ok) { rows.push({ ...c, verdict: 'unsupported', reason: d.reason }); continue; }
  if (!d.bbox) { rows.push({ ...c, verdict: 'empty-alpha' }); continue; }
  // 判据 1（外接矩形）：bbox 贴 PNG → H2；贴页声明尺寸 → H1
  const near = (a, b) => Math.abs(a - b) / b < 0.03;
  const fitPage = near(d.bbox.w, c.page[0]) && near(d.bbox.h, c.page[1]);
  const fitPng = near(d.bbox.w, c.png[0]) && near(d.bbox.h, c.png[1]);
  // 判据 2（框内占比，更稳健）：H1 下应≈1；H2 下应≈(page/png)²
  const insideRatio = d.opaque > 0 ? d.insideBox / d.opaque : 0;
  const expectH2 = (c.page[0] / c.png[0]) * (c.page[1] / c.png[1]);
  const vBox = fitPage ? 'H1-padding' : (fitPng ? 'H2-scaled' : 'inconclusive');
  const vInside = insideRatio > 0.9 ? 'H1-padding'
    : (Math.abs(insideRatio - expectH2) < 0.08 ? 'H2-scaled' : 'inconclusive');
  rows.push({
    rel: c.rel, page: c.page, png: c.png, ratio: Number(c.ratio.toFixed(4)),
    bbox: [d.bbox.w, d.bbox.h], bboxOrigin: [d.bbox.x, d.bbox.y],
    opaqueInsidePageBoxRatio: Number(insideRatio.toFixed(4)),
    expectIfH2: Number(expectH2.toFixed(4)),
    verdictByBbox: vBox, verdictByInsideRatio: vInside,
    verdict: vBox === vInside ? vBox : (vInside !== 'inconclusive' ? vInside : vBox),
  });
}

const tally = {};
for (const r of rows) tally[r.verdict] = (tally[r.verdict] ?? 0) + 1;
console.log('\n判定分布：', JSON.stringify(tally));
for (const r of rows.slice(0, 15)) {
  console.log(`  ${r.rel}  page=${r.page}  png=${r.png}  bbox=${r.bbox}  → ${r.verdict}`);
}

fs.mkdirSync(OUT_DIR, { recursive: true });
fs.writeFileSync(
  path.join(OUT_DIR, 'atlas-page-space-verdict.json'),
  JSON.stringify({
    _note: '只读像素判据：非透明像素外接矩形 vs 页声明尺寸 vs PNG 尺寸。不保存任何图像数据。',
    generatedBy: 'tools/skeleton-probe/png-page-probe.mjs',
    corpusRootRef: 'LOONGBONES_CORPUS_ROOT（不写入本文件）',
    tally, rows,
  }, null, 1) + '\n',
);
console.log(`\n输出：${path.join(OUT_DIR, 'atlas-page-space-verdict.json')}`);
