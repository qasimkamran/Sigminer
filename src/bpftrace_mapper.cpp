#include "internal/bpftrace_mapper.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
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

std::string EscapeStringLiteral(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size());

    for (char ch : text) {
        if (ch == '\\' || ch == '"')
            escaped.push_back('\\');
        escaped.push_back(ch);
    }

    return escaped;
}

std::string SanitizeIdentifier(std::string text)
{
    for (char& ch : text) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_')
            ch = '_';
    }

    if (text.empty() || std::isdigit(static_cast<unsigned char>(text.front())))
        text.insert(text.begin(), '_');

    return text;
}

std::string Indent(std::size_t depth)
{
    return std::string(depth * 2, ' ');
}

std::string Parenthesize(const std::string& expr)
{
    return "(" + expr + ")";
}

std::string MakeScratchVar(const std::string& label, const char* suffix)
{
    return "$" + SanitizeIdentifier(label) + "_" + suffix;
}

std::string PrimitiveCastName(
        std::size_t size,
        sigminer::Signedness sign,
        sigminer::PrimitiveKind kind)
{
    if (kind == sigminer::PrimitiveKind::BOOL)
        return "uint8";

    if (kind != sigminer::PrimitiveKind::INT && kind != sigminer::PrimitiveKind::ENUM)
        return {};

    if (size == 1)
        return (sign == sigminer::Signedness::SIGNED) ? "int8" : "uint8";
    if (size == 2)
        return (sign == sigminer::Signedness::SIGNED) ? "int16" : "uint16";
    if (size == 4)
        return (sign == sigminer::Signedness::SIGNED) ? "int32" : "uint32";
    if (size == 8)
        return (sign == sigminer::Signedness::SIGNED) ? "int64" : "uint64";

    return {};
}

bool IsScalarKind(sigminer::PrimitiveKind kind)
{
    return kind == sigminer::PrimitiveKind::BOOL ||
           kind == sigminer::PrimitiveKind::INT ||
           kind == sigminer::PrimitiveKind::ENUM ||
           kind == sigminer::PrimitiveKind::FLOAT;
}

std::string StorageTypeName(const sigminer::RichTypeEntry& type)
{
    if (type.kind == sigminer::PrimitiveKind::FLOAT) {
        if (type.size == 4)
            return "uint32";
        if (type.size == 8)
            return "uint64";
        return {};
    }

    return PrimitiveCastName(type.size, type.sign, type.kind);
}

std::string PointerIntegerTypeName(std::size_t size)
{
    if (size == 4)
        return "uint32";
    return "uint64";
}

std::string BuildAddressOffsetExpr(const std::string& baseExpr, std::size_t offset)
{
    if (offset == 0)
        return baseExpr;
    return "(" + baseExpr + " + " + std::to_string(offset) + ")";
}

void AppendLine(std::string& out, std::size_t depth, const std::string& line)
{
    out += Indent(depth) + line + "\n";
}

void AppendPrintf(std::string& out, std::size_t depth, const std::string& message)
{
    AppendLine(out, depth, "printf(\"" + EscapeStringLiteral(message) + "\\n\");");
}

void RenderRichValue(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& expr,
        const std::string& label,
        const BpftraceRenderOptions& opts,
        std::size_t depth,
        std::size_t aggregateDepth);

void RenderRichValueFromAddress(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& addressExpr,
        const std::string& label,
        const BpftraceRenderOptions& opts,
        std::size_t depth,
        std::size_t aggregateDepth);

void RenderAggregateMembers(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& addressExpr,
        const std::string& label,
        const BpftraceRenderOptions& opts,
        std::size_t depth,
        std::size_t aggregateDepth)
{
    if (aggregateDepth >= opts.maxAggregateDepth) {
        AppendPrintf(out, depth, label + ": <aggregate depth limit>");
        return;
    }

    const std::size_t memberCount = std::min(type.members.size(), opts.maxAggregateMembers);
    AppendPrintf(out, depth, label + ": {" + type.name + "}");

    for (std::size_t i = 0; i < memberCount; ++i) {
        const sigminer::RichTypeMember& member = type.members[i];
        if (!member.type)
            continue;

        RenderRichValueFromAddress(
                out,
                *member.type,
                BuildAddressOffsetExpr(addressExpr, member.offset),
                label + "." + member.name,
                opts,
                depth,
                aggregateDepth + 1);
    }

    if (type.members.size() > memberCount)
        AppendPrintf(out, depth, label + ": <member output truncated>");
}

