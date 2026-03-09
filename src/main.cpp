/*
 * Going to cut the guff and implement the whole program in this one file as a prototype.
 * Step-by-step, all packed into one function and tested along the way before being abstracted.
 * 
 * Step 0:
 *  Get the ELF file and convert it into a llvm dwarf library operable format, like a binary or
 *  object file, basically load the file so we can traverse through its DIE tree.
 *  [PASS]
 *
 * Step 1:
 *  Go through the CU of the binary being read from the ELF file then look through all of its
 *  subprograms, if the func we desire is found, return PASS, or else, return FAIL;
 *  [PASS]
 *
 * Step 2:
 *  For each subprogram found, resolve return value first as it is the primary type for the
 *  subprogram while making sure that it follows these rules: Represent signedness in an enum. 
 *  Specify the size of the value while discarding the wrapper types.
 */

#include <cstddef>
#include <cstdio>
#include <llvm-14/llvm/DebugInfo/DIContext.h>
#include <llvm-14/llvm/DebugInfo/DWARF/DWARFUnit.h>
#include <llvm-14/llvm/Support/Casting.h>
#include <llvm-14/llvm/Support/ELFAttributes.h>
#include <optional>
#include <string>
#include <stdio.h>

#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/Binary.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Error.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/DebugInfo/DWARF/DWARFDie.h"
#include "llvm/BinaryFormat/Dwarf.h"

class RetCode {
public:
    enum Value {
        PASS,
        FAIL,
        UNKNOWN
    };
    RetCode(Value v, const char* str) : value(v) {
        std::printf("%s: %s\n", (*this)(), str );
    }
    const char* operator()() const {
        switch (value) {
            case RetCode::PASS:    return "PASS";
            case RetCode::FAIL:    return "FAIL";
            case RetCode::UNKNOWN: return "UNKNOWN";
        }
        return "INVALID";
    }
    int asint() const { return static_cast<int>(value); }
private:
    Value value;
};

class Kind {
public:
    enum Value {
        VOID,
        BOOL,
        INT,
        FLOAT,
        POINTER,
        ENUM,
        AGGREGATE,
        UNKNOWN
    };

    Kind(Value v = UNKNOWN) : value(v) {}

    const char* toString() const {
        switch (value) {
            case VOID: return "void";
            case BOOL: return "bool";
            case INT: return "int";
            case FLOAT: return "float";
            case POINTER: return "pointer";
            case ENUM: return "enum";
            case AGGREGATE: return "aggregate";
            case UNKNOWN: return "unknown";
        }
        return "invalid";
    }

private:
    Value value;
};

class Signedness {
public:
    enum Value {
        SIGNED,
        UNSIGNED,
        UNKNOWN
    };

    Signedness(Value v = UNKNOWN) : value(v) {}

    const char* toString() const {
        switch (value) {
            case SIGNED: return "signed";
            case UNSIGNED: return "unsigned";
            case UNKNOWN: return "unknown";
        }
        return "invalid";
    }

private:
    Value value;
};

class TypeEntry {
public:
    Kind kind = Kind::UNKNOWN;
    Signedness signedness = Signedness::UNKNOWN;
    size_t size = 0;
    bool isPointer = false;

    std::string toString() const {
        return std::string("{kind:") + kind.toString() +
               ", signedness:" + signedness.toString() +
               ", size:" + std::to_string(size) +
               ", isPointer:" + (isPointer ? "true" : "false") + "}";
    }
};

static llvm::DWARFDie getTargetSubprogram(const llvm::DWARFDie& cuDie, std::string funcName) {
    for (llvm::DWARFDie die : cuDie.children()) {
        if (die.getTag() != llvm::dwarf::DW_TAG_subprogram)
            continue;
        const char* name = die.getShortName();
        if (!name)
            continue;
        if (funcName == name)
            return die;
    }
    return {};
}

static bool isWrapperTag(llvm::dwarf::Tag T) {
  switch (T) {
    case llvm::dwarf::DW_TAG_typedef:
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_volatile_type:
    case llvm::dwarf::DW_TAG_restrict_type:
    case llvm::dwarf::DW_TAG_atomic_type:
    case llvm::dwarf::DW_TAG_member:
    case llvm::dwarf::DW_TAG_subroutine_type:
      return true;
    default:
      return false;
  }
}

llvm::DWARFDie resolveUnderlyingType(llvm::DWARFDie type, unsigned MaxDepth = 64);

