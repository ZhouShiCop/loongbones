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
 * Task 5 A5（第二批静态数据）：
 *   06-minimal-mesh          最小非加权 mesh（含 hull / edges / width / height，均不落 canonical）
 *   07-weighted-mesh         加权 mesh，**负权重 -0.2 且和 = 1.0** ⇒ 合法（D34）
 *   08-linked-mesh           类型 4：父为非加权 mesh + 显式 parentSkin='default'（语料 0 例）
 *   09-mesh-bad-triangle     畸形：三角索引越界（**必须被拒绝**）
 *   10-mesh-bad-bone-index   畸形：绑定 boneIndex 越界（**必须被拒绝**）
 *   11-constraints           IK mix=1（enabled=true）+ transform 10 float 原值透传
 *
 * Task 5 A5 复核返修（2026-10-22 补齐三项覆盖缺口）：
 *   12-mesh-bad-weight-sum-zero      畸形：顶点权重和 = 0（**必须被拒绝**）
 *   13-mesh-bad-weight-sum-negative  畸形：顶点权重和 < 0（**必须被拒绝**）
 *   14-linked-mesh-implicit-skin     类型 5：父为加权 mesh + parentSkin=null（**语料 2 例形态**）
 *   15-ik-mix-zero           IK mix=0 ⇒ enabled=false（D35 的 false 分支，语料 302 例）
 *   16-minimal-mesh-plain    `nonessential = 0` 的 mesh（**语料 1109 / 1113 的形态**）：
 *                            无 edges / width / height、骨骼末尾无颜色；06 的补集
 *
 * Task 6 A4（共享图集布局，**按目录组织**，与前 16 个"扁平文件"不同）：
 *   17-shared-atlas        正向：两个骨架共用一份 `<上级目录名>_<目录名>.atlas.txt` + 单页 PNG
 *                          （== 语料唯一一处 `h0164s5/watch/` 的形态，2 个单位）
 *   18-ambiguous-atlas     负向：目录内 2 个候选图集且**都不同名** ⇒ 歧义，**必须显式拒绝**（Q6-2）
 *   19-multi-atlas-same-name 守卫：目录内 2 个图集但**骨架同名的一个存在** ⇒
 *                          必须走"同名优先"，**不得**判歧义、**不得**选到另一个（回归守卫）
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

/**
 * mesh(2) / weightedmesh(3)。
 * 字节序（严格对应 binary32.ts 的 readAttachment32）：
 *   key, name(null 回退), type, path, color, n(varint), uv(2n float),
 *   triangles(short 数组), vertices, hull(varint)；nonessential 时再写 edges(short 数组) + w + h。
 * 加权顶点 = 每顶点 boneCount(**float**) + n × [boneIndex(float), x, y, w]（N1：不是 varint）。
 */
function writeMesh(w, a, nonessential) {
  const n = a.bindings ? a.bindings.length : a.vertices.length / 2;
  w.str(a.name);
  w.str(a.attachmentName ?? null);
  w.u8(a.bindings ? 3 : 2);
  w.str(a.path ?? null);
  w.color(WHITE);
  w.varint(n);
  for (const uv of a.uvs) w.f32(uv);
  w.varint(a.triangles.length);
  for (const t of a.triangles) w.i16(t);
  if (a.bindings) {
    for (const binds of a.bindings) {
      w.f32(binds.length);
      for (const b of binds) { w.f32(b.boneIndex); w.f32(b.x); w.f32(b.y); w.f32(b.weight); }
    }
  } else {
    for (const v of a.vertices) w.f32(v);
  }
  w.varint(a.hull ?? n);
  if (nonessential) {
    const edges = a.edges ?? [];
    w.varint(edges.length);
    for (const e of edges) w.i16(e);
    w.f32(a.width ?? 0);
    w.f32(a.height ?? 0);
  }
}

/**
 * linkedmesh(4) / weightedlinkedmesh(5)：几何不落，只写引用。
 *   key, name, type, path, color, parentSkin, parent, inheritFFD(bool)；nonessential 时 + w + h。
 *
 * `parentSkin`：显式给 `null` 表示「父附件在同皮肤内」（**语料 2 例正是此形态**）；
 * 未给该字段时才回退到 `'default'`。注意这里**不能**用 `??` 判空 —— 那会把显式的
 * `null` 也吞成 `'default'`，导致两条解析路径在 fixture 里永远覆盖不到。
 */
