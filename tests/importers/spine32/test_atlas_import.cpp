// Task 4 B1：Atlas + PNG 导入切片测试。
//
// 【2026-10-21 重写说明】本文件在一次目录整理中被误删（无 git、回收站无记录，
// 无法还原）。当前内容是**依据 importers/spine32 的公开 API 与
// tools/atlas-import-tests/README.md 记录的覆盖项重新编写**的。用例名沿用原清单
// 的覆盖面，断言细节可能与原文件不完全一致。若找回原文件，请以原文件为准。
//
// 全部输入为合成 Atlas 文本 + 生成的 PNG 头，不读取任何游戏资产。

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <vector>

#include "loongbones/domain/atlas.hpp"
#include "loongbones/format/project_io.hpp"
#include "loongbones/importers/spine32/atlas_import.hpp"
#include "tests/domain/test_framework.hpp"

namespace fs = std::filesystem;
using namespace loongbones;
using namespace loongbones::importers::spine32;

namespace {

// ---- 合成 PNG（签名 + IHDR + CRC，不含像素数据）----

std::uint32_t crc32Of(const std::uint8_t* data, std::size_t size) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1u) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

void pushBe32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v >> 24));
  out.push_back(static_cast<std::uint8_t>(v >> 16));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v));
}

std::vector<std::uint8_t> makePng(std::int32_t width, std::int32_t height) {
  std::vector<std::uint8_t> b;
  const std::uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  b.insert(b.end(), signature, signature + 8);
  pushBe32(b, 13);  // IHDR 长度
  b.push_back('I'); b.push_back('H'); b.push_back('D'); b.push_back('R');
  pushBe32(b, static_cast<std::uint32_t>(width));
  pushBe32(b, static_cast<std::uint32_t>(height));
  b.push_back(8);  // bit depth
  b.push_back(6);  // color type RGBA
  b.push_back(0); b.push_back(0); b.push_back(0);  // compression / filter / interlace
  pushBe32(b, crc32Of(b.data() + 12, 17));
  return b;
}

// ---- 合成资源解析器（不碰磁盘）----

