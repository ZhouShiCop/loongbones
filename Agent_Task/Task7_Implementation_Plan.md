# Task 7 阶段 A 实施方案（提案）：时间线 / 曲线 / v4 / 顶点契约 / 渲染 ADR

- 状态：**已立项（负责人 2026-10-24「可启动 Task 7」）；阶段 A 可开工**
- 上游：[Task 7 范围草案](Task7.md)、`DECISIONS.md` **D42 / D43–D55**、`Type_Coverage_Status.md`
- 依赖：Task 6 阶段 A/B **已通过**（`Task6_A1A3_Review.md`、`Task6_B_Review.md`）
- 目标项目：`D:\Works\Spine\Loongbones`；语料：`D:\Game\Other\sdorica\working\full_export\assets\game\character\spinedata`（**只读**）
- **角色边界**：本文件只作**计划**；实现由实现会话执行，本对话验收
- 日期：2026-10-24
- 时间口径：只给「实现会话数 + 代码行量级 + 编译等待」，**不给日历日期**

---

## 1. 目标与边界

**阶段 A 的唯一目标**：把「对齐 Spine」所需的**基础契约**定清，让后续批次不再返工。
它**不实现**任何产品能力（`format/` `domain/` `importers/` 零改动）。

**为什么必须先做阶段 A**：D47 明确「**时间平滑程度等基础问题未定清前不得铺开**」，
而时间线在语料里有 **`timelines` 4,988,424 / `deformTimelines` 335,817 / `animations` 16,007** 条 ——
一旦按错误口径铺开，返工面比 Task 5 大一个数量级。

**边界**：

- ✅ 可以说：语料规模**已普查**、时间语义**已成文**、v4 schema **已提案**、渲染**已选型**
- ❌ 不得说：可播放、可渲染、可求值（求值与渲染实现仍延后，D43 / D45）
- ❌ 阶段 A **不得**改产品代码、不得放宽任何既有断言
- 合成样本（D13）只能证明「结构可解析 / 可拒绝」，**不得**写成「语料已验证」

---

## 2. 开工前置（checklist）

| # | 前置 | 说明 |
|---|---|---|
| **R1** | Task 6 收尾 3 项非阻断返修 | ① `SharedAtlasResolvesBothSkeletons` 改名或补真断言；② `listDirectory` 去兼容层（纯虚，3 个 Fake 各补实现）；③ `Task6_B_Report.md` 状态行与 §5 对齐 |
| **R2** | **提交当前叠加栈** | Task 5 B1–B5 + Task 6 A/B 全部未提交（停在 `049b1d3` 后，约 40 文件）。**强烈建议先提交**，否则 Task 7 一旦出问题无法二分定位 |
| **R3** | 干净构建目录 | `D:\Works\Spine\.builds\build_t7`（**不得**在仓库内建；`build_t6/` 残留已于 2026-10-24 清理） |
| **R4** | 工具链环境坑 | **`CXX` 环境变量优先于 `-DCMAKE_CXX_COMPILER`** ⇒ 必须显式设 `CXX` 指向 `g++.exe`（`c++.exe` 已不存在）；运行测试 exe 须带 mingw64/bin 的 PATH |
| **R5** | 语料只读 | 探针只产出统计与相对路径；**不复制语料**、不写机器绝对路径、不提交游戏资产 |

---

## 3. 阶段 A 工作包（**不实现**）

> 依赖顺序：**A1 必须先行**（A2/A3/A5 的数字来自 A1）。A4 / A6 可与 A1 并行起草，但**定稿须引用 A1 实测**。

### A1 时间线普查（只读语料，产出报告 + 探针）

1. **按类型普查**：`rotate / translate / scale / shear / attachment / color / deform / ik / transform / event / drawOrder` 各多少条，
   **Full 1113 / Primary 963 双口径分别报告**（D12，不混分母）。
