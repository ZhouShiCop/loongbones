# BACK_TASK：修复节点父级类型约束后重新提交 Task 1 验收

- 来源：Task 1（Core Asset Model）验收
- 状态：`CLOSED_SUPERSEDED`
- 优先级：P1（核心数据不变量可被公共 API 破坏）
- 目标项目：`D:\Works\Spine\Loongbones`
- 建议执行分支：启用 Git 后按项目分支规范创建；当前不假设仓库已启用 Git

## 1. 退回原因

Task 1 现有测试程序运行结果为 35 通过、0 失败，但代码审查发现公共写 API 能创建非法树结构，且全量校验未报告该结构，因此不能通过验收：

1. `Project::createGroupNode(parent, ...)` 仅检查 parent 存在，不检查 parent 是 `GroupNode`，因此可以把分组挂到图片节点下。
2. `Project::moveNode(id, newParent, ...)` 未检查 newParent 的类型，因此可以把任意节点移动到图片节点下。
3. `Project::validate()` 没有检查含子节点的父节点必须是分组，导致以上非法状态可能通过整体校验。
4. `reorderChild()` 应拒绝非分组父节点，避免把图片节点当作可排序容器使用。

依据：`domain/src/project.cpp` 中 `createGroupNode`、`moveNode`、`reorderChild` 和 `validate` 的实现；Task 1 模型契约约定 GroupNode 可包含节点，ImageNode 为图片实例。

## 2. 修复范围

仅补齐父级类型约束及相应诊断/测试，不扩展 Task 1 功能范围，不改文件格式、Qt、图片解码、渲染或动画功能。

### 2.1 公共写操作

- `createGroupNode`：parent 不存在或不是 GroupNode 时返回失败，不改变 Project。
- `createImageNode`：保留现有 GroupNode 校验和失败不变状态行为。
- `moveNode`：newParent 不存在或不是 GroupNode 时返回失败；原父子列表、目标父子列表、节点 parent 和节点总数均保持原样。
- `reorderChild`：parent 必须存在且为 GroupNode，否则返回稳定诊断码；不得改动顺序。
- 确认诊断码命名及字符串序列化规则遵守既有稳定诊断契约；若需新增诊断码，只追加，不更改既有含义。

### 2.2 全量数据校验

- 对每个含子节点的节点，检查其类型必须为 GroupNode。
- 对通过 `adoptData()` 导入的非法数据，`validate()` 应返回可定位诊断；`adoptData()` 必须拒绝该数据，并保留原 Project 状态。
- 诊断 location 指向父节点；诊断文本说明图片节点不能包含子节点。

## 3. 必须新增的回归用例

1. 不能在 ImageNode 下创建 GroupNode；失败后 node/child 数量及 validate 结果不变。
2. 不能在 ImageNode 下创建 ImageNode；覆盖已有父级类型错误校验的失败原子性。
3. 不能把 GroupNode 或 ImageNode 移入 ImageNode；失败后原 parent、所有 children 顺序和节点总数均不变。
4. 不能对 ImageNode 调用 `reorderChild`；失败后数据不变。
5. `adoptData()` 遇到 ImageNode 带 children 的结构时返回失败，并保留接收方原有有效数据。
6. 对正常 GroupNode 嵌套、移动、重排的现有行为保持通过。

## 4. 验证与交付

- 使用项目登记的 `tools/core-tests/README.md` 命令，从当前源码重新构建测试程序，再运行 CTest 或测试可执行文件；不能只运行旧的预构建二进制。
- 预期：新增测试全部通过，原 35 项仍通过；报告总用例数、通过/失败/跳过数和退出码。
- 回归检查每次失败操作后 `Project::validate()` 仍为空，且相关数据快照与操作前一致。
- 不通过跳过用例、放宽断言或篡改既有基线获取通过结果。
- 若开发/验证工具有变动，按 `项目规范.md` 要求更新工具同级/上级使用说明及根目录 `STRUCTURE.md`；本任务如未改工具则无需改工具登记。
- 提交修复文件清单、关键差异、验证命令及结果、尚存限制；完成后停止，等待重新验收。

## 5. 验收状态

**状态：`CLOSED_SUPERSEDED`。** 曾按本 Back_task 的限制实现并由验收方从当前源码重建验证（42/42，CTest 1/1，无编译警告）。随后负责人明确：普通节点可以挂载子节点，GroupNode 不应独占容器能力。该限制性需求被后续产品契约取代；本文件保留作为历史修复记录，Task 2 需更新实现、诊断和回归测试以符合现行场景规则。

### 验收记录

- 新增 `test_parent_type.cpp` 的 7 个回归用例均通过；失败路径断言数据快照不变，且 `validate()` 结果为空。
- `adoptData()` 非法导入拒绝与原数据保留通过；新构建目录为 `build/acceptance-review`，未复用旧二进制。
- 尚有限制：同名 ImageAsset 仍拒绝创建，重复导入策略待 ADR-0004 相关决策确认。
- 本 Back_task 的父级限制不再是现行契约；不要据此继续拒绝 ImageNode/SpriteNode 作为父节点。
