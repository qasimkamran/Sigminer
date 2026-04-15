#include <gtest/gtest.h>

#include <llvm/BinaryFormat/Dwarf.h>

#include "internal/dwarf_session.h"
#include "internal/type_resolver.h"
#include "test/unit/dwarf_test_utils.h"

TEST(TypeResolverTest, RecognizesWrapperTags)
{
    EXPECT_TRUE(type_resolver::IsWrapperTag(llvm::dwarf::DW_TAG_typedef));
    EXPECT_TRUE(type_resolver::IsWrapperTag(llvm::dwarf::DW_TAG_const_type));
    EXPECT_TRUE(type_resolver::IsWrapperTag(llvm::dwarf::DW_TAG_volatile_type));
    EXPECT_TRUE(type_resolver::IsWrapperTag(llvm::dwarf::DW_TAG_member));
    EXPECT_TRUE(type_resolver::IsWrapperTag(llvm::dwarf::DW_TAG_subroutine_type));
    EXPECT_FALSE(type_resolver::IsWrapperTag(llvm::dwarf::DW_TAG_pointer_type));
}

TEST(TypeResolverTest, ResolvesTypedefConstVolatileChainToBaseType)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "TakesWrappedUnsignedInt");
    ASSERT_TRUE(subprogramDie.isValid());
    llvm::DWARFDie paramDie = dwarf_test_utils::GetFormalParameterDie(subprogramDie, 0);
    ASSERT_TRUE(paramDie.isValid());
    llvm::DWARFDie wrappedType = paramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(wrappedType.isValid());

    const llvm::DWARFDie resolvedType = type_resolver::ResolveUnderlyingType(wrappedType);

    ASSERT_TRUE(resolvedType.isValid());
    EXPECT_EQ(resolvedType.getTag(), llvm::dwarf::DW_TAG_base_type);
    ASSERT_NE(resolvedType.getShortName(), nullptr);
    EXPECT_STREQ(resolvedType.getShortName(), "unsigned int");
}

TEST(TypeResolverTest, LeavesPointerTypeUnwrapped)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "TakesPointPointer");
    ASSERT_TRUE(subprogramDie.isValid());
    llvm::DWARFDie paramDie = dwarf_test_utils::GetFormalParameterDie(subprogramDie, 0);
    ASSERT_TRUE(paramDie.isValid());
    llvm::DWARFDie pointerType = paramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(pointerType.isValid());

    const llvm::DWARFDie resolvedType = type_resolver::ResolveUnderlyingType(pointerType);

    ASSERT_TRUE(resolvedType.isValid());
    EXPECT_EQ(resolvedType.getTag(), llvm::dwarf::DW_TAG_pointer_type);
}

TEST(TypeResolverTest, ResolvesSubroutineTypeToReturnType)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "TakesCallback");
    ASSERT_TRUE(subprogramDie.isValid());
    llvm::DWARFDie paramDie = dwarf_test_utils::GetFormalParameterDie(subprogramDie, 0);
    ASSERT_TRUE(paramDie.isValid());
    llvm::DWARFDie paramType = paramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(paramType.isValid());

    llvm::DWARFDie pointerType = type_resolver::ResolveUnderlyingType(paramType);
    ASSERT_TRUE(pointerType.isValid());
    EXPECT_EQ(pointerType.getTag(), llvm::dwarf::DW_TAG_pointer_type);

    llvm::DWARFDie subroutineType = pointerType.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(subroutineType.isValid());
    EXPECT_EQ(subroutineType.getTag(), llvm::dwarf::DW_TAG_subroutine_type);

    const llvm::DWARFDie resolvedType = type_resolver::ResolveUnderlyingType(subroutineType);

    ASSERT_TRUE(resolvedType.isValid());
    EXPECT_EQ(resolvedType.getTag(), llvm::dwarf::DW_TAG_base_type);
    ASSERT_NE(resolvedType.getShortName(), nullptr);
    EXPECT_STREQ(resolvedType.getShortName(), "int");
}