function writeLinkedMesh(w, a, nonessential) {
  w.str(a.name);
  w.str(a.attachmentName ?? null);
  w.u8(a.weighted ? 5 : 4);
  w.str(a.path ?? null);
  w.color(WHITE);
  const parentSkin = Object.prototype.hasOwnProperty.call(a, 'parentSkin') ? a.parentSkin : 'default';
  w.str(parentSkin);
  w.str(a.parent);
  w.bool(a.inheritDeform ?? false);
  if (nonessential) { w.f32(a.width ?? 0); w.f32(a.height ?? 0); }
}

/** 一个 skin：slotCount + 每槽 (slotIndex, attCount, 附件…)；按附件 type 分派。 */
function writeSkin(w, entries, nonessential) {
  w.varint(entries.length);
  for (const e of entries) {
    w.varint(e.slotIndex);
    w.varint(e.attachments.length);
    for (const a of e.attachments) {
      if (a.type === 'mesh') writeMesh(w, a, nonessential);
      else if (a.type === 'linkedMesh') writeLinkedMesh(w, a, nonessential);
      else writeRegion(w, a);
    }
  }
}

/**
 * 拼一份骨架二进制。
 * @param bones [{name, parentIndex, rotation, x, y, scaleX, scaleY, shearX, shearY, length,
 *                inheritScale, inheritRotation}]（第 0 根不写 parentIndex）
 * @param slots [{name, boneIndex, attachment, blend}]
 */
function buildSkeleton({ hash = KEY, version = VERSION, width = 100, height = 100, nonessential = false,
                         bones, slots, skinEntries, ik = [], transform = [] }) {
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
    // nonessential 时骨骼末尾还有 4 字节颜色（binary32.ts:133 读掉但不消费）
    if (nonessential) w.color(b.color ?? WHITE);
  }

  // ik：name, boneCount(varint), boneIndex…(varint), target(varint), mix(f32), bendPositive(byte)
  w.varint(ik.length);
  for (const c of ik) {
    w.str(c.name);
    w.varint(c.boneIndices.length);
    for (const bi of c.boneIndices) w.varint(bi);
    w.varint(c.targetIndex);
    w.f32(c.mix ?? 1);
    w.u8(c.bendPositive === false ? 255 : 1); // readByte() >= 0 ⇒ true
  }

  // transform：name, boneIndex, target, 4 个 mix + 6 个 offset（全 f32）
  w.varint(transform.length);
  for (const c of transform) {
    w.str(c.name);
    w.varint(c.boneIndex);
    w.varint(c.targetIndex);
    w.f32(c.rotateMix ?? 0); w.f32(c.translateMix ?? 0);
    w.f32(c.scaleMix ?? 0); w.f32(c.shearMix ?? 0);
    w.f32(c.offsetRotation ?? 0); w.f32(c.offsetX ?? 0); w.f32(c.offsetY ?? 0);
    w.f32(c.offsetScaleX ?? 0); w.f32(c.offsetScaleY ?? 0); w.f32(c.offsetShearY ?? 0);
  }

  w.varint(slots.length);
  for (const s of slots) {
    w.str(s.name);
    w.varint(s.boneIndex ?? 0);
    w.color(WHITE);
    w.str(s.attachment ?? null);
    w.u8(s.blend ?? 0);
  }

  writeSkin(w, skinEntries, nonessential);
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
// 16 个 fixture：5 个首批（Task 4）+ 11 个第二批（Task 5 A5 的 6 个 + 复核返修补的 5 个）
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
// Task 5 A5：第二批 fixture（mesh / 加权 / linked / 约束 / 畸形引用）
//
// 全部为**合成**样本，不含任何游戏资产；预期结果登记在
// `docs/skeleton/fixtures/README.md`，与真实语料的对应关系见
// `docs/skeleton/mesh-and-constraint-rfc.md` §12。
//
// nonessential 一律 true —— 语料 20250 条 mesh 的 width/height 100% 出现（A1 证据 §4），
// 合成样本保持同一形态，才能覆盖 edges / width / height 的读取分支。
// ---------------------------------------------------------------------------

