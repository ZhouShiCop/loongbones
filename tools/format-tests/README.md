# tools/format-tests — 原生项目格式 v1 codec 自动化测试

Task 3 阶段 B 的验证入口。被测对象是 `format/` 的**公开 API**，
本目录**不直接使用** RapidJSON（依赖隔离，RFC §7.4）。

## 与阶段 A 探针的分工

| | 阶段 A 探针 `tools/format-probe/` | 本目录 |
|---|---|---|
| 能力 | **只读**校验 | 读 + 写 + 目录工程 IO |
| 覆盖 | 14 个 fixture 的可判定性 | §6.5.1 宿主锚点、§6.6 数值精度、§4.5 确定性、§4.1 原子保存 |
| 数字精度 | `JsonValue` 存 `double`，**不满足** §6.6 | 满足（未知数值逐字节保留） |

探针的自检通过**不能**用来宣称 §6.5/§6.6 已验证 —— 那两项需要写出能力。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target loongbones_format_tests
ctest -R format_tests          # 或直接运行可执行文件
```

关闭：`-DLOONGBONES_BUILD_FORMAT_TESTS=OFF`。

## 测试分组（47 项）

| 组 | 覆盖点 |
|---|---|
| `parse` | RFC §4 严格解析：重复 key、非标准字面量、BOM、尾部多余内容、深度/大小上限 |
| `number` | RFC §6.6：未知大整数 / `1e400` / 高精度小数逐字节回写；已知字段越界拒绝 |
| `number`（边界） | Task 3 Back §2.1：占位**之前**用原始 token 长度执行 `maxStringBytes` 检查（返回 `kJsonSizeLimit` 且带 Pointer + 偏移）；§2.2：1025 个 `1e400` 未知字段在默认上限下仍成功解码并逐字节回写；批量占位不得改写字符串内的数字文本 |
| `anchor` | RFC §6.5.1：重排数组后未知字段仍归属**原对象**；`transform` 用相对子路径锚定 |
| `version` / `capabilities` | RFC §1.1、§1.2：formatId / 版本 / minimumReaderVersion / 能力 |
| `encode` | RFC §4.5：重复编码字节一致、往返稳定、未知键按原序输出 |
| `layering` | RFC §9：Format 失败不产生 Domain 诊断；非法 ID 由 Domain 的 `kIdMalformed` 报出 |
| `io` | Task 3 §4.1：创建/保存/打开往返、重复保存字节一致、保存失败不动源文件、无临时残留、另存不破坏原工程 |

## 已知限制

- `layering` 组的 Domain 侧只断言**第一条**违规：`Project::adoptData()` 内部对候选执行
  `validate()` 但只把首条放进 `Status`，且 `ProjectData` 成员私有，没有公开途径对候选再调一次。
  这是 Domain 既有 API 的边界，不是本测试的选择。
