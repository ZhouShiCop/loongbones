// 格式探针（Task 3 阶段 A）：严格解析 + native-project-v1 契约校验。
//
// 作用：证明 RFC（docs/format/native-project-v1-rfc.md）描述的每一条约束都是
// **可判定的**——能用程序区分「合法 / 非法」，并给出可定位诊断。
//
// 边界：这是**探针**，不是 codec。它不做领域解码（不产出 ProjectData）、
//       不做写出、不依赖任何第三方库或 Qt。阶段 B 才实现 codec。
//
// 用法：
//   format_probe <file.json> [<file.json> ...]   # 逐个校验，退出码 0=全合法
//   format_probe --selftest <fixtures-root>      # 对 fixtures/ 做回归，可复现

#include "json.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

// ---- 诊断码 ----------------------------------------------------------------
// 阶段 A 未把这些码加入 domain 枚举（RFC §9 提案待批准）。
// 领域不变量失败处复用**既有** domain 码，如 ID_MALFORMED / PAGE_NOT_IN_ATLAS。

constexpr const char* kJsonSyntaxError = "JSON_SYNTAX_ERROR";
constexpr const char* kJsonTypeMismatch = "JSON_TYPE_MISMATCH";
constexpr const char* kJsonInvalidUtf8 = "JSON_INVALID_UTF8";
constexpr const char* kJsonDuplicateKey = "JSON_DUPLICATE_KEY";
constexpr const char* kJsonNonFiniteNumber = "JSON_NON_FINITE_NUMBER";
constexpr const char* kJsonDepthLimit = "JSON_DEPTH_LIMIT";
constexpr const char* kJsonSizeLimit = "JSON_SIZE_LIMIT";

constexpr const char* kFormatIdUnsupported = "FORMAT_ID_UNSUPPORTED";
constexpr const char* kFormatVersionUnsupported = "FORMAT_VERSION_UNSUPPORTED";
constexpr const char* kFormatTooNew = "FORMAT_TOO_NEW";
constexpr const char* kCapabilityUnsupported = "CAPABILITY_UNSUPPORTED";
constexpr const char* kFormatMissingField = "FORMAT_MISSING_FIELD";
constexpr const char* kFormatReadFailed = "FORMAT_READ_FAILED";

constexpr const char* kDomainValidationFailed = "DOMAIN_VALIDATION_FAILED";
constexpr const char* kAssetNameDuplicate = "ASSET_NAME_DUPLICATE";
constexpr const char* kIdMalformed = "ID_MALFORMED";
constexpr const char* kIdDuplicate = "ID_DUPLICATE";
constexpr const char* kAssetNotFound = "ASSET_NOT_FOUND";
constexpr const char* kRegionNotFound = "REGION_NOT_FOUND";
constexpr const char* kTextureAssetNotFound = "TEXTURE_ASSET_NOT_FOUND";
constexpr const char* kPageNotInAtlas = "PAGE_NOT_IN_ATLAS";

constexpr int kReaderMaxVersion = 1;
constexpr const char* kNativeFormatId = "loongbones.native-project";

// ---- ULID（与 domain/src/ids.cpp 同规则）-------------------------------------

constexpr char kCrockfordBase32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr std::size_t kUlidLength = 26;

int decodeCrockford(char c) noexcept {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  for (std::size_t i = 0; kCrockfordBase32[i] != '\0'; ++i) {
    if (kCrockfordBase32[i] == c) return static_cast<int>(i);
  }
  return -1;
}

bool isWellFormedUlid(std::string_view text) noexcept {
  if (text.size() != kUlidLength) return false;
  const int first = decodeCrockford(text[0]);
  if (first < 0 || first > 7) return false;
  for (std::size_t i = 1; i < text.size(); ++i) {
    if (decodeCrockford(text[i]) < 0) return false;
  }
  return true;
}

// ---- 结构 ------------------------------------------------------------------

struct Diag {
  std::string code;
  std::string location;
  std::string message;
};

std::string formatLoc(const std::string& pointer, std::size_t offset) {
  return pointer + "@" + std::to_string(offset);
}

using lbfmt::JsonValue;

/// 取对象某字段的字符串值；缺失或非字符串返回 nullptr
const std::string* strField(const JsonValue& obj, const char* field) {
  const JsonValue* v = obj.find(field);
  if (v == nullptr || !v->isString()) return nullptr;
  return &v->stringValue();
}

bool isIntegerValue(const JsonValue& v) {
  return v.isNumber() &&
         v.numberValue() == static_cast<double>(static_cast<long long>(v.numberValue()));
}

class Probe {
 public:
  explicit Probe(std::string path) : path_(std::move(path)) {}

