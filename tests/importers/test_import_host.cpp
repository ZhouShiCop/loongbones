// Task 4 B6：静态插件宿主的自动化断言 —— 注册表匹配与提交事务语义。
//
// 覆盖 B6 第 3/4 条：
//   · prepare() 不改目标工程（草案是完整候选副本）；
//   · 提交顺序 = 暂存资源 → 发布资源 → 原子 saveProject()；
//   · 任一步失败 → 回滚本事务新建的文件，旧 project.json 保持有效。
//
// 输入全部为内存合成，不读真实语料。

#include "loongbones/importers/host/import_host.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "loongbones/domain/project.hpp"
#include "loongbones/format/format_diagnostics.hpp"
#include "loongbones/format/project_io.hpp"
#include "loongbones/importers/spine32/atlas_import.hpp"
#include "tests/domain/test_framework.hpp"

namespace fs = std::filesystem;

namespace {

using namespace loongbones;
using namespace loongbones::importers;

/// 内存中的只读源：授权根相对路径 → 字节。
class FakeResolver final : public api::IResourceResolver {
 public:
  void add(std::string path, std::vector<std::uint8_t> bytes) {
    files_[std::move(path)] = std::move(bytes);
  }
  bool read(std::string_view path, std::vector<std::uint8_t>& bytes,
            std::string& error) const override {
    const auto it = files_.find(std::string(path));
    if (it == files_.end()) {
      error = "no such resource";
      return false;
    }
    bytes = it->second;
    return true;
  }

 private:
  std::map<std::string, std::vector<std::uint8_t>> files_;
};

/// 可注入失败的资源暂存；记录 stage/publish/rollback/complete 调用。
class FakeStaging final : public api::IResourceStaging {
 public:
  enum class Fail { kNone, kStage, kPublish };

  explicit FakeStaging(Fail fail = Fail::kNone) : fail_(fail) {}

  bool stage(std::string_view path, const std::vector<std::uint8_t>& bytes,
             std::string& error) override {
    if (fail_ == Fail::kStage) {
      error = "injected stage failure";
      return false;
    }
    staged_.push_back({std::string(path), bytes, false});
    return true;
  }
  bool publish(std::string& error) override {
    if (fail_ == Fail::kPublish) {
      error = "injected publish failure";
      rollback();
      return false;
    }
    for (Entry& e : staged_) e.published = true;
    return true;
  }
  void rollback() noexcept override {
    ++rollbackCalls_;
    staged_.clear();
  }
  void complete() noexcept override { ++completeCalls_; }

  std::size_t stagedCount() const { return staged_.size(); }
  std::size_t rollbackCalls() const { return rollbackCalls_; }
  std::size_t completeCalls() const { return completeCalls_; }

 private:
  struct Entry {
    std::string path;
    std::vector<std::uint8_t> bytes;
    bool published = false;
  };
  Fail fail_;
  std::vector<Entry> staged_;
  std::size_t rollbackCalls_ = 0;
  std::size_t completeCalls_ = 0;
};

bool hasCode(const std::vector<api::ImportDiagnostic>& diags, api::ImportCode code) {
  for (const api::ImportDiagnostic& d : diags) if (d.code == code) return true;
  return false;
}

api::ImportDraft invalidDraft() {
  api::ImportDraft draft;
  draft.descriptor = api::ImportDescriptor{"test.plugin", "test.format", "1"};
  draft.valid = false;
  return draft;
}

}  // namespace

// ---- 注册表 ----

TEST(ImportHost, RegistryMatchesFormatIdAndVersionExactly) {
  auto& reg = host::ImportRegistry::instance();
  reg.clear();
  CHECK(reg.find("spine.binary", "3.2.01") == nullptr);

  host::registerBuiltinPlugins(reg);

  const api::IImportPlugin* plugin = reg.find("spine.binary", "3.2.01");
  CHECK(plugin != nullptr);
  CHECK_EQ(plugin->descriptor().pluginId, std::string{"loongbones.spine32"});
  CHECK_EQ(plugin->descriptor().formatId, std::string{"spine.binary"});
  CHECK_EQ(plugin->descriptor().formatVersion, std::string{"3.2.01"});

  // 精确匹配：版本不同、格式不同都必须失配。
  CHECK(reg.find("spine.binary", "3.8.99") == nullptr);
  CHECK(reg.find("spine.json", "3.2.01") == nullptr);
  CHECK_EQ(reg.descriptors().size(), std::size_t{1});

  reg.clear();
  CHECK(reg.find("spine.binary", "3.2.01") == nullptr);
}

TEST(ImportHost, NullPluginIsIgnored) {
  auto& reg = host::ImportRegistry::instance();
  reg.clear();
  reg.registerPlugin(nullptr);
  CHECK_EQ(reg.descriptors().size(), std::size_t{0});
  reg.clear();
}

// ---- 提交闸门 ----

