#include "loongbones/format/codec.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "loongbones/format/json_value.hpp"

namespace loongbones {
namespace format {
namespace {

// ---- 已知键集合：用于把「未识别的键」识别为未知字段（RFC §6.2）----
constexpr std::string_view kTopKeys[] = {"formatId",   "formatVersion", "minimumReaderVersion",
                                         "appVersion", "capabilities",  "project"};
constexpr std::string_view kProjectKeys[] = {"id",      "name",     "rootNodeId",
                                             "defaultSceneIndex", "scenes",   "nodes",
                                             "nodeOrder", "assets", "textures", "atlases"};
/// v2 的 project 键：**追加** skeletons，其余与 v1 完全一致。
///
/// 必须与 v1 分开：v1 文档若出现 `project.skeletons`，它会被当成未知字段收进
/// unknowns；迁移后又会被当成真字段写出，最终同一个 JSON 对象里出现两个
/// `skeletons` 键。故 v1 路径显式拒绝该字段（见 decodeProject）。
constexpr std::string_view kProjectV2Keys[] = {"id",      "name",     "rootNodeId",
                                               "defaultSceneIndex", "scenes",   "nodes",
                                               "nodeOrder", "assets", "textures", "atlases",
                                               "skeletons"};
constexpr std::string_view kSceneKeys[] = {"id", "name", "rootNodeId"};
constexpr std::string_view kNodeKeys[] = {"id",       "name",    "parent",   "children",
                                          "transform", "visible", "locked",  "type",
                                          "assetId",  "regionId"};
constexpr std::string_view kTransformKeys[] = {"x", "y", "rotationDeg", "scaleX", "scaleY"};
constexpr std::string_view kAssetKeys[] = {"id",      "name",  "type",       "storage",
                                           "location", "pixelWidth", "pixelHeight"};
constexpr std::string_view kTextureKeys[] = {"id",    "name",       "storage",
                                             "location", "pixelWidth", "pixelHeight"};
constexpr std::string_view kAtlasKeys[] = {"id", "name", "pages", "regions"};
constexpr std::string_view kPageKeys[] = {"id", "name", "textureAssetId", "width", "height"};
constexpr std::string_view kRegionKeys[] = {"id",       "name",     "pageId",   "packedX",
                                            "packedY",  "packedW",  "packedH",  "rotation",
                                            "originalW", "originalH", "trimmedW", "trimmedH",
                                            "offsetX",  "offsetY"};
constexpr std::string_view kCapabilitiesKeys[] = {"required", "optional"};

// ---- 骨架层已知键（v2 schema §3）----
constexpr std::string_view kSkeletonKeys[] = {"id",        "name",  "atlasIds", "boneOrder",
                                              "bones",     "drawOrder", "slots",    "skins",
                                              "sourceRef", "ikConstraints", "transformConstraints"};
constexpr std::string_view kBoneKeys[] = {"id",               "name",     "parentId",
                                          "x",                "y",        "rotationDeg",
                                          "scaleX",           "scaleY",   "shearXDeg",
                                          "shearYDeg",        "inheritRotation",
                                          "inheritScale",     "length"};
constexpr std::string_view kSlotKeys[] = {"id",     "name",         "boneId",
                                          "color",  "attachmentName", "blendMode"};
constexpr std::string_view kColorKeys[] = {"r", "g", "b", "a"};
constexpr std::string_view kSkinKeys[] = {"id", "name", "entries"};
constexpr std::string_view kSkinEntryKeys[] = {"id", "slotId", "attachmentName", "attachment"};
/// 附件的已知键**按 type 分派**（Task 5 B2 评审 P0）。
/// 绝不能用并集：若把 mesh 的 `uvs`/`triangles`/`vertices` 也塞进 region 的已知键，
/// region 里的这些键会被当成**已知 mesh 字段去解析**（而非未知字段保留），从而污染 region 的几何数据。
/// 按 type 分派后，类型不符的键（如 region 里的 `uvs`）会落到未知字段通道 ——
/// 按 RFC 原样保留并回写（见 `region_attachment_with_mesh_only_field_is_kept_as_unknown`），既不误解析也不丢失。
constexpr std::string_view kRegionAttachmentKeys[] = {"type", "regionId", "x",      "y",
                                                      "rotationDeg",      "scaleX", "scaleY",
                                                      "width", "height", "color"};
constexpr std::string_view kMeshAttachmentKeys[] = {"type", "regionId", "uvs",  "triangles",
                                                    "vertices", "width", "height"};
constexpr std::string_view kWeightedMeshAttachmentKeys[] = {
    "type", "regionId", "uvs", "triangles", "vertexBindings", "width", "height"};
constexpr std::string_view kVertexBindingKeys[] = {"boneId", "x", "y", "weight"};
constexpr std::string_view kLinkedMeshAttachmentKeys[] = {"type", "regionId", "parentKey",
                                                          "inheritDeform"};
constexpr std::string_view kParentKeyKeys[] = {"slotId", "attachmentName"};

/// 第二批约束的已知键（v3 schema §3）。缺一项就会被 collectUnknown 收成未知字段，
/// 再被 emitUnknowns 回写成**重复键** —— 多一轮往返即失败。
constexpr std::string_view kIkConstraintKeys[] = {"id",       "name",   "boneIds", "targetBoneId",
                                                  "mix",      "enabled", "bendPositive"};
constexpr std::string_view kTransformConstraintKeys[] = {
    "id",           "name",      "boneId",     "targetBoneId",  "rotateMix", "translateMix",
    "scaleMix",     "shearMix",  "offsetRotation", "offsetX",  "offsetY",    "offsetScaleX",
    "offsetScaleY", "offsetShearY"};
constexpr std::string_view kSourceRefKeys[] = {"formatId",        "formatVersion",
                                               "sourceHeaderHash", "contentHash",
                                               "sourceUnit",       "importedBy"};
constexpr std::string_view kImportedByKeys[] = {"pluginId", "pluginVersion"};

template <std::size_t N>
bool isKnownKey(std::string_view key, const std::string_view (&known)[N]) {
  for (const std::string_view candidate : known) {
    if (candidate == key) return true;
  }
  return false;
}

/// 按文档版本选择的已知键集合（只有 project 需要版本化，见 kProjectV2Keys 注释）。
std::vector<std::string_view> projectKeysForVersion(int version) {
  const std::string_view* first = version >= 2 ? std::begin(kProjectV2Keys) : std::begin(kProjectKeys);
  const std::string_view* last = version >= 2 ? std::end(kProjectV2Keys) : std::end(kProjectKeys);
  return std::vector<std::string_view>(first, last);
}

std::string childPointer(const std::string& base, std::string_view key) {
  return base + "/" + escapePointerSegment(key);
}

std::string indexPointer(const std::string& base, std::size_t index) {
  return base + "/" + std::to_string(index);
}

bool decodeNodeType(const std::string& text, NodeType& out) {
  if (text == "root") {
    out = NodeType::kRoot;
    return true;
  }
  if (text == "group") {
    out = NodeType::kGroup;
    return true;
  }
  if (text == "image") {
    out = NodeType::kImage;
    return true;
  }
  return false;
}

bool decodeStorage(const std::string& text, StorageMode& out) {
  if (text == "external") {
    out = StorageMode::kExternalReference;
    return true;
  }
  if (text == "inProject") {
    out = StorageMode::kInProject;
    return true;
  }
  return false;
}

bool decodeAssetType(const std::string& text, ImageAssetType& out) {
  if (text == "static") {
    out = ImageAssetType::kStatic;
    return true;
  }
  return false;
}

bool decodeRotation(const std::string& text, RegionRotation& out) {
  if (text == "none") {
    out = RegionRotation::kNone;
    return true;
  }
  if (text == "90") {
    out = RegionRotation::kDeg90;
    return true;
  }
  if (text == "180") {
    out = RegionRotation::kDeg180;
    return true;
  }
  if (text == "270") {
    out = RegionRotation::kDeg270;
    return true;
  }
  return false;
}

/// 插槽混合模式枚举字符串（v2 schema §3.3）
bool decodeBlendMode(const std::string& text, BlendMode& out) {
  if (text == "normal") {
    out = BlendMode::kNormal;
    return true;
  }
  if (text == "additive") {
    out = BlendMode::kAdditive;
    return true;
  }
  if (text == "multiply") {
    out = BlendMode::kMultiply;
    return true;
  }
  if (text == "screen") {
    out = BlendMode::kScreen;
    return true;
  }
  return false;
}

class Decoder {
 public:
  LoadResult run(const std::string& text, const ReaderLimits& limits) {
    LoadResult out;

    const JsonParseResult parsed = parseJson(text, limits);
    if (!parsed.ok) {
      out.source = DiagSource::kFormat;
      out.formatDiag = parsed.diag;
      return out;
    }
    if (!decodeRoot(parsed.root)) {
      out.source = DiagSource::kFormat;
      out.formatDiag = diag_;
      return out;
    }

    // RFC §9.2：Format 全部通过后才交给 Domain。
    // ID 的格式合法性由 Domain 校验（kIdMalformed），Format 只保证存在性与类型。
    ProjectData candidate = data_;
    Project probe;
    const Status status = probe.adoptData(candidate);
    if (!status.isOk()) {
      out.source = DiagSource::kDomain;
      out.domainDiags.push_back(status.diagnostic());
      return out;
    }

    out.ok = true;
    out.document.data = std::move(data_);
    out.document.unknowns = std::move(unknowns_);
    out.document.hasCapabilities = hasCapabilities_;
    out.document.requiredCapabilities = std::move(requiredCaps_);
    out.document.optionalCapabilities = std::move(optionalCaps_);

    // 版本状态：读出时的版本。**先于**迁移写入，迁移再据此判断是否要改。
    out.document.sourceVersion =
        version_ == kLegacyV1Version
            ? SourceFormatVersion::kV1
            : (version_ == kLegacyV2Version ? SourceFormatVersion::kV2
                                            : SourceFormatVersion::kV3);
    // v1 文档已在上面完整解码 + Domain 校验通过（若版本 >= 2 本函数是幂等空操作），
    // 此刻才把它迁移为内存中的 v2 结构。**不碰磁盘**。
    migrateV1ToV2(out.document);
    // 迁移链 v1 → v2 → v3（Task 5 B2）：v3 只增结构，同样是内存内、不碰磁盘。
    migrateV2ToV3(out.document);
    return out;
  }

