# Task 4 B5：Spine Atlas Reader 与 Source→Canonical Mapper

**状态：实现完成。** 32 项自动化测试通过（atlas Reader 17 + Mapper 15）；全语料
**1113 个单位**走完 Reader → Atlas Reader → Mapper 链路，**region 未命中 0**；
全部 6 个 CTest 目标绿。

---

## 1. 交付物

| 文件 | 作用 |
|---|---|
| `importers/spine32/include/…/atlas_text_reader.hpp` | `.atlas` 文本 → 插件私有 IR（`SourceAtlasPage` / `SourceAtlasRegion`） |
| `importers/spine32/src/atlas_text_reader.cpp` | 解析实现 |
| `importers/spine32/include/…/mapper.hpp` | Source → canonical 映射 API |
| `importers/spine32/src/mapper.cpp` | 映射实现 |
| `tests/importers/spine32/test_atlas_text_reader.cpp` | 17 项合成断言 |
| `tests/importers/spine32/test_mapper.cpp` | 15 项合成断言 |
| `tools/spine32-mapper-tests/` | 上述 32 项的构建入口（进 CTest） |
| `tools/spine32-mapper-smoke/` | 语料只读扫描（默认 OFF，需本地语料根） |

改动：`importers/spine32/CMakeLists.txt`、根 `CMakeLists.txt`；
`importers/spine32/src/atlas_import.cpp` 改为复用新 Reader（删除文件内重复解析器约 300 行）。

---

## 2. Atlas 文本 Reader

### 2.1 为什么单独抽出来

B1 里这套解析器是 `atlas_import.cpp` 的**文件内私有**实现。B5 的 Mapper 需要独立
复用它、也需要能单独断言，故提出为公开 API。B1 行为不变（8 项测试仍绿）。

### 2.2 解析范围（B5 方案第 1 条）

- 页：块名即页头 `file`；`size` 为**页声明坐标尺寸**。
- 页属性 `format` / `filter` / `repeat` / `pma`：**解析并保留在 IR 里**，同时给
  warning —— 当前 Domain 模型没有落点，保留 ≠ 进入 canonical。
- 区域：`xy` / `size` / `orig` / `offset` / `rotate` / `index`。
- 多页：语料 0 例，靠合成文本覆盖（有 3 项断言）。

### 2.3 与 PNG IHDR 的关系（B5 方案第 2 条）

页**声明** `size` 存进 `SourceAtlasPage::width/height`；PNG IHDR 的像素尺寸是
`TextureAsset::pixelWidth/pixelHeight`（B1 已落地）。**两者分别保存，不互相换算**。
缩放 `pixel/page` 只在页层约定中出现，不缩放 Region 坐标（field-mapping §10.2：
1107/1111 单位两者不一致）。

### 2.4 一颗矩形规则（B5 方案第 3 条）

沿用 `AtlasRegion` 的 `packed` / `rotation` / `original` / `trimmed` / `offset`，
不另造第二套。`rotate: true → 90°`；`orig` 缺省时回退为 `size`；`trimmed = size`
（libGDX 不单独写 trim）。`offset` 方向与 `original` 一致，不受打包旋转影响。

---

## 3. Mapper（Source → canonical）

### 3.1 ID 全部由宿主发放

Mapper 只调用 `IIdFactory::create()`，自己不生成、不复用源下标。源下标 → ID 的解析
全部发生在本次导入的**局部映射表**里，不写入任何产物。
测试断言了每个种类发放的 ID **数量**与对象数一致（不多发、不漏发）。

### 3.2 附件 region 查找（B5 方案第 4 条）

键 = **`path ?? name`**（name 为空时回退到皮肤键），三级回退有独立断言。

| 情况 | 处置 | 等级 |
|---|---|---|
| 精确命中 | 正常建 `SkinEntry` | — |
| 未命中，按 `trim()` 重试命中 | 降级匹配，记 `ATTACHMENT_REGION_DEGRADED`（T-B3） | **warning** |
| 仍未命中 | `ATTACHMENT_REGION_UNRESOLVED`，消息里带查找键 | **error** → 拒绝候选 |

`allowTrailingSpaceFallback` 关闭时，同一个尾随空格输入必须变成 error ——
有断言锁住"降级开关不是免死金牌"。

### 3.3 不支持的能力：只计数 + 诊断，不建半成品（B5 方案第 5 条）

B4 遍历并计数、但不入 IR 的能力（mesh 族 / boundingbox / 动画时间线 / IK / transform
/ 事件 / 未知类型），Mapper **不创建任何对应 Domain 对象**，只按类产出一条 warning
（含计数）并把 `UnsupportedCounts` 原样带出。

有断言：放 2 个 mesh + 1 个 region 时，`SkinEntry` 只有 1 个、`SkinEntryId`
只发了 1 个，但 `unsupported.mesh == 2` 且有 warning。

**已知缺口**：诊断的位置只有**段名**（`skins` / `animations` / `ik` …），不是字节偏移 ——
B4 的 `UnsupportedCounts` 只记了数量，没有记录首个出现位置。此处如实标注为段名，
不伪造精确位置。要补需要在 B4 的计数里加首个位置，属后续返修项。

### 3.4 error 与 warning 的分界

- **error**：版本不符、父索引前向引用/越界、插槽骨骼越界、皮肤 slot 越界、非有限浮点、
  region 未命中 → 拒绝整个候选，且 `result.skeleton` **不落地**（无半成品）。
