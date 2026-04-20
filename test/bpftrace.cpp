#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <unistd.h>

#include "sigminer/sigminer_c.h"

namespace {

constexpr const char* kBpftracePath = "/usr/local/bin/bpftrace";
constexpr const char* kEnvBpftraceMaxProbes = "BPFTRACE_MAX_PROBES";
constexpr const char* kEnvBpftraceMaxBpfProgs = "BPFTRACE_MAX_BPF_PROGS";
constexpr const char* kEnvBpftraceStrlen = "BPFTRACE_STRLEN";
constexpr const char* kEnvValueMaxProbes = "200000";
constexpr const char* kEnvValueMaxBpfProgs = "200000";
constexpr const char* kEnvValueStrlen = "200";

bool HasRegexMeta(const char* text)
{
    return text != nullptr && std::strpbrk(text, ".*+?[]()|^$\\") != nullptr;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <pid> <function_symbol> [module_path]\n";
        return 1;
    }

    char* end_ptr = nullptr;
    errno = 0;
    const unsigned long parsed_pid = std::strtoul(argv[1], &end_ptr, 10);
    if (errno != 0 || end_ptr == nullptr || *end_ptr != '\0' || parsed_pid == 0 ||
        parsed_pid > static_cast<unsigned long>(std::numeric_limits<int>::max())) {
        std::cerr << "Error: invalid pid '" << argv[1] << "'.\n";
        return 2;
    }

    const int pid = static_cast<int>(parsed_pid);
    const char* symbol = argv[2];
    if (symbol == nullptr || symbol[0] == '\0') {
        std::cerr << "Error: function symbol is required.\n";
        return 3;
    }
    if (HasRegexMeta(symbol)) {
        std::cerr << "Error: function symbol must be an exact symbol name.\n";
        return 4;
    }

    const std::string module_path =
            (argc == 4) ? argv[3] : "/home/ubuntu/Projects/Sigminer/build/libmock_lib.so";
    if (access(module_path.c_str(), R_OK) != 0) {
        std::cerr << "Error: module '" << module_path << "' is not readable.\n";
        return 5;
    }

    RichResult sig_result =
            SIGMINER_GetRichSignatureFromSharedObjectBySymbol(module_path.c_str(), symbol);
    if (!sig_result.HasSignature) {
        std::cerr << "Error: symbol '" << symbol << "' was not found in '" << module_path
                  << "'. ReturnCode=" << sig_result.RetCode << '\n';
        SIGMINER_FreeRichResult(&sig_result);
        return 6;
    }

    BpftraceProbeTarget target{
            .ModulePath = module_path.c_str(),
            .Symbol = symbol,
    };
    BpftraceRenderOptions opts{
            .HasPid = true,
            .Pid = pid,
            .IncludeEntryProbe = true,
            .IncludeReturnProbe = true,
            .IncludeTimingMs = true,
            .IncludeUserStack = true,
            .IncludeArgumentPrinting = true,
            .IncludeReturnPrinting = true,
            .EnableRichTypePrinting = true,
            .MaxAggregateDepth = 2,
            .MaxAggregateMembers = 16,
            .MaxArrayElements = 8,
    };

    char* script = const_cast<char*>(
            SIGMINER_BuildRichBpftraceUprobeScriptForTarget(&target, &sig_result.Sig, &opts));
    SIGMINER_FreeRichResult(&sig_result);

    if (script == nullptr) {
        std::cerr << "Error: failed to build bpftrace script.\n";
        return 7;
    }

    std::cerr << "[ INFO ] pid=" << pid << '\n';
    std::cerr << "[ INFO ] module=" << module_path << '\n';
    std::cerr << "[ INFO ] symbol=" << symbol << '\n';
    std::cerr << "[ INFO ] Press Ctrl-C to stop.\n";

    ::setenv(kEnvBpftraceMaxProbes, kEnvValueMaxProbes, 1);
    ::setenv(kEnvBpftraceMaxBpfProgs, kEnvValueMaxBpfProgs, 1);
    ::setenv(kEnvBpftraceStrlen, kEnvValueStrlen, 1);

    char* const args[] = {
            const_cast<char*>(kBpftracePath),
            const_cast<char*>("--unsafe"),
            const_cast<char*>("-v"),
            const_cast<char*>("-e"),
            script,
            nullptr,
    };
    ::execv(kBpftracePath, args);

    std::perror("execv(/usr/bin/bpftrace)");
    std::free(script);
    return 8;
}
