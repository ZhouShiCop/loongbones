// Task 4 B4：Spine 3.2.01 二进制 Reader 的语料只读扫描工具。
//
// 目的：用**真实语料**独立校验 Reader（strict EOF + 结构计数），
// 并把结果可与 docs/skeleton/sample-manifest.md §2 的锁定值对照。
//
// 除成败之外还报告**失败时已走到文件的百分之几**——用来区分
// "卡在皮肤段（布局假设错）"与"卡在动画段（另一处未验证）"。
//
// 只读：不写任何文件、不复制语料、不输出语料路径以外的内容。
// 需要本地语料根（$LOONGBONES_CORPUS_ROOT 或首参数），故不进 CTest 默认集合。

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "loongbones/importers/spine32/binary_reader.hpp"

namespace fs = std::filesystem;
using loongbones::importers::spine32::readSkeletonBinary32;
using loongbones::importers::spine32::toString;

namespace {

struct File {
  std::string rel;
  std::vector<std::uint8_t> bytes;
};

struct Row {
  const File* file = nullptr;
  bool ok = false;
  std::string code;
  std::string section;
  std::size_t offset = 0;
  std::size_t bones = 0;
  std::size_t slots = 0;
  std::size_t skins = 0;
  std::size_t regions = 0;
  std::size_t meshes = 0;
};

std::vector<std::uint8_t> readAll(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
}

std::vector<File> loadCorpus(const fs::path& corpus) {
  std::vector<File> files;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(corpus, ec), end; it != end; it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file()) continue;
    if (it->path().extension() != ".bytes") continue;
    File f;
    f.bytes = readAll(it->path());
    std::string rel = fs::relative(it->path(), corpus).string();
    std::replace(rel.begin(), rel.end(), '\\', '/');
    f.rel = std::move(rel);
    files.push_back(std::move(f));
  }
  return files;
}

double progressOf(const Row& r) {
  if (r.file->bytes.empty()) return 0.0;
  return static_cast<double>(r.offset) / static_cast<double>(r.file->bytes.size());
}

/// Primary 口径 = `<单位目录>/<同名>.skel.bytes`，且必须是**恰好两层**
/// （即单位目录直接位于语料根下）。与 `sample-manifest.md` §4 的 963 一致。
///
/// 注意别写成「任意深度的父目录同名」：那样会把 `h0166s5/h0166s5_buoy/…` 这类
/// 子单位也算进来，实测得 996 而不是 manifest 的 963。
/// Full 与 Primary 是两个不同分母，**不得混用**。
bool isPrimary(const std::string& rel) {
  const auto first = rel.find('/');
  if (first == std::string::npos) return false;
  if (rel.find('/', first + 1) != std::string::npos) return false;
  const std::string dir = rel.substr(0, first);
  return rel.substr(first + 1) == dir + ".skel.bytes";
}

