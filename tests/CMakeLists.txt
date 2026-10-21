# 核心模型单元测试。零第三方依赖: 自带极简测试框架（见 tools/core-tests/README.md）。
add_executable(loongbones_core_tests
  domain/test_main.cpp
  domain/test_framework.cpp
  domain/test_ids.cpp
  domain/test_model.cpp
  domain/test_operations.cpp
  domain/test_validation.cpp
  domain/test_atomicity.cpp
domain/test_parent_type.cpp
domain/test_scene_atlas.cpp
domain/test_skeleton.cpp
)
target_link_libraries(loongbones_core_tests PRIVATE loongbones_domain)
target_compile_features(loongbones_core_tests PRIVATE cxx_std_17)
add_test(NAME core_tests COMMAND loongbones_core_tests)