function fixtureMinimalMesh() {
  const skel = buildSkeleton({
    width: 64, height: 64, nonessential: true,
    bones: [{ name: 'root', length: 0 }],
    slots: [{ name: 'slot-mesh', boneIndex: 0, attachment: 'mesh-0' }],
    skinEntries: [{ slotIndex: 0, attachments: [{
      type: 'mesh', name: 'mesh-0', path: null,
      uvs: [0, 0, 1, 0, 0.5, 1],          // regionUVs，0..1
      vertices: [0, 0, 32, 0, 16, 32],      // 槽骨骼局部坐标（N2）
      triangles: [0, 1, 2],
      hull: 3, edges: [0, 1, 1, 2, 2, 0],   // hull / edges 只用于读取覆盖，不落 canonical（D31）
      width: 32, height: 32,
    }] }],
  });
  return {
    name: '06-minimal-mesh',
    files: {
      '06-minimal-mesh.skel.bytes': skel,
      '06-minimal-mesh.atlas.txt': Buffer.from(atlasText('06-minimal-mesh.png', 64, 64,
        [{ name: 'mesh-0', x: 0, y: 0, w: 32, h: 32 }]), 'utf8'),
      '06-minimal-mesh.png': pngBytes(64, 64, [255, 255, 0, 255]),
    },
  };
}

function fixtureWeightedMesh() {
  // 3 顶点 × (1~2 绑定)；顶点 0 故意含**负权重**(-0.2) 与 >1 的权重(1.2)：
  // 权重和为 1.0，负权重按 D34 合法（语义＝该绑定对顶点无影响），导入**不得**拒绝。
  const skel = buildSkeleton({
    width: 64, height: 64, nonessential: true,
    bones: [{ name: 'root', length: 0 }, { name: 'bone-a', parentIndex: 0, y: 10, length: 20 }],
    slots: [{ name: 'slot-wmesh', boneIndex: 1, attachment: 'wmesh-0' }],
    skinEntries: [{ slotIndex: 0, attachments: [{
      type: 'mesh', name: 'wmesh-0', path: null,
      uvs: [0, 0, 1, 0, 1, 1],
      bindings: [
        [{ boneIndex: 1, x: 0, y: 0, weight: 1.2 }, { boneIndex: 0, x: 10, y: 0, weight: -0.2 }],
        [{ boneIndex: 1, x: 32, y: 0, weight: 1.0 }],
        [{ boneIndex: 1, x: 32, y: 32, weight: 0.5 }, { boneIndex: 0, x: 0, y: 32, weight: 0.5 }],
      ],
      triangles: [0, 1, 2], hull: 3, edges: [], width: 32, height: 32,
    }] }],
  });
  return {
    name: '07-weighted-mesh',
    files: {
      '07-weighted-mesh.skel.bytes': skel,
      '07-weighted-mesh.atlas.txt': Buffer.from(atlasText('07-weighted-mesh.png', 64, 64,
        [{ name: 'wmesh-0', x: 0, y: 0, w: 32, h: 32 }]), 'utf8'),
      '07-weighted-mesh.png': pngBytes(64, 64, [0, 255, 128, 255]),
    },
  };
}

function fixtureLinkedMesh() {
  // 父 mesh + 两个 linked（inheritDeform true / false 各一）—— 与语料 2 例同构
  // （b3240c3 的 true、h0164s5 的 false）。子附件**没有**自己的顶点区。
  const skel = buildSkeleton({
    width: 64, height: 64, nonessential: true,
    bones: [{ name: 'root', length: 0 }],
    slots: [{ name: 'slot-body', boneIndex: 0, attachment: 'body_mesh' }],
    skinEntries: [{ slotIndex: 0, attachments: [
      { type: 'mesh', name: 'body_mesh', path: null,
        uvs: [0, 0, 1, 0, 1, 1], vertices: [0, 0, 32, 0, 32, 32],
        triangles: [0, 1, 2], hull: 3, edges: [], width: 32, height: 32 },
      { type: 'linkedMesh', name: 'body_mesh_linked', path: null,
        parentSkin: 'default', parent: 'body_mesh', inheritDeform: true, width: 32, height: 32 },
      { type: 'linkedMesh', name: 'body_mesh_linked_noffd', path: null,
        parentSkin: 'default', parent: 'body_mesh', inheritDeform: false, width: 32, height: 32 },
    ] }],
  });
  return {
    name: '08-linked-mesh',
    files: {
      '08-linked-mesh.skel.bytes': skel,
      '08-linked-mesh.atlas.txt': Buffer.from(atlasText('08-linked-mesh.png', 64, 64,
        [{ name: 'body_mesh', x: 0, y: 0, w: 32, h: 32 }]), 'utf8'),
      '08-linked-mesh.png': pngBytes(64, 64, [128, 0, 255, 255]),
    },
  };
}

