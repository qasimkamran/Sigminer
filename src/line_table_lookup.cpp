#include "internal/line_table_lookup.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugLine.h>
#include <llvm/DebugInfo/DWARF/DWARFUnit.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Path.h>

namespace {

struct RowCandidate
{
    std::uint32_t rowIndex = 0;
    int matchRank = std::numeric_limits<int>::max();
    std::uint32_t columnDistance = std::numeric_limits<std::uint32_t>::max();
    bool discriminatorMatches = false;
    bool isStmt = false;
    bool epilogueBegin = false;
};

bool AddressInRanges(const llvm::DWARFAddressRangesVector& ranges, std::uint64_t address)
{
    for (const llvm::DWARFAddressRange& range : ranges) {
        if (range.LowPC <= address && address < range.HighPC)
            return true;
    }
    return false;
}

llvm::DWARFAddressRangesVector GetDieAddressRanges(const llvm::DWARFDie& die)
{
    llvm::DWARFAddressRangesVector ranges{};
    if (!die)
        return ranges;

    llvm::Expected<llvm::DWARFAddressRangesVector> rangesOrErr = die.getAddressRanges();
    if (rangesOrErr)
        return std::move(*rangesOrErr);

    llvm::consumeError(rangesOrErr.takeError());

    std::uint64_t lowPc = 0;
    std::uint64_t highPc = 0;
    std::uint64_t sectionIndex = llvm::object::SectionedAddress::UndefSection;
    if (die.getLowAndHighPC(lowPc, highPc, sectionIndex) && lowPc < highPc)
        ranges.push_back({lowPc, highPc, sectionIndex});

    return ranges;
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

std::string GetRowFile(
        const llvm::DWARFDebugLine::LineTable& lineTable,
        llvm::DWARFUnit& unit,
        const llvm::DWARFDebugLine::Row& row)
{
    std::string file{};
    const char* compilationDir = unit.getCompilationDir();
    lineTable.getFileNameByIndex(
            row.File,
            compilationDir != nullptr ? compilationDir : "",
            llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
            file);
    return file;
}

bool SameFile(const std::string& lhs, const std::string& rhs)
{
    return !lhs.empty() && !rhs.empty() && lhs == rhs;
}

bool MatchesSourceFile(
        const std::string& rowFile,
        const sigminer::SourceReturnCandidate& sourceReturn)
{
    return SameFile(rowFile, sourceReturn.spellingLocation.file) ||
           SameFile(rowFile, sourceReturn.expansionLocation.file);
}

std::uint32_t ColumnDistance(std::uint32_t lhs, std::uint32_t rhs)
{
    return lhs > rhs ? lhs - rhs : rhs - lhs;
}

RowCandidate ScoreRow(
        std::uint32_t rowIndex,
        const llvm::DWARFDebugLine::Row& row,
        const std::string& rowFile,
        const sigminer::SourceReturnCandidate& sourceReturn)
{
    const sigminer::SourceLocation& spelling = sourceReturn.spellingLocation;
    const sigminer::SourceLocation& expansion = sourceReturn.expansionLocation;
    const bool fileMatches = MatchesSourceFile(rowFile, sourceReturn);
    const bool exactSpelling =
            SameFile(rowFile, spelling.file) &&
            row.Line == spelling.line &&
            row.Column == spelling.column;
    const bool exactExpansion =
            SameFile(rowFile, expansion.file) &&
            row.Line == expansion.line &&
            row.Column == expansion.column;
    const bool sameSpellingLine =
            SameFile(rowFile, spelling.file) && row.Line == spelling.line;
    const bool sameExpansionLine =
            SameFile(rowFile, expansion.file) && row.Line == expansion.line;

    RowCandidate candidate{};
    candidate.rowIndex = rowIndex;
    candidate.discriminatorMatches =
            sourceReturn.discriminator && row.Discriminator == *sourceReturn.discriminator;
    candidate.isStmt = row.IsStmt;
    candidate.epilogueBegin = row.EpilogueBegin;

    if (exactSpelling || exactExpansion) {
        candidate.matchRank = 0;
        candidate.columnDistance = 0;
    } else if (sameSpellingLine || sameExpansionLine) {
        candidate.matchRank = 1;
        std::uint32_t distance = std::numeric_limits<std::uint32_t>::max();
        if (sameSpellingLine)
            distance = std::min(distance, ColumnDistance(row.Column, spelling.column));
        if (sameExpansionLine)
            distance = std::min(distance, ColumnDistance(row.Column, expansion.column));
        candidate.columnDistance = distance;
    } else if (fileMatches && candidate.discriminatorMatches) {
        candidate.matchRank = 2;
    } else if (row.EpilogueBegin) {
        candidate.matchRank = 3;
    }

    return candidate;
}

bool BetterRowCandidate(const RowCandidate& lhs, const RowCandidate& rhs)
{
    return std::tuple{
                   lhs.matchRank,
                   lhs.columnDistance,
                   !lhs.discriminatorMatches,
                   !lhs.isStmt,
                   !lhs.epilogueBegin,
                   lhs.rowIndex,
           } <
           std::tuple{
                   rhs.matchRank,
                   rhs.columnDistance,
                   !rhs.discriminatorMatches,
                   !rhs.isStmt,
                   !rhs.epilogueBegin,
                   rhs.rowIndex,
           };
}

void AddCallerFrameAsExpansionLocation(
        const llvm::DWARFDie& die,
        sigminer::SourceReturnCandidate& sourceReturn)
{
    if (!die || die.getTag() != llvm::dwarf::DW_TAG_inlined_subroutine)
        return;

    std::uint32_t callFile = 0;
    std::uint32_t callLine = 0;
    std::uint32_t callColumn = 0;
    std::uint32_t callDiscriminator = 0;
    die.getCallerFrame(callFile, callLine, callColumn, callDiscriminator);

    if (callLine != 0)
        sourceReturn.expansionLocation.line = callLine;
    if (callColumn != 0)
        sourceReturn.expansionLocation.column = callColumn;
    if (callDiscriminator != 0)
        sourceReturn.discriminator = callDiscriminator;
}

} // namespace

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

std::vector<sigminer::SourceReturnProbePoint> MapSourceReturnsToProbePoints(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie,
        const std::vector<sigminer::SourceReturnCandidate>& sourceReturns)
{
    std::vector<sigminer::SourceReturnProbePoint> probePoints{};
    if (!subprogramDie || sourceReturns.empty())
        return probePoints;

    llvm::DWARFUnit* unit = subprogramDie.getDwarfUnit();
    if (!unit)
        return probePoints;

    const llvm::DWARFDebugLine::LineTable* lineTable =
            dwarfContext.getLineTableForUnit(unit);
    if (!lineTable)
        return probePoints;

    const llvm::DWARFAddressRangesVector ranges = GetDieAddressRanges(subprogramDie);
    const std::uint64_t functionLow = GetFunctionLow(ranges);
    if (functionLow == std::numeric_limits<std::uint64_t>::max())
        return probePoints;

    for (const sigminer::SourceReturnCandidate& originalSourceReturn : sourceReturns) {
        sigminer::SourceReturnCandidate sourceReturn = originalSourceReturn;
        AddCallerFrameAsExpansionLocation(subprogramDie, sourceReturn);

        std::optional<RowCandidate> best{};
        for (std::uint32_t rowIndex = 0; rowIndex < lineTable->Rows.size(); ++rowIndex) {
            const llvm::DWARFDebugLine::Row& row = lineTable->Rows[rowIndex];
            if (row.EndSequence)
                continue;

            const std::uint64_t rowAddress = row.Address.Address;
            if (!AddressInRanges(ranges, rowAddress))
                continue;

            const std::string rowFile = GetRowFile(*lineTable, *unit, row);
            RowCandidate candidate = ScoreRow(rowIndex, row, rowFile, sourceReturn);
            if (candidate.matchRank == std::numeric_limits<int>::max())
                continue;

            if (!best || BetterRowCandidate(candidate, *best))
                best = candidate;
        }

        if (!best)
            continue;

        const llvm::DWARFDebugLine::Row& row = lineTable->Rows[best->rowIndex];
        const std::uint64_t address = row.Address.Address;

        const std::string rowFile = GetRowFile(*lineTable, *unit, row);
        probePoints.push_back(sigminer::SourceReturnProbePoint{
                .sourceReturn = sourceReturn,
                .instructionAddress = address,
                .funcOffset = address - functionLow,
                .mappedLocation = sigminer::SourceLocation{
                        .file = rowFile,
                        .line = row.Line,
                        .column = row.Column,
                },
                .matchedEpilogue = row.EpilogueBegin != 0,
        });
    }

    return probePoints;
}

std::optional<std::string> InferSourceFileForSubprogram(
        llvm::DWARFContext& dwarfContext,
        const llvm::DWARFDie& subprogramDie)
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

    const char* compilationDir = unit->getCompilationDir();
    const llvm::StringRef compDir = compilationDir != nullptr ? compilationDir : "";

    if (auto declFileAttr = subprogramDie.find(llvm::dwarf::DW_AT_decl_file)) {
        auto declFile = declFileAttr->getAsUnsignedConstant();
        if (declFile) {
            std::string file{};
            if (lineTable->getFileNameByIndex(
                        *declFile,
                        compDir,
                        llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
                        file)) {
                return file;
            }
        }
    }

    const llvm::DWARFAddressRangesVector ranges = GetDieAddressRanges(subprogramDie);
    for (const llvm::DWARFDebugLine::Row& row : lineTable->Rows) {
        if (row.EndSequence)
            continue;

        if (!AddressInRanges(ranges, row.Address.Address))
            continue;

        std::string file{};
        if (lineTable->getFileNameByIndex(
                    row.File,
                    compDir,
                    llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
                    file)) {
            return file;
        }
    }

    return std::nullopt;
}
