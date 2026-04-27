#pragma once

#include <cstdint>
#include <unordered_set>

#include <llvm/DebugInfo/DWARF/DWARFDie.h>

#include "sigminer/signature.h"

namespace signature_builder {

sigminer::TypeEntry ResolveToTypeEntryFromType(llvm::DWARFDie type);
sigminer::Signature BuildSignature(llvm::DWARFDie subprogramDie);

namespace rich {

sigminer::rich::TypeEntry BuildTypeEntry(
        llvm::DWARFDie type,
        std::unordered_set<std::uint64_t>& active,
        unsigned depth);
sigminer::rich::TypeEntry ResolveToTypeEntryFromType(llvm::DWARFDie type);
sigminer::rich::Signature BuildSignature(llvm::DWARFDie subprogramDie);

} // namespace rich

} // namespace signature_builder
