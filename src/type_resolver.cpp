#include "internal/type_resolver.h"

namespace type_resolver {

bool IsWrapperTag(llvm::dwarf::Tag tag)
{
    switch (tag) {
        case llvm::dwarf::DW_TAG_typedef:
        case llvm::dwarf::DW_TAG_const_type:
        case llvm::dwarf::DW_TAG_volatile_type:
        case llvm::dwarf::DW_TAG_restrict_type:
        case llvm::dwarf::DW_TAG_atomic_type:
        case llvm::dwarf::DW_TAG_member:
        case llvm::dwarf::DW_TAG_subroutine_type:
            return true;
        default:
            return false;
    }
}

llvm::DWARFDie ResolveUnderlyingType(llvm::DWARFDie type, unsigned maxDepth)
{
    if (!type)
        return type;

    type = type.resolveTypeUnitReference();

    for (unsigned i = 0; i < maxDepth && type; ++i) {
        const auto tag = static_cast<llvm::dwarf::Tag>(type.getTag());
        if (!IsWrapperTag(tag))
            return type;

        llvm::DWARFDie next = type.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (!next || next == type)
            return type;

        type = next.resolveTypeUnitReference();
    }

    return type;
}

} // namespace type_resolver
