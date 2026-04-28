#pragma once

#include <cstddef>
#include <memory>
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

namespace rich {

class TypeEntry;

class TypeMember
{
public:
    std::string name{};
    std::size_t offset = 0;
    std::unique_ptr<TypeEntry> type{};
};

class Parameter
{
public:
    Parameter() = default;
    explicit Parameter(const struct ::RichParameter& source);

    Parameter(const Parameter& other);
    Parameter& operator=(const Parameter& other);
    Parameter(Parameter&&) noexcept = default;
    Parameter& operator=(Parameter&&) noexcept = default;

    std::string name{};
    std::unique_ptr<TypeEntry> type{};
};

class TypeEntry
{
public:
    TypeEntry() = default;
    explicit TypeEntry(const struct ::RichTypeEntry& source);

    TypeEntry(const TypeEntry& other);
    TypeEntry& operator=(const TypeEntry& other);
    TypeEntry(TypeEntry&&) noexcept = default;
    TypeEntry& operator=(TypeEntry&&) noexcept = default;

    PrimitiveKind kind = PrimitiveKind::UNKNOWN;
    Signedness sign = Signedness::UNKNOWN;
    std::size_t size = 0;
    std::string name{};

    bool isConst = false;
    bool isStringLike = false;
    bool isRecursiveReference = false;

    std::size_t arrayCount = 0;
    std::unique_ptr<TypeEntry> elementType;

    std::unique_ptr<TypeEntry> pointee;
    std::vector<TypeMember> members;

    bool operator!() const
    {
        if (kind == PrimitiveKind::UNKNOWN)
            return false;
        return true;
    }
};

class Signature {
public:
    Signature() = default;
    explicit Signature(const ::RichSignature& source);

    TypeEntry ret{};
    std::vector<Parameter> params{};
    bool hasVarArgs = false;
};

} // namespace rich

} // namespace sigminer
