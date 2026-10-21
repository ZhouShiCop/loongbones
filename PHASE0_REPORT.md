# Phase 0 报告：仓库接管、数据盘点、许可与基线

- 阶段：Phase 0（规范 `项目规范.md` §5 / §12）
- 日期：2026-10-19
- 状态：**已完成，待负责人审阅**。未写任何编辑器功能代码。
- 产出目录：`Loongbones/`

---

## 1. 目标仓库现状

| 项 | 事实 |
|---|---|
| 项目根 | `D:/Works/Spine/Loongbones/`（负责人在此放置规范，确认为目标根） |
| 内容 | **仅** `项目规范.md`（38,939 B） |
| `AGENTS.md` | **不存在** |
| 构建/发布方式 | **无**（无 package.json / CMakeLists / *.pro / 任何构建入口） |
| 既有代码 | **无** |
| Git | **未启用**（按规范 §9，未擅自 `git init`） |
| 大小写 | 实际目录 `Loongbones/`，规范正文写 `loongbones/` —— **需负责人定夺**（规范 §10.1 要求命名稳定可预测；Windows 大小写不敏感，跨平台会分叉） |

**按规范 §8.1/§12.1，未初始化仓库、未假定技术栈、未创建源码目录。** 仅提交 bootstrap 建议：

1. 先签定 ADR-0001（语言/Qt 绑定/构建/渲染后端），再按规范 §8.2 的建议分区创建
   `app/ domain/ runtime/ format/ importers/ render/ ui/ tests/ docs/ tools/`。
   在签定前预建空目录会造成"目录在但无归属"的假进度。
2. 建议首批落地：`.editorconfig`（charset=UTF-8、`end_of_line`、缩进、末尾换行、尾随空格）、
   `CONTRIBUTING.md`（集中开发约定）、`.gitignore`（**精确规则**，禁止忽略整类 `*.json`/`*.png`/`*.bin`，
   规范 §9.1）、`AGENTS.md`（把规范 §8 的工作约定落到 agent 可读入口）。
3. Git 初始化由负责人确认仓库根、远端、默认分支、大文件策略后再做（规范 §9）。

---

## 2. 语料只读清单（交付物 2）

工具：`tools/corpus-scan/scan.mjs`（用法见 `tools/corpus-scan/README.md`，已登记进 `STRUCTURE.md`）。
扫描范围：`$LOONGBONES_CORPUS_ROOT` 下全部 `*.skel.bytes`，**只读**。

### 2.1 总量（1113 全量口径）

| 指标 | 值 |
|---|---|
| `*.skel.bytes` 文件 | **1,113** |
| 解析成功 / 失败（strictEof） | **1,113 / 0**（未发现截断/损坏） |
| 版本头 | 全部 **`3.2.01`**（单一版本，无分叉） |
| 唯一 SHA-256 | 1,107（6 个完全重复，分 6 组） |
| 顶层目录数 | 968 |
| 伴随 `.atlas.txt` | 1,111 / 1,113 |
| 伴随 `.png` | 1,110 / 1,113 |
| 三件套齐全 | 1,110 / 1,113 |
| 骨骼 / 插槽 / 皮肤 | 72,256 / 42,955 / 1,113 |
| 动画 / 时间线 | 16,046 / 4,954,836 |
| IK / transform / path 约束 | 2,707 / 208 / **0** |
| region / mesh / 加权 mesh | 27,459 / 20,252 / 17,271 |
| deform / ik / event 时间线 | 335,817 / 38,680 / **0** |
| 事件（`ir.events`） | **1** |

### 2.2 **963 vs 1113：已解释并逐项复现（交付物 2 核心）**

**结论：两个数字数的是不同文件集，差异 150 个文件全部是子目录变体。**

- **1,113** = 递归遍历语料根得到的所有 `*.skel.bytes`。
- **963** = 旧 manifest 的枚举规则（`converter/src/corpus.ts:57-66` `findSkelBytesFiles`）：
  **非递归**，只取每个顶层目录内、**文件名与目录名完全相同**的 `<dir>/<dir>.skel.bytes`。
