#pragma once

#include <cstdint>

#include <llvm/DebugInfo/DWARF/DWARFAddressRange.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Support/Error.h>

namespace dwarf_ranges {

llvm::Expected<llvm::DWARFAddressRangesVector> GetDieAddressRanges(
        const llvm::DWARFDie& die);

std::uint64_t GetFunctionLow(const llvm::DWARFAddressRangesVector& ranges);

bool AddressInRanges(
        const llvm::DWARFAddressRangesVector& ranges,
        std::uint64_t address);

} // namespace dwarf_ranges