- **warning**：不支持的能力、尾随空格降级 → 候选可提交，但需宿主显式确认（B6 的事）。

---

## 4. 测试（32 项）

`ctest -R spine32_mapper_tests`。全部为**内存合成输入** —— 语料 1113 个单位全是
单页图集，多页 / `rotate` / trim / 页属性 / 序列 index 只能靠合成文本给反例。

| 组 | 覆盖 |
|---|---|
| Atlas Reader 17 项 | 页声明 size 原样保留；`orig`/`offset`/`rotate:true→90°`/`index`；多页归属；页属性解析进 IR **且**给 warning；非方形区域；缺 `xy`/`size`、区域先于页、空页、页缺 `size`、属性未缩进 → error；Unity TextAsset 包装被剥离；码名稳定 |
| Mapper 15 项 | ID 全由工厂发放；父子链与 `boneOrder`；前向引用/自环、插槽骨骼越界、皮肤 slot 越界 → error；`path ?? name` 三级回退；尾随空格降级=warning / 关闭=error；未命中=error 且消息带键；非 region 只计数不建对象；不支持能力只 warning；颜色 `0xRRGGBBAA`→0..1；blend 枚举；`drawOrder` 与 `slots` 互为排列；**映射结果过 Domain `Project::adoptData()`** |

最后一项是闭环证据：Mapper 产出的 `Skeleton` 是真的被 Domain 接受的，不是"自己说自己对"。

---

## 5. 语料只读扫描（1113 个单位）

`tools/spine32-mapper-smoke/`（`-DLOONGBONES_BUILD_SPINE32_MAPPER_SMOKE=ON`）。
只读：不写文件、不复制语料、不解码像素。退出码沿用 B4 那套守门规则（只有 0 可验收）。

```
单位总数           1113
Reader 失败        0
图集缺失           2
图集解析失败       0
进入映射           1111
映射失败（error）  0
region 附件总数    27457
非 region 跳过     20252（只计数+诊断，不进 canonical）
尾随空格降级       0
region 未命中      0

诊断码：
  UNSUPPORTED_CAPABILITY  6130

结论：全部单位映射通过，region 未命中 0。
```

### 5.1 与 field-mapping §10.1 的对账

`field-mapping.md` §10.1 记的查找总数是 **47,709**，其中 4 例未命中，根因是附件名
尾随空格。本工具的实测：

- **27,457（region） + 20,252（非 region 跳过） = 47,709** ✓ 总数精确吻合；
- region 侧 **0 未命中、0 降级**；
- 缺图集的 2 个单位（`h0164s5_watch_hour` / `h0164s5_watch_minute`，后者即 manifest
  样本 #12）使 region 数比 manifest 的 27,459 少 2 ✓。

**结论：那 4 例未命中落在非 region 附件上** —— 而这类附件本阶段根本不进 canonical
（只计数+诊断），因此不出现在 region 查找里。region 附件的命中率是 100%。

这条对账的意义：T-B3（尾随空格降级）在**本阶段可映射的范围内**实际触发数为 0，
降级路径仍然按契约实现并有断言，但没有真实语料能证明它必要。它仍是 T-B3 待验证项，
不应因为"实现好了"就宣称已验证。

### 5.2 缺图集的 2 个单位

`h0164s5/watch/h0164s5_watch_hour` 与 `h0164s5_watch_minute` 语料里没有 `.atlas.txt`
（manifest 已记 #12 无图集）。本工具把它们单列为「图集缺失」而非失败 —— 缺资源是
导入期的事（B6 的资源缺失诊断），不是映射逻辑的错误。

---

## 6. 边界（B5 做了什么、没做什么）

**做到的**：

- `.atlas` 文本完整解析，多页 / rotate / trim / 页属性全部覆盖；
- Source → canonical 的骨骼 / 插槽 / 皮肤 / region 附件映射，ID 全部由宿主发放；
- region 查找 `path ?? name` + 尾随空格降级 + 未命中 error；
- 不支持的能力**只计数 + 诊断，不建半成品**；
- 全语料 1113 个单位 region 未命中 0，且映射结果过 Domain 校验。

**没做的**：

- **没有任何动画、mesh、蒙皮、IK 进入 canonical** —— 20,252 个非 region 附件被
  计数后跳过。B5 不改变 B4 的交付边界，"能映射 region"不等于"能渲染网格"。
- UTF-8 校验仍未做（`kInvalidUtf8` 预留）。
- 不读 PNG、不校验 region 是否越出页声明尺寸（那是 Domain 的
  `SKELETON_REGION_OUT_OF_PAGE`，B6 导入期才具备上下文）。
- 页属性 `format` / `filter` / `repeat` / `pma` 解析保留但**不落 canonical**。

---

## 7. 本轮事故（记录在案）

整理 `atlas_import.cpp` 时用 PowerShell 的 `Get-Content`（默认 ANSI 解码）+ UTF-8 写出
做了一次整文件往返，**把该文件里的中文诊断字符串全部二次编码损坏**。
GBK 解码 UTF-8 字节时对无效序列降级成 `?`，**不可逆**。

处置：逐行重建了 33 行中文字符串（依据保留下来的 ASCII 锚点与语义）。
B1 的测试断言**不依赖消息文本**，故功能与行为不受影响；但文案是重建的，不是原文。

教训：这个仓库的源码是 UTF-8 无 BOM，任何整文件读写都必须显式指定编码，
且不得用 `Get-Content` / `Set-Content` 的默认编码往返。
