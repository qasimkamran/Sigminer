#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include <llvm/Support/Error.h>

#include "internal/source_return_extractor.h"
#include "test/unit/dwarf_test_utils.h"

namespace {

std::filesystem::path SourceFixturePath()
{
    return std::filesystem::weakly_canonical(
            std::filesystem::path(__FILE__).parent_path().parent_path() /
            "fixture/dwarf_types_fixture.cpp");
}

std::filesystem::path CompilationDatabaseDirectory()
{
    return dwarf_test_utils::BuiltArtifactPath("compile_commands.json").parent_path();
}

} // namespace

TEST(SourceReturnExtractorTest, ExtractsReturnFromQualifiedFunction)
{
    llvm::Expected<std::vector<sigminer::SourceReturnCandidate>> candidatesOrErr =
            source_return_extractor::ExtractSourceReturnCandidates(
                    CompilationDatabaseDirectory().string(),
                    SourceFixturePath().string(),
                    "fixture_ns::NestedFunction");
    ASSERT_TRUE(static_cast<bool>(candidatesOrErr))
            << dwarf_test_utils::ToString(candidatesOrErr.takeError());
    ASSERT_EQ(candidatesOrErr->size(), 1u);

    const sigminer::SourceReturnCandidate& candidate = candidatesOrErr->front();
    EXPECT_EQ(candidate.functionName, "fixture_ns::NestedFunction");
    EXPECT_EQ(std::filesystem::path(candidate.spellingLocation.file).filename(),
              "dwarf_types_fixture.cpp");
    EXPECT_EQ(candidate.spellingLocation.line, 24u);
    EXPECT_EQ(candidate.spellingLocation.column, 5u);
    EXPECT_EQ(candidate.spellingLocation.file, candidate.expansionLocation.file);
    EXPECT_EQ(candidate.spellingLocation.line, candidate.expansionLocation.line);
    EXPECT_EQ(candidate.spellingLocation.column, candidate.expansionLocation.column);
}

TEST(SourceReturnExtractorTest, RejectsUnqualifiedFunctionName)
{
    llvm::Expected<std::vector<sigminer::SourceReturnCandidate>> candidatesOrErr =
            source_return_extractor::ExtractSourceReturnCandidates(
                    CompilationDatabaseDirectory().string(),
                    SourceFixturePath().string(),
                    "NestedFunction");

    ASSERT_FALSE(static_cast<bool>(candidatesOrErr));
    EXPECT_NE(dwarf_test_utils::ToString(candidatesOrErr.takeError()).find("not found"),
              std::string::npos);
}

TEST(SourceReturnExtractorTest, RejectsAmbiguousOverloadSet)
{
    llvm::Expected<std::vector<sigminer::SourceReturnCandidate>> candidatesOrErr =
            source_return_extractor::ExtractSourceReturnCandidates(
                    CompilationDatabaseDirectory().string(),
                    SourceFixturePath().string(),
                    "fixture_overload::OverloadedReturn");

    ASSERT_FALSE(static_cast<bool>(candidatesOrErr));
    EXPECT_NE(dwarf_test_utils::ToString(candidatesOrErr.takeError()).find("ambiguous"),
              std::string::npos);
}

TEST(SourceReturnExtractorTest, ReportsInvalidInputAsError)
{
    llvm::Expected<std::vector<sigminer::SourceReturnCandidate>> candidatesOrErr =
            source_return_extractor::ExtractSourceReturnCandidates("", "", "");

    ASSERT_FALSE(static_cast<bool>(candidatesOrErr));
    EXPECT_FALSE(dwarf_test_utils::ToString(candidatesOrErr.takeError()).empty());
}
