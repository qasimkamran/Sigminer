#include "sigminer/sigminer_c.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

std::vector<sigminer::rich::Parameter> ToCppRichParameterVector(
        const RichParameter* params,
        size_t paramsList)
{
    std::vector<sigminer::rich::Parameter> cppParams;
    cppParams.reserve(paramsList);

    for (size_t i = 0; i < paramsList; ++i)
        cppParams.emplace_back(params[i]);

    return cppParams;
}

bpftrace_mapper::BpftraceRenderOptions ToCppBpfTraceRenderOptions(const BpftraceRenderOptions* opts)
{
    bpftrace_mapper::BpftraceRenderOptions cppOpts{};

    if (opts == nullptr)
        return cppOpts;

    cppOpts.pid = opts->HasPid ? std::optional<int>(opts->Pid) : std::nullopt;
    cppOpts.includeEntryProbe = opts->IncludeEntryProbe;
    cppOpts.includeReturnProbe = opts->IncludeReturnProbe;
    cppOpts.includeTimingMs = opts->IncludeTimingMs;
    cppOpts.includeUserStack = opts->IncludeUserStack;
    cppOpts.includeArgumentPrinting = opts->IncludeArgumentPrinting;
    cppOpts.includeReturnPrinting = opts->IncludeReturnPrinting;

    return cppOpts;
}

