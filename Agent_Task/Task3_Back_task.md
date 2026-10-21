# TASK 3 BACK TASK：修复超范围 JSON 数值的安全上限与解析次数边界

- 来源：Task 3 阶段 B 静态验收复核
- 状态：`CLOSED`（返修完成并复核通过）
- 优先级：P1（可配置输入上限被绕过；合法文档可能被未登记的固定次数限制拒绝）
- 目标项目：`D:\Works\Spine\Loongbones`
- 关联任务：`Agent_Task/Task3.md`
- 关联契约：`docs/format/native-project-v1-rfc.md` §4.4、§6.6

## 1. 复核结论

Task 3 阶段 B 已实现 RapidJSON SAX JSON Reader、Format codec、未知字段保留、目录工程 IO 与自动化测试。静态复核发现 `format/src/json_reader.cpp` 的超范围数字占位恢复还有两处未覆盖边界：

1. `kParseErrorNumberTooBig` 路径先将完整数字字面量等长替换成 `0` 和空格，重新解析时 `RawNumber` 只收到 `0`。因此 `RawNumber` 中对 `maxStringBytes` 的数字字面量长度检查无法检查原始 token；超过配置上限的数字可绕过长度限制。
2. `parseJson()` 将占位重解析次数固定为 1024。包含超过 1024 个语法合法、但 RapidJSON 判为 `kParseErrorNumberTooBig` 的数值的文档会在解析循环耗尽后被拒绝，尽管 RFC 未规定此数量限制且文档可能低于 `maxDocBytes`。

## 2. 必须修复

### 2.1 占位前执行原始数值 token 长度限制

在将 `kParseErrorNumberTooBig` 的 token 替换为占位值之前，使用其**原始字面量长度**执行对应 Reader 限制检查；不得依赖重解析后的 `RawNumber` 回调长度。超过上限时返回稳定的 `kJsonSizeLimit` 诊断，并包含可定位的 JSON Pointer/字节偏移。

要求：

1. 检查对象必须是 `working` 中尚未替换的原始 token，长度为 `scanNumberLiteral()` 识别出的完整 token 长度。
2. 拒绝路径不得继续重解析，也不得提交部分 JsonValue 树。
3. 上限应与 RFC §4.4 及 `ReaderLimits` 的既有语义一致；若实现决定数值 token 独立于字符串长度上限，则必须先更新 RFC/ReaderLimits 契约和对应说明，不能让代码注释暗示有检查、而行为实际不检查。
4. 添加回归测试：通过自定义 `ReaderLimits` 设置很小的 `maxStringBytes`，输入一个长度超过该值且会触发 `kParseErrorNumberTooBig` 的数字 token（例如超长指数），断言返回 `kJsonSizeLimit`、有位置，且不成功解码。
5. 保留既有测试：未知字段 `1e400` 在默认限制下仍成功并原样编码；已知字段 `1e400` 仍由 schema 解码阶段返回 `kJsonNumberOutOfRange`。

### 2.2 移除未声明的固定 1024 次限制

合法文档不得因为超范围数字 token 的数量超过实现内部常量而失败。删除固定的 1024 次拒绝上限，或改为一个与 RFC 明确声明的 Reader 安全策略一致、可配置且有稳定诊断码的上限。不得把“占位数过多”错误伪装成 JSON 语法错误。

推荐优先采用不增加持久化格式契约的方案：使处理能力由输入大小上限自然约束，不设置未声明的数量上限。实现需避免对正常输入引入不必要的超线性扫描/复制；可调整算法或建立有界且可复用的 token/offset 索引。

至少新增回归测试：

1. 构造含 **1025 个**未知数值字段、其值均为合法但超出 RapidJSON 可解析范围的字面量（如 `1e400`）的文档；文档低于默认 `maxDocBytes` 时必须成功解码。
2. 编码后断言这些原始数字字面量均仍存在且未被类型化、规范化或改写。
3. 相同测试不得通过抬高 `maxDocBytes` 或绕过公开 Reader API 来规避问题。
4. 若最终选择配置化安全上限，测试须覆盖恰好达到上限与超过上限两个边界，并验证超过时返回专用、稳定、带位置的诊断；同时同步 RFC §4.4、公开 API 注释、README 与 `STRUCTURE.md`。

## 3. 回归与兼容要求

- 不改写 RFC §6.6 的核心语义：未知数值只保留 raw，不做类型化转换；已知数值仍按字段类型解析并对越界值拒绝。
- 未知的 20 位整数、`1e400`、高精度小数仍逐字节往返。
- 已知 `number` 的 `1e400` 仍返回 `kJsonNumberOutOfRange`；非法 JSON 数值仍返回语法诊断。
- 深度、文档大小、字符串大小、重复 key、严格 UTF-8、失败不产生部分结果等既有行为不得回归。
- 不修改 `domain/`、原生格式 schema 字段、既有稳定诊断字符串或 RapidJSON 版本/commit。

## 4. 修改范围

允许：

- `format/src/json_reader.cpp` 及必要的 `format/include/loongbones/format/` 实现接口；
- `tools/format-tests/src/main.cpp`、`tools/format-tests/README.md` 与必要的测试构建文件；
- 如引入/调整公开 Reader 限制：`docs/format/native-project-v1-rfc.md`、`STRUCTURE.md`、`Agent_Task/Task3_Back_task.md`。

禁止：

- 修改 Domain 模型或既有 Domain API；
- 变更原生格式 v1 的字段与兼容语义；
- 用放宽默认全局文档上限、移除回归测试或吞掉诊断来规避边界；
- 顺带重构无关 codec、IO、GUI、Spine 导入或渲染代码。

## 5. 验收与交付