  std::vector<Diag> run(std::string_view text) {
    lbfmt::ParseError perr;
    std::optional<JsonValue> doc = lbfmt::parseStrict(text, lbfmt::Limits{}, &perr);
    if (!doc.has_value()) {
      diags_.push_back(Diag{perr.code, formatLoc(perr.pointer, perr.offset), perr.message});
      return diags_;
    }
    collectRootUnknown(*doc);
    checkDocument(*doc);
    return diags_;
  }

  const std::vector<std::string>& unknownFields() const noexcept { return unknown_; }

 private:
  std::string path_;
  std::vector<Diag> diags_;
  std::vector<std::string> unknown_;
  std::set<std::string> nodeIds_;
  std::set<std::string> assetIds_;
  std::set<std::string> textureIds_;
  std::set<std::string> atlasIds_;
  std::set<std::string> pageIds_;
  std::set<std::string> regionIds_;
  std::set<std::string> assetNames_;
  std::vector<std::pair<std::string, std::int32_t>> pageDims_;  // pageId -> (w,h)

  void add(const char* code, const std::string& loc, const std::string& msg) {
    diags_.push_back(Diag{code, loc, msg});
  }

  void collectRootUnknown(const JsonValue& doc) {
    if (!doc.isObject()) return;
    static const char* kKnown[] = {"formatId", "formatVersion", "minimumReaderVersion",
                                   "appVersion", "capabilities", "project"};
    collectUnknownKeys(doc, kKnown, 6, "");
  }

  void collectUnknownKeys(const JsonValue& obj, const char* const* known, std::size_t n,
                          const std::string& path) {
    if (!obj.isObject()) return;
    for (const auto& kv : obj.objectValue()) {
      bool k = false;
      for (std::size_t i = 0; i < n; ++i) {
        if (kv.first == known[i]) { k = true; break; }
      }
      if (!k) unknown_.push_back(path + "/" + kv.first);
    }
  }

  /// 声明一个 ID；重复即报 ID_DUPLICATE
  void claim(std::set<std::string>& space, const std::string& value, const std::string& loc) {
    if (value.empty()) return;
    if (!space.insert(value).second) {
      add(kIdDuplicate, loc, "ID 重复：" + value);
    }
  }

  /// 取字符串字段；缺失或非字符串返回空串
  static std::string strOrEmpty(const JsonValue& obj, const char* field) {
    const JsonValue* v = obj.find(field);
    if (v != nullptr && v->isString()) return v->stringValue();
    return std::string();
  }

  /// 取整数字段；缺失或非数值返回 0（调用方按需判空）
  static std::int64_t intField(const JsonValue& obj, const char* field) {
    const JsonValue* v = obj.find(field);
    if (v == nullptr || !v->isNumber()) return 0;
    return static_cast<std::int64_t>(v->numberValue());
  }

  bool isKnownEnum(const JsonValue& obj, const char* field, const char* const* allowed,
                   std::size_t n, const std::string& loc, const char* what) {
    const JsonValue* v = obj.find(field);
    if (v == nullptr) return true;  // 必需性由别处检查
    if (!v->isString()) {
      add(kJsonTypeMismatch, loc, std::string(what) + " 必须是字符串");
      return false;
    }
    for (std::size_t i = 0; i < n; ++i) {
      if (v->stringValue() == allowed[i]) return true;
    }
    std::string msg = std::string("非法") + what + "：" + v->stringValue() + "（允许 ";
    for (std::size_t i = 0; i < n; ++i) {
      if (i != 0) msg += "|";
      msg += allowed[i];
    }
    msg += "）";
    add(kJsonTypeMismatch, loc, msg);
    return false;
  }

  const JsonValue* requireArray(const JsonValue& obj, const char* field, const std::string& loc) {
    const JsonValue* v = obj.find(field);
    if (v == nullptr) {
      add(kFormatMissingField, loc, std::string("缺少必需字段 ") + field);
      return nullptr;
    }
    if (!v->isArray()) {
      add(kJsonTypeMismatch, loc, std::string(field) + " 必须是数组");
      return nullptr;
    }
    return v;
  }

  void requireString(const JsonValue& obj, const char* field, const std::string& loc,
                     const char* what) {
    const JsonValue* v = obj.find(field);
    if (v == nullptr) {
      add(kFormatMissingField, loc, std::string("缺少必需字段 ") + what);
    } else if (!v->isString()) {
      add(kJsonTypeMismatch, loc, std::string(what) + " 必须是字符串");
    }
  }

  void checkUlid(const JsonValue& obj, const char* field, const std::string& loc,
                 const char* what) {
    const JsonValue* v = obj.find(field);
    if (v == nullptr) {
      add(kFormatMissingField, loc, std::string("缺少必需字段 ") + what);
    } else if (!v->isString()) {
      add(kJsonTypeMismatch, loc, std::string(what) + " 必须是字符串");
    } else if (!isWellFormedUlid(v->stringValue())) {
      add(kIdMalformed, loc, std::string("非法 ULID：") + v->stringValue());
    }
  }

