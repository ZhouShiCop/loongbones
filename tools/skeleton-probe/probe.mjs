#!/usr/bin/env node
/**
 * Loongbones · Task 4 阶段 A — Spine 3.2.01 骨架只读探针
 *
 * 目的：为「骨架 Domain / 原生格式 RFC」与「插件宿主 RFC」提供**可复现的数值证据**，
 * 而不是凭经验下结论（规范 §8.4；DECISIONS.md V1/V2/V6/V7/V8）。
 *
 * 它回答四类问题：
 *   1. 源数据基线：版本分布、三件套伴随关系、SHA-256、对象计数；
 *   2. 引用闭合：骨骼父索引、插槽骨骼索引、IK/transform 约束索引、skin 插槽索引、
 *      附件 path → 图集 region 的解析率；
 *   3. 待验证项的语料实证：附件 `path` 与附件名不一致率、`nonessential` 覆盖率、
 *      名称唯一性、`linked mesh` 出现率、图集 rotation/trim/缩放形态；
 *   4. 失败分类：strictEof（截断/损坏）、非法索引、非法浮点、未知附件类型。
 *
 * 纪律（规范 §8.2 / §9.3；Task 4 §5）：
 *   · **只读**语料：不移动、不改写、不删除、不复制任何游戏文件；
 *   · 输出**只有元数据与统计量**：相对路径 + hash + 计数 + 分类计数，
 *     不写盘符/用户名、不写图集或二进制原文、不写 PNG 像素；
 *   · 本工具是**开发期分析探针，不是产品读取器**（Task 4 §2.4）。产品读取器在阶段 B
 *     按批准的 schema 契约**独立实现**，不复用本探针、不引入 Spine Runtime。
 *
 * 用法见同目录 README.md。
 */
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
// tools/skeleton-probe → 上溯 3 级 = D:/Works/Spine（旧项目根，Reader 所在处）
const OLD_ROOT = path.resolve(HERE, '../../..');
const toUrl = (p) => 'file:///' + p.replace(/\\/g, '/');

const args = process.argv.slice(2);
const flag = (n) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : undefined; };
const has = (n) => args.includes(n);
// 位置参数（第一个不以 `-` 开头的）也可作为语料根，与 `-r` 等价
const POSITIONAL = args.find((a) => !a.startsWith('-'));
const CORPUS_ROOT = path.resolve(flag('-r') ?? POSITIONAL ?? process.env['LOONGBONES_CORPUS_ROOT'] ?? '');
const OUT_DIR = path.resolve(flag('-o') ?? path.join(HERE, 'out'));
const LIMIT = Number(flag('--limit') ?? 0) || Infinity;
const SAMPLE_LIST = flag('--samples'); // 可选：样本清单 json（相对路径数组）
const ONLY_SAMPLES = has('--only-samples');

if (!CORPUS_ROOT || !fs.existsSync(CORPUS_ROOT)) {
  console.error('用法：node tools/skeleton-probe/probe.mjs <语料根目录> [-o 输出目录] [--limit N]');
  console.error('或设置环境变量 LOONGBONES_CORPUS_ROOT');
  process.exit(2);
}

// ---- 复用旧项目已验证的 3.2.01 Reader（只作证据参照；产品读取器另写）----
const { readSkeletonBinary32 } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/read/binary32.ts')));
const { IssueLog } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/model.ts')));
const { parseAtlas } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/atlas.ts')));

// ---------------------------------------------------------------------------
// 收集与工具
// ---------------------------------------------------------------------------

/** 递归收集 *.skel.bytes，返回排序后的相对路径（保证可复现）。 */
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

/** 只读读 PNG 的 magic + IHDR 宽高（不解码像素、不复制图像）。 */
function pngIhdr(abs) {
  try {
    const fd = fs.openSync(abs, 'r');
    const head = Buffer.alloc(24);
    fs.readSync(fd, head, 0, 24, 0);
    fs.closeSync(fd);
    const magic = head.subarray(0, 8).toString('latin1');
    if (magic !== '\x89PNG\r\n\x1a\n') return { ok: false, reason: 'not-png' };
    if (head.toString('latin1', 12, 16) !== 'IHDR') return { ok: false, reason: 'no-ihdr' };
    return { ok: true, width: head.readUInt32BE(16), height: head.readUInt32BE(20) };
  } catch (err) {
    return { ok: false, reason: String(err.message ?? err) };
  }
}

/** 附件在 `.atlas` 里的查找键：Spine 规则是 `path ?? name`。 */
const atlasKey = (a) => a.path ?? a.name;

// ---------------------------------------------------------------------------
// 单文件分析
// ---------------------------------------------------------------------------