- 968 个顶层目录中 963 个命中该规则；剩余 **150** 个文件位于变体子目录或异名，例如
  `b0001s5/a2/b0001s5_a2.skel.bytes`、`h0005s5/*`（同目录 10 个）、
  `h0261s5/s3/h3004c3.skel.bytes`（目录名与文件名不同）。
- 6 个文件是完全重复（同 SHA-256），与 150 的差额无关。

**证据（可复现）**：`scan.mjs` 对 primary 口径单独聚合，与旧 manifest 逐项比对——

| 指标 | 旧 manifest | primary 口径重扫 | Δ |
|---|---|---|---|
| files | 963 | 963 | **0** |
| bones | 65,171 | 65,171 | **0** |
| slots | 38,821 | 38,821 | **0** |
| skins | 963 | 963 | **0** |
| animations | 15,067 | 15,067 | **0** |
| ik | 2,481 | 2,481 | **0** |
| transform | 160 | 160 | **0** |
| timelines | 4,676,323 | 4,676,323 | **0** |
| meshes | 18,325 | 18,325 | **0** |
| weighted | 15,751 | 15,751 | **0** |

**10/10 项 Δ=0** ⇒ 963 口径完全复现，差异解释闭环。

**建议**：后续回归分母统一写明口径。能力指纹对齐用 **primary 963**（与历史可比）；
覆盖形态用**全量 1,113**（含变体）。两者都记录，不再混用。

### 2.3 文件编码与容器检查（交付物 2/4）

- 二进制 header 布局：`hash: string` + `version: string` + `width/height: float` +
  `nonessential: boolean` + `[imagesPath: string]`。
  ⚠️ **开头是 hash，第二个字符串才是 version**；按"前 16 字节读版本号"会全部得到 null（本次首版扫描即踩到，已修）。
- 图集命名：3.x 用 `<base>.atlas.txt`（1,111 个）；`.atlas` 旧命名 **0** 个。
- png 与 atlas 通过 atlas 正文的页文件名关联。
- 截断/损坏：strictEof **0 失败**。
- **`nonessential` 段覆盖率与语义未统计**，列入 Phase 1 不确定项 U3。

---

## 3. 首批样本选择（交付物 3）

工具：`tools/corpus-scan/select-samples.mjs`。产出 `tools/corpus-scan/sample-selection.md` 与
`out/samples.json`。**16 个样本，11 项能力标签全覆盖。**

构成：primary 单位 5 个 + 变体 11 个（变体覆盖了 primary 里没有的形态，如 `_meat`/`_cocoon`/`_doll2`/
`fx_*_s0X` 等特效骨架）。

覆盖的能力（全部命中）：`simple-skeleton`、`deep-hierarchy`、`ik`、`transform-constraint`、
`mesh`、`weighted-mesh`、`deform`、`event`、`many-animations`、`large-mesh`、`complete-trio`。

代表性样本：

| 样本 | 理由 |
|---|---|
| `h3181c2/h3181c2.skel.bytes` | **全语料唯一含事件的样本**（必收） |
| `b0001s3/b0001s3.skel.bytes` | 历史调查核心样本（29 骨 / 16 mesh / 11 旋转 region / 图集缩放 1.667） |
| `h0133s5/h0133s5.skel.bytes` | 最大最复杂（105 骨 / 43 槽 / 54 动画 / 974 KB） |
| `b3310c3/b3310c3_cocoon/…` | 最小骨架（1 骨 / 1 槽） |
| `h0215s5/h0215s5_doll2/…` | IK 约束 |
| `h0166s5/h0166s5_buoy/…` | transform 约束 |
| `h0164s5/watch/…`、`e0039c1/fx_e0039c1_s02/…` | 体积最小（加载/性能边界） |

### 3.1 语料能力边界（直接影响 Phase 1 能力矩阵）