static Kind tagToKind(llvm::dwarf::Tag tag) {
    switch (tag) {
        case llvm::dwarf::DW_TAG_base_type:
            return Kind::INT;
        case llvm::dwarf::DW_TAG_pointer_type:
        case llvm::dwarf::DW_TAG_subroutine_type:
            return Kind::POINTER;
        case llvm::dwarf::DW_TAG_enumeration_type:
            return Kind::ENUM;
        case llvm::dwarf::DW_TAG_structure_type:
        case llvm::dwarf::DW_TAG_class_type:
        case llvm::dwarf::DW_TAG_union_type:
        case llvm::dwarf::DW_TAG_array_type:
            return Kind::AGGREGATE;
        default:
            return Kind::UNKNOWN;
    }
}

static std::optional<uint64_t> getUnsignedAttr(llvm::DWARFDie die, llvm::dwarf::Attribute attr) {
    if (auto value = die.find(attr)) {
        if (auto constant = value->getAsUnsignedConstant())
            return *constant;
    }
    return std::nullopt;
}

static Signedness getSignednessFromTypeDie(llvm::DWARFDie typeDie) {
    if (!typeDie.isValid())
        return Signedness::UNKNOWN;

    llvm::dwarf::Tag tag = static_cast<llvm::dwarf::Tag>(typeDie.getTag());
    if (tag == llvm::dwarf::DW_TAG_base_type) {
        auto encoding = getUnsignedAttr(typeDie, llvm::dwarf::DW_AT_encoding);
        if (!encoding)
            return Signedness::UNKNOWN;

        switch (*encoding) {
            case llvm::dwarf::DW_ATE_signed:
            case llvm::dwarf::DW_ATE_signed_char:
                return Signedness::SIGNED;
            case llvm::dwarf::DW_ATE_unsigned:
            case llvm::dwarf::DW_ATE_unsigned_char:
            case llvm::dwarf::DW_ATE_boolean:
            case llvm::dwarf::DW_ATE_UTF:
                return Signedness::UNSIGNED;
            default:
                return Signedness::UNKNOWN;
        }
    }

    if (tag == llvm::dwarf::DW_TAG_enumeration_type) {
        llvm::DWARFDie underlying = typeDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (underlying.isValid())
            return getSignednessFromTypeDie(resolveUnderlyingType(underlying));
    }

    return Signedness::UNKNOWN;
}

static size_t getSizeFromTypeDie(llvm::DWARFDie typeDie) {
    if (!typeDie.isValid())
        return 0;

    if (auto byteSize = getUnsignedAttr(typeDie, llvm::dwarf::DW_AT_byte_size))
        return static_cast<size_t>(*byteSize);

    llvm::dwarf::Tag tag = static_cast<llvm::dwarf::Tag>(typeDie.getTag());
    if (tag == llvm::dwarf::DW_TAG_pointer_type || tag == llvm::dwarf::DW_TAG_subroutine_type) {
        if (const llvm::DWARFUnit* unit = typeDie.getDwarfUnit())
            return unit->getAddressByteSize();
    }

    if (tag == llvm::dwarf::DW_TAG_enumeration_type) {
        llvm::DWARFDie underlying = typeDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (underlying.isValid())
            return getSizeFromTypeDie(resolveUnderlyingType(underlying));
    }

    return 0;
}

static TypeEntry typeDieToTypeEntry(llvm::DWARFDie typeDie) {
    TypeEntry entry = {};
    if (!typeDie.isValid())
        return entry;

    llvm::dwarf::Tag tag = static_cast<llvm::dwarf::Tag>(typeDie.getTag());
    entry.kind = tagToKind(tag);
    entry.signedness = getSignednessFromTypeDie(typeDie);
    entry.size = getSizeFromTypeDie(typeDie);
    entry.isPointer = tag == llvm::dwarf::DW_TAG_pointer_type || tag == llvm::dwarf::DW_TAG_subroutine_type;

    if (tag == llvm::dwarf::DW_TAG_base_type) {
        auto encoding = getUnsignedAttr(typeDie, llvm::dwarf::DW_AT_encoding);
        if (encoding == llvm::dwarf::DW_ATE_boolean)
            entry.kind = Kind::BOOL;
        else if (encoding == llvm::dwarf::DW_ATE_float)
            entry.kind = Kind::FLOAT;
    }

    return entry;
}

