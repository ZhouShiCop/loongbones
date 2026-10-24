#include "loongbones/format/codec.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include "loongbones/format/json_value.hpp"

namespace loongbones {
namespace format {
namespace {

using Writer = rapidjson::PrettyWriter<rapidjson::StringBuffer>;

void emitKey(Writer& w, const char* name) {
  w.Key(name, static_cast<rapidjson::SizeType>(std::strlen(name)));
}

void emitStr(Writer& w, const std::string& text) {
  w.String(text.data(), static_cast<rapidjson::SizeType>(text.size()));
}

void emitInt(Writer& w, std::int64_t value) { w.Int64(value); }

void emitDouble(Writer& w, double value) {
  // RFC §4.5：已知数值按最短往返表示重新序列化（保证确定性）
  const std::string text = formatShortestDouble(value);
  w.RawValue(text.data(), static_cast<rapidjson::SizeType>(text.size()),
             rapidjson::kNumberType);
}

// 前向声明：未知值可能是任意嵌套结构
void emitValue(Writer& w, const JsonValue& value);

const UnknownBucket* bucketFor(const UnknownFieldIndex& index, OwnerKind kind,
                               const std::string& id, const std::string& subPath) {
  const auto it = index.find(UnknownKey{OwnerAnchor{kind, id}, subPath});
  if (it == index.end()) return nullptr;
  return &it->second;
}

// 按 (kind, 稳定 ID, 子路径) 取出未知键并**按首次出现顺序**追加（RFC §6.2）。
// 锚点是 ID，因此不受数组重排影响 —— 编码时无需先解析成 JSON Pointer。
void emitUnknowns(Writer& w, const UnknownFieldIndex& index, OwnerKind kind,
                  const std::string& id, const std::string& subPath) {
  const UnknownBucket* bucket = bucketFor(index, kind, id, subPath);
  if (bucket == nullptr) return;
  for (const auto& entry : bucket->entries) {
    w.Key(entry.first.data(), static_cast<rapidjson::SizeType>(entry.first.size()));
    emitValue(w, entry.second);
  }
}

void emitValue(Writer& w, const JsonValue& value) {
  switch (value.type) {
    case JsonType::kNull:
      w.Null();
      break;
    case JsonType::kBool:
      w.Bool(value.boolValue);
      break;
    case JsonType::kNumber:
      // RFC §6.6：未知数值**逐字节回写原始字面量**，不做任何数值转换。
      // 这是 1e400 / 20 位整数能无损往返的关键。
      w.RawValue(value.numberRaw.data(),
                 static_cast<rapidjson::SizeType>(value.numberRaw.size()),
                 rapidjson::kNumberType);
      break;
    case JsonType::kString:
      w.String(value.stringValue.data(),
               static_cast<rapidjson::SizeType>(value.stringValue.size()));
      break;
    case JsonType::kArray:
      w.StartArray();
      for (const auto& item : value.array) emitValue(w, item);
      w.EndArray();
      break;
    case JsonType::kObject:
      w.StartObject();
      for (const auto& member : value.members) {
        w.Key(member.first.data(), static_cast<rapidjson::SizeType>(member.first.size()));
        emitValue(w, member.second);
      }
      w.EndObject();
      break;
  }
}

const char* nodeTypeName(NodeType type) {
  switch (type) {
    case NodeType::kRoot:
      return "root";
    case NodeType::kGroup:
      return "group";
    case NodeType::kImage:
      return "image";
  }
  return "group";
}

const char* storageName(StorageMode mode) {
  switch (mode) {
    case StorageMode::kExternalReference:
      return "external";
    case StorageMode::kInProject:
      return "inProject";
  }
  return "external";
}

const char* rotationName(RegionRotation rotation) {
  switch (rotation) {
    case RegionRotation::kNone:
      return "none";
    case RegionRotation::kDeg90:
      return "90";
    case RegionRotation::kDeg180:
      return "180";
    case RegionRotation::kDeg270:
      return "270";
  }
  return "none";
}

/// 骨架混合模式枚举字符串（v2 schema §3.3）
const char* blendModeName(BlendMode mode) {
  switch (mode) {
    case BlendMode::kNormal:
      return "normal";
    case BlendMode::kAdditive:
      return "additive";
    case BlendMode::kMultiply:
      return "multiply";
    case BlendMode::kScreen:
      return "screen";
  }
  return "normal";
}

// 全部编码器共用：确定性输出 = 键按 RFC §2 的表格行序输出。
class Encoder {
 public:
  Encoder(const ProjectDocument& doc, const std::string& appVersion)
      : doc_(doc), appVersion_(appVersion) {}

