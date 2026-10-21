// Task 4 B5：`.atlas` 文本 Reader 的合成断言。
//
// 语料 1113 个单位**全部是单页图集**（sample-manifest §2），多页、rotate、trim
// 都只能靠合成文本覆盖 —— 这正是本文件的职责。

#include "loongbones/importers/spine32/atlas_text_reader.hpp"

#include <string>

#include "tests/domain/test_framework.hpp"

namespace {

using loongbones::Atlas;
using loongbones::RegionRotation;
using loongbones::importers::spine32::AtlasText;
using loongbones::importers::spine32::AtlasTextCode;
using loongbones::importers::spine32::AtlasTextSeverity;
using loongbones::importers::spine32::readAtlasText;
using loongbones::importers::spine32::SourceAtlasPage;
using loongbones::importers::spine32::SourceAtlasRegion;

std::size_t countSeverity(const AtlasText& t, AtlasTextSeverity severity) {
  std::size_t n = 0;
  for (const auto& d : t.diagnostics) if (d.severity == severity) ++n;
  return n;
}

bool hasCode(const AtlasText& t, AtlasTextCode code) {
  for (const auto& d : t.diagnostics) if (d.code == code) return true;
  return false;
}

}  // namespace

TEST("atlas-text", "minimal single page parses with declared size preserved") {
  const std::string text =
      "page1.png\n"
      "size: 1024,512\n"
      "\n"
      "head\n"
      "  xy: 0,0\n"
      "  size: 64,32\n";
  const AtlasText t = readAtlasText(text);
  CHECK(t.valid);
  CHECK_EQ(t.pages.size(), std::size_t{1});
  CHECK_EQ(t.pages[0].file, std::string{"page1.png"});
  // 页**声明**的坐标尺寸原样保存，不做任何换算（与 PNG IHDR 分离，见 atlas_import）。
  CHECK_EQ(t.pages[0].width, 1024);
  CHECK_EQ(t.pages[0].height, 512);
  CHECK_EQ(t.pages[0].regions.size(), std::size_t{1});
  const SourceAtlasRegion& r = t.pages[0].regions[0];
  CHECK_EQ(r.name, std::string{"head"});
  CHECK_EQ(r.x, 0);
  CHECK_EQ(r.y, 0);
  CHECK_EQ(r.width, 64);
  CHECK_EQ(r.height, 32);
}

TEST("atlas-text", "missing orig falls back to size and trimmed equals size") {
  const std::string text =
      "p.png\n"
      "size: 256,256\n"
      "\n"
      "a\n"
      "  xy: 10,20\n"
      "  size: 30,40\n";
  const AtlasText t = readAtlasText(text);
  CHECK(t.valid);
  const SourceAtlasRegion& r = t.pages[0].regions[0];
  CHECK(!r.hasOriginal);
  // libGDX 不单独写 trim —— size 就是裁切后尺寸。
  CHECK_EQ(r.originalWidth, 30);
  CHECK_EQ(r.originalHeight, 40);
  CHECK_EQ(r.trimmedWidth, 30);
  CHECK_EQ(r.trimmedHeight, 40);
}

TEST("atlas-text", "explicit orig and offset are kept, offset direction is original-aligned") {
  const std::string text =
      "p.png\n"
      "size: 256,256\n"
      "\n"
      "a\n"
      "  xy: 1,2\n"
      "  size: 30,40\n"
      "  orig: 50,60\n"
      "  offset: 7,-8\n";
  const AtlasText t = readAtlasText(text);
  CHECK(t.valid);
  const SourceAtlasRegion& r = t.pages[0].regions[0];
  CHECK(r.hasOriginal);
  CHECK_EQ(r.originalWidth, 50);
  CHECK_EQ(r.originalHeight, 60);
  CHECK_EQ(r.offsetX, 7);
  CHECK_EQ(r.offsetY, -8);
  CHECK_EQ(r.trimmedWidth, 30);
  CHECK_EQ(r.trimmedHeight, 40);
}

TEST("atlas-text", "non-square region keeps both extents") {
  const std::string text =
      "p.png\n"
      "size: 256,256\n"
      "\n"
      "a\n"
      "  xy: 3,4\n"
      "  size: 120,17\n";
  const AtlasText t = readAtlasText(text);
  CHECK(t.valid);
  CHECK_EQ(t.pages[0].regions[0].width, 120);
  CHECK_EQ(t.pages[0].regions[0].height, 17);
}

TEST("atlas-text", "rotate true maps to 90 degrees, false to none, other values rejected") {
  const std::string base =
      "p.png\n"
      "size: 64,64\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 8,8\n";
  const AtlasText on = readAtlasText(base + "  rotate: true\n");
  CHECK(on.valid);
  CHECK_EQ(on.pages[0].regions[0].rotation, RegionRotation::kDeg90);

  const AtlasText off = readAtlasText(base + "  rotate: false\n");
  CHECK(off.valid);
  CHECK_EQ(off.pages[0].regions[0].rotation, RegionRotation::kNone);

  const AtlasText bad = readAtlasText(base + "  rotate: 90\n");
  CHECK(!bad.valid);
  CHECK(hasCode(bad, AtlasTextCode::kValueInvalid));
}

TEST("atlas-text", "multi page atlas keeps regions on their own page") {
  const std::string text =
      "p1.png\n"
      "size: 128,128\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 8,8\n"
      "\n"
      "p2.png\n"
      "size: 256,256\n"
      "\n"
      "b\n"
      "  xy: 1,1\n"
      "  size: 9,9\n"
      "\n"
      "c\n"
      "  xy: 2,2\n"
      "  size: 10,10\n";
  const AtlasText t = readAtlasText(text);
  CHECK(t.valid);
  CHECK_EQ(t.pages.size(), std::size_t{2});
  CHECK_EQ(t.pages[0].file, std::string{"p1.png"});
  CHECK_EQ(t.pages[0].regions.size(), std::size_t{1});
  CHECK_EQ(t.pages[1].file, std::string{"p2.png"});
  CHECK_EQ(t.pages[1].regions.size(), std::size_t{2});
  CHECK_EQ(t.pages[1].regions[1].name, std::string{"c"});
}

