#include <string>

#include "internal/printf_specifier_mapper.h"

namespace printf_specifier_mapper {

std::string TypeEntryToPrintfSpecifier(const sigminer::TypeEntry& typeEntry)
{
    std::string result;
    switch (typeEntry.kind) {
        case sigminer::PrimitiveKind::POINTER:
            result = "%p";
            break;

        case sigminer::PrimitiveKind::BOOL:
            result = "%d";
            break;

        case sigminer::PrimitiveKind::INT:
        case sigminer::PrimitiveKind::ENUM:
            if (typeEntry.size == 8) {
                if (typeEntry.sign == sigminer::Signedness::SIGNED)
                    result = "%ld";
                else if (typeEntry.sign == sigminer::Signedness::UNSIGNED)
                    result = "%lu";
            } else {
                if (typeEntry.sign == sigminer::Signedness::UNSIGNED)
                    result = "%u";
                else
                    result = "%d";
            }
            break;

        case sigminer::PrimitiveKind::FLOAT:
            if (typeEntry.size == 8)
                result = "%lx";
            else
                result = "%x";
            break;

        case sigminer::PrimitiveKind::AGGREGATE:
        case sigminer::PrimitiveKind::UNKNOWN:
            if (typeEntry.isPointer)
                result = "%p";
            else
                result = "%lx";
            break;

        default:
            break;
    }
    result = "(" + typeEntry.name + ") " + result;
    return result;
}

std::string TypeEntriesToPrintfSpecifier(const std::vector<sigminer::TypeEntry>& typeEntries)
{
    std::string result;

    if (typeEntries.empty())
        return result;

    for (std::size_t i = 0; i < typeEntries.size(); ++i) {
        std::string specifier = TypeEntryToPrintfSpecifier(typeEntries[i]);
        if (!specifier.empty())
            result += specifier;
        else
            continue;

        if (i != typeEntries.size() - 1)
            result += ' ';
    }

    return result;
}

} // namespace printf_specifier_mapper