  /// 可选整数：允许缺失或 null，出现则必须是整数
  void checkOptInt(const JsonValue& obj, const char* field, const std::string& loc) {
    const JsonValue* v = obj.find(field);
    if (v == nullptr || v->isNull()) return;
    if (!v->isNumber() || !isIntegerValue(*v)) {
      add(kJsonTypeMismatch, loc, std::string(field) + " 必须是整数或 null");
    }
  }

  /// 必需整数且 >= min
  void checkIntMin(const JsonValue& obj, const char* field, std::int64_t min,
                   const std::string& loc) {
    const JsonValue* v = obj.find(field);
    if (v == nullptr) {
      add(kFormatMissingField, loc, std::string("缺少必需字段 ") + field);
      return;
    }
    if (!v->isNumber() || !isIntegerValue(*v)) {
      add(kJsonTypeMismatch, loc, std::string(field) + " 必须是整数");
      return;
    }
    const std::int64_t value = static_cast<std::int64_t>(v->numberValue());
    if (value < min) {
      add(kDomainValidationFailed, loc,
          std::string(field) + " 非法：" + std::to_string(value));
    }
  }

  void checkStorage(const JsonValue& obj, const std::string& loc) {
    static const char* kAllowed[] = {"external", "inProject"};
    isKnownEnum(obj, "storage", kAllowed, 2, loc + "/storage", "存储模式");
  }

  void checkDocument(const JsonValue& doc) {
    if (!doc.isObject()) {
      add(kJsonTypeMismatch, "/", "文档根必须是 JSON 对象");
      return;
    }
    const lbfmt::JsonValue* fid = doc.find("formatId");
    if (fid == nullptr) {
      add(kFormatMissingField, "/formatId", "缺少必需字段 formatId");
    } else if (!fid->isString()) {
      add(kJsonTypeMismatch, "/formatId", "formatId 必须是字符串");
    } else if (fid->stringValue() != kNativeFormatId) {
      add(kFormatIdUnsupported, "/formatId", "未知 formatId：" + fid->stringValue());
      return;  // 格式不匹配，整体拒绝，不做后续推断
    }

    const JsonValue* fv = doc.find("formatVersion");
    if (fv == nullptr) {
      add(kFormatMissingField, "/formatVersion", "缺少必需字段 formatVersion");
    } else if (!fv->isNumber() || !isIntegerValue(*fv)) {
      add(kJsonTypeMismatch, "/formatVersion", "formatVersion 必须是整数");
    } else if (const long long value = static_cast<long long>(fv->numberValue()); value != 1) {
      add(kFormatVersionUnsupported, "/formatVersion",
          "不支持的 formatVersion：" + std::to_string(value));
    }

    const JsonValue* mrv = doc.find("minimumReaderVersion");
    if (mrv == nullptr) {
      add(kFormatMissingField, "/minimumReaderVersion", "缺少必需字段 minimumReaderVersion");
    } else if (!mrv->isNumber() || !isIntegerValue(*mrv)) {
      add(kJsonTypeMismatch, "/minimumReaderVersion", "minimumReaderVersion 必须是整数");
    } else if (static_cast<long long>(mrv->numberValue()) > kReaderMaxVersion) {
      add(kFormatTooNew, "/minimumReaderVersion",
          "文档要求更高的 Reader 版本：" +
              std::to_string(static_cast<long long>(mrv->numberValue())));
    }

    if (const JsonValue* appv = doc.find("appVersion");
        appv != nullptr && !appv->isString()) {
      add(kJsonTypeMismatch, "/appVersion", "appVersion 必须是字符串");
    }

    checkCapabilities(doc);

    const JsonValue* proj = doc.find("project");
    if (proj == nullptr) {
      add(kFormatMissingField, "/project", "缺少必需字段 project");
      return;
    }
    if (!proj->isObject()) {
      add(kJsonTypeMismatch, "/project", "project 必须是对象");
      return;
    }
    checkProject(*proj);
  }

  void checkCapabilities(const JsonValue& doc) {
    const JsonValue* caps = doc.find("capabilities");
    if (caps == nullptr) return;
    if (!caps->isObject()) {
      add(kJsonTypeMismatch, "/capabilities", "capabilities 必须是对象");
      return;
    }
    const JsonValue* req = caps->find("required");
    if (req == nullptr) return;
    if (!req->isArray()) {
      add(kJsonTypeMismatch, "/capabilities/required", "required 必须是数组");
      return;
    }
    // v1 基线之外不支持任何必需能力
    for (const JsonValue& item : req->arrayValue()) {
      if (!item.isString()) {
        add(kJsonTypeMismatch, "/capabilities/required", "required 元素必须是字符串");
      } else {
        add(kCapabilityUnsupported, "/capabilities/required",
            "未知必需能力：" + item.stringValue());
      }
    }
  }

