# TASK 2 BACK TASK：补齐 Scene / Atlas 全量校验与 Sprite 公共 API

- 来源：Task 2 独立验收复核
- 状态：`CLOSED`（返修完成并复核通过）
- 优先级：P1（全量导入校验不完整；Sprite 区域引用不能经领域写 API 创建）
- 目标项目：`D:\Works\Spine\Loongbones`
- 最初复验：`build_t3` clean-first 构建通过，63/63 用例；当时发现的两项缺口已修复，最终复核见下文

## 1. 复核结论

以下已确认通过：

- `test_validation.cpp::DuplicateNodeIdDetected` 现构造 root + g1 + g2，再复制节点 ID，不再越界。
- 普通节点挂载规则已调整：图片节点下创建/移动子节点成功；根保持特殊身份。
- `build_t3` clean-first 重建成功；直接用例程序 45/45，CTest 1/1 通过，退出码 0，编译无警告。

以下项目仍未通过 Task 2 规格，需修正后重新提交：

### 最新复验补充

原 Scene/root、Atlas 全局身份与引用闭合、Sprite 公共 API、绘制顺序等返修项现已覆盖；当前测试文件含 23 个用例，测试总数为 63。此前“仅 5 个 TEST”的计数结论已过时。

仍有两项验收阻塞：

1. `domain/src/project.cpp::regionTrimFits()` 按 `rotation` 交换 `trimmedW/H` 后再与 `originalW/H` 比较，但 `AtlasRegion` 注释和 `hasNoTrim()` 将裁切尺寸定义在原图方向。非正方形无裁切 Region（例如 original/trimmed 为 64×48、旋转 90°）因此会被错误拒绝。请统一并明确尺寸方向语义；至少新增 90°/270° 非正方形无裁切和带 trim 的测试，分别覆盖合法边界与越界拒绝。
2. 多个 `adoptData()` 失败测试只检查接收对象的空容器或个别字段，没有按本任务 §4 对**完整接收方快照**作前后比较。请用已含有效节点、资源、Scene/Atlas 数据的接收对象作为失败前状态，并对每类失败导入断言完整 `ProjectData` 快照完全不变。

## 2. 必须修复

### 2.1 Scene/root 关系必须由 `validate()` 完整闭合

当前 `Project::validate()` 检查 Scene ID 和 `defaultSceneIndex`，但没有验证每个 `Scene::rootNodeId` 是否存在、是否指向 `NodeType::kRoot`，也没有保证默认 Scene 的 root 与兼容字段 `ProjectData::rootNodeId` 一致。仅校验 `rootNodeId` 本身可让 `adoptData()` 接受 Scene 指向缺失/错误节点的数据；随后 `defaultScene()`、`drawOrder()` 与 `rootNodeId()` 会观察到彼此矛盾的根。

要求：

1. 明确 Task 2 的 Scene 数量契约：若仅支持一个 Scene，`validate()` 拒绝额外场景；若支持多个，则每个 Scene 必须有有效且互不冲突的 Root，并明确节点树归属和遍历语义。
2. 校验 Scene ID 合法、唯一；Scene root 必须指向存在的 `kRoot` 节点；根必须无父、恒等变换、唯一且不能被挂载。
3. 默认 Scene 与 `ProjectData::rootNodeId` 必须相符，`setDefaultScene()` 后查询、校验和遍历使用同一个根身份。
4. `adoptData()` 对不一致候选必须返回可定位诊断且不改变接收对象。
5. 新增回归：缺失 Scene root、指向普通节点、默认 Scene/rootNodeId 不匹配、多个 Scene 复用一个 root，以及 Scene 数量策略边界。

### 2.2 Atlas 元数据校验必须覆盖导入路径和全局身份

当前 `addAtlasRegion()` 会检查基本矩形、旋转与页边界，但 `validate()` 未重复页边界检查，因此通过 `ProjectData`/`adoptData()` 导入的越界 Region 可能被接受。`validate()` 也没有对 TextureAsset、Atlas、AtlasPage、AtlasRegion 的 ID 合法性和同类型全局重复做完整检查；写 API 的 ID 重复检查局限于单个 Atlas，跨 Atlas 重复 ID 会导致 `findPage()` / `findRegion()` 查找歧义。Trim offset/尺寸与原始尺寸的组合也未验证。