function fixtureMeshBadTriangle() {
  // 3 个顶点，三角索引里出现 9（≥ 顶点数）→ 必须拒绝并报 MESH_TRIANGLE_INDEX_OUT_OF_RANGE
  const skel = buildSkeleton({
    width: 32, height: 32, nonessential: true,
    bones: [{ name: 'root', length: 0 }],
    slots: [{ name: 'slot-mesh', boneIndex: 0, attachment: 'mesh-bad' }],
    skinEntries: [{ slotIndex: 0, attachments: [{
      type: 'mesh', name: 'mesh-bad', path: null,
      uvs: [0, 0, 1, 0, 0.5, 1], vertices: [0, 0, 32, 0, 16, 32],
      triangles: [0, 1, 9], hull: 3, edges: [], width: 32, height: 32,
    }] }],
  });
  return { name: '09-mesh-bad-triangle', files: { '09-mesh-bad-triangle.skel.bytes': skel } };
}

function fixtureMeshBadBoneIndex() {
  // 加权 mesh 的 boneIndex = 7，但骨架只有 2 根骨骼 → 必须拒绝并报 MESH_WEIGHT_BONE_UNRESOLVED
  const skel = buildSkeleton({
    width: 32, height: 32, nonessential: true,
    bones: [{ name: 'root', length: 0 }, { name: 'bone-a', parentIndex: 0, length: 10 }],
    slots: [{ name: 'slot-wmesh', boneIndex: 0, attachment: 'wmesh-bad' }],
    skinEntries: [{ slotIndex: 0, attachments: [{
      type: 'mesh', name: 'wmesh-bad', path: null,
      uvs: [0, 0, 1, 0, 1, 1],
      bindings: [[{ boneIndex: 7, x: 0, y: 0, weight: 1 }],
                 [{ boneIndex: 1, x: 32, y: 0, weight: 1 }],
                 [{ boneIndex: 1, x: 32, y: 32, weight: 1 }]],
      triangles: [0, 1, 2], hull: 3, edges: [], width: 32, height: 32,
    }] }],
  });
  return { name: '10-mesh-bad-bone-index', files: { '10-mesh-bad-bone-index.skel.bytes': skel } };
}

function fixtureConstraints() {
  // IK：mix = 1 → enabled = true（D35）；transform：mix/offset 原值透传、不解释语义（D36）
  const skel = buildSkeleton({
    width: 32, height: 32, nonessential: true,
    bones: [{ name: 'root', length: 0 }, { name: 'leg', parentIndex: 0, y: 20, length: 20 }],
    slots: [{ name: 's', boneIndex: 0, attachment: null }],
    skinEntries: [{ slotIndex: 0, attachments: [] }],
    ik: [{ name: 'leg-ik', boneIndices: [1], targetIndex: 0, mix: 1, bendPositive: true }],
    transform: [{ name: 'copy-tr', boneIndex: 1, targetIndex: 0,
      rotateMix: 90, translateMix: 0, scaleMix: 0, shearMix: 0,
      offsetRotation: 0, offsetX: 0, offsetY: 1, offsetScaleX: 0.5, offsetScaleY: 1, offsetShearY: 0 }],
  });
  return { name: '11-constraints', files: { '11-constraints.skel.bytes': skel } };
}

