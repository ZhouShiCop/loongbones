# format-probe — 原生格式阶段 A 探针

本工具验证 `docs/format/native-project-v1-rfc.md` 与配套 fixture 的可判定性。它是**只读探针**：严格解析 JSON 并检查 RFC v1 结构/领域不变量；不产生 `ProjectData`、不写 JSON、不修改输入，也不是 Task 3 阶段 B 的正式 codec。

## 构建与运行

从项目根目录运行（Windows + MinGW 示例）：

```powershell
cmake -S . -B build_t6 -G "MinGW Makefiles"
cmake --build build_t6 --target loongbones_format_probe
.\build_t6\tools\format-probe\loongbones_format_probe.exe --selftest .\docs\format\fixtures
```

若用 CTest 执行探针自检：

```powershell
ctest --test-dir build_t6 -R format_probe_selftest --output-on-failure
```

也可传入一个或多个 JSON 文件进行只读检查：

```powershell
.\build_t6\tools\format-probe\loongbones_format_probe.exe .\docs\format\fixtures\valid\minimal.json
```

完整通过时退出码为 0；有非法文件或自检失败时退出码非 0，并输出诊断码、JSON Pointer/字节偏移和说明。`--selftest` 会遍历合法与非法 fixture，检查每个文件的预期结果。

## 约束与限制

- C++17、Qt-free、零第三方依赖；由 `tools/format-probe/CMakeLists.txt` 加入顶层 CMake/CTest。
- 不访问 `D:\Game\Other\sdorica` 或任何外部资源；fixture 均为合成数据。
- 当前实现只用于阶段 A RFC 探针。阶段 B 可选择经批准的 JSON 库/实现方式，不得因复用探针代码而默认冻结其 API 或数据结构。
- 修改或扩展 fixture 时同步维护 `docs/format/native-project-v1-rfc.md` 的 fixture 表与测试预期。
