#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "sigminer/sigminer_c.h"

namespace sigminer {

enum class PrimitiveKind
{
    VOID,
    BOOL,
    INT,
    FLOAT,
    POINTER,
    ENUM,
    AGGREGATE,
    UNKNOWN
};

enum class Signedness
{
    SIGNED,
    UNSIGNED,
    UNKNOWN
};

class TypeEntry
{
public:
    TypeEntry() = default;
    explicit TypeEntry(const ::TypeEntry& source);

    PrimitiveKind Kind = PrimitiveKind::UNKNOWN;
    Signedness Sign = Signedness::UNKNOWN;
    std::size_t Size = 0;
    bool IsPointer = false;
    std::string Name{};

    bool operator!() const
    {
        if (Kind == PrimitiveKind::UNKNOWN)
            return false;
        return true;
    }
};

class Signature
{
public:
    TypeEntry Ret{};
    std::vector<TypeEntry> Params{};
    bool HasVarArgs = false;
};

} // namespace sigminer