TEST("atlas-text", "page properties are parsed into IR and reported as warning") {
  const std::string text =
      "p.png\n"
      "size: 64,64\n"
      "format: RGBA8888\n"
      "filter: Linear,Linear\n"
      "repeat: none\n"
      "pma: true\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 8,8\n";
  const AtlasText t = readAtlasText(text);
  CHECK(t.valid);
  const SourceAtlasPage& p = t.pages[0];
  CHECK(p.properties.format.has_value());
  CHECK_EQ(*p.properties.format, std::string{"RGBA8888"});
  CHECK(p.properties.filter.has_value());
  CHECK_EQ(*p.properties.filter, std::string{"Linear,Linear"});
  CHECK(p.properties.repeat.has_value());
  CHECK_EQ(*p.properties.repeat, std::string{"none"});
  CHECK(p.properties.pma.has_value());
  CHECK(*p.properties.pma);
  // 解析保留 ≠ 进入 Domain：仍必须给出 warning，不得静默吞掉。
  CHECK_EQ(countSeverity(t, AtlasTextSeverity::kWarning), std::size_t{4});
  CHECK(hasCode(t, AtlasTextCode::kFeatureUnsupported));
}

TEST("atlas-text", "region index is recorded; non -1 warns") {
  const std::string base =
      "p.png\n"
      "size: 64,64\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 8,8\n";
  const AtlasText none = readAtlasText(base + "  index: -1\n");
  CHECK(none.valid);
  CHECK(none.pages[0].regions[0].index.has_value());
  CHECK_EQ(*none.pages[0].regions[0].index, -1);
  CHECK_EQ(countSeverity(none, AtlasTextSeverity::kWarning), std::size_t{0});

  const AtlasText seq = readAtlasText(base + "  index: 3\n");
  CHECK(seq.valid);
  CHECK_EQ(*seq.pages[0].regions[0].index, 3);
  CHECK_EQ(countSeverity(seq, AtlasTextSeverity::kWarning), std::size_t{1});
}

TEST("atlas-text", "region missing xy or size is an error") {
  const std::string head = "p.png\nsize: 64,64\n\n";
  const AtlasText noXy = readAtlasText(head + "a\n  size: 8,8\n");
  CHECK(!noXy.valid);
  CHECK(hasCode(noXy, AtlasTextCode::kValueInvalid));

  const AtlasText noSize = readAtlasText(head + "a\n  xy: 0,0\n");
  CHECK(!noSize.valid);
  CHECK(hasCode(noSize, AtlasTextCode::kValueInvalid));
}

TEST("atlas-text", "region before any page is an error") {
  const AtlasText t = readAtlasText("a\n  xy: 0,0\n  size: 8,8\n");
  CHECK(!t.valid);
  CHECK(hasCode(t, AtlasTextCode::kSyntax));
}

TEST("atlas-text", "empty text and empty page are errors") {
  const AtlasText empty = readAtlasText("");
  CHECK(!empty.valid);
  CHECK(hasCode(empty, AtlasTextCode::kSyntax));

  const AtlasText noPage = readAtlasText("p.png\nsize: 64,64\n");
  CHECK(!noPage.valid);
  CHECK(hasCode(noPage, AtlasTextCode::kSyntax));
}

TEST("atlas-text", "page without size is an error") {
  const AtlasText t = readAtlasText("p.png\nformat: RGBA8888\n\na\n  xy: 0,0\n  size: 8,8\n");
  CHECK(!t.valid);
  CHECK(hasCode(t, AtlasTextCode::kValueInvalid));
}

TEST("atlas-text", "non positive page size is an error") {
  const AtlasText t = readAtlasText("p.png\nsize: 0,64\n\na\n  xy: 0,0\n  size: 8,8\n");
  CHECK(!t.valid);
  CHECK(hasCode(t, AtlasTextCode::kValueInvalid));
}

TEST("atlas-text", "unindented region property is an error") {
  const AtlasText t = readAtlasText("p.png\nsize: 64,64\n\na\nxy: 0,0\n  size: 8,8\n");
  CHECK(!t.valid);
  CHECK(hasCode(t, AtlasTextCode::kSyntax));
}

TEST("atlas-text", "unity TextAsset wrapper is stripped as transport metadata") {
  // 真实 Unity TextAsset 的包装里，页名在 `m_Script = "` 的**下一行**。
  const std::string wrapped =
      "TextAsset\n"
      "string m_Script = \"\n"
      "p.png\n"
      "size: 64,64\n"
      "\n"
      "a\n"
      "  xy: 0,0\n"
      "  size: 8,8\n\"";
  const AtlasText t = readAtlasText(wrapped);
  CHECK(t.valid);
  CHECK_EQ(t.pages.size(), std::size_t{1});
  CHECK_EQ(t.pages[0].regions.size(), std::size_t{1});
}

TEST("atlas-text", "code strings are stable and non empty") {
  CHECK(std::string(loongbones::importers::spine32::toString(AtlasTextCode::kSyntax)).size() >= 2);
  CHECK(std::string(loongbones::importers::spine32::toString(
            AtlasTextCode::kValueInvalid)) !=
        loongbones::importers::spine32::toString(AtlasTextCode::kSyntax));
}
