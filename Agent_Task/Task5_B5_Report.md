# Task 5 阶段 B：B5 交付报告（语料闭环与回归）

- 上游：`Agent_Task/Task5_Implementation_Plan.md` §4 B5；B1+B2 / B3 / B4 均已完成（B4 三次复核通过）
- 日期：2026-10-23
- 状态：**B5 完成**；停点：按计划 §9 停下等待复核

## 1. 干净构建与全量回归（B5 第 1 条）

构建目录 `D:\Works\Spine\.builds\build_b5` —— **全新目录，未复用任何历史缓存**。

```
cmake -S D:\Works\Spine\Loongbones -B D:\Works\Spine\.builds\build_b5 -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_CXX_COMPILER=D:/Works/Spine/.devtools/mingw64/bin/g++.exe ^
  -DCMAKE_MAKE_PROGRAM=D:/Works/Spine/.devtools/mingw64/bin/mingw32-make.exe ^
  -DLOONGBONES_BUILD_SKELETON_IMPORT_SMOKE=ON -DLOONGBONES_BUILD_BINARY_READ_SMOKE=ON
mingw32-make -C D:\Works\Spine\.builds\build_b5 -j8
ctest --output-on-failure          (cwd = build_b5)
```

| 项 | 结果 |
|---|---|
| 工具链 | MinGW-W64 `g++ 16.2.0`（`D:\Works\Spine\.devtools\mingw64`）、CMake 4.4.4、`mingw32-make` |
| cmake 配置 | exit **0** |
| 编译 | exit **0**；**error 0、warning 0** |
| ctest | exit **0**，**7 / 7 通过，0 失败、0 跳过** |

各套件断言（干净构建目录下的新二进制，非旧二进制）：

| 套件 | 断言数 | 通过 | 失败 |
|---|---:|---:|---:|
| `loongbones_core_tests` | 135 | 135 | 0 |
| `loongbones_format_tests` | 86 | 86 | 0 |
| `loongbones_binary_read_tests` | 47 | 47 | 0 |
| `loongbones_spine32_mapper_tests` | 48 | 48 | 0 |
| `loongbones_skeleton_import_tests` | 40 | 40 | 0 |
| `loongbones_atlas_import_tests` | 9 | 9 | 0 |

## 2. Full / Primary 双口径（B5 第 2 条，D12 不混分母）

```
build_b5\tools\binary-read-smoke\loongbones_binary_read_smoke.exe "<语料根>"
```

| 口径 | 单位 | 通过 | 失败 |
|---|---:|---:|---:|
| **Full** | **1113** | **1113** | **0** |
| **Primary** | **963** | **963** | **0** |

- 退出码 **0**（该工具的 0 = Full 全通过 + manifest 锁定样本全部一致）。
- manifest 锁定样本对照逐条 OK，覆盖含第二批数据的样本（如 `h3181c2` 骨 31 / 槽 19 / region 14 / **mesh 8**、
  `h0133s5` 骨 105 / 槽 43 / region 55 / **mesh 28**）。
- 两个分母**分别报告**，未合并。

## 3. 真实样本的逐项语义比对（B5 第 3 条）

B6 起的 `skeleton-import-smoke` 只比对**对象数**；B5 在重开验证里追加了**逐项**比对：

- 附件：`type` 分支；`uvs` / `vertices`（容差）；`triangles`（精确）；加权 `vertexBindings` 的
  `boneId`（精确）与 `x` / `y` / `weight`（容差，权重按 **D30 原值**比对，不归一化）；
  linked 的 `parentKey.{slotId, attachmentName}` 与 `inheritDeform`。
- 约束：IK 的 `id` / `name` / `boneIds` / `targetBoneId` / `enabled`（**D35**）/ `bendPositive` / `mix`；
  transform 的 4×mix + 6×offset（**D36 原值**）。
- 输出一行统计量（ASCII 前缀便于管道取证），`mismatches > 0` 即判重开不一致（退出码 5）。

实测（语料 8 个完整提交单位，`--limit 8`）：

```
[deep-compare] items=18067 maxResidual=0 mismatches=0
```

⇒ **18,067 项逐项比对全部一致，最大浮点残差 0**（native 的 double 文本往返是**精确**的，不是近似）。
样本特征互不相同（骨 1…105、mesh 0…28、含加权与约束单位）。

同一轮还确认端到端计数一致：骨架 / 图集 / 纹理 / 区域 / 骨骼在提交前后逐项相同。

## 4. 失败原子性（B5 第 5 条）

既有断言覆盖（本轮全部通过）：

- `StagingFailureRollsBackAndLeavesNoFile` —— 资源暂存失败：回滚、不留文件；
- `PublishFailureRollsBackAndLeavesNoFile` —— 资源发布失败：同上；
- `SaveFailureRollsBackResources` —— `project.json` 保存失败：资源一并回滚。

⇒ 注入失败后目标工程前后快照一致、无半成品实体或临时资源（B6 基线，本轮零回归）。

## 5. 退出码口径（B5 第 6 条，不包装）

| 工具 | 退出码 | 判读 |
|---|---:|---|
| `binary-read-smoke`（双口径扫描） | **0** | ✅ 可作验收证据 |
| `ctest` | **0** | ✅ 可作验收证据 |
| `skeleton-import-smoke`（端到端） | **7** | ⚠️ **已知共享图集布局缺口（2 个单位，Task 6）**，**不作验收证据** |

退出码 7 的路径会先于结论返回 ⇒ 该轮**没有**给出"端到端闭合通过"的判语；
但同轮的 `[deep-compare]` 已执行且全一致（见 §3），两者分开陈述，不互相顶替。

## 6. 工具登记（B5 第 7 条，规范 §8.2）

本轮改动的工具：`tools/skeleton-import-smoke`（新增逐项语义比对）。
已在该工具就近 `README.md` 增加「逐项语义比对（Task 5 B5）」小节：覆盖字段、容差、输出格式、
实测数字，以及「退出码 7 的路径会先于结论返回，不得据此宣称闭环通过」的提醒。

## 7. 未完 / 待裁定

- **`sourceVersion` 的 kV2 遗留**（`atlas_import.cpp:329`、`spine32_import_plugin.cpp:378`）：
  落盘已是 v3，但这两处仍把内存文档标成 kV2，语义不自洽。改动会触碰既有断言
  `tests/importers/test_spine32_plugin.cpp:350-351` ⇒ **未擅自改，等裁定**（详见 `Task5_B4_Report.md` §4.5）。
- **BoundingBox**：语料 0 例（D32），B5 无真实样本可跑，只有类型占位与诊断，无功能验证。
- **共享图集缺口（Task 6）**：2 个单位，本轮仍未闭合（不在 B5 范围）。
- 工作区：B1/B2 返修 + B3 + B4 + B5 的改动**全部未提交**（停在 `049b1d3`）。

## 8. 停点

B5 判据（干净构建回归、双口径、逐项比对、失败原子性、退出码口径、工具登记）已逐项落地；
按计划 §9「每完成一个工作包即停止等待复核」，此处停下。阶段 B 的 B1–B5 至此全部完成。
