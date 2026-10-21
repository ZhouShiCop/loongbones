# tools/format-tests/：原生项目格式 v1 codec 的自动化测试（Task 3 阶段 B）
#
# 复用 tests/domain/test_framework.hpp（零第三方依赖的极简框架）。
# 本目录**不直接使用** RapidJSON —— 被测的是 format/ 的公开 API。

option(LOONGBONES_BUILD_FORMAT_TESTS "Build the native-project-v1 format tests" ON)
if(NOT LOONGBONES_BUILD_FORMAT_TESTS)
  return()
endif()

add_executable(loongbones_format_tests
  src/main.cpp
  ${CMAKE_SOURCE_DIR}/tests/format/test_v2_skeleton.cpp
  ${CMAKE_SOURCE_DIR}/tests/domain/test_framework.cpp
)

# 测试框架的头文件按仓库根解析：#include "tests/domain/test_framework.hpp"
target_include_directories(loongbones_format_tests PRIVATE
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(loongbones_format_tests PRIVATE loongbones_format)

set_target_properties(loongbones_format_tests PROPERTIES
  OUTPUT_NAME loongbones_format_tests
)

add_test(NAME format_tests COMMAND loongbones_format_tests)