// ---------------------------------------------------------------------------
// Task 5 A5 复核返修（2026-10-22）：补齐 3 项覆盖缺口
//   P2 非法权重：权重和 = 0（12）与权重和 < 0（13）—— 负责人裁定「和为 0 即可，
//      小于零也没有意义」⇒ 判据收紧为 **和必须有限且 > 0**，两者都拒绝。
//   P3 linked 与语料对齐：语料 2 例是 `parentSkin = null`（父在同皮肤内）+ 父为
//      加权 mesh（类型字节 5）；08 写的是显式 `'default'` + 父为非加权 mesh（类型 4）。
//      两条**解析路径不同**，14 补上语料那一侧（解析器两侧都要能处理，原生格式不受影响）。
//   P4 D35 的 false 分支：`mix = 0` ⇒ `enabled = false`（语料 302 例），此前只有 mix=1。
// ---------------------------------------------------------------------------

function fixtureMeshBadWeightSumZero() {
  // 顶点 0 的两条绑定权重都是 0 ⇒ 和 = 0 ⇒ 该顶点不受任何骨骼影响 ⇒ **必须拒绝**
  // （MESH_WEIGHT_SUM_INVALID）。顶点 1 / 2 正常（和 = 1）—— 拒绝是候选级的，
  // 所以 golden 只需钉住「顶点 0 的和 = 0」这一条。
  const skel = buildSkeleton({
    width: 32, height: 32, nonessential: true,
    bones: [{ name: 'root', length: 0 }, { name: 'bone-a', parentIndex: 0, y: 10, length: 20 }],
    slots: [{ name: 'slot-wmesh', boneIndex: 1, attachment: 'wmesh-zero' }],
    skinEntries: [{ slotIndex: 0, attachments: [{
      type: 'mesh', name: 'wmesh-zero', path: null,
      uvs: [0, 0, 1, 0, 1, 1],
      bindings: [
        [{ boneIndex: 1, x: 0, y: 0, weight: 0.0 }, { boneIndex: 0, x: 10, y: 0, weight: 0.0 }],
        [{ boneIndex: 1, x: 32, y: 0, weight: 1.0 }],
        [{ boneIndex: 1, x: 32, y: 32, weight: 1.0 }],
      ],
      triangles: [0, 1, 2], hull: 3, edges: [], width: 32, height: 32,
    }] }],
  });
  return { name: '12-mesh-bad-weight-sum-zero', files: { '12-mesh-bad-weight-sum-zero.skel.bytes': skel } };
}

function fixtureMeshBadWeightSumNegative() {
  // 顶点 0 的两条绑定都是负权重（-0.5 / -0.2）⇒ 和 = -0.7 < 0 ⇒ **必须拒绝**
  // （MESH_WEIGHT_SUM_INVALID）。判据是**权重和**，不是单条绑定的符号：
  // 单条负权重只要和仍 > 0 就合法（D34，语料 86 例），见 07-weighted-mesh。
  const skel = buildSkeleton({
    width: 32, height: 32, nonessential: true,
    bones: [{ name: 'root', length: 0 }, { name: 'bone-a', parentIndex: 0, y: 10, length: 20 }],
    slots: [{ name: 'slot-wmesh', boneIndex: 1, attachment: 'wmesh-neg' }],
    skinEntries: [{ slotIndex: 0, attachments: [{
      type: 'mesh', name: 'wmesh-neg', path: null,
      uvs: [0, 0, 1, 0, 1, 1],
      bindings: [
        [{ boneIndex: 1, x: 0, y: 0, weight: -0.5 }, { boneIndex: 0, x: 10, y: 0, weight: -0.2 }],
        [{ boneIndex: 1, x: 32, y: 0, weight: 1.0 }],
        [{ boneIndex: 1, x: 32, y: 32, weight: 1.0 }],
      ],
      triangles: [0, 1, 2], hull: 3, edges: [], width: 32, height: 32,
    }] }],
  });
  return { name: '13-mesh-bad-weight-sum-negative', files: { '13-mesh-bad-weight-sum-negative.skel.bytes': skel } };
}

