#include "loongbones/format/project_io.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace loongbones {
namespace format {
namespace fs = std::filesystem;
namespace {

bool readWholeFile(const fs::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (in.bad()) return false;
  out = buffer.str();
  return true;
}

// 原子替换。
//
// 同一卷内 rename 是原子的：**不存在**"目标已被删除但新名尚未就位"的窗口。
// Task 3 §4.1 明确禁止用「先删除旧文件再重命名」冒充原子替换。
bool atomicReplace(const fs::path& temporary, const fs::path& target) {
#ifdef _WIN32
  // MOVEFILE_REPLACE_EXISTING：目标存在则替换，由单次 rename 完成
  // MOVEFILE_WRITE_THROUGH：替换返回前不落地磁盘缓存
  return ::MoveFileExW(temporary.c_str(), target.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code ec;
  fs::rename(temporary, target, ec);
  return !ec;
#endif
}

FormatDiagnostic ioFailure(FormatDiagnosticCode code, std::string message) {
  return FormatDiagnostic(code, std::move(message), "");
}

// 写入 + 回读校验 + 原子替换。返回 kNone 表示成功。
FormatDiagnostic writeAtomic(const fs::path& dir, const std::string& text) {
  const fs::path target = dir / kProjectFileName;

  // 临时文件必须与目标**同目录**：跨卷 rename 不是原子操作
  const std::string temporaryName =
      std::string(kProjectFileName) + ".tmp-" + Id::generate(IdKind::Project).str();
  const fs::path temporary = dir / temporaryName;
  std::error_code ec;

  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) return ioFailure(FormatDiagnosticCode::kFormatWriteFailed, "无法创建临时文件");
    out << text;
    out.flush();
    if (!out) {
      out.close();
      fs::remove(temporary, ec);
      return ioFailure(FormatDiagnosticCode::kFormatWriteFailed, "写入临时文件失败");
    }
  }

  // 完整写入与校验：读回逐字节比对
  {
    std::string written;
    if (!readWholeFile(temporary, written) || written != text) {
      fs::remove(temporary, ec);
      return ioFailure(FormatDiagnosticCode::kFormatWriteFailed, "临时文件回读校验失败");
    }
  }

  if (!atomicReplace(temporary, target)) {
    fs::remove(temporary, ec);
    return ioFailure(FormatDiagnosticCode::kFormatWriteFailed, "原子替换失败");
  }
  return FormatDiagnostic();  // kNone = 成功
}

}  // namespace

IoResult openProject(const std::string& dir, const ReaderLimits& limits) {
  IoResult out;
  std::string text;
  if (!readWholeFile(fs::path(dir) / kProjectFileName, text)) {
    out.source = DiagSource::kFormat;
    out.formatDiag =
        ioFailure(FormatDiagnosticCode::kFormatReadFailed, "无法读取 " + std::string(kProjectFileName));
    return out;
  }

  const LoadResult loaded = decodeDocument(text, limits);
  out.ok = loaded.ok;
  out.document = std::move(loaded.document);
  out.source = loaded.source;
  out.formatDiag = loaded.formatDiag;
  out.domainDiags = loaded.domainDiags;
  return out;
}

FormatDiagnostic saveProject(const std::string& dir, const ProjectDocument& doc,
                             const std::string& appVersion) {
  std::error_code ec;
  const fs::path dirPath(dir);
  if (!fs::is_directory(dirPath, ec)) {
    return ioFailure(FormatDiagnosticCode::kFormatWriteFailed, "工程目录不存在");
  }
  return writeAtomic(dirPath, encodeDocument(doc, appVersion));
}

FormatDiagnostic saveProjectAs(const std::string& dir, const ProjectDocument& doc,
                               const std::string& appVersion) {
  std::error_code ec;
  const fs::path dirPath(dir);
  if (!fs::exists(dirPath, ec)) {
    if (!fs::create_directories(dirPath, ec)) {
      return ioFailure(FormatDiagnosticCode::kFormatWriteFailed, "无法创建工程目录");
    }
  }
  if (!fs::is_directory(dirPath, ec)) {
    return ioFailure(FormatDiagnosticCode::kFormatWriteFailed, "工程目录路径不是目录");
  }
  return writeAtomic(dirPath, encodeDocument(doc, appVersion));
}

ProjectDocument createProjectDocument(std::string name) {
  Project project = Project::create(std::move(name));
  ProjectDocument doc;
  doc.data = project.data();
  return doc;
}

}  // namespace format
}  // namespace loongbones
