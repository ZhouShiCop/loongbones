# domain: 稳定数据模型。**不得依赖 Qt、JSON 库、文件系统或 GUI**（规范 3.1 / 8.3）。
add_library(loongbones_domain STATIC
  src/ids.cpp
  src/diagnostics.cpp
  src/transform.cpp
  src/project.cpp
  src/skeleton.cpp
)

target_include_directories(loongbones_domain PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(loongbones_domain PUBLIC cxx_std_17)

set_target_properties(loongbones_domain PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME loongbones_domain
)