function fixtureLinkedMeshImplicitSkin() {
  // 与语料 2 例**逐字段对齐**：`parentSkin = null`（父在同皮肤内）+ 父是**加权** mesh
  // ⇒ 类型字节 5（weightedLinkedMesh）；`inheritDeform` true / false 各一。
  // 08 覆盖的是另一侧（显式 'default' + 非加权父 ⇒ 类型 4），两者都要能解析。
  const skel = buildSkeleton({
    width: 64, height: 64, nonessential: true,
    bones: [{ name: 'root', length: 0 }, { name: 'bone-a', parentIndex: 0, y: 10, length: 20 }],
    slots: [{ name: 'slot-body', boneIndex: 0, attachment: 'body_wmesh' }],
    skinEntries: [{ slotIndex: 0, attachments: [
      { type: 'mesh', name: 'body_wmesh', path: null,
        uvs: [0, 0, 1, 0, 1, 1],
        bindings: [
          [{ boneIndex: 1, x: 0, y: 0, weight: 1.0 }],
          [{ boneIndex: 1, x: 32, y: 0, weight: 1.0 }],
          [{ boneIndex: 1, x: 32, y: 32, weight: 1.0 }],
        ],
        triangles: [0, 1, 2], hull: 3, edges: [], width: 32, height: 32 },
      { type: 'linkedMesh', name: 'body_wmesh_linked', path: null,
        parentSkin: null, parent: 'body_wmesh', weighted: true,
        inheritDeform: true, width: 32, height: 32 },
      { type: 'linkedMesh', name: 'body_wmesh_linked_noffd', path: null,
        parentSkin: null, parent: 'body_wmesh', weighted: true,
        inheritDeform: false, width: 32, height: 32 },
    ] }],
  });
  return {
    name: '14-linked-mesh-implicit-skin',
    files: {
      '14-linked-mesh-implicit-skin.skel.bytes': skel,
      '14-linked-mesh-implicit-skin.atlas.txt': Buffer.from(atlasText('14-linked-mesh-implicit-skin.png', 64, 64,
        [{ name: 'body_wmesh', x: 0, y: 0, w: 32, h: 32 }]), 'utf8'),
      '14-linked-mesh-implicit-skin.png': pngBytes(64, 64, [255, 128, 0, 255]),
    },
  };
}

function fixtureIkMixZero() {
  // D35 的 false 分支：`mix = 0` ⇒ `enabled = false`（语料 302 例为此值）。
  // 11-constraints 只覆盖 mix=1 ⇒ true，此前 false 分支没有任何样本。
  // `bendPositive = false`：mix=0 的约束不参与求解，该位也一并钉住。
  const skel = buildSkeleton({
    width: 32, height: 32, nonessential: true,
    bones: [{ name: 'root', length: 0 }, { name: 'leg', parentIndex: 0, y: 20, length: 20 }],
    slots: [{ name: 's', boneIndex: 0, attachment: null }],
    skinEntries: [{ slotIndex: 0, attachments: [] }],
    ik: [{ name: 'leg-ik-off', boneIndices: [1], targetIndex: 0, mix: 0, bendPositive: false }],
  });
  return { name: '15-ik-mix-zero', files: { '15-ik-mix-zero.skel.bytes': skel } };
}

function fixtureMinimalMeshPlain() {
  // 06 走的是 `nonessential = true`（含 edges / width / height），而语料 1113 个单位里
  // **只有 4 个**是这一形态 —— 占 99%+ 的是 `nonessential = 0`：无 edges、无 width/height、
  // 骨骼末尾也没有 4 字节颜色。这一形态此前**没有 mesh fixture**（A5 复核 P1 缺口）。
  // 与「对齐语料」同一条裁定：fixture 尽量对齐语料真实形态，解析器两侧都要能处理。
  const skel = buildSkeleton({
    width: 64, height: 64, nonessential: false,
    bones: [{ name: 'root', length: 0 }],
    slots: [{ name: 'slot-mesh', boneIndex: 0, attachment: 'mesh-plain' }],
    skinEntries: [{ slotIndex: 0, attachments: [{
      type: 'mesh', name: 'mesh-plain', path: null,
      uvs: [0, 0, 1, 0, 0.5, 1],
      vertices: [0, 0, 32, 0, 16, 32],
      triangles: [0, 1, 2], hull: 3,
    }] }],
  });
  return {
    name: '16-minimal-mesh-plain',
    files: {
      '16-minimal-mesh-plain.skel.bytes': skel,
      '16-minimal-mesh-plain.atlas.txt': Buffer.from(atlasText('16-minimal-mesh-plain.png', 64, 64,
        [{ name: 'mesh-plain', x: 0, y: 0, w: 32, h: 32 }]), 'utf8'),
      '16-minimal-mesh-plain.png': pngBytes(64, 64, [0, 200, 200, 255]),
    },
  };
}

