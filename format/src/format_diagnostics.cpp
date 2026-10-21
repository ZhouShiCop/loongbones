#include "loongbones/format/format_diagnostics.hpp"

namespace loongbones {
namespace format {

const char* toString(FormatDiagnosticCode code) noexcept {
  switch (code) {
    case FormatDiagnosticCode::kNone:
      return "NONE";
    case FormatDiagnosticCode::kJsonSyntaxError:
      return "JSON_SYNTAX_ERROR";
    case FormatDiagnosticCode::kJsonTypeMismatch:
      return "JSON_TYPE_MISMATCH";
    case FormatDiagnosticCode::kJsonInvalidUtf8:
      return "JSON_INVALID_UTF8";
    case FormatDiagnosticCode::kJsonDuplicateKey:
      return "JSON_DUPLICATE_KEY";
    case FormatDiagnosticCode::kJsonNonFiniteNumber:
      return "JSON_NON_FINITE_NUMBER";
    case FormatDiagnosticCode::kJsonNumberOutOfRange:
      return "JSON_NUMBER_OUT_OF_RANGE";
    case FormatDiagnosticCode::kJsonDepthLimit:
      return "JSON_DEPTH_LIMIT";
    case FormatDiagnosticCode::kJsonSizeLimit:
      return "JSON_SIZE_LIMIT";
    case FormatDiagnosticCode::kFormatIdUnsupported:
      return "FORMAT_ID_UNSUPPORTED";
    case FormatDiagnosticCode::kFormatVersionUnsupported:
      return "FORMAT_VERSION_UNSUPPORTED";
    case FormatDiagnosticCode::kFormatTooNew:
      return "FORMAT_TOO_NEW";
    case FormatDiagnosticCode::kCapabilityUnsupported:
      return "CAPABILITY_UNSUPPORTED";
    case FormatDiagnosticCode::kFormatMissingField:
      return "FORMAT_MISSING_FIELD";
    case FormatDiagnosticCode::kFormatWriteFailed:
      return "FORMAT_WRITE_FAILED";
    case FormatDiagnosticCode::kFormatReadFailed:
      return "FORMAT_READ_FAILED";
  }
  return "UNKNOWN_FORMAT_DIAGNOSTIC";
}

std::string FormatDiagnostic::str() const {
  std::string s = toString(code);
  if (!location.empty() || hasOffset) {
    s += " at ";
    s += location.empty() ? "/" : location;
    if (hasOffset) {
      s += "@";
      s += std::to_string(offset);
    }
  }
  s += ": ";
  s += message;
  return s;
}

}  // namespace format
}  // namespace loongbones
