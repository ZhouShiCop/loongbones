#!/usr/bin/env node
/**
 * Loongbones · Task 4 阶段 A — 合成 fixture 生成器
 *
 * 生成**不含任何游戏资产**的 Spine 3.2.01 二进制骨架 + libGDX `.atlas.txt` + 合成 PNG，
 * 供阶段 B 的导入插件回归使用（Task 4 §2.4 要求 ≥3 个可再分发 fixture）。
 *
 * 生成物（默认写到 docs/skeleton/fixtures/）：
 *   01-minimal               最小骨架：1 骨 / 1 槽 / 1 skin / 1 region 附件 / 0 动画
 *   02-nested-slots-regions  嵌套骨骼 + 3 槽 + 3 region（含 rotation=90、path≠name）
 *   03-bad-parent-index      畸形：骨骼 parentIndex 越界（**必须被拒绝**）
 *   04-truncated             畸形：文件被截断（strictEof **必须失败**）
 *   05-non-finite-float      畸形：骨骼 rotation = NaN（**必须被拒绝**，见 domain-rfc §6.11）
 *
 * 布局依据（只读参照，不是产品代码）：converter/src/read/binary32.ts 与 binary.ts。
 * 本生成器按该布局**独立写出**字节，不调用旧项目写入器，也不用任何游戏文件做模板。
 *
 * 用法：
 *   node tools/skeleton-probe/make-fixture.mjs [-o 输出目录]
 *   node tools/skeleton-probe/make-fixture.mjs --verify   # 生成后回读自校验
 */
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const flag = (n) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : undefined; };
const OUT_DIR = path.resolve(flag('-o') ?? path.resolve(HERE, '../../docs/skeleton/fixtures'));
const VERIFY = args.includes('--verify');
const OLD_ROOT = path.resolve(HERE, '../../..');
const toUrl = (p) => 'file:///' + p.replace(/\\/g, '/');

// ---------------------------------------------------------------------------
// 二进制写出（大端；varint 为 optimizePositive 的 LEB128）
// ---------------------------------------------------------------------------

class W {
  constructor() { this.parts = []; }
  u8(v) { const b = Buffer.alloc(1); b.writeUInt8(v & 0xff, 0); this.parts.push(b); return this; }
  i8(v) { return this.u8(v < 0 ? v + 256 : v); }
  bool(v) { return this.i8(v ? 1 : 0); }
  f32(v) { const b = Buffer.alloc(4); b.writeFloatBE(v, 0); this.parts.push(b); return this; }
  i32(v) { const b = Buffer.alloc(4); b.writeInt32BE(v | 0, 0); this.parts.push(b); return this; }
  i16(v) { const b = Buffer.alloc(2); b.writeInt16BE(v, 0); this.parts.push(b); return this; }
  /** varint（非负）；越界/畸形 fixture 也走这里，值本身不校验。 */
  varint(v) {
    let x = v >>> 0;
    for (;;) {
      const b = x & 0x7f;
      x >>>= 7;
      if (x === 0) { this.u8(b); return this; }
      this.u8(b | 0x80);
    }
  }
  /** 0 → null；n → varint(n+1) + utf8 字节。 */
  str(s) {
    if (s === null || s === undefined) return this.varint(0);
    const b = Buffer.from(s, 'utf8');
    if (b.length === 0) return this.varint(1);
    this.varint(b.length + 1);
    this.parts.push(b);
    return this;
  }
  color(hex) { return this.i32(hex); }  // 0xRRGGBBAA
  bytes() { return Buffer.concat(this.parts); }
}

const WHITE = 0xffffffff;
const KEY = 'loongbones-fixture';   // header 的 hash 字段：随机串，不是版本号
const VERSION = '3.2.01';

/**
 * region 附件（type byte = 0）。
 *
 * 字节序（严格对应 binary32.ts 的 readSkin + readAttachment32）：
 *   1. skin 条目键 `key`（string）—— 皮肤里该附件的名字
 *   2. 附件自身 `name`（string，**可为 null**，null 时回退到 key）
 *   3. type byte = 0
 *   4. path（string，可为 null）、rotation、x、y、scaleX、scaleY、width、height、color
 */
