#pragma once

#include <optional>
#include <string>
#include <vector>

#include "sigminer/signature.h"
#include "sigminer/sigminer.h"

namespace sigminer {

enum class BpftraceProbeKind
{
    ENTRY,
    RETURN
};

struct BpftraceProbeTarget
{
    std::string ModulePath;
    std::string Symbol;
};

struct BpftraceRenderOptions
{
    std::optional<int> Pid{};
    bool IncludeEntryProbe = true;
    bool IncludeReturnProbe = true;
    bool IncludeTimingMs = true;
    bool IncludeUserStack = false;
    bool IncludeArgumentPrinting = true;
    bool IncludeReturnPrinting = true;
};

struct BpftraceResolvedSymbol
{
    BpftraceProbeTarget Target;
    Signature Sig{};
};

Result FindSignatureInModulesBySymbol(
        const std::vector<std::string>& ModulePaths,
        const std::string& Symbol,
        BpftraceResolvedSymbol* Resolved);

std::string BuildBpftraceArgumentPrintExpr(const Signature& Sig);

std::string BuildBpftraceReturnPrintExpr(const TypeEntry& RetType);

std::string BuildBpftraceProbeBody(
        BpftraceProbeKind ProbeKind,
        const Signature& Sig,
        const BpftraceRenderOptions& Options);

std::string BuildBpftraceUprobeScript(
        const BpftraceProbeTarget& Target,
        const Signature& Sig,
        const BpftraceRenderOptions& Options);

std::string BuildBpftraceUprobeScript(
        const std::vector<BpftraceResolvedSymbol>& ResolvedSymbols,
        const BpftraceRenderOptions& Options);

} // namespace sigminer