void RenderArrayValue(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& addressExpr,
        const std::string& label,
        const BpftraceRenderOptions& opts,
        std::size_t depth,
        std::size_t aggregateDepth)
{
    if (type.isStringLike) {
        AppendLine(out, depth, "printf(\"    " + EscapeStringLiteral(label) +
                                 ": %s\\n\", str(uptr(" + addressExpr + ")));");
        return;
    }

    AppendPrintf(out, depth, label + ": [");

    if (!type.elementType)
        return;

    if (type.elementType->size == 0) {
        AppendPrintf(out, depth, label + ": <unknown element size>");
        return;
    }

    const std::size_t count = std::min(type.arrayCount, opts.maxArrayElements);
    for (std::size_t i = 0; i < count; ++i) {
        RenderRichValueFromAddress(
                out,
                *type.elementType,
                BuildAddressOffsetExpr(addressExpr, i * type.elementType->size),
                label + "[" + std::to_string(i) + "]",
                opts,
                depth,
                aggregateDepth);
    }

    if (type.arrayCount > count)
        AppendPrintf(out, depth, label + ": <array output truncated>");
}

void RenderPrimitiveValueFromAddress(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& addressExpr,
        const std::string& label,
        std::size_t depth)
{
    sigminer::TypeEntry shallow{};
    shallow.kind = type.kind;
    shallow.sign = type.sign;
    shallow.size = type.size;
    shallow.name = type.name;

    const std::string storageType = StorageTypeName(type);
    const std::string specifier = printf_specifier_mapper::TypeEntryToPrintfSpecifier(shallow);
    if (storageType.empty() || specifier.empty()) {
        AppendPrintf(out, depth, label + ": <unsupported primitive read>");
        return;
    }

    const std::string typedPtrVar = MakeScratchVar(label, "typed_ptr");
    const std::string valueVar = MakeScratchVar(label, "value");
    AppendLine(
            out,
            depth,
            typedPtrVar + " = uptr((" + storageType + "*)(" + addressExpr + "));");
    AppendLine(out, depth, valueVar + " = *" + typedPtrVar + ";");
    AppendLine(
            out,
            depth,
            "printf(\"    " + EscapeStringLiteral(label) + ": " + EscapeStringLiteral(specifier) +
                    "\\n\", " + valueVar + ");");
}

void RenderPointerValueFromAddress(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& addressExpr,
        const std::string& label,
        const BpftraceRenderOptions& opts,
        std::size_t depth,
        std::size_t aggregateDepth)
{
    const std::string ptrStorageType = PointerIntegerTypeName(type.size);
    const std::string typedPtrVar = MakeScratchVar(label, "typed_ptr");
    const std::string ptrVar = MakeScratchVar(label, "ptr");
    AppendLine(
            out,
            depth,
            typedPtrVar + " = uptr((" + ptrStorageType + "*)(" + addressExpr + "));");
    AppendLine(out, depth, ptrVar + " = *" + typedPtrVar + ";");
    AppendLine(out, depth, "printf(\"    " + EscapeStringLiteral(label) + ".addr: %p\\n\", " + ptrVar + ");");
    AppendLine(out, depth, "if (" + ptrVar + " != 0) {");

    if (!type.pointee) {
        AppendPrintf(out, depth + 1, label + ": <unknown pointee>");
        AppendLine(out, depth, "}");
        return;
    }

    if (type.isStringLike) {
        AppendLine(
                out,
                depth + 1,
                "printf(\"    " + EscapeStringLiteral(label) + ": %s\\n\", str(uptr(" + ptrVar + ")));");
        AppendLine(out, depth, "}");
        return;
    }

    if (IsScalarKind(type.pointee->kind)) {
        RenderPrimitiveValueFromAddress(out, *type.pointee, ptrVar, label + ".value", depth + 1);
    } else if (type.pointee->kind == sigminer::PrimitiveKind::AGGREGATE) {
        RenderAggregateMembers(
                out,
                *type.pointee,
                ptrVar,
                label + ".value",
                opts,
                depth + 1,
                aggregateDepth);
    } else if (type.pointee->kind == sigminer::PrimitiveKind::POINTER) {
        RenderPointerValueFromAddress(
                out,
                *type.pointee,
                ptrVar,
                label + ".value",
                opts,
                depth + 1,
                aggregateDepth);
    } else if (type.pointee->arrayCount != 0 && type.pointee->elementType) {
        RenderArrayValue(out, *type.pointee, ptrVar, label + ".value", opts, depth + 1, aggregateDepth);
    } else {
        AppendPrintf(out, depth + 1, label + ": <unsupported pointee>");
    }

    AppendLine(out, depth, "}");
}

