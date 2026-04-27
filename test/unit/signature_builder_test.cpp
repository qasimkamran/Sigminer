#include <gtest/gtest.h>

#include <unordered_set>

#include <llvm/BinaryFormat/Dwarf.h>

#include "internal/dwarf_session.h"
#include "internal/signature_builder.h"
#include "test/unit/dwarf_test_utils.h"

TEST(SignatureBuilderTest, ResolvesWrappedUnsignedIntegerToBaseTypeEntry)
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

    llvm::DWARFDie paramTypeDie =
            paramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(paramTypeDie.isValid());

    const sigminer::TypeEntry entry = signature_builder::ResolveToTypeEntryFromType(paramTypeDie);

    EXPECT_EQ(entry.kind, sigminer::PrimitiveKind::INT);
    EXPECT_EQ(entry.sign, sigminer::Signedness::UNSIGNED);
    EXPECT_EQ(entry.size, 4u);
    EXPECT_FALSE(entry.isPointer);
    EXPECT_EQ(entry.name, "unsigned int");
}

TEST(SignatureBuilderTest, BuildsPointerSignatureUsingPointeeName)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "TakesPointPointer");
    ASSERT_TRUE(subprogramDie.isValid());

    const sigminer::Signature sig = signature_builder::BuildSignature(subprogramDie);

    EXPECT_EQ(sig.ret.kind, sigminer::PrimitiveKind::POINTER);
    EXPECT_TRUE(sig.ret.isPointer);
    EXPECT_EQ(sig.ret.name, "FixturePoint");
    ASSERT_EQ(sig.params.size(), 1u);
    EXPECT_EQ(sig.params[0].kind, sigminer::PrimitiveKind::POINTER);
    EXPECT_TRUE(sig.params[0].isPointer);
    EXPECT_EQ(sig.params[0].name, "FixturePoint");
}

TEST(SignatureBuilderTest, BuildsSignatureForMixedTypesFunction)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "MixedTypes");
    ASSERT_TRUE(subprogramDie.isValid());

    const sigminer::Signature sig = signature_builder::BuildSignature(subprogramDie);

    EXPECT_EQ(sig.ret.kind, sigminer::PrimitiveKind::BOOL);
    EXPECT_EQ(sig.ret.name, "bool");
    ASSERT_EQ(sig.params.size(), 4u);
    EXPECT_EQ(sig.params[0].kind, sigminer::PrimitiveKind::INT);
    EXPECT_EQ(sig.params[0].name, "int");
    EXPECT_EQ(sig.params[1].kind, sigminer::PrimitiveKind::POINTER);
    EXPECT_EQ(sig.params[1].name, "char");
    EXPECT_EQ(sig.params[2].kind, sigminer::PrimitiveKind::FLOAT);
    EXPECT_EQ(sig.params[2].name, "double");
    EXPECT_EQ(sig.params[3].kind, sigminer::PrimitiveKind::AGGREGATE);
    EXPECT_EQ(sig.params[3].name, "FixturePoint");
    EXPECT_FALSE(sig.hasVarArgs);
}

TEST(SignatureBuilderTest, MarksVariadicFunctions)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "VarArgFixture");
    ASSERT_TRUE(subprogramDie.isValid());

    const sigminer::Signature sig = signature_builder::BuildSignature(subprogramDie);

    EXPECT_TRUE(sig.hasVarArgs);
    ASSERT_EQ(sig.params.size(), 1u);
    EXPECT_EQ(sig.params[0].name, "int");
}

TEST(SignatureBuilderTest, RichResolveToTypeEntryPreservesConstWrappedIntegerMetadata)
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

    llvm::DWARFDie paramTypeDie =
            paramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    ASSERT_TRUE(paramTypeDie.isValid());

    const sigminer::rich::TypeEntry entry = signature_builder::rich::ResolveToTypeEntryFromType(paramTypeDie);

    EXPECT_EQ(entry.kind, sigminer::PrimitiveKind::INT);
    EXPECT_EQ(entry.sign, sigminer::Signedness::UNSIGNED);
    EXPECT_EQ(entry.size, 4u);
    EXPECT_EQ(entry.name, "unsigned int");
    EXPECT_TRUE(entry.isConst);
    EXPECT_FALSE(entry.isStringLike);
    EXPECT_FALSE(entry.isRecursiveReference);
    EXPECT_EQ(entry.arrayCount, 0u);
    EXPECT_EQ(entry.pointee, nullptr);
    EXPECT_EQ(entry.elementType, nullptr);
    EXPECT_TRUE(entry.members.empty());
}