2. **时间轴分布**：每类的帧数（min / median / max）、跨度（首帧→末帧）、**帧间隔是否恒为 1 tick**；
   给出「采样率是否就是 60」的实测 —— 这是 D47 的直接输入。
3. **曲线分布**：linear / stepped / bezier 各多少；bezier 的 **curve 数组长度分布**（4 / 8 / 12 … = 手柄段数）；
   是否存在**多手柄控制函数**（D44 要求支持）的真实样本，各多少。
4. **deform 依赖**：deform 时间线的 vertexCount 分布；与附件顶点数是否**逐一吻合**（A5 契约的证据）；
   是否出现**顶点数不一致**的畸形（决定拒绝判据）。
5. **事件 / drawOrder**：事件计数（语料 1 例）与形态；drawOrder 是否存在。
6. 产出：`docs/skeleton/timeline-census.md`（报告）+ `tools/skeleton-probe/timeline-probe.mjs`（**入库并登记** README / `STRUCTURE.md`）。

> **最大技术风险**：产品 Reader **尚未解析 animations**（Task 5 明确未做）⇒ 探针必须**独立按 3.2.01 布局走字节**，
> 并带全套护栏（`kMaxArrayCount` / `kMaxStringBytes` / 顶点数上限），否则字节错位会读到巨大长度并空转到超时。
> 探针须自带**交叉核对**：对已知字段（骨骼/槽/皮肤数）与既有 `probe.mjs` 结果对齐，证明走位正确。

### A2 时间语义契约（**D47 的核心**）

必须**写死**并给出可复现对照程序：

1. **时基**：D39 的 60 FPS 如何落到存储与求值（整数 tick；`t = frame / 60` 用有理数，**禁止写死 30**）。
2. **采样与插值**：stepped / linear / bezier 三档的**取值语义**；帧间（`t` 落在两帧之间）如何取；
   末帧之后 / 首帧之前（外推还是钳制）。
3. **「时间平滑程度」定清**（负责人点名要回答）：平滑由 **curve 数组**决定还是全局设置；
   若无 curve ⇒ 默认 linear 还是 stepped；`curve` 存在但退化（`cx1==cy1 && cx2==cy2`）如何处理。
4. **边界**：重复帧时间、时间倒序、零长动画、`t` 越界 —— 各自的**拒绝 / 钳制**判据。
5. 产出：`docs/skeleton/time-semantics-rfc.md` + `tools/skeleton-probe/time-eval-check.mjs`
   （对若干合成输入算出关键点取值，结果可复算 ⇒ 供实现阶段对拍）。

### A3 曲线 / 手柄语义（**D44**）

1. **编码**：curve 数组布局（每段 4 个 float：`cx1, cy1, cx2, cy2`，多段串联）；
   一条时间线内**多条曲线**（多手柄控制函数）如何对应到帧区间。
2. **求值**：三次贝塞尔求解（给定 x 求 t 再求 y）的**算法与数值边界**（迭代次数 / 容差 / 单调性假设）；
   与「线性插值」在退化曲线下的等价性。
3. **退化与畸形**：段数过多、`cx/cy` 越界（是否允许 >1 / <0）、曲线长度与帧数不匹配 —— 各自处置。
4. 产出：契约节（并入 A2 的 RFC）+ 合成 fixture 计划（A7 落地）。

### A4 v4 schema 提案（**D46** + **D55** + **D49**）

1. **承载内容**：`animations` / 各类型 timelines / `deformTimelines` / `events` / `drawOrder`；
   **`BoundingBox`（D49，非权重口径）**。
2. **源身份字段（D55 的前置）**：`Atlas` / `TextureAsset` 增加「**源路径**」（授权根相对），
   用于跨单位复用同一份资产 ⇒ 同时解决 D53 的落地。
3. **迁移**：`v3 → v4` 为**纯函数**（不生成 ID、不改既有字段、不碰磁盘、幂等）；v1/v2/v3 Reader 对 v4 **固定拒绝**。
4. **解码侧已知键同步清单**（Task 5 重复键陷阱）：新增键必须同步 `decode.cpp` 的已键集合，
   并按 `type` 分派；验收须做 **≥2 次保存的多轮往返**（`decode→encode→decode→encode→decode`）断言无重复键。
