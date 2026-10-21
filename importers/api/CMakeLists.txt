# importers/api/：导入插件契约与宿主服务抽象（Task 4 B6）
#
# 只依赖 domain（值类型）与 format（ProjectDocument）；**不依赖**任何具体
# 插件实现、文件系统或 GUI。插件实现与宿主实现反向依赖本库。

add_library(loongbones_importers_api STATIC
  src/import_plugin.cpp
  src/path_util.cpp
  src/sha256.cpp
)

target_include_directories(loongbones_importers_api PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(loongbones_importers_api PUBLIC loongbones_format)
target_compile_features(loongbones_importers_api PUBLIC cxx_std_17)
set_target_properties(loongbones_importers_api PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME loongbones_importers_api
)
