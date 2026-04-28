#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "sigminer/signature.h"
#include "sigminer/sigminer.h"

namespace bpftrace_mapper {

enum class BpftraceProbeKind
{
    ENTRY,
    RETURN
};

struct BpftraceProbeTarget
{
    std::string modulePath;
    std::string symbol;
};

struct BpftraceRenderOptions
{
    std::optional<int> pid{};
    bool includeEntryProbe = true;
    bool includeReturnProbe = true;
    bool includeTimingMs = true;
    bool includeUserStack = false;
    bool includeArgumentPrinting = true;
    bool includeReturnPrinting = true;
};

struct BpftraceResolvedSymbol
{
    BpftraceProbeTarget target;
    sigminer::Signature sig{};
};

sigminer::Result FindSignatureInModulesBySymbol(
        const std::vector<std::string>& modulePaths,
        const std::string& symbol,
        BpftraceResolvedSymbol* resolved);

std::string BuildBpftraceArgumentPrintExpr(const sigminer::Signature& sig);

std::string BuildBpftraceReturnPrintExpr(const sigminer::TypeEntry& retType);

std::string BuildBpftraceProbeBody(
        BpftraceProbeKind probeKind,
        const sigminer::Signature& sig,
        const BpftraceRenderOptions& opts);

std::string BuildBpftraceUprobeScript(
        const BpftraceProbeTarget& target,
        const sigminer::Signature& sig,
        const BpftraceRenderOptions& opts);

std::string BuildBpftraceUprobeScript(
        const std::vector<BpftraceResolvedSymbol>& resolvedSymbols,
        const BpftraceRenderOptions& opts);

namespace rich {

struct ExtendedBpftraceRenderOptions : BpftraceRenderOptions
{
    std::size_t maxAggregateDepth = 2;
    std::size_t maxAggregateMembers = 16;
    std::size_t maxArrayElements = 8;
};

std::string BuildBpftraceUprobeScript(
        const BpftraceProbeTarget& target,
        const sigminer::rich::Signature& sig,
        const ExtendedBpftraceRenderOptions& opts);

} // namespace rich

} // namespace bpftrace_mapper