要求：

1. 将同一套完整 Atlas 校验用于公开写 API 和 `validate()`/`adoptData()`，避免写入校验与导入校验两套规则漂移。
2. 校验纹理、图集、页、区域 ID 的 kind/格式及同类型全工程唯一性；跨 Atlas 重复的 Page/Region ID 必须拒绝。
3. 全量验证页引用纹理存在；区域引用本 Atlas 内的有效页（不能跨 Atlas 指向另一图集的 Page）；旋转后实际占位矩形不越过页边界。
4. 检查 packed rect、原始/裁切尺寸及 trim offset 的正值、非负和边界关系；边界算术避免有符号整数溢出。
5. 增加恶意 `ProjectData` fixture，经 `adoptData()` 验证越界 rect、无效 ID kind、跨 Atlas 重复 ID、无效 trim、无效 page/texture 引用均被拒绝，且接收对象完整快照不变。

### 2.3 提供 SpriteNode → AtlasRegion 的公开创建路径

当前 `NodeRecord` 有 `regionId`，但公开 `createImageNode()` 只接收 `ImageAssetId` 并设置 `assetId`；没有公共领域 API 能创建引用 AtlasRegion 的 SpriteNode。测试只能手工改候选 `ProjectData` 再 `adoptData()`，这不满足 Task 2 的 Sprite 区域引用 API 契约。现有校验也允许 Root/GroupNode 携带有效 regionId。

要求：

1. 新增明确的 Sprite 创建/设置 API，以稳定 `AtlasRegionId` 建立节点引用；保留 Task 1 `ImageNode` API 和已有 ID，不批量迁移或重建。
2. 只有图片/Sprite 节点可引用 Region；Root 与 GroupNode 不得保存 Region 引用。明确旧 ImageAsset/ImageNode 与 TextureAsset/AtlasRegion 的兼容映射；多步骤映射失败不得遗留新 TextureAsset、Atlas 或 Page。
3. 为不存在区域、跨错误资源、失败原子性、多个 Sprite 共享 Region 且变换独立新增测试。

## 3. 次要契约清理

- `drawOrder()` 目前返回 Root。场景规格规定 Root 不直接绘制；请明确该 API 是“含 Root 的树遍历顺序”还是“渲染节点顺序”。若是后者，结果应排除 Root；若保留 Root，改名/注释并提供不含 Root 的 drawable-order API 或保证渲染层过滤 Root，并增加对应测试。
- 提交报告须按测试框架真实计数描述新增用例数，勿把测试分组数当用例数。
- 修复成功后更新 `tools/core-tests/README.md` 覆盖文件清单、Task 2 报告和 `STRUCTURE.md` 测试总数。未改验证工具时无需改工具登记。

## 4. 交付与验收

- 修改范围限 `domain/`、`tests/`、`docs/`、`Agent_Task/Task2*`、`tools/core-tests/README.md` 与 `STRUCTURE.md`。
- 每个 `adoptData()` 失败用例都需断言接收方快照不变；错误带稳定诊断码和可定位 location。
- 使用新的干净构建目录重新配置、构建、运行测试程序和 CTest；报告总用例、通过/失败/跳过、退出码和编译警告。
- 确认现有 45 项全部保留或说明有语义依据的替换，并报告真实新增用例数量。
- 不实现 JSON codec、Spine 导入、Qt GUI 或渲染。

完成后停止并等待重新验收；不要自行将状态改为 `READY_FOR_REVIEW`。

## 5. 最终复核结果

- `regionTrimFits()` 现按 original 方向校验 trim，打包旋转转置只用于页内 packed 占位；新增 90°/270° 非正方形合法性、转置越界与 trim 越界用例。
- `adoptData()` 所有失败导入用例统一使用非空有效接收对象，并通过 `sameProjectData()` 比较 ProjectData 全部字段的前后快照。
- 已检查对应源码、测试与报告，并运行现有 `build_t5` 测试程序：67/67 通过，退出码 0。干净重建及 CTest 结果由 `Agent_Task/Task2_Report.md` 记录。
- 结论：Back Task 验收通过并关闭；Task 2 完成。