| 能力 | 语料实证 | 对首版的影响 |
|---|---|---|
| bone / slot / region | 65,171 / 38,821 / 24,605 | 可完整验证 |
| mesh（非加权 + 加权） | 18,325 / 15,751 | 可完整验证 |
| deform（FFD） | 316,722 条时间线 | 可完整验证 |
| IK | 2,481 + 36,408 | 可完整验证 |
| transform 约束 | 160 | 可验证但样本少 |
| drawOrder | 14,136 | 可验证 |
| **event** | **1 个 event、0 条时间线** | **几乎无法用真实样本验证** |
| **path 约束** | **0** | **无法验证** |
| **boundingbox / clipping / point / path 附件** | **0** | **无法验证**（语料只有 region + mesh 两类附件） |
| **sequence 附件** | `index` 全 -1 | **无法验证** |

⇒ 规范 §2.3 要求首版覆盖的"事件"在本语料几乎无样本；能力矩阵中标 `待验证` 的项
需要**合法可再分发的合成 fixture**（谁提供见许可清单 §3 问题 4）。

---

## 4. Spine 3.2.01 → Native Model 初步映射（交付物 4）

完整文档：`docs/mapping-spine32-to-native.md`。要点：

- **容器**：一个单位 = `.skel.bytes`（骨架）+ `.atlas.txt`（图集）+ `.png`（页图像）。
- **UV 语义（本次调查的核心结论）**：Spine 文件里 mesh 的 `uvs` 就是 **regionUVs（region 内相对 [0,1]）**，
  整页归一化 UV 是 runtime `UpdateUVs()` 加载时算的。**任何"把 UV 当整页坐标再逆变换"的处理都会把 UV 拉出 [0,1]。**
- **非加权 mesh 顶点**是**槽骨骼局部坐标**；加权 mesh 用 `[boneCount,(boneIdx,x,y,w)×n]` 交错式，
  `boneIdx` 是**骨架全局**下标。
- **骨骼动画帧值是相对 setup 的增量**，不是绝对值。
- **deform**：非加权按增量、加权按绝对值且按 `I.length/3*2` 展开。
- **`rotate`** 在语料里只有 `true`/`false`（等价 90°/0°），无度数写法。
- **图集缩放**：部分单位（如 b0001s3）原始尺寸 ÷ 图集尺寸 ≈ 1.667，图集被打包时统一缩小；
  缩放承载层（图集层 vs 附件层）是 ADR-0003 待决项。

**12 项不确定项**（U1–U12）已在映射文档 §4 列表，其中必须在写 Domain 类型前关闭的是：
坐标系方向、时基与舍入规则、UV 的 V 方向、矩阵布局。

---

## 5. 许可与资产风险（交付物 5）

完整文档：`docs/licenses-and-risks.md`。**最关键的一条**：

> `resources/extern/vendor/spine-r35/LICENSE` = **Spine Runtimes Software License v2.5**
> （Esoteric Software）。授权范围 "solely for personal or internal use"；
> 未经书面许可**不得** "(a) modify, **translate**, adapt, or develop new applications
> using the Spine Runtimes or otherwise create derivative works"。

**因此：把 Spine runtime 源码移植/翻译成产品代码不被许可允许。**
Phase 0 之前曾提出"对照移植 spine-r35 到 C++"，**该建议已撤回**。
可接受的做法是：以公开可观察行为 + 自行解析数据格式为依据**独立实现**，
用 Spine 源码仅作内部行为参照，并保留自研实现的 golden 测试证据链。

其他风险：`spine38-web` 打包产物无许可证（仅限本地参照，不可分发）；
Sdorica 游戏资产专有（只读，不入仓库）；DragonBonesJS 为 MIT（新项目已放弃该目标）。

---

## 6. Schema 与 Qt/渲染后端待决项（交付物 5）

ADR/RFC 草案已就位（`docs/adr/`，**均为草案，待负责人签定**）：

