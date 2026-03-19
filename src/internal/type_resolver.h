#pragma once

#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>

namespace type_resolver {

bool IsWrapperTag(llvm::dwarf::Tag tag);
llvm::DWARFDie ResolveUnderlyingType(llvm::DWARFDie type, unsigned maxDepth = 64);

} // namespace type_resolver
