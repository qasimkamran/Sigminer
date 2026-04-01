#include "sigminer/sigminer.h"

#include <llvm/Support/Error.h>

#include "internal/dwarf_session.h"
#include "internal/signature_builder.h"
#include "internal/subprogram_finder.h"

namespace sigminer {

Result GetSignatureFromSharedObjectBySymbol(
        const std::string& sharedObjectFilePath,
        const std::string& symbol)
{
    Result result{};

    if (sharedObjectFilePath.empty() || symbol.empty()) {
        result.retCode = ReturnCode::INVALID_INPUT;
        return result;
    }

    llvm::Expected<dwarf_session::Session> sessionOrErr = dwarf_session::Open(sharedObjectFilePath);
    if (!sessionOrErr) {
        const std::string error = llvm::toString(sessionOrErr.takeError());
        if (error.find("object file") != std::string::npos) {
            result.retCode = ReturnCode::INVALID_INPUT;
        } else {
            result.retCode = ReturnCode::FILE_OPEN_FAILURE;
        }
        return result;
    }

    dwarf_session::Session& session = *sessionOrErr;
    if (!session.context) {
        result.retCode = ReturnCode::DWARF_UNAVAILABLE;
        return result;
    }

    llvm::DWARFDie subprogramDie = subprogram_finder::GetTargetSubprogram(*session.context, symbol);
    if (!subprogramDie.isValid()) {
        result.retCode = ReturnCode::SYMBOL_RESOLUTION_FAILURE;
        return result;
    }

    if (!subprogramDie.isSubprogramDIE()) {
        result.retCode = ReturnCode::FUNCTION_DIE_NOT_IN_RANGE;
        return result;
    }

    result.sig = signature_builder::BuildSignature(subprogramDie);
    result.retCode = ReturnCode::SUCCESS;
    return result;
}

} // namespace sigminer
