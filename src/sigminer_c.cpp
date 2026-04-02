#include "sigminer/sigminer_c.h"

#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include "internal/bpftrace_mapper.h"
#include "internal/printf_specifier_mapper.h"
#include "sigminer/sigminer.h"
#include "sigminer/signature.h"

namespace {

PrimitiveKind ToCPrimitiveKind(sigminer::PrimitiveKind kind)
{
    switch (kind) {
        case sigminer::PrimitiveKind::VOID: return PRIMITIVE_KIND_VOID;
        case sigminer::PrimitiveKind::BOOL: return PRIMITIVE_KIND_BOOL;
        case sigminer::PrimitiveKind::INT: return PRIMITIVE_KIND_INT;
        case sigminer::PrimitiveKind::FLOAT: return PRIMITIVE_KIND_FLOAT;
        case sigminer::PrimitiveKind::POINTER: return PRIMITIVE_KIND_POINTER;
        case sigminer::PrimitiveKind::ENUM: return PRIMITIVE_KIND_ENUM;
        case sigminer::PrimitiveKind::AGGREGATE: return PRIMITIVE_KIND_AGGREGATE;
        case sigminer::PrimitiveKind::UNKNOWN: return PRIMITIVE_KIND_UNKNOWN;
    }
    return PRIMITIVE_KIND_UNKNOWN;
}

sigminer::PrimitiveKind ToCppPrimitiveKind(PrimitiveKind kind)
{
    switch (kind) {
        case PRIMITIVE_KIND_VOID: return sigminer::PrimitiveKind::VOID;
        case PRIMITIVE_KIND_BOOL: return sigminer::PrimitiveKind::BOOL;
        case PRIMITIVE_KIND_INT: return sigminer::PrimitiveKind::INT;
        case PRIMITIVE_KIND_FLOAT: return sigminer::PrimitiveKind::FLOAT;
        case PRIMITIVE_KIND_POINTER: return sigminer::PrimitiveKind::POINTER;
        case PRIMITIVE_KIND_ENUM: return sigminer::PrimitiveKind::ENUM;
        case PRIMITIVE_KIND_AGGREGATE: return sigminer::PrimitiveKind::AGGREGATE;
        case PRIMITIVE_KIND_UNKNOWN: return sigminer::PrimitiveKind::UNKNOWN;
    }
    return sigminer::PrimitiveKind::UNKNOWN;
}

Signedness ToCSignedness(sigminer::Signedness sign)
{
    switch (sign) {
        case sigminer::Signedness::SIGNED: return SIGNEDNESS_SIGNED;
        case sigminer::Signedness::UNSIGNED: return SIGNEDNESS_UNSIGNED;
        case sigminer::Signedness::UNKNOWN: return SIGNEDNESS_UNKNOWN;
    }
    return SIGNEDNESS_UNKNOWN;
}

sigminer::Signedness ToCppSignedness(Signedness sign)
{
    switch (sign) {
        case SIGNEDNESS_SIGNED: return sigminer::Signedness::SIGNED;
        case SIGNEDNESS_UNSIGNED: return sigminer::Signedness::UNSIGNED;
        case SIGNEDNESS_UNKNOWN: return sigminer::Signedness::UNKNOWN;
    }
    return sigminer::Signedness::UNKNOWN;
}

ReturnCode ToCReturnCode(sigminer::ReturnCode code)
{
    switch (code) {
        case sigminer::ReturnCode::SUCCESS: return RETURN_CODE_SUCCESS;
        case sigminer::ReturnCode::INVALID_INPUT: return RETURN_CODE_INVALID_INPUT;
        case sigminer::ReturnCode::FILE_OPEN_FAILURE: return RETURN_CODE_FILE_OPEN_FAILURE;
        case sigminer::ReturnCode::SYMBOL_RESOLUTION_FAILURE:
            return RETURN_CODE_SYMBOL_RESOLUTION_FAILURE;
        case sigminer::ReturnCode::DWARF_UNAVAILABLE: return RETURN_CODE_DWARF_UNAVAILABLE;
        case sigminer::ReturnCode::FUNCTION_DIE_NOT_IN_RANGE:
            return RETURN_CODE_FUNCTION_DIE_NOT_IN_RANGE;
        case sigminer::ReturnCode::UNSUPPORTED_TYPE: return RETURN_CODE_UNSUPPORTED_TYPE;
        case sigminer::ReturnCode::INTERNAL_FAILURE: return RETURN_CODE_INTERNAL_FAILURE;
    }
    return RETURN_CODE_INTERNAL_FAILURE;
}

std::vector<sigminer::TypeEntry> ToCppTypeEntryVector(const TypeEntry* params, size_t paramsList)
{
    std::vector<sigminer::TypeEntry> cppParams;
    cppParams.reserve(paramsList);

    for (size_t i = 0; i < paramsList; ++i)
        cppParams.emplace_back(params[i]);

    return cppParams;
}

bpftrace_mapper::BpftraceRenderOptions ToCppBpfTraceRenderOptions(const BpftraceRenderOptions* Opts)
{
    bpftrace_mapper::BpftraceRenderOptions cppOpts{};

    if (Opts == nullptr)
        return cppOpts;

    cppOpts.pid = Opts->HasPid ? std::optional<int>(Opts->Pid) : std::nullopt;
    cppOpts.includeEntryProbe = Opts->IncludeEntryProbe;
    cppOpts.includeReturnProbe = Opts->IncludeReturnProbe;
    cppOpts.includeTimingMs = Opts->IncludeTimingMs;
    cppOpts.includeUserStack = Opts->IncludeUserStack;
    cppOpts.includeArgumentPrinting = Opts->IncludeArgumentPrinting;
    cppOpts.includeReturnPrinting = Opts->IncludeReturnPrinting;

    return cppOpts;
}

bpftrace_mapper::BpftraceProbeTarget ToCppBpftraceProbeTarget(const BpftraceProbeTarget& target)
{
    bpftrace_mapper::BpftraceProbeTarget cppTarget{};
    cppTarget.modulePath = target.ModulePath != nullptr ? target.ModulePath : "";
    cppTarget.symbol = target.Symbol != nullptr ? target.Symbol : "";
    return cppTarget;
}

std::vector<bpftrace_mapper::BpftraceResolvedSymbol> ToCppBpftraceResolvedSymbols(
        const BpftraceResolvedSymbol* resolvedSymbols,
        size_t resolvedSymbolCount)
{
    std::vector<bpftrace_mapper::BpftraceResolvedSymbol> cppResolvedSymbols;
    cppResolvedSymbols.reserve(resolvedSymbolCount);

    for (size_t i = 0; i < resolvedSymbolCount; ++i) {
        bpftrace_mapper::BpftraceResolvedSymbol cppResolved{};
        cppResolved.target = ToCppBpftraceProbeTarget(resolvedSymbols[i].Target);
        cppResolved.sig = sigminer::Signature(resolvedSymbols[i].Sig);
        cppResolvedSymbols.push_back(std::move(cppResolved));
    }

    return cppResolvedSymbols;
}

char* DuplicateCString(const std::string& value)
{
    char* copy = static_cast<char*>(std::malloc(value.size() + 1));
    if (copy == nullptr)
        return nullptr;

    std::memcpy(copy, value.c_str(), value.size() + 1);
    return copy;
}

bool CopyTypeEntry(const sigminer::TypeEntry& source, TypeEntry* dest)
{
    if (dest == nullptr)
        return false;

    dest->Kind = ToCPrimitiveKind(source.kind);
    dest->Sign = ToCSignedness(source.sign);
    dest->Size = source.size;
    dest->IsPointer = source.isPointer;
    dest->Name = DuplicateCString(source.name);

    if (dest->Name == nullptr && !source.name.empty())
        return false;

    return true;
}

void ZeroTypeEntry(TypeEntry* entry)
{
    if (entry == nullptr)
        return;

    entry->Kind = PRIMITIVE_KIND_UNKNOWN;
    entry->Sign = SIGNEDNESS_UNKNOWN;
    entry->Size = 0;
    entry->IsPointer = false;
    entry->Name = nullptr;
}

void ZeroSignature(Signature* sig)
{
    if (sig == nullptr)
        return;

    ZeroTypeEntry(&sig->Ret);
    sig->Params = nullptr;
    sig->ParamCount = 0;
    sig->HasVarArgs = false;
}

} // namespace

