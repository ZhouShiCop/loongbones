// Task 4 B6：语料级**端到端闭合** —— 最小宿主跑通
// 「注册表 → prepare → 事务提交 → 重开验证」。
//
// 与 `tools/spine32-mapper-smoke/` 的分工：
//   · mapper-smoke 只回答「名字能否解析到 region」（不读 PNG、不落盘）；
//   · 本工具把整条链走到底：**真的写出**资源文件与 project.json，
//     再**真的重开**工程，逐项比对对象数。
//
// 守门规则与同级 smoke 工具一致 —— **只有退出码 0 可作验收证据**：
// 根目录给错 / 一个单位都走不完 / 存在 error 级诊断，都必须以非 0 退出，
// 拒绝把"跑完了"误当成"跑通了"。
//
// 只读纪律：语料只读（FileSystemResourceResolver 限死在授权根内），
// 写入只发生在 `--project` 指定的暂存目录（默认系统临时目录下的固定子目录）。

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "loongbones/domain/atlas.hpp"
#include "loongbones/domain/project.hpp"
#include "loongbones/domain/skeleton.hpp"
#include "loongbones/format/format_diagnostics.hpp"
#include "loongbones/format/project_io.hpp"
#include "loongbones/importers/api/import_plugin.hpp"
#include "loongbones/importers/host/import_host.hpp"
#include "loongbones/importers/spine32/atlas_import.hpp"
#include "loongbones/importers/spine32/spine32_import_plugin.hpp"

namespace fs = std::filesystem;

namespace {

using namespace loongbones;
using namespace loongbones::importers;

struct Unit {
  std::string rel;  ///< 授权根相对路径（`a/b.skel.bytes`）
  fs::path skel;
  fs::path atlas;  ///< 插件按「同名替换」推导出的图集路径
};

/// 插件用的图集路径规则：`<stem>.skel.bytes` → `<stem>.atlas.txt`。
/// 与 `spine32_import_plugin.cpp` 里的 `atlasPathFor()` 保持一致；这里重复一份
/// 只为**报告**用（判定某次失败是否属于"共享图集"布局），不参与导入决策。
fs::path atlasPathFor(const fs::path& skel) {
  fs::path atlas = skel;
  atlas.replace_extension("");  // 去掉 .bytes
  atlas.replace_extension("");  // 去掉 .skel
  atlas += ".atlas.txt";
  return atlas;
}

/// 判定某次 prepare 失败是否属于「同目录里**有**图集、但与骨架不同名」的布局。
/// 语料里确实存在这种单位（如 `h0164s5/watch/` 下两个骨架共用
/// `h0164s5_watch.atlas.txt`）；插件只认「同名替换」规则，故必然失败。
/// 这里只做**报告归类**（判断同目录是否存在任意 `.atlas.txt`），
/// 不去猜命名规律、也不改变导入行为。
bool hasSiblingAtlas(const fs::path& skel) {
  const fs::path parent = skel.parent_path();
  std::error_code ec;
  for (fs::directory_iterator it(parent, ec), end; it != end; it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file(ec)) continue;
    const std::string name = it->path().filename().string();
    constexpr std::string_view suffix = ".atlas.txt";
    if (name.size() >= suffix.size() &&
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
      return true;
    }
  }
  return false;
}

/// 收集语料里所有 `.skel.bytes`；以相对路径排序，保证多次运行结果一致。
std::vector<Unit> loadCorpus(const fs::path& root) {
  std::vector<Unit> out;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file(ec)) continue;
    const fs::path& p = it->path();
    const std::string name = p.filename().string();
    constexpr std::string_view suffix = ".skel.bytes";
    if (name.size() < suffix.size() ||
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
      continue;
    }
    Unit u;
    u.skel = p;
    u.atlas = atlasPathFor(p);
    std::error_code relEc;
    u.rel = fs::relative(p, root, relEc).generic_string();
    out.push_back(std::move(u));
  }
  std::sort(out.begin(), out.end(),
            [](const Unit& a, const Unit& b) { return a.rel < b.rel; });
  return out;
}

