#include "internal/signature_builder.h"

#include <llvm/BinaryFormat/Dwarf.h>

#include "internal/type_classifier.h"
#include "internal/type_resolver.h"

namespace signature_builder {

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

} // namespace signature_builder