1. 新增测试分别稳定复现并覆盖 §2.1、§2.2 两个缺陷；可在修复前证明失败、修复后通过。
2. 从新的干净构建目录 configure/build 并运行 `ctest -R format_tests`；报告总数、通过/失败/跳过、退出码和编译警告。
3. 运行核心 `core_tests`，确认 67 项不回归；运行阶段 A `format_probe_selftest`，确认 14 项不回归。若某入口因环境/超时未完成，必须单独说明，不得用旧二进制结果冒充本轮结果。
4. 更新 `tools/format-tests/README.md` 的测试数量、覆盖范围及任何新增限制；若改变工具入口或 Reader 契约，同步更新根 `STRUCTURE.md`。
5. 提交简短完成报告，列出根因、改动文件、测试实测结果、编译器/依赖版本、剩余限制。

修复完成后停止并等待复核；不得自行把本 Back Task 标记为验收通过。

---

## 6. 修复记录

### 6.1 §2.1 占位前执行原始数值 token 长度限制

根因：`kParseErrorNumberTooBig` 分支先把完整字面量等长替换为 `0` + 空格，重解析时
`RawNumber` 只收到 `0`，长度检查因此永远看不到原始 token。

修复：`format/src/json_reader.cpp` 的 `parseJson()` 在**占位替换之前**，用 `scanNumberLiteral()`
得到的原始 token 长度与 `limits.maxStringBytes` 比较；超限时直接返回 `kJsonSizeLimit`
（`FormatDiagnostic::withOffset`，location 取 `TreeBuilder::pendingPointer()` 的待写入成员
Pointer，offset 为 token 起始字节），不继续重解析、不提交 `JsonValue` 树。

契约：仍沿用 `ReaderLimits::maxStringBytes`（与既有 `RawNumber`/`String`/`Key` 检查同一语义），
不新增持久化字段；已在 RFC §4.4 与 `json_reader.hpp` 的 `ReaderLimits` 注释中显式声明
"字符串值 / 对象键名 / 数值字面量原文"共用该上限。

### 6.2 §2.2 移除未声明的固定 1024 次限制

根因：`parseJson()` 每遇到一个越界数值就把整份文档重新解析一遍，并以 `attempt < 1024`
硬性封顶，导致含 >1024 个越界数值的合法文档被拒绝。

修复：
1. 删除固定 1024 常量；循环上界改为 `working.size() + 2` —— 每轮至少把一个此前未占位的
   字面量占位，字面量互不相交且各占至少 1 字节，故轮数天然不可能超过文档字节数，
   处理能力完全由 §4.4 的 `maxDocBytes` 约束，**不引入任何新的持久化契约**。
2. 新增 `applyNumberPlaceholders()`：首次遇到越界数值时，对整份文档做**一次线性扫描**
   （跳过字符串内部，含转义）批量等长占位，之后只需再解析一次。避免 O(越界数量 × 文档字节数)
   的超线性行为。判据 `numberExceedsDoubleRange()` 是 RapidJSON 实际拒绝集合的**超集**；
   误判只是多一次等长占位，原文按字节偏移原样还原，无任何可见影响。
3. 占位表由 `(offset, 原文副本)` 改为 `(offset, length)`，原文直接从入参文本按同一偏移取回；
   `RawNumber` 用只进游标查表，还原为摊还 O(1)。
4. 防御性出口不再伪装成语法错误，改报 `kJsonSizeLimit`。

### 6.3 回归测试（新增 3 项，`number` 组，共 47 项）

| 用例 | 覆盖 |
|---|---|
| `oversized_number_token_length_limit_enforced` | §2.1：`maxStringBytes=16` + 37 字节 `0.000…01e400` → `kJsonSizeLimit`，偏移指向原始 token，Pointer 非空 |
| `many_out_of_range_number_tokens_are_accepted` | §2.2：1025 个未知 `1e400` 字段、默认 `ReaderLimits` → 解码成功，编码后 `": 1e400"` 恰好 1025 处，未出现 `Infinity` / `1.0e400` |
| `number_like_text_inside_strings_is_not_patched` | 批量占位只作用于字符串之外；字符串内的 `1e400` 原样保留 |

修复前实测：`build_bt3_pre`（临时回退版本）`tests 47 | pass 45 | fail 2`，两条新增断言分别为
`CHECK_EQ failed: result.formatDiag.code == kJsonSizeLimit` 与 `CHECK failed: result.ok`。
修复后：`tests 47 | pass 47 | fail 0`。

## 7. 最终复核结果

- 静态检查确认：`kParseErrorNumberTooBig` 首次命中时会先对原始 token 长度执行 `maxStringBytes` 检查；超限返回 `kJsonSizeLimit`、待写入成员 Pointer 与 token 起始偏移，且不继续解析。
- 静态检查确认：批量占位扫描跳过 JSON 字符串及转义序列；RapidJSON 两类 `kParseErrorNumberTooBig` 触发条件均被批量判据覆盖。占位表按 offset 升序，SAX 回调以只进游标从未修改的源文本恢复原始字面量。
- 未发现固定 1024 次拒绝上限；批量处理后最多对判据未覆盖项单点退化，循环次数由输入字节数约束。
- 本轮未重跑构建或测试。执行者报告：format 47/47、core 67/67、probe 14/14 均通过，退出码 0；严格编译标志下 0 警告；修复前的定向复现为 45/47，两个新回归均失败。
- RFC §4.4、ReaderLimits 注释、format-tests README 与根 `STRUCTURE.md` 已同步相关限制、覆盖项和 47 项计数。
- 结论：Task 3 Back Task 两项问题已由实现路径、定向回归与提交的构建证据闭合，**复核通过并关闭**。测试数字为执行者报告，本复核未独立执行。
