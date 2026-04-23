#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>

#include "sigminer/func_info.h"

bool PopulateSourceLocationForReturnSite(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        sigminer::ReturnSite& returnSite);

std::optional<sigminer::SourceLocation> GetSourceLocationForAddress(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        std::uint64_t address);

std::vector<sigminer::SourceReturnProbePoint> MapSourceReturnsToProbePoints(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        const std::vector<sigminer::SourceReturnCandidate>& sourceReturns);

std::optional<std::string> InferSourceFileForSubprogram(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie);
