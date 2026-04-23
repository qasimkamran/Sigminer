#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <llvm/DebugInfo/DWARF/DWARFAddressRange.h>

#include "sigminer/signature.h"
#include "sigminer/sigminer.h"

namespace sigminer {

struct ReturnSite {
    uint64_t instructionAddress;
    uint64_t funcOffset;
};

class FunctionInfo
{
public:
    FunctionInfo() = default;

    std::string name{};
    Signature sig{};
    llvm::DWARFAddressRangesVector addressRanges{};
};

class FunctionInfoResult
{
public:
    std::optional<FunctionInfo> functionInfo = std::nullopt;
    ReturnCode retCode = ReturnCode::SUCCESS;
};

FunctionInfoResult GetFunctionInfoFromSharedObjectBySymbol(
        const std::string& sharedObjectFilePath,
        const std::string& symbol);

} // namespace sigminer
