# tools/binary-read-tests/：Spine 3.2.01 二进制 Reader 的自动化测试（Task 4 B4）
#
# 复用 tests/domain/test_framework.hpp（零第三方依赖的极简框架）。
# 被测的是 importers/spine32 的 Reader 公开 API，不读真实语料（语料扫描由
# tools/binary-read-smoke/ 承担，需要本地语料根，不进 CTest 默认集合）。

add_executable(loongbones_binary_read_tests
  src/main.cpp
  ${CMAKE_SOURCE_DIR}/tests/domain/test_framework.cpp
  ${CMAKE_SOURCE_DIR}/tests/importers/spine32/test_binary_reader.cpp
)
target_include_directories(loongbones_binary_read_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(loongbones_binary_read_tests PRIVATE loongbones_spine32_atlas)
target_compile_features(loongbones_binary_read_tests PRIVATE cxx_std_17)
add_test(NAME binary_read_tests COMMAND loongbones_binary_read_tests)
