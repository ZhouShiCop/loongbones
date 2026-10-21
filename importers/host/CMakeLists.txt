# importers/host/：静态插件宿主（Task 4 B6）
#
# 持有注册表与宿主侧资源服务（只读解析 / 资源写入事务）。
# 只依赖 `api/`。内置插件的注册入口由插件侧提供定义，避免「宿主 → 插件」反向依赖。

add_library(loongbones_importers_host STATIC
  src/import_host.cpp
)

target_include_directories(loongbones_importers_host PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(loongbones_importers_host PUBLIC loongbones_importers_api)
target_compile_features(loongbones_importers_host PUBLIC cxx_std_17)
set_target_properties(loongbones_importers_host PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME loongbones_importers_host
)
