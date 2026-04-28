#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>

#include "sigminer/sigminer_c.h"
#include "test/unit/dwarf_test_utils.h"

namespace {

char* DuplicateCStringForTest(const char* value)
{
    const size_t length = std::strlen(value);
    char* copy = static_cast<char*>(std::malloc(length + 1));
    if (copy != nullptr)
        std::memcpy(copy, value, length + 1);
    return copy;
}

} // namespace

TEST(SigminerCTest, TypeEntryToPrintfSpecifierReturnsNullForNullInput)
{
    EXPECT_EQ(SIGMINER_TypeEntryToPrintfSpecifier(nullptr), nullptr);
}

TEST(SigminerCTest, TypeEntryToPrintfSpecifierFormatsPointerType)
{
    TypeEntry typeEntry{};
    typeEntry.Kind = PRIMITIVE_KIND_POINTER;
    typeEntry.Sign = SIGNEDNESS_UNKNOWN;
    typeEntry.Size = 8;
    typeEntry.IsPointer = true;
    typeEntry.Name = DuplicateCStringForTest("void *");

    const char* specifier = SIGMINER_TypeEntryToPrintfSpecifier(&typeEntry);

    ASSERT_NE(specifier, nullptr);
    EXPECT_STREQ(specifier, "(void *) %p");

    std::free(const_cast<char*>(specifier));
    std::free(typeEntry.Name);
}

TEST(SigminerCTest, SignatureParamsToPrintfSpecifierReturnsNullForEmptySignature)
{
    Signature sig{};

    EXPECT_EQ(SIGMINER_SignatureParamsToPrintfSpecifier(&sig), nullptr);
}

TEST(SigminerCTest, BuildBpftraceArgumentPrintExprRejectsMissingParamsArray)
{
    Signature sig{};
    sig.ParamCount = 1;
    sig.Params = nullptr;

    EXPECT_EQ(SIGMINER_BuildBpftraceArgumentPrintExpr(&sig), nullptr);
}

TEST(SigminerCTest, BuildBpftraceReturnPrintExprFormatsSignedIntegerReturn)
{
    TypeEntry retType{};
    retType.Kind = PRIMITIVE_KIND_INT;
    retType.Sign = SIGNEDNESS_SIGNED;
    retType.Size = 4;
    retType.IsPointer = false;
    retType.Name = DuplicateCStringForTest("int");

    const char* expr = SIGMINER_BuildBpftraceReturnPrintExpr(&retType);

    ASSERT_NE(expr, nullptr);
    EXPECT_STREQ(
            expr,
            "  $ret = (int32)retval;\n"
            "  printf(\"    retval: (int) %d\\n\", $ret);\n");

    std::free(const_cast<char*>(expr));
    std::free(retType.Name);
}

TEST(SigminerCTest, BuildBpftraceProbeBodyRejectsInvalidProbeKind)
{
    Signature sig{};

    EXPECT_EQ(
            SIGMINER_BuildBpftraceProbeBody(
                    static_cast<BpftraceProbeKind>(99),
                    &sig,
                    nullptr),
            nullptr);
}

TEST(SigminerCTest, FindSignatureInModulesBySymbolRejectsInvalidInput)
{
    EXPECT_EQ(
            SIGMINER_FindSignatureInModulesBySymbol(nullptr, 1, "RunLiveLoop", nullptr).RetCode,
            RETURN_CODE_INVALID_INPUT);
    EXPECT_EQ(
            SIGMINER_FindSignatureInModulesBySymbol(nullptr, 0, "RunLiveLoop", nullptr).RetCode,
            RETURN_CODE_INVALID_INPUT);
    EXPECT_EQ(
            SIGMINER_FindSignatureInModulesBySymbol(nullptr, 0, nullptr, nullptr).RetCode,
            RETURN_CODE_INVALID_INPUT);
}

TEST(SigminerCTest, FreeCStringAcceptsHeapAllocatedString)
{
    const char* str = DuplicateCStringForTest("hello");
    ASSERT_NE(str, nullptr);

    SIGMINER_FreeCString(str);

    SUCCEED();
}

