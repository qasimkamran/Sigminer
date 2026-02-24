#pragma once

#include <string>
#include <optional>

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
    std::optional<sigminer::Signature> Sig = std::nullopt;
    ReturnCode RetCode = ReturnCode::SUCCESS;
};

Result GetSignatureFromSharedObjectBySymbol(
        const std::string& SharedObjectFilePath,
        const std::string& Symbol );

} // namespace sigminer