5. 产出：`docs/format/native-project-v4-rfc.md` + 迁移函数签名 + 拒绝矩阵。

### A5 顶点 ID / 索引契约（**D42**）

1. 顶点顺序 = **源序**（不重排、不去重）；deform 的顶点索引基于该序。
2. 零点参考 = **(0,0,0)**（D42）；与 Task 5 已落的 `uvs / vertices / triangles` 的关系。
3. 语料一致性：deform 的 vertexCount ↔ 附件顶点数（A1 §4 的数字）逐类核对；
   不一致时是「拒绝」还是「降级诊断」—— 给出判据。
4. 产出：契约节 + 与 `mesh-and-constraint-rfc.md` 的对齐说明（避免两处口径冲突）。

### A6 渲染选型 ADR（**D43 / D50 / D51 / D52**）

1. **方案**：QML 承载界面 + `QQuickFramebufferObject`（Qt 5/6 通用）接入**自研 OpenGL core**（Win/Linux 3.3、macOS 4.1）；
   后期 bgfx 变体（D51）预留 **Render Adapter 多后端 + 构建期选择**。
2. **必须书面的契约**（否则渲染阶段必返工）：
   - 坐标系（Y 轴方向、原点）与角度单位/旋转方向；
   - 颜色空间与 **预乘 alpha**；
   - **混合模式映射表**（normal / additive / multiply / screen）—— 本项目 Reader 对未知混合模式是**拒绝**的；
   - 纹理过滤与 wrap 模式。
3. **性能基线**：AMD 核显 60 fps @ 1080p；给出语料的**最大骨骼数 / 顶点数**（来自 A1/A5）作为压测目标。
4. **可撤销比较实验**（可选，规范要求"做实验后签 ADR"）：`QOpenGLWidget` vs `QQuickFramebufferObject` 的最小 spike 各一，
   比较混入 QML 的复杂度与 Qt 版本兼容性。
5. 产出：`docs/gui/render-backend-adr.md`（**由负责人签字后才算定案**）。

### A7 合成 fixture 与阶段 A 交付

- 生成器扩展：**动画 / 曲线（含多手柄）/ deform / 事件 / bbox** 的最小合成 fixture（沿用 `make-fixture.mjs` + golden 表）。
- 交付：A1 报告 + A2/A3 契约 + A4 RFC + A5 契约 + A6 ADR + fixture/golden + 风险与待决项。

---

## 4. 阶段 A 验收判据

| # | 判据 |
|---|---|
| 1 | **A1 普查完整**（1113 / 963 双口径），按类型计数可复核；探针入库并登记 README / `STRUCTURE.md`；out JSON 可重跑复现 |
| 2 | A1 给出**帧间隔与曲线类型**的实测分布（D47 的直接输入），并点名**多手柄曲线**的真实样本数 |
| 3 | **A2 把「时间平滑程度」定清**，含与 D39 整数 tick 的衔接；对照程序可复算同一输入的取值 |
| 4 | A3 曲线编码/求值/退化边界写死，畸形样本有明确拒绝判据 |
| 5 | A4 v4 提案含 **源身份字段（D55）**、迁移纯函数、旧 Reader 拒绝矩阵、**解码侧已知键同步清单** |
| 6 | A5 顶点 ID 契约与 deform 依赖一致，且与 `mesh-and-constraint-rfc.md` 无口径冲突 |
| 7 | A6 ADR 含混合模式映射表、坐标/角度契约、性能基线，且**不绑定 Qt 版本**、不引入一期第三方渲染依赖 |
| 8 | **阶段 A 期间 `format/` `domain/` `importers/` 与 `tests/` 零改动**（查 mtime / git） |
| 9 | 全部结论按 D13 措辞：合成样本不得表述为「语料已验证」 |

