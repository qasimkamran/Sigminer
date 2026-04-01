#include "internal/dwarf_session.h"

#include <string>

#include <llvm/Support/Casting.h>

namespace dwarf_session {

Session::Session(llvm::object::OwningBinary<llvm::object::Binary>&& binary,
                 llvm::object::ObjectFile* object,
                 std::unique_ptr<llvm::DWARFContext>&& context)
    : binary(std::move(binary)), object(object), context(std::move(context))
{
}

void InitializeLLVM()
{
    static const bool initialized = true;
    (void) initialized;
}

llvm::Expected<Session> Open(std::string_view filePath)
{
    InitializeLLVM();

    llvm::Expected<llvm::object::OwningBinary<llvm::object::Binary>> binaryOrErr =
            llvm::object::createBinary(std::string(filePath));

    if (!binaryOrErr)
        return binaryOrErr.takeError();

    llvm::object::OwningBinary<llvm::object::Binary> owningBinary = std::move(*binaryOrErr);
    llvm::object::Binary& binary = *owningBinary.getBinary();
    llvm::object::ObjectFile* object = llvm::dyn_cast<llvm::object::ObjectFile>(&binary);
    if (!object)
        return llvm::createStringError(std::errc::invalid_argument, "input is not an object file");

    return Session(std::move(owningBinary), object, llvm::DWARFContext::create(*object));
}

} // namespace dwarf_session
