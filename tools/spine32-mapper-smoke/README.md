# tools/spine32-mapper-smoke

Task 4 **B5** 的语料只读扫描：Reader → Atlas Reader → Mapper 的**端到端数数**。

合成断言（`tools/spine32-mapper-tests/`）覆盖不了真实语料的规模与脏数据。
本工具回答三个问题：

1. 有多少附件**精确命中**图集 region（查找键 `path ?? name`，field-mapping §10.1）；
2. 有多少走**尾随空格降级**（warning）；
3. 有多少**未命中**（error —— 静默丢弃的反面，必须能被点名）。

## 用法

```bash
cmake -S . -B <build> -DLOONGBONES_BUILD_SPINE32_MAPPER_SMOKE=ON
cmake --build <build> --target loongbones_spine32_mapper_smoke
<build>/tools/spine32-mapper-smoke/loongbones_spine32_mapper_smoke "$LOONGBONES_CORPUS_ROOT"
```

## 退出码

与 `tools/binary-read-smoke/` 同一套守门规则 —— **只有 0 可作验收证据**。

| 码 | 含义 |
|---|---|
| 0 | 全部单位映射通过，region 未命中 0 |
| 2 | 未给语料根 / 语料根不是目录 |
| 3 | 根下无 `.skel.bytes`（根目录很可能给错） |
| **4** | **没有任何单位走完映射链路**（根给错或图集命名不匹配），拒绝当作通过 |
| 6 | 存在 region 未命中 |

## 约定与边界

- **只读**：不写任何文件、不复制语料、不解码像素。
- 图集路径 = 同名 `.skel.bytes` → `.atlas.txt`；不读 PNG（本工具只关心名字能否解析到 region）。
- 为每个单位临时造一份只含名字的 canonical Atlas，**仅为提供 region 名字索引**；
  不校验页边界、不产生纹理资产、不落盘。
- 不进 CTest 默认集合（需要本地语料）；`LOONGBONES_BUILD_SPINE32_MAPPER_SMOKE` 默认 OFF。
