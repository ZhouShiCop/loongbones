#!/usr/bin/env node
/**
 * 从 probe.mjs 的 JSON 输出摘录 16 个样本，打印 docs/skeleton/sample-manifest.md 用的数据表。
 *
 * 输出三张表：
 *   1) 三件套**路径 + 锁定**表 —— skeleton / atlas / PNG 各自的相对语料根路径、字节数、SHA-256
 *      （PNG 路径由图集页头 file 解析，多页图集会逐页列出）
 *   2) 结构表 —— 计数与闭合结论
 *
 * 只读元数据；不写绝对路径、不复制任何文件。
 */
import fs from 'node:fs';
const j = JSON.parse(fs.readFileSync(new URL('./out/skeleton-probe-report.json', import.meta.url), 'utf8'));
const WANT = [
  'h3181c2/h3181c2.skel.bytes',
  'b3310c3/b3310c3_cocoon/b3310c3_cocoon.skel.bytes',
  'h0261s5/s3/h3004c3.skel.bytes',
  'h0215s5/h0215s5_doll2/h0215s5_doll2.skel.bytes',
  'h0166s5/h0166s5_buoy/h0166s5_buoy.skel.bytes',
  'b0015s5/b0015s5_meat/b0015s5_meat.skel.bytes',
  'e0039c1/fx_e0039c1_s01/e0039c1_s01_bug.skel.bytes',
  't0269s5/s1/s1_shan1.skel.bytes',
  'h0210s5/h0210s5_s3/h0210s5_S3.skel.bytes',
  'h0040s5/bowl/h0040s5.skel.bytes',
  'h0133s5/h0133s5.skel.bytes',
  'h0164s5/watch/h0164s5_watch_minute.skel.bytes',
  'e0039c1/fx_e0039c1_s02/e0039c1_s02.skel.bytes',
  'b0001s3/b0001s3.skel.bytes',
  'b0146c3/b0146c3.skel.bytes',
  'h3687c4/h3687c4.skel.bytes',
];
const by = new Map(j.entries.map((e) => [e.path, e]));
const no = '—';

console.log('=== 表 1：三件套路径与锁定 ===');
console.log('| # | `.skel.bytes`（相对路径） | 字节 | SHA-256 | `.atlas.txt`（相对路径） | 字节 | SHA-256 | `.png`（相对路径） | 字节 | SHA-256 | 页数 |');
console.log('|---|---|---|---|---|---|---|---|---|---|---|');
WANT.forEach((p, i) => {
  const e = by.get(p);
  if (!e) { console.log(`| ${String(i + 1).padStart(2, '0')} | MISSING ${p} | | | | | | | | | |`); return; }
  const c = e.companionHashes ?? {};
  const at = c.atlasTxt;
  const pages = e.atlas?.pageDetails ?? [];
  const pg = pages[0];
  const cell = (x) => (x ? `${x.bytes} | \`${x.sha256}\`` : `${no} | ${no}`);
  const pngCell = pg
    ? `\`${pg.pngRelPath ?? no}\` | ${pg.pngBytes ?? no} | ${pg.pngSha256 ? `\`${pg.pngSha256}\`` : no}`
    : `${no} | ${no} | ${no}`;
  console.log(`| ${String(i + 1).padStart(2, '0')} | \`${e.path}\` | ${e.size} | \`${e.sha256}\` | ` +
    `\`${e.atlasTxtRelPath ?? no}\` | ${cell(at)} | ${pngCell} | ${pages.length || no} |`);
});

console.log('\n=== 表 1b：多纹理样本明细（页名 → PNG 相对路径）===');
console.log('| # | 页序 | 图集页头 file | PNG 相对路径 | 字节 | SHA-256 | 声明尺寸 | region 数 |');
console.log('|---|---|---|---|---|---|---|---|');
WANT.forEach((p, i) => {
  const e = by.get(p);
  const pages = e?.atlas?.pageDetails ?? [];
  if (pages.length === 0) { console.log(`| ${String(i + 1).padStart(2, '0')} | — | — | ${no} | — | — | — | — |`); return; }
  for (const pg of pages) {
    console.log(`| ${String(i + 1).padStart(2, '0')} | ${pg.order} | \`${pg.pageFile ?? no}\` | ` +
      `\`${pg.pngRelPath ?? no}\` | ${pg.pngBytes ?? no} | ${pg.pngSha256 ? `\`${pg.pngSha256}\`` : no} | ` +
      `${pg.declaredSize ? pg.declaredSize.join('×') : no} | ${pg.regions} |`);
  }
});

console.log('\n=== 表 2：结构与结论 ===');
console.log('| # | 版本 | 骨/槽/皮肤 | 动画/时间线 | IK/TC | region/mesh/加权 | deform | 页/region | 三件套 | 闭合 |');
console.log('|---|---|---|---|---|---|---|---|---|---|');
WANT.forEach((p, i) => {
  const e = by.get(p);
  if (!e) { console.log(`| ${String(i + 1).padStart(2, '0')} | MISSING | | | | | | | | |`); return; }
  const a = e.atlas ?? {};
  console.log([
    String(i + 1).padStart(2, '0'), e.version,
    `${e.counts.bones}/${e.counts.slots}/${e.counts.skins}`,
    `${e.counts.animations}/${e.counts.timelines}`,
    `${e.counts.ik}/${e.counts.transform}`,
    `${e.attachments.region}/${e.attachments.mesh}/${e.attachments.weightedMesh}`,
    e.timelineKinds.deform,
    `${a.pages ?? '-'}/${a.regions ?? '-'}`,
    e.trio ? 'Y' : '**N**',
    e.referenceClosed ? 'ok' : 'NOT-CLOSED',
  ].join(' | '));
});
