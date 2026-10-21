# corpus-scan — Sdorica Spine 3.2.01 语料只读扫描器

Loongbones **Phase 0** 数据盘点工具。把真实样本做成可复现的清单基线，
供后续阶段的回归分母与样本选择使用。

对应规范：`Loongbones/项目规范.md` §5 Phase 0、§12 交付物 2/3、§8.2（工具登记）。

## 它做什么

对语料根目录下所有 `*.skel.bytes` 做**只读**扫描，逐文件记录：

- 相对路径、字节大小、**SHA-256**
- 文件头 / 版本号
- 伴随文件是否存在（`.atlas.txt` / `.atlas` / `.png`）
- 解析统计：骨骼、插槽、皮肤、动画、IK、transform、path 约束、时间线总数、
  region / mesh / 加权 mesh / 其他附件、deform / ik / event / bone / slot / drawOrder 时间线、
  最大网格顶点数
- 解析结果与失败原因（用 Reader 的 `strictEof` 识别截断/损坏）

并汇总总量、**963 vs 1113 口径分析**（唯一 hash 数、重复组数、顶层目录数）、版本分布。

## 它不做什么

- **不修改语料**：不移动、不改写、不删除、不复制任何游戏文件。
- **不把游戏资产写进仓库**：输出只有元数据（相对路径 + hash + 统计量）。
- **不写绝对路径**：语料根由参数/环境变量提供，manifest 内只存相对路径。

## 运行

```bash
# 方式一：环境变量
LOONGBONES_CORPUS_ROOT="D:/Game/Other/sdorica/working/full_export/assets/game/character/spinedata" \
  node tools/corpus-scan/scan.mjs

# 方式二：命令行参数
node tools/corpus-scan/scan.mjs "<语料根目录>" -o tools/corpus-scan/out

# 抽样试跑（不扫全量）
node tools/corpus-scan/scan.mjs "<语料根目录>" --limit 20
```

必须在**旧项目根**（`D:/Works/Spine`，Reader 所在处）或能按相对路径解析到它的位置运行；
脚本用 `../../..` 定位旧项目根来 import Reader。

## 输入 / 输出

| | |
|---|---|
| 输入 | 语料根目录（只读）；默认取 `$LOONGBONES_CORPUS_ROOT` |
| 输出 | `out/corpus-manifest.json` — 机器可读全量清单 |
| | `out/corpus-manifest.md` — 人读摘要 |
| | `out/corpus-failures.json` — 解析失败明细（无失败为空数组） |

`out/` 是生成目录，可随时删除后按上文命令重建；不要手工编辑。

## 依赖

- Node.js（开发机已有）
- 旧项目 Reader：`converter/src/read/binary32.ts` 与 `converter/src/model.ts`
  （已经 963 个真实文件验证；**这是 Phase 0 的分析工具，不是产品读取器**。
  产品读取器属 Phase 1/2 范围，届时按 schema 契约重写。）

## 可重跑性

可安全重复运行；输出确定（文件按路径排序，统计为纯聚合）。
全量约 1100 个文件，耗时约 1–2 分钟。
