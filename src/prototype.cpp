#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>

#include <llvm/Support/Error.h>

#include "internal/dwarf_session.h"
#include "internal/signature_builder.h"
#include "internal/subprogram_finder.h"
#include "sigminer/sigminer.h"

namespace {

const char* ToString(sigminer::PrimitiveKind kind)
{
    switch (kind) {
        case sigminer::PrimitiveKind::VOID: return "void";
        case sigminer::PrimitiveKind::BOOL: return "bool";
        case sigminer::PrimitiveKind::INT: return "int";
        case sigminer::PrimitiveKind::FLOAT: return "float";
        case sigminer::PrimitiveKind::POINTER: return "pointer";
        case sigminer::PrimitiveKind::ENUM: return "enum";
        case sigminer::PrimitiveKind::AGGREGATE: return "aggregate";
        case sigminer::PrimitiveKind::UNKNOWN: return "unknown";
    }
    return "invalid";
}

const char* ToString(sigminer::Signedness sign)
{
    switch (sign) {
        case sigminer::Signedness::SIGNED: return "signed";
        case sigminer::Signedness::UNSIGNED: return "unsigned";
        case sigminer::Signedness::UNKNOWN: return "unknown";
    }
    return "invalid";
}

void PrintIndent(std::size_t depth)
{
    for (std::size_t i = 0; i < depth; ++i)
        std::printf("  ");
}

void PrintType(const char* label, const sigminer::TypeEntry& entry)
{
    std::printf(
            "%s - {kind:%s, signedness:%s, size:%zu, isPointer:%s, name:%s}\n",
            label,
            ToString(entry.kind),
            ToString(entry.sign),
            entry.size,
            entry.isPointer ? "true" : "false",
            entry.name.c_str());
}

sigminer::ReturnCode GetRichSignatureFromSharedObjectBySymbol(
        const std::string& sharedObjectFilePath,
        const std::string& symbol,
        std::optional<sigminer::rich::Signature>* sig)
{
    if (sig == nullptr)
        return sigminer::ReturnCode::INTERNAL_FAILURE;

    sig->reset();

    if (sharedObjectFilePath.empty() || symbol.empty())
        return sigminer::ReturnCode::INVALID_INPUT;

    llvm::Expected<dwarf_session::Session> sessionOrErr = dwarf_session::Open(sharedObjectFilePath);
    if (!sessionOrErr) {
        const std::string error = llvm::toString(sessionOrErr.takeError());
        if (error.find("object file") != std::string::npos)
            return sigminer::ReturnCode::INVALID_INPUT;
        return sigminer::ReturnCode::FILE_OPEN_FAILURE;
    }

    dwarf_session::Session& session = *sessionOrErr;
    if (!session.context)
        return sigminer::ReturnCode::DWARF_UNAVAILABLE;

    llvm::DWARFDie subprogramDie =
            subprogram_finder::GetTargetSubprogram(*session.context, symbol);
    if (!subprogramDie.isValid())
        return sigminer::ReturnCode::SYMBOL_RESOLUTION_FAILURE;

    if (!subprogramDie.isSubprogramDIE())
        return sigminer::ReturnCode::FUNCTION_DIE_NOT_IN_RANGE;

    *sig = signature_builder::rich::BuildSignature(subprogramDie);
    return sigminer::ReturnCode::SUCCESS;
}

void PrintRichType(const char* label, const sigminer::rich::TypeEntry& entry, std::size_t depth = 0)
{
    PrintIndent(depth);
    std::printf(
            "%s - {kind:%s, signedness:%s, size:%zu, name:%s, isConst:%s, isStringLike:%s, "
            "isRecursiveReference:%s, arrayCount:%zu}\n",
            label,
            ToString(entry.kind),
            ToString(entry.sign),
            entry.size,
            entry.name.c_str(),
            entry.isConst ? "true" : "false",
            entry.isStringLike ? "true" : "false",
            entry.isRecursiveReference ? "true" : "false",
            entry.arrayCount);

    if (entry.pointee)
        PrintRichType("pointee", *entry.pointee, depth + 1);

    if (entry.elementType)
        PrintRichType("elementType", *entry.elementType, depth + 1);

    for (const sigminer::rich::TypeMember& member : entry.members) {
        PrintIndent(depth + 1);
        std::printf("member %s @ %zu\n", member.name.c_str(), member.offset);
        if (member.type)
            PrintRichType("type", *member.type, depth + 2);
    }
}

void PrintRichSignature(const sigminer::rich::Signature& sig)
{
    PrintRichType("Return type", sig.ret);
    for (const sigminer::rich::Parameter& param : sig.params) {
        std::printf("Argument %s\n", param.name.c_str());
        if (param.type) {
            PrintRichType("type", *param.type, 1);
        }
        else {
            PrintIndent(1);
            std::printf("type - <null>\n");
        }
    }
    std::printf("hasVarArgs: %s\n", sig.hasVarArgs ? "true" : "false");
}

} // namespace

int main(int argc, char** argv)
{
    bool emitRichSignature = false;
    int firstArgIndex = 1;
    if (argc > 1 && (std::string(argv[1]) == "--rich" || std::string(argv[1]) == "--rich-script")) {
        emitRichSignature = true;
        firstArgIndex = 2;
    }

    if (argc - firstArgIndex != 2) {
        std::printf("FAIL: usage: %s [--rich] <shared_object> <symbol>\n", argv[0]);
        return 1;
    }

    const std::string sharedObjectFilePath = argv[firstArgIndex];
    const std::string symbol = argv[firstArgIndex + 1];

    if (emitRichSignature) {
        std::optional<sigminer::rich::Signature> richSig = std::nullopt;
        const sigminer::ReturnCode retCode =
                GetRichSignatureFromSharedObjectBySymbol(sharedObjectFilePath, symbol, &richSig);
        if (!richSig) {
            std::printf("FAIL: rich signature lookup returned code %d\n", static_cast<int>(retCode));
            return 1;
        }

        PrintRichSignature(*richSig);
        std::printf("PASS: all steps passed without failure\n");
        return 0;
    }

    const sigminer::Result result =
            sigminer::GetSignatureFromSharedObjectBySymbol(sharedObjectFilePath, symbol);
    if (!result.sig) {
        std::printf("FAIL: library returned code %d\n", static_cast<int>(result.retCode));
        return 1;
    }

    PrintType("Return type", result.sig->ret);
    for (const sigminer::TypeEntry& typeEntry : result.sig->params)
        PrintType("Argument type", typeEntry);

    std::printf("PASS: all steps passed without failure\n");
    return 0;
}
