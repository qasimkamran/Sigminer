#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "internal/bpftrace_mapper.h"
#include "sigminer/signature.h"

TEST(BpftraceMapperTest, BuildsArgumentPrintExpressionForEmptyParameterList)
{
    sigminer::Signature sig;

    EXPECT_EQ(
            bpftrace_mapper::BuildBpftraceArgumentPrintExpr(sig),
            "  printf(\"    args: (none)\\n\");\n");
}

TEST(BpftraceMapperTest, BuildsArgumentPrintExpressionForMultipleParameters)
{
    sigminer::TypeEntry pointerParam;
    pointerParam.kind = sigminer::PrimitiveKind::POINTER;
    pointerParam.sign = sigminer::Signedness::UNKNOWN;
    pointerParam.size = 8;
    pointerParam.isPointer = true;
    pointerParam.name = "int *";

    sigminer::TypeEntry intParam;
    intParam.kind = sigminer::PrimitiveKind::INT;
    intParam.sign = sigminer::Signedness::SIGNED;
    intParam.size = 4;
    intParam.isPointer = false;
    intParam.name = "int";

    sigminer::Signature sig;
    sig.params = {pointerParam, intParam};

    EXPECT_EQ(
            bpftrace_mapper::BuildBpftraceArgumentPrintExpr(sig),
            "  printf(\"    args: (int *) %p (int) %d\\n\", arg0, arg1);\n");
}

TEST(BpftraceMapperTest, BuildsReturnPrintExpressionForVoidReturn)
{
    sigminer::TypeEntry retType;
    retType.kind = sigminer::PrimitiveKind::VOID;
    retType.sign = sigminer::Signedness::UNKNOWN;
    retType.size = 0;
    retType.isPointer = false;
    retType.name = "void";

    EXPECT_EQ(
            bpftrace_mapper::BuildBpftraceReturnPrintExpr(retType),
            "  printf(\"    retval: (void)\\n\");\n");
}

TEST(BpftraceMapperTest, BuildsReturnPrintExpressionForSignedIntegerReturn)
{
    sigminer::TypeEntry retType;
    retType.kind = sigminer::PrimitiveKind::INT;
    retType.sign = sigminer::Signedness::SIGNED;
    retType.size = 4;
    retType.isPointer = false;
    retType.name = "int";

    EXPECT_EQ(
            bpftrace_mapper::BuildBpftraceReturnPrintExpr(retType),
            "  $ret = (int32)retval;\n"
            "  printf(\"    retval: (int) %d\\n\", $ret);\n");
}

TEST(BpftraceMapperTest, BuildsEntryProbeBodyWithTimingAndArguments)
{
    sigminer::TypeEntry argType;
    argType.kind = sigminer::PrimitiveKind::BOOL;
    argType.sign = sigminer::Signedness::UNKNOWN;
    argType.size = 1;
    argType.isPointer = false;
    argType.name = "bool";

    sigminer::Signature sig;
    sig.params = {argType};

    bpftrace_mapper::BpftraceRenderOptions opts;
    opts.includeEntryProbe = true;
    opts.includeReturnProbe = false;
    opts.includeTimingMs = true;
    opts.includeUserStack = false;
    opts.includeArgumentPrinting = true;
    opts.includeReturnPrinting = false;

    EXPECT_EQ(
            bpftrace_mapper::BuildBpftraceProbeBody(
                    bpftrace_mapper::BpftraceProbeKind::ENTRY,
                    sig,
                    opts),
            "  @start[tid] = nsecs;\n"
            "  printf(\"    args: (bool) %d\\n\", arg0);\n");
}

TEST(BpftraceMapperTest, BuildsFullUprobeScriptWithEntryAndReturnProbes)
{
    sigminer::TypeEntry retType;
    retType.kind = sigminer::PrimitiveKind::INT;
    retType.sign = sigminer::Signedness::UNSIGNED;
    retType.size = 8;
    retType.isPointer = false;
    retType.name = "unsigned long";

    sigminer::TypeEntry argType;
    argType.kind = sigminer::PrimitiveKind::POINTER;
    argType.sign = sigminer::Signedness::UNKNOWN;
    argType.size = 8;
    argType.isPointer = true;
    argType.name = "void *";

    sigminer::Signature sig;
    sig.ret = retType;
    sig.params = {argType};

    bpftrace_mapper::BpftraceProbeTarget target{
            .modulePath = "/tmp/libmock_lib.so",
            .symbol = "RunLiveLoop",
    };

    bpftrace_mapper::BpftraceRenderOptions opts;
    opts.pid = 4242;
    opts.includeEntryProbe = true;
    opts.includeReturnProbe = true;
    opts.includeTimingMs = true;
    opts.includeUserStack = false;
    opts.includeArgumentPrinting = true;
    opts.includeReturnPrinting = true;

    EXPECT_EQ(
            bpftrace_mapper::BuildBpftraceUprobeScript(target, sig, opts),
            "uprobe:/tmp/libmock_lib.so:RunLiveLoop /pid == 4242/\n"
            "{\n"
            "  printf(\"/tmp/libmock_lib.so:RunLiveLoop [entry]\\n\");\n"
            "  @start[tid] = nsecs;\n"
            "  printf(\"    args: (void *) %p\\n\", arg0);\n"
            "}\n"
            "\n"
            "uretprobe:/tmp/libmock_lib.so:RunLiveLoop /pid == 4242/\n"
            "{\n"
            "  printf(\"/tmp/libmock_lib.so:RunLiveLoop [return]\\n\");\n"
            "  $ret = (uint64)retval;\n"
            "  printf(\"    retval: (unsigned long) %lu\\n\", $ret);\n"
            "  if (@start[tid]) {\n"
            "    $elapsed_ms = (nsecs - @start[tid]) / 1000000;\n"
            "    printf(\"    elapsed_ms: %llu\\n\", $elapsed_ms);\n"
            "    delete(@start[tid]);\n"
            "  }\n"
            "}\n");
}

TEST(BpftraceMapperTest, RichBuildsFullUprobeScriptWithPrimitiveArgumentAndReturn)
{
    auto argType = std::make_unique<sigminer::rich::TypeEntry>();
    argType->kind = sigminer::PrimitiveKind::INT;
    argType->sign = sigminer::Signedness::SIGNED;
    argType->size = 4;
    argType->name = "int";

    sigminer::rich::Parameter arg;
    arg.name = "count";
    arg.type = std::move(argType);

    sigminer::rich::Signature sig;
    sig.ret.kind = sigminer::PrimitiveKind::INT;
    sig.ret.sign = sigminer::Signedness::SIGNED;
    sig.ret.size = 4;
    sig.ret.name = "int";
    sig.params.push_back(std::move(arg));

    bpftrace_mapper::BpftraceProbeTarget target{
            .modulePath = "/tmp/libmock_lib.so",
            .symbol = "RunLiveLoop",
    };

    bpftrace_mapper::rich::ExtendedBpftraceRenderOptions opts;
    opts.pid = 4242;
    opts.includeEntryProbe = true;
    opts.includeReturnProbe = true;
    opts.includeTimingMs = true;
    opts.includeUserStack = false;
    opts.includeArgumentPrinting = true;
    opts.includeReturnPrinting = true;

    EXPECT_EQ(
            bpftrace_mapper::rich::BuildBpftraceUprobeScript(target, sig, opts),
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
}
