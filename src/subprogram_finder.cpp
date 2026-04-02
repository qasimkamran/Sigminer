#include "internal/subprogram_finder.h"

#include <memory>

#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/DebugInfo/DWARF/DWARFUnit.h>

namespace subprogram_finder {

static llvm::DWARFDie GetTargetSubprogramInUnit(const llvm::DWARFDie& cuDie, std::string_view funcName)
{
    for (llvm::DWARFDie die : cuDie.children()) {
        if (die.getTag() != llvm::dwarf::DW_TAG_subprogram)
            continue;

        const char* name = die.getShortName();
        if (!name)
            continue;

        if (funcName == name)
            return die;
    }

    return {};
}

static llvm::DWARFDie GetTargetSubprogramInTree(const llvm::DWARFDie& die, std::string_view funcName)
{
    if (!die)
        return {};

    if (die.getTag() == llvm::dwarf::DW_TAG_subprogram) {
        const char* name = die.getShortName();
        if (name && funcName == name)
            return die;
    }

    for (llvm::DWARFDie child : die.children()) {
        llvm::DWARFDie subprogramDie = GetTargetSubprogramInTree(child, funcName);
        if (subprogramDie)
            return subprogramDie;
    }

    return {};
}

llvm::DWARFDie GetTargetSubprogram(llvm::DWARFContext& dwarfContext, std::string_view funcName)
{
    for (const std::unique_ptr<llvm::DWARFUnit>& cu : dwarfContext.compile_units()) {
        const llvm::DWARFDie cuDie = cu->getUnitDIE(false);
        if (!cuDie)
            continue;

        llvm::DWARFDie subprogramDie = GetTargetSubprogramInUnit(cuDie, funcName);
        if (subprogramDie)
            return subprogramDie;

        subprogramDie = GetTargetSubprogramInTree(cuDie, funcName);
        if (subprogramDie)
            return subprogramDie;
    }

    return {};
}

} // namespace subprogram_finder
