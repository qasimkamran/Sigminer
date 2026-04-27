#ifndef SIGMINER_C_H
#define SIGMINER_C_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum PrimitiveKind
{
    PRIMITIVE_KIND_VOID = 0,
    PRIMITIVE_KIND_BOOL,
    PRIMITIVE_KIND_INT,
    PRIMITIVE_KIND_FLOAT,
    PRIMITIVE_KIND_POINTER,
    PRIMITIVE_KIND_ENUM,
    PRIMITIVE_KIND_AGGREGATE,
    PRIMITIVE_KIND_UNKNOWN
} PrimitiveKind;

typedef enum Signedness
{
    SIGNEDNESS_SIGNED = 0,
    SIGNEDNESS_UNSIGNED,
    SIGNEDNESS_UNKNOWN
} Signedness;

typedef enum ReturnCode
{
    RETURN_CODE_SUCCESS = 0,
    RETURN_CODE_INVALID_INPUT,
    RETURN_CODE_FILE_OPEN_FAILURE,
    RETURN_CODE_SYMBOL_RESOLUTION_FAILURE,
    RETURN_CODE_DWARF_UNAVAILABLE,
    RETURN_CODE_FUNCTION_DIE_NOT_IN_RANGE,
    RETURN_CODE_UNSUPPORTED_TYPE,
    RETURN_CODE_INTERNAL_FAILURE
} ReturnCode;

typedef struct TypeEntry
{
    PrimitiveKind Kind;
    Signedness Sign;
    size_t Size;
    bool IsPointer;
    char* Name;
} TypeEntry;

typedef struct Signature
{
    TypeEntry Ret;
    TypeEntry* Params;
    size_t ParamCount;
    bool HasVarArgs;
} Signature;

typedef struct Result
{
    Signature Sig;
    bool HasSignature;
    ReturnCode RetCode;
} Result;

typedef struct RichParameter
{
    char* Name;
    TypeEntry Type;
} RichParameter;

typedef enum BpftraceProbeKind
{
    BPFTRACE_PROBE_KIND_ENTRY = 0,
    BPFTRACE_PROBE_KIND_RETURN
} BpftraceProbeKind;

typedef struct BpftraceProbeTarget
{
    const char* ModulePath;
    const char* Symbol;
} BpftraceProbeTarget;

typedef struct BpftraceRenderOptions
{
    bool HasPid;
    int  Pid;
    bool IncludeEntryProbe;
    bool IncludeReturnProbe;
    bool IncludeTimingMs;
    bool IncludeUserStack;
    bool IncludeArgumentPrinting;
    bool IncludeReturnPrinting;
} BpftraceRenderOptions;

typedef struct BpftraceResolvedSymbol
{
    BpftraceProbeTarget Target;
    Signature Sig;
} BpftraceResolvedSymbol;

Result SIGMINER_GetSignatureFromSharedObjectBySymbol( const char* SharedObjectFilePath, const char* Symbol );
Result SIGMINER_FindSignatureInModulesBySymbol(
        const char* const* ModulePaths,
        size_t ModulePathCount,
        const char* Symbol,
        BpftraceResolvedSymbol* Resolved );
void SIGMINER_FreeSignature( Signature* Sig );
void SIGMINER_FreeResult( Result* Res );
void SIGMINER_FreeBpftraceResolvedSymbol( BpftraceResolvedSymbol* Resolved );
void SIGMINER_FreeCString( const char* Str );
const char* SIGMINER_TypeEntryToPrintfSpecifier( const TypeEntry* typeEntry );
const char* SIGMINER_SignatureParamsToPrintfSpecifier( const Signature* Sig );
const char* SIGMINER_BuildBpftraceArgumentPrintExpr( const Signature* Sig );
const char* SIGMINER_BuildBpftraceReturnPrintExpr( const TypeEntry* RetType );
const char* SIGMINER_BuildBpftraceProbeBody(
        BpftraceProbeKind ProbeKind,
        const Signature* Sig,
        const BpftraceRenderOptions* Opts );
const char* SIGMINER_BuildBpftraceUprobeScriptForTarget(
        const BpftraceProbeTarget* Target,
        const Signature* Sig,
        const BpftraceRenderOptions* Opts );
const char* SIGMINER_BuildBpftraceUprobeScriptForResolvedSymbols(
        const BpftraceResolvedSymbol* ResolvedSymbols,
        size_t ResolvedSymbolCount,
        const BpftraceRenderOptions* Opts );

#ifdef __cplusplus
}
#endif
#endif
