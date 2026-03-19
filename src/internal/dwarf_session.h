#pragma once

#include <memory>
#include <string_view>

#include <llvm/Object/Binary.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Support/Error.h>

namespace dwarf_session {

struct Session
{
    llvm::object::OwningBinary<llvm::object::Binary> Binary;
    llvm::object::ObjectFile* Object = nullptr;
    std::unique_ptr<llvm::DWARFContext> Context;

    Session(llvm::object::OwningBinary<llvm::object::Binary>&& binary,
            llvm::object::ObjectFile* object,
            std::unique_ptr<llvm::DWARFContext>&& context);
};

void InitializeLLVM();
llvm::Expected<Session> Open(std::string_view filePath);

} // namespace dwarf_session