function writeRegion(w, a) {
  w.str(a.name);                 // 1. skin 条目键
  w.str(a.attachmentName ?? null); // 2. 附件自身 name（常为 null）
  w.u8(0);                       // 3. type = region
  w.str(a.path ?? null);         // 4. path
  w.f32(a.rotation ?? 0); w.f32(a.x ?? 0); w.f32(a.y ?? 0);
  w.f32(a.scaleX ?? 1); w.f32(a.scaleY ?? 1);
  w.f32(a.width); w.f32(a.height);
  w.color(WHITE);
}

/** 一个 skin：slotCount + 每槽 (slotIndex, attCount, 附件…) */
function writeSkin(w, entries) {
  w.varint(entries.length);
  for (const e of entries) {
    w.varint(e.slotIndex);
    w.varint(e.attachments.length);
    for (const a of e.attachments) writeRegion(w, a);
  }
}

/**
 * 拼一份骨架二进制。
 * @param bones [{name, parentIndex, rotation, x, y, scaleX, scaleY, shearX, shearY, length,
 *                inheritScale, inheritRotation}]（第 0 根不写 parentIndex）
 * @param slots [{name, boneIndex, attachment, blend}]
 */
function buildSkeleton({ hash = KEY, version = VERSION, width = 100, height = 100, nonessential = false,
                         bones, slots, skinEntries }) {
  const w = new W();
  w.str(hash);
  w.str(version);
  w.f32(width);
  w.f32(height);
  w.bool(nonessential);
  // nonessential 时头部多一个 imagesPath（语料只有 4 个单位如此，fixture 走 false）
  if (nonessential) w.str(null);

  w.varint(bones.length);
  for (let i = 0; i < bones.length; i++) {
    const b = bones[i];
    w.str(b.name);
    if (i > 0) w.varint(b.parentIndex ?? -1);
    w.f32(b.rotation ?? 0); w.f32(b.x ?? 0); w.f32(b.y ?? 0);
    w.f32(b.scaleX ?? 1); w.f32(b.scaleY ?? 1);
    w.f32(b.shearX ?? 0); w.f32(b.shearY ?? 0);
    w.f32(b.length ?? 0);
    w.bool(b.inheritScale ?? true);
    w.bool(b.inheritRotation ?? true);
  }

  w.varint(0); // ik
  w.varint(0); // transform

  w.varint(slots.length);
  for (const s of slots) {
    w.str(s.name);
    w.varint(s.boneIndex ?? 0);
    w.color(WHITE);
    w.str(s.attachment ?? null);
    w.u8(s.blend ?? 0);
  }

  writeSkin(w, skinEntries);
  w.varint(0); // 额外 skin 数
  w.varint(0); // events
  w.varint(0); // animations
  return w.bytes();
}

// ---------------------------------------------------------------------------
// libGDX .atlas.txt 与合成 PNG
// ---------------------------------------------------------------------------

function atlasText(pageFile, pageW, pageH, regions) {
  const L = [pageFile, `size: ${pageW},${pageH}`, 'format: RGBA8888', 'filter: Linear,Linear', 'repeat: none', ''];
  for (const r of regions) {
    L.push(r.name);
    L.push(`  rotate: ${r.rotate ? 'true' : 'false'}`);
    L.push(`  xy: ${r.x}, ${r.y}`);
    L.push(`  size: ${r.w}, ${r.h}`);
    L.push(`  orig: ${r.ow ?? r.w}, ${r.oh ?? r.h}`);
    L.push(`  offset: ${r.ox ?? 0}, ${r.oy ?? 0}`);
    L.push(`  index: ${r.index ?? -1}`);
    L.push('');
  }
  return L.join('\n');
}

