# Task 4 B4：Spine 二进制 Reader（仅解析，不映射）

**状态：全语料精确到达 EOF —— Full 1113/1113、Primary 963/963。** 27 项测试通过；
manifest **16 个**锁定样本骨/槽/皮肤/region 计数 **16/16 一致**；扫描工具退出码 0。

> ## 交付边界：结构可读（structural read），不等于可转换
>
> **做到的**：字节布局 100% 覆盖。1113 个文件精确到最后一字节，文件里再没有
> 未知字节 —— 头/骨/槽/IK/transform/皮肤/事件/动画（含全部时间线）的字段顺序
> 与长度都已确定。
>
> **没做的**：`SourceSkeleton` 只保存 **region 附件**的完整数据。mesh 族、
> boundingbox、linked mesh、**全部动画时间线**、IK/transform 约束的参数
> —— 每个字节都知道怎么读，但**读完即弃，只留计数**。
>
> 因此本阶段**不得**宣称"可渲染网格 / 可蒙皮 / 可播放动画"。导入哪些、丢弃哪些、
> 丢弃时如何报告，由 B5 按 §9 的清单裁定。

阻塞已解除。关键是停止"猜类型映射"、改用**本工作区已过语料验证的
`converter/src/read/binary32.ts`** 作为 3.2 布局的权威参照：它给出
`region=0, boundingbox=1, mesh=2, weightedmesh=3, linkedmesh=4, weightedlinkedmesh=5`
（注意 **3 是加权网格**，与官方 3.5+ 枚举里 `Linkedmesh=3` 的顺序不同），
并指出 sdorica 的加权顶点里 **boneCount / boneIndex 是 float 而非 varint**。
按此实现后 skins 段失败 1054 → 0，随后又补上 ik/transform 段形状与骨骼的非必要
颜色字段，最终全语料 0 失败。

全部改动均**独立实现**（读 TS 只作为格式参照，未复制代码），且每一处仍以
"能否精确到达 EOF" 在 1113 个真实文件上判定。

---

## 1. 交付物

| 文件 | 作用 |
|---|---|
| `importers/spine32/include/loongbones/importers/spine32/source_model.hpp` | 源格式私有 IR（**源格式对象**，不是 canonical 模型） |
| `importers/spine32/include/loongbones/importers/spine32/binary_reader.hpp` | Reader 公开 API + 诊断码枚举 |
| `importers/spine32/src/binary_reader.cpp` | Reader 实现 |
| `tests/importers/spine32/test_binary_reader.cpp` | 27 项自动化测试 |
| `tools/binary-read-tests/` | 测试可执行目标（进 CTest 默认集合） |
| `tools/binary-read-smoke/` | 语料只读扫描（默认 OFF，需本地语料根） |
| `tools/skeleton-probe/mesh-tail.mjs` | 逐字段解码打点（用于字节走查，非产品代码） |
| `tools/skeleton-probe/dump.mjs` / `scan16.mjs` | 按偏移 dump / 按 uint16 扫描，定位数组边界 |
| `tools/skeleton-probe/full-decode.mjs` | 全文件逐字段解码打点（可切 deform 帧假设） |
| `tools/skeleton-probe/tail-search.mjs` | 从锚点穷举尾段布局组合，按「精确 EOF」判定 |

改动：`importers/spine32/CMakeLists.txt`、`CMakeLists.txt`（新增两个子目录）。

### 相对《Task4_Implementation_Plan.md》的一处偏差

方案把 `source_model.hpp` 列在 `importers/spine32/src/`（私有）。实际放在
`include/`，因为 `ReadResult::skeleton` **按值**返回 `SourceSkeleton`，
公开头文件必须能看到完整定义。若坚持留在 `src/` 只能改成 `unique_ptr` +
前向声明，得不偿失。IR 的"私有"语义靠命名空间与 B5 不向 Domain 暴露来保证，
不靠文件路径。

---

## 2. 字节布局

依据：`docs/skeleton/field-mapping.md` + 对真实语料的逐字节走查
（样本 02 `b3310c3_cocoon`，425 字节，手工解码到 EOF 逐段对齐）。
附件类型集合与字段顺序参照本工作区已验证的 `converter/src/read/binary32.ts`，
但仍用 1113 个文件的「精确到达 EOF」独立判定（见 §7）。

