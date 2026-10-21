# format/: 原生项目格式 v1 codec（Task 3 阶段 B）
#
# 依赖方向：format -> domain，**禁止**反向依赖（规范 §3.1）。
# RapidJSON 只出现在 .cpp 实现文件，**不得**进入任何公开头文件（RFC §7.4）。

set(LOONGBONES_FORMAT_SOURCES
  src/format_diagnostics.cpp
  src/json_value.cpp
  src/json_reader.cpp
  src/decode.cpp
  src/encode.cpp
  src/project_io.cpp
)

add_library(loongbones_format STATIC ${LOONGBONES_FORMAT_SOURCES})

target_include_directories(loongbones_format PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(loongbones_format PUBLIC loongbones_domain)

target_compile_features(loongbones_format PUBLIC cxx_std_17)

# vendored 的 RapidJSON v1.1.0（commit f54b0e47...）/ MIT。
# SYSTEM 用于抑制第三方头文件自身的警告，不污染本项目的 -Werror 式严格构建。
target_include_directories(loongbones_format SYSTEM PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/rapidjson/include
)

set_target_properties(loongbones_format PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME loongbones_format
)
