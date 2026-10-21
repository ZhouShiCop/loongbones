cmake_minimum_required(VERSION 3.20)
project(Loongbones LANGUAGES CXX)

# 语言标准：C++17（ADR-0001 D1 原则已定案）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# 默认构建类型：单配置生成器下用 Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 测试统一注册到 CTest（tools/core-tests/README.md 有运行说明）
include(CTest)
enable_testing()

# domain/：稳定模型。**不得依赖 Qt、JSON 库、文件系统或 GUI**（规范 §3.1、§8.3）。
add_subdirectory(domain)

# format/：原生项目格式 v1 codec（Task 3 阶段 B，见 docs/format/native-project-v1-rfc.md）。
# 依赖 domain，并使用 vendored 的 RapidJSON v1.1.0（MIT，见 vendor/rapidjson/VENDOR.md）。
add_subdirectory(format)

# importers/: B1 Atlas + PNG-only vertical slice; B4 Spine 3.2.01 binary reader (parse only);
# B5 Atlas Reader/Mapper; B6 静态插件宿主（api/ 定义契约，host/ 实现注册表与事务）。
add_subdirectory(importers/api)
add_subdirectory(importers/host)
add_subdirectory(importers/spine32)
add_subdirectory(tools/atlas-import-smoke)
add_subdirectory(tools/binary-read-tests)
add_subdirectory(tools/binary-read-smoke)
add_subdirectory(tools/spine32-mapper-smoke)
add_subdirectory(tools/skeleton-import-smoke)

# tools/format-tests/：codec 的自动化测试（可用 -DLOONGBONES_BUILD_FORMAT_TESTS=OFF 关闭）
add_subdirectory(tools/format-tests)

# tests/：自动化测试
if(BUILD_TESTING)
  add_subdirectory(tests)
  add_subdirectory(tools/atlas-import-tests)
  add_subdirectory(tools/spine32-mapper-tests)
  add_subdirectory(tools/skeleton-import-tests)
endif()

# tools/format-probe/：原生格式 v1 探针（Task 3 阶段 A，见 docs/format/native-project-v1-rfc.md）
# 只是验证工具，不是工程 codec；可用 -DLOONGBONES_BUILD_FORMAT_PROBE=OFF 关闭。
option(LOONGBONES_BUILD_FORMAT_PROBE "Build the native-project-v1 format probe" ON)
if(LOONGBONES_BUILD_FORMAT_PROBE)
  add_subdirectory(tools/format-probe)
endif()