  void checkProject(const lbfmt::JsonValue& proj) {
    checkUlid(proj, "id", "/project/id", "project.id");
    requireString(proj, "name", "/project/name", "project.name");
    checkUlid(proj, "rootNodeId", "/project/rootNodeId", "project.rootNodeId");

    const JsonValue* dsi = proj.find("defaultSceneIndex");
    if (dsi == nullptr) {
      add(kFormatMissingField, "/project/defaultSceneIndex",
          "缺少必需字段 project.defaultSceneIndex");
    } else if (!dsi->isNumber() || !isIntegerValue(*dsi)) {
      add(kJsonTypeMismatch, "/project/defaultSceneIndex", "defaultSceneIndex 必须是整数");
    } else if (static_cast<long long>(dsi->numberValue()) != 0) {
      add(kDomainValidationFailed, "/project/defaultSceneIndex",
          "单场景契约下 defaultSceneIndex 必须为 0");
    }

    const JsonValue* scenes = requireArray(proj, "scenes", "/project/scenes");
    const JsonValue* nodes = requireArray(proj, "nodes", "/project/nodes");
    const JsonValue* order = requireArray(proj, "nodeOrder", "/project/nodeOrder");
    const JsonValue* assets = requireArray(proj, "assets", "/project/assets");
    const JsonValue* textures = requireArray(proj, "textures", "/project/textures");
    const JsonValue* atlases = requireArray(proj, "atlases", "/project/atlases");

    // 先收集节点 ID（供场景根与父子引用闭合判定）
    if (nodes != nullptr) {
      const std::size_t n = nodes->arrayValue().size();
      for (std::size_t i = 0; i < n; ++i) {
        const JsonValue& nd = nodes->arrayValue()[i];
        if (nd.isObject()) {
          claim(nodeIds_, strOrEmpty(nd, "id"),
                "/project/nodes/" + std::to_string(i) + "/id");
        }
      }
    }

    // 再收集资源层 ID，供节点引用闭合判定
    if (assets != nullptr) {
      const std::size_t n = assets->arrayValue().size();
      for (std::size_t i = 0; i < n; ++i) checkAsset(assets->arrayValue()[i], i);
    }
    if (textures != nullptr) {
      const std::size_t n = textures->arrayValue().size();
      for (std::size_t i = 0; i < n; ++i) checkTexture(textures->arrayValue()[i], i);
    }
    if (atlases != nullptr) {
      const std::size_t n = atlases->arrayValue().size();
      for (std::size_t i = 0; i < n; ++i) checkAtlas(atlases->arrayValue()[i], i);
    }



    // 节点（含引用闭合）
    if (nodes != nullptr) {
      const std::size_t n = nodes->arrayValue().size();
      std::size_t roots = 0;
      for (std::size_t i = 0; i < n; ++i) {
        const JsonValue& nd = nodes->arrayValue()[i];
        checkNode(nd, i);
        if (nd.isObject()) {
          if (const JsonValue* t = nd.find("type");
              t != nullptr && t->isString() && t->stringValue() == "root") {
            ++roots;
          }
        }
      }
      if (roots == 0) {
        add(kDomainValidationFailed, "/project/nodes", "缺少 kRoot 根节点");
      } else if (roots > 1) {
        add(kDomainValidationFailed, "/project/nodes",
            "工程中存在多个根节点（root 数量 = " + std::to_string(roots) + "）");
      }
      if (order != nullptr && order->arrayValue().size() != n) {
        add(kDomainValidationFailed, "/project/nodeOrder",
            "nodeOrder 长度与 nodes 不一致：" +
                std::to_string(order->arrayValue().size()) + " != " + std::to_string(n));
      }
    }

    // 场景与根一致性（RFC §2.2）：依赖上面收集到的 nodeIds_
    if (scenes != nullptr) {
      const std::size_t n = scenes->arrayValue().size();
      if (n != 1) {
        add(kDomainValidationFailed, "/project/scenes",
            "单场景契约要求 scenes 长度恰为 1，实际为 " + std::to_string(n));
      }
      for (std::size_t i = 0; i < n; ++i) checkScene(scenes->arrayValue()[i], i);

      const JsonValue* rootId = proj.find("rootNodeId");
      if (n == 1 && scenes->arrayValue()[0].isObject()) {
        const JsonValue* sroot = scenes->arrayValue()[0].find("rootNodeId");
        if (rootId != nullptr && rootId->isString() && sroot != nullptr && sroot->isString() &&
            rootId->stringValue() != sroot->stringValue()) {
          add(kDomainValidationFailed, "/project/rootNodeId",
              "默认场景根与兼容字段 rootNodeId 不一致");
        }
        if (sroot != nullptr && sroot->isString() && !sroot->stringValue().empty() &&
            nodeIds_.count(sroot->stringValue()) == 0) {
          add(kDomainValidationFailed, "/project/scenes/0/rootNodeId",
              "场景根指向缺失节点");
        }
      }
    }
  }

