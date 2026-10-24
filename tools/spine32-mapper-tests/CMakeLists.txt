add_executable(loongbones_spine32_mapper_tests
  src/main.cpp
  ${CMAKE_SOURCE_DIR}/tests/domain/test_framework.cpp
  ${CMAKE_SOURCE_DIR}/tests/importers/spine32/test_atlas_text_reader.cpp
  ${CMAKE_SOURCE_DIR}/tests/importers/spine32/test_mapper.cpp
)
target_include_directories(loongbones_spine32_mapper_tests PRIVATE ${CMAKE_SOURCE_DIR})
# Task 5 B3：合成 fixture 目录在构建时注入（产品测试直接加载字节）。
target_compile_definitions(loongbones_spine32_mapper_tests PRIVATE
  LOONGBONES_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/docs/skeleton/fixtures")
target_link_libraries(loongbones_spine32_mapper_tests PRIVATE loongbones_spine32_atlas)
target_compile_features(loongbones_spine32_mapper_tests PRIVATE cxx_std_17)
add_test(NAME spine32_mapper_tests COMMAND loongbones_spine32_mapper_tests)
