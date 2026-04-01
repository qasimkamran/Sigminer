#include "internal/bpftrace_mapper.h"

#include <string>
#include <vector>

#include "internal/printf_specifier_mapper.h"

namespace bpftrace_mapper {
namespace {

std::string BuildProbeLabel(
        const BpftraceProbeTarget& target,
        BpftraceProbeKind probeKind)
{
    const char* probeLabel = (probeKind == BpftraceProbeKind::ENTRY) ? "entry" : "return";
    return target.modulePath + ":" + target.symbol + " [" + probeLabel + "]";
}

std::string BuildProbeSelector(const BpftraceRenderOptions& opts)
{
    if (!opts.pid.has_value())
        return {};

    return " /pid == " + std::to_string(*opts.pid) + "/";
}

std::string BuildProbeHeader(
        const BpftraceProbeTarget& target,
        BpftraceProbeKind probeKind)
{
    return "  printf(\"" + BuildProbeLabel(target, probeKind) + "\\n\");\n";
}

std::string BuildTimingKey(const BpftraceProbeTarget& target)
{
    return target.modulePath + ":" + target.symbol;
}

} // namespace

sigminer::Result FindSignatureInModulesBySymbol(
        const std::vector<std::string>& modulePaths,
        const std::string& symbol,
        BpftraceResolvedSymbol* resolved)
{
    sigminer::Result lastResult{};
    lastResult.retCode = sigminer::ReturnCode::INVALID_INPUT;

    if (modulePaths.empty() || symbol.empty()) {
        return lastResult;
    }

    for (const std::string& modulePath : modulePaths)
    {
        sigminer::Result result = sigminer::GetSignatureFromSharedObjectBySymbol(modulePath, symbol);
        
        if (result.retCode != sigminer::ReturnCode::SUCCESS || !result.sig) {
            lastResult = std::move(result);
            continue;
        }

        if (resolved) {
            resolved->target.modulePath = modulePath;
            resolved->target.symbol = symbol;
            resolved->sig = *result.sig;
        }
        return result;
    }
    return lastResult;
}

std::string BuildBpftraceArgumentPrintExpr(const sigminer::Signature& sig)
{
    if (sig.params.empty())
        return "  printf(\"    args: (none)\\n\");\n";

    std::string specifiers = printf_specifier_mapper::TypeEntriesToPrintfSpecifier(sig.params);
    
    if (specifiers.empty())
        return {};

    std::string argsList;
    for (std::size_t i = 0; i < sig.params.size(); ++i) {
        if (i > 0)
            argsList += ", ";

        argsList += "arg" + std::to_string(i);
    }

    return "  printf(\"    args: " + specifiers + "\\n\", " + argsList + ");\n";
}

std::string BuildBpftraceReturnPrintExpr(const sigminer::TypeEntry& retType)
{
    std::string specifier =
            printf_specifier_mapper::TypeEntryToPrintfSpecifier(retType);
    if (specifier.empty())
        return {};

    if (retType.kind == sigminer::PrimitiveKind::VOID)
        return "  printf(\"    retval: (void)\\n\");\n";

    if (retType.kind == sigminer::PrimitiveKind::BOOL) {
        return "  $ret = (uint8)retval;\n"
               "  printf(\"    retval: " + specifier + "\\n\", $ret);\n";
    }

    if (retType.kind == sigminer::PrimitiveKind::INT ||
        retType.kind == sigminer::PrimitiveKind::ENUM) {
        std::string cast;

        switch (retType.size) {
            case 1:
                cast = (retType.sign == sigminer::Signedness::SIGNED) ? "int8" : "uint8";
                break;

            case 2:
                cast = (retType.sign == sigminer::Signedness::SIGNED) ? "int16" : "uint16";
                break;

            case 4:
                cast = (retType.sign == sigminer::Signedness::SIGNED) ? "int32" : "uint32";
                break;

            case 8:
                cast = (retType.sign == sigminer::Signedness::SIGNED) ? "int64" : "uint64";
                break;

            default:
                break;
        }

        if (!cast.empty()) {
            return "  $ret = (" + cast + ")retval;\n"
                   "  printf(\"    retval: " + specifier + "\\n\", $ret);\n";
        }
    }

    return "  printf(\"    retval: " + specifier + "\\n\", retval);\n";
}

std::string BuildBpftraceProbeBody(
        BpftraceProbeKind probeKind,
        const sigminer::Signature& sig,
        const BpftraceRenderOptions& opts)
{
    std::string body;

    if (probeKind == BpftraceProbeKind::ENTRY)
    {
        if (opts.includeTimingMs)
            body += "  @start[tid] = nsecs;\n";

        if (opts.includeArgumentPrinting)
            body += BuildBpftraceArgumentPrintExpr(sig);
    }
    else
    {
        if (opts.includeReturnPrinting)
            body += BuildBpftraceReturnPrintExpr(sig.ret);

        if (opts.includeTimingMs) {
            body += "  if (@start[tid]) {\n";
            body += "    $elapsed_ms = (nsecs - @start[tid]) / 1000000;\n";
            body += "    printf(\"    elapsed_ms: %llu\\n\", $elapsed_ms);\n";
            body += "    delete(@start[tid]);\n";
            body += "  }\n";
        }
    }

    if (opts.includeUserStack)
        body += "  print(ustack());\n";

    return body;
}

std::string BuildBpftraceUprobeScript(
        const BpftraceProbeTarget& target,
        const sigminer::Signature& sig,
        const BpftraceRenderOptions& opts)
{
    std::string selector = BuildProbeSelector(opts);
    std::string script;

    if (opts.includeEntryProbe) {
        script += "uprobe:" + target.modulePath + ":" + target.symbol + selector + "\n";
        script += "{\n";
        script += BuildProbeHeader(target, BpftraceProbeKind::ENTRY);
        script += BuildBpftraceProbeBody(BpftraceProbeKind::ENTRY, sig, opts);
        script += "}\n";
    }

    if (opts.includeReturnProbe) {
        if (!script.empty())
            script += "\n";

        script += "uretprobe:" + target.modulePath + ":" + target.symbol + selector + "\n";
        script += "{\n";
        script += BuildProbeHeader(target, BpftraceProbeKind::RETURN);
        script += BuildBpftraceProbeBody(BpftraceProbeKind::RETURN, sig, opts);
        script += "}\n";
    }

    return script;
}

std::string BuildBpftraceUprobeScript(
        const std::vector<BpftraceResolvedSymbol>& resolvedSymbols,
        const BpftraceRenderOptions& opts)
{
    std::string script;

    for (const BpftraceResolvedSymbol& resolved : resolvedSymbols) {
        if (!script.empty())
            script += "\n";

        script += BuildBpftraceUprobeScript(resolved.target, resolved.sig, opts);
    }

    return script;
}

} // namespace bpftrace_mapper
