# tools/skeleton-import-tests

Task 4 **B6** 的自动化测试：静态插件宿主事务 + Spine 3.2.01 插件集成。

覆盖 `tests/importers/test_import_host.cpp`（10 项）与
`tests/importers/test_spine32_plugin.cpp`（15 项），共 25 项。

## 运行

```bash
cmake --build <build> --target loongbones_skeleton_import_tests
ctest --test-dir <build> -R skeleton_import_tests --output-on-failure
```

## 输入

无外部输入 —— 全部是**内存里的合成 fixture**（合成 `.skel.bytes` 字节、
合成 `.atlas.txt` 文本、合成 PNG 头）。不读真实语料
（语料级验证见 `tools/skeleton-import-smoke/`）。

## 断言要点

### 宿主事务（`test_import_host.cpp`）

| 断言 | 期望 |
|---|---|
| 注册表精确匹配 | `spine.binary@3.2.01` 命中；版本/格式不符返回 `nullptr` |
| 注册 `nullptr` | 被忽略，不计入 descriptors |
| 无效草案 | `committed == false`，不 stage、不落盘 |
| warning 未确认 | `IMPORT_WARNINGS_NOT_ACCEPTED`，不写任何文件 |
| 暂存失败 | `IMPORT_STAGE_FAILED`，rollback 调用 1 次，无残留文件 |
| 发布失败 | `IMPORT_STAGE_FAILED`，`complete()` 未被调用 |
| 保存失败 | `IMPORT_PROJECT_SAVE_FAILED`，回滚资源，旧 `project.json` 仍可读 |
| 授权根 | `..` / 绝对路径 / 空路径 一律拒绝；根内相对路径可读 |

### 插件集成（`test_spine32_plugin.cpp`）

| 断言 | 期望 |
|---|---|
| descriptor | `loongbones.spine32` / `spine.binary` / `3.2.01` |
| `canImport` | 格式与版本都精确匹配才为真 |
| 内置注册 | `registerBuiltinPlugins()` 后可被注册表找到 |
| 最小单位 | 1 骨 / 1 槽 / 1 皮肤 / 1 region 附件；草案有效 |
| prepare 不改 current | 提交前后 current 的对象计数逐项一致 |
| 查找键 | 无 `path` 时用附件名命中 |
| 尾随空格 | 降级命中 → `IMPORT_ATTACHMENT_REGION_DEGRADED`（warning，1 条） |
| 未命中 | `IMPORT_UNRESOLVED_REFERENCE`（error），草案无效 |
| 缺 `.atlas.txt` | `IMPORT_RESOURCE_MISSING` |
| 缺 PNG | `IMPORT_RESOURCE_MISSING` |
| 非法 PNG | `IMPORT_RESOURCE_INVALID` |
| 取消 | `IMPORT_CANCELLED`，草案无效 |
| 越权源路径 | `IMPORT_SOURCE_REJECTED` |
| `contentHash` | 等于宿主对 `.skel.bytes` 全文计算的 SHA-256 |
| 端到端 | 提交后资源文件落盘、`project.json` 存在；**重开工程**对象数与提交前一致 |

## 纪律

- 只用 `tests/domain/test_framework.hpp`，零第三方测试库。
- 不写真实语料、不复制任何输入；临时目录用完即删。
