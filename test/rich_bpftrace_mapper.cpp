#include <cstring>
#include <iostream>
#include <string>

#include "sigminer/sigminer_c.h"

namespace {

bool Contains(const char* haystack, const char* needle)
{
    return haystack != nullptr && needle != nullptr && std::strstr(haystack, needle) != nullptr;
}

int CheckScript(const char* modulePath, const char* symbol, const char* expected)
{
    RichResult sig = SIGMINER_GetRichSignatureFromSharedObjectBySymbol(modulePath, symbol);
    if (!sig.HasSignature) {
        std::cerr << "rich signature lookup failed for " << symbol << "\n";
        return 1;
    }

    BpftraceProbeTarget target{
            .ModulePath = modulePath,
            .Symbol = symbol,
    };
    BpftraceRenderOptions opts{
            .HasPid = false,
            .Pid = 0,
            .IncludeEntryProbe = true,
            .IncludeReturnProbe = true,
            .IncludeTimingMs = false,
            .IncludeUserStack = false,
            .IncludeArgumentPrinting = true,
            .IncludeReturnPrinting = true,
            .EnableRichTypePrinting = true,
            .MaxAggregateDepth = 2,
            .MaxAggregateMembers = 16,
            .MaxArrayElements = 8,
    };

    const char* script = SIGMINER_BuildRichBpftraceUprobeScriptForTarget(&target, &sig.Sig, &opts);
    if (script == nullptr || !Contains(script, expected)) {
        std::cerr << "unexpected rich script for " << symbol << "\n";
        SIGMINER_FreeCString(script);
        SIGMINER_FreeRichResult(&sig);
        return 2;
    }

    SIGMINER_FreeCString(script);
    SIGMINER_FreeRichResult(&sig);
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <module>\n";
        return 1;
    }

    Result shallow = SIGMINER_GetSignatureFromSharedObjectBySymbol(argv[1], "CountString");
    if (!shallow.HasSignature) {
        std::cerr << "shallow signature lookup failed\n";
        return 2;
    }

    BpftraceProbeTarget target{
            .ModulePath = argv[1],
            .Symbol = "CountString",
    };
    BpftraceRenderOptions shallowOpts{
            .HasPid = false,
            .Pid = 0,
            .IncludeEntryProbe = true,
            .IncludeReturnProbe = true,
            .IncludeTimingMs = false,
            .IncludeUserStack = false,
            .IncludeArgumentPrinting = true,
            .IncludeReturnPrinting = true,
            .EnableRichTypePrinting = false,
            .MaxAggregateDepth = 0,
            .MaxAggregateMembers = 0,
            .MaxArrayElements = 0,
    };
    const char* shallowScript =
            SIGMINER_BuildBpftraceUprobeScriptForTarget(&target, &shallow.Sig, &shallowOpts);
    if (shallowScript == nullptr || Contains(shallowScript, "str(uptr")) {
        std::cerr << "shallow script unexpectedly contains rich rendering\n";
        SIGMINER_FreeCString(shallowScript);
        SIGMINER_FreeResult(&shallow);
        return 3;
    }
    SIGMINER_FreeCString(shallowScript);
    SIGMINER_FreeResult(&shallow);

    if (CheckScript(argv[1], "CountString", "reg(\"di\")") != 0)
        return 4;
    if (CheckScript(argv[1], "DerefInt", "uptr((int32*)($arg0__value__ptr))") != 0)
        return 5;
    if (CheckScript(argv[1], "SumTraceNode", "($arg0__node__ptr + 8)") != 0)
        return 6;
    if (CheckScript(argv[1], "SumTraceNodeByValue", "reg(\"sp\") + 8") != 0)
        return 7;
    if (CheckScript(argv[1], "SumSmallTraceByValue", "reg(\"si\")") != 0)
        return 8;

    return 0;
}
