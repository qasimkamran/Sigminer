#pragma once

#include <vector>

#include "sigminer/func_info.h"
#include <llvm/DebugInfo/DWARF/DWARFAddressRange.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Object/ObjectFile.h>

llvm::DWARFAddressRangesVector GetSubprogramDieAddressRanges(const llvm::DWARFDie& subprogramDie);
std::vector<sigminer::ReturnSite> GetReturnSitesWithinSubprogramDie(
        const llvm::DWARFDie& subprogramDie,
        const llvm::object::ObjectFile& object);
