#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

#include "internal/subprogram_finder.h"

namespace dwarf_test_utils {

inline std::filesystem::path BuiltArtifactPath(std::string_view filename)
{
    const std::filesystem::path exePath = std::filesystem::read_symlink("/proc/self/exe");
    return exePath.parent_path() / std::string(filename);
}

inline std::string ToString(llvm::Error error)
{
    std::string message;
    llvm::raw_string_ostream stream(message);
    stream << error;
    return message;
}

inline llvm::DWARFDie GetFormalParameterDie(llvm::DWARFDie subprogramDie, std::size_t index)
{
    std::size_t currentIndex = 0;
    for (llvm::DWARFDie child : subprogramDie.children()) {
        if (!child.isValid())
            continue;

        if (child.getTag() != llvm::dwarf::DW_TAG_formal_parameter)
            continue;

        if (currentIndex == index)
            return child;

        ++currentIndex;
    }

    return {};
}

inline llvm::DWARFDie GetSubprogramDie(llvm::DWARFContext& dwarfContext, std::string_view name)
{
    return subprogram_finder::GetTargetSubprogram(dwarfContext, name);
}

inline llvm::DWARFDie FindNamedDieInTree(
        llvm::DWARFDie die,
        llvm::dwarf::Tag tag,
        std::string_view name)
{
    if (!die.isValid())
        return {};

    if (die.getTag() == tag) {
        const char* shortName = die.getShortName();
        if (shortName != nullptr && name == shortName)
            return die;
    }

    for (llvm::DWARFDie child : die.children()) {
        llvm::DWARFDie found = FindNamedDieInTree(child, tag, name);
        if (found.isValid())
            return found;
    }

    return {};
}

inline llvm::DWARFDie FindNamedDie(
        llvm::DWARFContext& dwarfContext,
        llvm::dwarf::Tag tag,
        std::string_view name)
{
    for (const std::unique_ptr<llvm::DWARFUnit>& cu : dwarfContext.compile_units()) {
        llvm::DWARFDie found = FindNamedDieInTree(cu->getUnitDIE(false), tag, name);
        if (found.isValid())
            return found;
    }

    return {};
}

} // namespace dwarf_test_utils