TEST(SignatureBuilderTest, RichBuildTypeEntryCapturesAggregateMembers)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie typeDie = dwarf_test_utils::FindNamedDie(
            *sessionOrErr->context,
            llvm::dwarf::DW_TAG_structure_type,
            "FixturePoint");
    ASSERT_TRUE(typeDie.isValid());

    std::unordered_set<std::uint64_t> active{};
    const sigminer::rich::TypeEntry entry = signature_builder::rich::BuildTypeEntry(typeDie, active, 0);

    EXPECT_EQ(entry.kind, sigminer::PrimitiveKind::AGGREGATE);
    EXPECT_EQ(entry.name, "FixturePoint");
    ASSERT_EQ(entry.members.size(), 2u);

    EXPECT_EQ(entry.members[0].name, "x");
    EXPECT_EQ(entry.members[0].offset, 0u);
    ASSERT_NE(entry.members[0].type, nullptr);
    EXPECT_EQ(entry.members[0].type->kind, sigminer::PrimitiveKind::INT);
    EXPECT_EQ(entry.members[0].type->name, "int");

    EXPECT_EQ(entry.members[1].name, "y");
    EXPECT_EQ(entry.members[1].offset, 8u);
    ASSERT_NE(entry.members[1].type, nullptr);
    EXPECT_EQ(entry.members[1].type->kind, sigminer::PrimitiveKind::FLOAT);
    EXPECT_EQ(entry.members[1].type->name, "double");
}

TEST(SignatureBuilderTest, RichBuildSignatureKeepsParameterNamesAndRichTypes)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");
    auto sessionOrErr = dwarf_session::Open(fixturePath.string());
    ASSERT_TRUE(static_cast<bool>(sessionOrErr))
            << dwarf_test_utils::ToString(sessionOrErr.takeError());

    llvm::DWARFDie subprogramDie =
            dwarf_test_utils::GetSubprogramDie(*sessionOrErr->context, "MixedTypes");
    ASSERT_TRUE(subprogramDie.isValid());

    const sigminer::rich::Signature sig = signature_builder::rich::BuildSignature(subprogramDie);

    EXPECT_EQ(sig.ret.kind, sigminer::PrimitiveKind::BOOL);
    EXPECT_EQ(sig.ret.name, "bool");
    EXPECT_FALSE(sig.hasVarArgs);
    ASSERT_EQ(sig.params.size(), 4u);

    EXPECT_EQ(sig.params[0].name, "count");
    ASSERT_NE(sig.params[0].type, nullptr);
    EXPECT_EQ(sig.params[0].type->kind, sigminer::PrimitiveKind::INT);
    EXPECT_EQ(sig.params[0].type->name, "int");

    EXPECT_EQ(sig.params[1].name, "label");
    ASSERT_NE(sig.params[1].type, nullptr);
    EXPECT_EQ(sig.params[1].type->kind, sigminer::PrimitiveKind::POINTER);
    EXPECT_TRUE(sig.params[1].type->isStringLike);
    ASSERT_NE(sig.params[1].type->pointee, nullptr);
    EXPECT_EQ(sig.params[1].type->pointee->name, "char");

    EXPECT_EQ(sig.params[2].name, "ratio");
    ASSERT_NE(sig.params[2].type, nullptr);
    EXPECT_EQ(sig.params[2].type->kind, sigminer::PrimitiveKind::FLOAT);
    EXPECT_EQ(sig.params[2].type->name, "double");

    EXPECT_EQ(sig.params[3].name, "value");
    ASSERT_NE(sig.params[3].type, nullptr);
    EXPECT_EQ(sig.params[3].type->kind, sigminer::PrimitiveKind::AGGREGATE);
    ASSERT_EQ(sig.params[3].type->members.size(), 2u);
}