  std::string run() {
    rapidjson::StringBuffer buffer;
    Writer writer(buffer);
    writer.SetIndent(' ', 2);
    emitTop(writer);
    std::string out(buffer.GetString(), buffer.GetSize());
    out += '\n';  // RFC §4.5：文件末尾保留单个换行
    return out;
  }

 private:
  // Task 4 B3：写出**统一**为 v2，并声明 minimumReaderVersion = 2 与
  // required loongbones.skeleton.v1（Task4_Implementation_Plan.md B3 第 5 条）。
  // 打开 v1 后只有用户显式保存才写 v2 —— 本函数被 saveProject 调用，即"显式保存"。
  void emitTop(Writer& w) {
    w.StartObject();
    emitKey(w, "formatId");
    emitStr(w, kFormatId);
    emitKey(w, "formatVersion");
    emitInt(w, kFormatVersion);
    emitKey(w, "minimumReaderVersion");
    emitInt(w, kFormatVersion);

    // appVersion：写出时提供（U4）；空串表示省略
    if (!appVersion_.empty()) {
      emitKey(w, "appVersion");
      emitStr(w, appVersion_);
    }
    emitCapabilities(w);
    emitKey(w, "project");
    emitProject(w);

    emitUnknowns(w, doc_.unknowns, OwnerKind::kDocument, "", "");
    w.EndObject();
  }

  /// v2 必须声明的 required 能力集合。缺失项在写出时补齐，已有项不重复写。
  /// optional 条目原样保留（RFC §1.2：可安全忽略，但内容仍须保留）。
  void emitCapabilities(Writer& w) {
    std::vector<std::string> required = doc_.requiredCapabilities;
    for (const char* mandate : {kBaselineCapability, kSkeletonCapability}) {
      const auto found = std::find(required.begin(), required.end(), std::string(mandate));
      if (found == required.end()) required.push_back(mandate);
    }
    w.Key("capabilities");
    w.StartObject();
    emitKey(w, "required");
    w.StartArray();
    for (const auto& item : required) emitStr(w, item);
    w.EndArray();
    emitKey(w, "optional");
    w.StartArray();
    for (const auto& item : doc_.optionalCapabilities) emitStr(w, item);
    w.EndArray();
    emitUnknowns(w, doc_.unknowns, OwnerKind::kDocument, "", "/capabilities");
    w.EndObject();
  }

  void emitProject(Writer& w) {
    const ProjectData& data = doc_.data;
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, data.id.str());
    emitKey(w, "name");
    emitStr(w, data.name);
    emitKey(w, "rootNodeId");
    emitStr(w, data.rootNodeId.str());
    emitKey(w, "defaultSceneIndex");
    emitInt(w, static_cast<std::int64_t>(data.defaultSceneIndex));

    emitKey(w, "scenes");
    w.StartArray();
    for (const auto& scene : data.scenes) emitScene(w, scene);
    w.EndArray();

    emitKey(w, "nodes");
    w.StartArray();
    for (const auto& node : data.nodes) emitNode(w, node);
    w.EndArray();

    emitKey(w, "nodeOrder");
    w.StartArray();
    for (const auto& id : data.nodeOrder) emitStr(w, id.str());
    w.EndArray();

    emitKey(w, "assets");
    w.StartArray();
    for (const auto& asset : data.assets) emitAsset(w, asset);
    w.EndArray();

    emitKey(w, "textures");
    w.StartArray();
    for (const auto& texture : data.textures) emitTexture(w, texture);
    w.EndArray();

    emitKey(w, "atlases");
    w.StartArray();
    for (const auto& atlas : data.atlases) emitAtlas(w, atlas);
    w.EndArray();

    // v2 schema §1：project.skeletons 键**恒存在**，可为空数组。
    // 这是「能在读数据之前检查能力声明」的前提，故即使没有骨架也要写出 []。
    emitKey(w, "skeletons");
    w.StartArray();
    for (const auto& skeleton : data.skeletons) emitSkeleton(w, skeleton);
    w.EndArray();

    emitUnknowns(w, doc_.unknowns, OwnerKind::kProject, data.id.str(), "");
    w.EndObject();
  }