function analyze(rel, buf) {
  const r = {
    path: rel,
    size: buf.length,
    sha256: crypto.createHash('sha256').update(buf).digest('hex'),
    // primary 口径 = <dir>/<dir>.skel.bytes（与 corpus-scan 同口径，不混用分母）
    isPrimary: /^([^/]+)\/\1\.skel\.bytes$/.test(rel),
  };

  // ---- 伴随文件（只读 stat / 存在性）----
  const baseNoExt = path.join(CORPUS_ROOT, rel).replace(/\.skel\.bytes$/i, '');
  r.companion = {
    atlasTxt: fs.existsSync(baseNoExt + '.atlas.txt'),
    atlas: fs.existsSync(baseNoExt + '.atlas'),
    png: fs.existsSync(baseNoExt + '.png'),
  };
  r.trio = r.companion.atlasTxt && r.companion.png;
  // 伴随文件的**相对语料根路径**（manifest 必须可定位，不能只凭 hash 猜文件）
  r.atlasTxtRelPath = r.companion.atlasTxt
    ? path.relative(CORPUS_ROOT, baseNoExt + '.atlas.txt').replace(/\\/g, '/')
    : null;
  r.pngRelPaths = r.companion.png
    ? [path.relative(CORPUS_ROOT, baseNoExt + '.png').replace(/\\/g, '/')]
    : [];

  // ---- 伴随文件锁定（sha256 + 字节数）：manifest 需要能同时锁定 skeleton / atlas / png ----
  // 只算 hash 与大小，**不读像素、不复制文件、不写任何伴随文件内容**。
  const sha = (p) => (fs.existsSync(p) ? crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex') : null);
  const bytes = (p) => (fs.existsSync(p) ? fs.statSync(p).size : null);
  const atlasPath = baseNoExt + '.atlas.txt';
  const pngPath = baseNoExt + '.png';
  r.companionHashes = {
    atlasTxt: r.companion.atlasTxt ? { sha256: sha(atlasPath), bytes: bytes(atlasPath) } : null,
    png: r.companion.png ? { sha256: sha(pngPath), bytes: bytes(pngPath) } : null,
  };

  // ---- 骨架解析（strictEof 识别截断/损坏）----
  const log = new IssueLog();
  let ir;
  try {
    ir = readSkeletonBinary32(buf, log, { strictEof: true, validate: true });
    r.parse = 'ok';
  } catch (err) {
    r.parse = 'fail';
    r.parseError = String(err.message ?? err).slice(0, 300);
    // 失败分类：strictEof 报的是"剩余字节"，其余按关键字粗分
    const m = /(\d+)\s*字节/.exec(r.parseError);
    r.failClass = /剩余|EOF|字节/.test(r.parseError) ? 'truncated-or-misaligned' : 'parse-error';
    if (m) r.parseErrorTailBytes = Number(m[1]);
    return r;
  }
  // IssueLog 用 issues[{level,where,message}]，level ∈ info/warn/drop
  r.readerWarnings = (log.issues ?? []).filter((x) => x.level === 'warn').length;
  r.readerDrops = (log.issues ?? []).filter((x) => x.level === 'drop').length;

  const sk = ir.skeleton ?? {};
  r.version = sk.version ?? null;
  r.skeletonHash = sk.hash ?? null;
  r.width = sk.width ?? null;
  r.height = sk.height ?? null;
  r.nonessential = sk.nonessential === true;
  r.imagesPath = sk.images ?? null;

  // ---- 对象计数 ----
  r.counts = {
    bones: ir.bones?.length ?? 0,
    slots: ir.slots?.length ?? 0,
    skins: ir.skins?.length ?? 0,
    animations: ir.animations?.length ?? 0,
    timelines: (ir.animations ?? []).reduce((a, x) => a + (x.timelines?.length ?? 0), 0),
    ik: ir.ik?.length ?? 0,
    transform: ir.transform?.length ?? 0,
    path: ir.path?.length ?? 0,
    events: ir.events?.length ?? 0,
  };

  // ---- 时间线类型分布（决定动画能力的语料可验证性）----
  const tl = { bone: 0, slot: 0, deform: 0, ik: 0, drawOrder: 0, event: 0, other: 0 };
  for (const an of ir.animations ?? []) {
    for (const t of an.timelines ?? []) {
      if (t.kind === 'rotate' || t.kind === 'translate' || t.kind === 'scale' || t.kind === 'shear') tl.bone++;
      else if (t.kind === 'attachment' || t.kind === 'color') tl.slot++;
      else if (t.kind === 'deform') tl.deform++;
      else if (t.kind === 'ik') tl.ik++;
      else if (t.kind === 'drawOrder') tl.drawOrder++;
      else if (t.kind === 'event') tl.event++;
      else tl.other++;
    }
  }
  r.timelineKinds = tl;

  // ---- 引用闭合（每一项都给出越界计数，供 RFC 定校验规则）----
  const boneCount = r.counts.bones;
  const slotCount = r.counts.slots;
  const ref = {
    boneParentOutOfRange: 0, boneParentForward: 0, boneParentSelfCycle: 0, boneRootCount: 0,
    slotBoneOutOfRange: 0,
    ikBoneOutOfRange: 0, ikTargetOutOfRange: 0,
    transformBoneOutOfRange: 0, transformTargetOutOfRange: 0,
    skinSlotOutOfRange: 0,
    unresolvedBoneParentNames: 0, unresolvedSlotBoneNames: 0,
  };
  for (let i = 0; i < boneCount; i++) {
    const b = ir.bones[i];
    const p = b.parentIndex ?? -1;
    if (p < -1 || p >= boneCount) { ref.boneParentOutOfRange++; continue; }
    if (p === -1) { ref.boneRootCount++; continue; }
    if (p >= i) ref.boneParentForward++;          // 前向引用：Spine 不允许
    if (p === i) ref.boneParentSelfCycle++;
    if (!b.parent) ref.unresolvedBoneParentNames++;
  }
  for (const s of ir.slots ?? []) {
    const bi = s.boneIndex ?? -1;
    if (bi < 0 || bi >= boneCount) ref.slotBoneOutOfRange++;
    else if (!s.bone) ref.unresolvedSlotBoneNames++;
  }
  for (const c of ir.ik ?? []) {
    for (const bn of c.bones ?? []) if (bn === '?' || bn === undefined) ref.ikBoneOutOfRange++;
    if (c.target === '?' || c.target === undefined) ref.ikTargetOutOfRange++;
  }
  for (const c of ir.transform ?? []) {
    for (const bn of c.bones ?? []) if (bn === '?' || bn === undefined) ref.transformBoneOutOfRange++;
    if (c.target === '?' || c.target === undefined) ref.transformTargetOutOfRange++;
  }
  for (const skin of ir.skins ?? []) {
    // Reader 在 skin 段遇到非法 slotIndex 会直接抛错，能走到这里说明索引闭合
    // （此处只统计"该 skin 是否绑定了 0 个 slot"，用于判断空 skin 形态）
    if (Object.keys(skin.attachments ?? {}).length === 0) ref.skinSlotOutOfRange += 0;
  }
  r.reference = ref;
  r.referenceClosed = ref.boneParentOutOfRange === 0 && ref.boneParentForward === 0 &&
    ref.slotBoneOutOfRange === 0 && ref.ikBoneOutOfRange === 0 && ref.ikTargetOutOfRange === 0 &&
    ref.transformBoneOutOfRange === 0 && ref.transformTargetOutOfRange === 0 &&
    ref.unresolvedBoneParentNames === 0 && ref.unresolvedSlotBoneNames === 0;

  // ---- 名称唯一性（V6）----
  const dup = (names) => {
    const seen = new Set();
    let d = 0;
    for (const n of names) { if (seen.has(n)) d++; else seen.add(n); }
    return d;
  };
  r.nameDuplicates = {
    bones: dup((ir.bones ?? []).map((b) => b.name)),
    slots: dup((ir.slots ?? []).map((s) => s.name)),
    // 附件名在 (skin, slot) 内唯一即可；这里统计同 slot 内重名
    attachmentsPerSlot: 0,
  };

  // ---- 附件分类与 path 语义（U4 / V8）----
  const att = {
    region: 0, mesh: 0, weightedMesh: 0, linkedMesh: 0, boundingbox: 0, other: 0, total: 0,
    pathPresent: 0, pathDiffersFromName: 0, pathNull: 0,
    regionRotationNonZero: 0, regionScaleNotOne: 0,
    meshUvOutOfRange: 0, triangleIndexOutOfRange: 0,
  };
  for (const skin of ir.skins ?? []) {
    for (const slotName of Object.keys(skin.attachments ?? {})) {
      const names = Object.keys(skin.attachments[slotName]);
      r.nameDuplicates.attachmentsPerSlot += dup(names);
      for (const n of names) {
        const a = skin.attachments[slotName][n];
        att.total++;
        if (a.type === 'region') {
          att.region++;
          if (a.path !== undefined && a.path !== null) {
            att.pathPresent++;
            if (a.path !== a.name) att.pathDiffersFromName++;
          } else att.pathNull++;
          if ((a.rotation ?? 0) !== 0) att.regionRotationNonZero++;
          if ((a.scaleX ?? 1) !== 1 || (a.scaleY ?? 1) !== 1) att.regionScaleNotOne++;
        } else if (a.type === 'mesh') {
          if (a.weighted) att.weightedMesh++; else att.mesh++;
          if (a.parent !== undefined && a.parent !== null) att.linkedMesh++;
          if (a.path !== undefined && a.path !== null) {
            att.pathPresent++;
            if (a.path !== a.name) att.pathDiffersFromName++;
          } else att.pathNull++;
          const n2 = (a.uvs?.length ?? 0) / 2;
          for (const u of a.uvs ?? []) if (u < -1e-4 || u > 1 + 1e-4) { att.meshUvOutOfRange++; break; }
          for (const t of a.triangles ?? []) if (t < 0 || t >= n2) { att.triangleIndexOutOfRange++; break; }
        } else if (a.type === 'boundingbox') att.boundingbox++;
        else att.other++;
      }
    }
  }
  r.attachments = att;

  // ---- 图集解析与跨文件闭合（U12 / V2 证据）----
  const atlasAbs = fs.existsSync(baseNoExt + '.atlas.txt')
    ? baseNoExt + '.atlas.txt'
    : (fs.existsSync(baseNoExt + '.atlas') ? baseNoExt + '.atlas' : null);
  if (atlasAbs) {
    try {
      const text = fs.readFileSync(atlasAbs, 'utf8');
      const atlas = parseAtlas(text);
      // atlas.ts 的 IR：{ pages: [{ file, size:[w,h], format, filter, repeat, pma,
      //                             regions: [{ name, rotate?: string, xy, size, orig, offset, index }] }] }
      const regions = (atlas.pages ?? []).flatMap((p) => p.regions ?? []);
      const regionNames = new Set(regions.map((x) => x.name));
      const a = {
        pages: atlas.pages?.length ?? 0,
        regions: regions.length,
        rotateTrue: 0, rotateFalse: 0, rotateOther: 0,
        trimmed: 0,          // offset != 0
        sizeNeOrig: 0,       // size != orig（缩放或裁剪）
        indexNeMinusOne: 0,
        pngIhdrMatch: 0,
        pngIhdrMismatch: 0,
        pngMissing: 0,
        pageSizePresent: 0,
        pageSizeMissing: 0,
        pmaTrue: 0, pmaFalse: 0, pmaAbsent: 0,
        formats: {},
        regionOutOfPageDeclared: 0,
        maxExtentW: 0,
        maxExtentH: 0,
        regionLookupByPath: { hit: 0, miss: 0 },
        regionLookupByName: { hit: 0, miss: 0 },
        // 页明细：页名（图集页头声明的文件名）+ 解析出的 PNG 相对路径 + 摘要。
        // manifest 必须按页列出纹理，不能只凭 hash 猜文件（Back Task §4.3）。
        pageDetails: [],
      };
      for (const g of regions) {
        // rotate 在 libGDX 文本里可能是 `rotate: true` 或 `rotate: 90`
        if (g.rotate === 'true' || g.rotate === '90') a.rotateTrue++;
        else if (g.rotate === 'false') a.rotateFalse++;
        else if (g.rotate !== undefined) a.rotateOther++;
        const off = g.offset ?? [0, 0];
        if (off[0] !== 0 || off[1] !== 0) a.trimmed++;
        const sz = g.size;
        const og = g.orig ?? sz;
        if ((sz?.[0] ?? 0) !== (og?.[0] ?? 0) || (sz?.[1] ?? 0) !== (og?.[1] ?? 0)) a.sizeNeOrig++;
        if ((g.index ?? -1) !== -1) a.indexNeMinusOne++;
      }
      // 页文件名 → PNG 存在性 + IHDR 尺寸与页声明尺寸比对（只读前 24 字节）
      const pngSizes = new Map();
      for (const p of atlas.pages ?? []) {
        if (p.size) a.pageSizePresent++; else a.pageSizeMissing++;
        if (p.pma === true) a.pmaTrue++;
        else if (p.pma === false) a.pmaFalse++;
        else a.pmaAbsent++;
        if (p.format) a.formats[p.format] = (a.formats[p.format] ?? 0) + 1;
        const pngPath = path.join(path.dirname(atlasAbs), p.file ?? '');
        if (!fs.existsSync(pngPath)) { a.pngMissing++; continue; }
        const ih = pngIhdr(pngPath);
        if (!ih.ok) { a.pngIhdrMismatch++; continue; }
        pngSizes.set(p.file, ih);
        const ps = p.size;
        if (ps && ih.width === ps[0] && ih.height === ps[1]) a.pngIhdrMatch++;
        else a.pngIhdrMismatch++;
      }
      // region 是否越出「页声明尺寸」；rotate 时页内占位转置
      const page0 = (atlas.pages ?? [])[0];
      const pw = page0?.size?.[0];
      const ph = page0?.size?.[1];
      if (pw && ph) {
        for (const g of regions) {
          const xy = g.xy ?? [0, 0];
          const sz = g.size ?? [0, 0];
          const rot = g.rotate === 'true' || g.rotate === '90';
          const ow = rot ? sz[1] : sz[0];
          const oh = rot ? sz[0] : sz[1];
          if (xy[0] + ow > pw || xy[1] + oh > ph) a.regionOutOfPageDeclared++;
          a.maxExtentW = Math.max(a.maxExtentW, xy[0] + ow);
          a.maxExtentH = Math.max(a.maxExtentH, xy[1] + oh);
        }
      }
      // 页声明尺寸 vs PNG 实际尺寸的比例（V2「图集缩放承载层」的直接证据）
      if (pw && ph && page0?.file) {
        const ih = pngSizes.get(page0.file);
        if (ih?.ok) {
          a.pageSize = [pw, ph];
          a.pngSize = [ih.width, ih.height];
          a.pngOverPageRatio = [Number((ih.width / pw).toFixed(4)), Number((ih.height / ph).toFixed(4))];
        }
      }
      // 逐页明细：页名 → 相对语料根的 PNG 路径 + 摘要 + 声明尺寸 + region 数。
      // 路径由图集页头 `file` 解析（**不**由 skeleton 文件名猜），多页图集会全部列出。
      (atlas.pages ?? []).forEach((p, i) => {
        const pngAbs = path.join(path.dirname(atlasAbs), p.file ?? '');
        const rel = path.relative(CORPUS_ROOT, pngAbs).replace(/\\/g, '/');
        const exists = fs.existsSync(pngAbs);
        a.pageDetails.push({
          order: i,
          pageFile: p.file ?? null,
          pngRelPath: exists ? rel : null,
          pngBytes: exists ? fs.statSync(pngAbs).size : null,
          pngSha256: exists ? crypto.createHash('sha256').update(fs.readFileSync(pngAbs)).digest('hex') : null,
          declaredSize: p.size ?? null,
          regions: (p.regions ?? []).length,
        });
      });
      // 附件 → region 查找：Spine 用 path ?? name
      for (const skin of ir.skins ?? []) {
        for (const slotName of Object.keys(skin.attachments ?? {})) {
          for (const n of Object.keys(skin.attachments[slotName])) {
            const x = skin.attachments[slotName][n];
            if (x.type !== 'region' && x.type !== 'mesh') continue;
            if (regionNames.has(atlasKey(x))) a.regionLookupByPath.hit++;
            else a.regionLookupByPath.miss++;
            if (regionNames.has(x.name)) a.regionLookupByName.hit++;
            else a.regionLookupByName.miss++;
          }
        }
      }
      r.atlas = a;
    } catch (err) {
      r.atlas = { parseError: String(err.message ?? err).slice(0, 200) };
    }
  } else {
    r.atlas = { missing: true };
  }

  return r;
}

// ---------------------------------------------------------------------------
// 主流程
// ---------------------------------------------------------------------------

let files = collect(CORPUS_ROOT);
if (ONLY_SAMPLES && SAMPLE_LIST) {
  const want = new Set(JSON.parse(fs.readFileSync(path.resolve(SAMPLE_LIST), 'utf8')));
  files = files.filter((f) => want.has(f));
}
if (!ONLY_SAMPLES) files = files.slice(0, LIMIT);

console.log(`语料根：${CORPUS_ROOT}`);
console.log(`待分析 ${files.length} 个 *.skel.bytes（只读）…`);
const t0 = Date.now();

const entries = [];
for (let i = 0; i < files.length; i++) {
  const rel = files[i];
  const buf = fs.readFileSync(path.join(CORPUS_ROOT, rel));
  entries.push(analyze(rel, buf));
  if ((i + 1) % 200 === 0) console.log(`  … ${i + 1}/${files.length}`);
}

// ---------------------------------------------------------------------------
// 汇总：分项统计（primary / full 双口径分别给，不混用分母）
// ---------------------------------------------------------------------------

const okEntries = entries.filter((e) => e.parse === 'ok');
const primaries = okEntries.filter((e) => e.isPrimary);

const sum = (list, pick) => list.reduce((a, e) => a + (pick(e) ?? 0), 0);
const agg = (list) => ({
  files: list.length,
  bones: sum(list, (e) => e.counts?.bones),
  slots: sum(list, (e) => e.counts?.slots),
  skins: sum(list, (e) => e.counts?.skins),
  animations: sum(list, (e) => e.counts?.animations),
  timelines: sum(list, (e) => e.counts?.timelines),
  ik: sum(list, (e) => e.counts?.ik),
  transform: sum(list, (e) => e.counts?.transform),
  pathConstraints: sum(list, (e) => e.counts?.path),
  events: sum(list, (e) => e.counts?.events),
  regionAttachments: sum(list, (e) => e.attachments?.region),
  meshAttachments: sum(list, (e) => e.attachments?.mesh),
  weightedMeshAttachments: sum(list, (e) => e.attachments?.weightedMesh),
  linkedMeshAttachments: sum(list, (e) => e.attachments?.linkedMesh),
  boundingboxAttachments: sum(list, (e) => e.attachments?.boundingbox),
  otherAttachments: sum(list, (e) => e.attachments?.other),
  // 时间线类型（决定动画类能力的语料可验证性）
  boneTimelines: sum(list, (e) => e.timelineKinds?.bone),
  slotTimelines: sum(list, (e) => e.timelineKinds?.slot),
  deformTimelines: sum(list, (e) => e.timelineKinds?.deform),
  ikTimelines: sum(list, (e) => e.timelineKinds?.ik),
  drawOrderTimelines: sum(list, (e) => e.timelineKinds?.drawOrder),
  eventTimelines: sum(list, (e) => e.timelineKinds?.event),
  // 语料实测 kind 全集：rotate/translate/scale/shear（骨骼）、attachment/color（插槽）、
  // deform、ik、drawOrder、transform（transform 约束）。event 时间线 0 条。
  transformTimelines: sum(list, (e) => e.timelineKinds?.other),
  // 形态覆盖（决定哪些能力"语料无法验证"）
  multiSkinUnits: list.filter((e) => (e.counts?.skins ?? 0) > 1).length,
  multiPageAtlases: list.filter((e) => (e.atlas?.pages ?? 0) > 1).length,
});

const totals = agg(okEntries);
const primaryTotals = agg(primaries);

// 页声明尺寸 vs PNG 实际尺寸的比例分布（V2 直接证据；只存比例计数，不存资产）
const ratioHist = new Map();
for (const e of okEntries) {
  const rt = e.atlas?.pngOverPageRatio?.[0];
  if (!rt) continue;
  const k = rt.toFixed(3);
  ratioHist.set(k, (ratioHist.get(k) ?? 0) + 1);
}
const ratioTop = [...ratioHist.entries()].sort((a, b) => b[1] - a[1]).slice(0, 10);
const formatHist = new Map();
for (const e of okEntries) {
  for (const [k, v] of Object.entries(e.atlas?.formats ?? {})) formatHist.set(k, (formatHist.get(k) ?? 0) + v);
}
const formatTop = [...formatHist.entries()].sort((a, b) => b[1] - a[1]).slice(0, 10);

const failures = entries.filter((e) => e.parse === 'fail');
const failClasses = {};
for (const f of failures) failClasses[f.failClass ?? 'other'] = (failClasses[f.failClass ?? 'other'] ?? 0) + 1;

const stats = {
  parseOk: okEntries.length,
  parseFail: failures.length,
  failClasses,
  strictEofFailures: failures.filter((f) => f.failClass === 'truncated-or-misaligned').length,
  versions: [...new Set(okEntries.map((e) => e.version))].sort(),
  distinctSkeletonHash: new Set(okEntries.map((e) => e.skeletonHash)).size,
  nonessentialTrue: okEntries.filter((e) => e.nonessential).length,
  nonessentialFalse: okEntries.filter((e) => !e.nonessential).length,
  imagesPathPresent: okEntries.filter((e) => e.imagesPath).length,
  trioComplete: entries.filter((e) => e.trio).length,
  atlasTxtPresent: entries.filter((e) => e.companion.atlasTxt).length,
  pngPresent: entries.filter((e) => e.companion.png).length,
  readerWarningsTotal: sum(okEntries, (e) => e.readerWarnings),
  readerDropsTotal: sum(okEntries, (e) => e.readerDrops),
  referenceClosedAll: okEntries.filter((e) => e.referenceClosed).length,
  referenceNotClosed: okEntries.filter((e) => !e.referenceClosed).length,
  // 引用闭合分项
  boneParentOutOfRange: sum(okEntries, (e) => e.reference?.boneParentOutOfRange),
  boneParentForward: sum(okEntries, (e) => e.reference?.boneParentForward),
  boneParentSelfCycle: sum(okEntries, (e) => e.reference?.boneParentSelfCycle),
  boneRoots: sum(okEntries, (e) => e.reference?.boneRootCount),
  slotBoneOutOfRange: sum(okEntries, (e) => e.reference?.slotBoneOutOfRange),
  ikRefOutOfRange: sum(okEntries, (e) => (e.reference?.ikBoneOutOfRange ?? 0) + (e.reference?.ikTargetOutOfRange ?? 0)),
  transformRefOutOfRange: sum(okEntries, (e) => (e.reference?.transformBoneOutOfRange ?? 0) + (e.reference?.transformTargetOutOfRange ?? 0)),
  // 名称唯一性
  duplicateBoneNames: sum(okEntries, (e) => e.nameDuplicates?.bones),
  duplicateSlotNames: sum(okEntries, (e) => e.nameDuplicates?.slots),
  duplicateAttachmentNames: sum(okEntries, (e) => e.nameDuplicates?.attachmentsPerSlot),
  // 附件 path 语义
  attachmentPathPresent: sum(okEntries, (e) => e.attachments?.pathPresent),
  attachmentPathNull: sum(okEntries, (e) => e.attachments?.pathNull),
  attachmentPathDiffersFromName: sum(okEntries, (e) => e.attachments?.pathDiffersFromName),
  regionRotationNonZero: sum(okEntries, (e) => e.attachments?.regionRotationNonZero),
  regionScaleNotOne: sum(okEntries, (e) => e.attachments?.regionScaleNotOne),
  meshUvOutOfRange: sum(okEntries, (e) => e.attachments?.meshUvOutOfRange),
  triangleIndexOutOfRange: sum(okEntries, (e) => e.attachments?.triangleIndexOutOfRange),
  // 图集形态
  atlasRegions: sum(okEntries, (e) => e.atlas?.regions),
  atlasPages: sum(okEntries, (e) => e.atlas?.pages),
  atlasRotateTrue: sum(okEntries, (e) => e.atlas?.rotateTrue),
  atlasRotateFalse: sum(okEntries, (e) => e.atlas?.rotateFalse),
  atlasRotateOther: sum(okEntries, (e) => e.atlas?.rotateOther),
  atlasTrimmed: sum(okEntries, (e) => e.atlas?.trimmed),
  atlasSizeNeOrig: sum(okEntries, (e) => e.atlas?.sizeNeOrig),
  atlasIndexNeMinusOne: sum(okEntries, (e) => e.atlas?.indexNeMinusOne),
  atlasPngMissing: sum(okEntries, (e) => e.atlas?.pngMissing),
  atlasPngIhdrMatch: sum(okEntries, (e) => e.atlas?.pngIhdrMatch),
  atlasPngIhdrMismatch: sum(okEntries, (e) => e.atlas?.pngIhdrMismatch),
  atlasPageSizePresent: sum(okEntries, (e) => e.atlas?.pageSizePresent),
  atlasPageSizeMissing: sum(okEntries, (e) => e.atlas?.pageSizeMissing),
  atlasRegionOutOfPageDeclared: sum(okEntries, (e) => e.atlas?.regionOutOfPageDeclared),
  atlasPmaTrue: sum(okEntries, (e) => e.atlas?.pmaTrue),
  atlasPmaFalse: sum(okEntries, (e) => e.atlas?.pmaFalse),
  atlasPmaAbsent: sum(okEntries, (e) => e.atlas?.pmaAbsent),
  regionLookupByPathHit: sum(okEntries, (e) => e.atlas?.regionLookupByPath?.hit),
  regionLookupByPathMiss: sum(okEntries, (e) => e.atlas?.regionLookupByPath?.miss),
  regionLookupByNameHit: sum(okEntries, (e) => e.atlas?.regionLookupByName?.hit),
  regionLookupByNameMiss: sum(okEntries, (e) => e.atlas?.regionLookupByName?.miss),
};

// ---- 输出 ----
fs.mkdirSync(OUT_DIR, { recursive: true });
const json = {
  _note: 'Loongbones Task 4 阶段 A · 骨架只读探针输出。仅含相对路径、hash、计数与分类计数，不含任何游戏资源本体。',
  generatedBy: 'tools/skeleton-probe/probe.mjs',
  corpusRootRef: 'LOONGBONES_CORPUS_ROOT（本地配置/环境变量，不写入本文件）',
  readerNote: '解析用旧项目 converter/src/read/binary32.ts + converter/src/atlas.ts（证据参照）；产品读取器在阶段 B 独立实现。',
  elapsedSec: Number(((Date.now() - t0) / 1000).toFixed(1)),
  totals,
  primaryTotals,
  stats,
  atlasPageVsPngRatioTop: ratioTop,
  atlasFormatTop: formatTop,
  entries,
};
fs.writeFileSync(path.join(OUT_DIR, 'skeleton-probe-report.json'), JSON.stringify(json, null, 1) + '\n');

const md = [];
md.push('# Spine 3.2.01 骨架只读探针报告（Task 4 阶段 A）');
md.push('');
md.push(`- 生成工具：\`tools/skeleton-probe/probe.mjs\`（用法见同目录 \`README.md\`）`);
md.push(`- 语料根：由 \`LOONGBONES_CORPUS_ROOT\` / 命令行参数提供，**不写入本文件**`);
md.push(`- 本次分析文件数：**${entries.length}**（其中 primary 口径 **${primaries.length}**）`);
md.push(`- 耗时：${json.elapsedSec} s`);
md.push('');
md.push('## 1. 解析与版本');
md.push('');
md.push('| 指标 | 值 |');
md.push('|---|---|');
md.push(`| 解析成功 | ${stats.parseOk} |`);
md.push(`| 解析失败 | ${stats.parseFail} |`);
md.push(`| 失败分类 | ${JSON.stringify(stats.failClasses)} |`);
md.push(`| strictEof 失败（截断/错位） | ${stats.strictEofFailures} |`);
md.push(`| 版本分布 | ${stats.versions.join(', ')} |`);
md.push(`| 不同 skeleton hash 数 | ${stats.distinctSkeletonHash} |`);
md.push(`| nonessential=true / false | ${stats.nonessentialTrue} / ${stats.nonessentialFalse} |`);
md.push(`| imagesPath 非空 | ${stats.imagesPathPresent} |`);
md.push(`| .atlas.txt 齐全 | ${stats.atlasTxtPresent} / ${entries.length} |`);
md.push(`| .png 齐全 | ${stats.pngPresent} / ${entries.length} |`);
md.push(`| 三件套齐全 | ${stats.trioComplete} / ${entries.length} |`);
md.push('');
md.push('## 2. 引用闭合');
md.push('');
md.push('| 指标 | 值 |');
md.push('|---|---|');
md.push(`| 全部闭合的骨架 | ${stats.referenceClosedAll} / ${stats.parseOk} |`);
md.push(`| 骨骼父索引越界 | ${stats.boneParentOutOfRange} |`);
md.push(`| 骨骼父索引前向引用 | ${stats.boneParentForward} |`);
md.push(`| 骨骼自环 | ${stats.boneParentSelfCycle} |`);
md.push(`| 根骨骼总数 | ${stats.boneRoots} |`);
md.push(`| 插槽骨骼索引越界 | ${stats.slotBoneOutOfRange} |`);
md.push(`| IK 骨骼/目标索引越界 | ${stats.ikRefOutOfRange} |`);
md.push(`| transform 约束索引越界 | ${stats.transformRefOutOfRange} |`);
md.push('');
md.push('## 3. 名称唯一性');
md.push('');
md.push(`- 骨骼重名：${stats.duplicateBoneNames}`);
md.push(`- 插槽重名：${stats.duplicateSlotNames}`);
md.push(`- 同 slot 内附件重名：${stats.duplicateAttachmentNames}`);
md.push('');
md.push('## 4. 附件 path 语义');
md.push('');
md.push(`- path 存在：${stats.attachmentPathPresent}；path 为 null：${stats.attachmentPathNull}`);
md.push(`- **path ≠ 附件名**：${stats.attachmentPathDiffersFromName}`);
md.push(`- region rotation ≠ 0：${stats.regionRotationNonZero}`);
md.push(`- region scale ≠ 1：${stats.regionScaleNotOne}`);
md.push(`- mesh UV 越出 [0,1]：${stats.meshUvOutOfRange}`);
md.push(`- 三角索引越界：${stats.triangleIndexOutOfRange}`);
md.push('');
md.push('## 5. 图集形态');
md.push('');
md.push('| 指标 | 值 |');
md.push('|---|---|');
md.push(`| 页数 / region 数 | ${stats.atlasPages} / ${stats.atlasRegions} |`);
md.push(`| rotate=true / false / 其它写法 | ${stats.atlasRotateTrue} / ${stats.atlasRotateFalse} / ${stats.atlasRotateOther} |`);
md.push(`| 存在 trim（offset≠0） | ${stats.atlasTrimmed} |`);
md.push(`| size ≠ orig（缩放或裁剪） | ${stats.atlasSizeNeOrig} |`);
md.push(`| index ≠ -1（sequence） | ${stats.atlasIndexNeMinusOne} |`);
md.push(`| 页对应 PNG 缺失 | ${stats.atlasPngMissing} |`);
md.push(`| PNG IHDR 与页声明尺寸一致 / 不符 | ${stats.atlasPngIhdrMatch} / ${stats.atlasPngIhdrMismatch} |`);
md.push(`| 页声明 size 存在 / 缺失 | ${stats.atlasPageSizePresent} / ${stats.atlasPageSizeMissing} |`);
md.push(`| region 越出页声明尺寸 | ${stats.atlasRegionOutOfPageDeclared} |`);
md.push(`| pma true / false / 缺省 | ${stats.atlasPmaTrue} / ${stats.atlasPmaFalse} / ${stats.atlasPmaAbsent} |`);
md.push(`| format 分布 | ${formatTop.map(([k, v]) => `${k}×${v}`).join('、') || '（无）'} |`);
md.push('');
md.push('**页声明尺寸 vs PNG 实际尺寸的比例分布（前 10，仅比例值）**：');
md.push('');
md.push('| pngW / pageW | 页数 |');
md.push('|---|---|');
for (const [k, v] of ratioTop) md.push(`| ${k} | ${v} |`);
md.push('');
md.push('## 6. 附件 → 图集 region 查找率');
md.push('');
md.push(`- 按 \`path ?? name\`（Spine 规则）：命中 ${stats.regionLookupByPathHit}，未命中 ${stats.regionLookupByPathMiss}`);
md.push(`- 仅按附件名：命中 ${stats.regionLookupByNameHit}，未命中 ${stats.regionLookupByNameMiss}`);
md.push('');
md.push('## 7. 对象计数（双口径，不混用分母）');
md.push('');
md.push('| 指标 | Full | Primary |');
md.push('|---|---|---|');
for (const k of Object.keys(totals)) md.push(`| ${k} | ${totals[k]} | ${primaryTotals[k]} |`);
md.push('');
fs.writeFileSync(path.join(OUT_DIR, 'skeleton-probe-report.md'), md.join('\n'));

console.log(`\n完成：${entries.length} 个文件，解析成功 ${stats.parseOk}，失败 ${stats.parseFail}`);
console.log(`引用闭合：${stats.referenceClosedAll}/${stats.parseOk}｜path≠name：${stats.attachmentPathDiffersFromName}`);
console.log(`图集：rotate=true ${stats.atlasRotateTrue}｜trim ${stats.atlasTrimmed}｜size≠orig ${stats.atlasSizeNeOrig}`);
console.log(`region 查找(path??name)：命中 ${stats.regionLookupByPathHit} / 未命中 ${stats.regionLookupByPathMiss}`);
console.log(`输出目录：${OUT_DIR}`);
