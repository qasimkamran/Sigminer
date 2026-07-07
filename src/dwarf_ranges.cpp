#include "internal/dwarf_ranges.h"

#include <algorithm>
#include <limits>
#include <system_error>

#include <llvm/Object/ObjectFile.h>

namespace dwarf_ranges {

llvm::Expected<llvm::DWARFAddressRangesVector> GetDieAddressRanges(
        const llvm::DWARFDie& die)
{
    if (!die) {
        return llvm::createStringError(
                std::errc::invalid_argument,
                "DIE is invalid");
    }

    llvm::Expected<llvm::DWARFAddressRangesVector> rangesOrErr = die.getAddressRanges();
    if (rangesOrErr)
        return std::move(*rangesOrErr);

    llvm::consumeError(rangesOrErr.takeError());

    std::uint64_t lowPc = 0;
    std::uint64_t highPc = 0;
    std::uint64_t sectionIndex = llvm::object::SectionedAddress::UndefSection;
    if (die.getLowAndHighPC(lowPc, highPc, sectionIndex) && lowPc < highPc)
        return llvm::DWARFAddressRangesVector{{lowPc, highPc, sectionIndex}};

    return llvm::createStringError(
            std::errc::result_out_of_range,
            "DIE has no address ranges");
}

std::uint64_t GetFunctionLow(const llvm::DWARFAddressRangesVector& ranges)
{
    std::uint64_t functionLow = std::numeric_limits<std::uint64_t>::max();
    for (const llvm::DWARFAddressRange& range : ranges) {
        if (range.LowPC < range.HighPC)
            functionLow = std::min(functionLow, range.LowPC);
    }
    return functionLow;
}

bool AddressInRanges(
        const llvm::DWARFAddressRangesVector& ranges,
        std::uint64_t address)
{
    for (const llvm::DWARFAddressRange& range : ranges) {
        if (range.LowPC <= address && address < range.HighPC)
            return true;
    }
    return false;
}

} // namespace dwarf_ranges