```
header      hash(string) version(string) width(f32) height(f32) nonessential(u8)
            [imagesPath(string)]                      // 仅 nonessential
bones       count(varint)
            per i: name(string) [parentIndex(varint, i>0)]
                   rotation,x,y,scaleX,scaleY,shearX,shearY,length (8×f32)
                   inheritScale(u8) inheritRotation(u8)
                   [color(i32)]                       // 仅 nonessential
ik          count(varint) per: name, boneCount, boneIndex…, target, mix, bendPositive
transform   count(varint) per: name, boneIndex, target, 5×mix, 5×offset
slots       count(varint) per: name, boneIndex, color(i32 0xRRGGBBAA),
                               attachment(string?), blend(u8)
skins       default 皮肤（**无名字字段**）→ extraSkinCount(varint) → 每个 (name + 内容)
  skin      slotCount(varint) per: slotIndex(varint) attCount(varint)
            per att: key(string) name(string?) type(u8) <类型专属>
  region(0) path(string?) rotation,x,y,scaleX,scaleY,width,height (7×f32) color(i32)
  bbox(1)   vertexCount(varint) 顶点(2n×f32) [color(i32)]        // 仅 nonessential
  mesh(2)   path color(i32) vertexCount(varint) UV(2n×f32)
            三角(u16 数组) 顶点(2n×f32) hull(varint)
            [edges(u16 数组) width(f32) height(f32)]             // 仅 nonessential
  wmesh(3)  同 mesh(2)，但顶点区见下
  link(4)   path color(i32) skinName(string) parent(string) inheritDeform(u8)
            [width(f32) height(f32)]                             // 仅 nonessential
  wlink(5)  同 link(4)
events      count(varint) per: name, int, float, string
animations  count(varint) per:
            name(string)
            slotCount(varint) per: slotIndex(varint) timelineCount(varint) → timelines
            boneCount(varint) per: boneIndex(varint) timelineCount(varint) → timelines
            ikCount(varint) per: ikIndex(varint) frameCount(varint) → 帧
            transformCount(varint) per: tcIndex(varint) frameCount(varint) → 帧
            deformCount(varint) → 见下
            drawOrderCount(varint) / eventCount(varint)
timeline    type(u8) frameCount(varint) frames[time(f32) + 类型专属值]
            + 帧间 curve 字节（**非最后一帧之后**）：0=linear 1=stepped 2=bezier(4×f32)

deform 段   skinCount(varint) → 逐 skin(skinIndex, slotCount)
            → 逐 slot(slotIndex, attachmentCount)
            → 逐附件(attachmentName(string), frameCount(varint))
            → 逐帧 time(f32) end(varint) [若 end≠0: start(varint) + end 个 f32]
            → 帧间 curve

加权顶点(type 3) 每顶点 boneCount(**f32**) + boneCount × (boneIndex(**f32**), x, y, w)
```

**关键实测点**（都是走查出来的，不是猜的）：

- **region 是 7 个 float**（rotation/x/y/scaleX/scaleY/width/height），不是 8。
  第一版按 8 个写，读 4 字节错位后整个结构崩掉、偏移跳到 811。
- **第 0 根骨骼不写 parentIndex 字段**；语料 1113 个文件恰好 1113 个根，
  即 `-1` 这个取值在实践中不出现（实现仍按 32 位无符号 varint 接受）。
- **动画段的分组结构是 `(index, timelineCount)` 再跟 timelines**，
  不是 `(timelineCount, index)`：样本 02 骨骼组读到 `00 04`（boneIndex=0，
  4 条时间线）才对得上，反过来会读成 "0 条时间线"。
- **curve 字节只写在非最后一帧之后**，且 attachment / drawOrder / event
  时间线**不写** curve。样本 02 的 color 时间线 2 帧之间恰好 1 个 `00`，
  attachment 时间线 2 帧之间没有字节。
- **动画末尾是 5 个计数**：ik / transform / deform / drawOrder / event。
  样本 02 动画 1 在骨骼时间线后是 `00 00 00 01 … 00`，正好 5 个计数 + 1 条
  drawOrder，收在文件最后一个字节。
