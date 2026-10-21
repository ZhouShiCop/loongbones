# core-tests — 核心数据模型单元测试

Task 1（Core Asset Model）与 Task 2（Scene / Atlas / Sprite）的自动化单元测试，含 Back Task 返修回归。**零第三方依赖**：
自带极简测试框架（`tests/domain/test_framework.hpp`）。
需求只有断言 + 统计 + 分组，引入 Catch2/GoogleTest 不划算
（规范 §8.3 要求新增第三方依赖前先论证必要性、许可证与替代方案）。

## 运行

```bash
cmake -S Loongbones -B Loongbones/build -G "MinGW Makefiles"
cmake --build Loongbones/build
./Loongbones/build/tests/loongbones_core_tests.exe
```

或经 CTest：`cd Loongbones/build && ctest --output-on-failure`
> Windows 上直接运行 exe 需要 MinGW 运行时 DLL 在 PATH
> （`libstdc++-6.dll` / `libgcc_s_seh-1.dll` / `libwinpthread-1.dll`），
> 否则报 `0xC0000135`。先执行：
> `set PATH=D:\Works\Spine\.devtools\mingw64\bin;%PATH%`

## 覆盖范围（对应 Agent_Task/Task1.md §7）

| 文件 | 覆盖 |
|---|---|
| `test_ids.cpp` | ID 合法/唯一、非法格式拒绝、ULID 编解码往返、种类区分 |
| `test_model.cpp` | 空工程与根、分组嵌套、资源与节点分离、变换与可见性 |
| `test_operations.cpp` | 移动、顺序调整、删除策略、根保护、引用保护 |
| `test_validation.cpp` | 重复/非法 ID、NaN/Inf、类型不一致、环、孤儿 |
| `test_atomicity.cpp` | 失败写操作不留半完成状态 |
| `test_parent_type.cpp` | 普通节点挂载、根不可变及失败原子性 |
| `test_scene_atlas.cpp` | Scene/root 单场景契约闭合、Atlas 引用链与全局 ID 唯一性、Region 边界/trim 跨 Atlas 校验、Sprite→Region 公开路径、绘制顺序（drawOrder / drawableOrder） |

当前共 **67 个用例**，分布在 7 个领域测试文件中。最新干净构建 `build_t5` 的测试程序与 CTest 均通过（67/67、CTest 1/1、0 编译警告、退出码 0）。

已闭合此前验收退回的两项：①非正方形旋转 Region 的 trim 尺寸方向——修正为按 original 方向判定（不随打包旋转转置，转置仅作用于页内占位），并补充 90°/270° 旋转用例；②`adoptData()` 失败路径——接收对象改为「已持有有效数据」，并对 ProjectData 全部字段做前后完整深比较，而非只查状态或单字段。