 private:
  bool fail(FormatDiagnosticCode code, std::string message, std::string location) {
    diag_ = FormatDiagnostic(code, std::move(message), std::move(location));
    failed_ = true;
    return false;
  }

  template <std::size_t N>
  void collectUnknown(const JsonValue& obj, const std::string_view (&known)[N],
                      const OwnerAnchor& anchor, const std::string& subPath) {
    std::vector<std::pair<std::string, JsonValue>> entries;
    for (const auto& member : obj.members) {
      if (isKnownKey(member.first, known)) continue;
      entries.emplace_back(member.first, member.second);
    }
    if (entries.empty()) return;
    unknowns_[UnknownKey{anchor, subPath}].entries = std::move(entries);
  }

  /// project 的已知键随版本变化，故需要在运行期选集合的同款重载。
  void collectUnknown(const JsonValue& obj, const std::vector<std::string_view>& known,
                      const OwnerAnchor& anchor, const std::string& subPath) {
    std::vector<std::pair<std::string, JsonValue>> entries;
    for (const auto& member : obj.members) {
      bool skip = false;
      for (const std::string_view candidate : known) {
        if (candidate == member.first) {
          skip = true;
          break;
        }
      }
      if (skip) continue;
      entries.emplace_back(member.first, member.second);
    }
    if (entries.empty()) return;
    unknowns_[UnknownKey{anchor, subPath}].entries = std::move(entries);
  }

  // ---- 原始取值 ----

  const JsonValue* mustHave(const JsonValue& obj, std::string_view key, const std::string& objPtr) {
    const JsonValue* v = obj.find(key);
    if (v == nullptr) {
      fail(FormatDiagnosticCode::kFormatMissingField, "缺必需字段：" + std::string(key),
           childPointer(objPtr, key));
      return nullptr;
    }
    return v;
  }

  const JsonValue* mustArray(const JsonValue& obj, std::string_view key,
                             const std::string& objPtr) {
    const JsonValue* v = mustHave(obj, key, objPtr);
    if (v == nullptr) return nullptr;
    if (!v->isArray()) {
      fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是数组", childPointer(objPtr, key));
      return nullptr;
    }
    return v;
  }

  bool readInt64Literal(const JsonValue& v, const std::string& ptr, std::int64_t& out) {
    switch (classifyInt64(v.numberRaw, out)) {
      case NumberLiteralKind::kOk:
        return true;
      case NumberLiteralKind::kOutOfRange:
        return fail(FormatDiagnosticCode::kJsonNumberOutOfRange,
                    "数值超出 int64 可表示范围：" + v.numberRaw, ptr);
      case NumberLiteralKind::kNotInteger:
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 integer", ptr);
      case NumberLiteralKind::kMalformed:
        return fail(FormatDiagnosticCode::kJsonSyntaxError,
                    "数值字面量非法：" + v.numberRaw, ptr);
    }
    return fail(FormatDiagnosticCode::kJsonSyntaxError, "数值无法解析：" + v.numberRaw, ptr);
  }

  bool readInt32Literal(const JsonValue& v, const std::string& ptr, std::int32_t& out) {
    std::int64_t wide = 0;
    if (!readInt64Literal(v, ptr, wide)) return false;
    if (wide < -2147483648LL || wide > 2147483647LL) {
      return fail(FormatDiagnosticCode::kJsonNumberOutOfRange,
                  "数值超出 int32 可表示范围：" + v.numberRaw, ptr);
    }
    out = static_cast<std::int32_t>(wide);
    return true;
  }

  bool readDoubleLiteral(const JsonValue& v, const std::string& ptr, double& out) {
    const auto status = parseDoubleStatus(v.numberRaw, out);
    if (status == DoubleParseStatus::kOutOfRange) {
      return fail(FormatDiagnosticCode::kJsonNumberOutOfRange,
                  "数值超出 double 可表示范围：" + v.numberRaw, ptr);
    }
    if (status == DoubleParseStatus::kMalformed) {
      return fail(FormatDiagnosticCode::kJsonSyntaxError, "数值字面量非法：" + v.numberRaw, ptr);
    }
    return true;
  }

  // ---- 具名字段读取 ----

  bool reqString(const JsonValue& obj, std::string_view key, const std::string& objPtr,
                 std::string& out) {
    const JsonValue* v = mustHave(obj, key, objPtr);
    if (v == nullptr) return false;
    const std::string ptr = childPointer(objPtr, key);
    if (!v->isString()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 string", ptr);
    }
    out = v->stringValue;
    return true;
  }

  bool reqInt64(const JsonValue& obj, std::string_view key, const std::string& objPtr,
                std::int64_t& out) {
    const JsonValue* v = mustHave(obj, key, objPtr);
    if (v == nullptr) return false;
    const std::string ptr = childPointer(objPtr, key);
    if (!v->isNumber()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 number", ptr);
    }
    return readInt64Literal(*v, ptr, out);
  }

  bool reqInt32(const JsonValue& obj, std::string_view key, const std::string& objPtr,
                std::int32_t& out) {
    const JsonValue* v = mustHave(obj, key, objPtr);
    if (v == nullptr) return false;
    const std::string ptr = childPointer(objPtr, key);
    if (!v->isNumber()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 number", ptr);
    }
    return readInt32Literal(*v, ptr, out);
  }

  // 可选整型：缺失保留 out 的原值（默认值）
  bool optInt32(const JsonValue& obj, std::string_view key, const std::string& objPtr,
                std::int32_t& out) {
    const JsonValue* v = obj.find(key);
    if (v == nullptr) return true;
    const std::string ptr = childPointer(objPtr, key);
    if (!v->isNumber()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 number", ptr);
    }
    return readInt32Literal(*v, ptr, out);
  }

  bool optInt32Optional(const JsonValue& obj, std::string_view key, const std::string& objPtr,
                        std::optional<std::int32_t>& out) {
    const JsonValue* v = obj.find(key);
    if (v == nullptr) return true;
    const std::string ptr = childPointer(objPtr, key);
    if (v->isNull()) {
      out.reset();
      return true;
    }
    if (!v->isNumber()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 number 或 null", ptr);
    }
    std::int32_t value = 0;
    if (!readInt32Literal(*v, ptr, value)) return false;
    out = value;
    return true;
  }