namespace sigminer {

TypeEntry::TypeEntry(const ::TypeEntry& source)
    : kind(ToCppPrimitiveKind(source.Kind)),
      sign(ToCppSignedness(source.Sign)),
      size(source.Size),
      isPointer(source.IsPointer),
      name(source.Name != nullptr ? source.Name : "")
{
}

Signature::Signature(const ::Signature& source)
    : ret(source.Ret),
      params(ToCppTypeEntryVector(source.Params, source.ParamCount)),
      hasVarArgs(source.HasVarArgs)
{
}

} // namespace sigminer

Result SIGMINER_GetSignatureFromSharedObjectBySymbol(
        const char* sharedObjectFilePath,
        const char* symbol)
{
    Result result{};
    result.RetCode = RETURN_CODE_INTERNAL_FAILURE;

    if (sharedObjectFilePath == nullptr || symbol == nullptr) {
        result.RetCode = RETURN_CODE_INVALID_INPUT;
        return result;
    }

    const sigminer::Result cppResult =
            sigminer::GetSignatureFromSharedObjectBySymbol(sharedObjectFilePath, symbol);
    result.RetCode = ToCReturnCode(cppResult.retCode);

    if (!cppResult.sig)
        return result;

    ZeroSignature(&result.Sig);
    result.Sig.HasVarArgs = cppResult.sig->hasVarArgs;

    if (!CopyTypeEntry(cppResult.sig->ret, &result.Sig.Ret)) {
        SIGMINER_FreeResult(&result);
        result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
        return result;
    }

    result.Sig.ParamCount = cppResult.sig->params.size();
    if (result.Sig.ParamCount != 0) {
        result.Sig.Params = static_cast<TypeEntry*>(
                std::calloc(result.Sig.ParamCount, sizeof(TypeEntry)));
        if (result.Sig.Params == nullptr) {
            SIGMINER_FreeResult(&result);
            result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
            return result;
        }

        for (size_t i = 0; i < result.Sig.ParamCount; ++i) {
            if (!CopyTypeEntry(cppResult.sig->params[i], &result.Sig.Params[i])) {
                SIGMINER_FreeResult(&result);
                result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
                return result;
            }
        }
    }

    result.HasSignature = true;
    return result;
}