- **ik / transform 段与 slot / bone 段形状不同**：约束下标之后**直接是帧数**，
  没有「timelineCount」也没有「类型字节」两层。曾按统一形状读，把帧的第一个
  浮点当成了类型字节，全语料炸出 547 例 `UNKNOWN_TIMELINE_TYPE` + 196 例
  `COUNT_LIMIT`。
- **deform 帧的 `end` 是"相对于 start 的长度"**：`end≠0` 时才跟一个 `start`
  （顶点起始下标），浮点个数就是 `end` 原值。曾按 `end - start` 算，在
  `m0020c2`（end=1、start=10）等样本上会得到负数。
- **nonessential=1 时每根骨骼多 4 字节颜色**。漏掉它只影响 4 个文件
  （`e0002s3` / `h0270s5` / `h0270s5_whale` / `h0270s5_miniwhale`），
  表现为在 `bones/1/name` 处 `STRING_TOO_LONG` —— 因为骨骼段整体错位后，
  把浮点字节读成了字符串长度。

---

## 3. Reader 契约

```cpp
ReadResult readSkeletonBinary32(const std::vector<std::uint8_t>& bytes);
```

- **只解析，不映射**：输出 `SourceSkeleton`，字段沿用源的索引与源语义；
  索引 → 稳定 ID、颜色 i32 → 0..1 浮点等都留给 B5。
- **strict EOF**：解析完还有尾随字节即失败（`kTrailingBytes`）。
- **失败不留半成品**：`ok == false` 时 `skeleton` 为空。
- **诊断只记第一条**（后续报错多是同一处损坏的连带结果，堆砌反而掩盖根因），
  每条带 `code` / `message` / `section` / `offset`。
- **读取前检查**：剩余长度、varint ≤ 5 字节且值 ≤ 2³²−1、字符串 ≤ 4096、
  数组长度 ≤ 2²⁰；float32 读出**立即** `isfinite`，拒绝 NaN/±Inf。
- **结构闭合**：骨骼父索引必须 ∈ {−1, 0..i−1}（拒绝越界 / 自环 / 前向引用）；
  插槽 `boneIndex` 必须在骨骼数内；`blend` ∈ 0..3。
- **版本只接受头版本 `3.2.01`**。

诊断码：

| code | 含义 |
|---|---|
| `TRUNCATED` | 剩余字节不足 |
| `TRAILING_BYTES` | strict EOF 失败 |
| `VARINT_OVERFLOW` / `STRING_TOO_LONG` / `COUNT_LIMIT` | 越界长度前缀 |
| `UNSUPPORTED_VERSION` | 头版本不是 3.2.01 |
| `NON_FINITE_FLOAT` | NaN / ±Inf |
| `UNKNOWN_ATTACHMENT_TYPE` | 附件类型字节**不在已验证集合 0..5 内**（即 ≥6）。注意 1..5 是**可遍历**的，不会触发本码 |
| `UNKNOWN_TIMELINE_TYPE` / `UNKNOWN_BLEND_MODE` | 未知枚举 |
| `INDEX_OUT_OF_RANGE` | 索引越界 / 自环 / 前向引用 |
| `INVALID_UTF8` | 预留（当前不做 UTF-8 校验，见 §8） |

---

## 4. 测试（27 项，全部通过）

`tools/binary-read-tests/`，`ctest -R binary_read_tests` 或直接跑可执行文件。

覆盖：最小合法文档、头字段、hash≠version、非 3.2.01 版本、尾随字节、截断、
空输入、骨骼 rotation=NaN、附件 width=+Inf / −Inf（并断言诊断带段名与 offset）、
骨骼前向引用、父索引越界、插槽骨骼索引越界、未知附件类型字节**拒绝**、
boundingbox / mesh / weightedmesh **可遍历**、加权网格 boneCount 非整数**拒绝**、
blend 越界、varint 超长、字符串超上限、数组长度超上限、0xFFFFFFFF 合法 varint、
nonessential+imagesPath、空 default 皮肤、失败不留半成品、诊断码字符串稳定。

---

## 5. 全语料只读扫描（1113 个 `.skel.bytes`）

`tools/binary-read-smoke/`（`LOONGBONES_BUILD_BINARY_READ_SMOKE=ON` 后手工跑）。
只读：不写文件、不复制语料、不输出像素。

