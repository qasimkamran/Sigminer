#include <gtest/gtest.h>

#include "internal/dwarf_session.h"
#include "internal/subprogram_finder.h"
#include "test/unit/dwarf_test_utils.h"

TEST(SubprogramFinderTest, FindsTopLevelSubprogramByExactName)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            subprogram_finder::GetTargetSubprogram(*sessionOrErr->context, "TakesWrappedUnsignedInt");

    ASSERT_TRUE(subprogramDie.isValid());
    EXPECT_EQ(subprogramDie.getTag(), llvm::dwarf::DW_TAG_subprogram);
    ASSERT_NE(subprogramDie.getShortName(), nullptr);
    EXPECT_STREQ(subprogramDie.getShortName(), "TakesWrappedUnsignedInt");
}

TEST(SubprogramFinderTest, FindsNestedSubprogramBelowNamespaceDie)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            subprogram_finder::GetTargetSubprogram(*sessionOrErr->context, "NestedFunction");

    ASSERT_TRUE(subprogramDie.isValid());
    ASSERT_NE(subprogramDie.getShortName(), nullptr);
    EXPECT_STREQ(subprogramDie.getShortName(), "NestedFunction");
}

TEST(SubprogramFinderTest, ReturnsInvalidDieWhenFunctionIsAbsent)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            subprogram_finder::GetTargetSubprogram(*sessionOrErr->context, "DoesNotExist");

    EXPECT_FALSE(subprogramDie.isValid());
}