TEST(ImportHost, InvalidDraftCommitsNothing) {
  auto& reg = host::ImportRegistry::instance();
  reg.clear();
  const fs::path dir = fs::temp_directory_path() / "lb_host_invalid";
  fs::remove_all(dir);

  api::ImportDraft draft = invalidDraft();
  draft.resources.push_back({"assets/textures/x.png", {1, 2, 3}});

  FakeStaging staging;
  format::ProjectDocument doc = format::createProjectDocument("p");
  const auto outcome = host::commitImport(draft, {dir.string(), "dev", true}, staging, doc);
  CHECK(!outcome.committed);
  CHECK_EQ(staging.stagedCount(), std::size_t{0});
  CHECK(!fs::exists(dir / "assets" / "textures" / "x.png"));

  reg.clear();
  fs::remove_all(dir);
}

TEST(ImportHost, UnacceptedWarningsWriteNothing) {
  const fs::path dir = fs::temp_directory_path() / "lb_host_warn";
  fs::remove_all(dir);
  fs::create_directories(dir);

  api::ImportDraft draft;
  draft.descriptor = api::ImportDescriptor{"test.plugin", "test.format", "1"};
  draft.valid = true;
  draft.report.diagnostics.push_back(
      {api::ImportCode::kUnsupportedCapability, api::ImportSeverity::kWarning, "有不支持的能力",
       "animations"});
  draft.resources.push_back({"assets/textures/w.png", {4, 5}});

  FakeStaging staging;
  format::ProjectDocument doc = format::createProjectDocument("p");
  const auto outcome = host::commitImport(draft, {dir.string(), "dev", false}, staging, doc);
  CHECK(!outcome.committed);
  CHECK(hasCode(outcome.diagnostics, api::ImportCode::kWarningsNotAccepted));
  CHECK_EQ(staging.stagedCount(), std::size_t{0});
  CHECK(!fs::exists(dir / "assets" / "textures" / "w.png"));

  // 确认后即可提交。
  const auto accepted = host::commitImport(draft, {dir.string(), "dev", true}, staging, doc);
  CHECK(accepted.committed);
  CHECK_EQ(staging.completeCalls(), std::size_t{1});

  fs::remove_all(dir);
}

TEST(ImportHost, StagingFailureRollsBackAndLeavesNoFile) {
  const fs::path dir = fs::temp_directory_path() / "lb_host_stagefail";
  fs::remove_all(dir);

  api::ImportDraft draft;
  draft.descriptor = api::ImportDescriptor{"test.plugin", "test.format", "1"};
  draft.valid = true;
  draft.resources.push_back({"assets/textures/s.png", {6}});

  FakeStaging staging(FakeStaging::Fail::kStage);
  format::ProjectDocument doc = format::createProjectDocument("p");
  const auto outcome = host::commitImport(draft, {dir.string(), "dev", true}, staging, doc);
  CHECK(!outcome.committed);
  CHECK(hasCode(outcome.diagnostics, api::ImportCode::kResourceStageFailed));
  CHECK_EQ(staging.rollbackCalls(), std::size_t{1});
  CHECK_EQ(staging.completeCalls(), std::size_t{0});
  CHECK(!fs::exists(dir / "assets" / "textures" / "s.png"));

  fs::remove_all(dir);
}

TEST(ImportHost, PublishFailureRollsBackAndLeavesNoFile) {
  const fs::path dir = fs::temp_directory_path() / "lb_host_pubfail";
  fs::remove_all(dir);

  api::ImportDraft draft;
  draft.descriptor = api::ImportDescriptor{"test.plugin", "test.format", "1"};
  draft.valid = true;
  draft.resources.push_back({"assets/textures/p.png", {7}});

  FakeStaging staging(FakeStaging::Fail::kPublish);
  format::ProjectDocument doc = format::createProjectDocument("p");
  const auto outcome = host::commitImport(draft, {dir.string(), "dev", true}, staging, doc);
  CHECK(!outcome.committed);
  CHECK(hasCode(outcome.diagnostics, api::ImportCode::kResourceStageFailed));
  CHECK_EQ(staging.completeCalls(), std::size_t{0});
  CHECK(!fs::exists(dir / "assets" / "textures" / "p.png"));

  fs::remove_all(dir);
}

// ---- 真实落盘事务（用 spine32 侧的 DirectoryResourceStaging）----

