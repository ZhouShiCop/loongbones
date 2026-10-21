# tools/spine32-mapper-tests

Task 4 **B5** 的自动化测试：`.atlas` 文本 Reader 与 Source→Canonical Mapper。

覆盖 `tests/importers/spine32/test_atlas_text_reader.cpp`（17 项）与
`tests/importers/spine32/test_mapper.cpp`（15 项）。

## 运行

```bash
cmake --build <build> --target loongbones_spine32_mapper_tests
ctest --test-dir <build> -R spine32_mapper_tests --output-on-failure
```

或直接跑 `<build>/tools/spine32-mapper-tests/loongbones_spine32_mapper_tests`。

## 输入

无外部输入 —— 全部是**内存里的合成文本与合成 IR**。

之所以必须用合成输入：语料 1113 个单位**全部是单页图集**（`sample-manifest.md` §2），
多页、`rotate: true`、trim/`orig`、`format`/`filter`/`repeat`/`pma`、序列 `index`
在真实语料里 0 例或多例但无法构造反例，只有合成文本能给断言。

## 断言要点

| 组 | 断言 |
|---|---|
| Atlas Reader | 页声明 `size` 原样保留（不与 PNG IHDR 混淆）；`orig`/`offset`/`rotate`/`index`；多页归属；页属性解析进 IR **且**给 warning；缺 `xy`/`size`、区域先于页、空页、页缺 `size`、属性未缩进 → error；Unity TextAsset 包装被剥离 |
| Mapper | 所有 ID 由宿主 `IIdFactory` 发放；骨骼父子链与 `boneOrder`；查找键 `path ?? name` 三级回退；尾随空格降级=warning、关闭降级=error；未命中=error；非 region 附件**只计数不建对象**；不支持能力只 warning；颜色 `0xRRGGBBAA`→0..1、blend 枚举；`drawOrder` 与 `slots` 互为排列；映射结果过 Domain `adoptData()` |

## 纪律

- 只用 `tests/domain/test_framework.hpp`，零第三方测试库。
- 不读真实语料、不写任何文件；语料级验证另见 `tools/binary-read-smoke/`。
