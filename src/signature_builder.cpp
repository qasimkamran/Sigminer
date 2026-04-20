#include "internal/signature_builder.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>

#include <llvm/BinaryFormat/Dwarf.h>

#include "internal/type_classifier.h"
#include "internal/type_resolver.h"

namespace signature_builder {
namespace {

constexpr unsigned kMaxRichTypeDepth = 8;

std::optional<std::uint64_t> GetMemberOffset(llvm::DWARFDie memberDie)
{
    if (auto value = memberDie.find(llvm::dwarf::DW_AT_data_member_location)) {
        if (auto constant = value->getAsUnsignedConstant())
            return *constant;
    }

    return std::nullopt;
}

std::size_t GetArrayCountFromTypeDie(llvm::DWARFDie typeDie)
{
    if (!typeDie.isValid())
        return 0;

    std::size_t totalCount = 1;
    bool sawSubrange = false;

    for (llvm::DWARFDie child : typeDie.children()) {
        if (!child.isValid() || child.getTag() != llvm::dwarf::DW_TAG_subrange_type)
            continue;

        sawSubrange = true;

        if (auto count = type_classifier::GetUnsignedAttr(child, llvm::dwarf::DW_AT_count)) {
            totalCount *= static_cast<std::size_t>(*count);
            continue;
        }

        if (auto upper = type_classifier::GetUnsignedAttr(child, llvm::dwarf::DW_AT_upper_bound)) {
            totalCount *= static_cast<std::size_t>(*upper + 1);
            continue;
        }

        return 0;
    }

    return sawSubrange ? totalCount : 0;
}

bool IsStringLikeCharType(const sigminer::RichTypeEntry& typeEntry)
{
    if (typeEntry.kind != sigminer::PrimitiveKind::INT || typeEntry.size != 1)
        return false;

    const std::string& name = typeEntry.name;
    return name == "char" || name == "signed char" || name == "unsigned char";
}

sigminer::RichTypeEntry BuildRichTypeEntry(
        llvm::DWARFDie type,
        std::unordered_set<std::uint64_t>& active,
        unsigned depth)
{
    sigminer::RichTypeEntry entry{};

    if (!type.isValid())
        return entry;

    llvm::DWARFDie resolvedType = type_resolver::ResolveUnderlyingType(type);
    if (!resolvedType.isValid())
        return entry;

    const auto tag = static_cast<llvm::dwarf::Tag>(resolvedType.getTag());
    const std::uint64_t offset = resolvedType.getOffset();

    entry.kind = type_classifier::TagToKind(tag);
    entry.sign = type_classifier::GetSignednessFromTypeDie(resolvedType);
    entry.size = type_classifier::GetSizeFromTypeDie(resolvedType);
    entry.name = type_classifier::DieNameOrFallback(resolvedType);

    for (llvm::DWARFDie wrapper = type; wrapper.isValid();) {
        const auto wrapperTag = static_cast<llvm::dwarf::Tag>(wrapper.getTag());
        if (wrapperTag == llvm::dwarf::DW_TAG_const_type)
            entry.isConst = true;

        if (!type_resolver::IsWrapperTag(wrapperTag))
            break;

        llvm::DWARFDie next = wrapper.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (!next.isValid() || next == wrapper)
            break;

        wrapper = next.resolveTypeUnitReference();
    }

    if (depth >= kMaxRichTypeDepth || active.contains(offset)) {
        entry.isRecursiveReference = true;
        return entry;
    }

    active.insert(offset);

    if (tag == llvm::dwarf::DW_TAG_pointer_type) {
        llvm::DWARFDie pointee = resolvedType.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (pointee.isValid()) {
            entry.pointee = std::make_unique<sigminer::RichTypeEntry>(
                    BuildRichTypeEntry(pointee, active, depth + 1));
            entry.isStringLike = IsStringLikeCharType(*entry.pointee);
        }
    } else if (tag == llvm::dwarf::DW_TAG_array_type) {
        entry.arrayCount = GetArrayCountFromTypeDie(resolvedType);
        llvm::DWARFDie elementType = resolvedType.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (elementType.isValid()) {
            entry.elementType = std::make_unique<sigminer::RichTypeEntry>(
                    BuildRichTypeEntry(elementType, active, depth + 1));
            entry.isStringLike = IsStringLikeCharType(*entry.elementType);
        }
    } else if (tag == llvm::dwarf::DW_TAG_structure_type ||
               tag == llvm::dwarf::DW_TAG_class_type ||
               tag == llvm::dwarf::DW_TAG_union_type) {
        for (llvm::DWARFDie child : resolvedType.children()) {
            if (!child.isValid() || child.getTag() != llvm::dwarf::DW_TAG_member)
                continue;

            llvm::DWARFDie memberType = child.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
            if (!memberType.isValid())
                continue;

            sigminer::RichTypeMember member{};
            member.name = type_classifier::DieNameOrFallback(child);
            member.offset = GetMemberOffset(child).value_or(0);
            member.type = std::make_unique<sigminer::RichTypeEntry>(
                    BuildRichTypeEntry(memberType, active, depth + 1));
            entry.members.push_back(std::move(member));
        }
    }

    active.erase(offset);
    return entry;
}

} // namespace

sigminer::TypeEntry ResolveToTypeEntryFromType(llvm::DWARFDie type)
{
    if (!type.isValid())
        return {};

    llvm::DWARFDie resolvedType = type_resolver::ResolveUnderlyingType(type);
    sigminer::TypeEntry typeEntry = type_classifier::TypeDieToTypeEntry(resolvedType);

    const auto tag = static_cast<llvm::dwarf::Tag>(resolvedType.getTag());
    std::string typeName = type_classifier::DieNameOrFallback(resolvedType);

    if (tag == llvm::dwarf::DW_TAG_pointer_type) {
        llvm::DWARFDie pointee = resolvedType.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (pointee.isValid()) {
            llvm::DWARFDie basePointee = type_resolver::ResolveUnderlyingType(pointee);
            typeName = type_classifier::DieNameOrFallback(basePointee);
        } else {
            typeName = "unknown";
        }
    }

    typeEntry.name = std::move(typeName);
    return typeEntry;
}

sigminer::Signature BuildSignature(llvm::DWARFDie subprogramDie)
{
    sigminer::Signature signature{};

    llvm::DWARFDie returnType = subprogramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    signature.ret = ResolveToTypeEntryFromType(returnType);

    for (llvm::DWARFDie child : subprogramDie.children()) {
        if (!child.isValid())
            continue;

        const auto tag = static_cast<llvm::dwarf::Tag>(child.getTag());
        if (tag == llvm::dwarf::DW_TAG_formal_parameter) {
            llvm::DWARFDie paramType = child.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
            signature.params.push_back(ResolveToTypeEntryFromType(paramType));
        } else if (tag == llvm::dwarf::DW_TAG_unspecified_parameters) {
            signature.hasVarArgs = true;
        }
    }

    return signature;
}

sigminer::RichTypeEntry ResolveToRichTypeEntryFromType(llvm::DWARFDie type)
{
    std::unordered_set<std::uint64_t> active{};
    return BuildRichTypeEntry(type, active, 0);
}

sigminer::RichSignature BuildRichSignature(llvm::DWARFDie subprogramDie)
{
    sigminer::RichSignature signature{};

    llvm::DWARFDie returnType = subprogramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    signature.ret = ResolveToRichTypeEntryFromType(returnType);

    for (llvm::DWARFDie child : subprogramDie.children()) {
        if (!child.isValid())
            continue;

        const auto tag = static_cast<llvm::dwarf::Tag>(child.getTag());
        if (tag == llvm::dwarf::DW_TAG_formal_parameter) {
            llvm::DWARFDie paramType = child.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);

            sigminer::RichParameter param{};
            if (const char* name = child.getShortName())
                param.name = name;
            else
                param.name = "arg" + std::to_string(signature.params.size());
            param.type = ResolveToRichTypeEntryFromType(paramType);
            signature.params.push_back(std::move(param));
        } else if (tag == llvm::dwarf::DW_TAG_unspecified_parameters) {
            signature.hasVarArgs = true;
        }
    }

    return signature;
}

} // namespace signature_builder