TEST(SigminerCTest, FreeBpftraceResolvedSymbolReleasesOwnedMemoryAndResetsFields)
{
    BpftraceResolvedSymbol resolved{};
    resolved.Target.ModulePath = DuplicateCStringForTest("/tmp/libmock_lib.so");
    resolved.Target.Symbol = DuplicateCStringForTest("RunLiveLoop");
    resolved.Sig.Ret.Kind = PRIMITIVE_KIND_INT;
    resolved.Sig.Ret.Sign = SIGNEDNESS_SIGNED;
    resolved.Sig.Ret.Size = 4;
    resolved.Sig.Ret.Name = DuplicateCStringForTest("int");
    resolved.Sig.ParamCount = 1;
    resolved.Sig.Params = static_cast<TypeEntry*>(std::calloc(1, sizeof(TypeEntry)));
    ASSERT_NE(resolved.Sig.Params, nullptr);
    resolved.Sig.Params[0].Kind = PRIMITIVE_KIND_POINTER;
    resolved.Sig.Params[0].Sign = SIGNEDNESS_UNKNOWN;
    resolved.Sig.Params[0].Size = 8;
    resolved.Sig.Params[0].IsPointer = true;
    resolved.Sig.Params[0].Name = DuplicateCStringForTest("void *");
    resolved.Sig.HasVarArgs = true;

    SIGMINER_FreeBpftraceResolvedSymbol(&resolved);

    EXPECT_EQ(resolved.Target.ModulePath, nullptr);
    EXPECT_EQ(resolved.Target.Symbol, nullptr);
    EXPECT_EQ(resolved.Sig.Ret.Name, nullptr);
    EXPECT_EQ(resolved.Sig.Params, nullptr);
    EXPECT_EQ(resolved.Sig.ParamCount, 0u);
    EXPECT_FALSE(resolved.Sig.HasVarArgs);
}

TEST(SigminerCTest, FreeSignatureReleasesOwnedMemoryAndResetsFields)
{
    Signature sig{};
    sig.Ret.Kind = PRIMITIVE_KIND_INT;
    sig.Ret.Sign = SIGNEDNESS_SIGNED;
    sig.Ret.Size = 4;
    sig.Ret.IsPointer = false;
    sig.Ret.Name = DuplicateCStringForTest("int");
    sig.ParamCount = 1;
    sig.Params = static_cast<TypeEntry*>(std::calloc(1, sizeof(TypeEntry)));
    ASSERT_NE(sig.Params, nullptr);
    sig.Params[0].Kind = PRIMITIVE_KIND_POINTER;
    sig.Params[0].Sign = SIGNEDNESS_UNKNOWN;
    sig.Params[0].Size = 8;
    sig.Params[0].IsPointer = true;
    sig.Params[0].Name = DuplicateCStringForTest("void *");
    sig.HasVarArgs = true;

    SIGMINER_FreeSignature(&sig);

    EXPECT_EQ(sig.Ret.Kind, PRIMITIVE_KIND_UNKNOWN);
    EXPECT_EQ(sig.Ret.Sign, SIGNEDNESS_UNKNOWN);
    EXPECT_EQ(sig.Ret.Size, 0u);
    EXPECT_FALSE(sig.Ret.IsPointer);
    EXPECT_EQ(sig.Ret.Name, nullptr);
    EXPECT_EQ(sig.Params, nullptr);
    EXPECT_EQ(sig.ParamCount, 0u);
    EXPECT_FALSE(sig.HasVarArgs);
}

TEST(SigminerCTest, FreeResultReleasesOwnedMemoryAndResetsResult)
{
    Result result{};
    result.Sig.Ret.Kind = PRIMITIVE_KIND_INT;
    result.Sig.Ret.Sign = SIGNEDNESS_SIGNED;
    result.Sig.Ret.Size = 4;
    result.Sig.Ret.Name = DuplicateCStringForTest("int");
    result.HasSignature = true;
    result.RetCode = RETURN_CODE_INTERNAL_FAILURE;

    SIGMINER_FreeResult(&result);

    EXPECT_FALSE(result.HasSignature);
    EXPECT_EQ(result.RetCode, RETURN_CODE_SUCCESS);
    EXPECT_EQ(result.Sig.Ret.Name, nullptr);
    EXPECT_EQ(result.Sig.ParamCount, 0u);
}

