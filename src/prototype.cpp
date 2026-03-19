#include <cstdio>
#include <string>
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

void PrintType(const char* label, const sigminer::TypeEntry& entry)
{
    std::printf(
            "%s - {kind:%s, signedness:%s, size:%zu, isPointer:%s, name:%s}\n",
            label,
            ToString(entry.Kind),
            ToString(entry.Sign),
            entry.Size,
            entry.IsPointer ? "true" : "false",
            entry.Name.c_str());
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::printf("FAIL: pass exactly 2 arguments\n");
        return 1;
    }

    const sigminer::Result result =
            sigminer::GetSignatureFromSharedObjectBySymbol(argv[1], argv[2]);
    if (!result.Sig) {
        std::printf("FAIL: library returned code %d\n", static_cast<int>(result.RetCode));
        return 1;
    }

    PrintType("Return type", result.Sig->Ret);
    for (const sigminer::TypeEntry& typeEntry : result.Sig->Params)
        PrintType("Argument type", typeEntry);

    std::printf("PASS: all steps passed without failure\n");
    return 0;
}
