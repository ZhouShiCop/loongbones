# tools/binary-read-smoke/：Spine 3.2.01 二进制 Reader 的语料只读扫描（Task 4 B4）
#
# 需要本地语料根（$LOONGBONES_CORPUS_ROOT 或命令行参数）；仓库不含语料，
# 故默认**不注册到 CTest**，仅在本地有语料时手工运行（见同目录 README.md）。

option(LOONGBONES_BUILD_BINARY_READ_SMOKE "Build the Spine binary reader corpus smoke" OFF)
if(NOT LOONGBONES_BUILD_BINARY_READ_SMOKE)
  return()
endif()

add_executable(loongbones_binary_read_smoke src/main.cpp)
target_link_libraries(loongbones_binary_read_smoke PRIVATE loongbones_spine32_atlas)
target_compile_features(loongbones_binary_read_smoke PRIVATE cxx_std_17)
set_target_properties(loongbones_binary_read_smoke PROPERTIES
  OUTPUT_NAME loongbones_binary_read_smoke
)
