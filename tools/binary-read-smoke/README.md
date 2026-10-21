# tools/binary-read-smoke

Task 4 B4 的**语料只读扫描**工具：用真实 Spine 语料独立校验
`importers/spine32` 的二进制 Reader（strict EOF + 结构计数）。

## 为什么单独一个工具

- 仓库**不含语料**；语料根由本地配置提供，不能进 CTest 默认集合。
- 单元测试（`tools/binary-read-tests/`）覆盖合成 fixture 与畸形输入；
  本工具覆盖"真实语料能否全部严格解析"。

## 用法

```bash
cmake -S . -B <build> -DLOONGBONES_BUILD_BINARY_READ_SMOKE=ON
cmake --build <build> --target loongbones_binary_read_smoke
<build>/tools/binary-read-smoke/loongbones_binary_read_smoke "$LOONGBONES_CORPUS_ROOT"
```

或直接传语料根目录作为第一个参数。

## 输出

| 段 | 含义 |
|---|---|
| `Full` | 语料根下**全部** `.skel.bytes` 的 ok / fail |
| `Primary` | `<目录>/<目录>.skel.bytes` 子集（一个单位只算一份）的 ok / fail |
| 失败分类 | 按 `ReadCode` 聚合的失败计数 |
| 前 20 条失败明细 | 相对路径 + 码 + **字节偏移** + 段名 |
| manifest 锁定样本对照 | 与 `docs/skeleton/sample-manifest.md` §1/§2 的**全部 16 个**样本对比 骨/槽/皮肤/region |

`Full` 与 `Primary` 是两个不同分母（`sample-manifest.md` §4），**不得混用**。

## 退出码

**只有 0 才可作为验收证据。** 单独看"扫描到了 N 个文件"不算通过 —— 语料根给错时
工具照样能扫到上千个文件，此时锁定样本一个都匹配不到，必须判失败。

| 码 | 含义 |
|---|---|
| 0 | Full 全通过 + 锁定样本全部一致 |
| 2 | 未给语料根 / 语料根不是目录 |
| 3 | 语料根下没有任何 `.skel.bytes`（根目录很可能给错） |
| 4 | **16 个锁定样本一个都没匹配到** —— 语料根给错的高风险信号，拒绝当作通过 |
| 5 | 锁定样本有匹配到但数值不一致 |
| 6 | 仍有文件未通过 strict EOF |

锁定样本缺失（语料里找不到该相对路径）单独计为「未找到」，不计入 `matched`；
因此"扫到很多文件但一个锁定样本都没有"会命中退出码 4。

## 纪律

- **只读**：不写任何文件、不复制语料、不输出像素。
- 语料路径只作为输入参数，不写入任何产物。
- 本工具不是产品依赖；`LOONGBONES_BUILD_BINARY_READ_SMOKE` 默认 OFF。