  void emitScene(Writer& w, const Scene& scene) {
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, scene.id.str());
    emitKey(w, "name");
    emitStr(w, scene.name);
    emitKey(w, "rootNodeId");
    emitStr(w, scene.rootNodeId.str());
    emitUnknowns(w, doc_.unknowns, OwnerKind::kScene, scene.id.str(), "");
    w.EndObject();
  }

  void emitNode(Writer& w, const NodeRecord& node) {
    const std::string nodeId = node.common.id.str();
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, nodeId);
    emitKey(w, "name");
    emitStr(w, node.common.name);
    emitKey(w, "parent");
    if (node.common.parent.isNone()) {
      w.Null();
    } else {
      emitStr(w, node.common.parent.str());
    }
    emitKey(w, "children");
    w.StartArray();
    for (const auto& child : node.common.children) emitStr(w, child.str());
    w.EndArray();
    emitKey(w, "transform");
    emitTransform(w, node.common.transform, nodeId);
    emitKey(w, "visible");
    w.Bool(node.common.visible);
    emitKey(w, "locked");
    w.Bool(node.common.locked);
    emitKey(w, "type");
    emitStr(w, nodeTypeName(node.type));
    emitKey(w, "assetId");
    if (node.assetId.has_value()) {
      emitStr(w, node.assetId->str());
    } else {
      w.Null();
    }
    emitKey(w, "regionId");
    if (node.regionId.has_value()) {
      emitStr(w, node.regionId->str());
    } else {
      w.Null();
    }
    emitUnknowns(w, doc_.unknowns, OwnerKind::kNode, nodeId, "");
    w.EndObject();
  }

