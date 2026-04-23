#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sigminer/signature.h"
#include "sigminer/sigminer.h"

namespace sigminer {

struct AddressRange
{
    std::uint64_t lowPc = 0;
    std::uint64_t highPc = 0;
};

class FunctionInfo
{
public:
    FunctionInfo() = default;

    std::string name{};
    Signature sig{};
    std::vector<AddressRange> addressRanges{};
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
