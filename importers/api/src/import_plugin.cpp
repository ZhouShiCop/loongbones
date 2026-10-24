#include "loongbones/importers/api/import_plugin.hpp"

namespace loongbones::importers::api {

std::size_t ImportReport::errorCount() const noexcept {
  std::size_t n = 0;
  for (const ImportDiagnostic& d : diagnostics) {
    if (d.severity == ImportSeverity::kError) ++n;
  }
  return n;
}

std::size_t ImportReport::warningCount() const noexcept {
  std::size_t n = 0;
  for (const ImportDiagnostic& d : diagnostics) {
    if (d.severity == ImportSeverity::kWarning) ++n;
  }
  return n;
}

const char* toString(ImportCode code) noexcept {
  switch (code) {
    case ImportCode::kNone: return "IMPORT_NONE";
    case ImportCode::kSourceMissing: return "IMPORT_SOURCE_MISSING";
    case ImportCode::kSourceRejected: return "IMPORT_SOURCE_REJECTED";
    case ImportCode::kParseError: return "IMPORT_PARSE_ERROR";
    case ImportCode::kResourceMissing: return "IMPORT_RESOURCE_MISSING";
    case ImportCode::kResourceInvalid: return "IMPORT_RESOURCE_INVALID";
    case ImportCode::kUnresolvedReference: return "IMPORT_UNRESOLVED_REFERENCE";
    case ImportCode::kAttachmentRegionDegraded: return "IMPORT_ATTACHMENT_REGION_DEGRADED";
    case ImportCode::kUnsupportedCapability: return "IMPORT_UNSUPPORTED_CAPABILITY";
    case ImportCode::kDomainRejected: return "IMPORT_DOMAIN_REJECTED";
    case ImportCode::kResourceStageFailed: return "IMPORT_STAGE_FAILED";
    case ImportCode::kProjectSaveFailed: return "IMPORT_SAVE_FAILED";
    case ImportCode::kWarningsNotAccepted: return "IMPORT_WARNINGS_NOT_ACCEPTED";
    case ImportCode::kCancelled: return "IMPORT_CANCELLED";
    case ImportCode::kResourceAmbiguous: return "IMPORT_RESOURCE_AMBIGUOUS";
  }
  return "IMPORT_UNKNOWN";
}

}  // namespace loongbones::importers::api