TEST(ImportHost, CommitPublishesResourcesAndReplacesProjectJson) {
  const fs::path dir = fs::temp_directory_path() / "lb_host_commit";
  fs::remove_all(dir);
  fs::create_directories(dir);

  // 先存一份「旧工程」，用于验证失败回滚时它保持有效。
  const format::FormatDiagnostic saved =
      format::saveProject(dir.string(), format::createProjectDocument("old"), "dev");
  CHECK_EQ(static_cast<int>(saved.code), static_cast<int>(format::FormatDiagnosticCode::kNone));
  CHECK(fs::exists(dir / format::kProjectFileName));

  api::ImportDraft draft;
  draft.descriptor = api::ImportDescriptor{"test.plugin", "test.format", "1"};
  draft.document = format::createProjectDocument("candidate");
  draft.document.sourceVersion = format::SourceFormatVersion::kV2;
  draft.resources.push_back({"assets/textures/a.png", {1, 2, 3}});
  draft.resources.push_back({"assets/textures/b.png", {4, 5, 6, 7}});
  draft.valid = true;

  spine32::DirectoryResourceStaging staging(dir.string());
  format::ProjectDocument doc = format::createProjectDocument("p");
  const auto outcome = host::commitImport(draft, {dir.string(), "dev", true}, staging, doc);

  CHECK(outcome.committed);
  CHECK(fs::exists(dir / "assets" / "textures" / "a.png"));
  CHECK(fs::exists(dir / "assets" / "textures" / "b.png"));

  // 提交后宿主文档应等于候选文档。
  CHECK_EQ(doc.data.name, std::string{"candidate"});

  // 没有残留 .stage-* 临时文件。
  for (const auto& entry : fs::recursive_directory_iterator(dir)) {
    CHECK(entry.path().filename().string().find(".stage-") == std::string::npos);
  }
  fs::remove_all(dir);
}

TEST(ImportHost, SaveFailureRollsBackResources) {
  const fs::path dir = fs::temp_directory_path() / "lb_host_savefail";
  fs::remove_all(dir);
  fs::create_directories(dir);

  api::ImportDraft draft;
  draft.descriptor = api::ImportDescriptor{"test.plugin", "test.format", "1"};
  draft.document = format::createProjectDocument("candidate");
  draft.resources.push_back({"assets/textures/f.png", {1}});
  draft.valid = true;

  // 把 project.json 换成目录 —— saveProject 无法原子替换，必然失败。
  fs::create_directories(dir / format::kProjectFileName);

  spine32::DirectoryResourceStaging staging(dir.string());
  format::ProjectDocument doc = format::createProjectDocument("p");
  const auto outcome = host::commitImport(draft, {dir.string(), "dev", true}, staging, doc);

  CHECK(!outcome.committed);
  CHECK(hasCode(outcome.diagnostics, api::ImportCode::kProjectSaveFailed));
  // 回滚后不得留下资源文件。
  CHECK(!fs::exists(dir / "assets" / "textures" / "f.png"));

  fs::remove_all(dir);
}

// ---- 授权根 ----

TEST(ImportHost, ResolverRejectsEscapingAndAbsolutePaths) {
  const fs::path root = fs::temp_directory_path() / "lb_host_resolver";
  fs::remove_all(root);
  fs::create_directories(root / "unit");
  std::ofstream((root / "unit" / "u.skel.bytes").string(), std::ios::binary).close();
  std::ofstream((root / "unit" / "u.png").string(), std::ios::binary).close();

  spine32::FileSystemResourceResolver resolver(root.string());
  CHECK(resolver.valid());

  std::vector<std::uint8_t> bytes;
  std::string error;

  // 合法：授权根内的相对路径。
  CHECK(resolver.read("unit/u.png", bytes, error));

  // 越权：.. 与绝对路径
  CHECK(!resolver.read("../outside.png", bytes, error));
  CHECK(!resolver.read("/absolute.png", bytes, error));
  CHECK(!resolver.read("unit/../../escape.png", bytes, error));
  CHECK(!resolver.read("", bytes, error));

  fs::remove_all(root);
}

TEST(ImportHost, RollbackKeepsPreExistingFiles) {
  const fs::path dir = fs::temp_directory_path() / "lb_host_keep";
  fs::remove_all(dir);
  fs::create_directories(dir / "assets" / "textures");
  {
    std::ofstream out((dir / "assets" / "textures" / "keep.png").string(), std::ios::binary);
    out << "existing";
  }

  api::ImportDraft draft;
  draft.descriptor = api::ImportDescriptor{"test.plugin", "test.format", "1"};
  draft.document = format::createProjectDocument("candidate");
  // 与既有文件同名 → publish 应拒绝覆盖并回滚。
  draft.resources.push_back({"assets/textures/keep.png", {9, 9}});
  draft.valid = true;

  spine32::DirectoryResourceStaging staging(dir.string());
  format::ProjectDocument doc = format::createProjectDocument("p");
  const auto outcome = host::commitImport(draft, {dir.string(), "dev", true}, staging, doc);
  CHECK(!outcome.committed);
  CHECK(hasCode(outcome.diagnostics, api::ImportCode::kResourceStageFailed));

  // 既有文件**必须**还在，且内容未被改写。
  CHECK(fs::exists(dir / "assets" / "textures" / "keep.png"));
  std::string content;
  {
    std::ifstream in((dir / "assets" / "textures" / "keep.png").string(), std::ios::binary);
    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  CHECK_EQ(content, std::string{"existing"});

  fs::remove_all(dir);
}
