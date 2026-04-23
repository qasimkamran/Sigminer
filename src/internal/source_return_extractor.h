#pragma once

#include <string>
#include <vector>

#include "sigminer/func_info.h"

std::vector<sigminer::SourceReturnCandidate> ExtractSourceReturnCandidates(
        const std::string& compileCommandsPathOrDirectory,
        const std::string& sourceFilePath,
        const std::string& functionName);