  void emitTransform(Writer& w, const Transform& t, const std::string& nodeId) {
    w.StartObject();
    emitKey(w, "x");
    emitDouble(w, t.x);
    emitKey(w, "y");
    emitDouble(w, t.y);
    emitKey(w, "rotationDeg");
    emitDouble(w, t.rotationDeg);
    emitKey(w, "scaleX");
    emitDouble(w, t.scaleX);
    emitKey(w, "scaleY");
    emitDouble(w, t.scaleY);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kNode, nodeId, "/transform");
    w.EndObject();
  }

  void emitAsset(Writer& w, const ImageAsset& asset) {
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, asset.id.str());
    emitKey(w, "name");
    emitStr(w, asset.name);
    emitKey(w, "type");
    emitStr(w, "static");
    emitKey(w, "storage");
    emitStr(w, storageName(asset.storage));
    emitKey(w, "location");
    emitStr(w, asset.location);
    emitKey(w, "pixelWidth");
    if (asset.pixelWidth.has_value()) {
      emitInt(w, *asset.pixelWidth);
    } else {
      w.Null();
    }
    emitKey(w, "pixelHeight");
    if (asset.pixelHeight.has_value()) {
      emitInt(w, *asset.pixelHeight);
    } else {
      w.Null();
    }
    emitUnknowns(w, doc_.unknowns, OwnerKind::kImageAsset, asset.id.str(), "");
    w.EndObject();
  }

  void emitTexture(Writer& w, const TextureAsset& texture) {
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, texture.id.str());
    emitKey(w, "name");
    emitStr(w, texture.name);
    emitKey(w, "storage");
    emitStr(w, storageName(texture.storage));
    emitKey(w, "location");
    emitStr(w, texture.location);
    emitKey(w, "pixelWidth");
    if (texture.pixelWidth.has_value()) {
      emitInt(w, *texture.pixelWidth);
    } else {
      w.Null();
    }
    emitKey(w, "pixelHeight");
    if (texture.pixelHeight.has_value()) {
      emitInt(w, *texture.pixelHeight);
    } else {
      w.Null();
    }
    emitUnknowns(w, doc_.unknowns, OwnerKind::kTextureAsset, texture.id.str(), "");
    w.EndObject();
  }

  void emitAtlas(Writer& w, const Atlas& atlas) {
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, atlas.id.str());
    emitKey(w, "name");
    emitStr(w, atlas.name);
    emitKey(w, "pages");
    w.StartArray();
    for (const auto& page : atlas.pages) emitPage(w, page);
    w.EndArray();
    emitKey(w, "regions");
    w.StartArray();
    for (const auto& region : atlas.regions) emitRegion(w, region);
    w.EndArray();
    emitUnknowns(w, doc_.unknowns, OwnerKind::kAtlas, atlas.id.str(), "");
    w.EndObject();
  }

  void emitPage(Writer& w, const AtlasPage& page) {
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, page.id.str());
    emitKey(w, "name");
    emitStr(w, page.name);
    emitKey(w, "textureAssetId");
    emitStr(w, page.textureAssetId.str());
    emitKey(w, "width");
    emitInt(w, page.width);
    emitKey(w, "height");
    emitInt(w, page.height);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kAtlasPage, page.id.str(), "");
    w.EndObject();
  }

  void emitRegion(Writer& w, const AtlasRegion& region) {
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, region.id.str());
    emitKey(w, "name");
    emitStr(w, region.name);
    emitKey(w, "pageId");
    emitStr(w, region.pageId.str());
    emitKey(w, "packedX");
    emitInt(w, region.packedX);
    emitKey(w, "packedY");
    emitInt(w, region.packedY);
    emitKey(w, "packedW");
    emitInt(w, region.packedW);
    emitKey(w, "packedH");
    emitInt(w, region.packedH);
    emitKey(w, "rotation");
    emitStr(w, rotationName(region.rotation));
    emitKey(w, "originalW");
    emitInt(w, region.originalW);
    emitKey(w, "originalH");
    emitInt(w, region.originalH);
    emitKey(w, "trimmedW");
    emitInt(w, region.trimmedW);
    emitKey(w, "trimmedH");
    emitInt(w, region.trimmedH);
    emitKey(w, "offsetX");
    emitInt(w, region.offsetX);
    emitKey(w, "offsetY");
    emitInt(w, region.offsetY);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kAtlasRegion, region.id.str(), "");
    w.EndObject();
  }

  // ===== 骨架层（v2；键序与 v2 schema §3 的表格行序一致）=====

  /// ColorF 无 ID，宿主是外层对象（Slot / Attachment），子路径 /color。
  void emitColor(Writer& w, const ColorF& color, OwnerKind hostKind, const std::string& hostId) {
    w.StartObject();
    emitKey(w, "r");
    emitDouble(w, color.r);
    emitKey(w, "g");
    emitDouble(w, color.g);
    emitKey(w, "b");
    emitDouble(w, color.b);
    emitKey(w, "a");
    emitDouble(w, color.a);
    emitUnknowns(w, doc_.unknowns, hostKind, hostId, "/color");
    w.EndObject();
  }

  void emitIdArray(Writer& w, const std::vector<Id>& ids) {
    w.StartArray();
    for (const auto& id : ids) emitStr(w, id.str());
    w.EndArray();
  }

  void emitSkeleton(Writer& w, const Skeleton& skeleton) {
    const std::string self = skeleton.id.str();
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, self);
    emitKey(w, "name");
    emitStr(w, skeleton.name);

    emitKey(w, "atlasIds");
    emitIdArray(w, skeleton.atlasIds);

    emitKey(w, "boneOrder");
    emitIdArray(w, skeleton.boneOrder);

    emitKey(w, "bones");
    w.StartArray();
    for (const auto& bone : skeleton.bones) emitBone(w, bone);
    w.EndArray();

    emitKey(w, "drawOrder");
    emitIdArray(w, skeleton.drawOrder);

    emitKey(w, "slots");
    w.StartArray();
    for (const auto& slot : skeleton.slots) emitSlot(w, slot);
    w.EndArray();

    emitKey(w, "skins");
    w.StartArray();
    for (const auto& skin : skeleton.skins) emitSkin(w, skin);
    w.EndArray();

    // 第二批约束（Task 5 B2，v3 schema §3）：两个键**恒存在**，可为空数组。
    emitKey(w, "ikConstraints");
    w.StartArray();
    for (const auto& c : skeleton.ikConstraints) emitIkConstraint(w, c, self);
    w.EndArray();

    emitKey(w, "transformConstraints");
    w.StartArray();
    for (const auto& c : skeleton.transformConstraints) emitTransformConstraint(w, c, self);
    w.EndArray();

    // sourceRef：**整个可选**。省略（原生创建）时不写这个键；
    // null 从不写出 —— 缺失就是用「省略」表达（v2 schema §3.7）。
    if (skeleton.sourceRef.has_value()) {
      emitKey(w, "sourceRef");
      emitSourceRef(w, *skeleton.sourceRef, self);
    }

    emitUnknowns(w, doc_.unknowns, OwnerKind::kSkeleton, self, "");
    w.EndObject();
  }

  /// IK 约束（v3 schema §3）。`mix` 写源原值；`enabled` 是**是否开启**（D35），
  /// 二者并列写出，避免"只有 0/1"的现状被误当成语义上限。
  void emitIkConstraint(Writer& w, const IkConstraint& c, const std::string& skeletonId) {
    (void)skeletonId;  // 约束自身有稳定 ID，未知字段锚点用它自己的 ID
    const std::string self = c.id.str();
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, self);
    emitKey(w, "name");
    emitStr(w, c.name);
    emitKey(w, "boneIds");
    emitIdArray(w, c.boneIds);
    emitKey(w, "targetBoneId");
    emitStr(w, c.targetBoneId.str());
    emitKey(w, "mix");
    emitDouble(w, c.mix);
    emitKey(w, "enabled");
    w.Bool(c.enabled);
    emitKey(w, "bendPositive");
    w.Bool(c.bendPositive);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kIkConstraint, self, "");
    w.EndObject();
  }

  /// Transform 约束：4 个 mix + 6 个 offset **原值写出、不解释语义**（D36）。
  void emitTransformConstraint(Writer& w, const TransformConstraint& c,
                               const std::string& skeletonId) {
    (void)skeletonId;  // 同上
    const std::string self = c.id.str();
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, self);
    emitKey(w, "name");
    emitStr(w, c.name);
    emitKey(w, "boneId");
    emitStr(w, c.boneId.str());
    emitKey(w, "targetBoneId");
    emitStr(w, c.targetBoneId.str());
    emitKey(w, "rotateMix");
    emitDouble(w, c.rotateMix);
    emitKey(w, "translateMix");
    emitDouble(w, c.translateMix);
    emitKey(w, "scaleMix");
    emitDouble(w, c.scaleMix);
    emitKey(w, "shearMix");
    emitDouble(w, c.shearMix);
    emitKey(w, "offsetRotation");
    emitDouble(w, c.offsetRotation);
    emitKey(w, "offsetX");
    emitDouble(w, c.offsetX);
    emitKey(w, "offsetY");
    emitDouble(w, c.offsetY);
    emitKey(w, "offsetScaleX");
    emitDouble(w, c.offsetScaleX);
    emitKey(w, "offsetScaleY");
    emitDouble(w, c.offsetScaleY);
    emitKey(w, "offsetShearY");
    emitDouble(w, c.offsetShearY);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kTransformConstraint, self, "");
    w.EndObject();
  }

  void emitBone(Writer& w, const Bone& bone) {
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, bone.id.str());
    emitKey(w, "name");
    emitStr(w, bone.name);
    emitKey(w, "parentId");
    if (bone.parent.isNone()) {
      w.Null();
    } else {
      emitStr(w, bone.parent.str());
    }
    emitKey(w, "x");
    emitDouble(w, bone.x);
    emitKey(w, "y");
    emitDouble(w, bone.y);
    emitKey(w, "rotationDeg");
    emitDouble(w, bone.rotationDeg);
    emitKey(w, "scaleX");
    emitDouble(w, bone.scaleX);
    emitKey(w, "scaleY");
    emitDouble(w, bone.scaleY);
    emitKey(w, "shearXDeg");
    emitDouble(w, bone.shearXDeg);
    emitKey(w, "shearYDeg");
    emitDouble(w, bone.shearYDeg);
    emitKey(w, "inheritRotation");
    w.Bool(bone.inheritRotation);
    emitKey(w, "inheritScale");
    w.Bool(bone.inheritScale);
    emitKey(w, "length");
    emitDouble(w, bone.length);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kBone, bone.id.str(), "");
    w.EndObject();
  }

  void emitSlot(Writer& w, const Slot& slot) {
    const std::string self = slot.id.str();
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, self);
    emitKey(w, "name");
    emitStr(w, slot.name);
    emitKey(w, "boneId");
    emitStr(w, slot.boneId.str());
    emitKey(w, "color");
    emitColor(w, slot.color, OwnerKind::kSlot, self);
    emitKey(w, "attachmentName");
    if (slot.attachmentName.has_value()) {
      emitStr(w, *slot.attachmentName);
    } else {
      w.Null();
    }
    emitKey(w, "blendMode");
    emitStr(w, blendModeName(slot.blendMode));
    emitUnknowns(w, doc_.unknowns, OwnerKind::kSlot, self, "");
    w.EndObject();
  }

  void emitSkin(Writer& w, const Skin& skin) {
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, skin.id.str());
    emitKey(w, "name");
    emitStr(w, skin.name);
    emitKey(w, "entries");
    w.StartArray();
    for (const auto& entry : skin.entries) emitSkinEntry(w, entry);
    w.EndArray();
    emitUnknowns(w, doc_.unknowns, OwnerKind::kSkin, skin.id.str(), "");
    w.EndObject();
  }

  void emitSkinEntry(Writer& w, const SkinEntry& entry) {
    const std::string self = entry.id.str();
    w.StartObject();
    emitKey(w, "id");
    emitStr(w, self);
    emitKey(w, "slotId");
    emitStr(w, entry.slotId.str());
    emitKey(w, "attachmentName");
    emitStr(w, entry.attachmentName);
    emitKey(w, "attachment");
    emitAttachment(w, entry.attachment, self);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kSkinEntry, self, "");
    w.EndObject();
  }

  /// Attachment 无独立 ID：宿主是 SkinEntry，子路径 /attachment（v2 schema §5.2）。
  /// v3 起按 `type` 分派 4 种：region / mesh / weightedMesh / linkedMesh。
  void emitAttachment(Writer& w, const Attachment& attachment, const std::string& entryId) {
    if (const auto* region = std::get_if<RegionAttachment>(&attachment)) {
      emitRegionAttachment(w, *region, entryId);
    } else if (const auto* mesh = std::get_if<MeshAttachment>(&attachment)) {
      emitMeshAttachment(w, *mesh, entryId);
    } else if (const auto* weighted = std::get_if<WeightedMeshAttachment>(&attachment)) {
      emitWeightedMeshAttachment(w, *weighted, entryId);
    } else {
      emitLinkedMeshAttachment(w, std::get<LinkedMeshAttachment>(attachment), entryId);
    }
  }

  void emitRegionAttachment(Writer& w, const RegionAttachment& attachment,
                            const std::string& entryId) {
    w.StartObject();
    emitKey(w, "type");
    emitStr(w, "region");
    emitKey(w, "regionId");
    emitStr(w, attachment.regionId.str());
    emitKey(w, "x");
    emitDouble(w, attachment.x);
    emitKey(w, "y");
    emitDouble(w, attachment.y);
    emitKey(w, "rotationDeg");
    emitDouble(w, attachment.rotationDeg);
    emitKey(w, "scaleX");
    emitDouble(w, attachment.scaleX);
    emitKey(w, "scaleY");
    emitDouble(w, attachment.scaleY);
    emitKey(w, "width");
    emitDouble(w, attachment.width);
    emitKey(w, "height");
    emitDouble(w, attachment.height);
    emitKey(w, "color");
    emitColor(w, attachment.color, OwnerKind::kSkinEntry, entryId);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kSkinEntry, entryId, "/attachment");
    w.EndObject();
  }

  /// 非加权 mesh（v3 schema §3）：顶点 = 槽骨骼局部坐标（N2）。
  void emitMeshAttachment(Writer& w, const MeshAttachment& a, const std::string& entryId) {
    w.StartObject();
    emitKey(w, "type");
    emitStr(w, "mesh");
    emitKey(w, "regionId");
    emitStr(w, a.regionId.str());
    emitKey(w, "uvs");
    w.StartArray();
    for (double v : a.uvs) emitDouble(w, v);
    w.EndArray();
    emitKey(w, "triangles");
    w.StartArray();
    for (std::uint32_t t : a.triangles) w.Uint(static_cast<unsigned>(t));
    w.EndArray();
    emitKey(w, "vertices");
    w.StartArray();
    for (double v : a.vertices) emitDouble(w, v);
    w.EndArray();
    emitKey(w, "width");
    emitDouble(w, a.width);
    emitKey(w, "height");
    emitDouble(w, a.height);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kSkinEntry, entryId, "/attachment");
    w.EndObject();
  }

  /// 加权 mesh：几何**只**落在 `vertexBindings`（顶点数 = 外层长度），权重写源原值。
  void emitWeightedMeshAttachment(Writer& w, const WeightedMeshAttachment& a,
                                  const std::string& entryId) {
    w.StartObject();
    emitKey(w, "type");
    emitStr(w, "weightedMesh");
    emitKey(w, "regionId");
    emitStr(w, a.regionId.str());
    emitKey(w, "uvs");
    w.StartArray();
    for (double v : a.uvs) emitDouble(w, v);
    w.EndArray();
    emitKey(w, "triangles");
    w.StartArray();
    for (std::uint32_t t : a.triangles) w.Uint(static_cast<unsigned>(t));
    w.EndArray();
    emitKey(w, "vertexBindings");
    w.StartArray();
    for (std::size_t i = 0; i < a.vertexBindings.size(); ++i) {
      const std::vector<VertexBinding>& binds = a.vertexBindings[i];
      w.StartArray();
      for (std::size_t j = 0; j < binds.size(); ++j) {
        const VertexBinding& b = binds[j];
        w.StartObject();
        emitKey(w, "boneId");
        emitStr(w, b.boneId.str());
        emitKey(w, "x");
        emitDouble(w, b.x);
        emitKey(w, "y");
        emitDouble(w, b.y);
        emitKey(w, "weight");
        emitDouble(w, b.weight);
        // 绑定对象内的未知字段（路径与 decode 侧**逐字一致**）
        emitUnknowns(w, doc_.unknowns, OwnerKind::kSkinEntry, entryId,
                     "/attachment/vertexBindings/" + std::to_string(i) + "/" +
                         std::to_string(j));
        w.EndObject();
      }
      w.EndArray();
    }
    w.EndArray();
    emitKey(w, "width");
    emitDouble(w, a.width);
    emitKey(w, "height");
    emitDouble(w, a.height);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kSkinEntry, entryId, "/attachment");
    w.EndObject();
  }

  /// linked mesh：**不复制几何**，只写 `parentKey`（v3 schema §3）。
  void emitLinkedMeshAttachment(Writer& w, const LinkedMeshAttachment& a,
                                const std::string& entryId) {
    w.StartObject();
    emitKey(w, "type");
    emitStr(w, "linkedMesh");
    emitKey(w, "regionId");
    emitStr(w, a.regionId.str());
    emitKey(w, "parentKey");
    w.StartObject();
    emitKey(w, "slotId");
    emitStr(w, a.parentSlotId.str());
    emitKey(w, "attachmentName");
    emitStr(w, a.parentAttachmentName);
    // parentKey 内的未知字段（与 decode 侧路径**逐字一致**）
    emitUnknowns(w, doc_.unknowns, OwnerKind::kSkinEntry, entryId, "/attachment/parentKey");
    w.EndObject();
    emitKey(w, "inheritDeform");
    w.Bool(a.inheritDeform);
    emitUnknowns(w, doc_.unknowns, OwnerKind::kSkinEntry, entryId, "/attachment");
    w.EndObject();
  }

  void emitSourceRef(Writer& w, const SourceRef& ref, const std::string& skeletonId) {
    const OwnerAnchor anchor{OwnerKind::kSkeleton, skeletonId};
    w.StartObject();
    emitKey(w, "formatId");
    emitStr(w, ref.formatId);
    emitKey(w, "formatVersion");
    emitStr(w, ref.formatVersion);
    if (ref.sourceHeaderHash.has_value()) {
      emitKey(w, "sourceHeaderHash");
      emitStr(w, *ref.sourceHeaderHash);
    }
    emitKey(w, "contentHash");
    emitStr(w, ref.contentHash);
    if (ref.sourceUnit.has_value()) {
      emitKey(w, "sourceUnit");
      emitStr(w, *ref.sourceUnit);
    }
    emitKey(w, "importedBy");
    w.StartObject();
    emitKey(w, "pluginId");
    emitStr(w, ref.importedByPluginId);
    emitKey(w, "pluginVersion");
    emitInt(w, ref.importedByPluginVersion);
    emitUnknowns(w, doc_.unknowns, anchor.kind, anchor.id, "/sourceRef/importedBy");
    w.EndObject();
    emitUnknowns(w, doc_.unknowns, anchor.kind, anchor.id, "/sourceRef");
    w.EndObject();
  }

  const ProjectDocument& doc_;
  const std::string& appVersion_;
};

}  // namespace

std::string encodeDocument(const ProjectDocument& doc, const std::string& appVersion) {
  Encoder encoder(doc, appVersion);
  return encoder.run();
}

}  // namespace format
}  // namespace loongbones
