# skeleton-probe — Spine 3.2.01 骨架只读探针（Task 4 阶段 A）

对应规范：`项目规范.md` §5 Phase 1、§8.2（工具登记）；任务：`Agent_Task/Task4.md` §2.1 / §2.4。

## 它做什么

对语料根目录下所有 `*.skel.bytes` 做**只读**解析，产出骨架契约所需的**数值证据**：

| 脚本 | 产出 |
|---|---|
| `probe.mjs` | 版本/三件套、对象计数、引用闭合、名称唯一性、附件 `path` 语义、图集形态、附件→region 查找率 |
| `png-page-probe.mjs` | 用像素判据判定「图集页坐标空间」（页声明 size vs PNG 实际尺寸） |
| `make-fixture.mjs` | 生成**合成** fixture（不含任何游戏资产），并可选回读自校验 |
| `emit-samples.mjs` | 从 `probe.mjs` 的 JSON 输出中摘录 16 个样本，打印 `docs/skeleton/sample-manifest.md` 用的数据表 |
| `tc1-space-control.mjs` | **Task 5 A1 判据对照实验**：用已知答案的样本（多骨骼顶点自洽性、region.x/y、同 slot 的 region↔mesh）检验「顶点空间判据」本身是否成立；判据无效时会直接暴露 |
| `type-byte-census.mjs` | **Task 5 A1（TBD-F）**：附件**类型字节**直接普查。不复用旧 TS Reader，按 `binary_reader.cpp`（B4 已验证布局）独立走字节，给出 0…5 各类型的 Full/Primary 计数与 linked 附件的父引用解析；自带与 A1 已知计数的交叉核对表 |
| `mesh-constraint-probe.mjs` | **Task 5 A1**：第二批静态数据取证 —— 加权顶点（骨骼数/权重和/索引越界）、顶点空间判据、`hull`/`edges` 出现率、boundingbox 0 例复核、LinkedMesh 逐条、IK / transform 约束取值域（Full 与 Primary 双口径） |
| `weight-sum-census.mjs` | **Task 5 A5（D37 证据）**：加权顶点**权重和**分布普查 —— `sum = 0` / `sum < 0` / 非有限 / `|sum−1| > tol` 的计数与 min/max，以及单条负权重绑定数。用于判定 D37 的拒绝路径在真实语料里**有没有触发样本** |

`probe.mjs` 逐文件记录：相对路径、**SHA-256**、体积、版本、`nonessential`/`imagesPath`、
骨骼/插槽/皮肤/动画/时间线/约束/事件计数、附件按类型计数、**引用闭合**（骨骼父索引、
插槽骨骼索引、IK/transform 目标索引、skin 插槽索引）、名称重复数、附件 `path` 与
附件名不一致数、region rotation/scale 形态、mesh UV 与三角索引越界数、
图集页/region 形态（rotate/trim/size≠orig/index）、PNG 存在性与 IHDR 尺寸、
附件→region 查找命中率，以及解析失败分类（strictEof / 非法索引 / 非法浮点）。

## 它不做什么

- **不修改语料**：不移动、不改写、不删除、不复制任何游戏文件。
- **不把游戏资产写进仓库**：输出只有元数据（相对路径 + hash + 计数 + 分类计数）。
  `png-page-probe.mjs` 只在内存中解码 alpha 用于统计，**不落地任何图像**。
- **不写绝对路径**：语料根由参数/环境变量提供，输出内只存相对路径。
- **不是产品读取器**：解析复用旧项目 `converter/src/read/binary32.ts`（证据参照）。
  阶段 B 的产品读取器按批准的 schema 契约**独立实现**，不依赖本探针、不引入 Spine Runtime。

## 运行

必须在**旧项目根**（`D:/Works/Spine`，Reader 所在处）之下运行；脚本用 `../../..` 定位它。

```bash
# 全量只读扫描（约 1113 个文件，10–60 s）
node tools/skeleton-probe/probe.mjs "<语料根目录>" -o tools/skeleton-probe/out

# 或环境变量 / -r
LOONGBONES_CORPUS_ROOT="<语料根目录>" node tools/skeleton-probe/probe.mjs

# 只跑合成 fixture（回归用）
node tools/skeleton-probe/probe.mjs docs/skeleton/fixtures -o tools/skeleton-probe/out-fixtures

# 图集页坐标空间判定（默认挑差异最大的 40 页）
node tools/skeleton-probe/png-page-probe.mjs "<语料根目录>" --limit 24

# 生成合成 fixture 并回读自校验
node tools/skeleton-probe/make-fixture.mjs --verify

# Task 5 A1：第二批静态数据（mesh / 蒙皮 / 约束）取证
node tools/skeleton-probe/mesh-constraint-probe.mjs "<语料根目录>" -o tools/skeleton-probe/out

# Task 5 A1：顶点空间判据的对照实验（判据自检，必须和取证一起跑）
node tools/skeleton-probe/tc1-space-control.mjs "<语料根目录>"

# Task 5 A1（TBD-F）：附件类型字节直接普查（不依赖旧 Reader 的继承回退）
node tools/skeleton-probe/type-byte-census.mjs "<语料根目录>" -o tools/skeleton-probe/out

# Task 5 A5（D37）：加权顶点权重和分布普查（约 2 s）
node tools/skeleton-probe/weight-sum-census.mjs "<语料根目录>"
```

## 输入 / 输出

| | |
|---|---|
| 输入 | 语料根目录（只读）；默认取 `$LOONGBONES_CORPUS_ROOT` 或第一个位置参数 |
| 输出 | `out/skeleton-probe-report.json` — 机器可读全量结果 |
| | `out/skeleton-probe-report.md` — 人读摘要 |
| | `out/atlas-page-space-verdict.json` — 页坐标空间判定明细 |
| | `out/mesh-constraint-probe.json` / `.md` — Task 5 A1 第二批数据取证结果 |
| | `out/type-byte-census.md` — Task 5 A1（TBD-F）附件类型字节普查与交叉核对 |
| | `docs/skeleton/fixtures/*` — 合成 fixture（由 `make-fixture.mjs` 生成） |

`out/` 是生成目录，可随时删除后按上文命令重建；不要手工编辑。
`docs/skeleton/fixtures/` 是**入库**的合成 fixture，可再分发，不含游戏资产。

## 依赖

- Node.js v22+（本机 v24；用内建 type-stripping 直接 import 旧项目的 `.ts`，无需构建步骤）
- 旧项目 Reader：`converter/src/read/binary32.ts`、`converter/src/model.ts`、`converter/src/atlas.ts`
- 仅 `node:zlib`（PNG 解码）+ 标准库；**零第三方依赖**

## 可重跑性

可安全重复运行；输出确定（文件按路径排序，统计为纯聚合）。

## 已知限制

- `probe.mjs` 的图集统计只取**第 1 页**做「页声明尺寸 vs PNG」比对（语料实测全为单页图集）。
- `png-page-probe.mjs` 只支持 8 位非隔行 PNG；其它形态记为 `unsupported`，不猜测。
- 时间线 `shear` 计入骨骼时间线；`transform` 计入 `transformTimelines`。