// ---------------------------------------------------------------------------
// Task 6 A4：共享图集布局（17 / 18 / 19 —— **目录即单位**，与 01–16 的扁平文件不同）
// ---------------------------------------------------------------------------
//
// 目录形态逐字对齐语料**唯一一处** `h0164s5/watch/`（A1 取证
// `docs/skeleton/shared-atlas-layout-evidence.md`）：
//
//   <parent>/<dir>/<parent>_<dir>.atlas.txt          ← 图集名 = 上级目录名 + '_' + 目录名
//   <parent>/<dir>/<parent>_<dir>_<role>.skel.bytes  ← 骨架名带角色后缀 ⇒ 与图集**不同名**
//   <parent>/<dir>/<parent>_<dir>.png                ← 单页，两个骨架共用
//
// 注意：**不能**用"与目录同名"来命名图集 —— 语料里严格 `<dir>.atlas.txt` 是 **0 例**（A1 §2）。

function sharedAtlasSkeleton(role, bone, region, color) {
  return buildSkeleton({
    hash: 'loongbones-fixture-' + role, width: 64, height: 64, nonessential: false,
    bones: [{ name: 'root', length: 0 }, { name: bone, parentIndex: 0, x: 8, length: 20 }],
    slots: [{ name: 'slot-' + role, boneIndex: 1, attachment: region }],
    skinEntries: [{ slotIndex: 0, attachments: [{ name: region, path: null, width: 32, height: 32 }] }],
  });
}

function fixtureSharedAtlas() {
  // 17 正向：2 个骨架共用 1 份图集 + 1 张单页 PNG（语料 2 个单位同形态）。
  // 图集里两个 region 各给一个骨架 ⇒ 若实现把两个骨架都指到"第一个 region"，
  // 另一个骨架的 region 名就会未命中（可被断言抓住），因此不能靠"随便取一个"通过。
  const dir = '17-shared-atlas/pair/hub';
  return {
    name: '17-shared-atlas',
    files: {
      [dir + '/pair_hub_left.skel.bytes']: sharedAtlasSkeleton('pair-hub-left', 'arm', 'hub_left'),
      [dir + '/pair_hub_right.skel.bytes']: sharedAtlasSkeleton('pair-hub-right', 'leg', 'hub_right'),
      [dir + '/pair_hub.atlas.txt']: Buffer.from(atlasText('pair_hub.png', 64, 64,
        [{ name: 'hub_left', x: 0, y: 0, w: 32, h: 32 },
         { name: 'hub_right', x: 32, y: 0, w: 32, h: 32 }]), 'utf8'),
      [dir + '/pair_hub.png']: pngBytes(64, 64, [40, 160, 220, 255]),
    },
  };
}

function fixtureAmbiguousAtlas() {
  // 18 负向（Q6-2 裁定「歧义拒绝」）：目录内 2 个候选图集，且**都不与骨架同名**。
  // 两个图集都含 `duo_region` ⇒ 无论选哪个"都能解析成功"，所以只有**显式拒绝**才算对；
  // 若实现"取第一个"就查不出来 —— 这正是本 fixture 要钉住的语义。
  const dir = '18-ambiguous-atlas/amb/duo';
  return {
    name: '18-ambiguous-atlas',
    files: {
      [dir + '/duo_unit.skel.bytes']: sharedAtlasSkeleton('duo-unit', 'arm', 'duo_region'),
      [dir + '/duo_alpha.atlas.txt']: Buffer.from(atlasText('duo_alpha.png', 32, 32,
        [{ name: 'duo_region', x: 0, y: 0, w: 16, h: 16 }]), 'utf8'),
      [dir + '/duo_beta.atlas.txt']: Buffer.from(atlasText('duo_beta.png', 32, 32,
        [{ name: 'duo_region', x: 0, y: 0, w: 16, h: 16 }]), 'utf8'),
      [dir + '/duo_alpha.png']: pngBytes(32, 32, [220, 80, 80, 255]),
      [dir + '/duo_beta.png']: pngBytes(32, 32, [80, 220, 80, 255]),
    },
  };
}

