# tools/skeleton-import-smoke

Task 4 **B6** 的**最小宿主**：在真实语料上跑端到端闭合 ——
**注册表 → prepare → 事务提交 → 重开验证**。

与同级 smoke 工具的分工：

| 工具 | 链路 | 是否落盘 |
|---|---|---|
| `binary-read-smoke` | Reader | 否 |
| `spine32-mapper-smoke` | Reader → Atlas → Mapper | 否 |
| **`skeleton-import-smoke`** | **上面全部 + 宿主事务提交 + 重开比对** | **是** |

本工具会**真的写出**资源文件与 `project.json`，再**真的重开**工程逐项比对对象数。
前两级工具只回答"名字能否解析到 region"，本工具回答"整个宿主能不能闭环"。

## 用法

```bash
cmake -S . -B <build> -DLOONGBONES_BUILD_SKELETON_IMPORT_SMOKE=ON
cmake --build <build> --target loongbones_skeleton_import_smoke
<build>/tools/skeleton-import-smoke/loongbones_skeleton_import_smoke "$LOONGBONES_CORPUS_ROOT"
```

选项：

| 选项 | 说明 |
|---|---|
| `--project <dir>` | 导入落地目录（默认临时目录 `lb_skeleton_import_smoke`） |
| `--limit <n>` | 完整提交（含重开验证）的单位数上限，默认 8 |
| `--reject-warnings` | 有 warning 时拒绝提交（默认接受并计入报告） |

默认**不构建、不进 CTest** —— 仓库不含语料，需要本地显式打开。

## 退出码

与 `tools/binary-read-smoke/`、`tools/spine32-mapper-smoke/` 同一套守门规则 ——
**只有 0 可作验收证据**。

| 码 | 含义 |
|---|---|
| 0 | 端到端闭合通过：对象数一致，region 未命中 0，无 error 级诊断 |
| 2 | 未给语料根 / 语料根不是目录 |
| 3 | 根下无 `.skel.bytes`（根目录很可能给错了） |
| 4 | 没有任何单位产出有效候选，拒绝当作通过 |
| 5 | 提交失败，或重开后对象数与提交前不一致 |
| 6 | 存在 error 级诊断（region 未命中、Domain 拒绝等） |
| 7 | 有单位 prepare 失败，且全部属"同目录存在图集但不同名"的已知布局缺口 |

7 与 6 分开是为了让"语料布局特例"和"代码/数据真的有问题"不被混为一谈；
两者都**不是**通过。

### 逐项语义比对（Task 5 B5）

B6 起本工具只比对**对象数**；B5 起在重开验证里追加**逐项**比对，覆盖第二批字段：

- 附件：`type` 分支一致；`uvs` / `vertices`（容差）、`triangles`（精确）、
  加权 `vertexBindings` 的 `boneId`（精确）与 `x` / `y` / `weight`（容差，权重按 D30 原值比对）、
  linked 的 `parentKey.{slotId, attachmentName}` 与 `inheritDeform`；
- 约束：IK 的 `id` / `name` / `boneIds` / `targetBoneId` / `enabled`（D35）/ `bendPositive` / `mix`；
  transform 的 4×mix + 6×offset（D36 原值）。

输出一行**统计量**（ASCII 前缀，便于管道取证）：

```
[deep-compare] items=<比对项数> maxResidual=<最大浮点残差> mismatches=<不一致项数>
```

`mismatches > 0` 时重开判定为不一致（退出码 5）。实测（2026-10-23，语料 8 个完整提交单位）：
`items=18067 maxResidual=0 mismatches=0` —— native 的 double 文本往返是**精确**的，不是近似。

> 注意：退出码 7（共享图集缺口，Task 6）的路径**在重开验证之后**打印，
> 但会先于结论返回 ⇒ 该路径下不会给出"端到端闭合通过"的判语，不得据退出码 7 宣称闭环通过。

## 它验证什么

1. **注册表**：`registerBuiltinPlugins()` 之后能用 `spine.binary@3.2.01` 找到插件；
   版本或格式不符必须返回 `nullptr`。
2. **prepare 不改目标工程**：候选是完整副本，用临时 `Project::adoptData()` 全量校验。
3. **事务顺序**：资源 stage → publish（不覆盖既有文件）→ 原子 `saveProject()`。
4. **重开一致**：保存→重开后骨架/图集/纹理/区域/骨骼数量与提交前一致。
5. **无静默丢失**：`animations` / `timelines` / `mesh` 族 / IK / transform / 事件
   只计数 + warning，全部在报告里列出。

## 语料实测基线（2026-10-21）

语料根 `…/assets/game/character/spinedata`，1113 个单位：

| 指标 | 值 |
|---|---|
| prepare 成功 | 1111 |
| prepare 失败 | 2（均为共享图集布局） |
| region 未命中 | **0** |
| 尾随空格降级 | 0 |
| 完整提交 + 重开 | 8 / 8 一致 |
| 落盘资源文件 | 8 |
| 进度回调 | 5561 次，异常 0 |

## 边界与纪律

- **只读**：语料经 `FileSystemResourceResolver` 限死在授权根内，只查询、不复制、
  不解码像素；写入只发生在 `--project` 指定的暂存目录。
- 图集路径规则 = 同名替换（`<stem>.skel.bytes` → `<stem>.atlas.txt`）。
  语料里 `h0164s5/watch/` 下两个骨架共用 `h0164s5_watch.atlas.txt`，
  与该规则不符 → 计入退出码 7，属**已知缺口**：支持它需要给
  `IResourceResolver` 增加目录列举能力，超出 B6 范围。
- 不复制或提交任何真实游戏输入。