  void checkScene(const lbfmt::JsonValue& scene, std::size_t index) {
    const std::string base = "/project/scenes/" + std::to_string(index);
    if (!scene.isObject()) {
      add(kJsonTypeMismatch, base, "scenes 元素必须是对象");
      return;
    }
    checkUlid(scene, "id", base + "/id", "scenes[].id");
    requireString(scene, "name", base + "/name", "scenes[].name");
    checkUlid(scene, "rootNodeId", base + "/rootNodeId", "scenes[].rootNodeId");
  }

  void checkAsset(const lbfmt::JsonValue& a, std::size_t index) {
    const std::string base = "/project/assets/" + std::to_string(index);
    if (!a.isObject()) {
      add(kJsonTypeMismatch, base, "assets 元素必须是对象");
      return;
    }
    checkUlid(a, "id", base + "/id", "assets[].id");
    claim(assetIds_, strOrEmpty(a, "id"), base + "/id");
    requireString(a, "name", base + "/name", "assets[].name");
    if (const JsonValue* n = a.find("name"); n != nullptr && n->isString()) {
      if (!assetNames_.insert(n->stringValue()).second) {
        add(kAssetNameDuplicate, base + "/name", "资源名重复：" + n->stringValue());
      }
    }
    static const char* kTypes[] = {"static"};
    isKnownEnum(a, "type", kTypes, 1, base + "/type", "资源类型");
    checkStorage(a, base);
    requireString(a, "location", base + "/location", "assets[].location");
    checkOptInt(a, "pixelWidth", base + "/pixelWidth");
    checkOptInt(a, "pixelHeight", base + "/pixelHeight");
  }

  void checkTexture(const lbfmt::JsonValue& t, std::size_t index) {
    const std::string base = "/project/textures/" + std::to_string(index);
    if (!t.isObject()) {
      add(kJsonTypeMismatch, base, "textures 元素必须是对象");
      return;
    }
    checkUlid(t, "id", base + "/id", "textures[].id");
    claim(textureIds_, strOrEmpty(t, "id"), base + "/id");
    requireString(t, "name", base + "/name", "textures[].name");
    checkStorage(t, base);
    requireString(t, "location", base + "/location", "textures[].location");
    checkOptInt(t, "pixelWidth", base + "/pixelWidth");
    checkOptInt(t, "pixelHeight", base + "/pixelHeight");
  }

  void checkAtlas(const lbfmt::JsonValue& a, std::size_t index) {
    const std::string base = "/project/atlases/" + std::to_string(index);
    if (!a.isObject()) {
      add(kJsonTypeMismatch, base, "atlases 元素必须是对象");
      return;
    }
    checkUlid(a, "id", base + "/id", "atlases[].id");
    claim(atlasIds_, strOrEmpty(a, "id"), base + "/id");
    requireString(a, "name", base + "/name", "atlases[].name");

    // 本图集的页集合（用于区域引用闭合）
    std::set<std::string> pagesHere;
    std::map<std::string, std::pair<std::int32_t, std::int32_t>> dimsHere;

    const JsonValue* pages = requireArray(a, "pages", base + "/pages");
    if (pages != nullptr) {
      const std::size_t n = pages->arrayValue().size();
      for (std::size_t i = 0; i < n; ++i) {
        checkPage(pages->arrayValue()[i], base + "/pages/" + std::to_string(i), pagesHere, dimsHere);
      }
    }

    const JsonValue* regions = requireArray(a, "regions", base + "/regions");
    if (regions != nullptr) {
      const std::size_t n = regions->arrayValue().size();
      for (std::size_t i = 0; i < n; ++i) {
        checkRegion(regions->arrayValue()[i], base + "/regions/" + std::to_string(i), pagesHere, dimsHere);
      }
    }
  }

  void checkPage(const lbfmt::JsonValue& p, const std::string& base,
                 std::set<std::string>& pagesHere,
                 std::map<std::string, std::pair<std::int32_t, std::int32_t>>& dimsHere) {
    if (!p.isObject()) {
      add(kJsonTypeMismatch, base, "pages 元素必须是对象");
      return;
    }
    checkUlid(p, "id", base + "/id", "pages[].id");
    claim(pageIds_, strOrEmpty(p, "id"), base + "/id");
    requireString(p, "name", base + "/name", "pages[].name");
    checkUlid(p, "textureAssetId", base + "/textureAssetId", "pages[].textureAssetId");

    // 纹理引用闭合（跨全工程）
    if (const std::string* tid = strField(p, "textureAssetId"); tid != nullptr) {
      if (textureIds_.count(*tid) == 0) {
        add(kTextureAssetNotFound, base + "/textureAssetId", "页引用的纹理资产不存在：" + *tid);
      }
    }

    checkIntMin(p, "width", 1, base + "/width");
    checkIntMin(p, "height", 1, base + "/height");

    if (const std::string* pid = strField(p, "id"); pid != nullptr) {
      pagesHere.insert(*pid);
      dimsHere.emplace(*pid, std::make_pair(
        p.find("width") != nullptr && p.find("width")->isNumber()
            ? static_cast<std::int32_t>(p.find("width")->numberValue()) : 0,
        p.find("height") != nullptr && p.find("height")->isNumber()
            ? static_cast<std::int32_t>(p.find("height")->numberValue()) : 0));
    }
  }

