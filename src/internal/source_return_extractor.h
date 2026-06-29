#pragma once

#include <string>
#include <vector>

#include <llvm/Support/Error.h>

#include "sigminer/func_info.h"

namespace source_return_extractor {

llvm::Expected<std::vector<sigminer::SourceReturnCandidate>> ExtractSourceReturnCandidates(
        const std::string& compileCommandsPathOrDirectory,
        const std::string& sourceFilePath,
        const std::string& functionName,
        const std::vector<std::string>& extraClangArgs = {});

} // namespace source_return_extractor
