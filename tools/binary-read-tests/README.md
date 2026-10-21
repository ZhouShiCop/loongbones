# tools/binary-read-tests

Task 4 B4 的 **Spine 3.2.01 二进制 Reader 自动化测试**（27 项）。

被测对象是 `importers/spine32` 的 Reader 公开 API。输入全部是**内存合成
fixture**，不读真实语料 —— 真实语料的扫描由 [`../binary-read-smoke/`](../binary-read-smoke/)
承担（需要本地语料根，故不进 CTest 默认集合）。

## 用法

```bash
cmake -S . -B <build> -G "MinGW Makefiles"
cmake --build <build> --target loongbones_binary_read_tests
ctest --test-dir <build> -R binary_read_tests --output-on-failure
```

或直接跑 `<build>/tools/binary-read-tests/loongbones_binary_read_tests.exe`。

MinGW-w64 不在 PATH 时需前置 `.devtools/mingw64/bin`；见仓库外的
`D:/Works/Spine/.devtools/README.md`。

## 覆盖

| 类别 | 用例 |
|---|---|
| 合法输入 | `MinimalDocumentAccepted`、`EmptyDefaultSkinAccepted`、`NonessentialImagesPathAccepted` |
| 头字段 | `HeaderFieldsCaptured`、`HashIsNotTreatedAsVersion`、`UnsupportedVersionRejected` |
| strict EOF | `TrailingBytesRejected`、`TruncatedRejected`、`EmptyInputRejected` |
| 非有限浮点 | `NanInBoneRejected`、`InfinityInAttachmentRejected`、`NonFiniteDiagnosticCarriesOffsetAndSection` |
| 结构闭合 | `BoneParentForwardReferenceRejected`、`BoneParentOutOfRangeRejected`、`SlotBoneIndexOutOfRangeRejected`、`UnknownBlendModeRejected` |
| 附件类型 | `UnknownAttachmentTypeRejected`（类型字节 ≥6）、`BoundingBoxAttachmentTraversed`、`MeshAttachmentTraversed`、`WeightedMeshAttachmentTraversed`、`WeightedMeshBadBoneCountRejected` |
| 长度前缀边界 | `VarintOverflowRejected`、`StringLengthLimitRejected`、`ArrayCountLimitRejected`、`MaxVarintAccepted`（`0xFFFFFFFF` 是合法 varint，不得因"看起来大"被拒） |
| 失败语义 | `FailureProducesNoPartialSkeleton`（失败时 skeleton 为空）、`ResultCodeStringsAreStable` |

其中三条 `...Traversed` 断言的是**mesh 族附件可以被安全遍历到 EOF**，
但不进 IR（B4 只做解析不做映射，见 `docs/skeleton/task4_B4.md`）；
它们同时也是附件类型布局的回归锁 —— 布局一旦改错，这几条会先红。

## 输入 / 输出

- 输入：无（纯内存构造，`tests/importers/spine32/test_binary_reader.cpp` 内的 `W` 写入器）。
- 输出：终端逐用例 `PASS`/`FAIL` 与汇总；全通过退出码 0。
- 依赖：C++17、CMake、MinGW-w64；复用 `tests/domain/test_framework.hpp`，零第三方测试库。
- 可安全重跑：是（全部为确定性合成输入）。

## 与语料扫描的关系

| | 本工具 | `binary-read-smoke` |
|---|---|---|
| 输入 | 合成 fixture + 畸形输入 | 真实语料 1113 个 `.skel.bytes` |
| 断言 | 每条规则一个用例 | 严格到达 EOF + manifest 计数对照 |
| 进 CTest 默认集合 | 是 | 否（需 `LOONGBONES_BUILD_BINARY_READ_SMOKE=ON`） |

两者互补：本工具保证规则被逐条锁住，扫描工具保证真实语料全通。
