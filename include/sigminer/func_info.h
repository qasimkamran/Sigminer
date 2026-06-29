#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <llvm/DebugInfo/DWARF/DWARFAddressRange.h>

#include "sigminer/signature.h"
#include "sigminer/sigminer.h"

namespace sigminer {

struct SourceLocation
{
    std::string file{};
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

struct ReturnSite
{
    std::uint64_t instructionAddress = 0;
    std::uint64_t funcOffset = 0;
    std::optional<SourceLocation> sourceLocation = std::nullopt;
};

struct SourceReturnCandidate
{
    std::string functionName{};
    SourceLocation spellingLocation{};
    SourceLocation expansionLocation{};
    std::optional<std::uint32_t> discriminator = std::nullopt;
};

struct SourceReturnProbePoint
{
    std::uint64_t instructionAddress = 0;
    std::uint64_t funcOffset = 0;
    SourceLocation mappedLocation{};
    bool matchedEpilogue = false;
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
    ReturnCode retCode = ReturnCode::INTERNAL_FAILURE;
};

FunctionInfoResult GetFunctionInfoFromSharedObjectBySymbol(
        const std::string& sharedObjectFilePath,
        const std::string& symbol);

} // namespace sigminer
