#pragma once

#include <cstddef>
#include <vector>

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

class TypeEntry
{
public:
    PrimitiveKind Kind = PrimitiveKind::UNKNOWN;
    std::size_t Size = 0;
    bool Signed = false;
};

class Signature
{
public:
    TypeEntry Ret{};
    std::vector<TypeEntry> Params{};
    bool HasVarArgs = false;
};

} // namespace sigminer