struct Tallies {
  std::size_t units = 0;
  std::size_t prepared = 0;      ///< prepare 产出 valid 候选
  std::size_t prepareFailed = 0; ///< prepare 未产出 valid 候选
  std::size_t unresolved = 0;    ///< kUnresolvedReference（error）
  std::size_t degraded = 0;      ///< kAttachmentRegionDegraded（warning）
  /// 失败单位中「同目录存在与目录同名的图集」者 —— 共享图集布局，已知缺口。
  std::size_t sharedAtlasUnits = 0;
  std::size_t committed = 0;
  std::size_t stagedFiles = 0;
  /// warning 级诊断样本（不含 unsupported 计数），便于核对"到底警告了什么"。
  std::map<std::string, std::size_t> warningSamples;
  std::size_t bones = 0;
  std::size_t slots = 0;
  std::size_t skins = 0;
  std::size_t skinEntries = 0;
  std::size_t regions = 0;
  std::size_t textures = 0;
  std::map<api::ImportCode, std::size_t> codes;
  std::map<std::string, std::size_t> unsupported;
  std::vector<std::string> errorUnits;
  std::vector<std::string> unresolvedUnits;
};

std::size_t countOf(const std::vector<std::pair<std::string, std::size_t>>& entries,
                    std::string_view key) {
  for (const auto& kv : entries) {
    if (kv.first == key) return kv.second;
  }
  return 0;
}

/// 只数调用次数的进度接收器：本工具不打印进度（1113 个单位会刷屏），
/// 但要确认插件**确实**在按阶段回抛。
class CountingProgress final : public api::IProgressSink {
 public:
  void report(std::string_view stage, std::size_t done, std::size_t total) override {
    ++calls_;
    if (done > total) ++inconsistent_;
    last_ = std::string(stage);
  }
  std::size_t calls() const noexcept { return calls_; }
  std::size_t inconsistent() const noexcept { return inconsistent_; }
  const std::string& lastStage() const noexcept { return last_; }

 private:
  std::size_t calls_ = 0;
  std::size_t inconsistent_ = 0;
  std::string last_;
};

constexpr int kExitOk = 0;
constexpr int kExitNoRoot = 2;
constexpr int kExitNoSkeleton = 3;
constexpr int kExitNothingPrepared = 4;
constexpr int kExitCommitFailed = 5;
constexpr int kExitErrorsPresent = 6;
constexpr int kExitResourceMissing = 7;

// ---- Task 5 B5：真实样本的**逐项**语义比对（不只是对象数）----
// 比对第二批字段：uvs / triangles / vertices / 绑定(boneId,x,y,weight) / parentKey /
// 约束参数。浮点用**残差**记录（native 落盘是 double 文本往返，容差取 1e-9）。

struct DeepDiff {
  std::size_t compared = 0;
  double maxAbsResidual = 0.0;
  std::vector<std::string> mismatches;
};

void noteResidual(double a, double b, DeepDiff& out) {
  ++out.compared;
  const double d = a - b;
  const double r = d < 0 ? -d : d;
  if (r > out.maxAbsResidual) out.maxAbsResidual = r;
  if (r > 1e-9 && out.mismatches.size() < 10) {
    out.mismatches.push_back("浮点残差 " + std::to_string(r) + "（" + std::to_string(a) + " vs " +
                             std::to_string(b) + "）");
  }
}

void expectEqual(bool ok, const std::string& what, DeepDiff& out) {
  ++out.compared;
  if (!ok && out.mismatches.size() < 10) out.mismatches.push_back(what);
}