  void checkRegion(const lbfmt::JsonValue& r, const std::string& base,
                   const std::set<std::string>& pagesHere,
                   const std::map<std::string, std::pair<std::int32_t, std::int32_t>>& dimsHere) {
    if (!r.isObject()) {
      add(kJsonTypeMismatch, base, "regions 元素必须是对象");
      return;
    }
    checkUlid(r, "id", base + "/id", "regions[].id");
    claim(regionIds_, strOrEmpty(r, "id"), base + "/id");
    requireString(r, "name", base + "/name", "regions[].name");
    checkUlid(r, "pageId", base + "/pageId", "regions[].pageId");

    // 尺寸与偏移
    checkIntMin(r, "packedW", 1, base + "/packedW");
    checkIntMin(r, "packedH", 1, base + "/packedH");
    checkIntMin(r, "originalW", 1, base + "/originalW");
    checkIntMin(r, "originalH", 1, base + "/originalH");
    checkIntMin(r, "trimmedW", 1, base + "/trimmedW");
    checkIntMin(r, "trimmedH", 1, base + "/trimmedH");
    checkIntMin(r, "offsetX", 0, base + "/offsetX");
    checkIntMin(r, "offsetY", 0, base + "/offsetY");

    // 旋转枚举
    if (const JsonValue* rot = r.find("rotation"); rot != nullptr) {
      if (!rot->isString()) {
        add(kJsonTypeMismatch, base + "/rotation", "rotation 必须是字符串");
      } else {
        static const char* kAllowed[] = {"none", "90", "180", "270"};
        bool ok = false;
        for (const char* k : kAllowed) {
          if (rot->stringValue() == k) { ok = true; break; }
        }
        if (!ok) {
          add(kJsonTypeMismatch, base + "/rotation",
            "非法旋转值：" + rot->stringValue() + "（允许 none|90|180|270）");
        }
      }
    }

    // trim 边界（方向与 original 一致，**不**随打包旋转转置）
    const std::int64_t ox = intField(r, "offsetX");
    const std::int64_t oy = intField(r, "offsetY");
    const std::int64_t tw = intField(r, "trimmedW");
    const std::int64_t th = intField(r, "trimmedH");
    const std::int64_t ow = intField(r, "originalW");
    const std::int64_t oh = intField(r, "originalH");
    if (ow > 0 && oh > 0) {
      if (ox + tw > ow || oy + th > oh) {
        add(kDomainValidationFailed, base,
            "trim 偏移叠加裁切尺寸超出原始尺寸（offset+trimmed 必须落在 original 内）");
      }
    }

    // 页引用必须属于同一图集
    const std::string* pid = strField(r, "pageId");
    if (pid != nullptr && !pid->empty() && pagesHere.count(*pid) == 0) {
      add(kPageNotInAtlas, base + "/pageId", "区域引用的页不属于同一图集：" + *pid);
      return;
    }
    // 页内占位（90/270 转置），int64 防溢出
    if (pid != nullptr) {
      const auto it = dimsHere.find(*pid);
      if (it == dimsHere.end()) return;
      const std::int64_t pw = it->second.first;
      const std::int64_t ph = it->second.second;
      const std::string rs = r.find("rotation") != nullptr && r.find("rotation")->isString()
                                  ? r.find("rotation")->stringValue()
                                  : std::string("none");
      const bool swap = (rs == "90" || rs == "270");
      const std::int64_t occW = swap ? intField(r, "packedH") : intField(r, "packedW");
      const std::int64_t occH = swap ? intField(r, "packedW") : intField(r, "packedH");
      const std::int64_t px = intField(r, "packedX");
      const std::int64_t py = intField(r, "packedY");
      if (px < 0 || py < 0 || occW <= 0 || occH <= 0 || px + occW > pw || py + occH > ph) {
        add(kDomainValidationFailed, base + "/packedX", "区域矩形越出页边界");
      }
    }
  }

  // ---- 节点 ----

