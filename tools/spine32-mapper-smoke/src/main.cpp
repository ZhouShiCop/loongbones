// Task 4 B5：语料只读扫描 —— Reader + Atlas Reader + Mapper 的**端到端数数**。
//
// 目的：B5 的合成断言覆盖不了真实语料的规模与脏数据。这里对语料里每个
// `.skel.bytes` 走一遍完整链路，回答三个问题：
//   1. 有多少附件**精确命中**图集 region（field-mapping §10.1：`path ?? name`）；
//   2. 有多少走**尾随空格降级**（warning，语料预期 4 例）；
//   3. 有多少**未命中**（error，静默丢弃的反面）。
//
// 判据不是"能不能跑完"，而是**未命中必须能被点名** —— 与 B4 扫描工具同一套纪律：
// 语料根给错时锁定样本 0 匹配 → 非 0 退出。

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "loongbones/domain/atlas.hpp"
#include "loongbones/domain/ids.hpp"
#include "loongbones/importers/spine32/atlas_text_reader.hpp"
#include "loongbones/importers/spine32/binary_reader.hpp"
#include "loongbones/importers/spine32/mapper.hpp"

namespace fs = std::filesystem;
using namespace loongbones;
using namespace loongbones::importers::spine32;

namespace {

struct Unit {
  std::string rel;
  fs::path skel;
  fs::path atlas;
};

std::vector<Unit> loadCorpus(const fs::path& root) {
  std::vector<Unit> out;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file(ec)) continue;
    const fs::path& p = it->path();
    if (p.extension() != ".bytes") continue;
    const std::string name = p.filename().string();
    const std::string suffix = ".skel.bytes";
    if (name.size() < suffix.size() || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
    Unit u;
    u.skel = p;
    u.atlas = p;
    u.atlas.replace_extension("");                      // 去掉 .bytes
    u.atlas.replace_extension("");                      // 去掉 .skel
    u.atlas += ".atlas.txt";
    std::error_code relEc;
    u.rel = fs::relative(p, root, relEc).generic_string();
    out.push_back(std::move(u));
  }
  return out;
}

bool readWholeFile(const fs::path& p, std::vector<std::uint8_t>& bytes) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  in.seekg(0, std::ios::end);
  const auto size = in.tellg();
  if (size < 0) return false;
  in.seekg(0, std::ios::beg);
  bytes.resize(static_cast<std::size_t>(size));
  if (!bytes.empty()) in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<std::size_t>(in.gcount()) == bytes.size();
}

/// 把解析出的图集文本落成一份**只含名字**的 canonical Atlas，供 Mapper 查 region。
/// 这里不读 PNG、不校验页边界 —— 本工具只关心"名字能否解析到 region"。
Atlas atlasFromText(const AtlasText& parsed, const std::string& name) {
  Atlas atlas;
  atlas.id = Id::generate(IdKind::Atlas);
  atlas.name = name;
  for (const SourceAtlasPage& page : parsed.pages) {
    AtlasPage p;
    p.id = Id::generate(IdKind::AtlasPage);
    p.name = page.file;
    p.textureAssetId = Id::generate(IdKind::TextureAsset);
    p.width = page.width;
    p.height = page.height;
    atlas.pages.push_back(p);
    for (const SourceAtlasRegion& r : page.regions) {
      AtlasRegion region;
      region.id = Id::generate(IdKind::AtlasRegion);
      region.name = r.name;
      region.pageId = p.id;
      region.packedX = r.x;
      region.packedY = r.y;
      region.packedW = r.width;
      region.packedH = r.height;
      region.rotation = r.rotation;
      region.originalW = r.originalWidth;
      region.originalH = r.originalHeight;
      region.trimmedW = r.trimmedWidth;
      region.trimmedH = r.trimmedHeight;
      region.offsetX = r.offsetX;
      region.offsetY = r.offsetY;
      atlas.regions.push_back(std::move(region));
    }
  }
  return atlas;
}

class SmokeIdFactory final : public IIdFactory {
 public:
  Id create(IdKind kind) override { return Id::generate(kind); }
};