TEST(SigminerCTest, GetRichSignatureFromSharedObjectBySymbolBuildsRichSignature)
{
    const auto fixturePath = dwarf_test_utils::BuiltArtifactPath("libdwarf_fixture_lib.so");

    RichResult result =
            SIGMINER_GetRichSignatureFromSharedObjectBySymbol(fixturePath.c_str(), "MixedTypes");

    ASSERT_EQ(result.RetCode, RETURN_CODE_SUCCESS);
    ASSERT_TRUE(result.HasSignature);
    EXPECT_EQ(result.Sig.Ret.Kind, PRIMITIVE_KIND_BOOL);
    EXPECT_STREQ(result.Sig.Ret.Name, "bool");
    ASSERT_EQ(result.Sig.ParamCount, 4u);
    EXPECT_STREQ(result.Sig.Params[0].Name, "count");
    EXPECT_EQ(result.Sig.Params[0].Type.Kind, PRIMITIVE_KIND_INT);
    EXPECT_STREQ(result.Sig.Params[1].Name, "label");
    EXPECT_EQ(result.Sig.Params[1].Type.Kind, PRIMITIVE_KIND_POINTER);
    EXPECT_EQ(result.Sig.Params[1].Type.Pointee->Kind, PRIMITIVE_KIND_INT);
    EXPECT_STREQ(result.Sig.Params[3].Name, "value");
    EXPECT_EQ(result.Sig.Params[3].Type.Kind, PRIMITIVE_KIND_AGGREGATE);
    ASSERT_EQ(result.Sig.Params[3].Type.MemberCount, 2u);
    EXPECT_STREQ(result.Sig.Params[3].Type.Members[0].Name, "x");

    SIGMINER_FreeRichResult(&result);
}

TEST(SigminerCTest, BuildRichBpftraceUprobeScriptForTargetFormatsPrimitiveSignature)
{
    RichSignature sig{};
    sig.Ret.Kind = PRIMITIVE_KIND_INT;
    sig.Ret.Sign = SIGNEDNESS_SIGNED;
    sig.Ret.Size = 4;
    sig.Ret.Name = DuplicateCStringForTest("int");
    sig.ParamCount = 1;
    sig.Params = static_cast<RichParameter*>(std::calloc(1, sizeof(RichParameter)));
    ASSERT_NE(sig.Params, nullptr);
    sig.Params[0].Name = DuplicateCStringForTest("count");
    sig.Params[0].Type.Kind = PRIMITIVE_KIND_INT;
    sig.Params[0].Type.Sign = SIGNEDNESS_SIGNED;
    sig.Params[0].Type.Size = 4;
    sig.Params[0].Type.Name = DuplicateCStringForTest("int");

    BpftraceProbeTarget target{
            .ModulePath = "/tmp/libmock_lib.so",
            .Symbol = "RunLiveLoop",
    };

    BpftraceRenderOptions opts{};
    opts.HasPid = true;
    opts.Pid = 4242;
    opts.IncludeEntryProbe = true;
    opts.IncludeReturnProbe = true;
    opts.IncludeTimingMs = true;
    opts.IncludeUserStack = false;
    opts.IncludeArgumentPrinting = true;
    opts.IncludeReturnPrinting = true;

    const char* script = SIGMINER_BuildRichBpftraceUprobeScriptForTarget(&target, &sig, &opts);

    ASSERT_NE(script, nullptr);
    EXPECT_STREQ(
            script,
            "uprobe:/tmp/libmock_lib.so:RunLiveLoop /pid == 4242/\n"
            "{\n"
            "  printf(\"/tmp/libmock_lib.so:RunLiveLoop [entry]\\n\");\n"
            "  @start[tid] = nsecs;\n"
            "  $arg0__count__value = (int32)(reg(\"di\"));\n"
            "  printf(\"    arg0 (count): (int) %d\\n\", $arg0__count__value);\n"
            "}\n"
            "\n"
            "uretprobe:/tmp/libmock_lib.so:RunLiveLoop /pid == 4242/\n"
            "{\n"
            "  printf(\"/tmp/libmock_lib.so:RunLiveLoop [return]\\n\");\n"
            "  $retval_value = (int32)(retval);\n"
            "  printf(\"    retval: (int) %d\\n\", $retval_value);\n"
            "  if (@start[tid]) {\n"
            "    $elapsed_ms = (nsecs - @start[tid]) / 1000000;\n"
            "    printf(\"    elapsed_ms: %llu\\n\", $elapsed_ms);\n"
            "    delete(@start[tid]);\n"
            "  }\n"
            "}\n");

    SIGMINER_FreeCString(script);
    SIGMINER_FreeRichSignature(&sig);
}

