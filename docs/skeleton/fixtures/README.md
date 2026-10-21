# 合成 fixture（Task 4 阶段 A）

**不含任何游戏资产。** 全部由 `tools/skeleton-probe/make-fixture.mjs` 按
Spine 3.2.01 二进制布局**独立写出**的字节（不调用旧项目写入器、不以任何游戏文件为模板），
图集为 libGDX `.atlas.txt`，PNG 为脚本生成的纯色图。可再分发。

生成/重建：

```bash
node tools/skeleton-probe/make-fixture.mjs --verify
```

`--verify` 会回读自校验（用旧项目 Reader 作对照解析，非产品读取器）。

---

## 1. 清单

| fixture | 文件 | 覆盖点 |
|---|---|---|
| **01-minimal** | `.skel.bytes` + `.atlas.txt` + `.png` | 最小骨架：1 根骨骼 / 1 个插槽 / 1 个 `default` 皮肤 / 1 个 region 附件 / 0 动画 / 0 约束 |
| **02-nested-slots-regions** | `.skel.bytes` + `.atlas.txt` + `.png` | 嵌套骨骼（root→hip→chest→head）、3 个插槽、3 个 region 附件；含 **rotation=90**、**附件名 ≠ region 名（靠 `path` 指过去）**、图集 **`rotate: true`** |
| **03-bad-parent-index** | `.skel.bytes` | 畸形：第 3 根骨骼的 `parentIndex = 9`（只有 3 根骨骼）→ **必须被拒绝** |
| **04-truncated** | `.skel.bytes` | 畸形：文件被砍掉最后 8 字节 → **EOF/截断必须失败** |
| **05-non-finite-float** | `.skel.bytes` | 畸形：第 2 根骨骼的 `rotation` 是 **NaN**（字节 `0x7F 0xC0 …`，位于偏移 83）→ **必须被拒绝**（`SKELETON_FLOAT_NON_FINITE`，见 `../skeleton-domain-rfc.md` §6.11） |

---

## 2. 预期结果（阶段 B 的断言依据）

| fixture | 期望 |
|---|---|
| 01-minimal | 解析成功；`bones=1 slots=1 skins=1 attachments=1 animations=0`；region `region-0` 解析到 `01-minimal.atlas.txt` 的同名 region；无 error 级诊断 |
| 02-nested-slots-regions | 解析成功；`bones=4 slots=3 skins=1 attachments=3`；`head-skin-a` 的 `path=head` → 解析到 region `head`；`weapon` 的 `rotationDeg=90`、`x=4`、`y=-2`；图集 region `head` 的 `rotation == "90"`（源 `rotate: true`） |
| 03-bad-parent-index | **拒绝**，诊断 `SKELETON_BONE_PARENT_OUT_OF_RANGE`，location 指向该骨骼；候选整体丢弃，工程不变 |
| 04-truncated | **拒绝**，截断/EOF 诊断（带字节偏移）；候选整体丢弃 |
| 05-non-finite-float | **拒绝**，诊断 `SKELETON_FLOAT_NON_FINITE`，`sourceLocation` 含段名（`bones`）与**字节偏移**；候选整体丢弃 |

**关于 03 / 05 的重要说明**：旧项目的宽松 Reader 能把这两个文件**读完**
（03 只对越界父索引记一条 warn；05 根本不校验浮点有限性）—— 这正是 fixture 要钉住的语义差别：
**产品读取器必须拒绝**，不能沿用"能读完就算成功"的宽松口径。

生成器 `--verify` 的当前实测输出：

```
01-minimal:               ok  bones=1 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
02-nested-slots-regions:  ok  bones=4 slots=3 skins=1 anims=0 version=3.2.01 warnings=0
03-bad-parent-index:      ok  bones=3 slots=1 skins=1 anims=0 version=3.2.01 warnings=1
                             warn: bones.b — 找不到骨骼索引为 9 的父骨骼
04-truncated:             FAIL — 读取 float 越界
05-non-finite-float:      ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
```

05 的 `ok` 是**旧宽松 Reader 的行为**（它不校验浮点有限性），**不是**期望行为；
产品读取器对 05 必须拒绝（见 §2 表格）。

## 3. 不含什么

- 不含任何游戏资产、不含源二进制片段、不含真实图集/纹理
- PNG 为脚本生成的**纯色图**（01 红、02 蓝），仅用于验证纹理链接与 IHDR 尺寸解析
- 不含绝对路径、盘符、用户名

## 4. 与真实语料的关系

fixture 只覆盖**结构层**与**拒绝语义**。以下能力语料里**没有真实样本**，
因此只能由 fixture 覆盖结构、语义仍需待验证（见 `../skeleton-domain-rfc.md` §11.2）：

| 能力 | 语料情况 | fixture 覆盖程度 |
|---|---|---|
| 多皮肤 | 0 个多皮肤单位（1113/1113 单皮肤） | 未覆盖（需后续补 fixture） |
| 多页图集 | 0 例多页 | 未覆盖（需后续补 fixture） |
| path 约束 / boundingbox / clipping | 0 例 | 未覆盖 |
| 图集 trim | 0 例（`size == orig` 100%） | 未覆盖 |
| 图集 `index`（sequence） | 0 例非 -1 | 未覆盖 |
