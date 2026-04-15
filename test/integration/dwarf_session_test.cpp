#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <llvm/Support/raw_ostream.h>

#include "internal/dwarf_session.h"

namespace {

std::filesystem::path FindBuiltMockLibrary()
{
    const std::filesystem::path exePath = std::filesystem::read_symlink("/proc/self/exe");
    return exePath.parent_path() / "libmock_lib.so";
}

std::string ToString(llvm::Error error)
{
    std::string message;
    llvm::raw_string_ostream stream(message);
    stream << error;
    return message;
}

} // namespace

TEST(DwarfSessionTest, OpensSharedObjectAndBuildsDwarfContext)
{
    const std::filesystem::path mockLibraryPath = FindBuiltMockLibrary();
    ASSERT_TRUE(std::filesystem::exists(mockLibraryPath))
        << "expected test fixture library at " << mockLibraryPath.string();

    llvm::Expected<dwarf_session::Session> sessionOrErr =
            dwarf_session::Open(mockLibraryPath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << "Open failed for " << mockLibraryPath.string() << ": "
            << ToString(sessionOrErr.takeError());

    dwarf_session::Session& session = *sessionOrErr;
    EXPECT_NE(session.object, nullptr);
    ASSERT_NE(session.context, nullptr);

    EXPECT_FALSE(session.context->compile_units().empty());
}

TEST(DwarfSessionTest, RejectsNonObjectInput)
{
    llvm::Expected<dwarf_session::Session> sessionOrErr =
            dwarf_session::Open("/home/ubuntu/Projects/Sigminer/CMakeLists.txt");

    ASSERT_FALSE(static_cast<bool>(sessionOrErr));
    EXPECT_FALSE(ToString(sessionOrErr.takeError()).empty());
}