TEST(SigminerCTest, FreeRichSignatureReleasesOwnedMemoryAndResetsFields)
{
    RichSignature sig{};
    sig.Ret.Kind = PRIMITIVE_KIND_POINTER;
    sig.Ret.Sign = SIGNEDNESS_UNKNOWN;
    sig.Ret.Size = 8;
    sig.Ret.Name = DuplicateCStringForTest("char *");
    sig.Ret.IsStringLike = true;
    sig.Ret.Pointee = static_cast<RichTypeEntry*>(std::calloc(1, sizeof(RichTypeEntry)));
    ASSERT_NE(sig.Ret.Pointee, nullptr);
    sig.Ret.Pointee->Kind = PRIMITIVE_KIND_INT;
    sig.Ret.Pointee->Sign = SIGNEDNESS_UNSIGNED;
    sig.Ret.Pointee->Size = 1;
    sig.Ret.Pointee->Name = DuplicateCStringForTest("char");

    sig.ParamCount = 1;
    sig.Params = static_cast<RichParameter*>(std::calloc(1, sizeof(RichParameter)));
    ASSERT_NE(sig.Params, nullptr);
    sig.Params[0].Name = DuplicateCStringForTest("value");
    sig.Params[0].Type.Kind = PRIMITIVE_KIND_AGGREGATE;
    sig.Params[0].Type.Name = DuplicateCStringForTest("FixturePoint");
    sig.Params[0].Type.MemberCount = 1;
    sig.Params[0].Type.Members =
            static_cast<RichTypeMember*>(std::calloc(1, sizeof(RichTypeMember)));
    ASSERT_NE(sig.Params[0].Type.Members, nullptr);
    sig.Params[0].Type.Members[0].Name = DuplicateCStringForTest("x");
    sig.Params[0].Type.Members[0].Type =
            static_cast<RichTypeEntry*>(std::calloc(1, sizeof(RichTypeEntry)));
    ASSERT_NE(sig.Params[0].Type.Members[0].Type, nullptr);
    sig.Params[0].Type.Members[0].Type->Kind = PRIMITIVE_KIND_INT;
    sig.Params[0].Type.Members[0].Type->Sign = SIGNEDNESS_SIGNED;
    sig.Params[0].Type.Members[0].Type->Size = 4;
    sig.Params[0].Type.Members[0].Type->Name = DuplicateCStringForTest("int");
    sig.HasVarArgs = true;

    SIGMINER_FreeRichSignature(&sig);

    EXPECT_EQ(sig.Ret.Kind, PRIMITIVE_KIND_UNKNOWN);
    EXPECT_EQ(sig.Ret.Name, nullptr);
    EXPECT_EQ(sig.Ret.Pointee, nullptr);
    EXPECT_EQ(sig.Params, nullptr);
    EXPECT_EQ(sig.ParamCount, 0u);
    EXPECT_FALSE(sig.HasVarArgs);
}