void compareAttachmentDeep(const Attachment& a, const Attachment& b, const std::string& where,
                           DeepDiff& out) {
  expectEqual(a.index() == b.index(), where + ": 附件类型不同", out);
  if (a.index() != b.index()) return;
  if (const auto* ma = std::get_if<MeshAttachment>(&a)) {
    const auto& mb = std::get<MeshAttachment>(b);
    expectEqual(ma->uvs.size() == mb.uvs.size(), where + ": uvs 长度不同", out);
    expectEqual(ma->triangles == mb.triangles, where + ": triangles 不同", out);
    for (std::size_t i = 0; i < ma->uvs.size() && i < mb.uvs.size(); ++i) {
      noteResidual(ma->uvs[i], mb.uvs[i], out);
    }
    for (std::size_t i = 0; i < ma->vertices.size() && i < mb.vertices.size(); ++i) {
      noteResidual(ma->vertices[i], mb.vertices[i], out);
    }
  } else if (const auto* wa = std::get_if<WeightedMeshAttachment>(&a)) {
    const auto& wb = std::get<WeightedMeshAttachment>(b);
    expectEqual(wa->vertexBindings.size() == wb.vertexBindings.size(),
                where + ": 顶点数不同", out);
    expectEqual(wa->triangles == wb.triangles, where + ": triangles 不同", out);
    for (std::size_t i = 0; i < wa->vertexBindings.size() && i < wb.vertexBindings.size(); ++i) {
      expectEqual(wa->vertexBindings[i].size() == wb.vertexBindings[i].size(),
                  where + ": 顶点 " + std::to_string(i) + " 的绑定数不同", out);
      for (std::size_t j = 0;
           j < wa->vertexBindings[i].size() && j < wb.vertexBindings[i].size(); ++j) {
        const auto& x = wa->vertexBindings[i][j];
        const auto& y = wb.vertexBindings[i][j];
        expectEqual(x.boneId == y.boneId, where + ": 绑定 boneId 不同", out);
        noteResidual(x.x, y.x, out);
        noteResidual(x.y, y.y, out);
        noteResidual(x.weight, y.weight, out);  // 权重**原值**往返（D30）
      }
    }
  } else if (const auto* la = std::get_if<LinkedMeshAttachment>(&a)) {
    const auto& lb = std::get<LinkedMeshAttachment>(b);
    expectEqual(la->parentSlotId == lb.parentSlotId, where + ": parentKey.slotId 不同", out);
    expectEqual(la->parentAttachmentName == lb.parentAttachmentName,
                where + ": parentKey.attachmentName 不同", out);
    expectEqual(la->inheritDeform == lb.inheritDeform, where + ": inheritDeform 不同", out);
  }
}

void compareSkeletonDeep(const Skeleton& a, const Skeleton& b, DeepDiff& out) {
  const std::string where = "skeletons/" + a.id.str();
  expectEqual(a.skins.size() == b.skins.size(), where + ": 皮肤数不同", out);
  for (std::size_t s = 0; s < a.skins.size() && s < b.skins.size(); ++s) {
    const std::string sw = where + "/skins[" + std::to_string(s) + "]";
    expectEqual(a.skins[s].entries.size() == b.skins[s].entries.size(), sw + ": 条目数不同", out);
    for (std::size_t e = 0; e < a.skins[s].entries.size() && e < b.skins[s].entries.size(); ++e) {
      compareAttachmentDeep(a.skins[s].entries[e].attachment, b.skins[s].entries[e].attachment,
                            sw + "/entries[" + std::to_string(e) + "]", out);
    }
  }
  expectEqual(a.ikConstraints.size() == b.ikConstraints.size(), where + ": IK 数不同", out);
  for (std::size_t i = 0; i < a.ikConstraints.size() && i < b.ikConstraints.size(); ++i) {
    const auto& x = a.ikConstraints[i];
    const auto& y = b.ikConstraints[i];
    const std::string w = where + "/ikConstraints[" + std::to_string(i) + "]";
    expectEqual(x.id == y.id, w + ": id 不同", out);
    expectEqual(x.name == y.name, w + ": name 不同", out);
    expectEqual(x.boneIds == y.boneIds, w + ": boneIds 不同", out);
    expectEqual(x.targetBoneId == y.targetBoneId, w + ": targetBoneId 不同", out);
    expectEqual(x.enabled == y.enabled, w + ": enabled 不同（D35）", out);
    expectEqual(x.bendPositive == y.bendPositive, w + ": bendPositive 不同", out);
    noteResidual(x.mix, y.mix, out);
  }
  expectEqual(a.transformConstraints.size() == b.transformConstraints.size(),
              where + ": transform 数不同", out);
  for (std::size_t i = 0;
       i < a.transformConstraints.size() && i < b.transformConstraints.size(); ++i) {
    const auto& x = a.transformConstraints[i];
    const auto& y = b.transformConstraints[i];
    const std::string w = where + "/transformConstraints[" + std::to_string(i) + "]";
    expectEqual(x.id == y.id, w + ": id 不同", out);
    expectEqual(x.boneId == y.boneId, w + ": boneId 不同", out);
    expectEqual(x.targetBoneId == y.targetBoneId, w + ": targetBoneId 不同", out);
    // 4×mix + 6×offset **原值透传**（D36）
    noteResidual(x.rotateMix, y.rotateMix, out);
    noteResidual(x.translateMix, y.translateMix, out);
    noteResidual(x.scaleMix, y.scaleMix, out);
    noteResidual(x.shearMix, y.shearMix, out);
    noteResidual(x.offsetRotation, y.offsetRotation, out);
    noteResidual(x.offsetX, y.offsetX, out);
    noteResidual(x.offsetY, y.offsetY, out);
    noteResidual(x.offsetScaleX, y.offsetScaleX, out);
    noteResidual(x.offsetScaleY, y.offsetScaleY, out);
    noteResidual(x.offsetShearY, y.offsetShearY, out);
  }
}

