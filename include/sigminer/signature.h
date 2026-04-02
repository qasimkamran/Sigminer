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

    PrimitiveKind kind = PrimitiveKind::UNKNOWN;
    Signedness sign = Signedness::UNKNOWN;
    std::size_t size = 0;
    bool isPointer = false;
    std::string name{};

    bool operator!() const
    {
        if (kind == PrimitiveKind::UNKNOWN)
            return false;
        return true;
    }
};

class Signature
{
public:
    Signature() = default;
    explicit Signature(const ::Signature& source);

    TypeEntry ret{};
    std::vector<TypeEntry> params{};
    bool hasVarArgs = false;
};

} // namespace sigminer