void RenderRichValueFromAddress(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& addressExpr,
        const std::string& label,
        const BpftraceRenderOptions& opts,
        std::size_t depth,
        std::size_t aggregateDepth)
{
    if (type.isRecursiveReference) {
        AppendPrintf(out, depth, label + ": <recursive reference>");
        return;
    }

    if (type.arrayCount != 0 && type.elementType) {
        RenderArrayValue(out, type, addressExpr, label, opts, depth, aggregateDepth);
        return;
    }

    if (IsScalarKind(type.kind)) {
        RenderPrimitiveValueFromAddress(out, type, addressExpr, label, depth);
        return;
    }

    if (type.kind == sigminer::PrimitiveKind::POINTER) {
        RenderPointerValueFromAddress(out, type, addressExpr, label, opts, depth, aggregateDepth);
        return;
    }

    if (type.kind == sigminer::PrimitiveKind::AGGREGATE) {
        RenderAggregateMembers(out, type, addressExpr, label, opts, depth, aggregateDepth);
        return;
    }

    AppendPrintf(out, depth, label + ": <unsupported address read>");
}

void RenderPrimitiveValue(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& expr,
        const std::string& label,
        std::size_t depth)
{
    sigminer::TypeEntry shallow{};
    shallow.kind = type.kind;
    shallow.sign = type.sign;
    shallow.size = type.size;
    shallow.name = type.name;

    std::string specifier = printf_specifier_mapper::TypeEntryToPrintfSpecifier(shallow);
    if (specifier.empty()) {
        AppendPrintf(out, depth, label + ": <unsupported primitive>");
        return;
    }

    if (type.kind == sigminer::PrimitiveKind::BOOL) {
        const std::string valueVar = MakeScratchVar(label, "value");
        AppendLine(out, depth, valueVar + " = (uint8)(" + expr + ");");
        AppendLine(
                out,
                depth,
                "printf(\"    " + EscapeStringLiteral(label) + ": " + EscapeStringLiteral(specifier) +
                        "\\n\", " + valueVar + ");");
        return;
    }

    const std::string cast = PrimitiveCastName(type.size, type.sign, type.kind);
    if (!cast.empty()) {
        const std::string valueVar = MakeScratchVar(label, "value");
        AppendLine(out, depth, valueVar + " = (" + cast + ")(" + expr + ");");
        AppendLine(
                out,
                depth,
                "printf(\"    " + EscapeStringLiteral(label) + ": " + EscapeStringLiteral(specifier) +
                        "\\n\", " + valueVar + ");");
        return;
    }

    AppendLine(
            out,
            depth,
            "printf(\"    " + EscapeStringLiteral(label) + ": " + EscapeStringLiteral(specifier) +
                    "\\n\", " + expr + ");");
}

void RenderPointerValue(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& expr,
        const std::string& label,
        const BpftraceRenderOptions& opts,
        std::size_t depth,
        std::size_t aggregateDepth)
{
    const std::string ptrVar = MakeScratchVar(label, "ptr");
    AppendLine(out, depth, ptrVar + " = " + expr + ";");
    AppendLine(out, depth, "printf(\"    " + EscapeStringLiteral(label) + ".addr: %p\\n\", " + ptrVar + ");");
    AppendLine(out, depth, "if (" + ptrVar + " != 0) {");

    if (!type.pointee) {
        AppendPrintf(out, depth + 1, label + ": <unknown pointee>");
        AppendLine(out, depth, "}");
        return;
    }

    if (type.isStringLike) {
        AppendLine(
                out,
                depth + 1,
                "printf(\"    " + EscapeStringLiteral(label) + ": %s\\n\", str(uptr(" + ptrVar + ")));");
        AppendLine(out, depth, "}");
        return;
    }

    if (IsScalarKind(type.pointee->kind)) {
        RenderPrimitiveValueFromAddress(out, *type.pointee, ptrVar, label + ".value", depth + 1);
    } else if (type.pointee->kind == sigminer::PrimitiveKind::AGGREGATE) {
        RenderAggregateMembers(
                out,
                *type.pointee,
                ptrVar,
                label + ".value",
                opts,
                depth + 1,
                aggregateDepth);
    } else if (type.pointee->kind == sigminer::PrimitiveKind::POINTER) {
        RenderPointerValueFromAddress(
                out,
                *type.pointee,
                ptrVar,
                label + ".value",
                opts,
                depth + 1,
                aggregateDepth);
    } else if (type.pointee->arrayCount != 0 && type.pointee->elementType) {
        RenderArrayValue(out, *type.pointee, ptrVar, label + ".value", opts, depth + 1, aggregateDepth);
    } else {
        AppendPrintf(out, depth + 1, label + ": <unsupported pointee>");
    }

    AppendLine(out, depth, "}");
}

