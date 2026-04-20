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

class RichTypeEntry;

class RichTypeMember
{
public:
    std::string name{};
    std::size_t offset = 0;
    std::unique_ptr<RichTypeEntry> type{};
};

class RichTypeEntry
{
public:
    RichTypeEntry() = default;
    explicit RichTypeEntry(const struct ::RichTypeEntry& source);
    RichTypeEntry(const RichTypeEntry& other);
    RichTypeEntry& operator=(const RichTypeEntry& other);
    RichTypeEntry(RichTypeEntry&&) noexcept = default;
    RichTypeEntry& operator=(RichTypeEntry&&) noexcept = default;

    PrimitiveKind kind = PrimitiveKind::UNKNOWN;
    Signedness sign = Signedness::UNKNOWN;
    std::size_t size = 0;
    std::string name{};
    bool isConst = false;
    bool isStringLike = false;
    bool isRecursiveReference = false;
    std::size_t arrayCount = 0;
    std::unique_ptr<RichTypeEntry> pointee{};
    std::unique_ptr<RichTypeEntry> elementType{};
    std::vector<RichTypeMember> members{};
};

class RichParameter
{
public:
    RichParameter() = default;
    explicit RichParameter(const struct ::RichParameter& source);
    RichParameter(const RichParameter& other);
    RichParameter& operator=(const RichParameter& other);
    RichParameter(RichParameter&&) noexcept = default;
    RichParameter& operator=(RichParameter&&) noexcept = default;

    std::string name{};
    RichTypeEntry type{};
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

class RichSignature
{
public:
    RichSignature() = default;
    explicit RichSignature(const struct ::RichSignature& source);

    RichTypeEntry ret{};
    std::vector<RichParameter> params{};
    bool hasVarArgs = false;
};

} // namespace sigminer