std::vector<Row> scan(const std::vector<File>& files) {
  std::vector<Row> rows;
  rows.reserve(files.size());
  for (const File& f : files) {
    const auto r = readSkeletonBinary32(f.bytes);
    Row row;
    row.file = &f;
    row.ok = r.ok;
    if (!r.ok && !r.diagnostics.empty()) {
      row.code = toString(r.diagnostics[0].code);
      row.section = r.diagnostics[0].section;
      row.offset = r.diagnostics[0].offset;
    }
    if (r.ok) {
      row.bones = r.skeleton.bones.size();
      row.slots = r.skeleton.slots.size();
      row.skins = r.skeleton.skins.size();
      for (const auto& skin : r.skeleton.skins) {
        for (const auto& slot : skin.slots) {
          for (const auto& a : slot.attachments) {
            if (a.typeByte == 0) ++row.regions;
            else ++row.meshes;
          }
        }
      }
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

/// manifest 16 个锁定样本（路径来自 sample-manifest.md §1，期望值来自 §2 的
/// 「骨/槽/皮肤」与「region/mesh/加权」的首项）。**期望值不得由本工具反推**。
const std::vector<std::pair<std::string, std::array<std::size_t, 4>>>& lockedSamples() {
  static const std::vector<std::pair<std::string, std::array<std::size_t, 4>>> locked = {
      {"h3181c2/h3181c2.skel.bytes", {31, 19, 1, 14}},                              // 01
      {"b3310c3/b3310c3_cocoon/b3310c3_cocoon.skel.bytes", {1, 1, 1, 1}},           // 02
      {"h0261s5/s3/h3004c3.skel.bytes", {55, 17, 1, 9}},                            // 03
      {"h0215s5/h0215s5_doll2/h0215s5_doll2.skel.bytes", {25, 12, 1, 7}},           // 04
      {"h0166s5/h0166s5_buoy/h0166s5_buoy.skel.bytes", {16, 14, 1, 10}},            // 05
      {"b0015s5/b0015s5_meat/b0015s5_meat.skel.bytes", {2, 1, 1, 0}},               // 06
      {"e0039c1/fx_e0039c1_s01/e0039c1_s01_bug.skel.bytes", {4, 1, 1, 0}},          // 07
      {"t0269s5/s1/s1_shan1.skel.bytes", {16, 6, 1, 1}},                            // 08
      {"h0210s5/h0210s5_s3/h0210s5_S3.skel.bytes", {25, 11, 1, 2}},                 // 09
      {"h0040s5/bowl/h0040s5.skel.bytes", {2, 2, 1, 2}},                            // 10
      {"h0133s5/h0133s5.skel.bytes", {105, 43, 1, 55}},                             // 11
      {"h0164s5/watch/h0164s5_watch_minute.skel.bytes", {3, 1, 1, 1}},              // 12
      {"e0039c1/fx_e0039c1_s02/e0039c1_s02.skel.bytes", {4, 2, 1, 2}},              // 13
      {"b0001s3/b0001s3.skel.bytes", {29, 23, 1, 8}},                               // 14
      {"b0146c3/b0146c3.skel.bytes", {40, 26, 1, 18}},                              // 15
      {"h3687c4/h3687c4.skel.bytes", {41, 25, 1, 16}},                              // 16
  };
  return locked;
}

/// 返回进程退出码。**只有 0 才可作为验收证据** —— 语料根给错、锁定样本一个都没
/// 匹配到、或仍有文件未通过，都必须是非 0，否则一次无效扫描会被当成通过。
int reportDetail(const std::vector<Row>& rows) {
  std::size_t okCount = 0, primaryCount = 0, okPrimary = 0;
  for (const Row& r : rows) {
    if (r.ok) ++okCount;
    if (isPrimary(r.file->rel)) {
      ++primaryCount;
      if (r.ok) ++okPrimary;
    }
  }
  std::cout << "Full    " << rows.size() << " | ok " << okCount << " | fail "
            << (rows.size() - okCount) << "\n";
  std::cout << "Primary " << primaryCount << " | ok " << okPrimary << " | fail "
            << (primaryCount - okPrimary) << "\n\n";

  std::vector<std::pair<std::string, std::size_t>> buckets;
  for (const Row& r : rows) {
    if (r.ok) continue;
    auto it = std::find_if(buckets.begin(), buckets.end(),
                           [&](const auto& p) { return p.first == r.code; });
    if (it == buckets.end()) buckets.emplace_back(r.code, 1);
    else ++it->second;
  }
  std::sort(buckets.begin(), buckets.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  std::cout << "失败分类：\n";
  for (const auto& p : buckets) std::cout << "  " << p.first << "  " << p.second << "\n";

  std::size_t inSkin = 0, inAnim = 0, other = 0;
  for (const Row& r : rows) {
    if (r.ok) continue;
    if (r.section.rfind("skins/", 0) == 0) ++inSkin;
    else if (r.section.rfind("animations/", 0) == 0) ++inAnim;
    else ++other;
  }
  std::cout << "\n失败位置：skins=" << inSkin << " animations=" << inAnim << " 其它=" << other
            << "\n";

  std::vector<double> prog;
  for (const Row& r : rows) {
    if (r.ok) continue;
    prog.push_back(progressOf(r));
  }
  std::sort(prog.begin(), prog.end());
  if (!prog.empty()) {
    std::cout << "失败时进度：median=" << static_cast<int>(prog[prog.size() / 2] * 100)
              << "%  mean=" << static_cast<int>(std::accumulate(prog.begin(), prog.end(), 0.0) /
                                                static_cast<double>(prog.size()) * 100)
              << "%\n";
  }

  std::cout << "\n走得最远的 12 个失败：\n";
  std::vector<const Row*> sorted;
  for (const Row& r : rows) if (!r.ok) sorted.push_back(&r);
  std::sort(sorted.begin(), sorted.end(), [](const Row* a, const Row* b) {
    return progressOf(*a) > progressOf(*b);
  });
  for (std::size_t i = 0; i < sorted.size() && i < 12; ++i) {
    const Row& r = *sorted[i];
    const double pc = progressOf(r);
    std::cout << "  " << static_cast<int>(pc * 100) << "%  " << r.file->rel << "  " << r.code
              << " @" << r.offset << "/" << r.file->bytes.size() << " [" << r.section << "]\n";
  }

  std::cout << "\nmanifest 锁定样本对照（期望 骨/槽/皮肤/region）：\n";
  std::size_t matched = 0, checked = 0, missing = 0;
  for (const auto& want : lockedSamples()) {
    const auto it = std::find_if(rows.begin(), rows.end(),
                                 [&](const Row& r) { return r.file->rel == want.first; });
    if (it == rows.end()) {
      ++missing;
      std::cout << "  ? 未找到  " << want.first << "\n";
      continue;
    }
    ++checked;
    const bool same = it->ok && it->bones == want.second[0] && it->slots == want.second[1] &&
                      it->skins == want.second[2] && it->regions == want.second[3];
    if (same) ++matched;
    std::cout << "  " << (same ? "OK  " : "DIFF") << "  " << want.first << "  期望 "
              << want.second[0] << "/" << want.second[1] << "/" << want.second[2] << "/"
              << want.second[3] << "  实际 " << it->bones << "/" << it->slots << "/" << it->skins
              << "/" << it->regions << "  mesh=" << it->meshes
              << (it->ok ? "" : std::string("  失败=" + it->code)) << "\n";
  }
  std::cout << "\n锁定样本 " << matched << "/" << checked << " 一致";
  if (missing > 0) std::cout << "（另有 " << missing << " 个在语料中未找到）";
  std::cout << "\n";

  // ---- 验收判定：任何一项不成立都必须以非 0 退出 ----
  if (rows.empty()) {
    std::cout << "\n结论：语料根下没有任何 .skel.bytes —— 根目录很可能给错了。\n";
    return 3;
  }
  if (checked == 0) {
    std::cout << "\n结论：16 个锁定样本**一个都没在语料中匹配到**。语料根大概率给错"
                 "（例如给了 sdorica 上层目录），拒绝把这次扫描当作验收通过。\n";
    return 4;
  }
  if (matched != checked) {
    std::cout << "\n结论：锁定样本对照不一致 " << matched << "/" << checked << "。\n";
    return 5;
  }
  if (okCount != rows.size()) {
    std::cout << "\n结论：仍有 " << (rows.size() - okCount) << " 个文件未通过 strict EOF。\n";
    return 6;
  }
  std::cout << "\n结论：Full " << okCount << "/" << rows.size() << "、Primary " << okPrimary << "/"
            << primaryCount << "、锁定样本 " << matched << "/" << checked << " 全部一致。\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  // 让 UTF-8 的中文输出在 GBK 控制台上不乱码
  SetConsoleOutputCP(CP_UTF8);
#endif
  const char* root = nullptr;
  if (argc > 1) {
    root = argv[1];
  } else {
    root = std::getenv("LOONGBONES_CORPUS_ROOT");
  }
  if (root == nullptr || std::string(root).empty()) {
    std::cout << "用法：binary-read-smoke <语料根目录>   或设置 $LOONGBONES_CORPUS_ROOT\n";
    return 2;
  }
  const fs::path corpus(root);
  if (!fs::is_directory(corpus)) {
    std::cout << "语料根不存在\n";
    return 2;
  }

  const auto files = loadCorpus(corpus);
  return reportDetail(scan(files));
}