---

## 5. 后续批次（阶段 B 起，**本轮不授权**）

| 批次 | 内容 | 量级（按语料） |
|---|---|---|
| **T7-B** | 非 deform 时间线建模 + **格式升 v4** + 导入闭环 + 源身份字段（D53/D55） | `timelines` 约 4.65M（扣 deform） |
| **T7-C** | Deform 时间线（依赖 A5 顶点契约） | `deformTimelines` **335,817** |
| **T7-D** | 事件 + 皮肤相关 + `BoundingBox`（D49，非权重） | `events` 1 / bbox 0 |
| **T7-E** | 非权重绑定求值（骨骼层级 / 子物体矩阵，D45） | 待 A1 取证 |
| **T7-F** | 渲染实现（D43 延后项）+ QML 界面 | 不在本轮 |

> 批次拆分与命名在**阶段 A 验收后**由 `Task7_Implementation_Plan.md` 第 2 版定稿。

---

## 6. 数字纪律

1. **Full 1113 / Primary 963 双口径不混用**，报告必须标分母。
2. 语料计数必须由**可重跑探针**产出，不接受手抄；探针须带护栏与交叉核对。
3. **退出码只有 0 可作验收证据**；既有 7 或非 0 一律单列、不得包装。
4. 时间估计只给「会话数 + 行量级 + 编译等待」，**不给日历日期**。
5. 「可解析」不得表述为「可播放 / 可求值 / 可渲染」。

---

## 7. 已知坑（继承 Task 4 / 5 / 6）

| 坑 | 规避 |
|---|---|
| `Status::ok()` 是静态工厂 ⇒ `!status.ok()` 恒 false | 判成功一律 `isOk()`；修死代码类缺陷必须同步补断言 + 反向探针 |
| codec 未知字段字面量回写 ⇒ 新增键漏同步会**重复键**、第二次保存打不开 | 新增键必须同步解码侧已知键；验收做 **≥2 次保存**多轮往返 |
| 二进制**多字节数值是大端**；`count()` 无护栏会空转到超时 | 探针一律 `readInt32BE/readFloatBE` + `kMaxArrayCount` 等护栏 |
| 探针分类**短路**会把"从未走到"说成"语料没有" | 区分**候选存在性**与**解析路径**两个口径（Task 6 教训） |
| 仓库内建构建目录 / `CXX` 环境变量覆盖 `-D` | 只用 `.builds/`；显式设 `CXX` |
| 验收工具把错误行为写死（如"1 单位 = 1 图集"） | 改语义时先看工具断言是否与新语义冲突 |
| 无 `-Wall` 的新代码在另一编译器下才报错 | 编译保持 `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`，零 warning |

---

## 8. 允许与禁止

**阶段 A 允许**：`docs/`（取证与提案）、`Agent_Task/Task7*.md`、`DECISIONS.md`、`STRUCTURE.md`、
`Type_Coverage_Status.md`、`tools/skeleton-probe/`（新探针 + README）、合成 fixture 与生成器。

**禁止**：改 `format/` `domain/` `importers/` `tests/`；放宽授权根 / EOF / 边界断言；
把"可导入"写成"可播放/可求值"；用合成样本宣称语料兼容（D13）；顺带格式化；复制或提交原始资产；
**D47 未定清前铺开实现**。

---

## 9. 授权与停点

| 项 | 状态 |
|---|---|
| Task 6 | **阶段 A / B 均通过（2026-10-24）**；D53 延后 **D55** |
| Task 7 | **已立项（2026-10-24）**；阶段 A **可开工** |
| 阶段 A 工作包 | A1 → A2/A3/A5 → A4/A6 → A7（A1 先行） |
| 阶段 B 及以后 | **未授权**（阶段 A 验收后逐个批准） |

**停点**：阶段 A 交付后**停下等待验收**；**一个工作包的批准不自动授权下一个**。