```
Full    1113 | ok 1113 | fail 0
Primary  963 | ok  963 | fail 0

失败分类：（空）
失败位置：skins=0  animations=0  其它=0
锁定样本 16/16 一致      退出码 0
```

**1113 个单位全部严格到达 EOF**（不多不少读到最后一个字节），
即当前实现的布局对语料里出现的全部 region / boundingbox / mesh / weightedmesh /
linkedmesh / weightedlinkedmesh 附件与全部动画段时间线组合都成立。

### 5.1 两个分母（Full / Primary）与退出码

`sample-manifest.md` §4 给出两套语料级统计，**分母不同，不得混用**：

| 口径 | 定义 | 本工具实测 |
|---|---|---|
| **Full** | 语料根下**全部** `.skel.bytes` | 1113 / ok 1113 |
| **Primary** | `<单位目录>/<同名>.skel.bytes`，且**恰好两层**（单位目录直接在语料根下） | 963 / ok 963 |

Primary 的判定曾写错成「任意深度的父目录同名」，实测得 996 而非 manifest 的 963；
按「恰好两层」修正后与 manifest 一致。这个数字因此也是**口径的自检**：它不是
963 就说明根的层级给错了。

**退出码**（见 `tools/binary-read-smoke/README.md`）：只有 0 可作验收证据。
语料根给错时工具照样能扫到上千个文件，此时锁定样本一个都匹配不到，必须判失败
（实测：给 sdorica 上层目录 → 扫到 1131 个文件、锁定样本 0/0、**退出码 4**）。

逐轮推进：

| 轮次 | 改动 | ok |
|---|---|---|
| 初始 | 只 region | 25 |
| 一 | mesh 尾部只吃一个 varint | 25 |
| 二 | deform 段改 `skinIndex/slotIndex` 形状 + drawOrder 无 frameCount 层 | 38 |
| 三 | deform 帧顶点数按 `end`（非 `end−start`） | 50 |
| 四 | **附件类型改用转换器映射 + 加权顶点 boneCount/boneIndex 为 float** | 237 |
| 五 | ik / transform 段去掉 timelineCount/type 两层 | 1109 |
| 六 | nonessential 时骨骼多 4 字节颜色 | **1113** |

---

## 6. manifest 锁定样本对照（16 个，全列）

期望值取自 `docs/skeleton/sample-manifest.md` §1（路径）与 §2（骨/槽/皮肤、
region/mesh/加权），**不是由本工具反推**。顺序即 manifest 编号。

| # | 样本 | 期望 骨/槽/皮肤/region | 实测 | 非 region 附件 | |
|---|---|---|---|---|---|
| 01 | `h3181c2` | 31/19/1/14 | 31/19/1/14 | 8 | **一致** |
| 02 | `b3310c3_cocoon` | 1/1/1/1 | 1/1/1/1 | 0 | **一致** |
| 03 | `h3004c3` (s3) | 55/17/1/9 | 55/17/1/9 | 8 | **一致** |
| 04 | `h0215s5_doll2` | 25/12/1/7 | 25/12/1/7 | 5 | **一致** |
| 05 | `h0166s5_buoy` | 16/14/1/10 | 16/14/1/10 | 4 | **一致** |
| 06 | `b0015s5_meat` | 2/1/1/0 | 2/1/1/0 | 1 | **一致** |
| 07 | `e0039c1_s01_bug` | 4/1/1/0 | 4/1/1/0 | 1 | **一致** |
| 08 | `s1_shan1` | 16/6/1/1 | 16/6/1/1 | 5 | **一致** |
| 09 | `h0210s5_S3` | 25/11/1/2 | 25/11/1/2 | 9 | **一致** |
| 10 | `h0040s5` (bowl) | 2/2/1/2 | 2/2/1/2 | 0 | **一致** |
| 11 | `h0133s5` | 105/43/1/55 | 105/43/1/55 | 28 | **一致** |
| 12 | `h0164s5_watch_minute` | 3/1/1/1 | 3/1/1/1 | 0 | **一致** |
| 13 | `e0039c1_s02` | 4/2/1/2 | 4/2/1/2 | 0 | **一致** |
| 14 | `b0001s3` | 29/23/1/8 | 29/23/1/8 | 16 | **一致** |
| 15 | `b0146c3` | 40/26/1/18 | 40/26/1/18 | 10 | **一致** |
| 16 | `h3687c4` | 41/25/1/16 | 41/25/1/16 | 11 | **一致** |

