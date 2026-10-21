# 首批样本选择（Loongbones Phase 0）

- 来源：`tools/corpus-scan/out/corpus-manifest.json`（全量 1113 个文件，primary 口径 963 个）
- 选出 16 个：**primary 单位**（`<dir>/<dir>.skel.bytes`）5 个，
  **变体**（子目录/异名，如 `<dir>/<sub>/<name>.skel.bytes`）11 个
- 回归分母建议：能力指纹对齐用 primary 口径 963 个；本样本集另含变体，用于覆盖 primary 里没有的形态
- 语料根由 `LOONGBONES_CORPUS_ROOT` 提供，**不写入本文件**

| # | 相对路径 | 口径 | 体积 | SHA-256(前12) | 骨骼 | 插槽 | 动画 | IK | mesh | 加权 | deform | 三件套 | 选择理由 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `h3181c2/h3181c2.skel.bytes` | primary | 75369 | ec652e761fbf | 31 | 19 | 8 | 1 | 8 | 8 | 57 | ✔ | 全语料唯一含事件的样本 |
| 2 | `b3310c3/b3310c3_cocoon/b3310c3_cocoon.skel.bytes` | variant | 425 | 4555bd7100ec | 1 | 1 | 2 | 0 | 0 | 0 | 0 | ✔ | 最小骨架，覆盖基础骨骼/插槽/region |
| 3 | `h0261s5/s3/h3004c3.skel.bytes` | variant | 22427 | 3d3632b99e48 | 55 | 17 | 1 | 2 | 8 | 8 | 3 | ✔ | 最深层级，覆盖父子变换与继承 |
| 4 | `h0215s5/h0215s5_doll2/h0215s5_doll2.skel.bytes` | variant | 7883 | ecfa7c54640a | 25 | 12 | 1 | 3 | 5 | 2 | 5 | ✔ | IK 约束 |
| 5 | `h0166s5/h0166s5_buoy/h0166s5_buoy.skel.bytes` | variant | 26134 | 38ae5412577e | 16 | 14 | 2 | 0 | 4 | 4 | 8 | ✔ | transform 约束 |
| 6 | `b0015s5/b0015s5_meat/b0015s5_meat.skel.bytes` | variant | 1050 | 5bdbbf602d34 | 2 | 1 | 1 | 0 | 1 | 0 | 1 | ✔ | 非加权 mesh |
| 7 | `e0039c1/fx_e0039c1_s01/e0039c1_s01_bug.skel.bytes` | variant | 1317 | 53e85b0f92ec | 4 | 1 | 1 | 0 | 1 | 1 | 0 | ✔ | 加权 mesh |
| 8 | `t0269s5/s1/s1_shan1.skel.bytes` | variant | 8691 | ab48654eb35e | 16 | 6 | 1 | 1 | 5 | 5 | 5 | ✔ | 加权+deform+IK 组合 |
| 9 | `h0210s5/h0210s5_s3/h0210s5_S3.skel.bytes` | variant | 34050 | 38da4a5586bb | 25 | 11 | 1 | 0 | 9 | 9 | 8 | ✔ | 大网格顶点量 |
| 10 | `h0040s5/bowl/h0040s5.skel.bytes` | variant | 4561 | 3cffbdf06632 | 2 | 2 | 18 | 0 | 0 | 0 | 0 | ✔ | 动画数量最多 |
| 11 | `h0133s5/h0133s5.skel.bytes` | primary | 973731 | dcf446c7f893 | 105 | 43 | 54 | 2 | 28 | 25 | 1458 | ✔ | 时间线数量最多 |
| 12 | `h0164s5/watch/h0164s5_watch_minute.skel.bytes` | variant | 1026 | 0c77485f0af8 | 3 | 1 | 3 | 0 | 0 | 0 | 0 | ✘ | 体积最大（加载/性能边界） |
| 13 | `e0039c1/fx_e0039c1_s02/e0039c1_s02.skel.bytes` | variant | 1028 | 50c731c003bc | 4 | 2 | 1 | 0 | 0 | 0 | 0 | ✔ | 体积最小 |
| 14 | `b0001s3/b0001s3.skel.bytes` | primary | 138954 | e9bac82690f9 | 29 | 23 | 13 | 2 | 16 | 9 | 169 | ✔ | 历史调查核心样本（16 mesh / 11 旋转 region） |
| 15 | `b0146c3/b0146c3.skel.bytes` | primary | 55790 | 48e1a4fbea1a | 40 | 26 | 9 | 5 | 10 | 10 | 11 | ✔ | 能力覆盖补齐 |
| 16 | `h3687c4/h3687c4.skel.bytes` | primary | 56418 | cb1f7a2411ba | 41 | 25 | 8 | 3 | 11 | 9 | 88 | ✔ | 能力覆盖补齐 |

## 能力覆盖核对

- ✔ simple-skeleton
- ✔ deep-hierarchy
- ✔ ik
- ✔ transform-constraint
- ✔ mesh
- ✔ weighted-mesh
- ✔ deform
- ✔ event
- ✔ many-animations
- ✔ large-mesh
- ✔ complete-trio

## 语料能力边界（供 Phase 1 能力矩阵参考）

- 附件类型：全语料只有 `region` 与 `mesh` 两类；`boundingbox`/`path`/`clipping`/`point` **0 个**
- `path` 约束：**0 个**
- 事件：`ir.events` 合计 **1** 个，event 时间线 **0** 条 —— 事件能力在本语料几乎无法验证
- 解析：strictEof **0 失败**，未发现截断/损坏
- 版本：全部 `3.2.01`
- 三件套齐全：1110/1113