  void checkNode(const lbfmt::JsonValue& node, std::size_t index) {
    const std::string base = "/project/nodes/" + std::to_string(index);
    if (!node.isObject()) {
      add(kJsonTypeMismatch, base, "nodes 元素必须是对象");
    } else {
      checkUlid(node, "id", base + "/id", "nodes[].id");
      requireString(node, "name", base + "/name", "nodes[].name");

      const JsonValue* parent = node.find("parent");
      if (parent == nullptr) {
        add(kFormatMissingField, base + "/parent", "缺少必需字段 nodes[].parent");
      } else if (!parent->isNull() && !parent->isString()) {
        add(kJsonTypeMismatch, base + "/parent", "parent 必须是 NodeId 字符串或 null");
      } else if (parent->isString() && !isWellFormedUlid(parent->stringValue())) {
        add(kIdMalformed, base + "/parent", "非法 ULID：" + parent->stringValue());
      }

      const JsonValue* children = node.find("children");
      if (children == nullptr) {
        add(kFormatMissingField, base + "/children", "缺少必需字段 nodes[].children");
      } else if (!children->isArray()) {
        add(kJsonTypeMismatch, base + "/children", "children 必须是数组");
      } else {
        std::set<std::string> seen;
        for (const JsonValue& c : children->arrayValue()) {
          if (!c.isString()) {
            add(kJsonTypeMismatch, base + "/children", "children 元素必须是 NodeId 字符串");
            continue;
          }
          if (!isWellFormedUlid(c.stringValue())) {
            add(kIdMalformed, base + "/children", "非法 ULID：" + c.stringValue());
          }
          if (!seen.insert(c.stringValue()).second) {
            add(kDomainValidationFailed, base + "/children",
                "children 含重复子引用：" + c.stringValue());
          }
        }
      }

      const JsonValue* t = node.find("transform");
      if (t == nullptr) {
        add(kFormatMissingField, base + "/transform", "缺少必需字段 nodes[].transform");
      } else if (!t->isObject()) {
        add(kJsonTypeMismatch, base + "/transform", "transform 必须是对象");
      } else {
        checkTransform(*t, base + "/transform");
      }

      if (const JsonValue* v = node.find("visible"); v != nullptr && !v->isBool()) {
        add(kJsonTypeMismatch, base + "/visible", "visible 必须是布尔");
      }
      if (const JsonValue* l = node.find("locked"); l != nullptr && !l->isBool()) {
        add(kJsonTypeMismatch, base + "/locked", "locked 必须是布尔");
      }

      const std::string* type = strField(node, "type");
      if (type == nullptr) {
        add(kFormatMissingField, base + "/type", "缺少必需字段 nodes[].type");
      } else {
        static const char* kNodeTypes[] = {"root", "group", "image"};
        isKnownEnum(node, "type", kNodeTypes, 3, base + "/type", "节点种类");

        const bool hasAsset = node.find("assetId") != nullptr && node.find("assetId")->isString();
        const bool hasRegion = node.find("regionId") != nullptr && node.find("regionId")->isString();
        if (const JsonValue* av = node.find("assetId");
            av != nullptr && !av->isNull() && !av->isString()) {
          add(kJsonTypeMismatch, base + "/assetId", "assetId 必须是 ImageAssetId 字符串或 null");
        } else if (const JsonValue* rv = node.find("regionId");
                   rv != nullptr && !rv->isNull() && !rv->isString()) {
          add(kJsonTypeMismatch, base + "/regionId", "regionId 必须是 AtlasRegionId 字符串或 null");
        } else if (*type == "image") {
          if (hasAsset == hasRegion) {
            add(kDomainValidationFailed, base,
                "图片节点必须且只能持有一个内容源（assetId 与 regionId 恰好其一非空）");
          }
          if (hasAsset) {
            const std::string* aid = strField(node, "assetId");
            if (aid != nullptr && assetIds_.count(*aid) == 0) {
              add(kAssetNotFound, base + "/assetId", "引用的资源不存在：" + *aid);
            }
          }
          if (hasRegion) {
            const std::string* rid = strField(node, "regionId");
            if (rid != nullptr && regionIds_.count(*rid) == 0) {
              add(kRegionNotFound, base + "/regionId", "引用的区域不存在：" + *rid);
            }
          }
        } else if (hasAsset || hasRegion) {
          add(kDomainValidationFailed, base, "非图片节点（root/group）不得持有 assetId 或 regionId");
        }
      }
    }
  }

  void checkTransform(const lbfmt::JsonValue& t, const std::string& base) {
    static const char* kFields[] = {"x", "y", "rotationDeg", "scaleX", "scaleY"};
    for (const char* k : kFields) {
      const JsonValue* v = t.find(k);
      if (v == nullptr) {
        add(kFormatMissingField, base + "/" + k, std::string("缺少必需字段 transform.") + k);
      } else if (!v->isNumber()) {
        add(kJsonTypeMismatch, base + "/" + k, std::string("transform.") + k + " 必须是数值");
      } else if (!std::isfinite(v->numberValue())) {
        add(kJsonNonFiniteNumber, base + "/" + k, std::string("transform.") + k + " 必须是有限数");
      }
    }
  }
};

