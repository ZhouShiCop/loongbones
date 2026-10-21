# Spine 3.2.01 骨架只读探针报告（Task 4 阶段 A）

- 生成工具：`tools/skeleton-probe/probe.mjs`（用法见同目录 `README.md`）
- 语料根：由 `LOONGBONES_CORPUS_ROOT` / 命令行参数提供，**不写入本文件**
- 本次分析文件数：**4**（其中 primary 口径 **0**）
- 耗时：0 s

## 1. 解析与版本

| 指标 | 值 |
|---|---|
| 解析成功 | 3 |
| 解析失败 | 1 |
| 失败分类 | {"parse-error":1} |
| strictEof 失败（截断/错位） | 0 |
| 版本分布 | 3.2.01 |
| 不同 skeleton hash 数 | 1 |
| nonessential=true / false | 0 / 3 |
| imagesPath 非空 | 0 |
| .atlas.txt 齐全 | 2 / 4 |
| .png 齐全 | 2 / 4 |
| 三件套齐全 | 2 / 4 |

## 2. 引用闭合

| 指标 | 值 |
|---|---|
| 全部闭合的骨架 | 2 / 3 |
| 骨骼父索引越界 | 1 |
| 骨骼父索引前向引用 | 0 |
| 骨骼自环 | 0 |
| 根骨骼总数 | 3 |
| 插槽骨骼索引越界 | 0 |
| IK 骨骼/目标索引越界 | 0 |
| transform 约束索引越界 | 0 |

## 3. 名称唯一性

- 骨骼重名：0
- 插槽重名：0
- 同 slot 内附件重名：0

## 4. 附件 path 语义

- path 存在：1；path 为 null：3
- **path ≠ 附件名**：1
- region rotation ≠ 0：1
- region scale ≠ 1：0
- mesh UV 越出 [0,1]：0
- 三角索引越界：0

## 5. 图集形态

| 指标 | 值 |
|---|---|
| 页数 / region 数 | 2 / 4 |
| rotate=true / false / 其它写法 | 1 / 3 / 0 |
| 存在 trim（offset≠0） | 0 |
| size ≠ orig（缩放或裁剪） | 0 |
| index ≠ -1（sequence） | 0 |
| 页对应 PNG 缺失 | 0 |
| PNG IHDR 与页声明尺寸一致 / 不符 | 2 / 0 |
| 页声明 size 存在 / 缺失 | 2 / 0 |
| region 越出页声明尺寸 | 0 |

**页声明尺寸 vs PNG 实际尺寸的比例分布（前 10，仅比例值）**：

| pngW / pageW | 页数 |
|---|---|
| 1.000 | 2 |

## 6. 附件 → 图集 region 查找率

- 按 `path ?? name`（Spine 规则）：命中 4，未命中 0
- 仅按附件名：命中 3，未命中 1

## 7. 对象计数（双口径，不混用分母）

| 指标 | Full | Primary |
|---|---|---|
| files | 3 | 0 |
| bones | 8 | 0 |
| slots | 5 | 0 |
| skins | 3 | 0 |
| animations | 0 | 0 |
| timelines | 0 | 0 |
| ik | 0 | 0 |
| transform | 0 | 0 |
| pathConstraints | 0 | 0 |
| events | 0 | 0 |
| regionAttachments | 4 | 0 |
| meshAttachments | 0 | 0 |
| weightedMeshAttachments | 0 | 0 |
| linkedMeshAttachments | 0 | 0 |
| boundingboxAttachments | 0 | 0 |
| otherAttachments | 0 | 0 |
| boneTimelines | 0 | 0 |
| slotTimelines | 0 | 0 |
| deformTimelines | 0 | 0 |
| ikTimelines | 0 | 0 |
| drawOrderTimelines | 0 | 0 |
| eventTimelines | 0 | 0 |
| transformTimelines | 0 | 0 |
| multiSkinUnits | 0 | 0 |
| multiPageAtlases | 0 | 0 |
