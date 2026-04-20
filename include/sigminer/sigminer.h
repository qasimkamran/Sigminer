#pragma once

#include <optional>
#include <string>

#include "sigminer/signature.h"

namespace sigminer {

enum class ReturnCode
{
    SUCCESS,
    INVALID_INPUT,
    FILE_OPEN_FAILURE,
    SYMBOL_RESOLUTION_FAILURE,
    DWARF_UNAVAILABLE,
    FUNCTION_DIE_NOT_IN_RANGE,
    UNSUPPORTED_TYPE,
    INTERNAL_FAILURE,
};

class Result
{
public:
    std::optional<Signature> sig = std::nullopt;
    ReturnCode retCode = ReturnCode::SUCCESS;
};

Result GetSignatureFromSharedObjectBySymbol(
        const std::string& sharedObjectFilePath,
        const std::string& symbol);

struct RichResult
{
    std::optional<RichSignature> sig = std::nullopt;
    ReturnCode retCode = ReturnCode::SUCCESS;
};

RichResult GetRichSignatureFromSharedObjectBySymbol(
        const std::string& sharedObjectFilePath,
        const std::string& symbol);

} // namespace sigminer