// ---- fixture 自检 -----------------------------------------------------------

/// 期望码映射（fixture 文件名 stem -> 期望的首个诊断码）
const char* expectedCodeFor(const std::string& stem) {
  static const std::pair<const char*, const char*> kMap[] = {
      {"syntax-truncated", kJsonSyntaxError},
      {"format-id-unknown", kFormatIdUnsupported},
      {"version-unsupported", kFormatVersionUnsupported},
      {"minimum-reader-too-new", kFormatTooNew},
      {"capability-required-unknown", kCapabilityUnsupported},
      {"type-mismatch", kJsonTypeMismatch},
      {"missing-required", kFormatMissingField},
      {"enum-invalid", kJsonTypeMismatch},
      {"id-invalid", kIdMalformed},
      {"duplicate-key", kJsonDuplicateKey},
      {"nan-literal", kJsonNonFiniteNumber},
      {"domain-inconsistent", kPageNotInAtlas},
  };
  for (const auto& kv : kMap) {
    if (stem == kv.first) return kv.second;
  }
  return nullptr;
}

bool readFileBytes(const std::filesystem::path& p, std::string& out) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

int runSelftest(const std::filesystem::path& root) {
  int failures = 0;
  int checked = 0;

  const std::filesystem::path validDir = root / "valid";
  if (std::filesystem::exists(validDir)) {
    for (const auto& entry : std::filesystem::directory_iterator(validDir)) {
      if (entry.path().extension() != ".json") continue;
      ++checked;
      std::string bytes;
      if (!readFileBytes(entry.path(), bytes)) {
        std::cout << "FAIL(read) " << entry.path().string() << "\n";
        ++failures;
        continue;
      }
      Probe probe(entry.path().string());
      const std::vector<Diag> diags = probe.run(bytes);
      if (diags.empty()) {
        std::cout << "PASS " << entry.path().string() << " (合法)\n";
      } else {
        std::cout << "FAIL " << entry.path().string() << " (期望合法，实际: ";
        for (const Diag& d : diags) std::cout << d.code << "@" << d.location << " ";
        std::cout << ")\n";
        ++failures;
      }
    }
  }

  const std::filesystem::path invalidDir = root / "invalid";
  if (std::filesystem::exists(invalidDir)) {
    for (const auto& entry : std::filesystem::directory_iterator(invalidDir)) {
      if (entry.path().extension() != ".json") continue;
      ++checked;
      const std::string stem = entry.path().stem().string();
      const char* expected = expectedCodeFor(stem);
      if (expected == nullptr) {
        std::cout << "SKIP " << entry.path().string() << " (未登记期望码)\n";
        continue;
      }
      std::string bytes;
      if (!readFileBytes(entry.path(), bytes)) {
        std::cout << "FAIL(read) " << entry.path().string() << "\n";
        ++failures;
        continue;
      }
      Probe probe(entry.path().string());
      const std::vector<Diag> diags = probe.run(bytes);
      if (diags.empty()) {
        std::cout << "FAIL " << entry.path().string() << " (期望 " << expected
                  << "，实际判定为合法)\n";
        ++failures;
        continue;
      }
      if (diags.front().code == expected) {
        std::cout << "PASS " << entry.path().string() << " (" << expected << ")\n";
      } else {
        std::cout << "FAIL " << entry.path().string() << " (期望 " << expected << "，实际 "
                  << diags.front().code << ")\n";
        ++failures;
      }
    }
  }

  std::cout << "selftest checked=" << checked << " fail=" << failures << "\n";
  return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  std::ios::sync_with_stdio(false);

  if (argc >= 3 && std::string(argv[1]) == "--selftest") {
    return runSelftest(argv[2]);
  }
  if (argc < 2) {
    std::cout << "usage: format_probe <file.json>...\n";
    std::cout << "       format_probe --selftest <fixtures-root>\n";
    return 2;
  }

  int bad = 0;
  for (int i = 1; i < argc; ++i) {
    const std::string path = argv[i];
    std::string bytes;
    if (!readFileBytes(path, bytes)) {
      std::cout << "FAIL " << path << "\n  " << kFormatReadFailed
                << "  /  " << "无法读取文件\n";
      ++bad;
      continue;
    }
    Probe probe(path);
    const std::vector<Diag> diags = probe.run(bytes);
    if (diags.empty()) {
      std::cout << "PASS " << path << "\n";
      for (const std::string& uf : probe.unknownFields()) {
        std::cout << "  INFO 未知可选字段（应保留）: " << uf << "\n";
      }
      continue;
    }
    std::cout << "FAIL " << path << "\n";
    for (const Diag& d : diags) {
      std::cout << "  " << d.code << "  " << d.location << "  " << d.message << "\n";
    }
    ++bad;
  }
  return bad == 0 ? 0 : 1;
}