| 编号 | 标题 | 阻塞 |
|---|---|---|
| 0001 | 技术栈：语言 / Qt 绑定 / 构建 / 渲染后端 / 目标平台 | Phase 1 垂直切片 |
| 0002 | 原生格式容器（JSON / 二进制 / 压缩 / 增量） | Schema RFC |
| 0003 | 坐标 / 角度 / 时基 / 曲线 / UV 原点契约 | **所有 Domain 字段** |
| 0004 | 稳定 ID 策略与索引迁移 | 对象引用、权重绑定 |

**0003 建议最先签定**：旧项目缺陷史显示，"渲染散开 / UV 翻转"类问题根因几乎都是语义歧义
（坐标系、顶点空间、deform 域、帧值域、rotate 表示、UV 原点），
不写成 schema 级契约就会每个消费端各自解释一遍（规范 §3.2 已明确要求）。

规范 §3.3 已定的容器形状（格式头 + manifest + 规范化数据文档 + 资源引用 + 可选扩展包）
与必需元数据（`formatId`/`formatVersion`/`minimumReaderVersion`/`projectId`/稳定 ID/
资源清单/单位与时基/扩展命名空间/可选校验和）不属待决项，直接作为 Schema RFC 的前置约束。

---

## 7. 验证入口（交付物 7）

| 工具 | 状态 | 用途 |
|---|---|---|
| `tools/corpus-scan/`（scan / analyze / select-samples） | **已建并登记** | Phase 0 数据盘点、口径复现、样本选择 |
| `tools/golden-contract/` | **建议 Phase 1 建** | C1–C9 契约的 golden 数值表，任何 Domain/Render 变更重跑 |
| `tools/bench-render/` | **建议 ADR-0001 签定后建** | 跨渲染后端同 skeleton 基准，输出耗时与像素指纹 |

所有工具已按规范 §8.2 登记进 `STRUCTURE.md`，并各有就近使用说明（`tools/corpus-scan/README.md`）。

**已执行的验证及结果**：

| 验证 | 命令 | 结果 |
|---|---|---|
| 全量语料扫描 | `node tools/corpus-scan/scan.mjs` | 1,113 文件 / 解析 1,113 / 失败 **0** |
| 口径复现 | 同上（内置 primary 聚合与 legacy 比对） | **10/10 项 Δ=0** |
| 样本覆盖核对 | `node tools/corpus-scan/select-samples.mjs` | 16 样本 / **11/11 能力标签命中** |

未运行 converter 旧测试套件——本阶段未改旧项目任何代码，无回归必要。

---

## 8. 已知缺口与下一阶段输入

1. **能力覆盖缺口**（事件 / path 约束 / boundingbox / clipping / point / path 附件 / sequence）
   ——语料无样本，需合法可再分发的合成 fixture；请负责人指定提供方。
2. **`nonessential` 段语义未统计**（U3）——影响"哪些字段可安全丢弃"的判断。
3. **名称唯一性未统计**（bone/slot/附件）——影响 ADR-0004 的 ID 派生键选取。
4. **`nonessential` 覆盖率、linked mesh 出现率、`deform offset` 形态分布**未统计。
5. **Qt 未安装完成**（负责人正在安装）——ADR-0001 的比较实验尚无法开始。
6. **目录大小写**（`Loongbones/` vs `loongbones/`）待定夺。

**给 Phase 1 的输入**：§4 映射文档、§6 ADR 草案、§7 验证入口、16 个样本集
（`tools/corpus-scan/out/samples.json`）、primary 963 的 golden 统计基线
（`tools/corpus-scan/out/corpus-manifest.json` 的 `primaryTotals` 段）。

---

## 9. 本阶段未做的事（边界声明）

- 未写任何编辑器/Domain/Runtime/Format/UI 产品代码。
- 未初始化 Git、未创建源码目录、未选定技术栈（规范 §8.1/§9）。
- 未修改、移动、删除任何语料文件（全程只读）。
- 未把任何游戏资产复制进 `Loongbones/`（manifest 仅含相对路径 + hash + 统计量）。
- 未修改旧项目 `D:/Works/Spine/` 下任何文件；仅**借用**其 Reader 作为分析工具。
- 未宣称任何视觉效果验收（规范 §7：视觉艺术判断留给用户）。
