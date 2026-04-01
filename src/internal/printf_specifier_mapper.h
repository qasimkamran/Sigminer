#pragma once

#include <string>
#include <vector>

#include "sigminer/signature.h"

namespace printf_specifier_mapper {

std::string TypeEntryToPrintfSpecifier(const sigminer::TypeEntry& typeEntry);
std::string TypeEntriesToPrintfSpecifier(const std::vector<sigminer::TypeEntry>& typeEntries);

} // namespace printf_specifier_mapper
