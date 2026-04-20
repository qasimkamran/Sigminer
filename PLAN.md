# Rich Value Decoding Feasibility Study

## Summary

This branch was implemented as a feasibility study for richer `bpftrace` output generation in Sigminer.

Primary goal:

- decode pointers and aggregates into meaningful printed values instead of only printing raw addresses

Constraints we followed:

- keep the current shallow type/signature path intact for speed and low complexity
- add richer behavior through a separate opt-in path
- validate the approach with concrete mock symbols and generated-script checks

## 1. Dual-Mode Type Metadata

### Plan Followed

Keep the existing cutdown `TypeEntry` model for the fast path and introduce a separate recursive rich model that can carry:

- pointee information
- aggregate member lists
- array metadata
- parameter names
- string-like classification

### How It Was Implemented

The shallow types were left in place and a parallel rich model was added in:

- `include/sigminer/signature.h`
- `include/sigminer/sigminer_c.h`

Added rich types:

- `RichTypeEntry`
- `RichTypeMember`
- `RichParameter`
- `RichSignature`
- `RichResult`

The shallow API still backs the old lightweight flow. The rich API is separate and only used when richer decoding is requested.

## 2. Rich Signature Extraction

### Plan Followed

Add a dedicated rich-signature extraction path instead of forcing the existing shallow signature lookup to always build a recursive DWARF type graph.

### How It Was Implemented

New rich extraction APIs were added in:

- `include/sigminer/sigminer.h`
- `include/sigminer/sigminer_c.h`
- `src/sigminer.cpp`
- `src/sigminer_c.cpp`

New entry points:

- `GetRichSignatureFromSharedObjectBySymbol(...)`
- `SIGMINER_GetRichSignatureFromSharedObjectBySymbol(...)`

Rich extraction logic was implemented in:

- `src/signature_builder.cpp`

Current rich extraction behavior:

- recursively resolves pointee types
- recursively resolves struct/class/union members
- captures fixed array counts where DWARF exposes them
- captures parameter names from `DW_TAG_formal_parameter`
- marks `char *` and `char[N]` as string-like
- uses recursion depth and active-DIE tracking to stop cycles

## 3. Rich bpftrace Rendering

### Plan Followed

Keep the old shallow mapper behavior for the default case, and add a second renderer that emits richer `bpftrace` code for:

- string pointers
- pointer-to-primitive dereference
- pointer-to-aggregate expansion
- aggregate member printing
- bounded array expansion

### How It Was Implemented

The rich renderer was added in:

- `src/internal/bpftrace_mapper.h`
- `src/bpftrace_mapper.cpp`

The options surface was extended with:

- `EnableRichTypePrinting`
- `MaxAggregateDepth`
- `MaxAggregateMembers`
- `MaxArrayElements`

New rich builder entry points:

- `BuildRichBpftraceUprobeScript(...)`
- `SIGMINER_BuildRichBpftraceUprobeScriptForTarget(...)`

Implemented rendering behavior:

- `char *` and `const char *` render through `str(uptr(...))`
- pointer-to-primitive prints the pointer address and dereferenced value
- pointer-to-aggregate prints the pointer address and recursively expanded fields
- aggregate-by-value arguments are expanded when the probe form makes that possible
- arrays are handled in a limited way, with special handling for string-like arrays
- unsupported or truncated cases fall back to explicit labeled output

Compatibility work done while making this actually run on the installed `bpftrace`:

- switched generated aggregate preambles to C-compatible type names
- stopped reusing scratch variables across different typed values
- replaced pointer truthiness checks with explicit `!= 0`

## 4. Generated Aggregate Preambles

### Plan Followed

Emit only the aggregate declarations required by the rich-rendered values, in a form `bpftrace` can parse and use for field access.

### How It Was Implemented

The rich renderer now generates aggregate definitions before the probe blocks.

Behavior:

- referenced aggregate types are collected recursively
- each aggregate is emitted once
- unnamed types are sanitized into deterministic generated names
- declarations are intentionally minimal and focused on field access compatibility

This branch does not attempt full source-level reconstruction of the original C/C++ type definitions.

## 5. ABI and Ownership Strategy

### Plan Followed

Avoid inflating the existing shallow public ABI. Add new rich structs and APIs rather than turning the old `TypeEntry`/`Signature` path into a recursive ownership graph.

### How It Was Implemented

The rich C ABI was added in:

- `include/sigminer/sigminer_c.h`
- `src/sigminer_c.cpp`

Implemented support:

- shallow-to-rich and rich-to-C conversion paths
- recursive copy/allocation logic for rich types
- recursive free logic for rich types and signatures
- `SIGMINER_FreeCString(...)` implementation for returned script strings

The old shallow API remains available and unchanged in behavior.

## 6. Mock Validation and Feasibility Checks

### Plan Followed

Validate the approach with concrete mock functions that exercise the main target cases:

- string pointers
- primitive pointers
- aggregate pointers

### How It Was Implemented

New mock symbols were added in:

- `test/mock_lib.c`

Added functions:

- `DerefInt`
- `CountString`
- `SumTraceNode`

The live mock driver was extended in:

- `test/mock_live.cpp`

It now continuously calls all three new symbols so the tracer can be attached directly to them.

Script-generation regression coverage was added in:

- `test/rich_bpftrace_mapper.cpp`

Build integration was added in:

- `CMakeLists.txt`

The main checks now prove:

- the shallow path stays shallow
- string pointers generate `str(uptr(...))`
- primitive pointers generate typed dereference code
- aggregate pointers generate aggregate preambles and field expansion

## 7. What Was Left Unimplemented

### Plan Followed

Treat this branch as a feasibility study, not a fully hardened or complete final feature.

### How It Was Left

Items intentionally left incomplete or partial:

- aggregate return-by-value rich decoding is not implemented
- array rendering exists but is not broadly validated or hardened
- no rich bulk resolved-symbol script builder was added alongside the shallow bulk builder
- no broad compatibility matrix was added for unions, typedef-heavy layouts, or nested array-heavy types
- no automated end-to-end runtime validation was added across multiple `bpftrace` versions
- fallback behavior is practical for the study but not polished as a final product surface

## 8. Outcome

### Plan Followed

Demonstrate that Sigminer can support richer pointer and aggregate decoding without sacrificing the current shallow path.

### How It Was Achieved

This branch demonstrates:

- rich DWARF extraction is workable in parallel with the existing shallow path
- richer `bpftrace` script generation is practical
- live decoding works for:
  - string pointers
  - primitive pointers
  - aggregate pointers

This branch should be read as a working exploration and implementation base, not the final end-state of the feature.
