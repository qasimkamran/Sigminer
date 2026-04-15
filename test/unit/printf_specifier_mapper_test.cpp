#include <gtest/gtest.h>

#include "internal/printf_specifier_mapper.h"
#include "sigminer/signature.h"

TEST(PrintfSpecifierMapperTest, MapsPointerTypeEntry)
{
    sigminer::TypeEntry voidPointerTypeEntry;
    voidPointerTypeEntry.isPointer = true;
    voidPointerTypeEntry.kind = sigminer::PrimitiveKind::POINTER;
    voidPointerTypeEntry.name = "void *";
    voidPointerTypeEntry.sign = sigminer::Signedness::UNKNOWN;
    voidPointerTypeEntry.size = 8;

    const std::string voidPointerSpecifier =
            printf_specifier_mapper::TypeEntryToPrintfSpecifier(voidPointerTypeEntry);

    EXPECT_EQ(voidPointerSpecifier, "(void *) %p");

    sigminer::TypeEntry intPointerTypeEntry;
    intPointerTypeEntry.isPointer = true;
    intPointerTypeEntry.kind = sigminer::PrimitiveKind::POINTER;
    intPointerTypeEntry.name = "int *";
    intPointerTypeEntry.sign = sigminer::Signedness::UNKNOWN;
    intPointerTypeEntry.size = 8;

    const std::string intPointerSpecifier =
            printf_specifier_mapper::TypeEntryToPrintfSpecifier(intPointerTypeEntry);

    EXPECT_EQ(intPointerSpecifier, "(int *) %p");

    EXPECT_NE(voidPointerSpecifier, intPointerSpecifier);
}

TEST(PrintfSpecifierMapperTest, MapsSignedEightByteIntegerTypeEntry)
{
    sigminer::TypeEntry typeEntry;
    typeEntry.isPointer = false;
    typeEntry.kind = sigminer::PrimitiveKind::INT;
    typeEntry.name = "long";
    typeEntry.sign = sigminer::Signedness::SIGNED;
    typeEntry.size = 8;

    EXPECT_EQ(printf_specifier_mapper::TypeEntryToPrintfSpecifier(typeEntry), "(long) %ld");
}

TEST(PrintfSpecifierMapperTest, MapsUnsignedNonEightByteIntegerTypeEntry)
{
    sigminer::TypeEntry typeEntry;
    typeEntry.isPointer = false;
    typeEntry.kind = sigminer::PrimitiveKind::INT;
    typeEntry.name = "unsigned int";
    typeEntry.sign = sigminer::Signedness::UNSIGNED;
    typeEntry.size = 4;

    EXPECT_EQ(
            printf_specifier_mapper::TypeEntryToPrintfSpecifier(typeEntry),
            "(unsigned int) %u");
}

TEST(PrintfSpecifierMapperTest, MapsFloatingPointTypeEntry)
{
    sigminer::TypeEntry floatTypeEntry;
    floatTypeEntry.isPointer = false;
    floatTypeEntry.kind = sigminer::PrimitiveKind::FLOAT;
    floatTypeEntry.name = "float";
    floatTypeEntry.sign = sigminer::Signedness::UNKNOWN;
    floatTypeEntry.size = 4;

    EXPECT_EQ(printf_specifier_mapper::TypeEntryToPrintfSpecifier(floatTypeEntry), "(float) %x");

    sigminer::TypeEntry doubleTypeEntry;
    doubleTypeEntry.isPointer = false;
    doubleTypeEntry.kind = sigminer::PrimitiveKind::FLOAT;
    doubleTypeEntry.name = "double";
    doubleTypeEntry.sign = sigminer::Signedness::UNKNOWN;
    doubleTypeEntry.size = 8;

    EXPECT_EQ(printf_specifier_mapper::TypeEntryToPrintfSpecifier(doubleTypeEntry), "(double) %lx");
}

TEST(PrintfSpecifierMapperTest, JoinsMultipleTypeEntriesWithSpaces)
{
    sigminer::TypeEntry boolTypeEntry;
    boolTypeEntry.isPointer = false;
    boolTypeEntry.kind = sigminer::PrimitiveKind::BOOL;
    boolTypeEntry.name = "bool";
    boolTypeEntry.sign = sigminer::Signedness::UNKNOWN;
    boolTypeEntry.size = 1;

    sigminer::TypeEntry enumTypeEntry;
    enumTypeEntry.isPointer = false;
    enumTypeEntry.kind = sigminer::PrimitiveKind::ENUM;
    enumTypeEntry.name = "Color";
    enumTypeEntry.sign = sigminer::Signedness::UNSIGNED;
    enumTypeEntry.size = 4;

    const std::vector<sigminer::TypeEntry> typeEntries{boolTypeEntry, enumTypeEntry};

    EXPECT_EQ(
            printf_specifier_mapper::TypeEntriesToPrintfSpecifier(typeEntries),
            "(bool) %d (Color) %u");
}

TEST(PrintfSpecifierMapperTest, ReturnsEmptyStringForEmptyTypeEntryList)
{
    const std::vector<sigminer::TypeEntry> typeEntries;

    EXPECT_TRUE(printf_specifier_mapper::TypeEntriesToPrintfSpecifier(typeEntries).empty());
}
