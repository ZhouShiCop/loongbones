# tools/spine32-mapper-smoke/：B5 Mapper 的语料只读扫描（Reader → Atlas → Mapper）
#
# 需要本地语料根（$LOONGBONES_CORPUS_ROOT 或命令行参数）；仓库不含语料，
# 故默认**不注册到 CTest**，仅在本地有语料时手工运行（见同目录 README.md）。

option(LOONGBONES_BUILD_SPINE32_MAPPER_SMOKE "Build the Spine32 mapper corpus smoke" OFF)
if(NOT LOONGBONES_BUILD_SPINE32_MAPPER_SMOKE)
  return()
endif()

add_executable(loongbones_spine32_mapper_smoke src/main.cpp)
target_link_libraries(loongbones_spine32_mapper_smoke PRIVATE loongbones_spine32_atlas)
target_compile_features(loongbones_spine32_mapper_smoke PRIVATE cxx_std_17)
set_target_properties(loongbones_spine32_mapper_smoke PROPERTIES
  OUTPUT_NAME loongbones_spine32_mapper_smoke
)