void SIGMINER_FreeSignature(Signature* sig)
{
    if (sig == nullptr)
        return;

    std::free(sig->Ret.Name);
    sig->Ret.Name = nullptr;

    for (size_t i = 0; i < sig->ParamCount; ++i) {
        std::free(sig->Params[i].Name);
        sig->Params[i].Name = nullptr;
    }

    std::free(sig->Params);
    ZeroSignature(sig);
}

void SIGMINER_FreeResult(Result* res)
{
    if (res == nullptr)
        return;

    SIGMINER_FreeSignature(&res->Sig);
    res->HasSignature = false;
    res->RetCode = RETURN_CODE_SUCCESS;
}

const char* SIGMINER_TypeEntryToPrintfSpecifier(const TypeEntry* typeEntry)
{
    if (typeEntry == nullptr)
        return nullptr;

    sigminer::TypeEntry cppTypeEntry(*typeEntry);
    std::string result = printf_specifier_mapper::TypeEntryToPrintfSpecifier(cppTypeEntry);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_SignatureParamsToPrintfSpecifier(const Signature* sig)
{
    if (sig == nullptr)
        return nullptr;

    std::vector<sigminer::TypeEntry> cppTypeEntries;
    cppTypeEntries.reserve(sig->ParamCount);

    for (size_t i = 0; i < sig->ParamCount; ++i)
        cppTypeEntries.emplace_back(sig->Params[i]);

    std::string result = printf_specifier_mapper::TypeEntriesToPrintfSpecifier(cppTypeEntries);
    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildBpftraceArgumentPrintExpr( const Signature* Sig )
{
    if (Sig == nullptr)
        return nullptr;

    if (Sig->ParamCount != 0 && Sig->Params == nullptr)
        return nullptr;

    sigminer::Signature cppSignature(*Sig);

    std::string result = bpftrace_mapper::BuildBpftraceArgumentPrintExpr(cppSignature);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildBpftraceReturnPrintExpr( const TypeEntry* RetType )
{
    if (RetType == nullptr)
        return nullptr;

    sigminer::TypeEntry cppRetType(*RetType);

    std::string result = bpftrace_mapper::BuildBpftraceReturnPrintExpr(cppRetType);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildBpftraceProbeBody(
        BpftraceProbeKind ProbeKind,
        const Signature* Sig,
        const BpftraceRenderOptions* Opts )
{
    if (Sig == nullptr)
        return nullptr;

    if (Sig->ParamCount != 0 && Sig->Params == nullptr)
        return nullptr;

    bpftrace_mapper::BpftraceProbeKind cppProbeKind;
    if (ProbeKind == BPFTRACE_PROBE_KIND_ENTRY)
        cppProbeKind = bpftrace_mapper::BpftraceProbeKind::ENTRY;
    else if (ProbeKind == BPFTRACE_PROBE_KIND_RETURN)
        cppProbeKind = bpftrace_mapper::BpftraceProbeKind::RETURN;
    else
        return nullptr;

    sigminer::Signature cppSig(*Sig);

    bpftrace_mapper::BpftraceRenderOptions cppOpts = ToCppBpfTraceRenderOptions(Opts);

    std::string result = bpftrace_mapper::BuildBpftraceProbeBody(cppProbeKind, cppSig, cppOpts);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildBpftraceUprobeScriptForTarget(
        const BpftraceProbeTarget* Target,
        const Signature* Sig,
        const BpftraceRenderOptions* Opts )
{
    if (Target == nullptr || Sig == nullptr)
        return nullptr;

    if (Target->ModulePath == nullptr || Target->Symbol == nullptr)
        return nullptr;

    if (Sig->ParamCount != 0 && Sig->Params == nullptr)
        return nullptr;

    const bpftrace_mapper::BpftraceProbeTarget cppTarget = ToCppBpftraceProbeTarget(*Target);
    const sigminer::Signature cppSig(*Sig);
    const bpftrace_mapper::BpftraceRenderOptions cppOpts = ToCppBpfTraceRenderOptions(Opts);

    std::string result = bpftrace_mapper::BuildBpftraceUprobeScript(cppTarget, cppSig, cppOpts);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildBpftraceUprobeScriptForResolvedSymbols(
        const BpftraceResolvedSymbol* ResolvedSymbols,
        size_t ResolvedSymbolCount,
        const BpftraceRenderOptions* Opts )
{
    if (ResolvedSymbols == nullptr && ResolvedSymbolCount != 0)
        return nullptr;

    for (size_t i = 0; i < ResolvedSymbolCount; ++i) {
        if (ResolvedSymbols[i].Target.ModulePath == nullptr ||
            ResolvedSymbols[i].Target.Symbol == nullptr)
            return nullptr;

        if (ResolvedSymbols[i].Sig.ParamCount != 0 &&
            ResolvedSymbols[i].Sig.Params == nullptr)
            return nullptr;
    }

    const std::vector<bpftrace_mapper::BpftraceResolvedSymbol> cppResolvedSymbols =
            ToCppBpftraceResolvedSymbols(ResolvedSymbols, ResolvedSymbolCount);
    const bpftrace_mapper::BpftraceRenderOptions cppOpts = ToCppBpfTraceRenderOptions(Opts);

    std::string result =
            bpftrace_mapper::BuildBpftraceUprobeScript(cppResolvedSymbols, cppOpts);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}
