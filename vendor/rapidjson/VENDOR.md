# RapidJSON（vendored）

| 项 | 值 |
|---|---|
| 项目 | `Tencent/rapidjson` |
| 版本 | release **v1.1.0** |
| commit | **`f54b0e47a08782a6131cc3d60f94d038fa6e0a51`** |
| 发布日期 | 2016-08-25 |
| 取源 URL | `https://codeload.github.com/Tencent/rapidjson/zip/refs/tags/v1.1.0` |
| vendored 日期 | 2026-10-20（阶段 B，RFC §7.4） |
| 取源包大小 | 1,145,513 字节（magic `50 4B 03 04`，合法 ZIP） |
| 取出内容 | `include/rapidjson/`（**33** 个头文件）+ `license.txt` → `LICENSE-MIT.txt` |
| 版本自证 | `include/rapidjson/rapidjson.h` 中 `RAPIDJSON_MAJOR/MINOR/PATCH_VERSION` = **1 / 1 / 0** |

## 许可证：MIT（vendored 树内 100% MIT）

上游 `license.txt` 声明 RapidJSON 源码为 **MIT**，但列有两个第三方组件。本 vendoring 的处理：

| 组件 | 上游许可证 | 本 vendoring 处理 | 理由 |
|---|---|---|---|
| `bin/jsonchecker/` | **JSON License**（含"shall be used for Good, not Evil"条款） | **未取出** | 我们只取 `include/rapidjson/`，该目录不在其中 |
| `include/rapidjson/msinttypes/` | **BSD-3-Clause** | **已删除** | 见下 |

**删除 `msinttypes/` 的依据**（可验证，非推测）：`rapidjson.h:162` 的包含条件是
`#if defined(_MSC_VER) && (_MSC_VER < 1800)`（即 VS2013 之前）。本项目要求 **C++17**，
VS2013 不支持 C++17，故该分支**不可达**；MinGW/GCC 与其他编译器走 `#else` 分支用
`<stdint.h>` / `<inttypes.h>`。删除后 vendored 树内**不含任何非 MIT 代码**。

## 使用约束（RFC §7.4）

1. **固定 commit** `f54b0e47…`，**不得**跟随上游 `master`。升级须重新评审并更新本文件与
   `docs/licenses-and-risks.md` §1.5。
2. 只出现在 `format/` 与 `tools/format-tests/` 的 **`.cpp` 实现文件**中；
   **不得**出现在任何公开头文件，**不得**进入 `domain/`。
3. 用其 **SAX 接口**自建 JSON 值树；**不直接使用其 DOM**（RFC §7.3）。
4. 解析时使用 `kParseNumbersAsStringsFlag`，在 SAX 的 `RawNumber` 回调中取原始字面量，
   以满足 RFC §6.6「未知数值只保留 raw、不做类型化转换」。

## 升级流程

1. 用 `refs/tags/<tag>`（不是 `master`）重新取源；
2. 只取 `include/rapidjson/` 与 `license.txt`，删除 `msinttypes/`；
3. 更新本文件的版本/commit/URL/文件数，与 `docs/licenses-and-risks.md` §1.5 的登记保持一致。
