#include "sigminer/sigminer.h"

#include <llvm/Support/Error.h>

#include "internal/dwarf_session.h"
#include "internal/signature_builder.h"
#include "internal/subprogram_finder.h"

namespace sigminer {

namespace {

struct DwarfSessionResult
{
    std::optional<dwarf_session::Session> session = std::nullopt;
    ReturnCode retCode = ReturnCode::SUCCESS;
};

struct SubprogramResult
{
    llvm::DWARFDie subprogramDie{};
    ReturnCode retCode = ReturnCode::SUCCESS;
};

} // namespace

static DwarfSessionResult ValidateAndOpenDwarfSession(const std::string& sharedObjectFilePath)
{
    if (sharedObjectFilePath.empty()) {
        return {.retCode = ReturnCode::INVALID_INPUT};
    }

    llvm::Expected<dwarf_session::Session> sessionOrErr = dwarf_session::Open(sharedObjectFilePath);
    if (!sessionOrErr)
    {
        const std::string error = llvm::toString(sessionOrErr.takeError());
        if (error.find("object file") != std::string::npos) {
            return {.retCode = ReturnCode::INVALID_INPUT};
        } else {
            return {.retCode = ReturnCode::FILE_OPEN_FAILURE};
        }
    }

    dwarf_session::Session& session = *sessionOrErr;
    if (!session.context) {
        return {.retCode = ReturnCode::DWARF_UNAVAILABLE};
    }

    return {
            .session = std::move(*sessionOrErr),
            .retCode = ReturnCode::SUCCESS,
    };
}

static SubprogramResult ValidateAndGetSubprogram(
        llvm::DWARFContext& context,
        const std::string& symbol)
{
    llvm::DWARFDie subprogramDie = subprogram_finder::GetTargetSubprogram(context, symbol);
    if (!subprogramDie.isValid()) {
        return {.retCode = ReturnCode::SYMBOL_RESOLUTION_FAILURE};
    }

    if (!subprogramDie.isSubprogramDIE()) {
        return {.retCode = ReturnCode::FUNCTION_DIE_NOT_IN_RANGE};
    }

    return {
            .subprogramDie = subprogramDie,
            .retCode = ReturnCode::SUCCESS,
    };
}

Result GetSignatureFromSharedObjectBySymbol(
        const std::string& sharedObjectFilePath,
        const std::string& symbol)
{
    Result result{};

    DwarfSessionResult dwarfSession = ValidateAndOpenDwarfSession(sharedObjectFilePath);
    if (dwarfSession.retCode != ReturnCode::SUCCESS) {
        result.retCode = dwarfSession.retCode;
        return result;
    }

    dwarf_session::Session& session = *dwarfSession.session;
    SubprogramResult subprogram = ValidateAndGetSubprogram(*session.context, symbol);
    if (subprogram.retCode != ReturnCode::SUCCESS) {
        result.retCode = subprogram.retCode;
        return result;
    }

    result.sig = signature_builder::BuildSignature(subprogram.subprogramDie);
    result.retCode = ReturnCode::SUCCESS;
    return result;
}

namespace rich {

Result GetSignatureFromSharedObjectBySymbol(
        const std::string& sharedObjectFilePath,
        const std::string& symbol)
{
    Result result{};

    if (sharedObjectFilePath.empty() || symbol.empty()) {
        result.retCode = ReturnCode::INVALID_INPUT;
        return result;
    }

    DwarfSessionResult dwarfSession = ValidateAndOpenDwarfSession(sharedObjectFilePath);
    if (dwarfSession.retCode != ReturnCode::SUCCESS) {
        result.retCode = dwarfSession.retCode;
        return result;
    }

    dwarf_session::Session& session = *dwarfSession.session;
    SubprogramResult subprogram = ValidateAndGetSubprogram(*session.context, symbol);
    if (subprogram.retCode != ReturnCode::SUCCESS) {
        result.retCode = subprogram.retCode;
        return result;
    }

    result.sig = signature_builder::rich::BuildSignature(subprogram.subprogramDie);
    result.retCode = ReturnCode::SUCCESS;
    return result;
}

} // namespace rich

} // namespace sigminer
