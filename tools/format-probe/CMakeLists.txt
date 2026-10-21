# 格式探针（Task 3 阶段 A）
#
# 验证 docs/format/native-project-v1-rfc.md 描述的约束是可判定的。
# 零第三方依赖、Qt-free；只读解析，不做领域解码、不做写出（那是阶段 B 的 codec）。

add_executable(loongbones_format_probe
  src/json.cpp
  src/main.cpp
)

target_include_directories(loongbones_format_probe PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(loongbones_format_probe PRIVATE cxx_std_17)

# 回归入口：对 docs/format/fixtures 做一键可复现校验
add_test(NAME format_probe_selftest
         COMMAND loongbones_format_probe
                 --selftest ${PROJECT_SOURCE_DIR}/docs/format/fixtures)