void RenderRichValue(
        std::string& out,
        const sigminer::RichTypeEntry& type,
        const std::string& expr,
        const std::string& label,
        const BpftraceRenderOptions& opts,
        std::size_t depth,
        std::size_t aggregateDepth)
{
    if (type.isRecursiveReference) {
        AppendPrintf(out, depth, label + ": <recursive reference>");
        return;
    }

    if (type.arrayCount != 0 && type.elementType) {
        AppendPrintf(out, depth, label + ": <by-value array rendering unsupported>");
        return;
    }

    switch (type.kind) {
        case sigminer::PrimitiveKind::VOID:
            AppendPrintf(out, depth, label + ": (void)");
            break;
        case sigminer::PrimitiveKind::BOOL:
        case sigminer::PrimitiveKind::INT:
        case sigminer::PrimitiveKind::FLOAT:
        case sigminer::PrimitiveKind::ENUM:
            RenderPrimitiveValue(out, type, expr, label, depth);
            break;
        case sigminer::PrimitiveKind::POINTER:
            RenderPointerValue(out, type, expr, label, opts, depth, aggregateDepth);
            break;
        case sigminer::PrimitiveKind::AGGREGATE:
            AppendPrintf(out, depth, label + ": <by-value aggregate rendering unsupported>");
            break;
        case sigminer::PrimitiveKind::UNKNOWN:
            AppendPrintf(out, depth, label + ": <unknown>");
            break;
    }
}

std::string BuildRichArgumentPrintExpr(
        const sigminer::RichSignature& sig,
        const BpftraceRenderOptions& opts)
{
    if (sig.params.empty())
        return "  printf(\"    args: (none)\\n\");\n";

    std::string out;
    for (std::size_t i = 0; i < sig.params.size(); ++i) {
        const sigminer::RichParameter& param = sig.params[i];
        std::string expr = "arg" + std::to_string(i);
        RenderRichValue(out, param.type, expr, "arg" + std::to_string(i) + " (" + param.name + ")", opts, 1, 0);
    }
    return out;
}

std::string BuildRichReturnPrintExpr(
        const sigminer::RichTypeEntry& retType,
        const BpftraceRenderOptions& opts)
{
    if (retType.kind == sigminer::PrimitiveKind::AGGREGATE) {
        return "  printf(\"    retval: raw aggregate return unsupported\\n\");\n";
    }

    std::string out;
    RenderRichValue(out, retType, "retval", "retval", opts, 1, 0);
    return out;
}

std::string BuildRichProbeBody(
        BpftraceProbeKind probeKind,
        const sigminer::RichSignature& sig,
        const BpftraceRenderOptions& opts)
{
    std::string body;

    if (probeKind == BpftraceProbeKind::ENTRY) {
        if (opts.includeTimingMs)
            body += "  @start[tid] = nsecs;\n";

        if (opts.includeArgumentPrinting)
            body += BuildRichArgumentPrintExpr(sig, opts);
    } else {
        if (opts.includeReturnPrinting)
            body += BuildRichReturnPrintExpr(sig.ret, opts);

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

std::string BuildRichBpftraceUprobeScript(
        const BpftraceProbeTarget& target,
        const sigminer::RichSignature& sig,
        const BpftraceRenderOptions& opts)
{
    if (!opts.enableRichTypePrinting)
        return {};

    std::string selector = BuildProbeSelector(opts);
    std::string script;

    if (opts.includeEntryProbe) {
        script += "uprobe:" + target.modulePath + ":" + target.symbol + selector + "\n";
        script += "{\n";
        script += BuildProbeHeader(target, BpftraceProbeKind::ENTRY);
        script += BuildRichProbeBody(BpftraceProbeKind::ENTRY, sig, opts);
        script += "}\n";
    }

    if (opts.includeReturnProbe) {
        if (!script.empty())
            script += "\n";

        script += "uretprobe:" + target.modulePath + ":" + target.symbol + selector + "\n";
        script += "{\n";
        script += BuildProbeHeader(target, BpftraceProbeKind::RETURN);
        script += BuildRichProbeBody(BpftraceProbeKind::RETURN, sig, opts);
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
