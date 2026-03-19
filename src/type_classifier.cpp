#include "internal/type_classifier.h"
#include <cstdint>
#include <string>

#include <llvm/DebugInfo/DWARF/DWARFUnit.h>

namespace type_classifier {

std::optional<std::uint64_t> GetUnsignedAttr(llvm::DWARFDie die, llvm::dwarf::Attribute attr)
{
    if (auto value = die.find(attr)) {
        if (auto constant = value->getAsUnsignedConstant())
            return *constant;
    }
    return std::nullopt;
}

sigminer::PrimitiveKind TagToKind(llvm::dwarf::Tag tag)
{
    switch (tag) {
        case llvm::dwarf::DW_TAG_base_type:
            return sigminer::PrimitiveKind::INT;
        case llvm::dwarf::DW_TAG_pointer_type:
        case llvm::dwarf::DW_TAG_subroutine_type:
            return sigminer::PrimitiveKind::POINTER;
        case llvm::dwarf::DW_TAG_enumeration_type:
            return sigminer::PrimitiveKind::ENUM;
        case llvm::dwarf::DW_TAG_structure_type:
        case llvm::dwarf::DW_TAG_class_type:
        case llvm::dwarf::DW_TAG_union_type:
        case llvm::dwarf::DW_TAG_array_type:
            return sigminer::PrimitiveKind::AGGREGATE;
        default:
            return sigminer::PrimitiveKind::UNKNOWN;
    }
}

sigminer::Signedness GetSignednessFromTypeDie(llvm::DWARFDie typeDie)
{
    if (!typeDie.isValid())
        return sigminer::Signedness::UNKNOWN;

    const auto tag = static_cast<llvm::dwarf::Tag>(typeDie.getTag());
    if (tag == llvm::dwarf::DW_TAG_base_type) {
        const auto encoding = GetUnsignedAttr(typeDie, llvm::dwarf::DW_AT_encoding);
        if (!encoding)
            return sigminer::Signedness::UNKNOWN;

        switch (*encoding) {
            case llvm::dwarf::DW_ATE_signed:
            case llvm::dwarf::DW_ATE_signed_char:
                return sigminer::Signedness::SIGNED;
            case llvm::dwarf::DW_ATE_unsigned:
            case llvm::dwarf::DW_ATE_unsigned_char:
            case llvm::dwarf::DW_ATE_boolean:
            case llvm::dwarf::DW_ATE_UTF:
                return sigminer::Signedness::UNSIGNED;
            default:
                return sigminer::Signedness::UNKNOWN;
        }
    }

    if (tag == llvm::dwarf::DW_TAG_enumeration_type) {
        const llvm::DWARFDie underlying =
                typeDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (underlying.isValid())
            return GetSignednessFromTypeDie(underlying.resolveTypeUnitReference());
    }

    return sigminer::Signedness::UNKNOWN;
}

std::size_t GetSizeFromTypeDie(llvm::DWARFDie typeDie)
{
    if (!typeDie.isValid())
        return 0;

    if (const auto byteSize = GetUnsignedAttr(typeDie, llvm::dwarf::DW_AT_byte_size))
        return static_cast<std::size_t>(*byteSize);

    const auto tag = static_cast<llvm::dwarf::Tag>(typeDie.getTag());
    if (tag == llvm::dwarf::DW_TAG_pointer_type || tag == llvm::dwarf::DW_TAG_subroutine_type) {
        if (const llvm::DWARFUnit* unit = typeDie.getDwarfUnit())
            return unit->getAddressByteSize();
    }

    if (tag == llvm::dwarf::DW_TAG_enumeration_type) {
        const llvm::DWARFDie underlying =
                typeDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (underlying.isValid())
            return GetSizeFromTypeDie(underlying.resolveTypeUnitReference());
    }

    return 0;
}

std::string DieNameOrFallback(llvm::DWARFDie die)
{
    if (const char* shortName = die.getShortName())
        return shortName;
    return std::string("<unnamed@0x") + std::to_string(die.getOffset()) + ">";
}

sigminer::TypeEntry TypeDieToTypeEntry(llvm::DWARFDie typeDie)
{
    sigminer::TypeEntry entry{};
    if (!typeDie.isValid())
        return entry;

    const auto tag = static_cast<llvm::dwarf::Tag>(typeDie.getTag());
    entry.Kind = TagToKind(tag);
    entry.Sign = GetSignednessFromTypeDie(typeDie);
    entry.Size = GetSizeFromTypeDie(typeDie);
    entry.IsPointer =
            tag == llvm::dwarf::DW_TAG_pointer_type || tag == llvm::dwarf::DW_TAG_subroutine_type;

    if (tag == llvm::dwarf::DW_TAG_base_type) {
        const auto encoding = GetUnsignedAttr(typeDie, llvm::dwarf::DW_AT_encoding);
        if (encoding && *encoding == llvm::dwarf::DW_ATE_boolean) {
            entry.Kind = sigminer::PrimitiveKind::BOOL;
        } else if (encoding && *encoding == llvm::dwarf::DW_ATE_float) {
            entry.Kind = sigminer::PrimitiveKind::FLOAT;
        }
    }

    return entry;
}

} // namespace type_classifier