function fixtureMultiAtlasSameName() {
  // 19 守卫（阶段 B 回归的**具名样本**）：目录内有 2 个图集，但**骨架同名的一个存在**。
  // 语料实证：5 个目录含 ≥2 张图集、11 个骨架全靠「同名优先」才解析正确（A1 复核 §4）。
  // 判据：
  //   ✅ 取 `pick_unit.atlas.txt`（同名优先），region `pick_region` 命中；
  //   ❌ 判成歧义并拒绝 —— 错（语料里 11 个单位会全部导入失败）；
  //   ❌ 取到 `decoy.atlas.txt` —— 错；且因 decoy 里**没有** `pick_region`，
  //      会表现为 region 未命中（可定位，不是静默错值）。
  const dir = '19-multi-atlas-same-name/multi/pick';
  return {
    name: '19-multi-atlas-same-name',
    files: {
      [dir + '/pick_unit.skel.bytes']: sharedAtlasSkeleton('pick-unit', 'arm', 'pick_region'),
      [dir + '/pick_unit.atlas.txt']: Buffer.from(atlasText('pick_unit.png', 32, 32,
        [{ name: 'pick_region', x: 0, y: 0, w: 16, h: 16 }]), 'utf8'),
      [dir + '/pick_unit.png']: pngBytes(32, 32, [240, 200, 60, 255]),
      [dir + '/decoy.atlas.txt']: Buffer.from(atlasText('decoy.png', 32, 32,
        [{ name: 'decoy_region', x: 0, y: 0, w: 16, h: 16 }]), 'utf8'),
      [dir + '/decoy.png']: pngBytes(32, 32, [120, 120, 240, 255]),
    },
  };
}

// ---------------------------------------------------------------------------
// 写出 + 可选回读自校验
// ---------------------------------------------------------------------------

fs.mkdirSync(OUT_DIR, { recursive: true });
const all = [fixtureMinimal(), fixtureNested(), fixtureBadParentIndex(), fixtureTruncated(),
             fixtureNonFiniteFloat(),
             fixtureMinimalMesh(), fixtureWeightedMesh(), fixtureLinkedMesh(),
             fixtureMeshBadTriangle(), fixtureMeshBadBoneIndex(), fixtureConstraints(),
             fixtureMeshBadWeightSumZero(), fixtureMeshBadWeightSumNegative(),
             fixtureLinkedMeshImplicitSkin(), fixtureIkMixZero(),
             fixtureMinimalMeshPlain(),
             fixtureSharedAtlas(), fixtureAmbiguousAtlas(), fixtureMultiAtlasSameName()];
const index = [];
for (const fx of all) {
  for (const [rel, buf] of Object.entries(fx.files)) {
    const out = path.join(OUT_DIR, rel);
    fs.mkdirSync(path.dirname(out), { recursive: true });   // 17–19 是**目录**布局，需要建子目录
    fs.writeFileSync(out, buf);
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
    const rels = Object.keys(fx.files).filter((k) => k.endsWith('.skel.bytes'));
    for (const rel of rels) {
      // 17 起一个 fixture 可以有**多个**骨架（共享图集 = 多单位指向同一图集）⇒ 逐个回读
      const label = rels.length > 1 ? `${fx.name} [${path.basename(rel)}]` : fx.name;
      const buf = fs.readFileSync(path.join(OUT_DIR, rel));
      const log = new IssueLog();
      try {
        const ir = readSkeletonBinary32(buf, log, { strictEof: true });
        const warns = (log.issues ?? []).filter((x) => x.level === 'warn');
        console.log(`  ${label}: ok  bones=${ir.bones.length} slots=${ir.slots.length} skins=${ir.skins.length}` +
          ` anims=${(ir.animations ?? []).length} version=${ir.skeleton?.version}` +
          ` warnings=${warns.length}`);
        for (const wn of warns) console.log(`     warn: ${wn.where} — ${wn.message}`);
      } catch (err) {
        console.log(`  ${label}: FAIL — ${String(err.message ?? err)}`);
      }
    }
  }
}
console.log(`\n输出目录：${OUT_DIR}`);