bpftrace_mapper::rich::ExtendedBpftraceRenderOptions ToCppRichBpfTraceRenderOptions(
        const BpftraceRenderOptions* opts)
{
    bpftrace_mapper::rich::ExtendedBpftraceRenderOptions cppOpts{};

    if (opts == nullptr)
        return cppOpts;

    cppOpts.pid = opts->HasPid ? std::optional<int>(opts->Pid) : std::nullopt;
    cppOpts.includeEntryProbe = opts->IncludeEntryProbe;
    cppOpts.includeReturnProbe = opts->IncludeReturnProbe;
    cppOpts.includeTimingMs = opts->IncludeTimingMs;
    cppOpts.includeUserStack = opts->IncludeUserStack;
    cppOpts.includeArgumentPrinting = opts->IncludeArgumentPrinting;
    cppOpts.includeReturnPrinting = opts->IncludeReturnPrinting;
    cppOpts.maxAggregateDepth = (opts->MaxAggregateDepth != 0) ? opts->MaxAggregateDepth : 2;
    cppOpts.maxAggregateMembers =
            (opts->MaxAggregateMembers != 0) ? opts->MaxAggregateMembers : 16;
    cppOpts.maxArrayElements = (opts->MaxArrayElements != 0) ? opts->MaxArrayElements : 8;

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

bool CopyRichTypeEntry(const sigminer::rich::TypeEntry& source, RichTypeEntry* dest);

bool CopyRichTypeMember(const sigminer::rich::TypeMember& source, RichTypeMember* dest)
{
    if (dest == nullptr)
        return false;

    dest->Name = DuplicateCString(source.name);
    dest->Offset = source.offset;
    dest->Type = nullptr;

    if (dest->Name == nullptr && !source.name.empty())
        return false;

    if (source.type) {
        dest->Type = static_cast<RichTypeEntry*>(std::calloc(1, sizeof(RichTypeEntry)));
        if (dest->Type == nullptr)
            return false;

        if (!CopyRichTypeEntry(*source.type, dest->Type))
            return false;
    }

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

void ZeroRichTypeEntry(RichTypeEntry* entry)
{
    if (entry == nullptr)
        return;

    entry->Kind = PRIMITIVE_KIND_UNKNOWN;
    entry->Sign = SIGNEDNESS_UNKNOWN;
    entry->Size = 0;
    entry->Name = nullptr;
    entry->IsConst = false;
    entry->IsStringLike = false;
    entry->IsRecursiveReference = false;
    entry->ArrayCount = 0;
    entry->Pointee = nullptr;
    entry->ElementType = nullptr;
    entry->Members = nullptr;
    entry->MemberCount = 0;
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

void ZeroRichSignature(RichSignature* sig)
{
    if (sig == nullptr)
        return;

    ZeroRichTypeEntry(&sig->Ret);
    sig->Params = nullptr;
    sig->ParamCount = 0;
    sig->HasVarArgs = false;
}

void FreeRichTypeEntryTree(RichTypeEntry* entry)
{
    if (entry == nullptr)
        return;

    FreeRichTypeEntryTree(entry->Pointee);
    FreeRichTypeEntryTree(entry->ElementType);
    for (size_t i = 0; i < entry->MemberCount; ++i) {
        std::free(entry->Members[i].Name);
        FreeRichTypeEntryTree(entry->Members[i].Type);
        std::free(entry->Members[i].Type);
        entry->Members[i].Type = nullptr;
    }
    std::free(entry->Name);
    std::free(entry->Pointee);
    std::free(entry->ElementType);
    std::free(entry->Members);
    ZeroRichTypeEntry(entry);
}

bool CopyRichTypeEntry(const sigminer::rich::TypeEntry& source, RichTypeEntry* dest)
{
    if (dest == nullptr)
        return false;

    ZeroRichTypeEntry(dest);
    dest->Kind = ToCPrimitiveKind(source.kind);
    dest->Sign = ToCSignedness(source.sign);
    dest->Size = source.size;
    dest->Name = DuplicateCString(source.name);
    dest->IsConst = source.isConst;
    dest->IsStringLike = source.isStringLike;
    dest->IsRecursiveReference = source.isRecursiveReference;
    dest->ArrayCount = source.arrayCount;

    if (dest->Name == nullptr && !source.name.empty())
        return false;

    if (source.pointee) {
        dest->Pointee = static_cast<RichTypeEntry*>(std::calloc(1, sizeof(RichTypeEntry)));
        if (dest->Pointee == nullptr || !CopyRichTypeEntry(*source.pointee, dest->Pointee))
            return false;
    }

    if (source.elementType) {
        dest->ElementType = static_cast<RichTypeEntry*>(std::calloc(1, sizeof(RichTypeEntry)));
        if (dest->ElementType == nullptr ||
            !CopyRichTypeEntry(*source.elementType, dest->ElementType))
            return false;
    }

    dest->MemberCount = source.members.size();
    if (dest->MemberCount != 0) {
        dest->Members = static_cast<RichTypeMember*>(
                std::calloc(dest->MemberCount, sizeof(RichTypeMember)));
        if (dest->Members == nullptr)
            return false;

        for (size_t i = 0; i < dest->MemberCount; ++i) {
            if (!CopyRichTypeMember(source.members[i], &dest->Members[i]))
                return false;
        }
    }

    return true;
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

namespace rich {

TypeEntry::TypeEntry(const ::RichTypeEntry& source)
    : kind(ToCppPrimitiveKind(source.Kind)),
      sign(ToCppSignedness(source.Sign)),
      size(source.Size),
      name(source.Name != nullptr ? source.Name : ""),
      isConst(source.IsConst),
      isStringLike(source.IsStringLike),
      isRecursiveReference(source.IsRecursiveReference),
      arrayCount(source.ArrayCount)
{
    if (source.Pointee != nullptr)
        pointee = std::make_unique<TypeEntry>(*source.Pointee);
    if (source.ElementType != nullptr)
        elementType = std::make_unique<TypeEntry>(*source.ElementType);
    for (size_t i = 0; i < source.MemberCount; ++i) {
        TypeMember member{};
        member.name = source.Members[i].Name != nullptr ? source.Members[i].Name : "";
        member.offset = source.Members[i].Offset;
        if (source.Members[i].Type != nullptr)
            member.type = std::make_unique<TypeEntry>(*source.Members[i].Type);
        members.push_back(std::move(member));
    }
}

TypeEntry::TypeEntry(const TypeEntry& other)
    : kind(other.kind),
      sign(other.sign),
      size(other.size),
      name(other.name),
      isConst(other.isConst),
      isStringLike(other.isStringLike),
      isRecursiveReference(other.isRecursiveReference),
      arrayCount(other.arrayCount)
{
    if (other.pointee)
        pointee = std::make_unique<TypeEntry>(*other.pointee);
    if (other.elementType)
        elementType = std::make_unique<TypeEntry>(*other.elementType);
    members.reserve(other.members.size());
    for (const TypeMember& srcMember : other.members) {
        TypeMember member{};
        member.name = srcMember.name;
        member.offset = srcMember.offset;
        if (srcMember.type)
            member.type = std::make_unique<TypeEntry>(*srcMember.type);
        members.push_back(std::move(member));
    }
}

TypeEntry& TypeEntry::operator=(const TypeEntry& other)
{
    if (this == &other)
        return *this;

    TypeEntry copy(other);
    *this = std::move(copy);
    return *this;
}

Parameter::Parameter(const ::RichParameter& source)
    : name(source.Name != nullptr ? source.Name : "")
{
    type = std::make_unique<TypeEntry>(source.Type);
}

Parameter::Parameter(const Parameter& other)
    : name(other.name)
{
    if (other.type)
        type = std::make_unique<TypeEntry>(*other.type);
}

Parameter& Parameter::operator=(const Parameter& other)
{
    if (this == &other)
        return *this;

    name = other.name;
    if (other.type)
        type = std::make_unique<TypeEntry>(*other.type);
    else
        type.reset();

    return *this;
}

Signature::Signature(const ::RichSignature& source)
    : ret(source.Ret),
      params(ToCppRichParameterVector(source.Params, source.ParamCount)),
      hasVarArgs(source.HasVarArgs)
{
}

} // namespace rich

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

RichResult SIGMINER_GetRichSignatureFromSharedObjectBySymbol(
        const char* sharedObjectFilePath,
        const char* symbol)
{
    RichResult result{};
    result.RetCode = RETURN_CODE_INTERNAL_FAILURE;

    if (sharedObjectFilePath == nullptr || symbol == nullptr) {
        result.RetCode = RETURN_CODE_INVALID_INPUT;
        return result;
    }

    const sigminer::rich::Result cppResult =
            sigminer::rich::GetSignatureFromSharedObjectBySymbol(sharedObjectFilePath, symbol);
    result.RetCode = ToCReturnCode(cppResult.retCode);

    if (!cppResult.sig)
        return result;

    ZeroRichSignature(&result.Sig);
    result.Sig.HasVarArgs = cppResult.sig->hasVarArgs;

    if (!CopyRichTypeEntry(cppResult.sig->ret, &result.Sig.Ret)) {
        SIGMINER_FreeRichResult(&result);
        result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
        return result;
    }

    result.Sig.ParamCount = cppResult.sig->params.size();
    if (result.Sig.ParamCount != 0) {
        result.Sig.Params = static_cast<RichParameter*>(
                std::calloc(result.Sig.ParamCount, sizeof(RichParameter)));
        if (result.Sig.Params == nullptr) {
            SIGMINER_FreeRichResult(&result);
            result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
            return result;
        }

        for (size_t i = 0; i < result.Sig.ParamCount; ++i) {
            result.Sig.Params[i].Name = DuplicateCString(cppResult.sig->params[i].name);
            if (result.Sig.Params[i].Name == nullptr && !cppResult.sig->params[i].name.empty()) {
                SIGMINER_FreeRichResult(&result);
                result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
                return result;
            }

            if (cppResult.sig->params[i].type == nullptr ||
                !CopyRichTypeEntry(*cppResult.sig->params[i].type, &result.Sig.Params[i].Type)) {
                SIGMINER_FreeRichResult(&result);
                result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
                return result;
            }
        }
    }

    result.HasSignature = true;
    return result;
}

Result SIGMINER_FindSignatureInModulesBySymbol(
        const char* const* modulePaths,
        size_t modulePathCount,
        const char* symbol,
        BpftraceResolvedSymbol* resolved)
{
    Result result{};
    result.RetCode = RETURN_CODE_INTERNAL_FAILURE;

    if (modulePaths == nullptr || modulePathCount == 0 || symbol == nullptr) {
        result.RetCode = RETURN_CODE_INVALID_INPUT;
        return result;
    }

    std::vector<std::string> cppModulePaths;
    cppModulePaths.reserve(modulePathCount);

    for (size_t i = 0; i < modulePathCount; ++i) {
        if (modulePaths[i] == nullptr) {
            result.RetCode = RETURN_CODE_INVALID_INPUT;
            return result;
        }

        cppModulePaths.emplace_back(modulePaths[i]);
    }

    bpftrace_mapper::BpftraceResolvedSymbol cppResolved{};
    sigminer::Result cppResult =
            bpftrace_mapper::FindSignatureInModulesBySymbol(cppModulePaths, symbol, &cppResolved);
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

    if (resolved != nullptr) {
        resolved->Target.ModulePath = DuplicateCString(cppResolved.target.modulePath);
        resolved->Target.Symbol = DuplicateCString(cppResolved.target.symbol);
        ZeroSignature(&resolved->Sig);
        resolved->Sig.HasVarArgs = cppResolved.sig.hasVarArgs;

        if ((resolved->Target.ModulePath == nullptr && !cppResolved.target.modulePath.empty()) ||
            (resolved->Target.Symbol == nullptr && !cppResolved.target.symbol.empty()) ||
            !CopyTypeEntry(cppResolved.sig.ret, &resolved->Sig.Ret)) {
            SIGMINER_FreeBpftraceResolvedSymbol(resolved);
            SIGMINER_FreeResult(&result);
            result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
            return result;
        }

        resolved->Sig.ParamCount = cppResolved.sig.params.size();
        if (resolved->Sig.ParamCount != 0) {
            resolved->Sig.Params = static_cast<TypeEntry*>(
                    std::calloc(resolved->Sig.ParamCount, sizeof(TypeEntry)));
            if (resolved->Sig.Params == nullptr) {
                SIGMINER_FreeBpftraceResolvedSymbol(resolved);
                SIGMINER_FreeResult(&result);
                result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
                return result;
            }

            for (size_t i = 0; i < resolved->Sig.ParamCount; ++i) {
                if (!CopyTypeEntry(cppResolved.sig.params[i], &resolved->Sig.Params[i])) {
                    SIGMINER_FreeBpftraceResolvedSymbol(resolved);
                    SIGMINER_FreeResult(&result);
                    result.RetCode = RETURN_CODE_INTERNAL_FAILURE;
                    return result;
                }
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

void SIGMINER_FreeRichSignature(RichSignature* sig)
{
    if (sig == nullptr)
        return;

    FreeRichTypeEntryTree(&sig->Ret);

    for (size_t i = 0; i < sig->ParamCount; ++i) {
        std::free(sig->Params[i].Name);
        FreeRichTypeEntryTree(&sig->Params[i].Type);
    }

    std::free(sig->Params);
    ZeroRichSignature(sig);
}

void SIGMINER_FreeBpftraceResolvedSymbol(BpftraceResolvedSymbol* resolved)
{
    if (resolved == nullptr)
        return;

    SIGMINER_FreeSignature(&resolved->Sig);

    std::free(const_cast<char*>(resolved->Target.ModulePath));
    resolved->Target.ModulePath = nullptr;

    std::free(const_cast<char*>(resolved->Target.Symbol));
    resolved->Target.Symbol = nullptr;
}

void SIGMINER_FreeResult(Result* res)
{
    if (res == nullptr)
        return;

    SIGMINER_FreeSignature(&res->Sig);
    res->HasSignature = false;
    res->RetCode = RETURN_CODE_SUCCESS;
}

void SIGMINER_FreeRichResult(RichResult* res)
{
    if (res == nullptr)
        return;

    SIGMINER_FreeRichSignature(&res->Sig);
    res->HasSignature = false;
    res->RetCode = RETURN_CODE_SUCCESS;
}

void SIGMINER_FreeCString(const char* str)
{
    std::free(const_cast<char*>(str));
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

const char* SIGMINER_BuildBpftraceArgumentPrintExpr(const Signature* sig)
{
    if (sig == nullptr)
        return nullptr;

    if (sig->ParamCount != 0 && sig->Params == nullptr)
        return nullptr;

    sigminer::Signature cppSignature(*sig);
    std::string result = bpftrace_mapper::BuildBpftraceArgumentPrintExpr(cppSignature);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildBpftraceReturnPrintExpr(const TypeEntry* retType)
{
    if (retType == nullptr)
        return nullptr;

    sigminer::TypeEntry cppRetType(*retType);
    std::string result = bpftrace_mapper::BuildBpftraceReturnPrintExpr(cppRetType);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildBpftraceProbeBody(
        BpftraceProbeKind probeKind,
        const Signature* sig,
        const BpftraceRenderOptions* opts)
{
    if (sig == nullptr)
        return nullptr;

    if (sig->ParamCount != 0 && sig->Params == nullptr)
        return nullptr;

    bpftrace_mapper::BpftraceProbeKind cppProbeKind;
    if (probeKind == BPFTRACE_PROBE_KIND_ENTRY)
        cppProbeKind = bpftrace_mapper::BpftraceProbeKind::ENTRY;
    else if (probeKind == BPFTRACE_PROBE_KIND_RETURN)
        cppProbeKind = bpftrace_mapper::BpftraceProbeKind::RETURN;
    else
        return nullptr;

    sigminer::Signature cppSig(*sig);
    const bpftrace_mapper::BpftraceRenderOptions cppOpts = ToCppBpfTraceRenderOptions(opts);

    std::string result = bpftrace_mapper::BuildBpftraceProbeBody(cppProbeKind, cppSig, cppOpts);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildBpftraceUprobeScriptForTarget(
        const BpftraceProbeTarget* target,
        const Signature* sig,
        const BpftraceRenderOptions* opts)
{
    if (target == nullptr || sig == nullptr)
        return nullptr;

    if (target->ModulePath == nullptr || target->Symbol == nullptr)
        return nullptr;

    if (sig->ParamCount != 0 && sig->Params == nullptr)
        return nullptr;

    const bpftrace_mapper::BpftraceProbeTarget cppTarget = ToCppBpftraceProbeTarget(*target);
    const sigminer::Signature cppSig(*sig);
    const bpftrace_mapper::BpftraceRenderOptions cppOpts = ToCppBpfTraceRenderOptions(opts);

    std::string result = bpftrace_mapper::BuildBpftraceUprobeScript(cppTarget, cppSig, cppOpts);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildRichBpftraceUprobeScriptForTarget(
        const BpftraceProbeTarget* target,
        const RichSignature* sig,
        const BpftraceRenderOptions* opts)
{
    if (target == nullptr || sig == nullptr)
        return nullptr;

    if (target->ModulePath == nullptr || target->Symbol == nullptr)
        return nullptr;

    if (sig->ParamCount != 0 && sig->Params == nullptr)
        return nullptr;

    const bpftrace_mapper::BpftraceProbeTarget cppTarget = ToCppBpftraceProbeTarget(*target);
    const sigminer::rich::Signature cppSig(*sig);
    const bpftrace_mapper::rich::ExtendedBpftraceRenderOptions cppOpts =
            ToCppRichBpfTraceRenderOptions(opts);

    std::string result = bpftrace_mapper::rich::BuildBpftraceUprobeScript(cppTarget, cppSig, cppOpts);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}

const char* SIGMINER_BuildBpftraceUprobeScriptForResolvedSymbols(
        const BpftraceResolvedSymbol* resolvedSymbols,
        size_t resolvedSymbolCount,
        const BpftraceRenderOptions* opts)
{
    if (resolvedSymbols == nullptr && resolvedSymbolCount != 0)
        return nullptr;

    for (size_t i = 0; i < resolvedSymbolCount; ++i) {
        if (resolvedSymbols[i].Target.ModulePath == nullptr ||
            resolvedSymbols[i].Target.Symbol == nullptr)
            return nullptr;

        if (resolvedSymbols[i].Sig.ParamCount != 0 &&
            resolvedSymbols[i].Sig.Params == nullptr)
            return nullptr;
    }

    const std::vector<bpftrace_mapper::BpftraceResolvedSymbol> cppResolvedSymbols =
            ToCppBpftraceResolvedSymbols(resolvedSymbols, resolvedSymbolCount);
    const bpftrace_mapper::BpftraceRenderOptions cppOpts = ToCppBpfTraceRenderOptions(opts);

    std::string result =
            bpftrace_mapper::BuildBpftraceUprobeScript(cppResolvedSymbols, cppOpts);

    if (result.empty())
        return nullptr;

    return DuplicateCString(result);
}
