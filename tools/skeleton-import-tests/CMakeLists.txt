add_executable(loongbones_skeleton_import_tests
  src/main.cpp
  ${CMAKE_SOURCE_DIR}/tests/domain/test_framework.cpp
  ${CMAKE_SOURCE_DIR}/tests/importers/test_import_host.cpp
  ${CMAKE_SOURCE_DIR}/tests/importers/test_spine32_plugin.cpp
)
target_include_directories(loongbones_skeleton_import_tests PRIVATE ${CMAKE_SOURCE_DIR})
# Task 5 B4: synthetic fixture dir injected at build time (plugin-level end-to-end tests
# load the real .skel.bytes / .atlas.txt bytes; no dependence on the current directory).
target_compile_definitions(loongbones_skeleton_import_tests PRIVATE
  LOONGBONES_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/docs/skeleton/fixtures")
target_link_libraries(loongbones_skeleton_import_tests PRIVATE loongbones_spine32_atlas)
target_compile_features(loongbones_skeleton_import_tests PRIVATE cxx_std_17)
add_test(NAME skeleton_import_tests COMMAND loongbones_skeleton_import_tests)
