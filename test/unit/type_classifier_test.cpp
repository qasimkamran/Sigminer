#include <gtest/gtest.h>

#include <llvm/BinaryFormat/Dwarf.h>

#include "internal/dwarf_session.h"
#include "internal/type_classifier.h"
#include "test/unit/dwarf_test_utils.h"

TEST(TypeClassifierTest, ClassifiesResolvedWrappedUnsignedInteger)
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
    llvm::DWARFDie typeDie = paramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(typeDie.isValid());
    typeDie = typeDie.resolveTypeUnitReference();

    EXPECT_EQ(type_classifier::GetSignednessFromTypeDie(typeDie), sigminer::Signedness::UNKNOWN);

    llvm::DWARFDie resolvedType = typeDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type)
                                          .getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type)
                                          .getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(resolvedType.isValid());

    EXPECT_EQ(type_classifier::GetSignednessFromTypeDie(resolvedType), sigminer::Signedness::UNSIGNED);
    EXPECT_EQ(type_classifier::GetSizeFromTypeDie(resolvedType), 4u);

    const sigminer::TypeEntry entry = type_classifier::TypeDieToTypeEntry(resolvedType);
    EXPECT_EQ(entry.kind, sigminer::PrimitiveKind::INT);
    EXPECT_EQ(entry.sign, sigminer::Signedness::UNSIGNED);
    EXPECT_EQ(entry.size, 4u);
    EXPECT_FALSE(entry.isPointer);
}

TEST(TypeClassifierTest, ClassifiesPointerTypeEntries)
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
    llvm::DWARFDie typeDie = paramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(typeDie.isValid());

    EXPECT_EQ(type_classifier::TagToKind(static_cast<llvm::dwarf::Tag>(typeDie.getTag())),
              sigminer::PrimitiveKind::POINTER);
    EXPECT_EQ(type_classifier::GetSizeFromTypeDie(typeDie), 8u);

    const sigminer::TypeEntry entry = type_classifier::TypeDieToTypeEntry(typeDie);
    EXPECT_EQ(entry.kind, sigminer::PrimitiveKind::POINTER);
    EXPECT_TRUE(entry.isPointer);
}

TEST(TypeClassifierTest, ClassifiesEnumAndBoolTypes)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie enumSubprogram =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "TakesEnum");
    ASSERT_TRUE(enumSubprogram.isValid());
    llvm::DWARFDie enumParam = dwarf_test_utils::GetFormalParameterDie(enumSubprogram, 0);
    ASSERT_TRUE(enumParam.isValid());
    llvm::DWARFDie enumType = enumParam.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(enumType.isValid());

    const sigminer::TypeEntry enumEntry = type_classifier::TypeDieToTypeEntry(enumType);
    EXPECT_EQ(enumEntry.kind, sigminer::PrimitiveKind::ENUM);
    EXPECT_EQ(enumEntry.sign, sigminer::Signedness::UNSIGNED);
    EXPECT_EQ(enumEntry.size, 4u);

    llvm::DWARFDie mixedTypesSubprogram =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "MixedTypes");
    ASSERT_TRUE(mixedTypesSubprogram.isValid());
    llvm::DWARFDie boolType =
            mixedTypesSubprogram.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(boolType.isValid());

    const sigminer::TypeEntry boolEntry = type_classifier::TypeDieToTypeEntry(boolType);
    EXPECT_EQ(boolEntry.kind, sigminer::PrimitiveKind::BOOL);
    EXPECT_EQ(boolEntry.sign, sigminer::Signedness::UNSIGNED);
    EXPECT_EQ(boolEntry.size, 1u);
}

TEST(TypeClassifierTest, UsesDieNameForAggregateTypes)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "MixedTypes");
    ASSERT_TRUE(subprogramDie.isValid());
    llvm::DWARFDie paramDie = dwarf_test_utils::GetFormalParameterDie(subprogramDie, 3);
    ASSERT_TRUE(paramDie.isValid());
    llvm::DWARFDie typeDie = paramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(typeDie.isValid());

    EXPECT_EQ(type_classifier::DieNameOrFallback(typeDie), "FixturePoint");
    EXPECT_EQ(type_classifier::TagToKind(static_cast<llvm::dwarf::Tag>(typeDie.getTag())),
              sigminer::PrimitiveKind::AGGREGATE);
}
