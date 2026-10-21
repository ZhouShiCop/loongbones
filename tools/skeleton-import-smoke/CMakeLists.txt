# tools/skeleton-import-smoke/：B6 的**最小宿主**（语料级端到端闭合）。
#
# 跑通「注册表 → prepare → 事务提交 → 重开验证」：真的写出资源文件与
# project.json，再重开工程逐项比对对象数。
#
# 需要本地语料根（$LOONGBONES_CORPUS_ROOT 或命令行参数）；仓库不含语料，
# 故默认**不构建、不注册到 CTest**，仅在本地有语料时显式打开并手工运行
# （见同目录 README.md）。

option(LOONGBONES_BUILD_SKELETON_IMPORT_SMOKE "Build the skeleton-import end-to-end smoke" OFF)
if(NOT LOONGBONES_BUILD_SKELETON_IMPORT_SMOKE)
  return()
endif()

add_executable(loongbones_skeleton_import_smoke src/main.cpp)
target_link_libraries(loongbones_skeleton_import_smoke PRIVATE loongbones_spine32_atlas)
target_compile_features(loongbones_skeleton_import_smoke PRIVATE cxx_std_17)
set_target_properties(loongbones_skeleton_import_smoke PROPERTIES
  OUTPUT_NAME loongbones_skeleton_import_smoke
)