**16/16 一致**。

「非 region 附件」一列与 manifest §2 的 `region/mesh/加权` 三列对得上：
例如 #04 是 `7/3/2` → 非 region = 3+2 = 5；#11 是 `55/3/25` → 3+25 = 28。
含 mesh 的样本 region 数逐项对上，说明 mesh 族被正确遍历、没有侵蚀到 region 计数。

> 工具此前只锁 11 个（04 / 07 / 08 / 09 / 12 缺），与实施计划「至少 16 个」不符。
> 本轮补全到 16。

---

## 7. 附件类型：改用权威参照后一次解决

1054/1113 个单位含 mesh 族附件，曾是唯一的大障碍。纯靠字节走查推类型映射
推了四轮没推出结果，改为**先确定 3.2 有哪几种类型**再验证。

### 7.1 权威来源

本地没有 3.2 runtime（只有 `resources/extern/vendor/spine-r35` 的 3.5 与
`spine-libgdx` 的 3.8 jar）。但**本工作区自己就有已过真实语料验证的 3.2 读取器**：
`converter/src/read/binary32.ts`（此前 converter 阶段用过）。它的常量给出了映射：

```ts
ATTACHMENT_TYPE = ['region', 'boundingbox', 'mesh', 'weightedmesh', 'linkedmesh', 'weightedlinkedmesh']
//                    0           1            2          3              4                5
```

**与官方 3.5+ 的枚举顺序不同**：官方 `AttachmentType { Region, Boundingbox, Mesh,
Linkedmesh, Path, Point, Clipping }` 里 `Linkedmesh = 3`。按官方顺序读正是前几轮
630 例 `STRING_TOO_LONG` 的根源。3.2 里 **3 = weightedmesh，4 = linkedmesh**。

另一处关键差异在加权顶点：

```ts
// sdorica 写法：每个顶点 boneCount(float) + 每组 [boneIndex(float), x, y, w]
// **骨骼数量、骨骼下标都是 float 而不是 varint**
```

官方 3.5 的 `ReadVertices` 是先读一个 weighted 布尔位，再用 varint 读骨数；
3.2 这里**两者都不是**。

### 7.2 独立字节验证（不是照抄）

拿 `b0001s3` 的 `L_finger`（type=3、vertexCount=40）复核：三角之后顶点区
2687..3742 共 **1056 字节 = 264 个 float**，且 `2.0,6.0` 这个模式每 **36 字节**
重复一次。按「每顶点 `boneCount` + `boneCount × (boneIdx, x, y, w)`」解析：

- 顶点 0：`2.0`(2 根骨) → `6.0`(骨 6)、`92.06`、`72.67`、`0.47` → `5.0`(骨 5)、
  `171.46`、`116.99`、`0.53` = 9 个 float = **36 字节** ✓ 与实测周期一致
- 264 = Σ(1 + 4·bcᵢ)，40 个顶点 → Σbcᵢ = 56；
  24 个顶点 1 根骨 + 16 个顶点 2 根骨 = 24×5 + 16×9 = **264** ✓ 精确相符
- 顶点块后：`1b`(hull=27) → `02` 正好是下一个 **slotIndex** ✓

对照之下，按官方写法（varint 骨数）会在顶点区首字节 `0x40` 读到 64 根骨，
超过该骨架 29 根骨的上限，必然错位。

### 7.3 结果

按此实现后 skins 段失败 **1054 → 0**，全语料 ok 50 → 237。
1..5 全部可遍历；只有 **6 及以上**仍按 `kUnknownAttachmentType` 拒绝。

---

## 8. 动画段：deform / drawOrder / ik / transform

### 8.1 deform 段结构（已确认）

原来是「skinName(string) + attachmentName + timelineCount」，**错的**。
用 `b0015s5_meat` 尾段穷举（唯一命中精确 EOF 的组合）确定为：

