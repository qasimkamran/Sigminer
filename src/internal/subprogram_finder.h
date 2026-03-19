#pragma once

#include <string_view>

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>

namespace subprogram_finder {

llvm::DWARFDie GetTargetSubprogram(llvm::DWARFContext& dwarfContext, std::string_view funcName);

} // namespace subprogram_finder
