# Rich Value Decoding in Sigminer

## 1. Problem

The original `bpftrace` script generation in Sigminer only printed flat values:

- primitive arguments were printed directly
- pointers were mostly printed as addresses
- aggregates were not decoded into member values

That meant output like:

- `arg0: 0x7f...`
- `arg1: 0x7f...`

instead of:

- a pointer to string being rendered as the pointed-to string
- a pointer to primitive being rendered as address plus dereferenced value
- an aggregate being rendered member-by-member

The first idea was to solve this by generating richer typed `bpftrace` scripts and letting `bpftrace` itself do aggregate field access.

That ran into a fundamental environment problem:

- the target machines used `bpftrace` builds without `libdw`
- those builds could not consume DWARF for user-space type-driven field access
- even where the target `.so` had valid `.debug_info`, `bpftrace` could not use it

So the real problem became:

- Sigminer can read DWARF
- but the generated tracing path must not depend on `bpftrace` having DWARF support

## 2. Changes Required

To solve that properly, the implementation needed to do more than just tweak printing.

### 2.1 Rich Type Information in Sigminer

Sigminer needed a richer internal type model than the old shallow `TypeEntry`:

- pointee type information
- aggregate member lists
- member offsets
- array element information
- parameter names
- string-like classification

Without this, Sigminer could not know what to print for pointers or how to decode aggregate layouts.

### 2.2 A Separate Rich Extraction Path

The existing shallow signature path was useful and cheap. It should not be replaced wholesale.

So a separate rich extraction path was needed:

- keep shallow lookup for fast/basic usage
- add rich signature extraction only when nicer output is requested

### 2.3 A Renderer That Does Not Depend on bpftrace DWARF

The original typed approach would have required:

- generated `struct` definitions
- `bpftrace` field access like `.field`
- `bpftrace` understanding DWARF and typed user-space layouts

That was not viable on the real target environment.

So the renderer needed to change to:

- use Sigminer’s offline DWARF understanding
- compute field offsets in Sigminer
- emit raw `bpftrace` memory reads at `base_ptr + offset`
- recurse through nested aggregates using addresses and offsets only

### 2.4 An ABI Layer for Argument Capture

Even after removing typed aggregate access, one more dependency remained:

- `bpftrace` argument access via `arg0`, `arg1`, ... can itself depend on DWARF in some uprobe setups

So the renderer also needed to stop relying on `argN` for rich decoding and instead capture arguments directly from the platform ABI:

- register-based capture for integer/pointer args
- stack-based capture for spilled args
- special handling for by-value aggregates

## 3. What Was Implemented

### 3.1 Dual-Mode Type Model

The shallow type/signature model was preserved.

A parallel rich model was added in:

- `include/sigminer/signature.h`
- `include/sigminer/sigminer_c.h`

Added rich types:

- `RichTypeEntry`
- `RichTypeMember`
- `RichParameter`
- `RichSignature`
- `RichResult`

This gives Sigminer enough offline information to decode pointees and aggregate layouts without making the old API heavier.

### 3.2 Rich DWARF Extraction

A dedicated rich extraction path was added:

- `GetRichSignatureFromSharedObjectBySymbol(...)`
- `SIGMINER_GetRichSignatureFromSharedObjectBySymbol(...)`

Implemented in:

- `src/signature_builder.cpp`
- `src/sigminer.cpp`
- `src/sigminer_c.cpp`

What it captures:

- pointee type graphs
- aggregate members and offsets
- fixed array counts where available
- parameter names
- string-like `char*` / `char[N]`
- recursion/cycle stopping

### 3.3 Rich Script Builder

The rich script builder was added alongside the shallow one:

- `BuildRichBpftraceUprobeScript(...)`
- `SIGMINER_BuildRichBpftraceUprobeScriptForTarget(...)`

Implemented in:

- `src/internal/bpftrace_mapper.h`
- `src/bpftrace_mapper.cpp`

Render options were extended with:

- `EnableRichTypePrinting`
- `MaxAggregateDepth`
- `MaxAggregateMembers`
- `MaxArrayElements`

### 3.4 Offset-Based Aggregate Decoding

The first implementation attempted typed aggregate rendering through generated declarations and direct field access.

That was replaced with the final strategy:

- no aggregate preambles are required for the final rich path
- aggregate members are rendered from `base_ptr + member.offset`
- primitive fields are read from typed loads at the computed address
- pointer fields are read as pointer-sized integers at the computed address
- string fields are rendered by reading the pointer then calling `str(uptr(...))`
- nested aggregates recurse by advancing the base address