llvm::DWARFDie resolveUnderlyingType(llvm::DWARFDie type, unsigned MaxDepth) {
  if (!type)
      return type;

  type = type.resolveTypeUnitReference();

  for (unsigned i = 0; i < MaxDepth && type; ++i) {
    auto tag = static_cast<llvm::dwarf::Tag>(type.getTag());

    if (!isWrapperTag(tag))
      return type;

    llvm::DWARFDie next = type.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    if (!next || next == type)
      return type;

    type = next.resolveTypeUnitReference();
  }
  return type;
}

static const char* tagToString(llvm::dwarf::Tag tag) {
    switch (tag) {
        case llvm::dwarf::DW_TAG_structure_type: return "struct";
        case llvm::dwarf::DW_TAG_class_type: return "class";
        case llvm::dwarf::DW_TAG_union_type: return "union";
        case llvm::dwarf::DW_TAG_base_type: return "base";
        case llvm::dwarf::DW_TAG_pointer_type: return "pointer";
        case llvm::dwarf::DW_TAG_array_type: return "array";
        case llvm::dwarf::DW_TAG_enumeration_type: return "enum";
        default: return "other";
    }
}

static std::string dieNameOrFallback(llvm::DWARFDie die) {
    if (const char* shortName = die.getShortName())
        return shortName;
    return std::string("<unnamed@0x") + std::to_string(die.getOffset()) + ">";
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        return RetCode(RetCode::FAIL, "Pass exactly 2 arguments").asint();
    }

    std::string filepath = std::string(argv[1]);
    std::string funcName = std::string(argv[2]);

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    llvm::Expected<llvm::object::OwningBinary<llvm::object::Binary>> binaryOrErr = llvm::object::createBinary(argv[1]);

    if (!binaryOrErr)
        return RetCode(RetCode::FAIL, llvm::toString(binaryOrErr.takeError()).c_str()).asint();

    llvm::object::OwningBinary<llvm::object::Binary>& owningBin = *binaryOrErr;
    llvm::object::Binary& bin = *owningBin.getBinary();
    llvm::object::ObjectFile* obj = llvm::dyn_cast<llvm::object::ObjectFile>(&bin);

    if (!obj) {
        return RetCode(RetCode::FAIL, "Input is not an object file").asint();
    }

    auto dwarfContext = llvm::DWARFContext::create(*obj);
    llvm::DWARFDie subprogramDie = {};
    for (const std::unique_ptr<llvm::DWARFUnit>& cu : dwarfContext->compile_units()) {
        llvm::DWARFDie cuDie = cu->getUnitDIE(false);
        if (!cuDie)
            continue;
        subprogramDie = getTargetSubprogram(cuDie, funcName);
        if (subprogramDie)
            break;
    }
    if (!subprogramDie.isValid())
        return RetCode(RetCode::FAIL, "Could not find func in symbols").asint();
    if (!subprogramDie.isSubprogramDIE())
        return RetCode(RetCode::FAIL, "Found DIE is not a subprogram").asint();

    if (!subprogramDie.find(llvm::dwarf::DW_AT_type)) {
        TypeEntry returnTypeEntry = {};
        returnTypeEntry.kind = Kind::VOID;
        std::string message = std::string("Return type - ") + returnTypeEntry.toString();
        return RetCode(RetCode::PASS, message.c_str()).asint();
    }

    llvm::DWARFDie type = subprogramDie.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
    if (!type.isValid())
        return RetCode(RetCode::FAIL, "Subprogram has DW_AT_type but it could not be resolved").asint();

    llvm::DWARFDie resolvedType = resolveUnderlyingType(type);
    TypeEntry returnTypeEntry = typeDieToTypeEntry(resolvedType);

    llvm::dwarf::Tag returnTag = static_cast<llvm::dwarf::Tag>(resolvedType.getTag());

    std::string typeName = dieNameOrFallback(resolvedType);

    if (returnTag == llvm::dwarf::DW_TAG_pointer_type) {
        llvm::DWARFDie pointee = resolvedType.getAttributeValueAsReferencedDie(llvm::dwarf::DW_AT_type);
        if (pointee.isValid()) {
            llvm::DWARFDie basePointee = resolveUnderlyingType(pointee);
            typeName = dieNameOrFallback(basePointee);
        } else {
            typeName = "unknown";
        }
    }

    std::string message = "Return type - " + returnTypeEntry.toString() + ", name - " + typeName;
    return RetCode(RetCode::PASS, message.c_str()).asint();
}