let crcTable = null;
function crc32(buf) {
  if (!crcTable) {
    crcTable = new Int32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
      crcTable[n] = c;
    }
  }
  let c = -1;
  for (let i = 0; i < buf.length; i++) c = crcTable[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ -1) >>> 0;
}
function pngChunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length, 0);
  const t = Buffer.from(type, 'latin1');
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(Buffer.concat([t, data])), 0);
  return Buffer.concat([len, t, data, crc]);
}
/** 合成一张纯色 RGBA PNG（不含任何游戏图像）。 */
function pngBytes(w, h, rgba) {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) {
    const off = y * (w * 4 + 1);
    raw[off] = 0; // filter: none
    for (let x = 0; x < w; x++) {
      const p = off + 1 + x * 4;
      raw[p] = rgba[0]; raw[p + 1] = rgba[1]; raw[p + 2] = rgba[2]; raw[p + 3] = rgba[3];
    }
  }
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    pngChunk('IHDR', ihdr),
    pngChunk('IDAT', zlib.deflateSync(raw)),
    pngChunk('IEND', Buffer.alloc(0)),
  ]);
}

// ---------------------------------------------------------------------------
// 4 个 fixture
// ---------------------------------------------------------------------------

function fixtureMinimal() {
  const skel = buildSkeleton({
    width: 64, height: 64,
    bones: [{ name: 'root', length: 0 }],
    slots: [{ name: 'slot-0', boneIndex: 0, attachment: 'region-0' }],
    skinEntries: [{ slotIndex: 0, attachments: [{ name: 'region-0', path: null, width: 32, height: 32 }] }],
  });
  return {
    name: '01-minimal',
    files: {
      '01-minimal.skel.bytes': skel,
      '01-minimal.atlas.txt': Buffer.from(atlasText('01-minimal.png', 64, 64,
        [{ name: 'region-0', x: 0, y: 0, w: 32, h: 32 }]), 'utf8'),
      '01-minimal.png': pngBytes(64, 64, [255, 0, 0, 255]),
    },
  };
}

function fixtureNested() {
  const skel = buildSkeleton({
    width: 128, height: 128,
    bones: [
      { name: 'root', length: 0 },
      { name: 'hip', parentIndex: 0, y: 10, length: 20 },
      { name: 'chest', parentIndex: 1, y: 20, rotation: 15, length: 24 },
      { name: 'head', parentIndex: 2, y: 24, length: 12 },
    ],
    slots: [
      { name: 'slot-body', boneIndex: 1, attachment: 'body' },
      // 附件名 ≠ 图集 region 名：靠 path 指过去（语料实证 84 例的等价形态）
      { name: 'slot-head', boneIndex: 3, attachment: 'head-skin-a' },
      { name: 'slot-weapon', boneIndex: 2, attachment: 'weapon' },
    ],
    skinEntries: [
      { slotIndex: 0, attachments: [{ name: 'body', path: null, width: 48, height: 64 }] },
      { slotIndex: 1, attachments: [{ name: 'head-skin-a', path: 'head', width: 32, height: 32 }] },
      // rotation=90：验证附件旋转不为 0 的导入路径（语料 24351/27459 非零）
      { slotIndex: 2, attachments: [{ name: 'weapon', path: null, rotation: 90, x: 4, y: -2, width: 16, height: 48 }] },
    ],
  });
  return {
    name: '02-nested-slots-regions',
    files: {
      '02-nested-slots-regions.skel.bytes': skel,
      '02-nested-slots-regions.atlas.txt': Buffer.from(atlasText('02-nested-slots-regions.png', 128, 128, [
        { name: 'body', x: 0, y: 0, w: 48, h: 64 },
        // rotate:true —— 语料 17734/43704 为此形态
        { name: 'head', x: 48, y: 0, w: 32, h: 32, rotate: true },
        { name: 'weapon', x: 80, y: 0, w: 16, h: 48 },
      ]), 'utf8'),
      '02-nested-slots-regions.png': pngBytes(128, 128, [0, 128, 255, 255]),
    },
  };
}