  bool optDouble(const JsonValue& obj, std::string_view key, const std::string& objPtr,
                 double& out) {
    const JsonValue* v = obj.find(key);
    if (v == nullptr) return true;
    const std::string ptr = childPointer(objPtr, key);
    if (!v->isNumber()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 number", ptr);
    }
    return readDoubleLiteral(*v, ptr, out);
  }

  // ---- 各对象 ----

  bool decodeRoot(const JsonValue& root) {
    if (!root.isObject()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "文档根必须是 JSON 对象", "");
    }

    std::string formatId;
    if (!reqString(root, "formatId", "", formatId)) return false;
    if (formatId != kFormatId) {
      return fail(FormatDiagnosticCode::kFormatIdUnsupported,
                  "formatId 不是 " + std::string(kFormatId), "/formatId");
    }

    // ---- 第 1 步：formatVersion（B3 第 2 条：版本 -> minimumReader -> capabilities）----
    std::int64_t formatVersion = 0;
    if (!reqInt64(root, "formatVersion", "", formatVersion)) return false;
    if (formatVersion < kReaderMinVersion || formatVersion > kReaderMaxVersion) {
      return fail(FormatDiagnosticCode::kFormatVersionUnsupported, "formatVersion 超出本 Reader 范围",
                  "/formatVersion");
    }
    // 后续解码分支据此选择；同时决定 document.sourceVersion
    version_ = static_cast<int>(formatVersion);

    // ---- 第 2 步：minimumReaderVersion ----
    std::int64_t minimumReader = 0;
    if (!reqInt64(root, "minimumReaderVersion", "", minimumReader)) return false;
    if (minimumReader > kReaderMaxVersion) {
      return fail(FormatDiagnosticCode::kFormatTooNew, "minimumReaderVersion 高于本 Reader",
                  "/minimumReaderVersion");
    }

    // appVersion 可选，缺失**不得**报错（U4）
    // ---- 第 3 步：capabilities ----
    const JsonValue* caps = root.find("capabilities");
    if (caps != nullptr && !decodeCapabilities(*caps)) return false;
    if (!checkDeclaredCapabilities()) return false;

    const JsonValue* project = root.find("project");
    if (project == nullptr) {
      return fail(FormatDiagnosticCode::kFormatMissingField, "缺必需字段：project", "/project");
    }
    if (!project->isObject()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "project 必须是对象", "/project");
    }
    if (!decodeProject(*project)) return false;

    collectUnknown(root, kTopKeys, OwnerAnchor{OwnerKind::kDocument, ""}, "");
    return true;
  }

  /// v2 必须声明 kSkeletonCapability 为 required（负责人 2026-10-20 裁定）。
  ///
  /// 放在**解码任何骨架数据之前**检查：能力不匹配时不该先尝试理解数据。
  /// v1 文档没有骨架能力的要求（那是 v1 的时代还没骨架），故不检查。
  bool checkDeclaredCapabilities() {
    if (version_ < 2) return true;
    for (const std::string& cap : requiredCaps_) {
      if (cap == kSkeletonCapability) return true;
    }
    return fail(FormatDiagnosticCode::kFormatMissingField,
                std::string("v2 文档必须在 capabilities.required 中声明 ") + kSkeletonCapability,
                "/capabilities/required");
  }

  /// 能力白名单按**文档版本**分档：v1 只认基线能力，v2 追加骨架能力。
  /// 未知能力一律 kCapabilityUnsupported。
  bool isAcceptedCapability(const std::string& cap) const {
    if (cap == kBaselineCapability) return true;
    if (version_ >= 2 && cap == kSkeletonCapability) return true;
    return false;
  }

  bool decodeCapabilities(const JsonValue& obj) {
    const std::string ptr = "/capabilities";
    if (!obj.isObject()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "capabilities 必须是对象", ptr);
    }
    hasCapabilities_ = true;

