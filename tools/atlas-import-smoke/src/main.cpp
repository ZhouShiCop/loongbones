#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "loongbones/importers/spine32/atlas_import.hpp"
#include "loongbones/format/project_io.hpp"

namespace fs = std::filesystem;
using namespace loongbones;
using namespace loongbones::importers::spine32;

namespace {

bool readAtlas(FileSystemResourceResolver& resolver, const std::string& path,
               std::string& text, std::string& error) {
  std::vector<std::uint8_t> bytes;
  if (!resolver.read(path, bytes, error)) return false;
  if (bytes.size() > 16U * 1024U * 1024U) { error = "Atlas 超过 16 MiB 安全上限"; return false; }
  text.assign(bytes.begin(), bytes.end());
  return true;
}

void printDiagnostics(const std::vector<AtlasImportDiagnostic>& diagnostics) {
  for (const auto& d : diagnostics) {
    const char* severity = d.severity == AtlasImportSeverity::kError ? "error" : "warning";
    std::cerr << severity << " [" << toString(d.code) << "] " << d.sourceLocation << ": " << d.message << '\n';
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4 || argc > 5) {
    std::cerr << "Usage: loongbones_atlas_import_smoke <authorized-source-root> "
                 "<atlas-relative-path> <project-directory> [--accept-warnings]\n";
    return 2;
  }
  const bool acceptWarnings = argc == 5 && std::string(argv[4]) == "--accept-warnings";
  if (argc == 5 && !acceptWarnings) {
    std::cerr << "Unknown option: " << argv[4] << '\n';
    return 2;
  }

  FileSystemResourceResolver resolver(argv[1]);
  if (!resolver.valid()) { std::cerr << "Invalid authorized source root\n"; return 2; }
  std::string atlasText, error;
  if (!readAtlas(resolver, argv[2], atlasText, error)) {
    std::cerr << "Cannot read Atlas: " << error << '\n'; return 2;
  }

  const fs::path projectDir(argv[3]);
  format::ProjectDocument current;
  const fs::path projectFile = projectDir / format::kProjectFileName;
  if (fs::exists(projectFile)) {
    const auto opened = format::openProject(projectDir.string(), format::ReaderLimits{});
    if (!opened.ok) {
      std::cerr << "Cannot open project: " << opened.formatDiag.str() << '\n';
      return 2;
    }
    current = opened.document;
  } else {
    std::string projectName = fs::path(argv[2]).stem().string();
    if (projectName.empty()) projectName = "Imported Atlas";
    current = format::createProjectDocument(std::move(projectName));
  }

  RandomIdFactory ids;
  const AtlasImportDraft draft = prepareAtlasImport(current, atlasText, argv[2], resolver, ids);
  if (!draft.valid) {
    printDiagnostics(draft.diagnostics);
    return 1;
  }
  std::cout << "Atlas candidate: pages=" << draft.pageCount << " regions=" << draft.regionCount
            << " textures=" << draft.textures.size() << '\n';
  printDiagnostics(draft.diagnostics);

  if (!acceptWarnings && !draft.diagnostics.empty()) {
    std::cerr << "No files written. Review warnings, then rerun with --accept-warnings to commit.\n";
    return 3;
  }

  std::error_code ec;
  fs::create_directories(projectDir, ec);
  if (ec) { std::cerr << "Cannot create project directory: " << ec.message() << '\n'; return 1; }
  DirectoryResourceStaging staging(projectDir.string());
  if (!staging.valid()) { std::cerr << "Cannot initialize project resource staging\n"; return 1; }
  auto diagnostics = commitAtlasImport(draft, projectDir.string(), "0.1.0", acceptWarnings,
                                       staging, current);
  const bool failed = std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& d) {
    return d.severity == AtlasImportSeverity::kError;
  });
  if (failed) { printDiagnostics(diagnostics); return 1; }
  std::cout << "Saved Loongbones project: " << (projectDir / format::kProjectFileName).string() << '\n';
  return 0;
}
