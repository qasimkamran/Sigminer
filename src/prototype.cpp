#include <cstdio>
#include <string>

#include <llvm/Support/Error.h>

#include "internal/dwarf_session.h"
#include "internal/line_table_lookup.h"
#include "internal/return_site_finder.h"
#include "internal/subprogram_finder.h"
#include "sigminer/func_info.h"
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
            ToString(entry.kind),
            ToString(entry.sign),
            entry.size,
            entry.isPointer ? "true" : "false",
            entry.name.c_str());
}

void PrintReturnSites(const char* sharedObjectFilePath, const char* symbol)
{
    llvm::Expected<dwarf_session::Session> sessionOrErr =
            dwarf_session::Open(sharedObjectFilePath);
    if (!sessionOrErr) {
        std::printf("Return sites - unavailable: %s\n", llvm::toString(sessionOrErr.takeError()).c_str());
        return;
    }

    dwarf_session::Session& session = *sessionOrErr;
    if (!session.context || !session.object) {
        std::printf("Return sites - unavailable: missing DWARF context or object file\n");
        return;
    }

    const llvm::DWARFDie subprogramDie =
            subprogram_finder::GetTargetSubprogram(*session.context, symbol);
    if (!subprogramDie) {
        std::printf("Return sites - unavailable: symbol not found in DWARF\n");
        return;
    }

    std::vector<sigminer::ReturnSite> returnSites =
            GetReturnSitesWithinSubprogramDie(subprogramDie, *session.object);
    for (sigminer::ReturnSite& returnSite : returnSites)
        PopulateSourceLocationForReturnSite(*session.context, subprogramDie, returnSite);

    std::printf("Return sites - count:%zu\n", returnSites.size());
    for (const sigminer::ReturnSite& returnSite : returnSites) {
        if (returnSite.sourceLocation) {
            std::printf(
                    "Return site - {address:0x%llx, funcOffset:0x%llx, source:%s:%u:%u}\n",
                    static_cast<unsigned long long>(returnSite.instructionAddress),
                    static_cast<unsigned long long>(returnSite.funcOffset),
                    returnSite.sourceLocation->file.c_str(),
                    returnSite.sourceLocation->line,
                    returnSite.sourceLocation->column);
        } else {
            std::printf(
                    "Return site - {address:0x%llx, funcOffset:0x%llx, source:<unresolved>}\n",
                    static_cast<unsigned long long>(returnSite.instructionAddress),
                    static_cast<unsigned long long>(returnSite.funcOffset));
        }
    }
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
    if (!result.sig) {
        std::printf("FAIL: library returned code %d\n", static_cast<int>(result.retCode));
        return 1;
    }

    PrintType("Return type", result.sig->ret);
    for (const sigminer::TypeEntry& typeEntry : result.sig->params)
        PrintType("Argument type", typeEntry);

    PrintReturnSites(argv[1], argv[2]);

    std::printf("PASS: all steps passed without failure\n");
    return 0;
}
