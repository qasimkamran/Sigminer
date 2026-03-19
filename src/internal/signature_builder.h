#pragma once

#include <llvm/DebugInfo/DWARF/DWARFDie.h>

#include "sigminer/signature.h"

namespace signature_builder {

sigminer::TypeEntry ResolveToTypeEntryFromType(llvm::DWARFDie type);
sigminer::Signature BuildSignature(llvm::DWARFDie subprogramDie);

} // namespace signature_builder