```
deformCount(varint)
  逐 skin：  skinIndex(varint) → slotCount(varint)
    逐 slot：  slotIndex(varint) → attachmentCount(varint)
      逐附件：  attachmentName(string) → frameCount(varint)
        逐帧：    time(f32) → endVertices(varint)
                  [endVertices≠0 才读] startVertices(varint) + 浮点 × endVertices
                  非末帧：curve
```

`meat` 的实测（1010 起，到 1033 结束）：

| 偏移 | 值 |
|---|---|
| 1010 | deformCount=1 |
| 1011 / 1012 | skinIndex=0 / slotCount=1 |
| 1013 / 1014 | slotIndex=0 / attachmentCount=1 |
| 1015-1020 | attachmentName = `"meat1"`（原先被误当成 skinName） |
| 1021 | frameCount=2 |
| 1022 | f0 t=0.0 endVertices=0（无顶点）curve=stepped |
| 1028 | f1 t=0.9677 endVertices=0 |

### 8.2 deform 帧的顶点区（已确认）

关键：`endVertices` **不是**浮点个数，其后**还跟一个 `startVertices`**（仅当
endVertices≠0）。按"vc 个浮点"读会少读一个 varint，使后续 curve 错位。

`h0137s4_umbrella_fly` 的帧 0 实测：

```
2045-2048  time = 0.0
2049       endVertices = 22
2050       startVertices = 0
2051-2138  22 个浮点（-247.4 / 562.4 / -71.9 / … ，像坐标）
2139       curve = 01 (stepped)  ✓ 正好接上
```

浮点个数按 `endVertices` 计（`startVertices` 照读但不参与长度）。
曾试过 `end - start`，在 `m0020c2` 等 6 个样本上得到负数（end=1、start=10），
全语料 ok 数 38 vs 50 → 证伪。

### 8.4 ik / transform 段（已确认）

**与 slot / bone 段形状不同**：约束下标之后**直接是帧数**，没有
「timelineCount」也没有「类型字节」两层。

```
ik:        count(varint) → 逐约束(ikIndex, frameCount) → 帧(time, mix, bendPositive, curve)
transform: count(varint) → 逐约束(tcIndex, frameCount) → 帧(time, 4×mix, curve)
```

按统一形状（index → timelineCount → 逐类型）读时，会把帧的第一个浮点当成类型
字节，全语料炸出 547 例 `UNKNOWN_TIMELINE_TYPE` 与 196 例 `COUNT_LIMIT`
（典型：`t0207s5_s1` 在 `ik/2/timelines/0/type`、`h0234s3` 在 `ik/0/timelines/0/frameCount`）。
改对后这两类全部消失。

### 8.5 试过并被语料证伪/修正的假设

| 假设 | 结果 |
|---|---|
| type 3/5 = linked mesh（照抄官方 3.5 枚举顺序） | 证伪。630 个文件在 `parent` 处 STRING_TOO_LONG |
| type 2..5 全按非加权 mesh | 证伪。ok 数不升，STRING_TOO_LONG 涨到 630 |
| type 4/5 按加权读顶点 | 证伪。ok 数不升，VARINT_OVERFLOW 55、COUNT_LIMIT 105 |
| 一次构建跑 7 种「放行掩码 × 加权掩码」组合 | **全部 ok=50，无一提升** —— 说明错的不在掩码，而在顶点编码本身 |
| 加权顶点用 varint 骨数（官方 3.x 写法） | 证伪。`0x40` 会被读成 64 根骨，超过骨架骨骼数 |
| deform 帧顶点数 = `end − start` | 证伪。`m0020c2` 上 end=1、start=10 得负数；38 vs 50 |
| 骨骼段没有非必要颜色字段 | 证伪。4 个 nonessential=1 的文件在 `bones/1/name` 处错位 |

前四条的共同教训是：在**不知道有哪些类型**的前提下枚举布局组合是无效的 ——
掩码组合有 7 种，真正的差异却在"顶点里的骨数是 float 还是 varint"这一处
不在枚举空间里。先确定类型集合、再验证编码，才是一次命中。

### 8.4 drawOrder 段

`count(varint)` 后**直接是帧**，没有 frameCount 这一层；每帧 = `time(f32) +
offsetCount(varint) + 逐 offset(slotIndex, offset)`，无 curve。
`meat` 实测：1033 处 count=3，三帧时间 0.0 / 1.3333 / 1.9333，offsetCount 均为 0，
随后 eventCount=0 → **精确落在 1050**。

