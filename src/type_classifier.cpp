#include "internal/type_classifier.h"
#include "sigminer/signature.h"
#include <llvm-14/llvm/BinaryFormat/Dwarf.h>
#include <llvm-14/llvm/DebugInfo/DWARF/DWARFDie.h>

llvm::DWARFDie StripTypeWrappers( llvm::DWARFDie Start )
{
    if( !Start.isValid() )
        return {};

    std::unordered_set<type_classifier::Key, type_classifier::KeyHash> Seen;

    llvm::DWARFDie Current = Start;

    while( Current.isValid() && type_classifier::IsTypeWrapperTag( Current.getTag() ) )
    {
        type_classifier::Key K{ Current.getDwarfUnit(), Current.getOffset() };

        if( !Seen.insert( K ).second )
            return Current;

        llvm::DWARFDie Next = Current.getAttributeValueAsReferencedDie( llvm::dwarf::DW_AT_type );

        if( !Next.isValid() )
            return Current;

        Current = Next;
    }
    return Current;
}

sigminer::TypeEntry MatchBaseTypeToTypeEntry( llvm::DWARFDie BaseTypeDie )
{
    if( !BaseTypeDie.isValid() || BaseTypeDie.getTag() != llvm::dwarf::DW_TAG_base_type )
        return {};

    sigminer::TypeEntry Entry = {};
    
    return Entry;
}