void printUsage() {
  std::cout << "用法：loongbones_skeleton_import_smoke <语料根目录> [选项]\n"
               "      或设置 $LOONGBONES_CORPUS_ROOT\n"
               "选项：\n"
               "  --project <dir>      导入落地目录（默认临时目录/lb_skeleton_import_smoke）\n"
               "  --limit <n>          完整提交（含重开验证）的单位数上限，默认 8\n"
               "  --reject-warnings    有 warning 时拒绝提交（默认接受并计入报告）\n";
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif

  // ---- 参数 ----
  std::string rootArg;
  fs::path projectDir;
  std::size_t commitLimit = 8;
  bool acceptWarnings = true;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--project" && i + 1 < argc) {
      projectDir = argv[++i];
    } else if (arg == "--limit" && i + 1 < argc) {
      commitLimit = static_cast<std::size_t>(std::stoul(argv[++i]));
    } else if (arg == "--reject-warnings") {
      acceptWarnings = false;
    } else if (arg == "--help" || arg == "-h") {
      printUsage();
      return kExitOk;
    } else if (!arg.empty() && arg[0] == '-') {
      std::cout << "未知选项：" << arg << "\n";
      printUsage();
      return kExitNoRoot;
    } else if (rootArg.empty()) {
      rootArg = arg;
    }
  }

  if (rootArg.empty()) {
    if (const char* env = std::getenv("LOONGBONES_CORPUS_ROOT"); env != nullptr) rootArg = env;
  }
  if (rootArg.empty()) {
    printUsage();
    return kExitNoRoot;
  }
  const fs::path corpusRoot(rootArg);
  if (!fs::is_directory(corpusRoot)) {
    std::cout << "语料根不存在或不是目录：" << rootArg << "\n";
    return kExitNoRoot;
  }
  if (projectDir.empty()) {
    projectDir = fs::temp_directory_path() / "lb_skeleton_import_smoke";
  }

  // ---- 最小宿主：注册表 + 内置插件 ----
  auto& registry = host::ImportRegistry::instance();
  registry.clear();
  host::registerBuiltinPlugins(registry);
  const api::IImportPlugin* plugin =
      registry.find(spine32::kSpine32FormatId, spine32::kSpine32FormatVersion);
  if (plugin == nullptr) {
    std::cout << "注册表里找不到 spine.binary@3.2.01 插件 —— 内置注册未生效。\n";
    return kExitCommitFailed;
  }

  const std::vector<Unit> units = loadCorpus(corpusRoot);
  constexpr std::size_t kMaxErrorSamples = 20;

  // 准备暂存目录：从空工程开始，逐个单位累加导入。
  fs::remove_all(projectDir);
  std::error_code mkEc;
  fs::create_directories(projectDir, mkEc);
  if (mkEc) {
    std::cout << "无法创建导入落地目录：" << projectDir << "\n";
    return kExitCommitFailed;
  }

  format::ProjectDocument document = format::createProjectDocument("skeleton-import-smoke");
  spine32::FileSystemResourceResolver resolver(corpusRoot.string());
  spine32::RandomIdFactory ids;
  CountingProgress progress;
  Tallies t;
  t.units = units.size();
  // 提交前的骨架副本：重开后与它做**逐项**比对（B5）。
  std::vector<Skeleton> expectedSkeletons;

  for (const Unit& unit : units) {
    api::ImportRequest request{document, unit.rel, resolver, ids};
    request.progressSink = &progress;
    const api::ImportDraft draft = plugin->prepare(request);
    if (!draft.valid) {
      ++t.prepareFailed;
      if (hasSiblingAtlas(unit.skel)) ++t.sharedAtlasUnits;
      if (t.errorUnits.size() < kMaxErrorSamples) {
        std::string detail = unit.rel;
        for (const api::ImportDiagnostic& d : draft.report.diagnostics) {
          if (d.severity == api::ImportSeverity::kError) {
            detail += "  [" + std::string(toString(d.code)) + "] " + d.message + " @" + d.location;
            break;
          }
        }
        t.errorUnits.push_back(detail);
      }
    } else {
      ++t.prepared;
    }

    for (const api::ImportDiagnostic& d : draft.report.diagnostics) {
      ++t.codes[d.code];
      if (d.code == api::ImportCode::kUnresolvedReference) {
        ++t.unresolved;
        if (t.unresolvedUnits.size() < kMaxErrorSamples) t.unresolvedUnits.push_back(unit.rel);
      }
      if (d.code == api::ImportCode::kAttachmentRegionDegraded) ++t.degraded;
      if (d.severity == api::ImportSeverity::kWarning &&
          d.code != api::ImportCode::kUnsupportedCapability && t.warningSamples.size() < 8) {
        ++t.warningSamples[std::string(toString(d.code)) + " @" + d.location + ": " + d.message];
      }
    }
    for (const auto& kv : draft.report.unsupported) t.unsupported[kv.first] += kv.second;

    // ---- 端到端：只对前 commitLimit 个有效单位做完整提交 + 重开验证 ----
    // 其余单位只做 prepare 计数（把 1113 个单位的 PNG 全部复制一遍没有意义）。
    if (!draft.valid || t.committed >= commitLimit) continue;
    if (draft.report.hasErrors()) continue;

    spine32::DirectoryResourceStaging staging(projectDir.string());
    host::ImportCommitOptions options{projectDir.string(), "dev", acceptWarnings};
    const host::ImportCommitOutcome outcome = host::commitImport(draft, options, staging, document);
    if (!outcome.committed) {
      std::cout << "\n提交失败：" << unit.rel << "\n";
      for (const api::ImportDiagnostic& d : outcome.diagnostics) {
        if (d.severity == api::ImportSeverity::kError) {
          std::cout << "  [" << toString(d.code) << "] " << d.message << " @" << d.location << "\n";
        }
      }
      return kExitCommitFailed;
    }
    ++t.committed;
    // 保存提交前的骨架（commitImport 之后 document 已被替换）
    if (!draft.document.data.skeletons.empty()) {
      expectedSkeletons.push_back(draft.document.data.skeletons.back());
    }
    t.stagedFiles += draft.resources.size();
    t.textures += countOf(draft.report.counts, "texture");
    t.regions += countOf(draft.report.counts, "atlasRegion");
    t.bones += countOf(draft.report.counts, "bone");
    t.slots += countOf(draft.report.counts, "slot");
    t.skins += countOf(draft.report.counts, "skin");
    t.skinEntries += countOf(draft.report.counts, "skinEntry");
  }

  // ---- 重开验证：落盘的工程必须把**同样多**的对象读回来 ----
  bool reopenConsistent = false;
  if (t.committed > 0) {
    const format::IoResult reopened = format::openProject(projectDir.string(), format::ReaderLimits{});
    reopenConsistent = reopened.ok &&
                       reopened.document.data.skeletons.size() == t.committed &&
                       reopened.document.data.atlases.size() == t.committed &&
                       reopened.document.data.textures.size() == t.textures &&
                       reopened.document.data.atlases.size() == t.committed;
    if (!reopened.ok) {
      std::cout << "\n重开失败：[" << toString(reopened.formatDiag.code) << "] "
                << reopened.formatDiag.message << "\n";
      return kExitCommitFailed;
    }
    std::size_t reopenedRegions = 0;
    for (const Atlas& a : reopened.document.data.atlases) reopenedRegions += a.regions.size();
    std::size_t reopenedBones = 0;
    for (const Skeleton& s : reopened.document.data.skeletons) reopenedBones += s.bones.size();
    reopenConsistent = reopenConsistent && reopenedRegions == t.regions && reopenedBones == t.bones;

    // ---- 逐项语义比对（B5）：顶点 / UV / 三角 / 权重 / 约束参数 ----
    DeepDiff diff;
    for (std::size_t i = 0;
         i < expectedSkeletons.size() && i < reopened.document.data.skeletons.size(); ++i) {
      compareSkeletonDeep(expectedSkeletons[i], reopened.document.data.skeletons[i], diff);
    }
    // 输出用 ASCII 前缀：管道/编码不影响取证（规范 §8.2：只输出统计量）。
    std::cout << "\n[deep-compare] items=" << diff.compared
              << " maxResidual=" << diff.maxAbsResidual
              << " mismatches=" << diff.mismatches.size() << "\n";
    for (const std::string& m : diff.mismatches) std::cout << "  ! " << m << "\n";
    reopenConsistent = reopenConsistent && diff.mismatches.empty();

    std::cout << "\n重开后：骨架 " << reopened.document.data.skeletons.size() << " / 图集 "
              << reopened.document.data.atlases.size() << " / 纹理 "
              << reopened.document.data.textures.size() << " / 区域 " << reopenedRegions
              << " / 骨骼 " << reopenedBones << "\n";
  }

  // ---- 报告 ----
  std::cout << "\n语料根              " << corpusRoot.string() << "\n";
  std::cout << "导入落地目录        " << projectDir.string() << "\n";
  std::cout << "单位总数            " << t.units << "\n";
  std::cout << "prepare 成功        " << t.prepared << "\n";
  std::cout << "prepare 失败        " << t.prepareFailed << "\n";
  std::cout << "完整提交（含重开）  " << t.committed << "（上限 " << commitLimit << "）\n";
  std::cout << "落盘资源文件        " << t.stagedFiles << "\n";
  std::cout << "region 附件         " << t.regions << "\n";
  std::cout << "尾随空格降级        " << t.degraded << "\n";
  std::cout << "region 未命中       " << t.unresolved << "\n";
  std::cout << "prepare 失败中共享图集布局  " << t.sharedAtlasUnits << "\n";
  std::cout << "进度回调次数        " << progress.calls() << "（异常 " << progress.inconsistent()
            << "）\n";

  std::cout << "\n不支持能力（只计数 + warning，不建半成品对象）：\n";
  for (const auto& kv : t.unsupported) std::cout << "  " << kv.first << "  " << kv.second << "\n";

  if (!t.warningSamples.empty()) {
    std::cout << "\nwarning 样本（前 " << t.warningSamples.size() << " 类）：\n";
    for (const auto& kv : t.warningSamples) std::cout << "  " << kv.second << "× " << kv.first << "\n";
  }

  std::cout << "\n诊断码：\n";
  for (const auto& kv : t.codes) std::cout << "  " << toString(kv.first) << "  " << kv.second << "\n";

  if (!t.errorUnits.empty()) {
    std::cout << "\nprepare 失败单位（前 " << t.errorUnits.size() << "）：\n";
    for (const std::string& s : t.errorUnits) std::cout << "  " << s << "\n";
  }
  if (!t.unresolvedUnits.empty()) {
    std::cout << "\nregion 未命中单位（前 " << t.unresolvedUnits.size() << "）：\n";
    for (const std::string& s : t.unresolvedUnits) std::cout << "  " << s << "\n";
  }

  fs::remove_all(projectDir);

  // ---- 验收判定：与同级 smoke 工具同一套守门规则 ----
  if (t.units == 0) {
    std::cout << "\n结论：语料根下没有任何 .skel.bytes —— 根目录很可能给错了。\n";
    return kExitNoSkeleton;
  }
  if (t.prepared == 0) {
    std::cout << "\n结论：没有任何单位产出有效候选 —— 语料根给错或图集命名不匹配，"
                 "拒绝把这次扫描当作验收通过。\n";
    return kExitNothingPrepared;
  }
  if (t.unresolved > 0) {
    std::cout << "\n结论：存在 region 未命中（" << t.unresolved << " 个单位）。\n";
    return kExitErrorsPresent;
  }
  if (t.prepareFailed > 0) {
    std::cout << "\n结论：存在 prepare 失败的单位（" << t.prepareFailed << " 个，其中 "
              << t.sharedAtlasUnits << " 个属共享图集布局的已知缺口）。\n";
    return t.sharedAtlasUnits == t.prepareFailed ? kExitResourceMissing : kExitErrorsPresent;
  }
  if (!reopenConsistent) {
    std::cout << "\n结论：提交后重开的对象数与提交前不一致。\n";
    return kExitCommitFailed;
  }
  std::cout << "\n结论：端到端闭合通过 —— prepare → 事务提交 → 重开，对象数一致，region 未命中 0。\n";
  return kExitOk;
}