## 9. B4 交付边界与 B5 交接清单

### 9.1 边界

B4 的验收是「**结构可读**」：1113/1113 精确到达 EOF，证明语料里再没有未知字节。
它**不**保证源语义已全部进入 IR。下表是 B4 结束时 `SourceSkeleton` 的真实内容 ——
**「只计数」意味着数据读完即弃**。

### 9.2 逐项交接清单

| 源数据 | B4 现状 | 语料规模（Full） | B5 待裁定 |
|---|---|---|---|
| 头（hash / version / 尺寸 / nonessential / imagesPath） | **入 IR** | 1113 | 可导入 |
| 骨骼（名 / 父索引 / 8 变换 / 继承布尔） | **入 IR** | 72256 | 可导入 |
| 插槽（名 / 骨骼索引 / 颜色 / 默认附件 / blend） | **入 IR** | 42955 | 可导入 |
| 皮肤骨架（default + 附加皮肤、槽索引、附件键） | **入 IR** | 1113 | 可导入 |
| **region 附件**（路径 / 变换 / 尺寸 / 颜色） | **入 IR** | 27459 | **当前唯一可完整转换的附件类型** |
| boundingbox | 只计数 | — | 待定（建议报告为不支持） |
| mesh / 加权 mesh（UV / 三角 / 顶点 / 权重 / hull） | 只计数 | 2981 + 17271，**涉及 1054/1113 个单位** | 待定 |
| linked mesh（parent / inheritDeform） | 只计数 | 2 | 待定 |
| **全部动画时间线** | 只计数 | 16046 动画 / 4,954,836 时间线 | 待定 |
| IK / transform 约束参数 | 只计数 | 2707 / 208 | 待定 |
| 事件定义 | 只计数 | 1 | 待定 |
| UTF-8 校验 | 未做 | — | 见 §10 |

（规模取自 `sample-manifest.md` §4 的 Full 列。）

### 9.3 B5 的三条硬约束

1. **不得静默丢失**。凡 B5 不导入的，必须产出带 `code` + 源位置 + **计数**的诊断，
   让用户知道丢了什么、丢了多少。计数现成：失败路径之外，`ReadResult::skeleton
   .unsupported` 已把各类的数量带出来，B5 直接消费，不需要重新解析。
2. **不得因为「结构可读」就默认「可转换」**。上表「只计数」的项字节布局**已验证**
   但数据**未落地**。B5 若决定导入，先扩 IR + 补测试，再谈映射 ——
   每一步仍可用 1113 个文件的 EOF 判据兜底，不会再有猜错导致的连锁错位。
3. **region 是当前唯一可完整转换的附件类型**；其余类型的转换能力不得由本阶段
   的通过数推定。

---

## 10. 纪律核对

| 约束 | 状态 |
|---|---|
| 不链接、不复制 Spine Runtime | 通过；C++ 全部自写。`converter/src/read/binary32.ts` 只作**格式参照**（读它的类型映射与字段顺序），未复制其代码 |
| 只实现已观察且已验证的字段 | 通过；每一段都以 1113 个文件的「精确到达 EOF」判定 |
| 未知/不支持显式诊断，不猜测、不静默丢弃 | 通过；附件类型 6+ 与时间线未知类型仍 `fail`，加权网格骨数非整数也 `fail` |
| 解析只到源 IR，不做源 → canonical 映射 | 通过；mesh 族只计数不进 IR |
| 诊断带稳定 code + 段名 + 字节 offset | 通过（有测试断言） |
| 语料路径不写进产物 | 通过；扫描工具只在运行时接收路径，输出相对路径 |
| 每次读取前检查剩余长度与整数溢出 | 通过 |
| float32 读出立即 isfinite | 通过 |
| 文件 strict EOF | 通过（**1113/1113** 全部精确到最后一字节） |

**未做**：UTF-8 校验（`kInvalidUtf8` 已预留但未实现）。字符串目前按字节原样
收进 `std::string`，未校验编码，也未做 UTF-16 转换。
若需要，"读出来时才校验、不 lossy 转换"是 B5 映射的既有原则，B4 未越界处理。
