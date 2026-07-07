#include "internal/line_table_lookup.h"

#include <optional>
#include <string>
#include <system_error>

#include <llvm/DebugInfo/DWARF/DWARFDebugLine.h>
#include <llvm/DebugInfo/DWARF/DWARFUnit.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Path.h>

namespace line_table_lookup {

llvm::Expected<sigminer::SourceLocation> GetSourceLocationForAddress(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        std::uint64_t address)
{
    if (!subprogramDie) {
        return llvm::createStringError(
                std::errc::invalid_argument,
                "subprogram DIE is invalid");
    }

    llvm::DWARFUnit* unit = subprogramDie.getDwarfUnit();
    if (!unit) {
        return llvm::createStringError(
                std::errc::no_such_file_or_directory,
                "subprogram DIE has no DWARF unit");
    }

    const llvm::DWARFDebugLine::LineTable* lineTable =
            dwarfContext.getLineTableForUnit(unit);
    if (!lineTable) {
        return llvm::createStringError(
                std::errc::no_such_file_or_directory,
                "DWARF unit has no line table");
    }

    const llvm::object::SectionedAddress sectionedAddress{address, llvm::object::SectionedAddress::UndefSection};
    
    const std::uint32_t rowIndex = lineTable->lookupAddress(sectionedAddress);
    if (rowIndex == lineTable->UnknownRowIndex || rowIndex >= lineTable->Rows.size()) {
        return llvm::createStringError(
                std::errc::result_out_of_range,
                "address not found in line table");
    }

    const llvm::DWARFDebugLine::Row& row = lineTable->Rows[rowIndex];

    std::string file{};
    const char* compilationDir = unit->getCompilationDir();
    if (!lineTable->getFileNameByIndex(
                row.File,
                compilationDir != nullptr ? compilationDir : "",
                llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
                file)) {
        return llvm::createStringError(
                std::errc::no_such_file_or_directory,
                "line-table row has no resolvable source file");
    }

    return sigminer::SourceLocation{
            .file = file,
            .line = row.Line,
            .column = row.Column,
    };
}

llvm::Error PopulateSourceLocationForReturnSite(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        sigminer::ReturnSite& returnSite)
{
    llvm::Expected<sigminer::SourceLocation> sourceLocation =
            GetSourceLocationForAddress(
                    dwarfContext,
                    subprogramDie,
                    returnSite.instructionAddress);
    if (!sourceLocation)
        return sourceLocation.takeError();

    returnSite.sourceLocation = std::move(*sourceLocation);
    return llvm::Error::success();
}

} // namespace line_table_lookup
