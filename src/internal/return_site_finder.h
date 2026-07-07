#pragma once

#include <vector>

#include "sigminer/func_info.h"
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Object/ObjectFile.h>

namespace return_site_finder {

std::vector<sigminer::ReturnSite> GetReturnSitesWithinSubprogramDie(
        const llvm::DWARFDie& subprogramDie,
        const llvm::object::ObjectFile& object);

} // namespace return_site_finder