This moved aggregate understanding fully into Sigminer and out of `bpftrace`’s DWARF parser.

### 3.5 ABI-Based Argument Capture

The rich path no longer relies on `arg0`, `arg1`, ... for its main decoding logic.

Instead, for x86_64 SysV:

- integer/pointer args 0-5 are captured from:
  - `reg("di")`
  - `reg("si")`
  - `reg("dx")`
  - `reg("cx")`
  - `reg("r8")`
  - `reg("r9")`
- later integer/pointer args are captured from stack slots based on `reg("sp")`

This was required to keep rich decoding working even when `bpftrace` argument discovery via DWARF was unavailable.

### 3.6 Aggregate ABI Handling

By-value aggregates needed separate ABI handling.

Implemented cases:

- large memory-passed by-value aggregates:
  - treated as stack-backed memory
  - decoded by field offset from the stack address

- small non-float by-value aggregates up to 16 bytes on x86_64 SysV:
  - treated as register-backed aggregates
  - split into 8-byte ABI slots
  - fields decoded from those register slots by offset

This was the missing ABI layer needed for cases where a by-value aggregate was not a simple pointer and not memory-passed.

### 3.7 qk_eBPFTest Integration

`qk_eBPFTest.c` was updated to use the rich API:

- rich signature lookup
- rich script builder
- rich render options

It was also updated to allow the `bpftrace` binary to be overridden through:

- `BPFTRACE_PATH`

This was necessary because the real environment required testing with a rebuilt `bpftrace` rather than the hardcoded system one.

## 4. Why Each Choice Was Made

### 4.1 Why Keep a Shallow Path?

Because the original shallow API is still valuable:

- less complexity
- less DWARF traversal
- useful for quick/basic output

The richer path is more expensive and more specialized, so it should remain opt-in.

### 4.2 Why Not Let bpftrace Decode Structs Directly?

Because the real target environment showed this was not dependable:

- valid embedded DWARF existed in the target library
- the installed `bpftrace` still could not use it due to missing `libdw`

So a direct typed-field approach would fail in exactly the environments where the feature was needed.

### 4.3 Why Move Decoding Into Sigminer?

Because Sigminer already has the DWARF context needed to understand:

- member offsets
- nested layouts
- pointee types
- string-like fields

Once Sigminer has that information, it can generate raw-memory reads and no longer depend on `bpftrace` for type interpretation.

### 4.4 Why Add ABI Logic?

Because removing typed aggregate access was not enough.

The rich path also had to stop depending on `argN`, otherwise some environments would still indirectly require DWARF.

Using the platform ABI makes rich decoding more self-contained:

- arguments come from registers/stack
- field decoding comes from Sigminer’s DWARF
- `bpftrace` becomes a lower-level execution engine rather than the type engine

## 5. Validation Performed

Mock coverage was added and extended in:

- `test/mock_lib.c`
- `test/mock_live.cpp`
- `test/rich_bpftrace_mapper.cpp`

Covered cases:

- string pointers
- pointer-to-primitive
- pointer-to-aggregate
- memory-passed by-value aggregate
- small register-passed by-value aggregate

The local verification path used:

- `cmake --build build -j4`
- `./build/rich_bpftrace_mapper ./build/libmock_lib.so`

## 6. Current Limitations

This branch is still a feasibility study, not a complete production implementation.

Known gaps:

- float/SSE-class small aggregate ABI handling is still not implemented
- fields that straddle 8-byte ABI slot boundaries are still not fully handled
- aggregate return-by-value decoding is still limited
- arrays are only partially exercised and hardened
- no broad union/bitfield compatibility work has been added
- no rich bulk resolved-symbol script builder has been added
- no wide runtime compatibility matrix across `bpftrace` versions has been completed

## 7. Outcome

The branch now demonstrates a workable architecture for rich decoding that is much less dependent on `bpftrace`’s own DWARF support:

- Sigminer reads DWARF offline
- Sigminer computes layout and offsets
- Sigminer generates raw-memory decode logic
- the script captures arguments via the ABI rather than `argN`

In practical terms, this tackled the original problem by shifting responsibility:

- from `bpftrace` as the type interpreter
- to Sigminer as the type/layout engine

That is the main result of this work, and the reason this approach is a stronger fit for the real target environment.
