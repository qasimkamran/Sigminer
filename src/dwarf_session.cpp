#include "internal/dwarf_session.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <llvm/Support/Casting.h>
#include <llvm/Support/CRC.h>
#include <llvm/Support/Endian.h>
#include <llvm/Support/MemoryBuffer.h>

namespace dwarf_session {

namespace {

struct DebugLink
{
    std::string fileName;
    std::uint32_t crc = 0;
};

std::optional<DebugLink> GetDebugLink(const llvm::object::ObjectFile& object)
{
    for (const llvm::object::SectionRef& section : object.sections()) {
        llvm::Expected<llvm::StringRef> name = section.getName();
        if (!name) {
            llvm::consumeError(name.takeError());
            continue;
        }
        if (*name != ".gnu_debuglink")
            continue;

        llvm::Expected<llvm::StringRef> contents = section.getContents();
        if (!contents) {
            llvm::consumeError(contents.takeError());
            return std::nullopt;
        }

        const std::size_t terminator = contents->find('\0');
        if (terminator == llvm::StringRef::npos || terminator == 0)
            return std::nullopt;

        const std::size_t crcOffset = (terminator + 1 + 3) & ~std::size_t(3);
        if (crcOffset + sizeof(std::uint32_t) > contents->size())
            return std::nullopt;

        const auto* crcBytes = reinterpret_cast<const std::uint8_t*>(
                contents->data() + crcOffset);
        return DebugLink{
                .fileName = contents->substr(0, terminator).str(),
                .crc = llvm::support::endian::read32le(crcBytes),
        };
    }
    return std::nullopt;
}

bool HasExpectedCrc(const std::filesystem::path& path, std::uint32_t expectedCrc)
{
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer =
            llvm::MemoryBuffer::getFile(path.string());
    if (!buffer)
        return false;

    const llvm::StringRef data = (*buffer)->getBuffer();
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data.data());
    return llvm::crc32(llvm::ArrayRef<std::uint8_t>(bytes, data.size())) == expectedCrc;
}

std::optional<std::filesystem::path> FindDebugFile(
        const std::filesystem::path& objectPath,
        const DebugLink& link)
{
    const std::filesystem::path directory = objectPath.parent_path();
    std::vector<std::filesystem::path> candidates{
            directory / link.fileName,
            directory / ".debug" / link.fileName,
    };

    if (objectPath.is_absolute()) {
        candidates.push_back(
                std::filesystem::path("/usr/lib/debug") /
                objectPath.relative_path().parent_path() / link.fileName);
    }

    for (const std::filesystem::path& candidate : candidates) {
        if (candidate != objectPath && HasExpectedCrc(candidate, link.crc))
            return candidate;
    }
    return std::nullopt;
}

} // namespace

Session::Session(llvm::object::OwningBinary<llvm::object::Binary>&& binary,
                 llvm::object::ObjectFile* object,
                 std::unique_ptr<llvm::DWARFContext>&& context)
    : binary(std::move(binary)), object(object), context(std::move(context))
{
}

void InitializeLLVM()
{
    static const bool initialized = true;
    (void) initialized;
}

llvm::Expected<Session> Open(std::string_view filePath)
{
    InitializeLLVM();

    llvm::Expected<llvm::object::OwningBinary<llvm::object::Binary>> binaryOrErr =
            llvm::object::createBinary(std::string(filePath));

    if (!binaryOrErr)
        return binaryOrErr.takeError();

    llvm::object::OwningBinary<llvm::object::Binary> owningBinary = std::move(*binaryOrErr);
    llvm::object::Binary& binary = *owningBinary.getBinary();
    llvm::object::ObjectFile* object = llvm::dyn_cast<llvm::object::ObjectFile>(&binary);
    if (!object)
        return llvm::createStringError(std::errc::invalid_argument, "input is not an object file");

    std::unique_ptr<llvm::DWARFContext> context = llvm::DWARFContext::create(*object);
    if (context && context->compile_units().empty()) {
        const std::optional<DebugLink> debugLink = GetDebugLink(*object);
        if (debugLink) {
            const std::optional<std::filesystem::path> debugFile =
                    FindDebugFile(std::filesystem::path(filePath), *debugLink);
            if (debugFile)
                return Open(debugFile->string());
        }
    }

    return Session(std::move(owningBinary), object, std::move(context));
}

} // namespace dwarf_session
