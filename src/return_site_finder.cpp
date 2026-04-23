#include "internal/return_site_finder.h"
#include "sigminer/func_info.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

namespace {

struct DisassemblerContext
{
    std::unique_ptr<llvm::MCRegisterInfo> registerInfo;
    std::unique_ptr<llvm::MCAsmInfo> asmInfo;
    std::unique_ptr<llvm::MCSubtargetInfo> subtargetInfo;
    std::unique_ptr<llvm::MCInstrInfo> instrInfo;
    std::unique_ptr<llvm::MCContext> context;
    std::unique_ptr<llvm::MCDisassembler> disassembler;
};

bool RangeContainsRange(
        std::uint64_t rangeLow,
        std::uint64_t rangeHigh,
        std::uint64_t addressLow,
        std::uint64_t addressHigh)
{
    return rangeLow <= addressLow && addressLow <= addressHigh && addressHigh <= rangeHigh;
}

bool InitializeTargetForTriple(const llvm::Triple& triple)
{
    static const bool x86Initialized = [] {
        LLVMInitializeX86TargetInfo();
        LLVMInitializeX86TargetMC();
        LLVMInitializeX86Disassembler();
        return true;
    }();

    switch (triple.getArch()) {
        case llvm::Triple::x86:
        case llvm::Triple::x86_64:
            (void) x86Initialized;
            return true;
        default:
            return false;
    }
}

std::unique_ptr<DisassemblerContext> CreateDisassemblerContext(
        const llvm::object::ObjectFile& object)
{
    llvm::Triple triple = object.makeTriple();
    if (!InitializeTargetForTriple(triple))
        return nullptr;

    std::string error{};
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple.str(), error);
    if (!target)
        return nullptr;

    std::unique_ptr<DisassemblerContext> ctx = std::make_unique<DisassemblerContext>();
    const std::string tripleName = triple.str();
    const std::string features = object.getFeatures().getString();

    ctx->registerInfo.reset(target->createMCRegInfo(tripleName));
    if (!ctx->registerInfo)
        return nullptr;

    llvm::MCTargetOptions targetOptions{};
    ctx->asmInfo.reset(target->createMCAsmInfo(*ctx->registerInfo, tripleName, targetOptions));
    if (!ctx->asmInfo)
        return nullptr;

    ctx->subtargetInfo.reset(target->createMCSubtargetInfo(tripleName, "", features));
    if (!ctx->subtargetInfo)
        return nullptr;

    ctx->instrInfo.reset(target->createMCInstrInfo());
    if (!ctx->instrInfo)
        return nullptr;

    ctx->context = std::make_unique<llvm::MCContext>(
            triple,
            ctx->asmInfo.get(),
            ctx->registerInfo.get(),
            ctx->subtargetInfo.get());

    ctx->disassembler.reset(target->createMCDisassembler(*ctx->subtargetInfo, *ctx->context));
    if (!ctx->disassembler)
        return nullptr;

    return ctx;
}

} // namespace

llvm::DWARFAddressRangesVector GetSubprogramDieAddressRanges(const llvm::DWARFDie& subprogramDie)
{
    if (!subprogramDie)
        return {};

    llvm::Expected<llvm::DWARFAddressRangesVector> rangesOrErr = subprogramDie.getAddressRanges();
    if (!rangesOrErr) {
        llvm::consumeError(rangesOrErr.takeError());
        return {};
    }

    return std::move(*rangesOrErr);
}

std::vector<sigminer::ReturnSite> GetReturnSitesWithinSubprogramDie(
        const llvm::DWARFDie& subprogramDie,
        const llvm::object::ObjectFile& object)
{
    if (!subprogramDie)
        return {};

    std::vector<sigminer::ReturnSite> returnSites{};
    const std::unique_ptr<DisassemblerContext> disasmCtx = CreateDisassemblerContext(object);
    if (!disasmCtx)
        return {};

    const llvm::DWARFAddressRangesVector addressRanges = GetSubprogramDieAddressRanges(subprogramDie);
    std::uint64_t functionLow = std::numeric_limits<std::uint64_t>::max();
    for (const llvm::DWARFAddressRange& range : addressRanges) {
        if (range.LowPC < range.HighPC)
            functionLow = std::min(functionLow, range.LowPC);
    }

    if (functionLow == std::numeric_limits<std::uint64_t>::max())
        return {};

    for (const llvm::DWARFAddressRange& range : addressRanges)
    {
        if (range.LowPC >= range.HighPC)
            continue;

        for (const llvm::object::SectionRef& section : object.sections()) {
            if (!section.isText())
                continue;

            const std::uint64_t sectionLow = section.getAddress();
            const std::uint64_t sectionHigh = sectionLow + section.getSize();
            if (!RangeContainsRange(sectionLow, sectionHigh, range.LowPC, range.HighPC))
                continue;

            llvm::Expected<llvm::StringRef> contentsOrErr = section.getContents();
            if (!contentsOrErr) {
                llvm::consumeError(contentsOrErr.takeError());
                continue;
            }

            const llvm::StringRef contents = *contentsOrErr;
            const std::uint64_t rangeOffset = range.LowPC - sectionLow;
            const std::uint64_t rangeSize = range.HighPC - range.LowPC;
            if (rangeOffset > contents.size() || rangeSize > contents.size() - rangeOffset)
                continue;

            const llvm::StringRef rangeBytes = contents.substr(rangeOffset, rangeSize);
            std::uint64_t offset = 0;
            while (offset < rangeBytes.size())
            {
                llvm::MCInst inst{};
                std::uint64_t instSize = 0;
                llvm::raw_null_ostream nullStream{};
                
                const auto* byteData = reinterpret_cast<const std::uint8_t*>(rangeBytes.data() + offset);
                
                const llvm::ArrayRef<std::uint8_t> remainingBytes(byteData, rangeBytes.size() - offset);
                
                const std::uint64_t instructionAddress = range.LowPC + offset;

                const llvm::MCDisassembler::DecodeStatus status =
                        disasmCtx->disassembler->getInstruction(
                                inst,
                                instSize,
                                remainingBytes,
                                instructionAddress,
                                nullStream);

                if (status == llvm::MCDisassembler::Fail || instSize == 0) {
                    ++offset;
                    continue;
                }

                const llvm::MCInstrDesc& instDesc = disasmCtx->instrInfo->get(inst.getOpcode());
                if (instDesc.isReturn()) {
                    returnSites.push_back(sigminer::ReturnSite{
                            .instructionAddress = instructionAddress,
                            .funcOffset = instructionAddress - functionLow,
                    });
                }
                offset += instSize;
            }
        }
    }
    return returnSites;
}
