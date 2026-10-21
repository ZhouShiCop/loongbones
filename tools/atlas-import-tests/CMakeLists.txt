add_executable(loongbones_atlas_import_tests
  src/main.cpp
  ${CMAKE_SOURCE_DIR}/tests/domain/test_framework.cpp
  ${CMAKE_SOURCE_DIR}/tests/importers/spine32/test_atlas_import.cpp
)
target_include_directories(loongbones_atlas_import_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(loongbones_atlas_import_tests PRIVATE loongbones_spine32_atlas)
target_compile_features(loongbones_atlas_import_tests PRIVATE cxx_std_17)
add_test(NAME atlas_import_tests COMMAND loongbones_atlas_import_tests)