struct Tallies {
  std::size_t units = 0;
  std::size_t readerFailed = 0;
  std::size_t atlasMissing = 0;
  std::size_t atlasFailed = 0;
  std::size_t mapped = 0;
  std::size_t mapErrors = 0;
  std::size_t regions = 0;
  std::size_t degraded = 0;
  std::size_t unresolved = 0;
  /// 非 region 附件：本阶段**不进 canonical**，但必须数出来（不得静默）。
  std::size_t skippedNonRegion = 0;
  std::map<MapperCode, std::size_t> codes;
  std::vector<std::string> unresolvedUnits;
  std::vector<std::string> atlasMissingUnits;
};

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif
  const char* root = nullptr;
  if (argc > 1) root = argv[1];
  if (root == nullptr || std::string(root).empty()) root = std::getenv("LOONGBONES_CORPUS_ROOT");
  if (root == nullptr || std::string(root).empty()) {
    std::cout << "用法：spine32-mapper-smoke <语料根目录>   或设置 $LOONGBONES_CORPUS_ROOT\n";
    return 2;
  }
  const fs::path corpus(root);
  if (!fs::is_directory(corpus)) {
    std::cout << "语料根不存在\n";
    return 2;
  }

  const std::vector<Unit> units = loadCorpus(corpus);
  Tallies t;
  t.units = units.size();
  for (const Unit& u : units) {
    std::vector<std::uint8_t> skelBytes;
    if (!readWholeFile(u.skel, skelBytes)) { ++t.readerFailed; continue; }
    const ReadResult read = readSkeletonBinary32(skelBytes);
    if (!read.ok) { ++t.readerFailed; continue; }

    if (!fs::exists(u.atlas)) {
      ++t.atlasMissing;
      if (t.atlasMissingUnits.size() < 20) t.atlasMissingUnits.push_back(u.rel);
      continue;
    }
    std::vector<std::uint8_t> atlasBytes;
    if (!readWholeFile(u.atlas, atlasBytes)) {
      ++t.atlasMissing;
      if (t.atlasMissingUnits.size() < 20) t.atlasMissingUnits.push_back(u.rel);
      continue;
    }
    const std::string atlasText(atlasBytes.begin(), atlasBytes.end());
    const AtlasText parsed = readAtlasText(atlasText);
    if (!parsed.valid) { ++t.atlasFailed; continue; }

    const Atlas atlas = atlasFromText(parsed, u.rel);
    AtlasRegionIndex index({atlas});
    SmokeIdFactory ids;
    MapperOptions options;
    options.skeletonName = u.rel;
    options.atlasIds = {atlas.id};
    const MapResult mapped = mapSkeletonToCanonical(read.skeleton, options, index, ids);

    ++t.mapped;
    t.regions += mapped.regionAttachmentCount;
    const UnsupportedCounts& u0 = mapped.unsupported;
    t.skippedNonRegion += u0.mesh + u0.weightedMesh + u0.linkedMesh + u0.weightedLinkedMesh +
                          u0.boundingBox + u0.otherAttachmentType;
    bool unresolvedHere = false;
    for (const MapperDiagnostic& d : mapped.diagnostics) {
      ++t.codes[d.code];
      if (d.code == MapperCode::kAttachmentRegionDegraded) ++t.degraded;
      if (d.code == MapperCode::kAttachmentRegionUnresolved) unresolvedHere = true;
    }
    if (!mapped.valid) {
      ++t.mapErrors;
      if (unresolvedHere) {
        ++t.unresolved;
        if (t.unresolvedUnits.size() < 20) t.unresolvedUnits.push_back(u.rel);
      }
    }
  }

  std::cout << "单位总数           " << t.units << "\n";
  std::cout << "Reader 失败        " << t.readerFailed << "\n";
  std::cout << "图集缺失           " << t.atlasMissing << "\n";
  std::cout << "图集解析失败       " << t.atlasFailed << "\n";
  std::cout << "进入映射           " << t.mapped << "\n";
  std::cout << "映射失败（error）  " << t.mapErrors << "\n";
  std::cout << "region 附件总数    " << t.regions << "\n";
  std::cout << "非 region 跳过     " << t.skippedNonRegion << "（只计数+诊断，不进 canonical）\n";
  std::cout << "尾随空格降级       " << t.degraded << "\n";
  std::cout << "region 未命中      " << t.unresolved << "\n";

  if (!t.atlasMissingUnits.empty()) {
    std::cout << "\n缺图集单位（前 " << t.atlasMissingUnits.size() << "）：\n";
    for (const std::string& s : t.atlasMissingUnits) std::cout << "  " << s << "\n";
  }

  std::cout << "\n诊断码：\n";
  for (const auto& kv : t.codes) std::cout << "  " << toString(kv.first) << "  " << kv.second << "\n";

  if (!t.unresolvedUnits.empty()) {
    std::cout << "\n未命中单位（前 " << t.unresolvedUnits.size() << "）：\n";
    for (const std::string& s : t.unresolvedUnits) std::cout << "  " << s << "\n";
  }

  // ---- 验收判定：与 B4 扫描工具同一套守门规则 ----
  if (t.units == 0) {
    std::cout << "\n结论：语料根下没有任何 .skel.bytes —— 根目录很可能给错了。\n";
    return 3;
  }
  if (t.mapped == 0) {
    std::cout << "\n结论：没有任何单位走完映射链路 —— 语料根给错或图集命名不匹配，"
                 "拒绝把这次扫描当作验收通过。\n";
    return 4;
  }
  if (t.unresolved > 0) {
    std::cout << "\n结论：存在 region 未命中（" << t.unresolved << " 个单位）。\n";
    return 6;
  }
  std::cout << "\n结论：全部单位映射通过，region 未命中 0。\n";
  return 0;
}
