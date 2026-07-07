#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "internal/dwarf_session.h"
#include "internal/dwarf_ranges.h"
#include "internal/line_table_lookup.h"
#include "internal/return_site_finder.h"
#include "test/unit/dwarf_test_utils.h"

namespace {

struct ReturnSiteFixture
{
    dwarf_session::Session session;
    llvm::DWARFDie subprogram;

    explicit ReturnSiteFixture(dwarf_session::Session&& openedSession)
        : session(std::move(openedSession)),
          subprogram(dwarf_test_utils::GetSubprogramDie(*session.context, "ReturnSiteSingle"))
    {
    }
};

ReturnSiteFixture OpenReturnSiteFixture()
{
    const std::filesystem::path fixturePath =
            dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    llvm::Expected<dwarf_session::Session> sessionOrErr =
            dwarf_session::Open(fixturePath.string());
    if (!sessionOrErr) {
        ADD_FAILURE() << "failed to open return-site fixture: "
                      << dwarf_test_utils::ToString(sessionOrErr.takeError());
        throw std::runtime_error("failed to open return-site fixture");
    }

    return ReturnSiteFixture(std::move(*sessionOrErr));
}

} // namespace

TEST(ReturnSiteFinderTest, FindsMachineReturnWithinFunctionAddressRange)
{
    ReturnSiteFixture fixture = OpenReturnSiteFixture();
    ASSERT_TRUE(fixture.subprogram.isValid());

    llvm::Expected<llvm::DWARFAddressRangesVector> rangesOrErr =
            dwarf_ranges::GetDieAddressRanges(fixture.subprogram);
    ASSERT_TRUE(static_cast<bool>(rangesOrErr))
            << dwarf_test_utils::ToString(rangesOrErr.takeError());
    const llvm::DWARFAddressRangesVector& ranges = *rangesOrErr;
    ASSERT_FALSE(ranges.empty());

    const std::vector<sigminer::ReturnSite> returnSites =
            return_site_finder::GetReturnSitesWithinSubprogramDie(
                    fixture.subprogram,
                    *fixture.session.object);
    ASSERT_EQ(returnSites.size(), 1u);

    const sigminer::ReturnSite& returnSite = returnSites.front();
    const std::uint64_t functionLow = ranges.front().LowPC;
    EXPECT_EQ(returnSite.funcOffset, returnSite.instructionAddress - functionLow);

    bool addressIsInFunction = false;
    for (const llvm::DWARFAddressRange& range : ranges) {
        addressIsInFunction = addressIsInFunction ||
                (range.LowPC <= returnSite.instructionAddress &&
                 returnSite.instructionAddress < range.HighPC);
    }
    EXPECT_TRUE(addressIsInFunction);
}

TEST(ReturnSiteFinderTest, MapsMachineReturnToStableSourceLocation)
{
    ReturnSiteFixture fixture = OpenReturnSiteFixture();
    ASSERT_TRUE(fixture.subprogram.isValid());

    std::vector<sigminer::ReturnSite> returnSites =
            return_site_finder::GetReturnSitesWithinSubprogramDie(
                    fixture.subprogram,
                    *fixture.session.object);
    ASSERT_EQ(returnSites.size(), 1u);

    llvm::Error populateError = line_table_lookup::PopulateSourceLocationForReturnSite(
            *fixture.session.context,
            fixture.subprogram,
            returnSites.front());
    ASSERT_FALSE(static_cast<bool>(populateError))
            << dwarf_test_utils::ToString(std::move(populateError));
    ASSERT_TRUE(returnSites.front().sourceLocation.has_value());

    const sigminer::SourceLocation& location = *returnSites.front().sourceLocation;
    EXPECT_EQ(std::filesystem::path(location.file).filename(), "dwarf_types_fixture.cpp");
    EXPECT_EQ(location.line, 204u);
}

TEST(ReturnSiteFinderTest, RejectsInvalidSubprogram)
{
    ReturnSiteFixture fixture = OpenReturnSiteFixture();
    const llvm::DWARFDie invalidDie{};

    llvm::Expected<llvm::DWARFAddressRangesVector> rangesOrErr =
            dwarf_ranges::GetDieAddressRanges(invalidDie);
    EXPECT_FALSE(static_cast<bool>(rangesOrErr));
    llvm::consumeError(rangesOrErr.takeError());
    EXPECT_TRUE(return_site_finder::GetReturnSitesWithinSubprogramDie(
            invalidDie,
            *fixture.session.object).empty());

    llvm::Expected<sigminer::SourceLocation> locationOrErr =
            line_table_lookup::GetSourceLocationForAddress(
                    *fixture.session.context,
                    invalidDie,
                    0);
    EXPECT_FALSE(static_cast<bool>(locationOrErr));
    llvm::consumeError(locationOrErr.takeError());
}