    const JsonValue* required = obj.find("required");
    if (required != nullptr) {
      const std::string rptr = childPointer(ptr, "required");
      if (!required->isArray()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "capabilities.required 必须是数组",
                    rptr);
      }
      for (std::size_t i = 0; i < required->array.size(); ++i) {
        const JsonValue& item = required->array[i];
        const std::string iptr = indexPointer(rptr, i);
        if (!item.isString()) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "能力标识符必须是 string", iptr);
        }
        if (!isAcceptedCapability(item.stringValue)) {
          return fail(FormatDiagnosticCode::kCapabilityUnsupported,
                      "不支持的必需能力：" + item.stringValue, iptr);
        }
        requiredCaps_.push_back(item.stringValue);
      }
    }

    // optional：不认识的条目忽略，但内容**保留**（RFC §1.2）
    const JsonValue* optional = obj.find("optional");
    if (optional != nullptr) {
      const std::string optr = childPointer(ptr, "optional");
      if (!optional->isArray()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "capabilities.optional 必须是数组",
                    optr);
      }
      for (std::size_t i = 0; i < optional->array.size(); ++i) {
        const JsonValue& item = optional->array[i];
        const std::string iptr = indexPointer(optr, i);
        if (!item.isString()) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "能力标识符必须是 string", iptr);
        }
        optionalCaps_.push_back(item.stringValue);
      }
    }

    collectUnknown(obj, kCapabilitiesKeys, OwnerAnchor{OwnerKind::kDocument, ""}, "/capabilities");
    return true;
  }

  bool decodeProject(const JsonValue& obj) {
    const std::string ptr = "/project";

    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    data_.id = Id::fromString(IdKind::Project, idText);

    if (!reqString(obj, "name", ptr, data_.name)) return false;

    std::string rootNodeText;
    if (!reqString(obj, "rootNodeId", ptr, rootNodeText)) return false;
    data_.rootNodeId = Id::fromString(IdKind::Node, rootNodeText);

    std::int64_t defaultSceneIndex = 0;
    if (!reqInt64(obj, "defaultSceneIndex", ptr, defaultSceneIndex)) return false;
    if (defaultSceneIndex < 0) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "defaultSceneIndex 不得为负",
                  childPointer(ptr, "defaultSceneIndex"));
    }
    data_.defaultSceneIndex = static_cast<std::size_t>(defaultSceneIndex);

    const JsonValue* scenes = mustArray(obj, "scenes", ptr);
    if (scenes == nullptr) return false;
    const std::string scenesPtr = childPointer(ptr, "scenes");
    for (std::size_t i = 0; i < scenes->array.size(); ++i) {
      Scene scene;
      if (!decodeScene(scenes->array[i], indexPointer(scenesPtr, i), scene)) return false;
      data_.scenes.push_back(scene);
    }

    const JsonValue* nodes = mustArray(obj, "nodes", ptr);
    if (nodes == nullptr) return false;
    const std::string nodesPtr = childPointer(ptr, "nodes");
    for (std::size_t i = 0; i < nodes->array.size(); ++i) {
      NodeRecord record;
      if (!decodeNode(nodes->array[i], indexPointer(nodesPtr, i), record)) return false;
      data_.nodes.push_back(record);
    }

    const JsonValue* nodeOrder = mustArray(obj, "nodeOrder", ptr);
    if (nodeOrder == nullptr) return false;
    const std::string orderPtr = childPointer(ptr, "nodeOrder");
    for (std::size_t i = 0; i < nodeOrder->array.size(); ++i) {
      const JsonValue& item = nodeOrder->array[i];
      const std::string iptr = indexPointer(orderPtr, i);
      if (!item.isString()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "nodeOrder 元素必须是 string（NodeId）",
                    iptr);
      }
      data_.nodeOrder.push_back(Id::fromString(IdKind::Node, item.stringValue));
    }

    const JsonValue* assets = mustArray(obj, "assets", ptr);
    if (assets == nullptr) return false;
    const std::string assetsPtr = childPointer(ptr, "assets");
    for (std::size_t i = 0; i < assets->array.size(); ++i) {
      ImageAsset asset;
      if (!decodeAsset(assets->array[i], indexPointer(assetsPtr, i), asset)) return false;
      data_.assets.push_back(asset);
    }

    const JsonValue* textures = mustArray(obj, "textures", ptr);
    if (textures == nullptr) return false;
    const std::string texturesPtr = childPointer(ptr, "textures");
    for (std::size_t i = 0; i < textures->array.size(); ++i) {
      TextureAsset texture;
      if (!decodeTexture(textures->array[i], indexPointer(texturesPtr, i), texture)) return false;
      data_.textures.push_back(texture);
    }

    const JsonValue* atlases = mustArray(obj, "atlases", ptr);
    if (atlases == nullptr) return false;
    const std::string atlasesPtr = childPointer(ptr, "atlases");
    for (std::size_t i = 0; i < atlases->array.size(); ++i) {
      Atlas atlas;
      if (!decodeAtlas(atlases->array[i], indexPointer(atlasesPtr, i), atlas)) return false;
      data_.atlases.push_back(atlas);
    }

    // ---- 骨架（仅 v2）----
    if (version_ < 2) {
      // v1 若含 project.skeletons，它会被 kProjectKeys 之外的收集路径当成未知字段，
      // 迁移后又作为真字段写出 -> 同一对象里出现两个 skeletons 键。显式拒绝，
      // 不猜测、不静默丢弃（Task 4 §6：不得猜测或静默丢弃）。
      if (obj.find("skeletons") != nullptr) {
        return fail(FormatDiagnosticCode::kFormatVersionUnsupported,
                    "v1 文档不得含 project.skeletons（该字段自 v2 起引入）",
                    childPointer(ptr, "skeletons"));
      }
    } else {
      const JsonValue* skeletons = mustArray(obj, "skeletons", ptr);
      if (skeletons == nullptr) return false;
      const std::string skeletonsPtr = childPointer(ptr, "skeletons");
      for (std::size_t i = 0; i < skeletons->array.size(); ++i) {
        Skeleton skeleton;
        if (!decodeSkeleton(skeletons->array[i], indexPointer(skeletonsPtr, i), skeleton)) {
          return false;
        }
        data_.skeletons.push_back(skeleton);
      }
    }

    collectUnknown(obj, projectKnown_, OwnerAnchor{OwnerKind::kProject, idText}, "");
    return true;
  }

  bool decodeScene(const JsonValue& obj, const std::string& ptr, Scene& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::Scene, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    std::string rootText;
    if (!reqString(obj, "rootNodeId", ptr, rootText)) return false;
    out.rootNodeId = Id::fromString(IdKind::Node, rootText);

    collectUnknown(obj, kSceneKeys, OwnerAnchor{OwnerKind::kScene, idText}, "");
    return true;
  }

  bool decodeNode(const JsonValue& obj, const std::string& ptr, NodeRecord& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.common.id = Id::fromString(IdKind::Node, idText);

    if (!reqString(obj, "name", ptr, out.common.name)) return false;

    // parent：string | null，必需（RFC §2.4）
    {
      const JsonValue* v = mustHave(obj, "parent", ptr);
      if (v == nullptr) return false;
      const std::string vptr = childPointer(ptr, "parent");
      if (v->isNull()) {
        out.common.parent = NodeId();
      } else if (v->isString()) {
        out.common.parent = Id::fromString(IdKind::Node, v->stringValue);
      } else {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "parent 必须是 string 或 null", vptr);
      }
    }

    // children：array[string]，必需，顺序有语义
    {
      const JsonValue* v = mustArray(obj, "children", ptr);
      if (v == nullptr) return false;
      const std::string vptr = childPointer(ptr, "children");
      for (std::size_t i = 0; i < v->array.size(); ++i) {
        const JsonValue& item = v->array[i];
        const std::string iptr = indexPointer(vptr, i);
        if (!item.isString()) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "children 元素必须是 string（NodeId）",
                      iptr);
        }
        out.common.children.push_back(Id::fromString(IdKind::Node, item.stringValue));
      }
    }

    // transform：object，必需（可为 {}，内部字段全可选）
    {
      const JsonValue* v = mustHave(obj, "transform", ptr);
      if (v == nullptr) return false;
      const std::string vptr = childPointer(ptr, "transform");
      if (!v->isObject()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "transform 必须是对象", vptr);
      }
      if (!decodeTransform(*v, vptr, out.common.transform)) return false;
      // transform 自身无 ID：用父节点锚点 + 相对子路径（RFC §6.5.1）
      collectUnknown(*v, kTransformKeys, OwnerAnchor{OwnerKind::kNode, idText}, "/transform");
    }

    // visible / locked：可选，缺失保留默认值
    {
      const JsonValue* visible = obj.find("visible");
      if (visible != nullptr) {
        const std::string vptr = childPointer(ptr, "visible");
        if (!visible->isBool()) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "visible 必须是 boolean", vptr);
        }
        out.common.visible = visible->boolValue;
      }
      const JsonValue* locked = obj.find("locked");
      if (locked != nullptr) {
        const std::string lptr = childPointer(ptr, "locked");
        if (!locked->isBool()) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "locked 必须是 boolean", lptr);
        }
        out.common.locked = locked->boolValue;
      }
    }

    // type：必需
    {
      std::string text;
      if (!reqString(obj, "type", ptr, text)) return false;
      NodeType type = NodeType::kGroup;
      if (!decodeNodeType(text, type)) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                    "type 必须是 root / group / image 之一", childPointer(ptr, "type"));
      }
      out.type = type;
    }

    // assetId / regionId：可选，string | null
    {
      const JsonValue* assetId = obj.find("assetId");
      if (assetId != nullptr) {
        const std::string aptr = childPointer(ptr, "assetId");
        if (assetId->isNull()) {
          out.assetId.reset();
        } else if (assetId->isString()) {
          out.assetId = Id::fromString(IdKind::ImageAsset, assetId->stringValue);
        } else {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "assetId 必须是 string 或 null",
                      aptr);
        }
      }
      const JsonValue* regionId = obj.find("regionId");
      if (regionId != nullptr) {
        const std::string rptr = childPointer(ptr, "regionId");
        if (regionId->isNull()) {
          out.regionId.reset();
        } else if (regionId->isString()) {
          out.regionId = Id::fromString(IdKind::AtlasRegion, regionId->stringValue);
        } else {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "regionId 必须是 string 或 null",
                      rptr);
        }
      }
    }

    collectUnknown(obj, kNodeKeys, OwnerAnchor{OwnerKind::kNode, idText}, "");
    return true;
  }

  bool decodeTransform(const JsonValue& obj, const std::string& ptr, Transform& out) {
    if (!optDouble(obj, "x", ptr, out.x)) return false;
    if (!optDouble(obj, "y", ptr, out.y)) return false;
    if (!optDouble(obj, "rotationDeg", ptr, out.rotationDeg)) return false;
    if (!optDouble(obj, "scaleX", ptr, out.scaleX)) return false;
    if (!optDouble(obj, "scaleY", ptr, out.scaleY)) return false;
    return true;
  }

  bool decodeAsset(const JsonValue& obj, const std::string& ptr, ImageAsset& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::ImageAsset, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    // type：可选，仅 "static"
    {
      const JsonValue* v = obj.find("type");
      if (v != nullptr) {
        const std::string vptr = childPointer(ptr, "type");
        if (!v->isString()) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "type 必须是 string", vptr);
        }
        ImageAssetType type = ImageAssetType::kStatic;
        if (!decodeAssetType(v->stringValue, type)) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "type 必须是 static", vptr);
        }
        out.type = type;
      }
    }

    // storage：必需
    {
      std::string text;
      if (!reqString(obj, "storage", ptr, text)) return false;
      StorageMode mode = StorageMode::kExternalReference;
      if (!decodeStorage(text, mode)) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                    "storage 必须是 external / inProject 之一", childPointer(ptr, "storage"));
      }
      out.storage = mode;
    }

    if (!reqString(obj, "location", ptr, out.location)) return false;

    if (!optInt32Optional(obj, "pixelWidth", ptr, out.pixelWidth)) return false;
    if (!optInt32Optional(obj, "pixelHeight", ptr, out.pixelHeight)) return false;

    collectUnknown(obj, kAssetKeys, OwnerAnchor{OwnerKind::kImageAsset, idText}, "");
    return true;
  }

  bool decodeTexture(const JsonValue& obj, const std::string& ptr, TextureAsset& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::TextureAsset, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    std::string storageText;
    if (!reqString(obj, "storage", ptr, storageText)) return false;
    StorageMode mode = StorageMode::kExternalReference;
    if (!decodeStorage(storageText, mode)) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                  "storage 必须是 external / inProject 之一", childPointer(ptr, "storage"));
    }
    out.storage = mode;

    if (!reqString(obj, "location", ptr, out.location)) return false;

    if (!optInt32Optional(obj, "pixelWidth", ptr, out.pixelWidth)) return false;
    if (!optInt32Optional(obj, "pixelHeight", ptr, out.pixelHeight)) return false;

    collectUnknown(obj, kTextureKeys, OwnerAnchor{OwnerKind::kTextureAsset, idText}, "");
    return true;
  }

  bool decodeAtlas(const JsonValue& obj, const std::string& ptr, Atlas& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::Atlas, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    const JsonValue* pages = mustArray(obj, "pages", ptr);
    if (pages == nullptr) return false;
    const std::string pagesPtr = childPointer(ptr, "pages");
    for (std::size_t i = 0; i < pages->array.size(); ++i) {
      AtlasPage page;
      if (!decodePage(pages->array[i], indexPointer(pagesPtr, i), page)) return false;
      out.pages.push_back(page);
    }

    const JsonValue* regions = mustArray(obj, "regions", ptr);
    if (regions == nullptr) return false;
    const std::string regionsPtr = childPointer(ptr, "regions");
    for (std::size_t i = 0; i < regions->array.size(); ++i) {
      AtlasRegion region;
      if (!decodeRegion(regions->array[i], indexPointer(regionsPtr, i), region)) return false;
      out.regions.push_back(region);
    }

    collectUnknown(obj, kAtlasKeys, OwnerAnchor{OwnerKind::kAtlas, idText}, "");
    return true;
  }

  bool decodePage(const JsonValue& obj, const std::string& ptr, AtlasPage& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::AtlasPage, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    std::string textureText;
    if (!reqString(obj, "textureAssetId", ptr, textureText)) return false;
    out.textureAssetId = Id::fromString(IdKind::TextureAsset, textureText);

    if (!reqInt32(obj, "width", ptr, out.width)) return false;
    if (!reqInt32(obj, "height", ptr, out.height)) return false;

    collectUnknown(obj, kPageKeys, OwnerAnchor{OwnerKind::kAtlasPage, idText}, "");
    return true;
  }

  bool decodeRegion(const JsonValue& obj, const std::string& ptr, AtlasRegion& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::AtlasRegion, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    std::string pageText;
    if (!reqString(obj, "pageId", ptr, pageText)) return false;
    out.pageId = Id::fromString(IdKind::AtlasPage, pageText);

    // 必需（> 0，由 Domain 校验）
    if (!reqInt32(obj, "packedW", ptr, out.packedW)) return false;
    if (!reqInt32(obj, "packedH", ptr, out.packedH)) return false;
    if (!reqInt32(obj, "originalW", ptr, out.originalW)) return false;
    if (!reqInt32(obj, "originalH", ptr, out.originalH)) return false;
    if (!reqInt32(obj, "trimmedW", ptr, out.trimmedW)) return false;
    if (!reqInt32(obj, "trimmedH", ptr, out.trimmedH)) return false;

    // 可选（默认 0 / none）
    if (!optInt32(obj, "packedX", ptr, out.packedX)) return false;
    if (!optInt32(obj, "packedY", ptr, out.packedY)) return false;
    if (!optInt32(obj, "offsetX", ptr, out.offsetX)) return false;
    if (!optInt32(obj, "offsetY", ptr, out.offsetY)) return false;

    const JsonValue* rotation = obj.find("rotation");
    if (rotation != nullptr) {
      const std::string rptr = childPointer(ptr, "rotation");
      if (!rotation->isString()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "rotation 必须是 string", rptr);
      }
      if (!decodeRotation(rotation->stringValue, out.rotation)) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                    "rotation 必须是 none / 90 / 180 / 270 之一", rptr);
      }
    }

    collectUnknown(obj, kRegionKeys, OwnerAnchor{OwnerKind::kAtlasRegion, idText}, "");
    return true;
  }

  // ===== 骨架层（v2；契约见 docs/format/native-project-v2-skeleton.md §3）=====

  /// ColorF：全字段可选，默认 1；范围 0..1 由 Format 层把关（§3.3）。
  bool decodeColor(const JsonValue& obj, const std::string& ptr, ColorF& out) {
    if (!optDouble(obj, "r", ptr, out.r)) return false;
    if (!optDouble(obj, "g", ptr, out.g)) return false;
    if (!optDouble(obj, "b", ptr, out.b)) return false;
    if (!optDouble(obj, "a", ptr, out.a)) return false;
    for (const double channel : {out.r, out.g, out.b, out.a}) {
      if (channel < 0.0 || channel > 1.0) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "color 分量必须在 0..1 内", ptr);
      }
    }
    return true;
  }

  bool decodeSkeleton(const JsonValue& obj, const std::string& ptr, Skeleton& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::Skeleton, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    // boneOrder / drawOrder 必需；缺失须报错，不得静默当作空数组（会让 Domain 的
    // 「互为排列」检查退化成一个更难懂的错误）。
    if (obj.find("boneOrder") == nullptr) {
      return fail(FormatDiagnosticCode::kFormatMissingField, "缺必需字段：boneOrder",
                  childPointer(ptr, "boneOrder"));
    }
    if (obj.find("drawOrder") == nullptr) {
      return fail(FormatDiagnosticCode::kFormatMissingField, "缺必需字段：drawOrder",
                  childPointer(ptr, "drawOrder"));
    }
    // atlasIds 可选，默认 []
    if (!decodeIdArray(obj, "atlasIds", ptr, IdKind::Atlas, "AtlasId", &out.atlasIds)) return false;
    if (!decodeIdArray(obj, "boneOrder", ptr, IdKind::Bone, "BoneId", &out.boneOrder)) return false;
    if (!decodeIdArray(obj, "drawOrder", ptr, IdKind::Slot, "SlotId", &out.drawOrder)) return false;

    const JsonValue* bones = mustArray(obj, "bones", ptr);
    if (bones == nullptr) return false;
    const std::string bonesPtr = childPointer(ptr, "bones");
    for (std::size_t i = 0; i < bones->array.size(); ++i) {
      Bone bone;
      if (!decodeBone(bones->array[i], indexPointer(bonesPtr, i), bone)) return false;
      out.bones.push_back(bone);
    }

    const JsonValue* slots = mustArray(obj, "slots", ptr);
    if (slots == nullptr) return false;
    const std::string slotsPtr = childPointer(ptr, "slots");
    for (std::size_t i = 0; i < slots->array.size(); ++i) {
      Slot slot;
      if (!decodeSlot(slots->array[i], indexPointer(slotsPtr, i), slot)) return false;
      out.slots.push_back(slot);
    }

    const JsonValue* skins = mustArray(obj, "skins", ptr);
    if (skins == nullptr) return false;
    const std::string skinsPtr = childPointer(ptr, "skins");
    for (std::size_t i = 0; i < skins->array.size(); ++i) {
      Skin skin;
      if (!decodeSkin(skins->array[i], indexPointer(skinsPtr, i), skin)) return false;
      out.skins.push_back(skin);
    }

    // 第二批约束（Task 5 B2，v3 schema §3）：v3 文档这两个键**恒存在**（可为空数组）；
    // v2 文档没有它们 ⇒ 视为空数组，等价于 migrateV2ToV3() 的补齐结果。
    const JsonValue* iks = obj.find("ikConstraints");
    if (iks != nullptr) {
      if (!iks->isArray()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "ikConstraints 必须是数组",
                    childPointer(ptr, "ikConstraints"));
      }
      const std::string ip = childPointer(ptr, "ikConstraints");
      for (std::size_t i = 0; i < iks->array.size(); ++i) {
        IkConstraint c;
        if (!decodeIkConstraint(iks->array[i], indexPointer(ip, i), c)) return false;
        out.ikConstraints.push_back(c);
      }
    }
    const JsonValue* trs = obj.find("transformConstraints");
    if (trs != nullptr) {
      if (!trs->isArray()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "transformConstraints 必须是数组",
                    childPointer(ptr, "transformConstraints"));
      }
      const std::string tp = childPointer(ptr, "transformConstraints");
      for (std::size_t i = 0; i < trs->array.size(); ++i) {
        TransformConstraint c;
        if (!decodeTransformConstraint(trs->array[i], indexPointer(tp, i), c)) return false;
        out.transformConstraints.push_back(c);
      }
    }

    // sourceRef：整个对象可选；存在则逐字段校验；null 不合法（§3.7）
    {
      const JsonValue* sourceRef = obj.find("sourceRef");
      if (sourceRef != nullptr && !sourceRef->isNull()) {
        const std::string sptr = childPointer(ptr, "sourceRef");
        SourceRef ref;
        if (!decodeSourceRef(*sourceRef, sptr, idText, ref)) return false;
        out.sourceRef = ref;
      } else if (sourceRef != nullptr) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                    "sourceRef 必须是对象；无来源时请省略该键，不要用 null",
                    childPointer(ptr, "sourceRef"));
      }
    }

    collectUnknown(obj, kSkeletonKeys, OwnerAnchor{OwnerKind::kSkeleton, idText}, "");
    return true;
  }

  bool decodeBone(const JsonValue& obj, const std::string& ptr, Bone& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::Bone, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    // parentId：string | null，可选（null = 根骨，§3.2）
    {
      const JsonValue* v = obj.find("parentId");
      if (v != nullptr) {
        const std::string vptr = childPointer(ptr, "parentId");
        if (v->isNull()) {
          out.parent = BoneId();
        } else if (v->isString()) {
          out.parent = Id::fromString(IdKind::Bone, v->stringValue);
        } else {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                      "parentId 必须是 string 或 null", vptr);
        }
      }
    }

    if (!optDouble(obj, "x", ptr, out.x)) return false;
    if (!optDouble(obj, "y", ptr, out.y)) return false;
    if (!optDouble(obj, "rotationDeg", ptr, out.rotationDeg)) return false;
    if (!optDouble(obj, "scaleX", ptr, out.scaleX)) return false;
    if (!optDouble(obj, "scaleY", ptr, out.scaleY)) return false;
    if (!optDouble(obj, "shearXDeg", ptr, out.shearXDeg)) return false;
    if (!optDouble(obj, "shearYDeg", ptr, out.shearYDeg)) return false;
    if (!optDouble(obj, "length", ptr, out.length)) return false;
    if (!optBool(obj, "inheritRotation", ptr, out.inheritRotation)) return false;
    if (!optBool(obj, "inheritScale", ptr, out.inheritScale)) return false;

    collectUnknown(obj, kBoneKeys, OwnerAnchor{OwnerKind::kBone, idText}, "");
    return true;
  }

  bool decodeSlot(const JsonValue& obj, const std::string& ptr, Slot& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::Slot, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    std::string boneText;
    if (!reqString(obj, "boneId", ptr, boneText)) return false;
    out.boneId = Id::fromString(IdKind::Bone, boneText);

    // color：可选对象（§3.3）
    {
      const JsonValue* color = obj.find("color");
      if (color != nullptr) {
        const std::string cptr = childPointer(ptr, "color");
        if (!color->isObject()) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "color 必须是对象", cptr);
        }
        if (!decodeColor(*color, cptr, out.color)) return false;
        // color 无 ID：锚点 = 所属 Slot + 相对子路径
        collectUnknown(*color, kColorKeys, OwnerAnchor{OwnerKind::kSlot, idText}, "/color");
      }
    }

    // attachmentName：string | null，可选
    {
      const JsonValue* v = obj.find("attachmentName");
      if (v != nullptr) {
        const std::string vptr = childPointer(ptr, "attachmentName");
        if (v->isNull()) {
          out.attachmentName.reset();
        } else if (v->isString()) {
          out.attachmentName = v->stringValue;
        } else {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                      "attachmentName 必须是 string 或 null", vptr);
        }
      }
    }

    // blendMode：可选枚举字符串，默认 normal
    {
      const JsonValue* v = obj.find("blendMode");
      if (v != nullptr) {
        const std::string vptr = childPointer(ptr, "blendMode");
        if (!v->isString()) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "blendMode 必须是 string", vptr);
        }
        if (!decodeBlendMode(v->stringValue, out.blendMode)) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                      "blendMode 必须是 normal / additive / multiply / screen 之一", vptr);
        }
      }
    }

    collectUnknown(obj, kSlotKeys, OwnerAnchor{OwnerKind::kSlot, idText}, "");
    return true;
  }

  bool decodeSkin(const JsonValue& obj, const std::string& ptr, Skin& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::Skin, idText);

    if (!reqString(obj, "name", ptr, out.name)) return false;

    // entries：必需数组，**可为空**（§3.4 记为必需，允许空 skin）
    const JsonValue* entries = mustArray(obj, "entries", ptr);
    if (entries == nullptr) return false;
    const std::string entriesPtr = childPointer(ptr, "entries");
    for (std::size_t i = 0; i < entries->array.size(); ++i) {
      SkinEntry entry;
      if (!decodeSkinEntry(entries->array[i], indexPointer(entriesPtr, i), entry)) return false;
      out.entries.push_back(entry);
    }

    collectUnknown(obj, kSkinKeys, OwnerAnchor{OwnerKind::kSkin, idText}, "");
    return true;
  }

  bool decodeSkinEntry(const JsonValue& obj, const std::string& ptr, SkinEntry& out) {
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::SkinEntry, idText);

    std::string slotText;
    if (!reqString(obj, "slotId", ptr, slotText)) return false;
    out.slotId = Id::fromString(IdKind::Slot, slotText);

    if (!reqString(obj, "attachmentName", ptr, out.attachmentName)) return false;

    // attachment：必需对象（v2 仅 region 一种）
    {
      const JsonValue* v = mustHave(obj, "attachment", ptr);
      if (v == nullptr) return false;
      const std::string vptr = childPointer(ptr, "attachment");
      if (!v->isObject()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "attachment 必须是对象", vptr);
      }
      // Attachment 没有独立 ID：宿主是 SkinEntry，子路径 /attachment（§5.2）。
      // 未知字段的收集按 `type` **分派**在 decodeAttachment 内部完成（评审 P0）。
      if (!decodeAttachment(*v, vptr, out.attachment, idText)) return false;
    }

    collectUnknown(obj, kSkinEntryKeys, OwnerAnchor{OwnerKind::kSkinEntry, idText}, "");
    return true;
  }

  /// region 附件：v2 起语义与字段**逐字不变**（Task 5 B2 只增不改）。
  bool decodeRegionAttachment(const JsonValue& obj, const std::string& ptr,
                              RegionAttachment& out) {
    std::string regionText;
    if (!reqString(obj, "regionId", ptr, regionText)) return false;
    out.regionId = Id::fromString(IdKind::AtlasRegion, regionText);

    if (!optDouble(obj, "x", ptr, out.x)) return false;
    if (!optDouble(obj, "y", ptr, out.y)) return false;
    if (!optDouble(obj, "rotationDeg", ptr, out.rotationDeg)) return false;
    if (!optDouble(obj, "scaleX", ptr, out.scaleX)) return false;
    if (!optDouble(obj, "scaleY", ptr, out.scaleY)) return false;
    if (!optDouble(obj, "width", ptr, out.width)) return false;
    if (!optDouble(obj, "height", ptr, out.height)) return false;

    const JsonValue* color = obj.find("color");
    if (color != nullptr) {
      const std::string cptr = childPointer(ptr, "color");
      if (!color->isObject()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "color 必须是对象", cptr);
      }
      if (!decodeColor(*color, cptr, out.color)) return false;
    }
    return true;
  }

  // ---- Task 5 B2：第二批附件（mesh / weightedMesh / linkedMesh）与约束 ----

  bool readDoubleElement(const JsonValue& v, const std::string& ptr, double& out) {
    if (!v.isNumber()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "数组元素必须是 number", ptr);
    }
    double d = 0.0;
    if (parseDoubleStatus(v.numberRaw, d) != DoubleParseStatus::kOk) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "数值字面量越出 double 可表示范围", ptr);
    }
    out = d;
    return true;
  }

  /// uvs / vertices：double 数组。缺失时视为空数组（Domain 侧再判「长度与顶点数不符」）。
  bool decodeDoubleArray(const JsonValue& obj, const char* key, const std::string& ptr,
                         std::vector<double>& out) {
    const JsonValue* arr = obj.find(key);
    if (arr == nullptr) return true;
    const std::string ap = childPointer(ptr, key);
    if (!arr->isArray()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是数组", ap);
    }
    for (std::size_t i = 0; i < arr->array.size(); ++i) {
      double v = 0.0;
      if (!readDoubleElement(arr->array[i], indexPointer(ap, i), v)) return false;
      out.push_back(v);
    }
    return true;
  }

  /// triangles：顶点索引，必须是**非负整数**且落在 uint32 值域（D23：值域越界即拒绝）。
  bool decodeTriangleArray(const JsonValue& obj, const std::string& ptr,
                           std::vector<std::uint32_t>& out) {
    const JsonValue* arr = obj.find("triangles");
    if (arr == nullptr) return true;
    const std::string ap = childPointer(ptr, "triangles");
    if (!arr->isArray()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "triangles 必须是数组", ap);
    }
    for (std::size_t i = 0; i < arr->array.size(); ++i) {
      const std::string ep = indexPointer(ap, i);
      const JsonValue& e = arr->array[i];
      if (!e.isNumber()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "triangles 元素必须是 number", ep);
      }
      std::int64_t v = 0;
      if (!readInt64Literal(e, ep, v)) return false;
      if (v < 0 || v > 4294967295LL) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                    "triangles 元素越出 uint32 值域", ep);
      }
      out.push_back(static_cast<std::uint32_t>(v));
    }
    return true;
  }

  bool readOptionalBool(const JsonValue& obj, const char* key, const std::string& ptr,
                        bool& out) {
    const JsonValue* v = obj.find(key);
    if (v == nullptr) return true;
    const std::string p = childPointer(ptr, key);
    if (!v->isBool()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 bool", p);
    }
    out = v->boolValue;
    return true;
  }

  bool decodeMeshAttachment(const JsonValue& obj, const std::string& ptr, MeshAttachment& out,
                            const std::string& entryId) {
    std::string regionText;
    if (!reqString(obj, "regionId", ptr, regionText)) return false;
    out.regionId = Id::fromString(IdKind::AtlasRegion, regionText);
    if (!decodeDoubleArray(obj, "uvs", ptr, out.uvs)) return false;
    if (!decodeTriangleArray(obj, ptr, out.triangles)) return false;
    if (!decodeDoubleArray(obj, "vertices", ptr, out.vertices)) return false;
    if (!optDouble(obj, "width", ptr, out.width)) return false;
    if (!optDouble(obj, "height", ptr, out.height)) return false;
    collectUnknown(obj, kMeshAttachmentKeys, OwnerAnchor{OwnerKind::kSkinEntry, entryId},
                   "/attachment");
    return true;
  }

  bool decodeWeightedMeshAttachment(const JsonValue& obj, const std::string& ptr,
                                    WeightedMeshAttachment& out, const std::string& entryId) {
    std::string regionText;
    if (!reqString(obj, "regionId", ptr, regionText)) return false;
    out.regionId = Id::fromString(IdKind::AtlasRegion, regionText);
    if (!decodeDoubleArray(obj, "uvs", ptr, out.uvs)) return false;
    if (!decodeTriangleArray(obj, ptr, out.triangles)) return false;

    const JsonValue* bindings = mustArray(obj, "vertexBindings", ptr);
    if (bindings == nullptr) return false;
    const std::string bp = childPointer(ptr, "vertexBindings");
    for (std::size_t i = 0; i < bindings->array.size(); ++i) {
      const JsonValue& vert = bindings->array[i];
      const std::string vp = indexPointer(bp, i);
      if (!vert.isArray()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch, "vertexBindings 元素必须是数组", vp);
      }
      std::vector<VertexBinding> binds;
      for (std::size_t j = 0; j < vert.array.size(); ++j) {
        const JsonValue& b = vert.array[j];
        const std::string bpj = indexPointer(vp, j);
        if (!b.isObject()) {
          return fail(FormatDiagnosticCode::kJsonTypeMismatch, "绑定必须是对象", bpj);
        }
        VertexBinding vb;
        std::string boneText;
        if (!reqString(b, "boneId", bpj, boneText)) return false;
        vb.boneId = Id::fromString(IdKind::Bone, boneText);
        if (!optDouble(b, "x", bpj, vb.x)) return false;
        if (!optDouble(b, "y", bpj, vb.y)) return false;
        if (!optDouble(b, "weight", bpj, vb.weight)) return false;
        binds.push_back(vb);
        collectUnknown(b, kVertexBindingKeys, OwnerAnchor{OwnerKind::kSkinEntry, entryId},
                       "/attachment/vertexBindings/" + std::to_string(i) + "/" +
                           std::to_string(j));
      }
      out.vertexBindings.push_back(binds);
    }

    if (!optDouble(obj, "width", ptr, out.width)) return false;
    if (!optDouble(obj, "height", ptr, out.height)) return false;
    collectUnknown(obj, kWeightedMeshAttachmentKeys, OwnerAnchor{OwnerKind::kSkinEntry, entryId},
                   "/attachment");
    return true;
  }

  /// linked mesh：**不读几何**，只读 `parentKey`（D38 两条路径在映射层统一为同键）。
  bool decodeLinkedMeshAttachment(const JsonValue& obj, const std::string& ptr,
                                  LinkedMeshAttachment& out, const std::string& entryId) {
    std::string regionText;
    if (!reqString(obj, "regionId", ptr, regionText)) return false;
    out.regionId = Id::fromString(IdKind::AtlasRegion, regionText);

    const JsonValue* pk = mustHave(obj, "parentKey", ptr);
    if (pk == nullptr) return false;
    const std::string pkp = childPointer(ptr, "parentKey");
    if (!pk->isObject()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "parentKey 必须是对象", pkp);
    }
    std::string slotText;
    if (!reqString(*pk, "slotId", pkp, slotText)) return false;
    out.parentSlotId = Id::fromString(IdKind::Slot, slotText);
    if (!reqString(*pk, "attachmentName", pkp, out.parentAttachmentName)) return false;
    collectUnknown(*pk, kParentKeyKeys, OwnerAnchor{OwnerKind::kSkinEntry, entryId},
                   "/attachment/parentKey");
    if (!readOptionalBool(obj, "inheritDeform", ptr, out.inheritDeform)) return false;
    collectUnknown(obj, kLinkedMeshAttachmentKeys, OwnerAnchor{OwnerKind::kSkinEntry, entryId},
                   "/attachment");
    return true;
  }

  /// 附件分派：按 `type` 严格判别；未知类型**显式拒绝**（v3 提案 §6），
  /// 不做「当作 region」的降级。
  bool decodeAttachment(const JsonValue& obj, const std::string& ptr, Attachment& out,
                        const std::string& entryId) {
    std::string typeText;
    if (!reqString(obj, "type", ptr, typeText)) return false;
    if (typeText == "region") {
      RegionAttachment r;
      if (!decodeRegionAttachment(obj, ptr, r)) return false;
      // region 的已知键与 mesh 族**不同**：region 附件里出现 `uvs` 必须报未知字段，
      // 而不是被静默回写（评审 P0）。
      collectUnknown(obj, kRegionAttachmentKeys, OwnerAnchor{OwnerKind::kSkinEntry, entryId},
                     "/attachment");
      out = r;
      return true;
    }
    if (typeText == "mesh") {
      MeshAttachment m;
      if (!decodeMeshAttachment(obj, ptr, m, entryId)) return false;
      out = m;
      return true;
    }
    if (typeText == "weightedMesh") {
      WeightedMeshAttachment m;
      if (!decodeWeightedMeshAttachment(obj, ptr, m, entryId)) return false;
      out = m;
      return true;
    }
    if (typeText == "linkedMesh") {
      LinkedMeshAttachment m;
      if (!decodeLinkedMeshAttachment(obj, ptr, m, entryId)) return false;
      out = m;
      return true;
    }
    return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                "attachment.type 未知（支持：region / mesh / weightedMesh / linkedMesh）",
                childPointer(ptr, "type"));
  }

  bool decodeIkConstraint(const JsonValue& obj, const std::string& ptr, IkConstraint& out) {
    if (!obj.isObject()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "ikConstraints 元素必须是对象", ptr);
    }
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::IkConstraint, idText);
    if (!reqString(obj, "name", ptr, out.name)) return false;
    if (!decodeIdArray(obj, "boneIds", ptr, IdKind::Bone, "BoneId", &out.boneIds)) return false;
    std::string targetText;
    if (!reqString(obj, "targetBoneId", ptr, targetText)) return false;
    out.targetBoneId = Id::fromString(IdKind::Bone, targetText);
    if (!optDouble(obj, "mix", ptr, out.mix)) return false;
    if (!readOptionalBool(obj, "enabled", ptr, out.enabled)) return false;
    if (!readOptionalBool(obj, "bendPositive", ptr, out.bendPositive)) return false;
    // 约束对象的未知字段此前**只写不读**（评审 G2）：这里补上收集侧，
    // 否则 emitUnknowns 写出的锚点在解码时没有来源。
    collectUnknown(obj, kIkConstraintKeys, OwnerAnchor{OwnerKind::kIkConstraint, idText}, "");
    return true;
  }

  bool decodeTransformConstraint(const JsonValue& obj, const std::string& ptr,
                                 TransformConstraint& out) {
    if (!obj.isObject()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                  "transformConstraints 元素必须是对象", ptr);
    }
    std::string idText;
    if (!reqString(obj, "id", ptr, idText)) return false;
    out.id = Id::fromString(IdKind::TransformConstraint, idText);
    if (!reqString(obj, "name", ptr, out.name)) return false;
    std::string boneText;
    if (!reqString(obj, "boneId", ptr, boneText)) return false;
    out.boneId = Id::fromString(IdKind::Bone, boneText);
    std::string targetText;
    if (!reqString(obj, "targetBoneId", ptr, targetText)) return false;
    out.targetBoneId = Id::fromString(IdKind::Bone, targetText);
    // 4 个 mix + 6 个 offset：**原值读入、不解释语义**（D36）
    if (!optDouble(obj, "rotateMix", ptr, out.rotateMix)) return false;
    if (!optDouble(obj, "translateMix", ptr, out.translateMix)) return false;
    if (!optDouble(obj, "scaleMix", ptr, out.scaleMix)) return false;
    if (!optDouble(obj, "shearMix", ptr, out.shearMix)) return false;
    if (!optDouble(obj, "offsetRotation", ptr, out.offsetRotation)) return false;
    if (!optDouble(obj, "offsetX", ptr, out.offsetX)) return false;
    if (!optDouble(obj, "offsetY", ptr, out.offsetY)) return false;
    if (!optDouble(obj, "offsetScaleX", ptr, out.offsetScaleX)) return false;
    if (!optDouble(obj, "offsetScaleY", ptr, out.offsetScaleY)) return false;
    if (!optDouble(obj, "offsetShearY", ptr, out.offsetShearY)) return false;
    collectUnknown(obj, kTransformConstraintKeys,
                   OwnerAnchor{OwnerKind::kTransformConstraint, idText}, "");
    return true;
  }

  /// skeletonId 用于锚点：sourceRef 与 importedBy 都是无 ID 的嵌套容器，
  /// 宿主就是外层 Skeleton（v2 schema §5.2）。
  bool decodeSourceRef(const JsonValue& obj, const std::string& ptr,
                       const std::string& skeletonId, SourceRef& out) {
    if (!obj.isObject()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "sourceRef 必须是对象", ptr);
    }
    const OwnerAnchor skeletonAnchor{OwnerKind::kSkeleton, skeletonId};
    if (!reqString(obj, "formatId", ptr, out.formatId)) return false;
    if (!reqString(obj, "formatVersion", ptr, out.formatVersion)) return false;
    if (!reqString(obj, "contentHash", ptr, out.contentHash)) return false;

    // 两个 hash 来源与算法不同，不得混称（Back Task §5.1）。
    // sourceHeaderHash 可选，是源自带头字段原文，无算法。
    if (!optString(obj, "sourceHeaderHash", ptr, out.sourceHeaderHash)) return false;
    if (!optString(obj, "sourceUnit", ptr, out.sourceUnit)) return false;

    // importedBy：必需对象
    const JsonValue* importedBy = mustHave(obj, "importedBy", ptr);
    if (importedBy == nullptr) return false;
    const std::string iptr = childPointer(ptr, "importedBy");
    if (!importedBy->isObject()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "importedBy 必须是对象", iptr);
    }
    if (!reqString(*importedBy, "pluginId", iptr, out.importedByPluginId)) return false;
    if (!reqInt64(*importedBy, "pluginVersion", iptr, out.importedByPluginVersion)) return false;

    collectUnknown(obj, kSourceRefKeys, skeletonAnchor, "/sourceRef");
    collectUnknown(*importedBy, kImportedByKeys, skeletonAnchor, "/sourceRef/importedBy");
    return true;
  }

  // ---- 骨架层辅助 ----

  /// 读取一个 ID 引用数组；字段缺失时**保留空向量**（调用方自行判断必需性）。
  bool decodeIdArray(const JsonValue& obj, std::string_view key, const std::string& objPtr,
                     IdKind kind, const char* what, std::vector<Id>* out) {
    const JsonValue* v = obj.find(key);
    if (v == nullptr) return true;
    const std::string ptr = childPointer(objPtr, key);
    if (!v->isArray()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是数组", ptr);
    }
    for (std::size_t i = 0; i < v->array.size(); ++i) {
      const JsonValue& item = v->array[i];
      const std::string iptr = indexPointer(ptr, i);
      if (!item.isString()) {
        return fail(FormatDiagnosticCode::kJsonTypeMismatch,
                    std::string("数组元素必须是 string（") + what + "）", iptr);
      }
      out->push_back(Id::fromString(kind, item.stringValue));
    }
    return true;
  }

  bool optBool(const JsonValue& obj, std::string_view key, const std::string& objPtr, bool& out) {
    const JsonValue* v = obj.find(key);
    if (v == nullptr) return true;
    const std::string ptr = childPointer(objPtr, key);
    if (!v->isBool()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 boolean", ptr);
    }
    out = v->boolValue;
    return true;
  }

  bool optString(const JsonValue& obj, std::string_view key, const std::string& objPtr,
                 std::optional<std::string>& out) {
    const JsonValue* v = obj.find(key);
    if (v == nullptr) return true;
    const std::string ptr = childPointer(objPtr, key);
    if (!v->isString()) {
      return fail(FormatDiagnosticCode::kJsonTypeMismatch, "字段必须是 string", ptr);
    }
    out = v->stringValue;
    return true;
  }

  FormatDiagnostic diag_;
  bool failed_ = false;
  ProjectData data_;
  UnknownFieldIndex unknowns_;
  bool hasCapabilities_ = false;
  std::vector<std::string> requiredCaps_;
  std::vector<std::string> optionalCaps_;
  int version_ = kFormatVersion;
  std::vector<std::string_view> projectKnown_ = projectKeysForVersion(kFormatVersion);
};

}  // namespace

