#pragma once

#include <cstdint>

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Support/Error.h>

#include "sigminer/func_info.h"

namespace line_table_lookup {

llvm::Error PopulateSourceLocationForReturnSite(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        sigminer::ReturnSite& returnSite);

llvm::Expected<sigminer::SourceLocation> GetSourceLocationForAddress(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        std::uint64_t address);

} // namespace line_table_lookup