function fixtureBadParentIndex() {
  // 3 根骨骼，第 3 根的 parentIndex = 9（越界）→ 导入**必须拒绝**并给出可定位诊断
  const skel = buildSkeleton({
    width: 64, height: 64,
    bones: [
      { name: 'root', length: 0 },
      { name: 'a', parentIndex: 0, length: 10 },
      { name: 'b', parentIndex: 9, length: 10 },
    ],
    slots: [{ name: 's', boneIndex: 0, attachment: null }],
    skinEntries: [{ slotIndex: 0, attachments: [] }],
  });
  return { name: '03-bad-parent-index', files: { '03-bad-parent-index.skel.bytes': skel } };
}

function fixtureTruncated() {
  const full = fixtureMinimal().files['01-minimal.skel.bytes'];
  // 砍掉最后 8 字节：strictEof 必须报错（模拟截断/损坏）
  return { name: '04-truncated', files: { '04-truncated.skel.bytes': full.subarray(0, full.length - 8) } };
}

function fixtureNonFiniteFloat() {
  // 第 2 根骨骼的 rotation 是 NaN：产品读取器必须拒绝整个候选，
  // 诊断 SKELETON_FLOAT_NON_FINITE 且带段名 + 字节偏移（domain-rfc §6.11）。
  // 注意：旧项目的宽松 Reader 不校验有限性，会把 NaN 原样收下 —— 这正是要钉住的语义差别。
  const skel = buildSkeleton({
    width: 32, height: 32,
    bones: [
      { name: 'root', length: 0 },
      { name: 'broken', parentIndex: 0, rotation: NaN, length: 8 },
    ],
    slots: [{ name: 's', boneIndex: 0, attachment: null }],
    skinEntries: [{ slotIndex: 0, attachments: [] }],
  });
  return { name: '05-non-finite-float', files: { '05-non-finite-float.skel.bytes': skel } };
}

// ---------------------------------------------------------------------------
// 写出 + 可选回读自校验
// ---------------------------------------------------------------------------

fs.mkdirSync(OUT_DIR, { recursive: true });
const all = [fixtureMinimal(), fixtureNested(), fixtureBadParentIndex(), fixtureTruncated(),
             fixtureNonFiniteFloat()];
const index = [];
for (const fx of all) {
  for (const [rel, buf] of Object.entries(fx.files)) {
    fs.writeFileSync(path.join(OUT_DIR, rel), buf);
  }
  index.push({ fixture: fx.name, files: Object.keys(fx.files) });
  console.log(`${fx.name}: ${Object.keys(fx.files).join(', ')}`);
}
fs.writeFileSync(path.join(OUT_DIR, 'fixtures.json'),
  JSON.stringify({ _note: '合成 fixture，不含任何游戏资产。', generatedBy: 'tools/skeleton-probe/make-fixture.mjs', items: index }, null, 1) + '\n');

if (VERIFY) {
  const { readSkeletonBinary32 } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/read/binary32.ts')));
  const { IssueLog } = await import(toUrl(path.join(OLD_ROOT, 'converter/src/model.ts')));
  console.log('\n回读自校验（strictEof: true）：');
  for (const fx of all) {
    const rel = Object.keys(fx.files).find((k) => k.endsWith('.skel.bytes'));
    const buf = fs.readFileSync(path.join(OUT_DIR, rel));
    const log = new IssueLog();
    try {
      const ir = readSkeletonBinary32(buf, log, { strictEof: true });
      const warns = (log.issues ?? []).filter((x) => x.level === 'warn');
      console.log(`  ${fx.name}: ok  bones=${ir.bones.length} slots=${ir.slots.length} skins=${ir.skins.length}` +
        ` anims=${(ir.animations ?? []).length} version=${ir.skeleton?.version}` +
        ` warnings=${warns.length}`);
      for (const wn of warns) console.log(`     warn: ${wn.where} — ${wn.message}`);
    } catch (err) {
      console.log(`  ${fx.name}: FAIL — ${String(err.message ?? err)}`);
    }
  }
}
console.log(`\n输出目录：${OUT_DIR}`);
