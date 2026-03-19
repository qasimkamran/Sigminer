#pragma once

#include <optional>
#include <llvm/ADT/StringRef.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>

#include "sigminer/signature.h"

namespace type_classifier {

std::optional<std::uint64_t> GetUnsignedAttr(llvm::DWARFDie die, llvm::dwarf::Attribute attr);
sigminer::PrimitiveKind TagToKind(llvm::dwarf::Tag tag);
sigminer::Signedness GetSignednessFromTypeDie(llvm::DWARFDie typeDie);
std::size_t GetSizeFromTypeDie(llvm::DWARFDie typeDie);
std::string DieNameOrFallback(llvm::DWARFDie die);
sigminer::TypeEntry TypeDieToTypeEntry(llvm::DWARFDie typeDie);

} // namespace type_classifier
