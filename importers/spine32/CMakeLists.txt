add_library(loongbones_spine32_atlas STATIC
  src/atlas_import.cpp
  src/atlas_text_reader.cpp
  src/binary_reader.cpp
  src/mapper.cpp
  src/spine32_import_plugin.cpp
  src/builtin_registration.cpp
)

target_include_directories(loongbones_spine32_atlas PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(loongbones_spine32_atlas PUBLIC loongbones_importers_host)
target_compile_features(loongbones_spine32_atlas PUBLIC cxx_std_17)
set_target_properties(loongbones_spine32_atlas PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME loongbones_spine32_atlas
)