LoadResult decodeDocument(const std::string& text, const ReaderLimits& limits) {
  Decoder decoder;
  return decoder.run(text, limits);
}

void migrateV2ToV3(ProjectDocument& document) {
  // 幂等：已是 v3（或未知来源）的文档不做任何改动。
  if (document.sourceVersion != SourceFormatVersion::kV2) return;

  // v3 相对 v2 只增结构：
  //   · 骨架的 ikConstraints / transformConstraints —— v2 无此二键，
  //     解码时已按「缺省空数组」处理（见 decodeSkeleton），故此处无需补字段；
  //   · 附件逐个不改（v2 只有 region，语义与字段完全不变，D29）。
  // 因此本迁移**只**标记版本：不生成 ID、不改字段、不碰磁盘。
  document.sourceVersion = SourceFormatVersion::kV3;
}

void migrateV1ToV2(ProjectDocument& document) {
  // 幂等：已是 v2 的文档不做任何改动。这让调用方可以无条件调用，
  // 不用担心把一次往返变成两次。
  if (document.sourceVersion != SourceFormatVersion::kV1) return;

  // 1. v1 没有 project.skeletons；内存中的 v2 结构必须有这一集合（可以为空）。
  //    v1 文档若真的写了 skeletons，会在 decodeProject 里被拒，故此处它必为空。
  document.data.skeletons.clear();

  // 2. 能力字段：确保 required 含骨架能力与基线能力，且**不重复**。
  //    既有条目（含 v1 时期的 atlas 能力与任意 optional 条目）原样保留，
  //    不得改写 —— 迁移只补不改（B3 第 3 条）。
  auto containsCapability = [](const std::vector<std::string>& caps, const char* wanted) {
    for (const std::string& c : caps) {
      if (c == wanted) return true;
    }
    return false;
  };
  if (!containsCapability(document.requiredCapabilities, kBaselineCapability)) {
    document.requiredCapabilities.insert(document.requiredCapabilities.begin(), kBaselineCapability);
  }
  if (!containsCapability(document.requiredCapabilities, kSkeletonCapability)) {
    document.requiredCapabilities.push_back(kSkeletonCapability);
  }
  document.hasCapabilities = true;

  // 3. 标记来源版本。
  //    注意：此处不改 sourceVersion —— 它是「磁盘上读入时的版本」，
  //    由调用方（或后续保存逻辑）决定何时清掉。保持 kV1 才能让上层知道
  //    「这次会话是因为迁移而来的 v2，尚未落盘」。
}

}  // namespace format
}  // namespace loongbones
