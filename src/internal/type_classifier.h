#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <llvm-14/llvm/BinaryFormat/Dwarf.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>

#include "sigminer/signature.h"

namespace type_classifier {

sigminer::TypeEntry GetTypeEntryFromDwarfAttr( const llvm::DWARFDie& Die );

static bool IsTypeWrapperTag( const llvm::dwarf::Tag& T )
{
    switch( T )
    {
        case llvm::dwarf::DW_TAG_typedef:
        case llvm::dwarf::DW_TAG_const_type:
        case llvm::dwarf::DW_TAG_volatile_type:
        case llvm::dwarf::DW_TAG_restrict_type:
        case llvm::dwarf::DW_TAG_atomic_type:
        case llvm::dwarf::DW_TAG_shared_type:
            return true;
        default:
            return false;
    }
}

struct Key
{
    const llvm::DWARFUnit *Unit;
    std::uint64_t Offset;
    bool operator==( const Key& K ) const { return Unit == K.Unit && Offset == K.Offset; }
};

struct KeyHash
{
    size_t operator()( const Key& K ) const
    {
        return std::hash<const void*>{}( K.Unit ) ^ ( std::hash<uint64_t>{}( K.Offset ) << 1 );
    }
};

static std::optional<uint64_t> GetUConst( llvm::DWARFDie Die, llvm::dwarf::Attribute Attr )
{
  if( auto V = Die.find( Attr ) )
  {
    if( auto C = V->getAsUnsignedConstant() )
      return *C;
  }
  return std::nullopt;
}

llvm::DWARFDie StripTypeWrappers( llvm::DWARFDie Start );
sigminer::TypeEntry MatchBaseTypeToTypeEntry( llvm::DWARFDie BaseTypeDie );

} // namespace type_classifier

