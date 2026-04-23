#include "internal/line_table_lookup.h"

#include <optional>
#include <string>

#include <llvm/DebugInfo/DWARF/DWARFDebugLine.h>
#include <llvm/DebugInfo/DWARF/DWARFUnit.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Path.h>

std::optional<sigminer::SourceLocation> GetSourceLocationForAddress(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        std::uint64_t address)
{
    if (!subprogramDie)
        return std::nullopt;

    llvm::DWARFUnit* unit = subprogramDie.getDwarfUnit();
    if (!unit)
        return std::nullopt;

    const llvm::DWARFDebugLine::LineTable* lineTable =
            dwarfContext.getLineTableForUnit(unit);
    if (!lineTable)
        return std::nullopt;

    const llvm::object::SectionedAddress sectionedAddress{address, llvm::object::SectionedAddress::UndefSection};
    
    const std::uint32_t rowIndex = lineTable->lookupAddress(sectionedAddress);
    if (rowIndex == lineTable->UnknownRowIndex || rowIndex >= lineTable->Rows.size())
        return std::nullopt;

    const llvm::DWARFDebugLine::Row& row = lineTable->Rows[rowIndex];

    std::string file{};
    const char* compilationDir = unit->getCompilationDir();
    if (!lineTable->getFileNameByIndex(
                row.File,
                compilationDir != nullptr ? compilationDir : "",
                llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
                file)) {
        return std::nullopt;
    }

    return sigminer::SourceLocation{
            .file = file,
            .line = row.Line,
            .column = row.Column,
    };
}

bool PopulateSourceLocationForReturnSite(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        sigminer::ReturnSite& returnSite)
{
    std::optional<sigminer::SourceLocation> sourceLocation =
            GetSourceLocationForAddress(
                    dwarfContext,
                    subprogramDie,
                    returnSite.instructionAddress);
    if (!sourceLocation)
        return false;

    returnSite.sourceLocation = std::move(*sourceLocation);
    return true;
}