class FakeResolver final : public IResourceResolver {
 public:
  void add(std::string path, std::vector<std::uint8_t> bytes) {
    files_.emplace(std::move(path), std::move(bytes));
  }
  bool read(std::string_view sourceRelativePath, std::vector<std::uint8_t>& bytes,
            std::string& error) const override {
    const auto it = files_.find(std::string(sourceRelativePath));
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

// ---- 临时工程目录 ----

fs::path makeTempDir(const std::string& tag) {
  const fs::path base = fs::temp_directory_path() / ("loongbones_atlas_test_" + tag);
  std::error_code ec;
  fs::remove_all(base, ec);
  fs::create_directories(base, ec);
  return base;
}

AtlasImportDraft prepare(const std::string& atlasText, const FakeResolver& resolver,
                         const format::ProjectDocument& current = format::createProjectDocument("t")) {
  RandomIdFactory ids;
  return prepareAtlasImport(current, atlasText, "atlas/hero.atlas.txt", resolver, ids);
}

bool hasWarning(const std::vector<AtlasImportDiagnostic>& diags) {
  return std::any_of(diags.begin(), diags.end(), [](const AtlasImportDiagnostic& d) {
    return d.severity == AtlasImportSeverity::kWarning;
  });
}

bool hasCode(const std::vector<AtlasImportDiagnostic>& diags, AtlasImportCode want) {
  return std::any_of(diags.begin(), diags.end(),
                     [want](const AtlasImportDiagnostic& d) { return d.code == want; });
}

}  // namespace

// ---- 解析与映射 ----

TEST(AtlasImport, SinglePageAtlasMapsRegions) {
  FakeResolver r;
  r.add("atlas/page1.png", makePng(512, 512));
  const std::string text =
      "page1.png\n"
      "size: 512,512\n"
      "\n"
      "hero_head\n"
      "  xy: 0,0\n"
      "  size: 64,32\n"
      "  orig: 64,32\n"
      "  offset: 0,0\n"
      "  rotate: false\n"
      "\n"
      "hero_arm\n"
      "  xy: 64,0\n"
      "  size: 32,16\n"
      "  orig: 32,16\n"
      "  offset: 0,0\n"
      "  rotate: true\n";

  const AtlasImportDraft d = prepare(text, r);
  CHECK(d.valid);
  CHECK_EQ(d.pageCount, std::size_t{1});
  CHECK_EQ(d.regionCount, std::size_t{2});
  CHECK_EQ(d.textures.size(), std::size_t{1});
  CHECK_EQ(d.document.data.atlases.size(), std::size_t{1});

  const Atlas& atlas = d.document.data.atlases.front();
  CHECK_EQ(atlas.pages.size(), std::size_t{1});
  CHECK_EQ(atlas.regions.size(), std::size_t{2});
  CHECK_EQ(atlas.pages[0].width, 512);
  CHECK_EQ(atlas.pages[0].height, 512);

  const AtlasRegion& head = atlas.regions[0];
  CHECK_EQ(head.packedX, 0);
  CHECK_EQ(head.packedY, 0);
  CHECK_EQ(head.packedW, 64);
  CHECK_EQ(head.packedH, 32);
  CHECK_EQ(static_cast<int>(head.rotation), static_cast<int>(RegionRotation::kNone));

  const AtlasRegion& arm = atlas.regions[1];
  CHECK_EQ(arm.packedX, 64);
  CHECK_EQ(static_cast<int>(arm.rotation), static_cast<int>(RegionRotation::kDeg90));
}

TEST(AtlasImport, MultiPageAtlasSeparatesPages) {
  FakeResolver r;
  r.add("atlas/page1.png", makePng(256, 256));
  r.add("atlas/page2.png", makePng(128, 128));
  const std::string text =
      "page1.png\n"
      "size: 256,256\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 10,10\n"
      "\n"
      "page2.png\n"
      "size: 128,128\n"
      "\n"
      "b\n"
      "  xy: 0,0\n"
      "  size: 20,20\n"
      "\n"
      "c\n"
      "  xy: 20,0\n"
      "  size: 30,30\n";

  const AtlasImportDraft d = prepare(text, r);
  CHECK(d.valid);
  CHECK_EQ(d.pageCount, std::size_t{2});
  CHECK_EQ(d.regionCount, std::size_t{3});
  CHECK_EQ(d.textures.size(), std::size_t{2});
  CHECK_EQ(d.document.data.atlases.front().pages.size(), std::size_t{2});
}

/// 图集坐标尺寸与 PNG 像素尺寸是两回事，不得互相覆盖。
TEST(AtlasImport, AtlasPageSizeIndependentOfPng) {
  FakeResolver r;
  r.add("atlas/page1.png", makePng(256, 128));  // IHDR 是 256×128
  const std::string text =
      "page1.png\n"
      "size: 512,512\n"  // 图集声明 512×512
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 10,10\n";

  const AtlasImportDraft d = prepare(text, r);
  CHECK(d.valid);
  const Atlas& atlas = d.document.data.atlases.front();
  CHECK_EQ(atlas.pages[0].width, 512);
  CHECK_EQ(atlas.pages[0].height, 512);
  CHECK(d.document.data.textures[0].pixelWidth.has_value());
  CHECK_EQ(d.document.data.textures[0].pixelWidth.value(), 256);
  CHECK_EQ(d.document.data.textures[0].pixelHeight.value(), 128);
}

// ---- 拒绝路径 ----

TEST(AtlasImport, PngHeaderInvalidRejected) {
  FakeResolver r;
  auto png = makePng(64, 64);
  png[16] ^= 0xFF;  // 破坏 IHDR 宽度 → CRC 校验失败
  r.add("atlas/page1.png", png);
  const std::string text =
      "page1.png\n"
      "size: 64,64\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 10,10\n";

  const AtlasImportDraft d = prepare(text, r);
  CHECK(!d.valid);
  CHECK(hasCode(d.diagnostics, AtlasImportCode::kPngInvalid));
}

TEST(AtlasImport, UnsafePagePathRejected) {
  FakeResolver r;
  r.add("evil.png", makePng(64, 64));
  const std::string text =
      "../evil.png\n"
      "size: 64,64\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 10,10\n";

  const AtlasImportDraft d = prepare(text, r);
  CHECK(!d.valid);
  CHECK(hasCode(d.diagnostics, AtlasImportCode::kSourcePathRejected));
}

/// 未进入 Domain 模型的 Atlas 字段产生 warning，不中断导入。
TEST(AtlasImport, UnsupportedPageFieldWarns) {
  FakeResolver r;
  r.add("atlas/page1.png", makePng(64, 64));
  const std::string text =
      "page1.png\n"
      "size: 64,64\n"
      "format: RGBA8888\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 10,10\n";

  const AtlasImportDraft d = prepare(text, r);
  CHECK(d.valid);
  CHECK(hasWarning(d.diagnostics));
  CHECK_EQ(d.regionCount, std::size_t{1});
}

// ---- 提交与落盘 ----

/// 有 warning 时必须显式接受，否则不写任何文件。
TEST(AtlasImport, CommitWithoutAcceptingWarningsRejected) {
  FakeResolver r;
  r.add("atlas/page1.png", makePng(64, 64));
  const std::string text =
      "page1.png\n"
      "size: 64,64\n"
      "format: RGBA8888\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 10,10\n";

  const AtlasImportDraft d = prepare(text, r);
  CHECK(d.valid);
  CHECK(hasWarning(d.diagnostics));

  const fs::path dir = makeTempDir("warn");
  DirectoryResourceStaging staging(dir.string());
  CHECK(staging.valid());
  format::ProjectDocument doc = d.document;
  const auto diags = commitAtlasImport(d, dir.string(), "0.1.0", false, staging, doc);
  CHECK(hasCode(diags, AtlasImportCode::kWarningsNotAccepted));
  CHECK(!fs::exists(dir / format::kProjectFileName));

  std::error_code ec;
  fs::remove_all(dir, ec);
}

TEST(AtlasImport, CommitSavesAndReopens) {
  FakeResolver r;
  r.add("atlas/page1.png", makePng(64, 64));
  const std::string text =
      "page1.png\n"
      "size: 64,64\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 10,10\n";

  const AtlasImportDraft d = prepare(text, r);
  CHECK(d.valid);

  const fs::path dir = makeTempDir("commit");
  DirectoryResourceStaging staging(dir.string());
  CHECK(staging.valid());
  format::ProjectDocument doc = d.document;
  const auto diags = commitAtlasImport(d, dir.string(), "0.1.0", true, staging, doc);
  CHECK(!hasCode(diags, AtlasImportCode::kProjectSaveFailed));
  CHECK(fs::exists(dir / format::kProjectFileName));
  CHECK(fs::exists(dir / "assets" / "textures"));

  const auto reopened = format::openProject(dir.string(), format::ReaderLimits{});
  CHECK(reopened.ok);
  CHECK_EQ(reopened.document.data.atlases.size(), std::size_t{1});
  CHECK_EQ(reopened.document.data.atlases.front().pages.size(), std::size_t{1});
  CHECK_EQ(reopened.document.data.atlases.front().regions.size(), std::size_t{1});
  CHECK_EQ(reopened.document.data.textures.size(), std::size_t{1});

  std::error_code ec;
  fs::remove_all(dir, ec);
}
