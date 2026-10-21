// 目录工程 IO（Task 3 §4.1）
//
// 最小服务：创建工程目录、打开其中 project.json、保存到原目录或另存到新目录。
// 图片/资源复制、资源重定位、自动保存、单文件打包不在本模块范围（Task 3 §0 不支持项）。

#ifndef LOONGBONES_FORMAT_PROJECT_IO_HPP
#define LOONGBONES_FORMAT_PROJECT_IO_HPP

#include <string>
#include <vector>

#include "loongbones/domain/diagnostics.hpp"
#include "loongbones/format/codec.hpp"

namespace loongbones {
namespace format {

/// 文档文件在目录工程中的固定名字（ADR-0002）
constexpr const char* kProjectFileName = "project.json";

/// 打开结果。
struct IoResult {
  bool ok = false;
  ProjectDocument document;
  DiagSource source = DiagSource::kNone;
  FormatDiagnostic formatDiag;
  std::vector<Diagnostic> domainDiags;
};

/// 打开目录工程：读取 <dir>/project.json 并解码 + 校验。
/// 文件不存在或不可读 → kFormatReadFailed。
IoResult openProject(const std::string& dir, const ReaderLimits& limits);

/// 保存到原目录（原子替换）。返回的 FormatDiagnostic 码为 kNone 表示成功。
/// 任何失败路径都**不改变**磁盘上已有的 project.json，并清理临时文件。
FormatDiagnostic saveProject(const std::string& dir, const ProjectDocument& doc,
                             const std::string& appVersion);

/// 另存到新目录（原子替换），目录不存在则创建。
/// 另存失败**不破坏**原工程（写入只发生在新目录）。
FormatDiagnostic saveProjectAs(const std::string& dir, const ProjectDocument& doc,
                               const std::string& appVersion);

/// 创建一个空工程的文档对象（ID 由 Domain 生成，含唯一的 kRoot 场景根）。
ProjectDocument createProjectDocument(std::string name);

}  // namespace format
}  // namespace loongbones

#endif  // LOONGBONES_FORMAT_PROJECT_IO_HPP
